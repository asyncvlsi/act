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
#include <string.h>
#include <common/misc.h>
#include <common/hash.h>

/*------------------------------------------------------------------------
 *
 *  Function inlining support
 *
 *------------------------------------------------------------------------
 */
struct hash_stack {
  Scope *sc;
  struct Hashtable *state;
  struct hash_stack *parent;
};

static Expr *_ex_one (Expr **x)
{
  Expr *e = x[0];
  FREE (x);
  return e;
}

static Expr **_lookup_binding (struct hash_stack *Hs,
			       const char *name, int err = 1)
{
  hash_bucket_t *b;
  while (Hs) {
    b = hash_lookup (Hs->state, name);
    if (b) {
      int ni, nb;
      int sz = 1;
      InstType *it = Hs->sc->Lookup (name);
      if (TypeFactory::isStructure (it)) {
	Data *d = dynamic_cast<Data *>(it->BaseType());
	d->getStructCount (&nb, &ni);
	sz = nb + ni;
      }
      /* return a copy */
      Expr **ret;
      MALLOC (ret, Expr *, sz);
      for (int i=0; i < sz; i++) {
	ret[i] = ((Expr **)b->v)[i];
      }
      return ret;
    }
    Hs = Hs->parent;
  }
  if (err) {
    act_error_ctxt (stderr);
    fatal_error ("Inlining failed! Variable `%s' used before being defined",
		 name);
  }
  return NULL;
}

static void _update_binding (struct hash_stack *Hs, ActId *id, Expr **update)
{
  InstType *xit = Hs->sc->Lookup (id->getName());
  int sz = 1;
  Data *xd;

#if 0
  printf ("update binding for: ");
  id->Print (stdout);
  printf ("\n");

  printf ("entry 0 is %p: ", update[0]);
  fflush (stdout);
  if (update[0]) {
    print_expr (stdout, update[0]);
  }
  else {
    printf ("-null-");
  }
  printf ("\n");
#endif
  
  Assert (xit, "What?");
  xd = NULL;

  if (TypeFactory::isStructure (xit)) {
    xd = dynamic_cast<Data *> (xit->BaseType());
    Assert (xd, "Hmm");
    int nb, ni;
    xd->getStructCount (&nb, &ni);
    sz = nb + ni;
  }

  hash_bucket_t *b;
  
  /* find partial or total update, and update entry in the hash table! 
     tmp is the FULL structure binding.
     if id->Rest() then we need to do a partial assignment.
   */
  b = hash_lookup (Hs->state, id->getName());
  if (!b) {
    Expr **bind;
    Expr **xtmp = _lookup_binding (Hs, id->getName(), 0);

    if (xtmp) {
      bind = xtmp;
    }
    else {
      MALLOC (bind, Expr *, sz);
      for (int i=0; i < sz; i++) {
	bind[i] = NULL;
      }
    }
    b = hash_add (Hs->state, id->getName());
    b->v = bind;
  }
  Expr **res = _lookup_binding (Hs, id->getName());

  if (id->Rest()) {
    Assert (xd, "What?!");
    int off = xd->getStructOffset (id->Rest());
    Assert (off >= 0 && off < sz, "What?");
    int sz2 = 1;
    int nb, ni;
    int id_pos = xd->FindPort (id->Rest()->getName());
    Assert (id_pos > 0, "What?!");
    InstType *tmp = xd->getPortType (id_pos-1);
    if (TypeFactory::isStructure (tmp)) {
      Data *xd2 = dynamic_cast<Data *> (tmp->BaseType());
      Assert (xd2, "What?!");
      xd2->getStructCount (&nb, &ni);
      sz2 = nb + ni;
    }
    Assert (off + sz2 <= sz, "Hmm");
    for (int i=0; i < sz2; i++) {
      res[off+i] = update[i];
    }
  }
  else {
    for (int i=0; i < sz; i++) {
      res[i] = update[i];
    }
  }
  b = hash_lookup (Hs->state, id->getName());
  Assert (b, "Hmm");
  Expr **xtmp = (Expr **)b->v;
  b->v = res;
#if 0  
  for (int i=0; i < sz; i++) {
    if (xtmp[i]) {
      expr_ex_free (xtmp[i]);
    }
  }
#endif  
  FREE (xtmp);
}


