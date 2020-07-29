/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#include <act/act.h>
#include <act/types.h>
#include <act/lang.h>
#include <act/inst.h>
#include <act/body.h>
#include <act/value.h>
#include "prs.h"
#include "qops.h"
#include "config.h"
#include <string.h>

/*--- 
  chp expressions have a slightly different expansion, because
  they might use real arrays
 *---
 */

/*------------------------------------------------------------------------
 * CHP-Expand expression, replacing all paramters. If it is an lval, then
 * it must be a pure identifier at the end of of the day. Default is
 * not an lval
 *------------------------------------------------------------------------
 */
Expr *chp_expr_expand (Expr *e, ActNamespace *ns, Scope *s)
{
  Expr *ret, *te;
  ActId *xid;
  Expr *tmp;
  
  if (!e) return NULL;

  NEW (ret, Expr);
  ret->type = e->type;

  switch (e->type) {

  case E_ANDLOOP:
  case E_ORLOOP:
    {
      int ilo, ihi;
      ValueIdx *vx;
      Expr *cur = NULL;

      act_syn_loop_setup (ns, s, (char *)e->u.e.l->u.e.l,
			  e->u.e.r->u.e.l, e->u.e.r->u.e.r->u.e.l,
			  &vx, &ilo, &ihi);

      int is_const = 1;
      int eval;
      int count = 0;
      int i;

      if (e->type == E_ANDLOOP) {
	ret->type = E_AND;
      }
      else {
	ret->type = E_OR;
      }
      
      for (i=ilo; i <= ihi; i++) {
	s->setPInt (vx->u.idx, i);
	Expr *tmp = chp_expr_expand (e->u.e.r->u.e.r->u.e.r, ns, s);
	if (is_const && expr_is_a_const (tmp)) {
	  if (tmp->type == E_TRUE || tmp->type == E_FALSE) {
	    if (e->type == E_ANDLOOP) {
	      if (tmp->type == E_FALSE) {
		/* we're done, the whole thing is false */
		break;
	      }
	    }
	    else {
	      if (tmp->type == E_TRUE) {
		break;
	      }
	    }
	  }
	  else {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "\texpanding expr: ");
	    print_expr (stderr, e);
	    fprintf (stderr,"\n");
	    fatal_error ("Incompatible types for &/| operator");
	  }
	}
	else {
	  is_const = 0;
	  if (count == 0) {
	    ret->u.e.l = tmp;
	  }
	  else if (count == 1) {
	    ret->u.e.r = tmp;
	    cur = ret;
	  }
	  else {
	    Expr *tmp2;
	    NEW (tmp2, Expr);
	    tmp2->u.e.l = cur->u.e.r;
	    tmp2->type = cur->type;
	    cur->u.e.r = tmp2;
	    tmp2->u.e.r = tmp;
	    cur = tmp2;
	  }
	  count++;
	}
      }
      if (is_const) {
	if (i <= ihi) {
	  /* short circuit */
	  if (e->type == E_ANDLOOP) {
	    ret->type = E_FALSE;
	  }
	  else {
	    ret->type = E_TRUE;
	  }
	}
	else {
	  if (e->type == E_ANDLOOP) {
	    ret->type = E_TRUE;
	  }
	  else {
	    ret->type = E_FALSE;
	  }
	}
      }
      else {
	if (count == 1) {
	  Expr *tmp = ret->u.e.l;
	  FREE (ret);
	  ret = tmp;
	}
      }
      act_syn_loop_teardown (ns, s, (char *)e->u.e.l->u.e.l, vx);
    }
    break;
    
  case E_AND:
  case E_OR:
  case E_XOR:
    ret->u.e.l = chp_expr_expand (e->u.e.l, ns, s);
    ret->u.e.r = chp_expr_expand (e->u.e.r, ns, s);
    if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
      if (ret->u.e.l->type == E_INT && ret->u.e.r->type == E_INT) {
	unsigned int v;

	v = ret->u.e.l->u.v;
	if (e->type == E_AND) {
	  v = v & ((unsigned int)ret->u.e.r->u.v);
	}
	else if (e->type == E_OR) {
	  v = v | ((unsigned int)ret->u.e.r->u.v);
	}
	else {
	  v = v ^ ((unsigned int)ret->u.e.r->u.v);
	}
	//FREE (ret->u.e.l);
	//FREE (ret->u.e.r);
	ret->type = E_INT;
	ret->u.v = v;

	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else if (ret->u.e.l->type == E_TRUE || ret->u.e.l->type == E_FALSE) {
	unsigned int v;

	v = (ret->u.e.l->type == E_TRUE) ? 1 : 0;
	if (e->type == E_AND) {
	  v = v & ((ret->u.e.r->type == E_TRUE) ? 1 : 0);
	}
	else if (e->type == E_OR) {
	  v = v | ((ret->u.e.r->type == E_TRUE) ? 1 : 0);
	}
	else {
	  v = v ^ ((ret->u.e.r->type == E_TRUE) ? 1 : 0);
	}
	//FREE (ret->u.e.l);
	//FREE (ret->u.e.r);
	if (v) {
	  ret->type = E_TRUE;
	}
	else {
	  ret->type = E_FALSE;
	}

	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Incompatible types for &/| operator");
      }
    }
    break;

  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    ret->u.e.l = chp_expr_expand (e->u.e.l, ns, s);
    ret->u.e.r = expr_expand (e->u.e.r, ns, s);
    if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
      if (ret->u.e.l->type == E_INT && ret->u.e.r->type == E_INT) {
	signed int v;

	v = ret->u.e.l->u.v;
	if (e->type == E_PLUS) {
	  v = v + ((signed int)ret->u.e.r->u.v);
	}
	else if (e->type == E_MINUS) {
	  v = v - ((signed int)ret->u.e.r->u.v);
	}
	else if (e->type == E_MULT) {
	  v = v * ((signed int)ret->u.e.r->u.v);
	}
	else if (e->type == E_DIV) {
	  v = v / ((signed int)ret->u.e.r->u.v);
	}
	else if (e->type == E_MOD) {
	  v = v % ((signed int)ret->u.e.r->u.v);
	}
	else if (e->type == E_LSL) {
	  v = v << ((unsigned int)ret->u.e.r->u.v);
	}
	else if (e->type == E_LSR) {
	  v = ((unsigned int)v) >> ((unsigned int)ret->u.e.r->u.v);
	}
	else { /* ASR */
	  v = (signed)v >> ((unsigned int)ret->u.e.r->u.v);
	}
	//FREE (ret->u.e.l);
	//FREE (ret->u.e.r);

	ret->type = E_INT;
	ret->u.v = v;

	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
	
      }
      else if ((ret->u.e.l->type == E_INT||ret->u.e.l->type == E_REAL)
	       && (ret->u.e.r->type == E_INT || ret->u.e.r->type == E_REAL)
	       && (e->type == E_PLUS || e->type == E_MINUS || e->type == E_MULT
		   || e->type == E_DIV)) {
	double v;

#define VAL(e) (((e)->type == E_INT) ? (unsigned int)(e)->u.v : (e)->u.f)

	v = VAL(ret->u.e.l);
	if (e->type == E_PLUS) {
	  v = v + VAL(ret->u.e.r);
	}
	else if (e->type == E_MINUS) {
	  v = v - VAL(ret->u.e.r);
	}
	else if (e->type == E_MULT) {
	  v = v * VAL(ret->u.e.r);
	}
	else if (e->type == E_DIV) {
	  v = v / VAL(ret->u.e.r);
	}
	if (ret->u.e.l->type != E_INT) FREE (ret->u.e.l);
	if (ret->u.e.r->type != E_INT) FREE (ret->u.e.r);
	ret->type = E_REAL;
	ret->u.f = v;
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Incompatible types for arithmetic operator");
      }
    }
    break;

  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
    ret->u.e.l = chp_expr_expand (e->u.e.l, ns, s);
    ret->u.e.r = chp_expr_expand (e->u.e.r, ns, s);
    if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
      if (ret->u.e.l->type == E_INT && ret->u.e.r->type == E_INT) {
	signed int v;

	v = ret->u.e.l->u.v;
	if (e->type == E_LT) {
	  v = (v < ((signed int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else if (e->type == E_GT) {
	  v = (v > ((signed int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else if (e->type == E_LE) {
	  v = (v <= ((signed int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else if (e->type == E_GE) {
	  v = (v >= ((signed int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else if (e->type == E_EQ) {
	  v = (v == ((signed int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else { /* NE */
	  v = (v != ((signed int)ret->u.e.r->u.v) ? 1 : 0);
	}
	//FREE (ret->u.e.l);
	//FREE (ret->u.e.r);
	if (v) {
	  ret->type = E_TRUE;
	}
	else {
	  ret->type = E_FALSE;
	}

	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else if (ret->u.e.l->type == E_REAL && ret->u.e.r->type == E_REAL) {
	double v;

	v = ret->u.e.l->u.f;
	if (e->type == E_LT) {
	  v = (v < ret->u.e.r->u.f ? 1 : 0);
	}
	else if (e->type == E_GT) {
	  v = (v > ret->u.e.r->u.f ? 1 : 0);
	}
	else if (e->type == E_LE) {
	  v = (v <= ret->u.e.r->u.f ? 1 : 0);
	}
	else if (e->type == E_GE) {
	  v = (v >= ret->u.e.r->u.f ? 1 : 0);
	}
	else if (e->type == E_EQ) {
	  v = (v == ret->u.e.r->u.f ? 1 : 0);
	}
	else { /* NE */
	  v = (v != ret->u.e.r->u.f ? 1 : 0);
	}
	FREE (ret->u.e.l);
	FREE (ret->u.e.r);
	if (v) {
	  ret->type = E_TRUE;
	}
	else {
	  ret->type = E_FALSE;
	}

	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Incompatible types for comparison operator");
      }
    }
    break;
    
  case E_NOT:
    ret->u.e.l = chp_expr_expand (e->u.e.l, ns, s);
    if (expr_is_a_const (ret->u.e.l)) {
      if (ret->u.e.l->type == E_TRUE) {
	//FREE (ret->u.e.l);
	ret->type = E_FALSE;

	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else if (ret->u.e.l->type == E_FALSE) {
	//FREE (ret->u.e.l);
	ret->type = E_TRUE;
	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Incompatible type for not operator");
      }
    }
    break;
    
  case E_COMPLEMENT:
    ret->u.e.l = chp_expr_expand (e->u.e.l, ns, s);
    if (expr_is_a_const (ret->u.e.l)) {
      if (ret->u.e.l->type == E_TRUE) {
	//FREE (ret->u.e.l);
	ret->type = E_FALSE;
	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else if (ret->u.e.l->type == E_FALSE) {
	//FREE (ret->u.e.l);
	ret->type = E_TRUE;
	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else if (ret->u.e.l->type == E_INT) {
	unsigned int v = ret->u.e.l->u.v;
	//FREE (ret->u.e.l);
	ret->type = E_INT;
	ret->u.v = ~v;
	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Incompatible type for complement operator");
      }
    }
    break;
    
  case E_UMINUS:
    ret->u.e.l = chp_expr_expand (e->u.e.l, ns, s);
    if (expr_is_a_const (ret->u.e.l)) {
      if (ret->u.e.l->type == E_INT) {
	signed int v = ret->u.e.l->u.v;
	//FREE (ret->u.e.l);
	ret->type = E_INT;
	ret->u.v = -v;
	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else if (ret->u.e.l->type == E_REAL) {
	double f = ret->u.e.l->u.f;
	FREE (ret->u.e.l);
	ret->type = E_REAL;
	ret->u.f = -f;
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Incompatible type for unary minus operator");
      }
    }
    break;

  case E_QUERY:
    ret->u.e.l = chp_expr_expand (e->u.e.l, ns, s);
    if (!expr_is_a_const (ret->u.e.l)) {
      ret->u.e.r = chp_expr_expand (e->u.e.r, ns, s);
    }
    else {
      if (ret->u.e.l->type == E_TRUE) {
	FREE (ret);
	ret = chp_expr_expand (e->u.e.r->u.e.l, ns, s);
      }
      else if (ret->u.e.l->type == E_FALSE) {
	FREE (ret);
	ret = chp_expr_expand (e->u.e.r->u.e.r, ns, s);
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Query operator expression has non-Boolean value");
      }
    }
    break;

  case E_COLON:
    /* you only get here for non-const things */
    ret->u.e.l = expr_expand (e->u.e.l, ns, s);
    ret->u.e.r = expr_expand (e->u.e.r, ns, s);
    break;

  case E_BITFIELD:
    ret->u.e.l = (Expr *) ((ActId *)e->u.e.l)->ExpandCHP (ns, s);
    if (!expr_is_a_const (ret->u.e.l)) {
      NEW (ret->u.e.r, Expr);
      ret->u.e.r->type = E_BITFIELD;
      ret->u.e.r->u.e.l = expr_expand (e->u.e.r->u.e.l, ns, s, 0);
      ret->u.e.r->u.e.r = expr_expand (e->u.e.r->u.e.r, ns, s, 0);
      if (!expr_is_a_const (ret->u.e.r->u.e.l) || !expr_is_a_const (ret->u.e.r->u.e.r)) {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Bitfield operator has non-const components");
      }
      if (ret->u.e.r->u.e.l->type != E_INT) {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Variable in bitfield operator is a non-integer");
      }
      if (ret->u.e.r->u.e.r->type != E_INT) {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Variable in bitfield operator is a non-integer");
      }
    }
    else {
      Expr *lo, *hi;
      lo = expr_expand (e->u.e.r->u.e.l, ns, s, 0);
      hi = expr_expand (e->u.e.r->u.e.r, ns, s, 0);
      if (!expr_is_a_const (lo) || !expr_is_a_const (hi)) {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Bitfield operator has const variable but non-const components");
      }
      if (ret->u.e.l->type != E_INT) {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Variable in bitfield operator is a non-integer");
      }
      unsigned int v;
      v = ret->u.e.l->u.v;
      //FREE (ret->u.e.l);
      if (lo->type != E_INT || hi->type != E_INT) {
	act_error_ctxt (stderr);
	fprintf (stderr, "\texpanding expr: ");
	print_expr (stderr, e);
	fprintf (stderr,"\n");
	fatal_error ("Bitfield parameter in operator is a non-integer");
      }
      if (lo->u.v > hi->u.v) {
	v = 0;
      }
      else {
	v = (v >> lo->u.v) & ~(~0UL << (hi->u.v - lo->u.v + 1));
      }
      //FREE (lo);
      //FREE (hi);
      ret->type = E_INT;
      ret->u.v = v;
      tmp = TypeFactory::NewExpr (ret);
      FREE (ret);
      ret = tmp;
    }
    break;

  case E_PROBE:
    ret->u.e.l = (Expr *) ((ActId *)e->u.e.l)->ExpandCHP (ns, s);
    break;

  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    ret->u.e.l = chp_expr_expand (e->u.e.l, ns, s);
    if (!e->u.e.r) {
      ret->u.e.r = NULL;
      if (expr_is_a_const (ret->u.e.l)) {
	if (ret->type == E_BUILTIN_BOOL) {
	  if (ret->u.e.l->u.v) {
	    ret->type = E_TRUE;
	    tmp = TypeFactory::NewExpr (ret);
	    FREE (ret);
	    ret = tmp;
	  }
	  else {
	    ret->type = E_FALSE;
	    tmp = TypeFactory::NewExpr (ret);
	    FREE (ret);
	    ret = tmp;
	  }
	}
	else {
	  if (ret->u.e.l->type == E_TRUE) {
	    ret->type = E_INT;
	    ret->u.v = 1;
	    tmp = TypeFactory::NewExpr (ret);
	    FREE (ret);
	    ret = tmp;
	  }
	  else {
	    ret->type = E_INT;
	    ret->u.v = 0;
	    tmp = TypeFactory::NewExpr (ret);
	    FREE (ret);
	    ret = tmp;
	  }
	}
      }
    }
    else {
      ret->u.e.r = expr_expand (e->u.e.r, ns, s, 0);
      if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
	unsigned long x = ret->u.e.l->u.v;
	int width = ret->u.e.r->u.v;
	x = x & (~(1 << width));

	ret->type = E_INT;
	ret->u.v = x;
	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
      else {
	act_error_ctxt (stderr);
	fatal_error ("int() operator requires a constant expression");
      }
    }
    break;
    
  case E_FUNCTION:
    fatal_error ("FIXME function");
    //_eval_function (ns, s, e, &ret);
    break;

  case E_VAR:
    /* expand an ID:
       this either returns an expanded ID, or 
       for parameterized types returns the value. */
    /* XXX FIXME */
    xid = ((ActId *)e->u.e.l)->ExpandCHP (ns, s);
    te = xid->Eval (ns, s, 0);
    if (te->type != E_VAR) {
      delete xid;
    }
    FREE (ret);
    ret = te;
    break;

  case E_INT:
    ret->u.v = e->u.v;
    tmp = TypeFactory::NewExpr (ret);
    FREE (ret);
    ret = tmp;
    break;

  case E_REAL:
    ret->u.f = e->u.f;
    break;

  case E_TRUE:
  case E_FALSE:
    tmp = TypeFactory::NewExpr (ret);
    FREE (ret);
    ret = tmp;
    
    break;

#if 0
  case E_ARRAY:
  case E_SUBRANGE:
    ret->u = e->u;
    break;
#endif

  case E_SELF:
    xid = new ActId ("self");
    te = xid->Eval (ns, s, 0);
    if (te->type != E_VAR) {
      delete xid;
    }
    FREE (ret);
    ret = te;
    break;

  default:
    fatal_error ("Unknown expression type (%d)!", e->type);
    break;
  }
  
  return ret;
}




static void chp_free (act_chp_lang_t *c)
{
  /* XXX FIXME */
}

act_prs_lang_t *prs_expand (act_prs_lang_t *, ActNamespace *, Scope *);

act_size_spec_t *act_expand_size (act_size_spec_t *sz, ActNamespace *ns, Scope *s);

static ActId *fullexpand_var (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;
  
  idtmp = id->Expand (ns, s);
  
  etmp = idtmp->Eval (ns, s);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}

/*
 * This is a chp variable to be read. So it can have array
 * dereferences
 */
static ActId *expand_var_read (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;

  /* this needs to be changed  to: expand as much as possible, not
     full expand. It needs to be a non-array variable type.
  */
  idtmp = id->Expand (ns, s);
  
  etmp = idtmp->Eval (ns, s);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}

/*
 * This is a chp variable to be written. So it can have array
 * dereferences
 */
ActId *expand_var_write (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;

  /* this needs to be changed  to: expand as much as possible, not
     full expand. It needs to be a non-array variable type.
  */
  idtmp = id->ExpandCHP (ns, s);
  
  etmp = idtmp->Eval (ns, s, 1);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}


static ActId *expand_var_chan (ActId *id, ActNamespace *ns, Scope *s)
{
  return expand_var_read (id, ns, s);
}

void _merge_duplicate_rules (act_prs_lang_t *p)
{
  /* XXX: fix this */
}

act_prs *prs_expand (act_prs *p, ActNamespace *ns, Scope *s)
{
  act_prs *ret;

  if (!p) return NULL;

  NEW (ret, act_prs);

  ret->vdd = fullexpand_var (p->vdd, ns, s);
  ret->gnd = fullexpand_var (p->gnd, ns, s);
  ret->psc = fullexpand_var (p->psc, ns, s);
  ret->nsc = fullexpand_var (p->nsc, ns, s);
  ret->p = prs_expand (p->p, ns, s); 
  _merge_duplicate_rules (ret->p);
 
  if (p->next) {
    ret->next = prs_expand (p->next, ns, s);
  }
  else {
    ret->next = NULL;
  }

  return ret;
}


act_prs_expr_t *prs_expr_expand (act_prs_expr_t *p, ActNamespace *ns, Scope *s)
{
  int pick;
  
  act_prs_expr_t *ret = NULL;
  if (!p) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = p->type;
  switch (p->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l = prs_expr_expand (p->u.e.l, ns, s);
    ret->u.e.r = prs_expr_expand (p->u.e.r, ns, s);
    ret->u.e.pchg = prs_expr_expand (p->u.e.pchg, ns, s);
    ret->u.e.pchg_type = p->u.e.pchg_type;

    pick = -1;

    if (ret->u.e.l->type == ACT_PRS_EXPR_TRUE) {
      /* return ret->u.e.r; any pchg is skipped */
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 0; /* RIGHT */
      }
      else {
	pick = 1; /* LEFT */
      }
    }
    else if (ret->u.e.l->type == ACT_PRS_EXPR_FALSE) {
	if (p->type == ACT_PRS_EXPR_AND) {
	  pick = 1; /* LEFT */
	}
	else {
	  pick = 0; /* RIGHT */
	}
    }
    else if (ret->u.e.r->type == ACT_PRS_EXPR_TRUE) {
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 1; /* LEFT */
      }
      else {
	pick = 0; /* RIGHT */
      }
    }
    else if (ret->u.e.r->type == ACT_PRS_EXPR_FALSE) {
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 0; /* RIGHT */
      }
      else {
	pick = 1; /* LEFT */
      }
    }
    if (pick == 0) {
      act_prs_expr_t *t;
      /* RIGHT */
      act_free_a_prs_expr (ret->u.e.l);
      act_free_a_prs_expr (ret->u.e.pchg);
      t = ret->u.e.r;
      FREE (ret);
      ret = t;
    }
    else if (pick == 1) {
      act_prs_expr_t *t;
      /* LEFT */
      act_free_a_prs_expr (ret->u.e.r);
      act_free_a_prs_expr (ret->u.e.pchg);
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    break;

  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = prs_expr_expand (p->u.e.l, ns, s);
    ret->u.e.r = NULL;
    if (ret->u.e.l->type == ACT_PRS_EXPR_FALSE) {
      act_prs_expr_t *t;
      ret->u.e.l->type = ACT_PRS_EXPR_TRUE;
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    else if (ret->u.e.l->type == ACT_PRS_EXPR_TRUE) {
      act_prs_expr_t *t;
      ret->u.e.l->type = ACT_PRS_EXPR_FALSE;
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l.label = p->u.l.label;
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v.sz = act_expand_size (p->u.v.sz, ns, s);
    ret->u.v.id = expand_var_read (p->u.v.id, ns, s);
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    {
      ValueIdx *vx;
      int ilo, ihi, i;

      act_syn_loop_setup (ns, s, p->u.loop.id, p->u.loop.lo, p->u.loop.hi,
			  &vx, &ilo, &ihi);
      
      if (ihi < ilo) {
	/* empty */
	if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	  ret->type = ACT_PRS_EXPR_TRUE;
	}
	else {
	  ret->type = ACT_PRS_EXPR_FALSE;
	}
      }
      else {
	FREE (ret);
	ret = NULL;

	for (i=ilo; i <= ihi; i++) {
	  act_prs_expr_t *at;

	  s->setPInt (vx->u.idx, i);
	  
	  at = prs_expr_expand (p->u.loop.e, ns, s);

	  if (!ret) {
	    ret = at;
	    if (ret->type == ACT_PRS_EXPR_TRUE) {
	      if (p->type == ACT_PRS_EXPR_ANDLOOP) {
		act_free_a_prs_expr (ret);
		ret = NULL;
	      }
	      else {
		break;
	      }
	    }
	    else if (ret->type == ACT_PRS_EXPR_FALSE) {
	      if (p->type == ACT_PRS_EXPR_ANDLOOP) {
		break;
	      }
	      else {
		act_free_a_prs_expr (ret);
		ret = NULL;
	      }
	    }
	  }
	  else if (at->type == ACT_PRS_EXPR_TRUE) {
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      /* nothing */
	    }
	    else {
	      /* we're done! */
	      act_free_a_prs_expr (ret);
	      ret = at;
	      break;
	    }
	  }
	  else if (at->type == ACT_PRS_EXPR_FALSE) {
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      /* we're done */
	      act_free_a_prs_expr (ret);
	      ret = at;
	      break;
	    }
	    else {
	      /* nothing */
	    }
	  }
	  else {
	    /* ok now & and | combine! */
	    act_prs_expr_t *tmp;
	    NEW (tmp, act_prs_expr_t);
	    tmp->u.e.l = ret;
	    tmp->u.e.r = at;
	    tmp->u.e.pchg = NULL;
	    tmp->u.e.pchg_type = -1;
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      tmp->type = ACT_PRS_EXPR_AND;
	    }
	    else {
	      tmp->type = ACT_PRS_EXPR_OR;
	    }
	    ret= tmp;
	  }
	}
	if (ret == NULL) {
	  NEW (ret, act_prs_expr_t);
	  if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	    ret->type = ACT_PRS_EXPR_TRUE;
	  }
	  else {
	    ret->type = ACT_PRS_EXPR_FALSE;
	  }
	}
      }
      act_syn_loop_teardown (ns, s, p->u.loop.id, vx);
    }
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;
    
  default:
    Assert (0, "Eh?");
    break;
  }
  
  
  return ret;
}

static int current_attr_num;
static  char **current_attr_table;
static act_attr_t *attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  act_attr_t *hd = NULL, *tl = NULL, *tmp;
  int pos;

  while (a) {
    NEW (tmp, act_attr_t);
    tmp->attr = a->attr;

    for (pos = 0; pos < current_attr_num; pos++) {
      if (strcmp (current_attr_table[pos]+4, a->attr) == 0) break;
    }
    Assert (pos != current_attr_num, "What?");
    tmp->e = expr_expand (a->e, ns, s);

    if (current_attr_table[pos][0] == 'i' && tmp->e->type != E_INT) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Integer attribute %s is assigned a non-integer/non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "  evaluated to: ");
      print_expr (stderr, tmp->e);
      fprintf (stderr, "\n");
      exit (1);
    } else if (current_attr_table[pos][0] == 'r' && tmp->e->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Real attribute %s is assigned a non-real/non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "  evaluated to: ");
      print_expr (stderr, tmp->e);
      fprintf (stderr, "\n");
      exit (1);
    } else if (current_attr_table[pos][0] == 'b' &&
	       (tmp->e->type != E_TRUE && tmp->e->type != E_FALSE)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Boolean attribute %s is assigned a non-Boolean/non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "  evaluated to: ");
      print_expr (stderr, tmp->e);
      fprintf (stderr, "\n");
      exit (1);
    }
    q_ins (hd, tl, tmp);
    a = a->next;
  }
  return hd;
}


act_attr_t *prs_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  current_attr_num = config_get_table_size ("act.prs_attr");
  current_attr_table = config_get_table_string ("act.prs_attr");
  return attr_expand (a, ns, s);
}

act_attr_t *inst_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  current_attr_num = config_get_table_size ("act.instance_attr");
  current_attr_table = config_get_table_string ("act.instance_attr");
  return attr_expand (a, ns, s);
}

act_size_spec_t *act_expand_size (act_size_spec_t *sz, ActNamespace *ns, Scope *s)
{
  act_size_spec_t *ret;
  if (!sz) return NULL;

  NEW (ret, act_size_spec_t);
  if (sz->w) {
    ret->w = expr_expand (sz->w, ns, s);
    if (ret->w->type != E_INT && ret->w->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size expression is not of type pint or preal\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->w);
      fprintf (stderr, "\n");
      exit (1);
   }
  }
  else {
    ret->w = NULL;
  }
  if (sz->l) {
    ret->l = expr_expand (sz->l, ns, s);
    if (ret->l->type != E_INT && ret->l->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size expression is not of type pint or preal\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->l);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  else {
    ret->l = NULL;
  }
  if (sz->folds) {
    ret->folds = expr_expand (sz->folds, ns, s);
    if (ret->folds->type != E_INT) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size folding amount is not a pint\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->folds);
      exit (1);
    }
  }
  else {
    ret->folds = NULL;
  }
  ret->flavor = sz->flavor;
  
  return ret;
}

struct act_prsmerge {
  act_prs_lang_t *vup;		/* previous prs */
  act_prs_lang_t *vdn;		/* previous prs */
};
  
  
act_prs_lang_t *prs_expand (act_prs_lang_t *p, ActNamespace *ns, Scope *s)
{
  act_prs_lang_t *hd = NULL;
  act_prs_lang_t *tl = NULL;
  act_prs_lang_t *tmp;
  ActId *idtmp;
  Expr *etmp;

  while (p) {
    /* expand one prs */
    NEW (tmp, act_prs_lang_t);
    tmp->type = p->type;
    tmp->next = NULL;
    switch (p->type) {
    case ACT_PRS_RULE:
      tmp->u.one.attr = prs_attr_expand (p->u.one.attr, ns, s);
      tmp->u.one.e = prs_expr_expand (p->u.one.e, ns, s);
      if (p->u.one.label == 0) {
	act_connection *ac;
	
	idtmp = p->u.one.id->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hmm");
	tmp->u.one.id = (ActId *)etmp->u.e.l;
	FREE (etmp);

	ac = idtmp->Canonical (s);
	if (ac->getDir() == Type::IN) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "\t");
	  idtmp->Print (stderr);
	  fprintf (stderr, " has a directional type that is not writable.\n");
	  fatal_error ("A `bool?' cannot be on the RHS of a production rule.");
	}
      }
      else {
	/* it is a char* */
	tmp->u.one.id = p->u.one.id;
	if (p->u.one.arrow_type != 0) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "@-expressions must only use -> arrows");
	  exit (1);
	}
      }
      tmp->u.one.arrow_type = p->u.one.arrow_type;
      tmp->u.one.dir = p->u.one.dir;
      tmp->u.one.label = p->u.one.label;
      break;
      
    case ACT_PRS_GATE:
      tmp->u.p.attr = prs_attr_expand (p->u.p.attr, ns, s);
      idtmp = p->u.p.s->Expand (ns, s);
      etmp = idtmp->Eval (ns, s);
      Assert (etmp->type == E_VAR, "Hm");
      tmp->u.p.s = (ActId *)etmp->u.e.l;
      FREE (etmp);

      idtmp = p->u.p.d->Expand (ns, s);
      etmp = idtmp->Eval (ns, s);
      Assert (etmp->type == E_VAR, "Hm");
      tmp->u.p.d = (ActId *)etmp->u.e.l;
      FREE (etmp);

      if (p->u.p.g) {
	idtmp = p->u.p.g->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hm");
	tmp->u.p.g = (ActId *)etmp->u.e.l;
	FREE (etmp);
      }
      else {
	tmp->u.p.g = NULL;
      }

      if (p->u.p._g) {
	idtmp = p->u.p._g->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hm");
	tmp->u.p._g = (ActId *)etmp->u.e.l;
	FREE (etmp);
      }
      else {
	tmp->u.p._g = NULL;
      }
      tmp->u.p.sz = act_expand_size (p->u.p.sz, ns, s);
      break;
      
    case ACT_PRS_LOOP:
      Assert (s->Add (p->u.l.id, TypeFactory::Factory()->NewPInt()),
	      "Should have been caught earlier");
      {
	ValueIdx *vx;
	int ilo, ihi, i;
	act_prs_lang_t *px, *pxrethd, *pxrettl;
	Expr *etmp;
	
	if (p->u.l.lo) {
	  etmp = expr_expand (p->u.l.lo, ns, s);
	  if (etmp->type != E_INT) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	    print_expr (stderr, p->u.l.lo);
	    fprintf (stderr, "\nNot a constant int\n");
	    exit (1);
	  }
	  ilo = etmp->u.v;
	  //FREE (etmp);
	}
	else {
	  ilo = 0;
	}
	etmp = expr_expand (p->u.l.hi, ns, s);
	if (etmp->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	  print_expr (stderr, p->u.l.hi);
	  fprintf (stderr, "\nNot a constant int\n");
	  exit (1);
	}
	if (p->u.l.lo) {
	  ihi = etmp->u.v;
	}
	else {
	  ihi = etmp->u.v-1;
	}
	//FREE (etmp);

	vx = s->LookupVal (p->u.l.id);
	vx->init = 1;
	vx->u.idx = s->AllocPInt();

	pxrethd = NULL;
	pxrettl = NULL;
	for (i = ilo; i <= ihi; i++) {
	  s->setPInt (vx->u.idx, i);
	  px = prs_expand (p->u.l.p, ns, s);
	  if (!pxrethd) {
	    pxrethd = px;
	    pxrettl = pxrethd;
	    while (pxrettl->next) {
	      pxrettl = pxrettl->next;
	    }
	  }
	  else {
	    pxrettl->next = px;
	    while (pxrettl->next) {
	      pxrettl = pxrettl->next;
	    }
	  }
	}
	FREE (tmp);
	tmp = pxrethd;
	s->DeallocPInt (vx->u.idx, 1);
      }
      s->Del (p->u.l.id);
      break;
      
    case ACT_PRS_TREE:
      if (p->u.l.lo) {
	etmp = expr_expand (p->u.l.lo, ns, s);
	if (etmp->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "tree<> expression is not of type pint\n");
	  fprintf (stderr, " expr: ");
	  print_expr (stderr, p->u.l.lo);
	  fprintf (stderr, "\n");
	  exit (1);
	}
      }
      else {
	etmp = NULL;
      }
      tmp->u.l.lo = etmp;
      tmp->u.l.hi = NULL;
      tmp->u.l.id = NULL;
      tmp->u.l.p = prs_expand (p->u.l.p, ns, s);
      break;
      
    case ACT_PRS_SUBCKT:
      tmp->u.l.id = p->u.l.id;
      tmp->u.l.lo = NULL;
      tmp->u.l.hi = NULL;
      tmp->u.l.p = prs_expand (p->u.l.p, ns, s);
      break;
      
    default:
      Assert (0, "Should not be here");
      break;
    }
    if (tmp) {
      if (!hd) {
	hd = tmp;
	tl = tmp;
      }
      else {
	tl->next = tmp;
      }
      while (tl->next) {
	tl = tl->next;
      }
    }
    p = q_step (p);
  }
  return hd;
}

act_chp *chp_expand (act_chp *c, ActNamespace *ns, Scope *s)
{
  act_chp *ret;
  if (!c) return NULL;

  NEW (ret, act_chp);
  
  ret->vdd = fullexpand_var (c->vdd, ns, s);
  ret->gnd = fullexpand_var (c->gnd, ns, s);
  ret->psc = fullexpand_var (c->psc, ns, s);
  ret->nsc = fullexpand_var (c->nsc, ns, s);
  ret->c = chp_expand (c->c, ns, s);
  
  return ret;
}

act_chp_lang_t *chp_expand (act_chp_lang_t *c, ActNamespace *ns, Scope *s)
{
  act_chp_lang_t *ret;
  act_chp_gc_t *gchd, *gctl, *gctmp, *tmp;
  listitem_t *li;
  ValueIdx *vx;
  
  if (!c) return NULL;
  NEW (ret, act_chp_lang_t);
  ret->space = NULL;
  ret->type = c->type;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    ret->u.semi_comma.cmd = list_new ();
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      list_append (ret->u.semi_comma.cmd, chp_expand ((act_chp_lang_t *)list_value (li), ns, s));
    }
    break;

  case ACT_CHP_COMMALOOP:
  case ACT_CHP_SEMILOOP:
    {
      int ilo, ihi;
      act_syn_loop_setup (ns, s, c->u.loop.id, c->u.loop.lo, c->u.loop.hi,
		      &vx, &ilo, &ihi);

      if (c->type == ACT_CHP_COMMALOOP) {
	ret->type = ACT_CHP_COMMA;
      }
      else {
	ret->type = ACT_CHP_SEMI;
      }
      ret->u.semi_comma.cmd = list_new ();
      for (int iter=ilo; iter <= ihi; iter++) {
	s->setPInt (vx->u.idx, iter);
	list_append (ret->u.semi_comma.cmd,
		     chp_expand (c->u.loop.body, ns, s));
	
      }
      act_syn_loop_teardown (ns, s, c->u.loop.id, vx);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    gchd = NULL;
    gctl = NULL;
    for (gctmp = c->u.gc; gctmp; gctmp = gctmp->next) {
      if (gctmp->id) {
	int ilo, ihi;

	act_syn_loop_setup (ns, s, gctmp->id, gctmp->lo, gctmp->hi,
			&vx, &ilo, &ihi);
	
	for (int iter=ilo; iter <= ihi; iter++) {
	  s->setPInt (vx->u.idx, iter);
	  NEW (tmp, act_chp_gc_t);
	  tmp->next = NULL;
	  tmp->id = NULL;
	  tmp->g = expr_expand (gctmp->g, ns, s);
	  if (tmp->g && expr_is_a_const (tmp->g) && tmp->g->type == E_FALSE) {
	    FREE (tmp);
	  }
	  else {
	    tmp->s = chp_expand (gctmp->s, ns, s);
	    q_ins (gchd, gctl, tmp);
	  }
	}
	act_syn_loop_teardown (ns, s, gctmp->id, vx);
      }
      else {
	NEW (tmp, act_chp_gc_t);
	tmp->id = NULL;
	tmp->next = NULL;
	tmp->g = expr_expand (gctmp->g, ns, s);
	if (tmp->g && expr_is_a_const (tmp->g) && tmp->g->type == E_FALSE &&
	    c->type != ACT_CHP_DOLOOP) {
	  FREE (tmp);
	}
	else {
	  tmp->s = chp_expand (gctmp->s, ns, s);
	  q_ins (gchd, gctl, tmp);
	}
      }
    }
    if (!gchd) {
      /* ok this is false -> skip */
      NEW (tmp, act_chp_gc_t);
      tmp->id = NULL;
      tmp->next = NULL;
      
      tmp->g = const_expr_bool (0);
      NEW (tmp->s, act_chp_lang);
      tmp->s->type = ACT_CHP_SKIP;
      q_ins (gchd, gctl, tmp);
    }
    if (c->type != ACT_CHP_DOLOOP) {
      if (gchd->next == NULL && (!gchd->g || expr_is_a_const (gchd->g))) {
	/* loops and selections that simplify to a single guard that
	   is constant */
	if (c->type == ACT_CHP_LOOP) {
	  if (gchd->g && gchd->g->type == E_FALSE) {
	    /* whole thing is a skip */
	    /* XXX: need chp_free */
	    ret->u.gc = gchd;
	    chp_free (ret);
	    NEW (ret, act_chp_lang);
	    ret->type = ACT_CHP_SKIP;
	    return ret;
	  }
	}
	else {
	  if (!gchd->g || gchd->g->type == E_TRUE) {
	    /* whole thing is the body */
	    
	    FREE (ret);
	    ret = gchd->s;
	    FREE (gchd);
	    if (!ret) {
	      NEW (ret, act_chp_lang);
	      ret->type = ACT_CHP_SKIP;
	    }
	    return ret;
	  }
	}
      }
    }
    ret->u.gc = gchd;
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    ret->u.assign.id = expand_var_write (c->u.assign.id, ns, s);
    ret->u.assign.e = expr_expand (c->u.assign.e, ns, s);
    break;
    
  case ACT_CHP_SEND:
    ret->u.comm.chan = expand_var_chan (c->u.comm.chan, ns, s);
    ret->u.comm.rhs = list_new ();
    for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
      list_append (ret->u.comm.rhs,
		   expr_expand ((Expr *)list_value (li), ns, s));
    }
    break;
    
  case ACT_CHP_RECV:
    ret->u.comm.chan = expand_var_chan (c->u.comm.chan, ns, s);
    ret->u.comm.rhs = list_new ();
    for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
      list_append (ret->u.comm.rhs,
		   expand_var_write ((ActId *)list_value (li), ns, s));
    }
    break;

  case ACT_CHP_FUNC:
    ret->u.func.name = c->u.func.name;
    ret->u.func.rhs = list_new ();
    for (li = list_first (c->u.func.rhs); li; li = list_next (li)) {
      act_func_arguments_t *arg, *ra;
      NEW (arg, act_func_arguments_t);
      ra = (act_func_arguments_t *) list_value (li);
      arg->isstring = ra->isstring;
      if (ra->isstring) {
	arg->u.s = ra->u.s;
      }
      else {
	arg->u.e = expr_expand (ra->u.e, ns, s);
      }
      list_append (ret->u.func.rhs, arg);
    }
    break;
    
  default:
    break;
  }
  return ret;
}


