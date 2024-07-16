/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#include <stdio.h>
#include <act/act.h>
#include <act/types.h>
#include <act/iter.h>
#include <act/inline.h>
#include <string.h>
#include <common/misc.h>
#include <common/hash.h>


static void _run_function_fwd (act_inline_table *Hs, act_chp_lang_t *c)
{
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    /*-- handle both as semicolons --*/
    for (listitem_t *li = list_first (c->u.semi_comma.cmd); li;
	 li = list_next (li)) {
      _run_function_fwd (Hs, (act_chp_lang_t *) list_value (li));
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    {
      int nh;
      nh = 0;
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	nh++;
      }
      Assert (nh > 0, "What?");

      act_inline_table **Hnew;
      Expr **guards;

      MALLOC (Hnew, act_inline_table *, nh);
      MALLOC (guards, Expr *, nh);
      for (int i=0; i < nh; i++) {
	Hnew[i] = act_inline_new (NULL, Hs);
      }
      nh = 0;
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	if (gc->g) {
	  guards[nh] = act_inline_expr_simple (Hs, gc->g);
	}
	else {
	  guards[nh] = NULL;
	}
	if (gc->s) {
	  _run_function_fwd (Hnew[nh], gc->s);
	}
	nh++;
      }

      Hs = act_inline_merge_tables (nh, Hnew, guards);
      FREE (guards);
      for (int i=0; i < nh; i++) {
	act_inline_free (Hnew[i]);
      }
      FREE (Hnew);
    }
    break;

  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    fatal_error ("Loop inlining is more complex...");
    break;

  case ACT_CHP_SKIP:
  case ACT_CHP_FUNC: /* we will lose log messages */
    break;
    
  case ACT_CHP_ASSIGN:
    {
      Expr **tmp = act_inline_expr (Hs, c->u.assign.e);
      act_inline_setval (Hs, c->u.assign.id, tmp);
    }
    break;
    
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    fatal_error ("Send/receive in function body?!");
    break;

  default:
    fatal_error ("Don't know how to handle this chp case (%d)!", c->type);
    break;
  }
}


/*
 * This only applies to non-parameter functions, and simple functions
 */
Expr **Function::toInline (int nargs, Expr **args)
{
  Assert (nargs == getNumPorts(), "Function for parameters used in CHP!");

  Assert (isSimpleInline() && !isExternal(), "Function::toInline() called for complex inlining scenario");

  act_error_push (getName(), getFile(), getLine());
  
  ActInstiter it(CurScope());
  for (it = it.begin(); it != it.end(); it++) {
    ValueIdx *vx = (*it);
    if (TypeFactory::isParamType (vx->t)) continue;
    if (vx->t->arrayInfo()) {
      act_error_ctxt (stderr);
      warning ("Inlining failed; array declarations!");
      act_error_pop ();
      return NULL;
    }
  }

  /* 
     take 
       "self = V"
     and walk backward through the function!
  */
  if (pending) {
    act_error_ctxt (stderr);
    fatal_error ("Sorry, recursive functions (`%s') not supported.",
		 getName());
  }
  pending = 1;

  /* convert CHP body into an expression! */
  act_inline_table *Hs = act_inline_new (CurScope(), NULL);

  if (!getlang() || !getlang()->getchp()) {
    act_error_ctxt (stderr);
    fatal_error ("Inlining function, but no CHP?");
  }

  /* bind arguments! */

  for (int i=0; i < nargs; i++) {
    Expr **te;

    if (TypeFactory::isStructure (getPortType (i))) {
      Data *d = dynamic_cast <Data *> (getPortType(i)->BaseType());
      int nb, ni;
      Assert (d, "What?");
      d->getStructCount (&nb, &ni);
      MALLOC (te, Expr *, nb + ni);
      int *types;
      ActId **fields = d->getStructFields (&types);
      if (args[i]->type != E_VAR) {
	act_error_ctxt (stderr);
	fatal_error ("toInline(): handle more complex expr `%s'",
		     args[i]->type);
      }
      for (int j=0; j < nb + ni; j++) {
	ActId *tmp = ((ActId *)args[i]->u.e.l)->Clone ();
	tmp->Tail()->Append (fields[j]);
	NEW (te[j], Expr);
	te[j]->type = E_VAR;
	te[j]->u.e.l = (Expr *) tmp;
	te[j]->u.e.r = NULL;
      }
      FREE (types);
      FREE (fields);
    }
    else {
      NEW (te, Expr *);
      te[0] = args[i];
    }
    
    Assert (i < getNumPorts(), "Hmm...");
    
    ActId *tmp = new ActId (getPortName (i));
    act_inline_setval (Hs, tmp, te);
    delete tmp;
  }

  _run_function_fwd (Hs, getlang()->getchp()->c);

  Expr **xret = act_inline_getval (Hs, "self");

  act_inline_free (Hs);
  
  if (!xret) {
    act_error_ctxt (stderr);
    warning ("%s: Function inlining failed; self was not assigned!",
	     getName());
    act_error_pop ();
    return NULL;
  }

  pending = 0;
  
  act_error_pop ();
  
  return xret;
}


