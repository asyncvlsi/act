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
#include <act/types.h>
#include <act/body.h>
#include <act/value.h>
#include <string.h>

#define PRINT_STEP				\
  do {						\
    len = strlen (buf+k);			\
    k += len;					\
    sz -= len;					\
    if (sz <= 1) return;			\
  } while (0)

/*
  Expression helper routines, not in the standard expr.c 
*/


/*
  Precedence printing

  highest

  10  #, ~, !
   9  * / %
   8  + -
   7  <<, >>, <, >, <=, >=, ==, !=
   6  & 
   5  ^
   4  |
   3  ?

   called with incoming precdence.
   If my precedence is higher, no parens is necessary
   otherwise parenthesize it.

*/
static void _print_expr (char *buf, int sz, Expr *e, int prec)
{
  int k = 0;
  int len;
  char *s;
  if (!e) return;
 
#define PREC_BEGIN(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      snprintf (buf+k, sz, "(");		\
      PRINT_STEP;				\
    }						\
  } while (0)

#define PREC_END(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      snprintf (buf+k, sz, ")");		\
      PRINT_STEP;				\
    }						\
  } while (0)

#define EMIT_BIN(myprec,sym)				\
  do {							\
    PREC_BEGIN(myprec);					\
    _print_expr (buf+k, sz, e->u.e.l, (myprec));	\
    PRINT_STEP;						\
    snprintf (buf+k, sz, "%s", (sym));			\
    PRINT_STEP;						\
    _print_expr (buf+k, sz, e->u.e.r, (myprec));	\
    PRINT_STEP;						\
    PREC_END (myprec);					\
  } while (0)