act_spec *spec_expand (act_spec *s, ActNamespace *ns, Scope *sc)
{
  act_spec *ret = NULL;
  act_spec *prev = NULL;
  act_spec *tmp = NULL;

  while (s) {
    NEW (tmp, act_spec);
    tmp->type = s->type;
    tmp->count = s->count;
    tmp->isrequires = s->isrequires;
    MALLOC (tmp->ids, ActId *, tmp->count);
    for (int i=0; i < tmp->count-1; i++) {
      tmp->ids[i] = s->ids[i]->Expand (ns, sc);
    }
    if (tmp->type != -1) {
      tmp->ids[tmp->count-1] = s->ids[tmp->count-1]->Expand (ns, sc);
    }
    else {
      tmp->ids[tmp->count-1] = (ActId *) (s->ids[tmp->count-1] ?
				expr_expand ((Expr *)s->ids[tmp->count-1],
					     ns, sc) : NULL);
    }
    tmp->extra = s->extra;
    tmp->next = NULL;
    if (prev) {
      prev->next = tmp;
    }
    else {
      ret = tmp;
    }
    prev = tmp;
    s = s->next;
  }
  return ret;
}

const char *act_spec_string (int type)
{
  static int num = -1;
  static char **opts;

  if (num == -1) {
    num = config_get_table_size ("act.spec_types");
    opts = config_get_table_string ("act.spec_types");
  }

  if (type == -1) {
    return "timing";
  }

  if (type < 0 || type >= num) {
    return NULL;
  }
  return opts[type];
}

