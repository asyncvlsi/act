/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2020 Rajit Manohar
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
#include "finline.h"
#include <act/iter.h>
#include <string.h>


ActCHPFuncInline::ActCHPFuncInline (Act *a) : ActPass (a, "finline")
{
  _useidx = 0;
}

void ActCHPFuncInline::_full_inline (act_chp_lang_t *c)
{
  list_t *l = list_new ();

  /* -- do any simple inlines, and collect complex inline functions -- */
  _inline_funcs (l, c);
  
  if (!list_isempty (l)) {
    /* -- there are complex inlines -- */
    list_t *m;

    m = list_new ();
    for (listitem_t *li = list_first (l); li; li = list_next (li)) {
      int found = 0;
      for (listitem_t *mi = list_first (m); mi; mi = list_next (mi)) {
	if (list_value (li) == list_value (mi)) {
	  found = 1;
	  break;
	}
      }
      if (!found) {
	list_append (m, list_value (li));
      }
    }

    /* first make sure that if the complex inlines call other
       functions, those have been inlined already */
    for (listitem_t *li = list_first (m); li; li = list_next (li)) {
      Function *fx = (Function *) list_value (li);
      Assert (!fx->isExternal() && !fx->isSimpleInline(), "Why am I here?");

      Scope *tmp = _cursc;
      _cursc = fx->CurScope();
      _full_inline (fx->getlang()->getchp()->c);
      _cursc = tmp;
    }
    list_free (m);
  }

  /* -- now do a one-step complex inline -- */
  if (list_isempty (l)) {
    _structure_assign (c);
  }
  else {
    _do_complex_inline (NULL, l, c);
  }

  list_free (l);
}
				    

void *ActCHPFuncInline::local_op (Process *p, int mode)
{
  list_t *_complex_inlines;
  
  if (!p) return NULL;
  if (!p->getlang()) return NULL;

  if (p) {
    _cursc = p->CurScope();
  }
  else {
    _cursc = ActNamespace::Global()->CurScope();
  }
  
  if (p->getlang()->getchp()) {
    _full_inline (p->getlang()->getchp()->c);
  }
  if (p->getlang()->getdflow()) {
    _complex_inlines = list_new ();
    for (listitem_t *li = list_first (p->getlang()->getdflow()->dflow);
	 li; li = list_next (li)) {
      _inline_funcs (_complex_inlines, (act_dataflow_element *) list_value (li));
    }
    if (!list_isempty (_complex_inlines)) {
      fprintf (stderr, "ERROR: Complex functions can't be inlined in a dataflow block.");
      fprintf (stderr, "  Functions:");
      for (listitem_t *li = list_first (_complex_inlines);
	   li; li = list_next (li)) {
	Function *fx = (Function *) list_value (li);
	fprintf (stderr, " %s", fx->getName());
      }
      fprintf (stderr, "\n");
      exit (1);
    }
    list_free (_complex_inlines);
  }
  return NULL;
}

void ActCHPFuncInline::free_local (void *v)
{
  
}

int ActCHPFuncInline::run (Process *p)
{
  return ActPass::run (p);
}

Expr *ActCHPFuncInline::_inline_funcs (list_t *l, Expr *e)
{
  Expr *tmp;
  
  if (!e) return e;
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
    e->u.e.l = _inline_funcs (l, e->u.e.l);
    e->u.e.r = _inline_funcs (l, e->u.e.r);
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    e->u.e.l = _inline_funcs (l, e->u.e.l);
    break;

  case E_BITFIELD:
    break;

  case E_QUERY:
    e->u.e.l = _inline_funcs (l, e->u.e.l);
    e->u.e.r->u.e.l = _inline_funcs (l, e->u.e.r->u.e.l);
    e->u.e.r->u.e.r = _inline_funcs (l, e->u.e.r->u.e.r);
    break;

  case E_CONCAT:
    tmp = e;
    do {
      tmp->u.e.l = _inline_funcs (l, tmp->u.e.l);
      tmp = tmp->u.e.r;
    } while (tmp);
    break;

  case E_FUNCTION:
    {
      Expr **x2 = _inline_funcs_general (l, e);
      if (x2) {
	e = x2[0];
	FREE (x2);
      }
    }
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
  case E_SELF:
  case E_SELF_ACK:
  case E_PROBE:
  case E_VAR:
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return e;
}