static Expr **_expand_inline (struct hash_stack *Hs, Expr *e)
{
  Expr *tmp, *tmp2;
  Expr *ret;
  Expr **rets;
    
  if (!e) return NULL;

  rets = NULL;
  
  NEW (ret, Expr);
  ret->type = e->type;
  ret->u.e.l = NULL;
  ret->u.e.r = NULL;
  
  switch (e->type) {
  case E_AND:
  case E_OR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV:
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_NE:
  case E_EQ:
    ret->u.e.l = _ex_one (_expand_inline (Hs, e->u.e.l));
    ret->u.e.r = _ex_one (_expand_inline (Hs, e->u.e.r));
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    ret->u.e.l = _ex_one (_expand_inline (Hs, e->u.e.l));
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_BITFIELD:
    /* we can work with bitfield so long as it is a basic varaible
       only */
    {
      Expr **res = _lookup_binding (Hs, ((ActId *)e->u.e.l)->getName());
      Expr *r = res[0];
      FREE (res);
      if (r->type != E_VAR) {
	fatal_error ("Can't inline bitfields of expressions");
      }
      ret->u.e.l = r->u.e.l;
      NEW (ret->u.e.r, Expr);
      ret->u.e.r->type = e->u.e.r->type;
      ret->u.e.r->u.e.l = e->u.e.r->u.e.l;
      ret->u.e.r->u.e.r = e->u.e.r->u.e.r;
    }
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_QUERY:
    ret->u.e.l = _ex_one (_expand_inline (Hs, e->u.e.l));
    ret->u.e.r->u.e.l = _ex_one (_expand_inline (Hs, e->u.e.r->u.e.l));
    ret->u.e.r->u.e.r = _ex_one (_expand_inline (Hs, e->u.e.r->u.e.r));
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_CONCAT:
    tmp = e;
    tmp2 = ret;
    do {
      tmp2->u.e.l = _ex_one (_expand_inline (Hs, tmp->u.e.l));
      tmp = tmp->u.e.r;
      if (tmp) {
	NEW (tmp2->u.e.r, Expr);
	tmp2 = tmp2->u.e.r;
	tmp2->type = E_COMMA;
	tmp2->u.e.l = NULL;
	tmp2->u.e.r = NULL;
      }
    } while (tmp);

    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_FUNCTION:
    {
      int args;
      Expr **arglist;
      
      tmp = e;
      tmp = tmp->u.fn.r;
      /* fix arguments */
      args = 0;
      while (tmp) {
	args++;
	tmp = tmp->u.e.r;
      }

      if (args > 0) {
	MALLOC (arglist, Expr *, args);
	tmp = e->u.fn.r;
	args = 0;
	while (tmp) {
	  arglist[args++] = _ex_one (_expand_inline (Hs, tmp->u.e.l));
	  tmp = tmp->u.e.r;
	}
      }
      else {
	arglist = NULL;
      }

      /*-- now simplify! --*/
      UserDef *ux = (UserDef *) e->u.fn.s;
      Assert (ux, "Hmm.");
      Function *fx = dynamic_cast<Function *> (ux);
      Assert (fx, "Hmm");

      Assert (!fx->isExternal(), "Why are we here?");
      Assert (fx->isSimpleInline(), "Why are we here?");

      rets = fx->toInline (args, arglist);
      Assert (rets, "What?!");
      
      if (args > 0) {
	FREE (arglist);
      }
    }
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
    FREE (ret);
    ret = e;
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;
    
  case E_PROBE:
    fatal_error ("Probes in functions?");
    break;
    
  case E_SELF:
    rets = _lookup_binding (Hs, "self");
    break;
    
  case E_VAR:
    {
      ActId *tid = (ActId *)e->u.e.l;
      rets = _lookup_binding (Hs, tid->getName());
    }
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return rets;
}

static void _run_function_fwd (struct hash_stack *Hs, act_chp_lang_t *c)
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
      struct hash_stack *Hnew;
      Expr **guards;
      MALLOC (Hnew, struct hash_stack, nh);
      MALLOC (guards, Expr *, nh);
      for (int i=0; i < nh; i++) {
	Hnew[i].state = hash_new (4);
	Hnew[i].parent = Hs;
	Hnew[i].sc = Hs->sc;
      }
      nh = 0;
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	if (gc->g) {
	  guards[nh] = _ex_one (_expand_inline (Hs, gc->g));
	}
	else {
	  guards[nh] = NULL;
	}
	if (gc->s) {
	  _run_function_fwd (&Hnew[nh], gc->s);
	}
	nh++;
      }

      struct Hashtable *Hmerge = hash_new (4);
      hash_iter_t it;
      hash_bucket_t *b;

      for (int i=0; i < nh; i++)  {
	hash_iter_init (Hnew[i].state, &it);
	while ((b = hash_iter_next (Hnew[i].state, &it))) {
	  if (!hash_lookup (Hmerge, b->key)) {
	    hash_add (Hmerge, b->key);
	  }
	}
      }
      
      /*-- Hmerge contains all the modified state --*/
      hash_iter_init (Hmerge, &it);
      while ((b = hash_iter_next (Hmerge, &it))) {
	hash_bucket_t *xb;
	/* construct expression for b->key! */
	int sz = 1;
	Expr **inpval;
	Expr *cur;

	inpval = _lookup_binding (Hs, b->key, 0);

	InstType *et = Hs->sc->Lookup (b->key);
	Assert (et, "What?");
	if (TypeFactory::isIntType (et)) {
	  if (inpval) {
	    cur = inpval[0];
	  }
	  else {
	    cur = const_expr (0);
	  }
	}
	else if (TypeFactory::isBoolType (et)) {
	  if (inpval) {
	    cur = inpval[0];
	  }
	  else {
	    cur = const_expr_bool (0);
	  }
	}
	else {
	  Assert (TypeFactory::isStructure (et), "What?");
	  fatal_error ("Fix structures!");
	  /*-- compute sz --*/
	}

	Expr *update = NULL;
	Expr *newval = NULL;
	
	for (int i=0; i < nh; i++) {
	  xb = hash_lookup (Hnew[i].state, b->key);
	  if (!update || guards[i]) {
	    if (!update) {
	      Assert (guards[i], "weird else clause?!");
	      NEW (update, Expr);
	      newval = update;
	    }
	    else {
	      NEW (update->u.e.r, Expr);
	      update = update->u.e.r;
	    }
	    update->type = E_QUERY;
	    update->u.e.l = guards[i];
	    update->u.e.r = NULL;
	  }
	  else {
	    /* 
	       update && !guards[i] : else clause, nothing to do here,
	       just update update->u.e.r 
	    */
	    Assert (i == nh-1, "else is not the last case?");
	  }
	  Expr *curval;
	  if (xb) {
	    Expr **val = (Expr **) xb->v;
	    curval = val[0];
	  }
	  else {
	    curval = expr_expand (cur, NULL, NULL, ACT_EXPR_EXFLAG_DUPONLY);
	  }
		
	  if (i == nh-1) {
	    update->u.e.r = curval;
	  }
	  else {
	    NEW (update->u.e.r, Expr);
	    update = update->u.e.r;
	    update->type = E_COLON;
	    update->u.e.l = curval;
	    update->u.e.r = NULL;
	  }
	}
	Expr **bind;
	MALLOC (bind, Expr *, 1);
	bind[0] = newval;
	ActId *tmpid = new ActId (b->key);
	_update_binding (Hs, tmpid, bind);
	delete tmpid;
      }
      

      FREE (guards);
      for (int i=0; i < nh; i++) {
	hash_free (Hnew[i].state);
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
      Expr **tmp = _expand_inline (Hs, c->u.assign.e);
      _update_binding (Hs, c->u.assign.id, tmp);
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
  int nbools, nints;
  Expr **ret;
  Assert (nargs == getNumPorts(), "Function for parameters used in CHP!");

  Assert (isSimpleInline() && !isExternal(), "Function::toInline() called for complex inlining scenario");
  
  ActInstiter it(CurScope());
  for (it = it.begin(); it != it.end(); it++) {
    ValueIdx *vx = (*it);
    if (TypeFactory::isParamType (vx->t)) continue;
    if (vx->t->arrayInfo()) {
      warning ("Inlining failed; array declarations!");
      return NULL;
    }
  }

  /* 
     take 
       "self = V"
     and walk backward through the function!
  */
  if (pending) {
    fatal_error ("Sorry, recursive functions (`%s') not supported.",
		 getName());
  }
  pending = 1;

  /* convert CHP body into an expression! */
  hash_stack Hs;
  Hs.state = hash_new (4);
  Hs.parent = NULL;
  Hs.sc = CurScope ();

  if (!getlang() || !getlang()->getchp()) {
    fatal_error ("Inlining function, but no CHP?");
  }

  /* bind arguments! */

  for (int i=0; i < nargs; i++) {
    Expr **te;
    hash_bucket_t *b;
    NEW (te, Expr *);
    te[0] = args[i];
    Assert (i < getNumPorts(), "Hmm...");
    b = hash_add (Hs.state, getPortName (i));
    b->v = te;
  }

  _run_function_fwd (&Hs, getlang()->getchp()->c);

  hash_bucket_t *b;
  b = hash_lookup (Hs.state, "self");
  if (!b) {
    warning ("Function inlining failed; self was not assigned!");
    return NULL;
  }
  Expr **xret = (Expr **) b->v;

  /* XXX: release all storage */
  hash_free (Hs.state);
  
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