static void _print_attr (FILE *fp, act_attr_t *a)
{
  fprintf (fp, "[");
  while (a) {
    fprintf (fp, "%s=", a->attr);
    print_expr (fp, a->e);
    if (a->next) {
      fprintf (fp, "; ");
    }
    a = a->next;
  }
  fprintf (fp, "]");
}

static void _print_size (FILE *fp, act_size_spec_t *sz)
{
  if (sz) {
    fprintf (fp, "<");
    if (sz->w) {
      print_expr (fp, sz->w);
      if (sz->l) {
	fprintf (fp, ",");
	print_expr (fp, sz->l);
      }
      if (sz->flavor != 0) {
	fprintf (fp, ",%s", act_dev_value_to_string (sz->flavor));
      }
    }
    if (sz->folds) {
      fprintf (fp, ";");
      print_expr (fp, sz->folds);
    }
    fprintf (fp, ">");
  }
}

void act_print_size (FILE *fp, act_size_spec_t *sz)
{
  _print_size (fp, sz);
}

static void _print_prs_expr (FILE *fp, act_prs_expr_t *e, int prec)
{
  if (!e) return;
 
#define PREC_BEGIN(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (fp, "(");			\
    }						\
  } while (0)

#define PREC_END(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (fp, ")");			\
    }						\
  } while (0)