Expr **ActCHPFuncInline::_inline_funcs_general (list_t *l, Expr *e)
{
  Assert (e->type == E_FUNCTION, "What?!");

  Expr *tmp;
  int args;
  Expr **arglist;

  UserDef *ux = (UserDef *) e->u.fn.s;
  Assert (ux, "Hmm.");
  Function *fx = dynamic_cast<Function *> (ux);
  Assert (fx, "Hmm");

  
  if (fx->isExternal()) {
    return NULL;
  }
  if (!fx->isSimpleInline()) {
    list_append (l, fx);
    return NULL;
  }
  
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
      arglist[args++] = expr_expand (tmp->u.e.l, NULL, NULL,
				     ACT_EXPR_EXFLAG_DUPONLY|ACT_EXPR_EXFLAG_CHPEX);
      arglist[args-1] = _inline_funcs (l, arglist[args-1]);
      tmp = tmp->u.e.r;
    }
  }
  else {
    arglist = NULL;
  }

  /*-- now simplify! --*/
  Expr **x2;

  x2 = fx->toInline (args, arglist);
  Assert (x2, "What?");
  
  if (args > 0) {
    FREE (arglist);
  }
  return x2;
}


void ActCHPFuncInline::_inline_funcs (list_t *l, act_dataflow_element *e)
{
  listitem_t *li;
  if (!e) return;
  switch (e->t) {
  case ACT_DFLOW_FUNC:
    {
      InstType *it = _cursc->FullLookup (e->u.func.rhs, NULL);
      Assert (TypeFactory::isChanType (it), "What?");
      it = TypeFactory::getChanDataType (it);
      if (TypeFactory::isStructure (it)) {
	if (e->u.func.lhs->type == E_VAR) {
	  /* nothing to be done here */
	}
	else {
	  Expr **vals = _inline_funcs_general (l, e->u.func.lhs);
	  Data *d;
	  int *types;
	  int nb, ni;

	  if (vals) {
	    /* simple inline */
	    d = dynamic_cast <Data *>(it->BaseType());
	    Assert (d, "Hmm");
	    ActId **fields = d->getStructFields (&types);
	    d->getStructCount (&nb, &ni);
	    int sz = nb + ni;
	    list_t *l = list_new ();

	    // we need to create a multi-assignment!

	    if (sz == 1) {
	      e->u.func.lhs = vals[0];
	    }
	    else {
	      Expr *te;
	      NEW (e->u.func.lhs, Expr);
	      e->u.func.lhs->type = E_CONCAT;
	      te = e->u.func.lhs;
	      for (int i=0; i < sz; i++) {
		te->u.e.l = vals[i];
		if (!vals[i]) {
		  act_error_ctxt (stderr);
		  warning ("Dataflow inlining: incomplete structure assignment, field #%d?", i);
		  te->u.e.l = const_expr (0);
		}
		else {
		  if (types[i] == 0) {
		    NEW (te->u.e.l, Expr);
		    te->u.e.l->type = E_BUILTIN_INT;
		    te->u.e.l->u.e.r = NULL;
		    te->u.e.l = vals[i];
		  }
		}
		if (i != sz-1) {
		  NEW (te->u.e.r, Expr);
		  te = te->u.e.r;
		  te->type = E_CONCAT;
		}
		else {
		  te->u.e.r = NULL;
		}
	      }
	      // this is not quite right!
	    }
	    FREE (fields);
	    FREE (types);
	    FREE (vals);

	    // we now need to wrap it in one function call!

	    UserDef *ux = dynamic_cast<UserDef *> (it->BaseType());
	    Assert (ux, "We should not be here");
	    Assert (ux->isExpanded(), "Only expanded types here");
	    UserMacro *um = ux->getMacro (ux->getUnexpanded()->getName());
	    if (!um) {
	      Data *xd = dynamic_cast<Data *> (ux);
	      Assert (xd, "Why am I here?");
	      xd->synthStructMacro();
	      um = ux->getMacro (ux->getUnexpanded()->getName());
	    }
	    Assert (um, "What?");
	    Expr *final;
	    NEW (final, Expr);
	    final->type = E_FUNCTION;
	    final->u.fn.s = (char *) um->getFunction();

	    NEW (final->u.fn.r, Expr);
	    final->u.fn.r->type = E_LT;
	    final->u.fn.r->u.e.r = NULL;
	    final->u.fn.r->u.e.l = e->u.func.lhs;
	    e->u.func.lhs = final;
	  }
	  // otherwise what are we doing!
	}
      }
      else {
	e->u.func.lhs = _inline_funcs (l, e->u.func.lhs);
      }
    }
    break;

  case ACT_DFLOW_CLUSTER:
    for (li = list_first (e->u.dflow_cluster); li; li = list_next (li)) {
      _inline_funcs (l, (act_dataflow_element *) list_value (li));
    }
    break;

  case ACT_DFLOW_SPLIT:
  case ACT_DFLOW_MERGE:
  case ACT_DFLOW_MIXER:
  case ACT_DFLOW_ARBITER:
  case ACT_DFLOW_SINK:
    break;

  default:
    fatal_error ("Unknown dataflow type %d", e->t);
    break;
  }
}