/*-- 
  check if the chp body of this function might have a simple inline
  option 
--*/
void Function::chkInline (void)
{
  if (!getlang() || !getlang()->getchp()) {
    is_simple_inline = 0;
    return;
  }

  if (!getlang()->getchp()->is_synthesizable) {
    is_simple_inline = 0;
    return;
  }

//  if (TypeFactory::isStructure (getRetType())) {
//    is_simple_inline = 0;
//    return;
//  }

  act_chp_lang_t *c = getlang()->getchp()->c;
  
  is_simple_inline = 1;
  _chk_inline (c);
}
    
  
void Function::_chk_inline (act_chp_lang_t *c)
{
  if (!c) return;
  
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    for (listitem_t *li = list_first (c->u.semi_comma.cmd); li;
	 li = list_next (li)) {
      _chk_inline ((act_chp_lang_t *) list_value (li));
      if (!is_simple_inline) {
	return;
      }
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->g) {
	_chk_inline (gc->g);
	if (!is_simple_inline) return;
      }
      if (gc->s) {
	_chk_inline (gc->s);
	if (!is_simple_inline) return;
      }
    }
    break;

  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    is_simple_inline = 0;
    break;

  case ACT_CHP_SKIP:
  case ACT_CHP_FUNC: /* we will lose log messages */
    break;
    
  case ACT_CHP_ASSIGN:
    _chk_inline (c->u.assign.e);
    break;
    
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    fatal_error ("Send/receive in function body?!");
    is_simple_inline = 0;
    break;

  default:
    fatal_error ("Don't know how to handle this chp case (%d)!", c->type);
    break;
  }    
}

void Function::_chk_inline (Expr *e)
{
  if (!e) return;
  if (!is_simple_inline) return;
  switch (e->type) {
  case E_INT:
  case E_TRUE:
  case E_FALSE:
  case E_REAL:
  case E_VAR:
  case E_PROBE:
  case E_BITFIELD:
    break;

  case E_FUNCTION:
    {
      UserDef *ux = (UserDef *) e->u.fn.s;
      Assert (ux, "What?");
      Function *fx = dynamic_cast <Function *> (ux);
      Assert (fx, "What?");

      if (!fx->isSimpleInline()) {
	is_simple_inline = 0;
	return;
      }

      Expr *tmp = e->u.fn.r;
      while (tmp) {
	_chk_inline (tmp->u.e.l);
	if (!is_simple_inline) {
	  return;
	}
	tmp = tmp->u.e.r;
      }
    }
    break;

  default:
    if (e->u.e.l) {
      _chk_inline (e->u.e.l);
      if (!is_simple_inline) {
	return;
      }
    }
    if (e->u.e.r) {
      _chk_inline (e->u.e.r);
      if (!is_simple_inline) {
	return;
      }
    }
    break;
  }
  return;
}


/*------------------------------------------------------------------------
 *
 *  Evaluate a parameter-only function
 *
 *------------------------------------------------------------------------
 */