#define EMIT_BIN(myprec,sym)					\
  do {								\
    PREC_BEGIN(myprec);						\
    _print_prs_expr (fp, e->u.e.l, (myprec));			\
    fprintf (fp, "%s", (sym));					\
    if (e->u.e.pchg) {						\
      fprintf (fp, "{%c", e->u.e.pchg_type ? '+' : '-');	\
      _print_prs_expr (fp, e->u.e.pchg, 0);				\
      fprintf (fp, "}");					\
    }								\
    _print_prs_expr (fp, e->u.e.r, (myprec));			\
    PREC_END (myprec);						\
  } while (0)

#define EMIT_UNOP(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    fprintf (fp, "%s", sym);			\
    _print_prs_expr (fp, e->u.e.l, (myprec));	\
    PREC_END (myprec);				\
  } while (0)
    
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
    EMIT_BIN(3,"&");
    break;
  case ACT_PRS_EXPR_OR:
    EMIT_BIN(2,"|");
    break;
  case ACT_PRS_EXPR_NOT:
    EMIT_UNOP(4, "~");
    break;

  case ACT_PRS_EXPR_TRUE:
    fprintf (fp, "true");
    break;

  case ACT_PRS_EXPR_FALSE:
    fprintf (fp, "false");
    break;

  case ACT_PRS_EXPR_LABEL:
    fprintf (fp, "@%s", e->u.l.label);
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fprintf (fp, "(%c%s:", e->type == ACT_PRS_EXPR_ANDLOOP ? '&' : '|',
	     e->u.loop.id);
    print_expr (fp, e->u.loop.lo);
    if (e->u.loop.hi) {
      fprintf (fp, " .. ");
      print_expr (fp, e->u.loop.hi);
    }
    fprintf (fp, ":");
    _print_prs_expr (fp, e->u.loop.e, 0);
    break;

  case ACT_PRS_EXPR_VAR:
    e->u.v.id->Print (fp);
    if (e->u.v.sz) {
      _print_size (fp, e->u.v.sz);
    }
    break;
  default:
    fatal_error ("What?");
    break;
  }
}