#define EMIT_UNOP(myprec,sym)				\
  do {							\
    PREC_BEGIN(myprec);					\
    snprintf (buf+k, sz, "%s", sym);			\
    PRINT_STEP;						\
    _print_expr (buf+k, sz, e->u.e.l, (myprec));	\
    PRINT_STEP;						\
    PREC_END (myprec);					\
  } while (0)
    
  switch (e->type) {
  case E_PROBE:
    PREC_BEGIN (10);
    snprintf (buf+k, sz, "#");
    PRINT_STEP;
    ((ActId *)(e->u.e.l))->sPrint (buf+k, sz);
    PRINT_STEP;
    PREC_END (10);
    break;
    
  case E_NOT: EMIT_UNOP(10, "!"); break;
  case E_COMPLEMENT: EMIT_UNOP(10, "~"); break;
  case E_UMINUS: EMIT_UNOP(10, "-"); break;

  case E_MULT: EMIT_BIN (9, "*"); break;
  case E_DIV:  EMIT_BIN (9, "/"); break;
  case E_MOD:  EMIT_BIN (9, "%"); break;

  case E_PLUS:  EMIT_BIN (8, "+"); break;
  case E_MINUS: EMIT_BIN (8, "-"); break;

  case E_LSL: EMIT_BIN (7, "<<"); break;
  case E_LSR: EMIT_BIN (7, ">>"); break;
  case E_ASR: EMIT_BIN (7, ">>>"); break;
  case E_LT:  EMIT_BIN (7, "<"); break;
  case E_GT:  EMIT_BIN (7, ">"); break;
  case E_LE:  EMIT_BIN (7, "<="); break;
  case E_GE:  EMIT_BIN (7, ">="); break;
  case E_EQ:  EMIT_BIN (7, "=="); break;
  case E_NE:  EMIT_BIN (7, "!="); break;

  case E_AND: EMIT_BIN (6, "&"); break;
    
  case E_XOR: EMIT_BIN (5, "^"); break;

  case E_OR: EMIT_BIN (4, "|"); break;

  case E_QUERY: /* prec = 3 */
    PREC_BEGIN(3);
    _print_expr (buf+k, sz, e->u.e.l, 3);
    PRINT_STEP;
    snprintf (buf+k, sz, " ? ");
    PRINT_STEP;
    Assert (e->u.e.r->type == E_COLON, "Hmm");
    _print_expr (buf+k, sz, e->u.e.r->u.e.l, 3);
    PRINT_STEP;
    snprintf (buf+k, sz, " : ");
    PRINT_STEP;
    _print_expr (buf+k, sz, e->u.e.r->u.e.r, 3);
    PRINT_STEP;
    PREC_END(3);
    break;

  case E_INT:
    snprintf (buf+k, sz, "%d", e->u.v);
    PRINT_STEP;
    break;

  case E_REAL:
    snprintf (buf+k, sz, "%g", e->u.f);
    PRINT_STEP;
    break;

  case E_TRUE:
    snprintf (buf+k, sz, "true");
    PRINT_STEP;
    break;

  case E_FALSE:
    snprintf (buf+k, sz, "false");
    PRINT_STEP;
    break;
    
  case E_VAR:
    ((ActId *)e->u.e.l)->sPrint (buf+k, sz);
    PRINT_STEP;
    break;

  case E_ARRAY:
  case E_SUBRANGE:
    /* id evaluated to an array */
    {
      ValueIdx *vx = (ValueIdx *) e->u.e.l;
      Scope *s;
      Arraystep *as;
      int first = 1;
      int type;
      
      if (e->type == E_ARRAY) {
	s = (Scope *) e->u.e.r;
	as = vx->t->arrayInfo()->stepper();
      }
      else {
	s = (Scope *) e->u.e.r->u.e.l;
	as = vx->t->arrayInfo()->stepper ((Array *)e->u.e.r->u.e.r);
      }

      if (TypeFactory::isPIntType (vx->t)) {
	type = 0;
      }
      else if (TypeFactory::isPRealType (vx->t)) {
	type = 1;
      }
      else if (TypeFactory::isPBoolType (vx->t)) {
	type = 2;
      }
      else if (TypeFactory::isPIntsType (vx->t)) {
	type = 3;
      }
      else {
	Assert (0, "E_ARRAY/SUBARRAY on non-params?");
      }

      snprintf (buf+k, sz, "{");
      PRINT_STEP;
      while (!as->isend()) {
	if (!first) {
	  snprintf (buf+k, sz, ",");
	  k++, sz--;
	}
	first = 0;
	if (type == 0) {
	  Assert (s->issetPInt (vx->u.idx + as->index()), "Hmm");
	  snprintf (buf+k, sz, "%lu", s->getPInt (vx->u.idx + as->index()));
	}
	else if (type == 1) {
	  Assert (s->issetPReal (vx->u.idx + as->index()), "Hmm");
	  snprintf (buf+k, sz, "%g", s->getPReal (vx->u.idx + as->index()));
	}
	else if (type == 2) {
	  Assert (s->issetPBool (vx->u.idx + as->index()), "Hmm");
	  snprintf (buf+k, sz, "%d", s->getPBool (vx->u.idx + as->index()));
	}
	else if (type == 3) {
	  Assert (s->issetPInts (vx->u.idx + as->index()), "Hmm");
	  snprintf (buf+k, sz, "%ld", s->getPInts (vx->u.idx + as->index()));
	}
	PRINT_STEP;
	as->step();
      }
      snprintf (buf+k, sz, "}");
      PRINT_STEP;
      delete as;
    }
    break;

  case E_FUNCTION:
  case E_BITFIELD:
  default:
    fatal_error ("Unhandled case!\n");
    break;
  }
}


/*------------------------------------------------------------------------
 *  s/print_expr() is the top-level function
 *------------------------------------------------------------------------
 */
void sprint_expr (char *buf, int sz, Expr *e)
{
  _print_expr (buf, sz, e, 0);
}
  
void print_expr (FILE *fp, Expr *e)
{
  char buf[10240];
  buf[0] = '\0';
  sprint_expr (buf, 10240, e);
  fprintf (fp, "%s", buf);
}


/*
  hash ids!
*/
//#define _id_equal(a,b) ((a) == (b))
//#define _id_equal ((ActId *)(a))->isEqual ((ActId *)(b))
static int _id_equal (Expr *a, Expr *b)
{
  ActId *ia, *ib;
  ia = (ActId *)a;
  ib = (ActId *)b;
  return ia->isEqual (ib);
}

/**
 *  Compare two expressions structurally for equality
 *
 *  \param a First expression to be compared
 *  \param b Second expression to be compared
 *  \return 1 if the two are structurally identical, 0 otherwise
 */