static void _run_chp (Scope *s, act_chp_lang_t *c)
{
  int loop_cnt = 0;
  
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      _run_chp (s, (act_chp_lang_t *) list_value (li));
    }
    break;

  case ACT_CHP_COMMALOOP:
  case ACT_CHP_SEMILOOP:
    {
      int ilo, ihi;
      ValueIdx *vx;
      act_syn_loop_setup (NULL, s, c->u.loop.id, c->u.loop.lo, c->u.loop.hi,
			  &vx, &ilo, &ihi);

      for (int iter=ilo; iter <= ihi; iter++) {
	s->setPInt (vx->u.idx, iter);
	_run_chp (s, c->u.loop.body);
      }
      act_syn_loop_teardown (NULL, s, c->u.loop.id, vx);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    {
      act_chp_gc_t *gc;
      for (gc = c->u.gc; gc; gc = gc->next) {
	Expr *guard;
	if (gc->id) {
	  ValueIdx *vx;
	  int ilo, ihi;

	  act_syn_loop_setup (NULL, s, gc->id, gc->lo, gc->hi,
			      &vx, &ilo, &ihi);
	
	  for (int iter=ilo; iter <= ihi; iter++) {
	    s->setPInt (vx->u.idx, iter);
	    guard = expr_expand (gc->g, NULL, s);
	    if (!guard || guard->type == E_TRUE) {
	      /* guard is true */
	      _run_chp (s, gc->s);
	      act_syn_loop_teardown (NULL, s, gc->id, vx);
	      return;
	    }
	    if (!expr_is_a_const (guard)) {
	      act_error_ctxt (stderr);
	      fatal_error ("Guard is not a constant expression?");
	    }
	  }
	  act_syn_loop_teardown (NULL, s, gc->id, vx);
	}
	else {
	  guard = expr_expand (gc->g, NULL, s);
	  if (!guard || guard->type == E_TRUE) {
	    _run_chp (s, gc->s);
	    return;
	  }
	  if (!expr_is_a_const (guard)) {
	    act_error_ctxt (stderr);
	    fatal_error ("Guard is not a constant expression?");
	  }
	}
      }
      act_error_ctxt (stderr);
      fatal_error ("In a function call: all guards are false!");
    }
    break;

  case ACT_CHP_LOOP:
    while (1) {
      loop_cnt++;
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	Expr *guard;
	if (gc->id) {
	  ValueIdx *vx;
	  int ilo, ihi;
	  
	  act_syn_loop_setup (NULL, s, gc->id, gc->lo, gc->hi,
			      &vx, &ilo, &ihi);
	
	  for (int iter=ilo; iter <= ihi; iter++) {
	    s->setPInt (vx->u.idx, iter);
	    guard = expr_expand (gc->g, NULL, s);
	    if (!guard || guard->type == E_TRUE) {
	      /* guard is true */
	      _run_chp (s, gc->s);
	      act_syn_loop_teardown (NULL, s, gc->id, vx);
	      goto resume;
	    }
	    if (!expr_is_a_const (guard)) {
	      act_error_ctxt (stderr);
	      fatal_error ("Guard is not a constant expression?");
	    }
	  }
	  act_syn_loop_teardown (NULL, s, gc->id, vx);
	}
	else {
	  guard = expr_expand (gc->g, NULL, s);
	  if (!guard || guard->type == E_TRUE) {
	    _run_chp (s, gc->s);
	    goto resume;
	  }
	  if (!expr_is_a_const (guard)) {
	    act_error_ctxt (stderr);
	    fatal_error ("Guard is not a constant expression?");
	  }
	}
      }
      /* all guards false */
      return;
    resume:
      if (loop_cnt > Act::max_loop_iterations) {
	fatal_error ("# of loop iterations exceeded limit (%d)", Act::max_loop_iterations);
      }
      ;
    }
    break;

  case ACT_CHP_DOLOOP:
    {
      Expr *guard;
      Assert (c->u.gc->next == NULL, "What?");
      do {
	_run_chp (s, c->u.gc->s);
	guard = expr_expand (c->u.gc->g, NULL, s);
      } while (!guard || guard->type == E_TRUE);
      if (!expr_is_a_const (guard)) {
	act_error_ctxt (stderr);
	fatal_error ("Guard is not a constant expression?");
      }
    }
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    {
      Expr *e = expr_expand (c->u.assign.e, NULL, s);
      ActId *id = expand_var_write (c->u.assign.id, NULL, s);
      if (!expr_is_a_const (e)) {
	act_error_ctxt (stderr);
	fatal_error ("Assignment: expression is not a constant?");
      }
      AExpr *ae = new AExpr (e);
      s->BindParam (id, ae);
      delete ae;
      delete id;
    }      
    break;
    
  case ACT_CHP_SEND:
    act_error_ctxt (stderr);
    fatal_error ("Channels in a function?");
    break;
    
  case ACT_CHP_RECV:
    act_error_ctxt (stderr);
    fatal_error ("Channels in a function?");
    break;

  case ACT_CHP_FUNC:
    /* built-in functions; skip */
    break;
    
  default:
    break;
  }
}

typedef long (*EXT_ACT_FUNC)(int nargs, long *args);

static struct ExtLibs *_act_ext_libs = NULL;