static void _print_one_prs (FILE *fp, act_prs_lang_t *prs)
{
  if (!prs) return;
  switch (prs->type) {
  case ACT_PRS_RULE:
    if (prs->u.one.attr) {
      _print_attr (fp, prs->u.one.attr);
    }
    _print_prs_expr (fp, prs->u.one.e, 0);
    if (prs->u.one.arrow_type == 0) {
      fprintf (fp, " -> ");
    }
    else if (prs->u.one.arrow_type == 1) {
      fprintf (fp, " => ");
    }
    else if (prs->u.one.arrow_type == 2) {
      fprintf (fp, " #> ");
    }
    else {
      fatal_error ("Eh?");
    }
    if (prs->u.one.label) {
      fprintf (fp, "@%s", (char *)prs->u.one.id);
    }
    else {
      prs->u.one.id->Print (fp);
    }
    if (prs->u.one.dir) {
      fprintf (fp, "+\n");
    }
    else {
      fprintf (fp, "-\n");
    }
    break;
  case ACT_PRS_GATE:
    if (prs->u.p.attr) {
      _print_attr (fp, prs->u.p.attr);
    }
    if (prs->u.p.g && prs->u.p._g) {
      fprintf (fp, "transgate");
    }
    else if (prs->u.p.g) {
      fprintf (fp, "passn");
    }
    else if (prs->u.p._g) {
      fprintf (fp, "passp");
    }
    else {
      Assert (0, "Hmm");
    }
    if (prs->u.p.sz) {
      _print_size (fp, prs->u.p.sz);
    }
    fprintf (fp, "(");
    if (prs->u.p.g) {
      prs->u.p.g->Print (fp);
      fprintf (fp, ",");
    }
    if (prs->u.p._g) {
      prs->u.p._g->Print (fp);
      fprintf (fp, ",");
    }
    prs->u.p.s->Print (fp);
    fprintf (fp, ",");
    prs->u.p.d->Print (fp);
    fprintf (fp, ")\n");
    break;
  case ACT_PRS_LOOP:
    fprintf (fp, "(%s:", prs->u.l.id);
    if (prs->u.l.lo) {
      print_expr (fp, prs->u.l.lo);
      fprintf (fp, " .. ");
    }
    print_expr (fp, prs->u.l.hi);
    fprintf (fp, ":");
    for (act_prs_lang_t *i = prs->u.l.p; i; i = i->next) {
      _print_one_prs (fp, i);
    }
    fprintf (fp, ")\n");
    break;
  case ACT_PRS_TREE:
    fprintf (fp, "tree");
    if (prs->u.l.lo) {
      fprintf (fp, "<");
      print_expr (fp, prs->u.l.lo);
      fprintf (fp, ">");
    }
    fprintf (fp, "{\n");
    for (act_prs_lang_t *i = prs->u.l.p; i; i = i->next) {
      _print_one_prs (fp, i);
    }
    fprintf (fp, "}\n");
    break;
  case ACT_PRS_SUBCKT:
    fprintf (fp, "subckt");
    if (prs->u.l.id) {
      fprintf (fp, "<%s>", prs->u.l.id);
    }
    fprintf (fp, "{\n");
    for (act_prs_lang_t *i = prs->u.l.p; i; i = i->next) {
      _print_one_prs (fp, i);
    }
    fprintf (fp, "}\n");
    break;
  default:
    fatal_error ("Unsupported");
    break;
  }
}