void ActCHPFuncInline::_inline_funcs (list_t *l, act_chp_lang_t *c)
{
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    {
      int is_struct = 0;
      InstType *it = _cursc->FullLookup (c->u.comm.chan, NULL);
      if (TypeFactory::isUserType (it)) {
	it = dynamic_cast<UserDef *>(it->BaseType())->root();
      }
      Chan *cx = dynamic_cast <Chan *> (it->BaseType());
      Assert (cx, "What?");
      if (TypeFactory::isStructure (cx->datatype())) {
	is_struct = 1;
      }

      if (!is_struct) {
	c->u.comm.e = _inline_funcs (l, c->u.comm.e);
      }
      else {
	/* XXX: check special case of function that returns a structure */
	warning ("Structure return value inlining for send data not currently supported!");
      }
    }
    break;

  case ACT_CHP_ASSIGN:
    {
      InstType *it = _cursc->FullLookup (c->u.assign.id, NULL);
      if (TypeFactory::isStructure (it)) {
	if (c->u.assign.e->type == E_VAR) {
	  /* nothing to be done here */
	}
	else {
	  Expr **vals = _inline_funcs_general (l, c->u.assign.e);
	  Data *d;
	  int *types;
	  int nb, ni;

	  if (vals) {
	    /* simple inline */
	  
	    d = dynamic_cast <Data *>(it->BaseType());
	    Assert (d, "Hmm");
	    ActId **fields = d->getStructFields (&types);
	    FREE (types);
	    d->getStructCount (&nb, &ni);
	    int sz = nb + ni;
	    list_t *l = list_new ();
	    for (int i=0; i < sz; i++) {
	      act_chp_lang_t *tc;

	      if (vals[i]) {
		NEW (tc, act_chp_lang_t);
		tc->type = ACT_CHP_ASSIGN;
		tc->label = NULL;
		tc->space = NULL;
		tc->u.assign.id = c->u.assign.id->Clone();
		tc->u.assign.id->Tail()->Append (fields[i]);
		tc->u.assign.e = vals[i];
		list_append (l, tc);
	      }
	      else {
		delete fields[i];
	      }
	    }
	    FREE (fields);
	    c->type = ACT_CHP_SEMI;
	    c->u.semi_comma.cmd = l;
	  }
	  else {
	    Assert (_inline_funcs_general (l, c->u.assign.e) == NULL, "What?");
	  }
	}
      }
      else {
	c->u.assign.e = _inline_funcs (l, c->u.assign.e);
      }
    }
    break;

  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      _inline_funcs (l, (act_chp_lang_t *) list_value (li));
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->g) {
	gc->g = _inline_funcs (l, gc->g);
      }
      if (gc->s) {
	_inline_funcs (l, gc->s);
      }
    }
    break;

  case ACT_CHP_SKIP:
  case ACT_CHP_FUNC:
  case ACT_CHP_HOLE:
    break;

  case ACT_CHP_SEMILOOP:
  case ACT_CHP_COMMALOOP:
  default:
    fatal_error ("Unknown CHP type %d", c->type);
    break;
  }
}


int ActCHPFuncInline::_get_fresh_idx (const char *prefix, int *idx)
{
  char tmpnm[1024];
  int i = *idx;
  
  do {
    snprintf (tmpnm, 1024, "%s_%d", prefix, i++);
  } while (_cursc->Lookup (tmpnm));
  *idx = i;

  return i-1;
}

/*------------------------------------------------------------------------
 *  
 *  Complex inlines: functions with loops
 *
 *------------------------------------------------------------------------
 */
struct fn_inline_args {
  Function *fx;
  ActId **args;
  ActId *ret;
  struct Hashtable *local_vars;
};