int expr_equal (Expr *a, Expr *b)
{
  int ret;
  if (a == b) { 
    return 1;
  }
  if (a == NULL || b == NULL) {
    /* this means a != b given the previous clause */
    return 0;
  }
  if (a->type != b->type) {
    return 0;
  }
  switch (a->type) {
    /* binary */
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
  case E_EQ:
  case E_NE:
    if (expr_equal (a->u.e.l, b->u.e.l) &&
	expr_equal (a->u.e.r, b->u.e.r)) {
      return 1;
    }
    else {
      return 0;
    }
    break;
    
    /* unary */
  case E_UMINUS:
  case E_NOT:
  case E_COMPLEMENT:
    return expr_equal (a->u.e.l, b->u.e.l);
    break;

    /* special */
  case E_QUERY:
    if (expr_equal (a->u.e.l, b->u.e.l) &&
	expr_equal (a->u.e.r->u.e.l, b->u.e.r->u.e.l) &&
	expr_equal (a->u.e.r->u.e.r, b->u.e.r->u.e.r)) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_CONCAT:
    fatal_error ("Should not be here");
    ret = 1;
    while (a && ret && a->type == E_CONCAT) {
      if (!b || b->type != E_CONCAT) return 0;
      ret = expr_equal (a->u.e.l, b->u.e.l);
      if (!ret) return 0;
      a = a->u.e.r;
      b = b->u.e.r;
    }
    return 1;
    break;

  case E_BITFIELD:
    if (_id_equal (a->u.e.l, b->u.e.l) &&
	(a->u.e.r->u.e.l == b->u.e.r->u.e.l) &&
	(a->u.e.r->u.e.r == b->u.e.r->u.e.r)) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_FUNCTION:
    fatal_error ("Fix this please");
    break;

    /* leaf */
  case E_INT:
    if (a->u.v == b->u.v) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_REAL:
    if (a->u.f == b->u.f) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_VAR:
  case E_PROBE:
    /* hash the id */
    if (_id_equal (a->u.e.l, b->u.e.l)) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_TRUE:
  case E_FALSE:
    return 1;
    break;

  case E_ARRAY:
  case E_SUBRANGE:
    {
      ValueIdx *vxa = (ValueIdx *) a->u.e.l;
      ValueIdx *vxb = (ValueIdx *) b->u.e.l;
      Scope *sa, *sb;
      Arraystep *asa, *asb;
      int type;
      
      if (a->type == E_ARRAY) {
	sa = (Scope *) a->u.e.r;
	sb = (Scope *) b->u.e.r;
	asa = vxa->t->arrayInfo()->stepper();
	asb = vxb->t->arrayInfo()->stepper();
      }
      else {
	sa = (Scope *) a->u.e.r->u.e.l;
	sb = (Scope *) b->u.e.r->u.e.l;
	asa = vxa->t->arrayInfo()->stepper ((Array *)a->u.e.r->u.e.r);
	asb = vxb->t->arrayInfo()->stepper ((Array *)b->u.e.r->u.e.r);
      }

      if (TypeFactory::isPIntType (vxa->t)) {
	if (!TypeFactory::isPIntType (vxb->t)) {
	  delete asa;
	  delete asb;
	  return 0;
	}
	type = 0;
      }
      else if (TypeFactory::isPRealType (vxa->t)) {
	if (!TypeFactory::isPRealType (vxb->t)) {
	  delete asa;
	  delete asb;
	  return 0;
	}
	type = 1;
      }
      else if (TypeFactory::isPBoolType (vxa->t)) {
	if (!TypeFactory::isPBoolType (vxb->t)) {
	  delete asa;
	  delete asb;
	  return 0;
	}
	type = 2;
      }
      else if (TypeFactory::isPIntsType (vxa->t)) {
	if (!TypeFactory::isPIntsType (vxb->t)) {
	  delete asa;
	  delete asb;
	  return 0;
	}
	type = 3;
      }
      else {
	Assert (0, "E_ARRAY/SUBARRAY on non-params?");
      }
      while (!asa->isend()) {
	if (asb->isend()) {
	  delete asa;
	  delete asb;
	  return 0;
	}
	if (type == 0) {
	  Assert (sa->issetPInt (vxa->u.idx + asa->index()), "Hmm");
	  Assert (sb->issetPInt (vxb->u.idx + asb->index()), "Hmm");
	  if (sa->getPInt (vxa->u.idx + asa->index()) !=
	      sb->getPInt (vxb->u.idx + asb->index())) {
	    delete asa;
	    delete asb;
	    return 0;
	  }
	}
	else if (type == 1) {
	  Assert (sa->issetPReal (vxa->u.idx + asa->index()), "Hmm");
	  Assert (sb->issetPReal (vxb->u.idx + asb->index()), "Hmm");
	  if (sa->getPReal (vxa->u.idx + asa->index()) !=
	      sb->getPReal (vxb->u.idx + asb->index())) {
	    delete asa;
	    delete asb;
	    return 0;
	  }
	}
	else if (type == 2) {
	  Assert (sa->issetPBool (vxa->u.idx + asa->index()), "Hmm");
	  Assert (sb->issetPBool (vxb->u.idx + asb->index()), "Hmm");
	  if (sa->getPBool (vxa->u.idx + asa->index()) !=
	      sb->getPBool (vxb->u.idx + asb->index())) {
	    delete asa;
	    delete asb;
	    return 0;
	  }
	}
	else if (type == 3) {
	  Assert (sa->issetPInts (vxa->u.idx + asa->index()), "Hmm");
	  Assert (sb->issetPInts (vxb->u.idx + asb->index()), "Hmm");
	  if (sa->getPInts (vxa->u.idx + asa->index()) !=
	      sb->getPInts (vxb->u.idx + asb->index())) {
	    delete asa;
	    delete asb;
	    return 0;
	  }
	}
	asa->step();
	asb->step();
      }
      if (!asb->isend()) {
	delete asa;
	delete asb;
	return 0;
      }
      delete asa;
      delete asb;
      return 1;
    }
    break;
    
  default:
    fatal_error ("Unknown expression type?");
    return 0;
    break;
  }
  return 0;
}