void prs_print (FILE *fp, act_prs *prs)
{
  while (prs) {
    fprintf (fp, "prs ");
    if (prs->vdd) {
      fprintf (fp, "<");
      prs->vdd->Print (fp);
      if (prs->gnd) {
	fprintf (fp, ", ");
	prs->gnd->Print (fp);
      }
      if (prs->psc) {
	fprintf (fp, " | ");
	prs->psc->Print (fp);
	fprintf (fp, ", ");
	prs->nsc->Print (fp);
      }
      fprintf (fp, "> ");
    }
    fprintf (fp, "{\n");
    act_prs_lang_t *p;
    for (p = prs->p; p; p = p->next) {
      _print_one_prs (fp, p);
    }
    fprintf (fp, "}\n");
    prs = prs->next;
  }
}

void chp_print (FILE *fp, act_chp_lang_t *c)
{
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    {
      listitem_t *li;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	chp_print (fp, (act_chp_lang_t *)list_value (li));
	if (list_next (li)) {
	  if (c->type == ACT_CHP_COMMA) {
	    fprintf (fp, ",");
	  }
	  else {
	    fprintf (fp, ";");
	  }
	}
      }
    }
    break;

  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    fprintf (fp, "*");
  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    fprintf (fp, "[");
    if (c->type == ACT_CHP_SELECT_NONDET) {
      fprintf (fp, "|");
    }
    {
      act_chp_gc_t *gc = c->u.gc;

      if (c->type == ACT_CHP_DOLOOP) {
	fprintf (fp, " ");
	chp_print (fp, gc->s);
	fprintf (fp, " <- ");
	if (gc->g) {
	  print_expr (fp, gc->g);
	}
	else {
	  fprintf (fp, "true");
	}
      }
      else {
	while (gc) {
	  if (!gc->g) {
	    if (c->type == ACT_CHP_LOOP) {
	      fprintf (fp, "true");
	    }
	    else {
	      fprintf (fp, "else");
	    }
	  }
	  else {
	    print_expr (fp, gc->g);
	  }
	  if (gc->s) {
	    fprintf (fp, " -> ");
	    chp_print (fp, gc->s);
	  }
	  if (gc->next) {
	    fprintf (fp, " [] ");
	  }
	  gc = gc->next;
	}
      }
    }
    if (c->type == ACT_CHP_SELECT_NONDET) {
      fprintf (fp, "|");
    }
    fprintf (fp, "]");
    break;
    
  case ACT_CHP_SKIP:
    fprintf (fp, "skip");
    break;

  case ACT_CHP_ASSIGN:
    c->u.assign.id->Print (fp);
    fprintf (fp, ":=");
    print_expr (fp, c->u.assign.e);
    break;
    
  case ACT_CHP_SEND:
    c->u.comm.chan->Print (fp);
    fprintf (fp, "!");
    if (list_length (c->u.comm.rhs) > 1) {
      fprintf (fp, "(");
    }
    {
      listitem_t *li;
      for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
	print_expr (fp, (Expr *)list_value (li));
	if (list_next (li)) {
	  fprintf (fp, ",");
	}
      }
    }
    if (list_length (c->u.comm.rhs) > 1) {
      fprintf (fp, ")");
    }
    break;
    
  case ACT_CHP_RECV:
    c->u.comm.chan->Print (fp);
    fprintf (fp, "?");
    if (list_length (c->u.comm.rhs) > 1) {
      fprintf (fp, "(");
    }
    {
      listitem_t *li;
      for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
	((ActId *)list_value (li))->Print (fp);
	if (list_next (li)) {
	  fprintf (fp, ",");
	}
      }
    }
    if (list_length (c->u.comm.rhs) > 1) {
      fprintf (fp, ")");
    }
    break;

  case ACT_CHP_FUNC:
  default:
    fatal_error ("Unknown type");
    break;
  }
}