void ActCHPFuncInline::_do_complex_inline (struct pHashtable *Hargs, list_t *l, act_chp_lang_t *c)
{
  char tmpnm[1024];
  int ret_idx = 0, arg_idx = 0, local_idx = 0;
  
  phash_bucket_t *b;

  if (!Hargs) {
    Hargs = phash_new (4);
  }
  
  for (listitem_t *li = list_first (l); li; li = list_next (li)) {
    Function *fx = (Function *) list_value (li);
    
    /* 
       If this is the first time we see this function, then we create
       its arguments and return value.
    */
    b = phash_lookup (Hargs, fx);
    if (!b) {
      struct fn_inline_args *args;
      NEW (args, struct fn_inline_args);
      int idx;

      args->fx = fx;
      idx = _get_fresh_idx ("fret", &ret_idx);
      snprintf (tmpnm, 1024, "fret_%d", idx);
      args->ret = new ActId (tmpnm);
      Assert (_cursc->Add (tmpnm, fx->getRetType()), "What?");
      
      if (fx->getNumPorts() > 0) {
	MALLOC (args->args, ActId *, fx->getNumPorts());
      }
      else {
	args->args = NULL;
      }
      for (int i=0; i < fx->getNumPorts(); i++) {
	idx = _get_fresh_idx ("farg", &arg_idx);
	snprintf (tmpnm, 1024, "farg_%d", idx);
	args->args[i] = new ActId (tmpnm);
	Assert (_cursc->Add (tmpnm, fx->getPortType (i)), "What?");
      }
      b = phash_add (Hargs, fx);
      b->v = args;

      args->local_vars = NULL;

      if (fx->getlang() && fx->getlang()->getchp()) {
	list_t *cmplx = list_new ();
	Scope *tmpsc;

	tmpsc = _cursc;
	_cursc = fx->CurScope ();
	_inline_funcs (cmplx, fx->getlang()->getchp()->c);
	if (!list_isempty (cmplx)) {
	  _do_complex_inline (Hargs, cmplx, fx->getlang()->getchp()->c);
	}
	_cursc = tmpsc;
	list_free (cmplx);
      }

      /* -- add arguments, result, and local variables to the current
	 scope -- */

      ActInstiter it(fx->CurScope());
      for (it = it.begin(); it != it.end(); it++) {
	ValueIdx *vx = (*it);
	if (TypeFactory::isParamType (vx->t)) continue;
	if (fx->FindPort (vx->getName()) == 0 &&
	    (strcmp (vx->getName(), "self") != 0)) {
	  hash_bucket_t *ab;
	  /* local variable */
	  if (!args->local_vars) {
	    args->local_vars = hash_new (4);
	  }
	  idx = _get_fresh_idx ("floc", &local_idx);
	  snprintf (tmpnm, 1024, "floc_%d", idx);
	  Assert (_cursc->Add (tmpnm, vx->t), "What?");
	  ab = hash_add (args->local_vars, vx->getName());
	  ab->v = new ActId (tmpnm);
	}
      }
    }
  }

  /* -- now apply complex inline -- */
  _complex_inline_helper (Hargs, c);
  
  /*-- free hash table --*/
  phash_iter_t it;
  phash_iter_init (Hargs, &it);
  while ((b = phash_iter_next (Hargs, &it))) {
    struct fn_inline_args *args;
    args = (struct fn_inline_args *) b->v;
    if (args->fx->getNumPorts() > 0) {
      for (int i=0; i < args->fx->getNumPorts(); i++) {
	delete args->args[i];
      }
      FREE (args->args);
    }
    delete args->ret;

    if (args->local_vars) {
      hash_iter_t it2;
      hash_bucket_t *b2;
      hash_iter_init (args->local_vars, &it2);
      while ((b2 = hash_iter_next (args->local_vars, &it2))) {
	ActId *tmp = (ActId *)b2->v;
	delete tmp;
      }
      hash_free (args->local_vars);
    }
    FREE (args);
  }
  phash_free (Hargs);

  while (!list_isempty (l)) {
    list_delete_head (l);
  }
}


void ActCHPFuncInline::_structure_assign (act_chp_lang_t *c)
{
  if (!c) return;

  switch (c->type) {
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    {
      InstType *it = _cursc->FullLookup (c->u.comm.chan, NULL);
      if (TypeFactory::isUserType (it)) {
	it = dynamic_cast<UserDef *>(it->BaseType())->root();
      }
      Chan *cx = dynamic_cast <Chan *> (it->BaseType());
      Assert (cx, "What?");
      if (TypeFactory::isStructure (cx->datatype())) {
	/* XXX: check special case of function that returns a structure */
	warning ("Structure return value inlining for send data not currently supported!");
      }
    }
    break;

  case ACT_CHP_ASSIGN:
    {
      InstType *it = _cursc->FullLookup (c->u.assign.id, NULL);
      if (TypeFactory::isStructure (it)) {
	if (c->u.assign.e->type == E_VAR) {
	  int *types;
	  int nb, ni;
	  ActId *e_rhs;

	  e_rhs = (ActId *) c->u.assign.e->u.e.l;
	  
	  /* element-wise assignment */
	  Data *d = dynamic_cast<Data *> (it->BaseType());
	  Assert (d, "Hmm");
	  ActId **fields = d->getStructFields (&types);
	  FREE (types);
	  d->getStructCount (&nb, &ni);
	  int sz = nb + ni;
	  list_t *l = list_new ();
	  for (int i=0; i < sz; i++) {
	    act_chp_lang_t *tc;
	    NEW (tc, act_chp_lang_t);
	    tc->type = ACT_CHP_ASSIGN;
	    tc->label = NULL;
	    tc->space = NULL;
	    tc->u.assign.id = c->u.assign.id->Clone();
	    tc->u.assign.id->Tail()->Append (fields[i]);
	    NEW (tc->u.assign.e, Expr);
	    tc->u.assign.e->type = E_VAR;
	    tc->u.assign.e->u.e.r = NULL;
	    tc->u.assign.e->u.e.l = (Expr *) e_rhs->Clone();
	    ((ActId *)tc->u.assign.e->u.e.l)->Append (fields[i]->Clone());
	    list_append (l, tc);
	  }
	  FREE (fields);
	  c->type = ACT_CHP_SEMI;
	  c->u.semi_comma.cmd = l;
	}
	else {
	  Assert (c->u.assign.e->type == E_FUNCTION, "What?");
	  Function *func = (Function *) c->u.assign.e->u.fn.s;
	  if (!func->isExternal() && !func->isSimpleInline()) {
	    fatal_error ("Fix this please (complex inline struct)!");
	  }
	}
      }
    }
    break;

  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      _structure_assign ((act_chp_lang_t *) list_value (li));
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->s) {
	_structure_assign (gc->s);
      }
    }
    break;

  case ACT_CHP_SKIP:
  case ACT_CHP_FUNC:
  case ACT_CHP_HOLE:
    break;

  case ACT_CHP_SEMILOOP:
  case ACT_CHP_COMMALOOP:
  default:
    fatal_error ("Unknown CHP type %d", c->type);
    break;
  }
}