/*------------------------------------------------------------------------
 * Expand expression, replacing all paramters. If it is an lval, then
 * it must be a pure identifier at the end of of the day. Default is
 * not an lval
 *------------------------------------------------------------------------
 */
Expr *expr_expand (Expr *e, ActNamespace *ns, Scope *s, int is_lval)
{
  Expr *ret, *te;
  ActId *xid;
  Expr *tmp;
  
  if (!e) return NULL;

  NEW (ret, Expr);
  ret->type = e->type;

#define LVAL_ERROR							\
    do {								\
      if (is_lval) {							\
	act_error_ctxt (stderr);					\
	fprintf (stderr, "\texpanding expr: ");				\
	print_expr (stderr, e);						\
	fprintf (stderr, "\n");						\
	fatal_error ("Invalid assignable or connectable value!");	\
      }									\
    } while (0)

  switch (e->type) {
  case E_AND:
  case E_OR:
  case E_XOR:
    LVAL_ERROR;
    ret->u.e.l = expr_expand (e->u.e.l, ns, s);
    ret->u.e.r = expr_expand (e->u.e.r, ns, s);
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
    LVAL_ERROR;
    ret->u.e.l = expr_expand (e->u.e.l, ns, s, is_lval);
    ret->u.e.r = expr_expand (e->u.e.r, ns, s, is_lval);
    if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
      if (ret->u.e.l->type == E_INT && ret->u.e.r->type == E_INT) {
	unsigned int v;

	v = ret->u.e.l->u.v;
	if (e->type == E_PLUS) {
	  v = v + ((unsigned int)ret->u.e.r->u.v);
	}
	else if (e->type == E_MINUS) {
	  v = v - ((unsigned int)ret->u.e.r->u.v);
	}
	else if (e->type == E_MULT) {
	  v = v * ((unsigned int)ret->u.e.r->u.v);
	}
	else if (e->type == E_DIV) {
	  v = v / ((unsigned int)ret->u.e.r->u.v);
	}
	else if (e->type == E_MOD) {
	  v = v % ((unsigned int)ret->u.e.r->u.v);
	}
	else if (e->type == E_LSL) {
	  v = v << ((unsigned int)ret->u.e.r->u.v);
	}
	else if (e->type == E_LSR) {
	  v = v << ((unsigned int)ret->u.e.r->u.v);
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
    LVAL_ERROR;
    ret->u.e.l = expr_expand (e->u.e.l, ns, s, is_lval);
    ret->u.e.r = expr_expand (e->u.e.r, ns, s, is_lval);
    if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
      if (ret->u.e.l->type == E_INT && ret->u.e.r->type == E_INT) {
	unsigned int v;

	v = ret->u.e.l->u.v;
	if (e->type == E_LT) {
	  v = (v < ((unsigned int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else if (e->type == E_GT) {
	  v = (v > ((unsigned int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else if (e->type == E_LE) {
	  v = (v <= ((unsigned int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else if (e->type == E_GE) {
	  v = (v >= ((unsigned int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else if (e->type == E_EQ) {
	  v = (v == ((unsigned int)ret->u.e.r->u.v) ? 1 : 0);
	}
	else { /* NE */
	  v = (v != ((unsigned int)ret->u.e.r->u.v) ? 1 : 0);
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
    LVAL_ERROR;
    ret->u.e.l = expr_expand (e->u.e.l, ns, s);
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
    LVAL_ERROR;
    ret->u.e.l = expr_expand (e->u.e.l, ns, s);
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
    LVAL_ERROR;
    ret->u.e.l = expr_expand (e->u.e.l, ns, s);
    if (expr_is_a_const (ret->u.e.l)) {
      if (ret->u.e.l->type == E_INT) {
	unsigned int v = ret->u.e.l->u.v;
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
    ret->u.e.l = expr_expand (e->u.e.l, ns, s);
    if (!expr_is_a_const (ret->u.e.l)) {
      ret->u.e.r = expr_expand (e->u.e.r, ns, s);
    }
    else {
      //FREE (ret->u.e.l);
      if (ret->u.e.l->type == E_TRUE) {
	FREE (ret);
	ret = expr_expand (e->u.e.r->u.e.l, ns, s);
      }
      else if (ret->u.e.l->type == E_FALSE) {
	FREE (ret);
	ret = expr_expand (e->u.e.r->u.e.r, ns, s);
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
    LVAL_ERROR;
    ret->u.e.l = (Expr *) ((ActId *)e->u.e.l)->Expand (ns, s);
    if (!expr_is_a_const (ret->u.e.l)) {
      NEW (ret->u.e.r, Expr);
      ret->u.e.r->type = E_BITFIELD;
      ret->u.e.r->u.e.l = expr_expand (e->u.e.r->u.e.l, ns, s);
      ret->u.e.r->u.e.r = expr_expand (e->u.e.r->u.e.r, ns, s);
    }
    else {
      Expr *lo, *hi;
      lo = expr_expand (e->u.e.r->u.e.l, ns, s);
      hi = expr_expand (e->u.e.r->u.e.r, ns, s);
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
    LVAL_ERROR;
    ret->u.e.l = (Expr *) ((ActId *)e->u.e.l)->Expand (ns, s);
    break;

  case E_VAR:
    /* expand an ID:
       this either returns an expanded ID, or 
       for parameterized types returns the value. */
    xid = ((ActId *)e->u.e.l)->Expand (ns, s);
    te = xid->Eval (ns, s, is_lval);
    if (te->type != E_VAR) {
      delete xid;
    }
    FREE (ret);
    ret = te;
    break;

  case E_INT:
    LVAL_ERROR;
    ret->u.v = e->u.v;

    tmp = TypeFactory::NewExpr (ret);
    FREE (ret);
    ret = tmp;
    
    break;

  case E_REAL:
    LVAL_ERROR;
    ret->u.f = e->u.f;
    break;

  case E_TRUE:
  case E_FALSE:
    LVAL_ERROR;

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
    break;

  default:
    fatal_error ("Unknown expression type (%d)!", e->type);
    break;
  }
  
  return ret;
}


/*------------------------------------------------------------------------
 *  Is this a constant? This works only after it has been expanded out.
 *------------------------------------------------------------------------
 */
int expr_is_a_const (Expr *e)
{
  Assert (e, "What?");
  if (e->type == E_INT || e->type == E_REAL ||
      e->type == E_TRUE || e->type == E_FALSE) {
    return 1;
  }
  
  return 0;
}


/*------------------------------------------------------------------------
 *
 * Array Expressions
 *
 *------------------------------------------------------------------------
 */
AExpr::AExpr (Expr *e)
{
  r = NULL;
  l = (AExpr *)e;
  t = AExpr::EXPR;
}


AExpr::AExpr (AExpr::type typ, AExpr *inl, AExpr *inr)
{
  t = typ;
  l = inl;
  r = inr;
}

AExpr::~AExpr ()
{
  if (t != AExpr::EXPR) {
    if (l) {
      delete l;
    }
    if (r) {
      delete r;
    }
  }
  else {
    /* YYY: hmm... expression memory management */
  }
}


void AExpr::Print (FILE *fp)
{
  char buf[10240];
  sPrint (buf, 10240);
  fprintf (fp, "%s", buf);
}

void AExpr::sPrint (char *buf, int sz)
{
  int k = 0;
  int len;
  
  AExpr *a;
  switch (t) {
  case AExpr::EXPR:
    sprint_expr (buf+k, sz, (Expr *)l);
    PRINT_STEP;
    break;

  case AExpr::CONCAT:
    a = this;
    while (a) {
      a->l->sPrint (buf+k, sz);
      PRINT_STEP;
      a = a->GetRight ();
      if (a) {
	snprintf (buf+k, sz, "#");
	PRINT_STEP;
      }
    }
    break;

  case AExpr::COMMA:
    snprintf (buf+k, sz, "{");
    PRINT_STEP;
    a = this;
    while (a) {
      a->l->sPrint (buf+k, sz);
      PRINT_STEP;
      a = a->GetRight ();
      if (a) {
	snprintf (buf+k, sz, ",");
	PRINT_STEP;
      }
    }
    snprintf (buf+k, sz, "}");
    PRINT_STEP;
    break;

  default:
    fatal_error ("Blah, or unimpl %x", this);
    break;
  }
}



/*------------------------------------------------------------------------
 * Compare two array expressions
 *------------------------------------------------------------------------
 */
int AExpr::isEqual (AExpr *a)
{
  if (!a) return 0;
  if (t != a->t) return 0;

  if ((l && !a->l) || (!l && a->l)) return 0;
  if ((r && !a->r) || (!r && a->r)) return 0;
  
  if (t == AExpr::EXPR) {
    return expr_equal ((Expr *)l, (Expr *)a->l);
  }
  if (l && !l->isEqual (a->l)) return 0;
  if (r && !r->isEqual (a->r)) return 0;

  return 1;
}


/*
  Expand: returns an expanded AExpr, all parameters removed.
*/
AExpr *AExpr::Expand (ActNamespace *ns, Scope *s, int is_lval)
{
  /* this evaluates the array expression: everything must be a
     constant or known parameter value */
  AExpr *newl, *newr;

  newl = NULL;
  newr = NULL;

  if (l) {
    if (t != AExpr::EXPR) {
      newl = l->Expand (ns, s);
    }
    else {
      Expr *xe;
      /* expr_expand: returns either a constant expression or an
	 expanded id or an expanded array */
      xe = expr_expand ((Expr *)l, ns, s, is_lval);
      if (!expr_is_a_const (xe) && xe->type != E_VAR
	  && xe->type != E_ARRAY && xe->type != E_SUBRANGE) {
	act_error_ctxt (stderr);
	fprintf (stderr, "\t array expression: ");
	this->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("In expanding array expression, found a non-identifier/non-const expression");
      }
      newl = (AExpr *) xe;
    }
  }
  if (r) {
    newr = r->Expand (ns, s, is_lval);
  }
  return new AExpr (t, newl, newr);
}


/*------------------------------------------------------------------------
 * Returns a deep copy
 *------------------------------------------------------------------------
 */
AExpr *AExpr::Clone()
{
  AExpr *newl, *newr;

  newl = NULL;
  newr = NULL;
  if (l) {
    if (t != AExpr::EXPR) {
      newl = l->Clone ();
    }
    else {
      newl = l;
    }
  }
  if (r) {
    newr = r->Clone ();
  }
  return new AExpr (t, newl, newr);
}


AExprstep *AExpr::stepper()
{
  return new AExprstep (this);
}

  