void chp_print (FILE *fp, act_chp *chp)
{
  if (chp) {
    fprintf (fp, "chp {\n");
    chp_print (fp, chp->c);
    fprintf (fp, "\n}\n");
    //chp = chp->next;
  }
}

void hse_print (FILE *fp, act_chp *chp)
{
  if (chp) {
    fprintf (fp, "hse {\n");
    chp_print (fp, chp->c);
    fprintf (fp, "\n}\n");
    //chp = chp->next;
  }
}


void spec_print (FILE *fp, act_spec *spec)
{
  int count = config_get_table_size ("act.spec_types");
  char **specs = config_get_table_string ("act.spec_types");
  fprintf (fp, "spec {\n");
  while (spec) {
    if (spec->type == -1) {
      fprintf (fp, "   timing ");

#define SPEC_PRINT_ID(x)			\
      do {					\
	if (spec->extra[x] & 0x04) {		\
	  fprintf (fp, "?");			\
	}					\
	spec->ids[x]->Print (fp);		\
	if (spec->extra[x] & 0x8) {		\
	  fprintf (fp, "*");			\
	}					\
	if (spec->extra[x] & 0x3) {		\
	  if ((spec->extra[x] & 0x03) == 1) {	\
	    fprintf (fp, "+");			\
	  }					\
	  else {				\
	    fprintf (fp, "-");			\
	  }					\
	}					\
      } while (0)

      if (spec->ids[0]) {
	SPEC_PRINT_ID(0);
	fprintf (fp, " : ");
      }
      SPEC_PRINT_ID (1);
      fprintf (fp, " < ");
      if (spec->ids[3]) {
	fprintf (fp, "[");
	print_expr (fp, (Expr *)spec->ids[3]);
	fprintf (fp, "] ");
      }
      SPEC_PRINT_ID(2);
      fprintf (fp, "\n");
    }
    else {
      Assert (spec->type >= 0 && spec->type < count, "What?");
      fprintf (fp, "   %s(", specs[spec->type]);
      for (int i=0; i < spec->count; i++) {
	if (i != 0) {
	  fprintf (fp, ", ");
	}
	spec->ids[i]->Print (fp);
      }
      fprintf (fp, ")\n");
    }
    spec = spec->next;
  }
  fprintf (fp, "}\n");
}

void refine_expand (act_refine *r, ActNamespace *ns, Scope *s)
{
  if (!r) return;
  if (r->b) {
    r->b->Expandlist (ns, s);
  }
}

act_languages *act_languages::Expand (ActNamespace *ns, Scope *s)
{
  act_languages *ret = new act_languages ();
  if (chp) {
    ret->chp = chp_expand (chp, ns, s);
  }
  if (hse) {
    ret->hse = chp_expand (hse, ns, s);
  }
  if (prs) {
    ret->prs = prs_expand (prs, ns, s);
  }
  if (refine) {
    refine_expand (refine, ns, s);
  }
  if (sizing) {
    ret->sizing = sizing_expand (sizing, ns, s);
  }
  return ret;
}


void refine_print (FILE *fp, act_refine *r)
{
  if (!r || !r->b) return;
  r->b->Print (fp);
}

void sizing_print (FILE *fp, act_sizing *s)
{
  while (s) {
    fprintf (fp, "sizing {");
    if (s->p_specified) {
      fprintf (fp, "   p_n_mode ");
      print_expr (fp, s->p_n_mode_e);
      fprintf (fp, ";\n");
    }
    if (s->unit_n_specified) {
      fprintf (fp, "   unit_n ");
      print_expr (fp, s->unit_n_e);
      fprintf (fp, ";\n");
    }
    for (int i=0; i < A_LEN (s->d); i++) {
      fprintf (fp, "   ");
      s->d[i].id->Print (fp);
      fprintf (fp, " { ");
      if (s->d[i].eup) {
	fprintf (fp, "+ ");
	print_expr (fp, s->d[i].eup);
	if (s->d[i].flav_up != 0) {
	  fprintf (fp, ",%s", act_dev_value_to_string (s->d[i].flav_up));
	}
	if (s->d[i].upfolds) {
	  fprintf (fp, ";");
	  print_expr (fp, s->d[i].upfolds);
	}
      }
      if (s->d[i].edn) {
	if (s->d[i].eup) {
	  fprintf (fp, ", ");
	}
	fprintf (fp, "- ");
	print_expr (fp, s->d[i].edn);
	if (s->d[i].flav_dn != 0) {
	  fprintf (fp, ",%s", act_dev_value_to_string (s->d[i].flav_dn));
	}
	if (s->d[i].dnfolds) {
	  fprintf (fp, ";");
	  print_expr (fp, s->d[i].dnfolds);
	}
      }
      fprintf (fp, " };\n");
    }
    fprintf (fp, "}\n");
    s = s->next;
  }
}

act_sizing *sizing_expand (act_sizing *sz, ActNamespace *ns, Scope *s)
{
  if (!sz) return NULL;
  act_sizing *ret;
  Expr *te;
  NEW (ret, act_sizing);
  ret->next = NULL;
  ret->p_specified = sz->p_specified;
  ret->unit_n_specified = sz->unit_n_specified;
  if (ret->p_specified) {
    te = expr_expand (sz->p_n_mode_e, ns, s);
    ret->p_n_mode_e = te;
    if (te && te->type == E_INT) {
      ret->p_n_mode = te->u.v;
    }
    else if (te && te->type == E_REAL) {
      ret->p_n_mode = te->u.f;
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("Expression for p_n_mode is not a const?");
    }
  }
  if (ret->unit_n_specified) {
    te = expr_expand (sz->unit_n_e, ns, s);
    ret->unit_n_e = te;
    if (te && te->type == E_INT) {
      ret->unit_n = te->u.v;
    }
    else if (te && te->type == E_REAL) {
      ret->unit_n = te->u.f;
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("Expression for unit_n is not a const?");
    }
  }
  A_INIT (ret->d);
  if (A_LEN (sz->d) > 0) {
    for (int i=0; i < A_LEN (sz->d); i++) {
      A_NEW (ret->d, act_sizing_directive);
      A_NEXT (ret->d).id = sz->d[i].id->Expand (ns, s);
      A_NEXT (ret->d).eup = expr_expand (sz->d[i].eup, ns, s);
      A_NEXT (ret->d).edn = expr_expand (sz->d[i].edn, ns, s);
      A_NEXT (ret->d).upfolds = expr_expand (sz->d[i].upfolds, ns, s);
      A_NEXT (ret->d).dnfolds = expr_expand (sz->d[i].dnfolds, ns, s);
      A_NEXT (ret->d).flav_up = sz->d[i].flav_up;
      A_NEXT (ret->d).flav_dn = sz->d[i].flav_dn;
      A_INC (ret->d);
    }
  }
  return ret;
}