void ActCHPFuncInline::_complex_inline_helper (struct pHashtable *H,
					       act_chp_lang_t *c)
{
  if (!c) return;

  switch (c->type) {
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    {
      int is_struct = 0;
      InstType *it = _cursc->FullLookup (c->u.comm.chan, NULL);
      if (TypeFactory::isUserType (it)) {
	it = dynamic_cast<UserDef *>(it->BaseType())->root();
      }
      Chan *cx = dynamic_cast <Chan *> (it->BaseType());
      Assert (cx, "What?");
      if (TypeFactory::isStructure (cx->datatype())) {
	is_struct = 1;
      }

      if (!is_struct) {
	list_t *l = list_new ();
	_collect_complex_inlines (l, c->u.comm.e);
	if (!list_isempty (l)) {
	  act_chp_lang_t *ch = _do_inline (H, l);
	  _apply_complex_inlines (l, c->u.comm.e);

	  act_chp_lang_t *d;
	  NEW (d, act_chp_lang_t);
	  *d = *c;
	  c->type = ACT_CHP_SEMI;
	  c->u.semi_comma.cmd = list_new ();
	  list_append (c->u.semi_comma.cmd, ch);
	  list_append (c->u.semi_comma.cmd, d);
	}
	list_free (l);
      }
      else {
	/* XXX: check special case of function that returns a structure */
	warning ("Structure return value inlining for send data not currently supported!");
      }
    }
    break;

  case ACT_CHP_ASSIGN:
    {
      InstType *it = _cursc->FullLookup (c->u.assign.id, NULL);
      if (TypeFactory::isStructure (it)) {
	if (c->u.assign.e->type == E_VAR) {
	  int *types;
	  int nb, ni;
	  ActId *e_rhs;

	  e_rhs = (ActId *) c->u.assign.e->u.e.l;
	  
	  /* element-wise assignment */
	  Data *d = dynamic_cast<Data *> (it->BaseType());
	  Assert (d, "Hmm");
	  ActId **fields = d->getStructFields (&types);
	  FREE (types);
	  d->getStructCount (&nb, &ni);
	  int sz = nb + ni;
	  list_t *l = list_new ();
	  for (int i=0; i < sz; i++) {
	    act_chp_lang_t *tc;
	    NEW (tc, act_chp_lang_t);
	    tc->type = ACT_CHP_ASSIGN;
	    tc->label = NULL;
	    tc->space = NULL;
	    tc->u.assign.id = c->u.assign.id->Clone();
	    tc->u.assign.id->Tail()->Append (fields[i]);
	    NEW (tc->u.assign.e, Expr);
	    tc->u.assign.e->type = E_VAR;
	    tc->u.assign.e->u.e.r = NULL;
	    tc->u.assign.e->u.e.l = (Expr *) e_rhs->Clone();
	    ((ActId *)tc->u.assign.e->u.e.l)->Append (fields[i]->Clone());
	    list_append (l, tc);
	  }
	  FREE (fields);
	  c->type = ACT_CHP_SEMI;
	  c->u.semi_comma.cmd = l;
	}
	else {
	  Assert (c->u.assign.e->type == E_FUNCTION, "What?");
	  Function *func = (Function *) c->u.assign.e->u.fn.s;
	  if (!func->isExternal() && !func->isSimpleInline()) {
	    list_t *l = list_new ();
	    _collect_complex_inlines (l, c->u.assign.e);
	    if (!list_isempty (l)) {
	      act_chp_lang_t *ch = _do_inline (H, l);
	      _apply_complex_inlines (l, c->u.assign.e);

	      act_chp_lang_t *d;
	      NEW (d, act_chp_lang_t);
	      *d = *c;
	      c->type = ACT_CHP_SEMI;
	      c->u.semi_comma.cmd = list_new ();
	      list_append (c->u.semi_comma.cmd, ch);
	      list_append (c->u.semi_comma.cmd, d);
	    }
	  }
	}
      }
      else {
	list_t *l = list_new ();
	_collect_complex_inlines (l, c->u.assign.e);
	if (!list_isempty (l)) {
	  act_chp_lang_t *ch = _do_inline (H, l);
	  _apply_complex_inlines (l, c->u.assign.e);

	  act_chp_lang_t *d;
	  NEW (d, act_chp_lang_t);
	  *d = *c;
	  c->type = ACT_CHP_SEMI;
	  c->u.semi_comma.cmd = list_new ();
	  list_append (c->u.semi_comma.cmd, ch);
	  list_append (c->u.semi_comma.cmd, d);
	}
	list_free (l);
      }
    }
    break;

  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      _complex_inline_helper (H, (act_chp_lang_t *) list_value (li));
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    {
      list_t *l = list_new ();
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	if (gc->g) {
	  _collect_complex_inlines (l, gc->g);
	}
	if (gc->s) {
	  _complex_inline_helper (H, gc->s);
	}
      }
      if (!list_isempty (l)) {
	act_chp_lang_t *ch = _do_inline (H, l);
	for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	  if (gc->g) {
	    _apply_complex_inlines (l, gc->g);
	  }
	}
	act_chp_lang_t *d;
	NEW (d, act_chp_lang_t);
	*d = *c;
	c->type = ACT_CHP_SEMI;
	c->u.semi_comma.cmd = list_new ();
	list_append (c->u.semi_comma.cmd, ch);
	list_append (c->u.semi_comma.cmd, d);
      }
      list_free (l);
    }
    break;

  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    {
      list_t *l = list_new ();
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	if (gc->g) {
	  _collect_complex_inlines (l, gc->g);
	}
	if (gc->s) {
	  _complex_inline_helper (H, gc->s);
	}
      }
      if (!list_isempty (l)) {
	fatal_error ("Fix loop guard inlining");
      }
      list_free (l);
    }
    break;

  case ACT_CHP_SKIP:
  case ACT_CHP_FUNC:
  case ACT_CHP_HOLE:
    break;

  case ACT_CHP_SEMILOOP:
  case ACT_CHP_COMMALOOP:
  default:
    fatal_error ("Unknown CHP type %d", c->type);
    break;
  }
}