Expr *Function::eval (ActNamespace *ns, int nargs, Expr **args)
{
  Assert (nargs == getNumParams(), "What?");
  
  for (int i=0; i < nargs; i++) {
      Assert (expr_is_a_const (args[i]), "Argument is not a constant?");
  }

  /* 
     now we allocate all the parameters within the function scope
     and bind them to the specified values.
  */
    
  /* evaluate function!
     1. Bind parameters 
  */

  if (pending) {
    fatal_error ("Sorry, recursive functions (`%s') not supported.",
		 getName());
  }

  /* XXX we could cache the allocated state, maybe later...*/
  
  I->FlushExpand ();
  pending = 1;
  expanded = 1;

  ValueIdx *vx;
      
  for (int i=0; i < getNumParams(); i++) {
    InstType *it;
    const char *name;
    it = getPortType (-(i+1));
    name = getPortName (-(i+1));
    
    I->Add (name, it);
    vx = I->LookupVal (name);
    Assert (vx, "Hmm");
    vx->init = 1;
    if (TypeFactory::isPIntType (it)) {
      vx->u.idx = I->AllocPInt();
    }
    else if (TypeFactory::isPBoolType (it)) {
      vx->u.idx = I->AllocPBool();
    }
    else if (TypeFactory::isPRealType (it)) {
      vx->u.idx = I->AllocPReal();
    }
    else {
      fatal_error ("Invalid type in function signature");
    }
    AExpr *ae = new AExpr (args[i]);
    I->BindParam (name, ae);
    delete ae;
  }

  I->Add ("self", getRetType ()->Expand (ns, I));
  vx = I->LookupVal ("self");
  Assert (vx, "Hmm");
  vx->init = 1;
  if (TypeFactory::isPIntType (getRetType())) {
    vx->u.idx = I->AllocPInt();
  }
  else if (TypeFactory::isPBoolType (getRetType())) {
    vx->u.idx = I->AllocPBool();
  }
  else if (TypeFactory::isPRealType (getRetType())) {
    vx->u.idx = I->AllocPReal();
  }
  else {
    fatal_error ("Invalid return type in function signature");
  }
  

  /* now run the chp body */
  act_chp *c = NULL;
  
  if (b) {
    ActBody *btmp;

    for (btmp = b; btmp; btmp = btmp->Next()) {
      ActBody_Lang *l;
      if (!(l = dynamic_cast<ActBody_Lang *>(btmp))) {
	btmp->Expand (ns, I);
      }
      else {
	Assert (l->gettype() == ActBody_Lang::LANG_CHP, "What?");
	c = (act_chp *)l->getlang();
      }
    }
  }

  int ext_found = 0;
  if (!c) {
    EXT_ACT_FUNC fext;
    /* no chp body! could be an external function */
    if (!_act_ext_libs) {
      _act_ext_libs = act_read_extern_table ("act.extern");
    }
    char buf[2040];
    snprintf (buf, 2040, "%s<>", getName());
    fext = (EXT_ACT_FUNC) act_find_dl_func (_act_ext_libs, ns, buf);
    if (!fext) {
      act_error_ctxt (stderr);
      fatal_error ("Function `%s': no chp body, and no external definition",
		   getName());
    }
    long *eargs = NULL;
    if (nargs != 0) {
      MALLOC (eargs, long, nargs);
      for (int i=0; i < nargs; i++) {
	if (args[i]->type == E_TRUE) {
	  eargs[i] = 1;
	}
	else if (args[i]->type == E_FALSE) {
	  eargs[i] = 0;
	}
	else if (args[i]->type == E_REAL) {
	  eargs[i] = args[i]->u.f;
	}
	else if (args[i]->type == E_INT) {
	  eargs[i] = args[i]->u.ival.v;
	}
	else {
	  Assert (0, "What?!");
	}
      }
    }

    nargs = (*fext) (nargs, eargs);
    if (nargs != 0) {
      FREE (eargs);
    }
    ext_found = 1;
  }
  else {
    /* run the chp */
    Assert (c, "Isn't this required?!");
    _run_chp (I, c->c);
  }

  pending = 0;

  Expr *ret = NULL;

  if (TypeFactory::isPIntType (getRetType())) {
    if (ext_found) {
      ret = const_expr (nargs);
    }
    else {
      if (I->issetPInt (vx->u.idx)) {
	ret = const_expr (I->getPInt (vx->u.idx));
      }
      else {
	act_error_ctxt (stderr);
	fatal_error ("self is not assigned!");
      }
    }
  }
  else if (TypeFactory::isPBoolType (getRetType())) {
    if (ext_found) {
      ret = const_expr_bool (nargs == 0 ? 0 : 1);
    }
    else {
      if (I->issetPBool (vx->u.idx)) {
	ret = const_expr_bool (I->getPBool (vx->u.idx));
      }
      else {
	act_error_ctxt (stderr);
	fatal_error ("self is not assigned!");
      }
    }
  }
  else if (TypeFactory::isPRealType (getRetType())) {
    if (ext_found) {
      ret = const_expr_real (nargs);
    }
    else {
      if (I->issetPReal (vx->u.idx)) {
	ret = const_expr_real (I->getPReal (vx->u.idx));
      }
      else {
	act_error_ctxt (stderr);
	fatal_error ("self is not assigned!");
      }
    }
  }
  else {
    fatal_error ("Invalid return type in function signature");
  }
  return ret;
}