/* utility functions for expanded rules */

static act_prs_expr_t *_copy_rule (act_prs_expr_t *e)
{
  act_prs_expr_t *ret;
  
  if (!e) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = e->type;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l = _copy_rule (e->u.e.l);
    ret->u.e.r = _copy_rule (e->u.e.r);
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = _copy_rule (e->u.e.l);
    ret->u.e.r = NULL;
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v = e->u.v;
    ret->u.v.sz = NULL;
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l = e->u.l;
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
  return ret;
}

act_prs_expr_t *act_prs_complement_rule (act_prs_expr_t *e)
{
  act_prs_expr_t *r;
  NEW (r, act_prs_expr_t);
  r->type = ACT_PRS_EXPR_NOT;
  r->u.e.r = NULL;
  r->u.e.pchg = NULL;
  r->u.e.pchg_type = -1;
  r->u.e.l = _copy_rule (e);
  return r;
}

static void _twiddle_leaves (act_prs_expr_t *e)
{
  act_prs_expr_t *tmp;
  
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    _twiddle_leaves (e->u.e.l);
    _twiddle_leaves (e->u.e.r);
    break;

  case ACT_PRS_EXPR_NOT:
    _twiddle_leaves (e->u.e.l);
    break;

  case ACT_PRS_EXPR_VAR:
    tmp = _copy_rule (e);
    e->type =ACT_PRS_EXPR_NOT;
    e->u.e.l = tmp;
    e->u.e.r = NULL;
    e->u.e.pchg = NULL;
    e->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_LABEL:
    tmp = _copy_rule (e);
    e->type =ACT_PRS_EXPR_NOT;
    e->u.e.l = tmp;
    e->u.e.r = NULL;
    e->u.e.pchg = NULL;
    e->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_TRUE:
    e->type = ACT_PRS_EXPR_FALSE;
    break;
    
  case ACT_PRS_EXPR_FALSE:
    e->type = ACT_PRS_EXPR_TRUE;
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
}

act_prs_expr_t *act_prs_celement_rule (act_prs_expr_t *e)
{
  act_prs_expr_t *r = _copy_rule (e);
  _twiddle_leaves (r);
  return r;
}

void initialize_print (FILE *fp, act_initialize *init)
{
  listitem_t *li;
  if (!init) return;
  if (!init->actions) return;

  fprintf (fp, "Initialize {\n");
  for (li = list_first (init->actions); li; li = list_next (li)) {
    act_chp_lang_t *c = (act_chp_lang_t *)list_value (li);
    fprintf (fp, " actions { ");
    chp_print (fp, c);
    fprintf (fp, " }");
    if (list_next (li)) {
      fprintf (fp, ";");
    }
    fprintf (fp, "\n");
  }
  fprintf (fp, "}\n");
}
  
act_initialize *initialize_expand (act_initialize *init, ActNamespace *ns,
				   Scope *s)
{
  act_initialize *ret;
  if (!init) return NULL;
  NEW (ret, act_initialize);
  if (!init->actions) {
    ret->actions = NULL;
  }
  else {
    listitem_t *li;
    ret->actions = list_new ();
    for (li = list_first (init->actions); li; li = list_next (li)) {
      act_chp_lang_t *c = (act_chp_lang_t *) list_value (li);
      list_append (ret->actions, chp_expand (c, ns, s));
    }
  }
  return ret;
}


void dflow_print (FILE *fp, act_dataflow *d)
{
  listitem_t *li;
  act_dataflow_element *e;
  
  if (!d) return;
  fprintf (fp, "dataflow {\n");
  for (li = list_first (d->dflow); li; li = list_next (li)) {
    e = (act_dataflow_element *) list_value (li);
    switch (e->t) {
    case ACT_DFLOW_FUNC:
      print_expr (fp, e->u.func.lhs);
      fprintf (fp, " -> ");
      if (e->u.func.nbufs) {			\
	fprintf (fp, "[");			\
	print_expr (fp, e->u.func.nbufs);	\
	if (e->u.func.init) {			\
	  fprintf (fp, ",");
	  print_expr (fp, e->u.func.init);
	}
	fprintf (fp, "] ");
      }
      e->u.func.rhs->Print (fp);
      break;

    case ACT_DFLOW_SPLIT:
      fprintf (fp, "{");
      e->u.splitmerge.guard->Print (fp);
      fprintf (fp, "} ");
      e->u.splitmerge.single->Print (fp);
      fprintf (fp, " -> ");
      for (int i=0; i < e->u.splitmerge.nmulti; i++) {
	if (e->u.splitmerge.multi[i]) {
	  e->u.splitmerge.multi[i]->Print (fp);
	}
	else {
	  fprintf (fp, "*");
	}
	if (i != e->u.splitmerge.nmulti-1) {
	  fprintf (fp, ", ");
	}
      }
      break;
      
    case ACT_DFLOW_MERGE:
    case ACT_DFLOW_MIXER:
    case ACT_DFLOW_ARBITER:
      fprintf (fp, "{");
      if (e->t == ACT_DFLOW_MERGE) {
	e->u.splitmerge.guard->Print (fp);
      }
      else if (e->t == ACT_DFLOW_MIXER) {
	fprintf (fp, "*");
      }
      else {
	fprintf (fp, "|");
      }
      fprintf (fp, "} ");
      for (int i=0; i < e->u.splitmerge.nmulti; i++) {
	if (e->u.splitmerge.multi[i]) {
	  e->u.splitmerge.multi[i]->Print (fp);
	}
	else {
	  fprintf (fp, "*");
	}
	if (i != e->u.splitmerge.nmulti-1) {
	  fprintf (fp, ", ");
	}
      }
      fprintf (fp, " -> ");
      e->u.splitmerge.single->Print (fp);
      break;
      
    default:
      fatal_error ("What?");
      break;
    }
    if (list_next (li)) {
      fprintf (fp, ";");
    }
    fprintf (fp, "\n");
  }
  fprintf (fp, "}\n");
}
  
act_dataflow *dflow_expand (act_dataflow *d, ActNamespace *ns, Scope *s)
{
  listitem_t *li;
  act_dataflow_element *e, *f;
  act_dataflow *ret;
  
  if (!d) return NULL;
  NEW (ret, act_dataflow);
  ret->dflow = list_new ();
  for (li = list_first (d->dflow); li; li = list_next (li)) {
    e = (act_dataflow_element *) list_value (li);
    NEW (f, act_dataflow_element);
    f->t = e->t;
    switch (e->t) {
    case ACT_DFLOW_FUNC:
      f->u.func.lhs = expr_expand (e->u.func.lhs, ns, s);
      f->u.func.nbufs = NULL;
      f->u.func.init  = NULL;
      if (e->u.func.nbufs) {
	f->u.func.nbufs = expr_expand (e->u.func.nbufs, ns, s);
	if (e->u.func.init) {
	  f->u.func.init = expr_expand (e->u.func.init, ns, s);
	}
      }
      f->u.func.rhs = e->u.func.rhs->Expand (ns, s);
      break;

    case ACT_DFLOW_SPLIT:
    case ACT_DFLOW_MERGE:
    case ACT_DFLOW_MIXER:
    case ACT_DFLOW_ARBITER:
      if (e->u.splitmerge.guard) {
	f->u.splitmerge.guard = e->u.splitmerge.guard->Expand (ns, s);
      }
      else {
	f->u.splitmerge.guard = NULL;
      }
      f->u.splitmerge.nmulti = e->u.splitmerge.nmulti;
      MALLOC (f->u.splitmerge.multi, ActId *, f->u.splitmerge.nmulti);
      for (int i=0; i < f->u.splitmerge.nmulti; i++) {
	if (e->u.splitmerge.multi[i]) {
	  f->u.splitmerge.multi[i] = e->u.splitmerge.multi[i]->Expand (ns, s);
	}
	else {
	  f->u.splitmerge.multi[i] = NULL;
	}
      }
      f->u.splitmerge.single = e->u.splitmerge.single->Expand (ns, s);
      break;

    default:
      fatal_error ("What?");
      break;
    }
    list_append (ret->dflow, f);
  }
  return ret;
}
  