struct complex_inline_req {
  Function *fx;
  Expr **args;
};

void ActCHPFuncInline::_collect_complex_inlines (list_t *l, Expr *e)
{
  if (!e) return;
  
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
    _collect_complex_inlines (l, e->u.e.l);
    _collect_complex_inlines (l, e->u.e.r);
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    _collect_complex_inlines (l, e->u.e.l);
    break;

  case E_BITFIELD:
    break;

  case E_QUERY:
    _collect_complex_inlines (l, e->u.e.l);
    _collect_complex_inlines (l, e->u.e.r->u.e.l);
    _collect_complex_inlines (l, e->u.e.r->u.e.r);
    break;

  case E_CONCAT:
    {
      Expr *tmp = e;
      do {
	_collect_complex_inlines (l, tmp->u.e.l);
	tmp = tmp->u.e.r;
      } while (tmp);
    }
    break;

  case E_FUNCTION:
    {
      Expr *tmp;
      tmp = e->u.fn.r;
      while (tmp) {
	_collect_complex_inlines (l, tmp->u.e.l);
	tmp = tmp->u.e.r;
      }
      Function *fx = (Function *) e->u.fn.s;
      if (!fx->isExternal() && !fx->isSimpleInline()) {
	struct complex_inline_req *req;
	int n;
	NEW (req, struct complex_inline_req);
	req->fx = (Function *) e->u.fn.s;
	if (req->fx->getNumPorts() > 0) {
	  MALLOC (req->args, Expr *, req->fx->getNumPorts());
	}
	else {
	  req->args = NULL;
	}
	tmp = e->u.fn.r;
	n = 0;
	while (tmp) {
	  req->args[n++] = tmp->u.e.l;
	  tmp = tmp->u.e.r;
	}
	list_append (l, req);
      }
    }
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
  case E_SELF:
  case E_SELF_ACK:
  case E_PROBE:
  case E_VAR:
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return;
}


