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
  int nbools, nints;
  Expr **ret;
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
    hash_bucket_t *b;

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
