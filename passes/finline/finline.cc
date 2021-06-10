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


ActCHPFuncInline::ActCHPFuncInline (Act *a) : ActPass (a, "finline")
{
  _complex_inlines = NULL;
}

void *ActCHPFuncInline::local_op (Process *p, int mode)
{
  if (!p) return NULL;
  if (!p->getlang()) return NULL;

  if (p->getlang()->getchp()) {
    _complex_inlines = list_new ();
    _inline_funcs (p->getlang()->getchp()->c);
    if (!list_isempty (_complex_inlines)) {
      warning ("Complex function inlines are currently skipped!");
      fprintf (stderr, "  Functions:");
      for (listitem_t *li = list_first (_complex_inlines);
	   li; li = list_next (li)) {
	Function *fx = (Function *) list_value (li);
	fprintf (stderr, " %s", fx->getName());
      }
      fprintf (stderr, "\n");
    }
    list_free (_complex_inlines);
    _complex_inlines = NULL;
  }
  if (p->getlang()->getdflow()) {
    _complex_inlines = list_new ();
    for (listitem_t *li = list_first (p->getlang()->getdflow()->dflow);
	 li; li = list_next (li)) {
      _inline_funcs ((act_dataflow_element *) list_value (li));
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

Expr *ActCHPFuncInline::_inline_funcs (Expr *e)
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
    e->u.e.l = _inline_funcs (e->u.e.l);
    e->u.e.r = _inline_funcs (e->u.e.r);
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    e->u.e.l = _inline_funcs (e->u.e.l);
    break;

  case E_BITFIELD:
    break;

  case E_QUERY:
    e->u.e.l = _inline_funcs (e->u.e.l);
    e->u.e.r->u.e.l = _inline_funcs (e->u.e.r->u.e.l);
    e->u.e.r->u.e.r = _inline_funcs (e->u.e.r->u.e.r);
    break;

  case E_CONCAT:
    tmp = e;
    do {
      tmp->u.e.l = _inline_funcs (tmp->u.e.l);
      tmp = tmp->u.e.r;
    } while (tmp);
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
	  arglist[args++] = expr_expand (tmp->u.e.l, NULL, NULL,
					 ACT_EXPR_EXFLAG_DUPONLY);
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

      if (!fx->isExternal()) {
	if (fx->isSimpleInline()) {
	  Expr **x2;
	  Assert (!TypeFactory::isStructure (fx->getRetType()), "What?");
	  x2 = fx->toInline (args, arglist);
	  Assert (x2, "What?");
	  tmp = x2[0];
	  FREE (x2);
	  e = tmp;
	}
	else {
	  list_append (_complex_inlines, fx);
	}
      }
      if (args > 0) {
	FREE (arglist);
      }
    }
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
  case E_SELF:
  case E_PROBE:
  case E_VAR:
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return e;
}

void ActCHPFuncInline::_inline_funcs (act_dataflow_element *e)
{
  listitem_t *li;
  if (!e) return;
  switch (e->t) {
  case ACT_DFLOW_FUNC:
    e->u.func.lhs = _inline_funcs (e->u.func.lhs);
    break;

  case ACT_DFLOW_CLUSTER:
    for (li = list_first (e->u.dflow_cluster); li; li = list_next (li)) {
      _inline_funcs ((act_dataflow_element *) list_value (li));
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

void ActCHPFuncInline::_inline_funcs (act_chp_lang_t *c)
{
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    /* XXX: check special case of function that returns a structure */
    c->u.comm.e = _inline_funcs (c->u.comm.e);
    break;

  case ACT_CHP_ASSIGN:
    /* XXX: check special case of function that returns a structure */
    c->u.assign.e = _inline_funcs (c->u.assign.e);
    break;

  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      _inline_funcs ((act_chp_lang_t *) list_value (li));
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->g) {
	gc->g = _inline_funcs (gc->g);
      }
      if (gc->s) {
	_inline_funcs (gc->s);
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