void ActCHPFuncInline::_apply_complex_inlines (list_t *l, Expr *e)
{
  if (!e) return;
  
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
    _apply_complex_inlines (l, e->u.e.l);
    _apply_complex_inlines (l, e->u.e.r);
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    _apply_complex_inlines (l, e->u.e.l);
    break;

  case E_BITFIELD:
    break;

  case E_QUERY:
    _apply_complex_inlines (l, e->u.e.l);
    _apply_complex_inlines (l, e->u.e.r->u.e.l);
    _apply_complex_inlines (l, e->u.e.r->u.e.r);
    break;

  case E_CONCAT:
    {
      Expr *tmp = e;
      do {
	_apply_complex_inlines (l, tmp->u.e.l);
	tmp = tmp->u.e.r;
      } while (tmp);
    }
    break;

  case E_FUNCTION:
    {
      Expr *tmp = e->u.fn.r;
      while (tmp) {
	_apply_complex_inlines (l, tmp->u.e.l);
	tmp = tmp->u.e.r;
      }
      Function *fx = (Function *) e->u.fn.s;
      if (!fx->isExternal() && !fx->isSimpleInline()) {
	e->type = E_VAR;
	e->u.e.l = (Expr *) list_delete_head (l);
	e->u.e.r = NULL;
      }
    }
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
  case E_SELF:
  case E_SELF_ACK:
  case E_PROBE:
  case E_VAR:
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return;
}

static ActId *_find_subst (struct fn_inline_args *fn, ActId *id)
{
  ActId *repl;
  if (id) {
    int pos = fn->fx->FindPort (id->getName());
    if (pos > 0) {
      repl = fn->args[pos-1];
    }
    else if (strcmp (id->getName(), "self") == 0) {
      repl = fn->ret;
    }
    else {
      hash_bucket_t *b = hash_lookup (fn->local_vars, id->getName());
      if (!b) {
	fatal_error ("Could not find id `%s'??", id->getName());
      }
      repl = (ActId *)b->v;
    }
  }
  else {
    repl = fn->ret;
  }

  repl = repl->Clone();

  if (id && id->Rest()) {
    repl->Append (id->Rest()->Clone());
  }
  return repl;
}

static Expr *_expr_clone_subst (struct fn_inline_args *fn, Expr *e)
{
  Expr *ret;
  
  if (!e) return NULL;
  
  NEW (ret, Expr);
  ret->u.e.l = NULL;
  ret->u.e.r = NULL;
  ret->type = e->type;
  
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
    ret->u.e.l = _expr_clone_subst (fn, e->u.e.l);
    ret->u.e.r = _expr_clone_subst (fn, e->u.e.r);
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    ret->u.e.l = _expr_clone_subst (fn, e->u.e.l);
    break;

  case E_BITFIELD:
    ret->u.e.l = (Expr *) _find_subst (fn, (ActId *)e->u.e.l);
    NEW (ret->u.e.r, Expr);
    ret->u.e.r->type = e->u.e.r->type;
    ret->u.e.r->u.e.l  = _expr_clone_subst (fn, e->u.e.r->u.e.l);
    ret->u.e.r->u.e.r  =  _expr_clone_subst (fn, e->u.e.r->u.e.r);
    break;

  case E_QUERY:
    ret->u.e.l = _expr_clone_subst (fn, e->u.e.l);
    NEW (ret->u.e.r, Expr);
    ret->u.e.r->type = e->u.e.r->type;
    ret->u.e.r->u.e.l = _expr_clone_subst (fn, e->u.e.r->u.e.l);
    ret->u.e.r->u.e.r = _expr_clone_subst (fn, e->u.e.r->u.e.r);
    break;

  case E_CONCAT:
    {
      Expr *tmpr;
      Expr *tmp = e;
      tmpr = ret;
      do {
	tmpr->u.e.l = _expr_clone_subst (fn, tmp->u.e.l);
	tmp = tmp->u.e.r;
	if (tmp) {
	  NEW (tmpr->u.e.r, Expr);
	  tmpr = tmpr->u.e.r;
	  tmpr->type = tmp->type;
	  tmpr->u.e.l = NULL;
	  tmpr->u.e.r = NULL;
	}
      } while (tmp);
    }
    break;

  case E_FUNCTION:
    {
      fatal_error ("Should not be here!");
    }
    break;
    
  case E_VAR:
    ret->u.e.l = (Expr *) _find_subst (fn, (ActId *)e->u.e.l);
    break;
    
  case E_SELF:
    ret->type = E_VAR;
    ret->u.e.l = (Expr *) _find_subst (fn, NULL);
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
    FREE (ret);
    ret = e;
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return ret;
}

static act_chp_lang_t *_chp_clone_subst (struct fn_inline_args *fn,
					 act_chp_lang_t *c)
{
  act_chp_lang_t *ret;
#if 0
  static int depth = 0;

  printf ("[%d] Clone: %s\n", depth, fn->fx->getName());
  for (int i=0; i < fn->fx->getNumPorts(); i++) {
    printf ("   arg %d: ", i);
    fn->args[i]->Print (stdout);
    printf ("\n");
  }
  printf ("   ret is: ");
  fn->ret->Print (stdout);
  printf ("\n");
  printf ("[%d] === chp ===\n", depth);
  printf ("   "); chp_print (stdout, c);
  printf ("\n[%d] ===\n\n", depth);
  depth++;
#endif
  
  if (!c) {
#if 0
    depth--;
#endif
    return NULL;
  }
  
  NEW (ret, act_chp_lang_t);
  ret->type = c->type;
  ret->space = NULL;
  ret->label = NULL;
  
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    ret->u.semi_comma.cmd = list_new ();
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      list_append (ret->u.semi_comma.cmd,
		   _chp_clone_subst (fn, (act_chp_lang_t *)list_value (li)));
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    {
      act_chp_gc_t *tmp = NULL;
      ret->u.gc = NULL;
      
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	if (!tmp) {
	  NEW (ret->u.gc, act_chp_gc_t);
	  tmp = ret->u.gc;
	}
	else {
	  NEW (tmp->next, act_chp_gc_t);
	  tmp = tmp->next;
	}
	tmp->id = NULL;
	tmp->lo = NULL;
	tmp->hi = NULL;
	tmp->g = _expr_clone_subst (fn, gc->g);
	tmp->s = _chp_clone_subst (fn, gc->s);
	tmp->next = NULL;
      }
    }
    break;

  case ACT_CHP_ASSIGN:
    ret->u.assign.id = _find_subst (fn, c->u.assign.id);
    ret->u.assign.e = _expr_clone_subst (fn, c->u.assign.e);
    break;

  case ACT_CHP_SKIP:
  case ACT_CHP_FUNC:
  case ACT_CHP_HOLE:
    ret->type = ACT_CHP_SKIP;
    break;

  default:
    fatal_error ("Unknown chp type %d\n", c->type);
    break;
  }

#if 0
  depth--;
  printf ("\n[%d] === returning ===\n   ", depth);
  chp_print (stdout, ret);
  printf ("\n[%d] ==========\n\n", depth);
#endif

  return ret;
}

					 

act_chp_lang_t *ActCHPFuncInline::_do_inline (struct pHashtable *H,
					      list_t *l)
{
  act_chp_lang_t *c, *tmpc;
  int len;

  NEW (c, act_chp_lang_t);
  c->label = NULL;
  c->space = NULL;
  c->type = ACT_CHP_SEMI;
  c->u.semi_comma.cmd = list_new ();

  len = list_length (l);
  while (len > 0) {
    struct complex_inline_req *req =
      (struct complex_inline_req *) list_delete_head (l);

    phash_bucket_t *b = phash_lookup (H, req->fx);
    Assert (b, "Can't find function?!");

    struct fn_inline_args *fn = (struct fn_inline_args *) b->v;
    Assert (fn->fx == req->fx, "What?!");

    if (fn->fx->getlang() && fn->fx->getlang()->getchp()) {
      
      /* -- inline this function -- */
      for (int i=0; i < fn->fx->getNumPorts(); i++) {
	NEW (tmpc, act_chp_lang_t);
	tmpc->label = NULL;
	tmpc->space = NULL;
	tmpc->type = ACT_CHP_ASSIGN;
	tmpc->u.assign.id = fn->args[i]->Clone();
	tmpc->u.assign.e = req->args[i];
	list_append (c->u.semi_comma.cmd, tmpc);
      }
      
      list_append (c->u.semi_comma.cmd,
		   _chp_clone_subst (fn, fn->fx->getlang()->getchp()->c));

      NEW (tmpc, act_chp_lang_t);
      tmpc->label = NULL;
      tmpc->space = NULL;
      tmpc->type = ACT_CHP_ASSIGN;
      NEW (tmpc->u.assign.e, Expr);
      tmpc->u.assign.e->type = E_VAR;
      tmpc->u.assign.e->u.e.r = NULL;
      tmpc->u.assign.e->u.e.l = (Expr *) fn->ret->Clone();

      int idx = _get_fresh_idx ("fuse", &_useidx);
      char buf[1024];
      snprintf (buf, 1024, "fuse_%d", idx);
      Assert (_cursc->Add (buf, fn->fx->getRetType()) == 1, "Hmm");
      tmpc->u.assign.id = new ActId (buf);
      list_append (l, new ActId (buf));

      list_append (c->u.semi_comma.cmd, tmpc);
      
    }
    len--;
  }
  return c;
}
