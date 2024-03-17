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
#include <act/lang.h>
#include <act/act.h>
#include <common/int.h>
#include <string.h>
#include <string>

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

  11  #
  10  ~, !
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
static void _print_expr (char *buf, int sz, const Expr *e, int prec, int parent)
{
  int k = 0;
  int len;
  if (!e) return;

  if (sz <= 1) return;
 
#define PREC_BEGIN(myprec)			\
  do {						\
    int uprec = (prec < 0 ? -prec : prec);	\
    if ((myprec) < uprec || ((myprec) == uprec && parent != e->type)) {	\
      snprintf (buf+k, sz, "(");		\
      PRINT_STEP;				\
    }						\
  } while (0)

#define PREC_END(myprec)			\
  do {						\
    int uprec = (prec < 0 ? -prec : prec);	\
    if ((myprec) < uprec || ((myprec) == uprec && parent != e->type)) {	\
      snprintf (buf+k, sz, ")");		\
      PRINT_STEP;				\
    }						\
  } while (0)

#define EMIT_BIN(myprec,sym)					\
  do {								\
    int my_sign = (prec < 0 ? -1 : 1);				\
    PREC_BEGIN(myprec);						\
    _print_expr (buf+k, sz, e->u.e.l, my_sign*(myprec), e->type);	\
    PRINT_STEP;							\
    snprintf (buf+k, sz, "%s", (sym));				\
    PRINT_STEP;							\
    _print_expr (buf+k, sz, e->u.e.r, my_sign*(myprec), e->type);	\
    PRINT_STEP;							\
    PREC_END (myprec);						\
  } while (0)


#define EMIT_BIN_NONASSOC(myprec,sym)					\
  do {									\
    int my_sign = (prec < 0 ? -1 : 1);					\
    PREC_BEGIN(myprec);							\
    _print_expr (buf+k, sz, e->u.e.l, my_sign*(myprec), e->type);	\
    PRINT_STEP;								\
    snprintf (buf+k, sz, "%s", (sym));					\
    PRINT_STEP;								\
    if (e->u.e.r->type == e->type) {					\
      snprintf (buf+k, sz, "(");					\
      PRINT_STEP;							\
    }									\
    _print_expr (buf+k, sz, e->u.e.r, my_sign*(myprec), e->type);	\
    PRINT_STEP;								\
    if (e->u.e.r->type == e->type) {					\
      snprintf (buf+k, sz, ")");					\
      PRINT_STEP;							\
    }									\
    PREC_END (myprec);							\
  } while (0)
  

#define EMIT_UNOP(myprec,sym)					\
  do {								\
    int my_sign = (prec < 0 ? -1 : 1);				\
    PREC_BEGIN(myprec);						\
    snprintf (buf+k, sz, "%s", sym);				\
    PRINT_STEP;							\
    _print_expr (buf+k, sz, e->u.e.l, my_sign*(myprec), e->type);	\
    PRINT_STEP;							\
    PREC_END (myprec);						\
  } while (0)
    
  switch (e->type) {
  case E_PROBE:
    PREC_BEGIN (11);
    snprintf (buf+k, sz, "#");
    PRINT_STEP;
    ((ActId *)(e->u.e.l))->sPrint (buf+k, sz);
    PRINT_STEP;
    PREC_END (11);
    break;
    
  case E_NOT: EMIT_UNOP(10,"~"); break;

  case E_COMPLEMENT: EMIT_UNOP(10,"~"); break;

    
  case E_UMINUS: EMIT_UNOP(10, "-"); break;

  case E_MULT: EMIT_BIN (9, "*"); break;
  case E_DIV:  EMIT_BIN_NONASSOC (9, "/"); break;
  case E_MOD:  EMIT_BIN_NONASSOC (9, "%"); break;

  case E_PLUS:  EMIT_BIN (8, "+"); break;
  case E_MINUS: EMIT_BIN (8, "-"); break;

  case E_LSL: EMIT_BIN_NONASSOC (7, "<<"); break;
  case E_LSR: EMIT_BIN_NONASSOC (7, ">>"); break;
  case E_ASR: EMIT_BIN_NONASSOC (7, ">>>"); break;
  case E_LT:  EMIT_BIN (7, "<"); break;
  case E_GT:  EMIT_BIN (7, ">"); break;
  case E_LE:  EMIT_BIN (7, "<="); break;
  case E_GE:  EMIT_BIN (7, ">="); break;
  case E_EQ:  EMIT_BIN (7, "="); break;
  case E_NE:  EMIT_BIN (7, "!="); break;

  case E_AND: EMIT_BIN (6, "&"); break;
    
  case E_XOR: EMIT_BIN (5, "^"); break;

  case E_OR: EMIT_BIN (4, "|"); break;

  case E_ANDLOOP:
    snprintf (buf+k, sz, "(&");
  case E_ORLOOP:
    snprintf (buf+k, sz, "(|");
    PRINT_STEP;
    snprintf (buf+k, sz, "%s:", (char *)e->u.e.l->u.e.l);
    PRINT_STEP;
    _print_expr (buf+k, sz, e->u.e.r->u.e.l, 1, -1);
    PRINT_STEP;
    if (e->u.e.r->u.e.r->u.e.l) {
      snprintf (buf+k, sz, "..");
      PRINT_STEP;
      _print_expr (buf+k, sz, e->u.e.r->u.e.r->u.e.l, 1, -1);
      PRINT_STEP;
    }
    snprintf (buf+k, sz, ":");
    PRINT_STEP;
    _print_expr (buf+k, sz, e->u.e.r->u.e.r->u.e.r, (prec < 0 ? -1 : 1), -1);
    PRINT_STEP;
    snprintf (buf+k, sz, ")");
    PRINT_STEP;
    break;

  case E_QUERY: /* prec = 3 */
    PREC_BEGIN(3);
    _print_expr (buf+k, sz, e->u.e.l, (prec < 0 ? -3 : 3), e->type);
    PRINT_STEP;
    snprintf (buf+k, sz, " ? ");
    PRINT_STEP;
    Assert (e->u.e.r->type == E_COLON, "Hmm");
    _print_expr (buf+k, sz, e->u.e.r->u.e.l, (prec < 0 ? -3 : 3), e->type);
    PRINT_STEP;
    snprintf (buf+k, sz, " : ");
    PRINT_STEP;
    _print_expr (buf+k, sz, e->u.e.r->u.e.r, (prec < 0 ? -3 : 3), e->type);
    PRINT_STEP;
    PREC_END(3);
    break;

  case E_INT:
    if (prec < 0) {
      if (e->u.ival.v_extra) {
	std::string s = ((BigInt *)e->u.ival.v_extra)->sPrint ();
	snprintf (buf+k, sz, "0x%s", s.c_str());
      }
      else {
	snprintf (buf+k, sz, "%lu", e->u.ival.v);
      }
    }
    else {
      snprintf (buf+k, sz, "%ld", e->u.ival.v);
    }
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
	  if (sz <= 1) return;
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

  case E_SELF:
    snprintf (buf+k, sz, "self");
    PRINT_STEP;
    return;
    break;

  case E_SELF_ACK:
    snprintf (buf+k, sz, "selfack");
    PRINT_STEP;
    return;
    break;

  case E_TYPE:
    {
      InstType *it = (InstType *)e->u.e.l;
      it->sPrint (buf+k, sz);
      PRINT_STEP;
    }
    return;
    break;

  case E_BUILTIN_BOOL:
    snprintf (buf+k, sz, "bool(");
    PRINT_STEP;
    if (prec < 0) {
      sprint_uexpr (buf+k, sz, e->u.e.l);
    }
    else {
      sprint_expr (buf+k, sz, e->u.e.l);
    }
    PRINT_STEP;
    snprintf (buf+k, sz, ")");
    PRINT_STEP;
    return;
    break;

  case E_BUILTIN_INT:
    snprintf (buf+k, sz, "int(");
    PRINT_STEP;
    if (prec < 0) {
      sprint_uexpr (buf+k, sz, e->u.e.l);
    }
    else {
      sprint_expr (buf+k, sz, e->u.e.l);
    }      
    PRINT_STEP;
    if (e->u.e.r) {
      snprintf (buf+k, sz, ",");
      PRINT_STEP;
      if (prec < 0) {
	sprint_uexpr (buf+k, sz, e->u.e.r);
      }
      else {
	sprint_expr (buf+k, sz, e->u.e.r);
      }
      PRINT_STEP;
    }
    snprintf (buf+k, sz, ")");
    PRINT_STEP;
    return;
    break;
    
  case E_FUNCTION:
    {
      UserDef *u = (UserDef *)e->u.fn.s;
      Function *f = dynamic_cast<Function *>(u);
      Expr *tmp;
      Assert (f, "Hmm.");
      if (f->getns() && f->getns() != ActNamespace::Global()) {
	char *s = f->getns()->Name();
	snprintf (buf+k, sz, "%s::", s);
	PRINT_STEP;
	FREE (s);
      }
      if (ActNamespace::Act()) {
	ActNamespace::Act()->msnprintfproc (buf+k, sz, f, 1);
      }
      else {
	snprintf (buf+k, sz, "%s", f->getName());
      }
      PRINT_STEP;

      if (e->u.fn.r && e->u.fn.r->type == E_GT) {
	snprintf (buf+k, sz, "<");
	PRINT_STEP;
	tmp = e->u.fn.r->u.e.l;
	while (tmp) {
	  if (prec < 0) {
	    sprint_uexpr (buf+k, sz, tmp->u.e.l);
	  }
	  else {
	    sprint_expr (buf+k, sz, tmp->u.e.l);
	  }
	  PRINT_STEP;
	  tmp = tmp->u.e.r;
	  if (tmp) {
	    snprintf (buf+k, sz, ",");
	    PRINT_STEP;
	  }
	}
	snprintf (buf+k, sz, ">");
	PRINT_STEP;
      }
      
      snprintf (buf+k, sz, "(");
      PRINT_STEP;

      if (e->u.fn.r && e->u.fn.r->type == E_GT) {
	tmp = e->u.fn.r->u.e.r;
      }
      else {
	tmp = e->u.fn.r;
      }
      while (tmp) {
	if (prec < 0) {
	  sprint_uexpr (buf+k, sz, tmp->u.e.l);
	}
	else {
	  sprint_expr (buf+k, sz, tmp->u.e.l);
	}
	PRINT_STEP;
	tmp = tmp->u.e.r;
	if (tmp) {
	  snprintf (buf+k, sz, ",");
	  PRINT_STEP;
	}
      }
      snprintf (buf+k, sz, ")");
      PRINT_STEP;
    }
    break;
    
  case E_BITFIELD:
    ((ActId *)e->u.e.l)->sPrint (buf+k, sz);
    PRINT_STEP;
    snprintf (buf+k, sz, "{");
    PRINT_STEP;

    if (e->u.e.r->u.e.l) {
      if (prec < 0) {
	sprint_uexpr (buf+k, sz, e->u.e.r->u.e.r);
      }
      else {
	sprint_expr (buf+k, sz, e->u.e.r->u.e.r);
      }
      PRINT_STEP;

      snprintf (buf+k, sz, "..");
      PRINT_STEP;

      if (prec < 0) {
	sprint_uexpr (buf+k, sz, e->u.e.r->u.e.l);
      }
      else {
	sprint_expr (buf+k, sz, e->u.e.r->u.e.l);
      }
      PRINT_STEP;
    }
    else {
      if (prec < 0) {
	sprint_uexpr (buf+k, sz, e->u.e.r->u.e.r);
      }
      else {
	sprint_expr (buf+k, sz, e->u.e.r->u.e.r);
      }
      PRINT_STEP;
    }
    snprintf (buf+k, sz, "}");
    PRINT_STEP;
    break;

  case E_CONCAT:
    snprintf (buf+k, sz, "{");
    PRINT_STEP;
    while (e) {
      if (prec < 0) {
	sprint_uexpr (buf+k, sz, e->u.e.l);
      }
      else {
	sprint_expr (buf+k, sz, e->u.e.l);
      }
      PRINT_STEP;
      if (e->u.e.r) {
	snprintf (buf+k, sz, ",");
	PRINT_STEP;
      }
      e = e->u.e.r;
    }
    snprintf (buf+k, sz, "}");
    PRINT_STEP;
    break;
    
  default:
    fatal_error ("Unhandled case!\n");
    break;
  }
}


/*------------------------------------------------------------------------
 *  s/print_expr() is the top-level function
 *------------------------------------------------------------------------
 */
void sprint_expr (char *buf, int sz, const Expr *e)
{
  if (sz <= 1) return;
  _print_expr (buf, sz, e, 1, -1);
}
  
void print_expr (FILE *fp, const Expr *e)
{
  char *buf;
  int bufsz = 10240;

  MALLOC (buf, char, bufsz);
  buf[0] = '\0';
  buf[bufsz-1] = '\0';
  buf[bufsz-2] = '\0';
  while (1) {
    sprint_expr (buf, bufsz, e);
    if (buf[bufsz-2] == '\0') {
      fprintf (fp, "%s", buf);
      FREE (buf);
      return;
    }
    bufsz *= 2;
    REALLOC (buf, char, bufsz);
    buf[0] = '\0';
    buf[bufsz-1] = '\0';
    buf[bufsz-2] = '\0';
  }
}

void sprint_uexpr (char *buf, int sz, const Expr *e)
{
  if (sz <= 1) return;
  _print_expr (buf, sz, e, -1, -1);
}
  
void print_uexpr (FILE *fp, const Expr *e)
{
  char *buf;
  int bufsz = 10240;

  MALLOC (buf, char, bufsz);
  buf[0] = '\0';
  buf[bufsz-1] = '\0';
  buf[bufsz-2] = '\0';
  while (1) {
    sprint_uexpr (buf, bufsz, e);
    if (buf[bufsz-2] == '\0') {
      fprintf (fp, "%s", buf);
      FREE (buf);
      return;
    }
    bufsz *= 2;
    REALLOC (buf, char, bufsz);
    buf[0] = '\0';
    buf[bufsz-1] = '\0';
    buf[bufsz-2] = '\0';
  }
}


/*
  hash ids!
*/
//#define _id_equal(a,b) ((a) == (b))
//#define _id_equal ((ActId *)(a))->isEqual ((ActId *)(b))
static int _id_equal (const Expr *a, const Expr *b)
{
  ActId *ia, *ib;
  ia = (ActId *)a;
  ib = (ActId *)b;
  return ia->isEqual (ib);
}

static int expr_args_equal (const Expr *a, const Expr *b)
{
  while (a && b) {
    if (!expr_equal (a->u.e.l, b->u.e.l)) {
      return 0;
    }
    a = a->u.e.r;
    b = b->u.e.r;
  }
  if (a || b) {
    return 0;
  }
  return 1;
}

/**
 *  Compare two expressions structurally for equality
 *
 *  \param a First expression to be compared
 *  \param b Second expression to be compared
 *  \return 1 if the two are structurally identical, 0 otherwise
 */
int expr_equal (const Expr *a, const Expr *b)
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


  case E_ANDLOOP:
  case E_ORLOOP:
    if (strcmp ((char *)a->u.e.l->u.e.l,
		(char *)b->u.e.l->u.e.l) != 0) {
      return 0;
    }
    if (!expr_equal (a->u.e.r->u.e.l, b->u.e.r->u.e.l)) {
      return 0;
    }
    if (!expr_equal (a->u.e.r->u.e.r->u.e.l, b->u.e.r->u.e.r->u.e.l)) {
      return 0;
    }
    if (!expr_equal (a->u.e.r->u.e.r->u.e.r, b->u.e.r->u.e.r->u.e.r)) {
      return 0;
    }
    return 1;
    break;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    if (!expr_equal (a->u.e.l, b->u.e.l)) {
      return 0;
    }
    if (!expr_equal (a->u.e.r, b->u.e.r)) {
      return 0;
    }
    return 1;
    break;
    
  case E_FUNCTION:
    if (a->u.fn.s != b->u.fn.s) return 0;
    {
      Expr *at, *bt;
      at = a->u.fn.r;
      bt = b->u.fn.r;

      if (at == NULL && bt == NULL) {
	return 1;
      }
      if (at == NULL || bt == NULL) {
	return 0;
      }

      if (at->type != bt->type) {
	return 0;
      }

      if (at->type == E_LT) {
	return expr_args_equal (at, bt);
      }
      else {
	if (!expr_args_equal (at->u.e.l, bt->u.e.l)) {
	  return 0;
	}
	if (!expr_args_equal (at->u.e.r, bt->u.e.r)) {
	  return 0;
	}
	return 1;
      }
    }
    break;

    /* leaf */
  case E_INT:
    if (a->u.ival.v == b->u.ival.v) {
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

int _act_chp_is_synth_flag = 0;

static void _eval_function (ActNamespace *ns, Scope *s, Expr *fn, Expr **ret,
			    int flags)
{
  Function *x = dynamic_cast<Function *>((UserDef *)fn->u.fn.s);
  Expr *e, *f;
  Assert (x, "What?");

  act_error_push (x->getName(), x->getFile(), x->getLine());
  if (!TypeFactory::isParamType (x->getRetType())) {
    /* ok just expand the arguments */
    Function *xf;
    xf = x->Expand (ns, s, 0, NULL);
    (*ret)->u.fn.s = (char *) ((UserDef *)xf);
    (*ret)->u.fn.r = NULL;
    e = fn->u.fn.r;
    f = NULL;
    while (e) {
      Expr *x = expr_expand (e->u.e.l, ns, s, flags);
      if (f == NULL) {
	NEW (f, Expr);
	(*ret)->u.fn.r = f;
	f->u.e.r = NULL;
      }
      else {
	NEW (f->u.e.r, Expr);
	f = f->u.e.r;
	f->u.e.r = NULL;
      }
      f->u.e.l = x;
      f->type = e->type;
      e = e->u.e.r;
    }
  }
  else {
    Expr **args;
    int nargs;
    Expr *e;

    nargs = 0;
    e = fn->u.e.r;
    while (e) {
      nargs++;
      e = e->u.e.r;
    }
    if (nargs > 0) {
      MALLOC (args, Expr *, nargs);
      e = fn->u.e.r;
      for (int i=0; i < nargs; i++) {
	args[i] = expr_expand (e->u.e.l, ns, s, flags);
	e = e->u.e.r;
      }
    }
    for (int i=0; i < nargs; i++) {
      if (!expr_is_a_const (args[i])) {
	if (args[i]->type == E_ARRAY) {
	  act_error_ctxt (stderr);
	  fatal_error ("Arrays are not supported as function arguments.");
	}
	else {
	  act_error_ctxt (stderr);
	  Assert (expr_is_a_const (args[i]), "Argument is not a constant?");
	}
      }
    }
    e = x->eval (ns, nargs, args);
    FREE (*ret);
    *ret = e;
    if (nargs > 0) {
      FREE (args);
    }
  }
  act_error_pop ();
  return;
}
    

/*------------------------------------------------------------------------
 * Expand expression, replacing all paramters. If it is an lval, then
 * it must be a pure identifier at the end of of the day. Default is
 * not an lval
 *
 *   pc = 1 means partial constant propagation is used
 *------------------------------------------------------------------------
 */
static int _int_width (unsigned long v)
{
  int w = 0;
  while (v) {
    v = v >> 1;
    w++;
  }
  if (w == 0) { w = 1; }
  return w;
}

#if 0
static BigInt *_int_const (unsigned long v)
{
  BigInt *btmp;

  /* compute bitwidth */
  int w = _int_width (v);
  
  btmp = new BigInt (w, 0, 1);
  btmp->setVal (0, v);
  btmp->setWidth (w);

  return btmp;
}
#endif

static Expr *_expr_expand (int *width, Expr *e,
			   ActNamespace *ns, Scope *s, unsigned int flags)
{
  Expr *ret, *te;
  ActId *xid;
  Expr *tmp;
  int pc;
  int lw, rw;
  
  if (!e) return NULL;

  NEW (ret, Expr);
  ret->type = e->type;
  ret->u.e.l = NULL;
  ret->u.e.r = NULL;

  pc = (flags & ACT_EXPR_EXFLAG_PARTIAL) ? 1 : 0;

#define LVAL_ERROR							\
    do {								\
      if (flags & ACT_EXPR_EXFLAG_ISLVAL) {				\
	act_error_ctxt (stderr);					\
	fprintf (stderr, "\texpanding expr: ");				\
	print_expr (stderr, e);						\
	fprintf (stderr, "\n");						\
	fatal_error ("Invalid assignable or connectable value!");	\
      }									\
    } while (0)

#include "expr_width.h"  

  switch (e->type) {

  case E_ANDLOOP:
  case E_ORLOOP:
    LVAL_ERROR;
    *width = 1;
    {
      int ilo, ihi;
      ValueIdx *vx;
      Expr *cur = NULL;

      act_syn_loop_setup (ns, s, (char *)e->u.e.l->u.e.l,
			  e->u.e.r->u.e.l, e->u.e.r->u.e.r->u.e.l,
			  &vx, &ilo, &ihi);

      int is_const = 1;
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
	Expr *tmp = _expr_expand (&lw, e->u.e.r->u.e.r->u.e.r, ns, s, flags);
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
    LVAL_ERROR;
    ret->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s, flags);
    ret->u.e.r = _expr_expand (&rw, e->u.e.r, ns, s, flags);
    WIDTH_UPDATE(WIDTH_MAX);
    if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
      if (ret->u.e.l->type == E_INT && ret->u.e.r->type == E_INT) {
	if (ret->u.e.l->u.ival.v_extra && ret->u.e.r->u.ival.v_extra) {
	  BigInt *l = (BigInt *)ret->u.e.l->u.ival.v_extra;
	  BigInt *r = (BigInt *)ret->u.e.r->u.ival.v_extra;

	  if (e->type == E_AND) {
	    *l &= (*r);
	  }
	  else if (e->type == E_OR) {
	    *l |= (*r);
	  }
	  else {
	    *l ^= (*r);
	  }
	  delete r;
	  FREE (ret->u.e.r);
	  FREE (ret->u.e.l);
	  ret->type = E_INT;
	  ret->u.ival.v_extra = l;
	  ret->u.ival.v = l->getVal (0);
	}
	else {
	  unsigned long v;

	  v = ret->u.e.l->u.ival.v;
	  if (e->type == E_AND) {
	    v = v & ((unsigned long)ret->u.e.r->u.ival.v);
	  }
	  else if (e->type == E_OR) {
	    v = v | ((unsigned long)ret->u.e.r->u.ival.v);
	  }
	  else {
	    v = v ^ ((unsigned long)ret->u.e.r->u.ival.v);
	  }
	  //FREE (ret->u.e.l);
	  //FREE (ret->u.e.r);
	  ret->type = E_INT;
	  ret->u.ival.v = v;
	  ret->u.ival.v_extra = NULL;

	  tmp = TypeFactory::NewExpr (ret);
	  FREE (ret);
	  ret = tmp;
	}
      }
      else if (ret->u.e.l->type == E_TRUE || ret->u.e.l->type == E_FALSE) {
	unsigned long v;

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
    else if ((e->type == E_AND || e->type == E_OR) &&
	     (expr_is_a_const (ret->u.e.l) || expr_is_a_const (ret->u.e.r))) {
      Expr *ce, *re;
      if (expr_is_a_const (ret->u.e.l)) {
	ce = ret->u.e.l;
	re = ret->u.e.r;}
      else {
	ce = ret->u.e.r;
	re = ret->u.e.l;
      }

      if (e->type == E_AND) {
	if (ce->type == E_INT && ce->u.ival.v == 0 &&
	    (!ce->u.ival.v_extra || ((BigInt *)ce->u.ival.v_extra)->isZero())) {
	  if (pc) {
	    /* return 0 */
	    FREE (ret);
	    ret = ce;
	    if (ce->u.ival.v_extra) {
	      ((BigInt *)ce->u.ival.v_extra)->setWidth (*width);
	    }
	    /* XXX: free re but with a new free function */
	  }
	}
	else if (ce->type == E_TRUE) {
	  FREE (ret);
	  ret = re;
	}
	else if (ret->u.e.l->type == E_FALSE) {
	  if (pc) {
	    /* return false */
	    FREE (ret);
	    ret = ce;
	    /* XXX: free re */
	  }
	}
      }
      else if (e->type == E_OR) {
	if (ce->type == E_INT && ce->u.ival.v == 0 &&
	    (!ce->u.ival.v_extra || ((BigInt *)ce->u.ival.v_extra)->isZero())) {
	  FREE (ret);
	  ret = re;
	}
	else if (ce->type == E_TRUE) {
	  if (pc) {
	    /* return true */
	    FREE (ret);
	    ret = ce;
	    /* XXX: free re */
	  }
	}
	else if (ret->u.e.l->type == E_FALSE) {
	  FREE (ret);
	  ret = re;
	}
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
    ret->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s, flags);
    ret->u.e.r = _expr_expand (&rw, e->u.e.r, ns, s, flags);

    if (e->type == E_PLUS || e->type == E_MINUS) {
      WIDTH_UPDATE (WIDTH_MAX1);
    }
    else if (e->type == E_MULT) {
      WIDTH_UPDATE (WIDTH_SUM);
    }
    else if (e->type == E_DIV || e->type == E_LSR || e->type == E_ASR) {
      WIDTH_UPDATE (WIDTH_LEFT);
    }
    else if (e->type == E_MOD) {
      WIDTH_UPDATE (WIDTH_RIGHT);
    }
    else if (e->type == E_LSL) {
      WIDTH_UPDATE (WIDTH_LSHIFT);
    }
    else {
      Assert (0, "What?");
    }
    
    if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
      if (ret->u.e.l->type == E_INT && ret->u.e.r->type == E_INT) {
	if (ret->u.e.l->u.ival.v_extra && ret->u.e.r->u.ival.v_extra) {
	  BigInt *l = (BigInt *)ret->u.e.l->u.ival.v_extra;
	  BigInt *r = (BigInt *)ret->u.e.r->u.ival.v_extra;

	  if (e->type == E_PLUS) {
	    *l += *r;
	  }
	  else if (e->type == E_MINUS) {
	    *l -= *r;
	  }
	  else if (e->type == E_MULT) {
	    *l = (*l) * (*r);
	  }
	  else if (e->type == E_DIV) {
	    *l = (*l) / (*r);
	  }
	  else if (e->type == E_MOD) {
	    *l = (*l) % (*r);
	  }
	  else if (e->type == E_LSL) {
	    *l <<= (*r);
	  }
	  else if (e->type == E_LSR) {
	    *l >>= (*r);
	  }
	  else {  /* ASR */
	    (*l).toSigned ();
	    *l >>= (*r);
	    (*l).toUnsigned ();
	  }
	  delete r;
	  FREE (ret->u.e.r);
	  FREE (ret->u.e.l);
	  ret->type = E_INT;
	  ret->u.ival.v_extra = l;
	  ret->u.ival.v = l->getVal (0);
	  l->setWidth (*width);
	}
	else {
	  signed long v;

	  v = ret->u.e.l->u.ival.v;
	  if (e->type == E_PLUS) {
	    v = v + ((signed long)ret->u.e.r->u.ival.v);
	  }
	  else if (e->type == E_MINUS) {
	    v = v - ((signed long)ret->u.e.r->u.ival.v);
	  }
	  else if (e->type == E_MULT) {
	    v = v * ((signed long)ret->u.e.r->u.ival.v);
	  }
	  else if (e->type == E_DIV) {
	    v = v / ((signed long)ret->u.e.r->u.ival.v);
	  }
	  else if (e->type == E_MOD) {
	    v = v % ((signed long)ret->u.e.r->u.ival.v);
	  }
	  else if (e->type == E_LSL) {
	    v = v << ((unsigned long)ret->u.e.r->u.ival.v);
	  }
	  else if (e->type == E_LSR) {
	    v = ((unsigned long)v) >> ((unsigned long)ret->u.e.r->u.ival.v);
	  }
	  else { /* ASR */
	    v = (signed long)v >> ((unsigned long)ret->u.e.r->u.ival.v);
	  }
	  //FREE (ret->u.e.l);
	  //FREE (ret->u.e.r);

	  ret->type = E_INT;
	  ret->u.ival.v = v;
	  ret->u.ival.v_extra = NULL;

	  tmp = TypeFactory::NewExpr (ret);
	  FREE (ret);
	  ret = tmp;
	}
      }
      else if ((ret->u.e.l->type == E_INT||ret->u.e.l->type == E_REAL)
	       && (ret->u.e.r->type == E_INT || ret->u.e.r->type == E_REAL)
	       && (e->type == E_PLUS || e->type == E_MINUS || e->type == E_MULT
		   || e->type == E_DIV)) {
	double v;

#define VAL(e) (((e)->type == E_INT) ? (unsigned long)(e)->u.ival.v : (e)->u.f)

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
    else if ((e->type == E_PLUS || e->type == E_MINUS || e->type == E_MULT
	      || e->type == E_DIV || e->type == E_MOD) && pc &&
	     (expr_is_a_const (ret->u.e.l) || expr_is_a_const (ret->u.e.r))) {
      Expr *ce, *re;
      if (expr_is_a_const (ret->u.e.l)) {
	ce = ret->u.e.l;
	re = ret->u.e.r;
      }
      else {
	ce = ret->u.e.r;
	re = ret->u.e.l;
      }
      if (e->type == E_PLUS && VAL(ce) == 0) {
	FREE (ret);
	ret = re;
      }
      else if (e->type == E_MINUS && VAL(ce) == 0) {
	if (ce == ret->u.e.r) {
	  FREE (ret);
	  ret = re;
	}
	else {
	  ret->type = E_UMINUS;
	  ret->u.e.l = re;
	  ret->u.e.r = NULL;
	}
      }
      else if (e->type == E_MULT && VAL(ce) == 0) {
	if (pc) {
	  FREE (ret);
	  ret = ce;
	  if (ce->type == E_INT && ce->u.ival.v_extra) {
	    ((BigInt *)ce->u.ival.v_extra)->setWidth (*width);
	  }
	  /* XXX: free re */
	}
      }
      else if (e->type == E_MULT && VAL(ce) == 1) {
	FREE (ret);
	ret = re;
      }
      else if (e->type == E_DIV && VAL(ce) == 0 && (ce == ret->u.e.l)) {
	if (pc) {
	  FREE (ret);
	  ret = ce;
	  if (ce->type == E_INT && ce->u.ival.v_extra) {
	    ((BigInt *)ce->u.ival.v_extra)->setWidth (*width);
	  }
	  /* XXX: free re */
	}
      }
      else if (e->type == E_DIV && VAL(ce) == 1 && (ce == ret->u.e.r)) {
	FREE (ret);
	ret = re;
      }
      else if (e->type == E_MOD && VAL(ce) == 0 && (ce == ret->u.e.l)) {
	if (pc) {
	  FREE (ret);
	  ret = ce;
	  if (ce->type == E_INT && ce->u.ival.v_extra) {
	    ((BigInt *)ce->u.ival.v_extra)->setWidth (*width);
	  }
	  /* XXX: free re */
	}
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
    ret->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s, flags);
    ret->u.e.r = _expr_expand (&rw, e->u.e.r, ns, s, flags);
    *width = 1;
    if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
      if (ret->u.e.l->type == E_INT && ret->u.e.r->type == E_INT) {
	if (ret->u.e.l->u.ival.v_extra && ret->u.e.r->u.ival.v_extra) {
	  BigInt *l = (BigInt *)ret->u.e.l->u.ival.v_extra;
	  BigInt *r = (BigInt *)ret->u.e.r->u.ival.v_extra;
	  int res;
	  if (e->type == E_LT) {
	    res = ((*l) < (*r)) ? 1 : 0;
	  }
	  else if (e->type == E_GT) {
	    res = ((*l) > (*r)) ? 1 : 0;
	  }
	  else if (e->type == E_LE) {
	    res = ((*l) <= (*r)) ? 1 : 0;
	  }
	  else if (e->type == E_GE) {
	    res = ((*l) >= (*r)) ? 1 : 0;
	  }
	  else if (e->type == E_EQ) {
	    res = ((*l) == (*r)) ? 1 : 0;
	  }
	  else {
	    res = ((*l) != (*r)) ? 1 : 0;
	  }
	  delete l;
	  delete r;
	  FREE (ret->u.e.l);
	  FREE (ret->u.e.r);
	  if (res) {
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
	  signed long v;

	  v = ret->u.e.l->u.ival.v;
	  if (e->type == E_LT) {
	    v = (v < ((signed long)ret->u.e.r->u.ival.v) ? 1 : 0);
	  }
	  else if (e->type == E_GT) {
	    v = (v > ((signed long)ret->u.e.r->u.ival.v) ? 1 : 0);
	  }
	  else if (e->type == E_LE) {
	    v = (v <= ((signed long)ret->u.e.r->u.ival.v) ? 1 : 0);
	  }
	  else if (e->type == E_GE) {
	    v = (v >= ((signed long)ret->u.e.r->u.ival.v) ? 1 : 0);
	  }
	  else if (e->type == E_EQ) {
	    v = (v == ((signed long)ret->u.e.r->u.ival.v) ? 1 : 0);
	  }
	  else { /* NE */
	    v = (v != ((signed long)ret->u.e.r->u.ival.v) ? 1 : 0);
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
	fprintf (stderr,"\n\tlhs: ");
	if (ret->u.e.l->type == E_INT) {
	  fprintf (stderr, "pint");
	}
	else if (ret->u.e.l->type == E_REAL) {
	  fprintf (stderr, "preal");
	}
	else {
	  fprintf (stderr, "other");
	}
	fprintf (stderr,";  rhs: ");
	if (ret->u.e.r->type == E_INT) {
	  fprintf (stderr, "pint");
	}
	else if (ret->u.e.r->type == E_REAL) {
	  fprintf (stderr, "preal");
	}
	else {
	  fprintf (stderr, "other");
	}
	fprintf (stderr, "\n");
	fatal_error ("Incompatible types for comparison operator");
      }
    }
    break;
    
  case E_NOT:
    LVAL_ERROR;
    ret->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s, flags);
    *width = lw;
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
    ret->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s, flags);
    *width = lw;
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
	if (ret->u.e.l->u.ival.v_extra) {
	  BigInt *l = (BigInt *)ret->u.e.l->u.ival.v_extra;
	  *l = ~(*l);
	  FREE (ret->u.e.l);
	  ret->type = E_INT;
	  ret->u.ival.v_extra = l;
	  ret->u.ival.v = l->getVal (0);
	}
	else {
	  unsigned long v = ret->u.e.l->u.ival.v;
	  //FREE (ret->u.e.l);
	  ret->type = E_INT;
	  ret->u.ival.v = ~v;
	  ret->u.ival.v_extra = NULL;
	  tmp = TypeFactory::NewExpr (ret);
	  FREE (ret);
	  ret = tmp;
	}
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
    ret->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s, flags);
    *width = lw;
    if (expr_is_a_const (ret->u.e.l)) {
      if (ret->u.e.l->type == E_INT) {
	if (ret->u.e.l->u.ival.v_extra) {
	  BigInt *l = (BigInt *)ret->u.e.l->u.ival.v_extra;
	  l->toSigned ();
	  *l = -(*l);
	  l->toUnsigned ();
	  FREE (ret->u.e.l);
	  ret->type = E_INT;
	  ret->u.ival.v_extra = l;
	  ret->u.ival.v = l->getVal (0);
	}
	else {
	  signed long v = ret->u.e.l->u.ival.v;
	  //FREE (ret->u.e.l);
	  ret->type = E_INT;
	  ret->u.ival.v = -v;
	  ret->u.ival.v_extra = NULL;
	  tmp = TypeFactory::NewExpr (ret);
	  FREE (ret);
	  ret = tmp;
	}
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
    LVAL_ERROR;
    ret->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s, flags);
    ret->u.e.r = _expr_expand (&rw, e->u.e.r, ns, s, flags);
    WIDTH_UPDATE (WIDTH_MAX);
    if (!expr_is_a_const (ret->u.e.l)) {
      Expr *tmp = ret->u.e.r;

      if (expr_is_a_const (tmp->u.e.l) && expr_is_a_const (tmp->u.e.r)) {
	Expr *ce = tmp->u.e.l;
	if ((tmp->u.e.l->type == E_TRUE && tmp->u.e.r->type == E_TRUE) ||
	    (tmp->u.e.l->type == E_FALSE && tmp->u.e.r->type == E_FALSE) ||
	    (VAL(tmp->u.e.l) == VAL(tmp->u.e.r))) {
	  /* XXX need to free ret->u.e.l */
	  FREE (ret);
	  FREE (tmp);
	  ret = ce;
	  if (ce->type == E_INT && ce->u.ival.v_extra) {
	    ((BigInt *)ce->u.ival.v_extra)->setWidth (*width);
	  }
	}
      }
    }
    else {
      Expr *tmp = ret->u.e.r;
      if (expr_is_a_const (tmp->u.e.l) && expr_is_a_const (tmp->u.e.r)) {
	//FREE (ret->u.e.l);
	if (ret->u.e.l->type == E_TRUE) {
	  FREE (ret);
	  ret = tmp->u.e.l;
	  expr_ex_free (tmp->u.e.r);
	  FREE (tmp);
	}
	else if (ret->u.e.l->type == E_FALSE) {
	  FREE (ret);
	  ret = tmp->u.e.r;
	  expr_ex_free (tmp->u.e.l);
	  FREE (tmp);
	}
	else {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "\texpanding expr: ");
	  print_expr (stderr, e);
	  fprintf (stderr,"\n");
	  fatal_error ("Query operator expression has non-Boolean value");
	}
	if (ret->type == E_INT && ret->u.ival.v_extra) {
	  ((BigInt *)ret->u.ival.v_extra)->setWidth (*width);
	}
      }
    }
    break;

  case E_COLON:
    LVAL_ERROR;
    /* you only get here for non-const things */
    ret->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s, flags);
    ret->u.e.r = _expr_expand (&rw, e->u.e.r, ns, s, flags);
    WIDTH_UPDATE (WIDTH_MAX);
    break;

  case E_BITFIELD:
    LVAL_ERROR;
    if (flags & ACT_EXPR_EXFLAG_DUPONLY) {
      ret->u.e.l = (Expr *) ((ActId *)e->u.e.l)->Clone();
      NEW (ret->u.e.r, Expr);
      ret->u.e.r->type = E_BITFIELD;
      ret->u.e.r->u.e.l = e->u.e.r->u.e.l;
      ret->u.e.r->u.e.r = e->u.e.r->u.e.r;
      *width = -1;
    }
    else {
      if (flags & ACT_EXPR_EXFLAG_CHPEX) {
	ActId *xid = ((ActId *)e->u.e.l)->ExpandCHP (ns, s);
	ret->u.e.l = (Expr *) xid;
	te = xid->EvalCHP (ns, s, 0);
	if (!expr_is_a_const (te)) {
	  if (te->type != E_VAR) {
	    expr_ex_free (te);
	  }
	  else {
	    FREE (te);
	  }
	}
	else {
	  delete xid;
	  ret->u.e.l = te;
	}
      }
      else {
	ret->u.e.l = (Expr *) ((ActId *)e->u.e.l)->Expand (ns, s);
      }
      if (!expr_is_a_const (ret->u.e.l)) {
	NEW (ret->u.e.r, Expr);
	ret->u.e.r->type = E_BITFIELD;
	ret->u.e.r->u.e.l = _expr_expand (&lw, e->u.e.r->u.e.l, ns, s, flags);
	ret->u.e.r->u.e.r = _expr_expand (&rw, e->u.e.r->u.e.r, ns, s, flags);
	if ((ret->u.e.r->u.e.l && !expr_is_a_const (ret->u.e.r->u.e.l)) || !expr_is_a_const (ret->u.e.r->u.e.r)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "\texpanding expr: ");
	  print_expr (stderr, e);
	  fprintf (stderr,"\n");
	  fatal_error ("Bitfield operator has non-const components");
	}
	if (ret->u.e.r->u.e.l && (ret->u.e.r->u.e.l->type != E_INT)) {
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
	if (ret->u.e.r->u.e.l) {
	  *width = ret->u.e.r->u.e.r->u.ival.v - ret->u.e.r->u.e.l->u.ival.v + 1;
	}
	else {
	  *width = 1;
	}
	InstType *it = s->FullLookup ((ActId *)ret->u.e.l, NULL);
	if (ret->u.e.r->u.e.r->u.ival.v >= TypeFactory::bitWidth (it)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "\texpanding expr: ");
	  print_expr (stderr, e);
	  fprintf (stderr, "\n");
	  warning ("Bit-width (%d) is less than the width specifier {%d..%d}",
		   TypeFactory::bitWidth (it), ret->u.e.r->u.e.r->u.ival.v,
		   ret->u.e.r->u.e.l ? ret->u.e.r->u.e.l->u.ival.v :
		   ret->u.e.r->u.e.r->u.ival.v);
	}
      }
      else {
	unsigned long v;
	Expr *lo, *hi;

	if (ret->u.e.l->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "\texpanding expr: ");
	  print_expr (stderr, e);
	  fprintf (stderr,"\n");
	  fatal_error ("Variable in bitfield operator is a non-integer");
	}
	v = ret->u.e.l->u.ival.v;
	if (e->u.e.r->u.e.l) {
	  lo = _expr_expand (&lw, e->u.e.r->u.e.l, ns, s, flags);
	}
	else {
	  lo = NULL;
	  lw = -1;
	}
	hi = _expr_expand (&rw, e->u.e.r->u.e.r, ns, s, flags);
	Assert (hi, "What?");
      
	unsigned long lov, hiv;

	if ((lo && lo->type != E_INT) || hi->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "\texpanding expr: ");
	  print_expr (stderr, e);
	  fprintf (stderr,"\n");
	  fatal_error ("Bitfield parameter in operator is a non-integer");
	}

	hiv = hi->u.ival.v;
	if (lo) {
	  lov = lo->u.ival.v;
	}
	else {
	  lov = hiv;
	}
      
	if (lov > hiv) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "\texpanding expr: ");
	  print_expr (stderr, e);
	  fprintf (stderr, "\n");
	  fatal_error ("Bitfield {%d..%d} : empty!", hiv, lov);
	}

	v = (v >> lov) & ~(~0UL << (hiv - lov + 1));

	BigInt *ltmp;
	if (ret->u.e.l->u.ival.v_extra) {
	  ltmp = (BigInt *) ret->u.e.l->u.ival.v_extra;
	}
	else {
	  ltmp = NULL;
	}

	ret->type = E_INT;
	ret->u.ival.v = v;

	if (ltmp) {
	  *ltmp >>= lov;
	  FREE (ret->u.e.l);
	  ltmp->setWidth (hiv-lov+1);
	  ret->u.ival.v_extra = ltmp;
	  ret->u.ival.v = ltmp->getVal (0);
	}
	else {
	  ret->u.ival.v_extra = NULL;
	  tmp = TypeFactory::NewExpr (ret);
	  FREE (ret);
	  ret = tmp;
	}
	*width = (hiv - lov + 1);
      }
    }
    break;

  case E_PROBE:
    LVAL_ERROR;
    if (flags & ACT_EXPR_EXFLAG_DUPONLY) {
      ret->u.e.l = (Expr *) ((ActId *)e->u.e.l)->Clone ();
    }
    else {
      ret->u.e.l = (Expr *) ((ActId *)e->u.e.l)->Expand (ns, s);
    }
    *width = 1;
    break;

  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    LVAL_ERROR;
    ret->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s, flags);
    if (!e->u.e.r) {
      ret->u.e.r = NULL;
      if (expr_is_a_const (ret->u.e.l)) {
	if (ret->type == E_BUILTIN_BOOL) {
	  if (ret->u.e.l->u.ival.v) {
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
	    ret->u.ival.v = 1;
            ret->u.ival.v_extra = NULL;
	    tmp = TypeFactory::NewExpr (ret);
	    FREE (ret);
	    ret = tmp;
	  }
	  else if (ret->u.e.l->type == E_REAL) {
	    Expr *texpr = ret->u.e.l;
	    ret->type = E_INT;
	    ret->u.ival.v = (long)texpr->u.f;
	    ret->u.ival.v_extra = NULL;
	    tmp = TypeFactory::NewExpr (ret);
	    FREE (ret);
	    expr_free (texpr);
	    ret = tmp;
	  }
	  else {
	    ret->type = E_INT;
	    ret->u.ival.v = 0;
            ret->u.ival.v_extra = NULL;
	    tmp = TypeFactory::NewExpr (ret);
	    FREE (ret);
	    ret = tmp;
	  }
	}
      }
      *width = 1;
    }
    else {
      ret->u.e.r = _expr_expand (&lw, e->u.e.r, ns, s, flags);

      if (expr_is_a_const (ret->u.e.l) && expr_is_a_const (ret->u.e.r)) {
	BigInt *l;
	int _width = ret->u.e.r->u.ival.v;

	if (ret->u.e.l->u.ival.v_extra) {
	  l = (BigInt *)ret->u.e.l->u.ival.v_extra;
	  FREE (ret->u.e.l);
	  ret->type = E_INT;
	  l->setWidth (_width);
	  ret->u.ival.v_extra = l;
	  ret->u.ival.v = l->getVal (0);
	}
	else {
	  unsigned long x = ret->u.e.l->u.ival.v;

	  if (_width < 64) {
	    x = x & ((1UL << _width)-1);
	  }

	  l = new BigInt;
	  l->setWidth (_width);
	  l->setVal (0, x);
	  
	  ret->type = E_INT;
	  ret->u.ival.v = x;
	  ret->u.ival.v_extra = l;
	}
	*width = _width;
      }
      else if (!expr_is_a_const (ret->u.e.r)) {
	act_error_ctxt (stderr);
	fatal_error ("int() operator requires a constant expression for the second argument");
      }
      else {
	*width = ret->u.e.r->u.ival.v;
      }
    }
    break;
    
  case E_FUNCTION:
    LVAL_ERROR;
    if (!(flags & ACT_EXPR_EXFLAG_CHPEX)) {
      if (flags & ACT_EXPR_EXFLAG_DUPONLY) {
	Expr *tmp, *prev;
	Expr *w = e->u.fn.r;
	ret->u.fn.s = e->u.fn.s;
	ret->u.fn.r = NULL;
	prev = NULL;
	while (w) {
	  int dummy;
	  NEW (tmp, Expr);
	  tmp->u.e.l = _expr_expand (&dummy, w->u.e.l, ns, s, flags);
	  tmp->u.e.r = NULL;
	  if (!prev) {
	    ret->u.fn.r = tmp;
	  }
	  else {
	    prev->u.e.r = tmp;
	  }
	  prev = tmp;
	  w = w->u.e.r;
	}
      }
      else {
	_eval_function (ns, s, e, &ret, flags);
      }
      *width = -1;
    }
    else {
      Expr *tmp, *etmp;
      Function *f = dynamic_cast<Function *>((UserDef *)e->u.fn.s);
      if (TypeFactory::isParamType (f->getRetType())) {
	// parameterized function: we need to evaluate this
	// statically, even in CHP mode
	_eval_function (ns, s, e, &ret, flags & ~ACT_EXPR_EXFLAG_CHPEX);
	*width = -1;
      }
      else {
	*width = TypeFactory::bitWidth (f->getRetType());
      
	if (e->u.fn.r && e->u.fn.r->type == E_GT) {
	  /* template parameters */
	  int count = 0;
	  Expr *w;
	  w = e->u.fn.r->u.e.l;
	  while (w) {
	    count++;
	    w = w->u.e.r;
	  }
	  inst_param *inst;
	  if (count == 0) {
	    f = f->Expand (ns, s, 0, NULL);
	  }
	  else {
	    MALLOC (inst, inst_param, count);
	    w = e->u.fn.r->u.e.l;
	    for (int i=0; i < count; i++) {
	      AExpr *tae;
	      inst[i].isatype = 0;
	      tae = new AExpr (expr_dup (w->u.e.l));
	      inst[i].u.tp = tae->Expand (ns, s, 0);
	      delete tae;
	      w = w->u.e.r;
	    }
	    f = f->Expand (ns, s, count, inst);
	    for (int i=0; i < count; i++) {
	      delete inst[i].u.tp;
	    }
	    FREE (inst);
	  }
	}
	else {
	  if (flags & ACT_EXPR_EXFLAG_DUPONLY) {
	    /* nothing here */
	  }
	  else {
	    f = f->Expand (ns, s, 0, NULL);
	  }
	}

	if (f->isExternal()) {
	  _act_chp_is_synth_flag = 0;
	}
      
	ret->u.fn.s = (char *) f;
	if (!e->u.fn.r) {
	  ret->u.fn.r = NULL;
	}
	else {
	  NEW (tmp, Expr);
	  tmp->type = E_LT;
	  ret->u.fn.r = tmp;
	  tmp->u.e.r = NULL;
	  if (e->u.fn.r->type == E_GT) {
	    etmp = e->u.fn.r->u.e.r;
	  }
	  else {
	    etmp = e->u.fn.r;
	  }
	  do {
	    tmp->u.e.l = _expr_expand (&lw, etmp->u.e.l, ns, s, flags);
	    if (etmp->u.e.r) {
	      NEW (tmp->u.e.r, Expr);
	      tmp->type = E_LT;
	      tmp = tmp->u.e.r;
	      tmp->u.e.r = NULL;
	    }
	    etmp = etmp->u.e.r;
	  } while (etmp);
	}
      }
    }
    break;

  case E_VAR:
    /* expand an ID:
       this either returns an expanded ID, or 
       for parameterized types returns the value. */
    if (flags & ACT_EXPR_EXFLAG_DUPONLY) {
      ret->u.e.l = (Expr *)((ActId *)e->u.e.l)->Clone ();
      *width = -1;
    }
    else {
      if (flags & ACT_EXPR_EXFLAG_CHPEX) {
	/* chp mode expansion */
	xid = ((ActId *)e->u.e.l)->ExpandCHP (ns, s);
	te = xid->EvalCHP (ns, s, 0);
	if (te->type == E_VAR) {
	  act_chp_macro_check (s, (ActId *)te->u.e.l);
	  InstType *it;
	  act_type_var (s, (ActId *)te->u.e.l, &it);
	  *width = TypeFactory::bitWidth (it);
	}
	else if (te->type == E_INT) {
	  if (te->u.ival.v_extra == NULL) {
	    /* XXX: add stuff here */
	  }
	  *width = _int_width (te->u.ival.v);
	}
	else {
	  *width = -1;
	}
      }
      else {
	/* non-chp expansion */
	xid = ((ActId *)e->u.e.l)->Expand (ns, s);
	te = xid->Eval (ns, s, (flags & ACT_EXPR_EXFLAG_ISLVAL) ? 1 : 0);
	if (te->type == E_INT) {
	  *width = _int_width (te->u.ival.v);
	}
	else {
	  *width = -1;
	}
      }
      if (te->type != E_VAR) {
	delete xid;
      }
      FREE (ret);
      ret = te;
    }
    break;

  case E_INT:
    LVAL_ERROR;
    ret->u.ival.v = e->u.ival.v;
    ret->u.ival.v_extra = NULL;
    *width = _int_width (ret->u.ival.v);

    if (flags & ACT_EXPR_EXFLAG_CHPEX) {
      if (e->u.ival.v_extra) {
	BigInt *btmp = new BigInt();
	*btmp = *((BigInt *)e->u.ival.v_extra);
	ret->u.ival.v_extra = btmp;
	*width = btmp->getWidth ();
      }
      else {
	BigInt *btmp = new BigInt (*width, 0, 1);
	btmp->setVal (0, ret->u.ival.v);
	ret->u.ival.v_extra = btmp;
      }
    }
    else {
      if (e->u.ival.v_extra) {
	BigInt *btmp = new BigInt();
	*btmp = *((BigInt *)e->u.ival.v_extra);
	ret->u.ival.v_extra = btmp;
	*width = btmp->getWidth();
      }
      else {
	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
    }
    break;

  case E_REAL:
    LVAL_ERROR;
    ret->u.f = e->u.f;
    *width = 64;
    break;

  case E_TRUE:
  case E_FALSE:
    LVAL_ERROR;

    tmp = TypeFactory::NewExpr (ret);
    FREE (ret);
    ret = tmp;
    *width = 1;
    break;

  case E_ARRAY:
  case E_SUBRANGE:
    ret->u = e->u;
    break;

  case E_SELF:
    xid = new ActId ("self");
    te = xid->Eval (ns, s, (flags & ACT_EXPR_EXFLAG_ISLVAL) ? 1 : 0);
    if (te->type != E_VAR) {
      delete xid;
    }
    FREE (ret);
    ret = te;
    *width = 0;
    break;

  case E_SELF_ACK:
    xid = new ActId ("selfack");
    te = xid->Eval (ns, s, (flags & ACT_EXPR_EXFLAG_ISLVAL) ? 1 : 0);
    if (te->type != E_VAR) {
      delete xid;
    }
    FREE (ret);
    ret = te;
    *width = 0;
    break;

  case E_CONCAT:
    {
      Expr *f = ret;
      ret->u.e.l = NULL;
      ret->u.e.r = NULL;
      *width = 0;
      while (e) {
	f->u.e.l = _expr_expand (&lw, e->u.e.l, ns, s,
				(flags & ~ACT_EXPR_EXFLAG_ISLVAL));
	*width += lw;
	if (e->u.e.r) {
	  NEW (f->u.e.r, Expr);
	  f = f->u.e.r;
	  f->type = E_CONCAT;
	  f->u.e.l = NULL;
	  f->u.e.r = NULL;
	}
	e = e->u.e.r;
      }
    }
    break;

  case E_ENUM_CONST:
    {
      Data *d = (Data *) e->u.fn.s;
      d = d->Expand (d->getns(), d->getns()->CurScope(), 0, NULL);
      ret->type = E_INT;
      ret->u.ival.v = d->enumVal ((char *)e->u.fn.r);
      ret->u.ival.v_extra = NULL;
      *width = TypeFactory::bitWidth (d);
      if (flags & ACT_EXPR_EXFLAG_CHPEX) {
	BigInt *btmp = new BigInt (*width, 0, 1);
	btmp->setVal (0, ret->u.ival.v);
	ret->u.ival.v_extra = btmp;
      }
      else {
	tmp = TypeFactory::NewExpr (ret);
	FREE (ret);
	ret = tmp;
      }
    }
    break;
    
  default:
    fatal_error ("Unknown expression type (%d)!", e->type);
    break;
  }
  if (ret->type == E_INT && ret->u.ival.v_extra) {
    BigInt *tmp = (BigInt *) ret->u.ival.v_extra;
    Assert (tmp->getWidth () == *width, "What?");
  }
  return ret;
}

Expr *expr_expand (Expr *e, ActNamespace *ns, Scope *s, unsigned int flags)
{
  int w;
  return _expr_expand (&w, e, ns, s, flags);
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

Expr *expr_dup_const (Expr *e)
{
  Assert (expr_is_a_const (e), "What?");
  if (e->type != E_REAL) {
    return e;
  }
  else {
    Expr *ret;
    NEW (ret, Expr);
    *ret = *e;
    return ret;
  }
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

AExpr::AExpr (ActId *id)
{
  Expr *e;
  NEW (e, Expr);
  e->type = E_VAR;
  e->u.e.l = (Expr *)id;
  e->u.e.r = NULL;

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
    Expr *e = (Expr *)l;
    if (e->type == E_SUBRANGE || e->type == E_TYPE || e->type == E_ARRAY
	|| e->type == E_SELF || e->type == E_REAL || e->type == E_SELF_ACK) {
      FREE (e);
    }
    else if (e->type == E_VAR) {
      delete ((ActId *)e->u.e.l);
      FREE (e);
    }
    else {
      expr_ex_free (e);
    } 
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

  if (sz <= 1) return;
  
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
	  && xe->type != E_ARRAY && xe->type != E_SUBRANGE &&
	  xe->type != E_TYPE) {
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
      newl = (AExpr *)expr_dup ((Expr *)l);
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

  



static void efree_ex (Expr *e)
{
  if (!e) return;

  switch (e->type) {
  case E_INT:
    if (e->u.ival.v_extra) {
      delete ((BigInt *)e->u.ival.v_extra);
    }
  case E_TRUE:
  case E_FALSE:
  case E_REAL:
    break;

  case E_FUNCTION:
    efree_ex (e->u.fn.r);
    break;

  case E_VAR:
  case E_PROBE:
    if (e->u.e.l) {
      delete (ActId *)e->u.e.l;
    }
    break;

  case E_BITFIELD:
    if (e->u.e.l) {
      delete (ActId *) (e->u.e.l);
    }
    FREE (e->u.e.r);
    break;

  case E_RAWFREE:
    if (e->u.e.l)  FREE (e->u.e.l);
    if (e->u.e.r) efree_ex (e->u.e.r);
    break;

  default:
    if (e->u.e.l) efree_ex (e->u.e.l);
    if (e->u.e.r) efree_ex (e->u.e.r);
    break;
  }
  if (e->type == E_TRUE || e->type == E_FALSE ||
      (e->type == E_INT && !e->u.ival.v_extra)) {
    /* cached */
  }
  else {
     FREE (e);
  }
  return;
}

void expr_ex_free (Expr *e)
{
  efree_ex (e);
}


const char *expr_op_name (int t)
{
  switch (t) {
  case E_AND:
    return "&";
    break;
  case E_OR:
    return "|";
    break;
  case E_NOT:
    return "~";
    break;
  case E_PLUS:
    return "+";
    break;
  case E_MINUS:
    return "-";
    break;
  case E_MULT:
    return "*";
    break;
  case E_DIV:
    return "/";
    break;
  case E_MOD:
    return "%";
    break;
  case E_LSL:
    return "<<";
    break;
  case E_LSR:
    return ">>";
    break;
  case E_ASR:
    return ">>>";
    break;
  case E_UMINUS:
    return "-";
    break;
  case E_QUERY:
    return "?";
    break;
  case E_XOR:
    return "^";
    break;
  case E_LT:
    return "<";
    break;
  case E_GT:
    return ">";
    break;
  case E_LE:
    return "<=";
    break;
  case E_GE:
    return ">=";
    break;
  case E_EQ:
    return "=";
    break;
  case E_NE:
    return "!=";
    break;
  case E_COLON:
    return ":";
    break;
  case E_PROBE:
    return "#";
    break;
  case E_COMMA:
    return ",";
    break;
  case E_CONCAT:
    return "{.,.}";
    break;
  case E_BITFIELD:
    return "{}";
    break;
  case E_COMPLEMENT:
    return "~";
    break;
  default:
    return "????";
    break;
  }
}

static char _expr_type_char (int type)
{
  switch (type) {
  case E_AND: return 'a'; break;
  case E_OR: return 'o'; break;
  case E_NOT: return 'n'; break;
  case E_PLUS: return 'p'; break;
  case E_MINUS: return 'm'; break;
  case E_MULT: return 's'; break;
  case E_DIV: return 'd'; break;
  case E_MOD: return 'r'; break;
  case E_LSL: return 'f'; break;
  case E_LSR: return 'h'; break;
  case E_ASR: return 'j'; break;
  case E_UMINUS: return 'u'; break;
  case E_QUERY: return 'q'; break;
  case E_XOR: return 'x'; break;
  case E_LT: return 'l'; break;
  case E_GT: return 'g'; break;
  case E_LE: return 'k'; break;
  case E_GE: return 'z'; break;
  case E_EQ: return 'e'; break;
  case E_NE: return 'y'; break;
  case E_COLON: return 'i'; break;
  case E_PROBE: return '#'; break;
  case E_COMMA: return ','; break;
  case E_CONCAT: return 'c'; break;
  case E_BITFIELD: return 'b'; break;
  case E_COMPLEMENT: return 't'; break;

  case E_BUILTIN_BOOL: return 'B'; break;
  case E_BUILTIN_INT: return 'I'; break;
    
  default: return '?'; break;
  }
}

static void _expr_expand_sz (char **buf, int *len, int *sz, int amt)
{
  if (*len >= (*sz-amt)) {
    REALLOC (*buf, char, 50 + amt + *sz);
    *sz += 50 + amt;
  }
}

static void _expr_append_char (char **buf, int *len, int *sz, char c)
{
  _expr_expand_sz (buf, len, sz, 1);
  (*buf)[*len] = c;
  *len = *len + 1;
  (*buf)[*len] = '\0';
}

static void _expr_to_var (char **buf, int *len, int *sz,
			  list_t *ids, ActId *v)
{
  int x = 0;
  listitem_t *li;
  int count = 0;

  for (li = list_first (ids); li; li = list_next (li)) {
    count++;
    if (v == (ActId *) list_value (li)) {
      break;
    }
    x++;
  }
  if (!li) {
    x = 0;
    for (li = list_first (ids); li; li = list_next (li)) {
      if (v->isEqual ((ActId *) list_value (li))) {
	break;
      }
      x++;
    }
  }
  else {
    count--;
    while (li) { 
       count++;
       li = list_next (li);
    }
  }
  if (*len >= (*sz-5)) {
    REALLOC ((*buf), char, (50 + *sz));
    *sz += 50;
  }
  if (count > 10) {
  (*buf)[*len] = 'v';
  *len = *len + 1;
  snprintf (*buf + *len, *sz - *len, "%d", x);
  *len += strlen (*buf + *len);
  }
  else {
    (*buf)[*len] = '0' + x;
    *len = *len + 1;
    (*buf)[*len] = '\0';
  }
}

static void _expr_to_string (char **buf, int *len, int *sz,
			     list_t *ids, Expr *e, int *isassoc)
{
  int iszero = 0;
  int tmp;
  switch (e->type) {
  case E_PLUS:
  case E_MULT:
  case E_AND:  case E_OR:  case E_XOR:
    if (*isassoc == 0) {
      iszero = 1;
    }
    if (e->u.e.l->type == e->type) {
      (*isassoc) = (*isassoc) + 1;
      _expr_to_string (buf, len, sz, ids, e->u.e.l, isassoc);
    }
    else {
      tmp = 0;
      _expr_to_string (buf, len, sz, ids, e->u.e.l, &tmp);
    }
    tmp = 0;
    _expr_to_string (buf, len, sz, ids, e->u.e.r, &tmp);
    if (iszero) {
      _expr_append_char (buf, len, sz, _expr_type_char (e->type));
      while (*isassoc) {
	_expr_append_char (buf, len, sz, _expr_type_char (e->type));
	(*isassoc) = (*isassoc) - 1;
      }
    }
    break;
    
  case E_MINUS:
  case E_DIV:  case E_MOD:
  case E_LSL:  case E_LSR:  case E_ASR:
  case E_LT:  case E_GT:
  case E_LE:  case E_GE:
  case E_EQ:  case E_NE:
    _expr_to_string (buf, len, sz, ids, e->u.e.l, &iszero);
    _expr_to_string (buf, len, sz, ids, e->u.e.r, &iszero);
    _expr_append_char (buf, len, sz, _expr_type_char (e->type));
    break;
    
  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
    _expr_to_string (buf, len, sz, ids, e->u.e.l, &iszero);
    _expr_append_char (buf, len, sz, _expr_type_char (e->type));
    break;

  case E_QUERY:
    _expr_to_string (buf, len, sz, ids, e->u.e.l, &iszero);
    _expr_to_string (buf, len, sz, ids, e->u.e.r->u.e.l, &iszero);
    _expr_to_string (buf, len, sz, ids, e->u.e.r->u.e.r, &iszero);
    _expr_append_char (buf, len, sz, _expr_type_char (e->type));
    break;

  case E_INT:
    _expr_expand_sz (buf, len, sz, 32);
    (*buf)[*len] = 'i';
    *len = *len + 1;
    snprintf (*buf + *len, *sz - *len, "%lu", e->u.ival.v);
    *len += strlen (*buf + *len);
    break;
    
  case E_TRUE:
  case E_FALSE:
    if (*len >= (*sz-2)) {
      REALLOC (*buf, char, 50 + *sz);
      *sz += 50;
    }
    (*buf)[*len] = 'i';
    (*buf)[*len+1] = (e->type == E_TRUE ? '1' : '0');
    (*buf)[*len+2] = '\0';
    *len = *len + 2;
    break;

  case E_PROBE:
  case E_VAR:
    _expr_to_var (buf, len, sz, ids, (ActId *)e->u.e.l);
    break;

  case E_CONCAT:
    {
      Expr *f = e;
      while (f) {
	_expr_to_string (buf, len, sz, ids, f->u.e.l, &iszero);
	f = f->u.e.r;
      }
      _expr_append_char (buf, len, sz, _expr_type_char (e->type));
    }
    break;
    
  case E_BITFIELD:
    /* var */
    _expr_to_var (buf, len, sz, ids, (ActId *)e->u.e.l);
    _expr_append_char (buf, len, sz, _expr_type_char (e->type));
    if (e->u.e.r->u.e.l) {
      _expr_to_string (buf, len, sz, ids, e->u.e.r->u.e.l, &iszero);
    }
    _expr_to_string (buf, len, sz, ids, e->u.e.r->u.e.r, &iszero);
    _expr_append_char (buf, len, sz, _expr_type_char (e->type));
    break;

  case E_BUILTIN_BOOL:
    _expr_to_string (buf, len, sz, ids, e->u.e.l, &iszero);
    _expr_append_char (buf, len, sz, _expr_type_char (e->type));
    break;
    
  case E_BUILTIN_INT:
    _expr_to_string (buf, len, sz, ids, e->u.e.l, &iszero);
    if (e->u.e.r) {
      _expr_to_string (buf, len, sz, ids, e->u.e.r, &iszero);
    }
    _expr_append_char (buf, len, sz, _expr_type_char (e->type));
    if (e->u.e.r) {
      _expr_append_char (buf, len, sz, '1');
    }
    else {
      _expr_append_char (buf, len, sz, '0');
    }
    break;

  case E_FUNCTION:
  case E_COMMA:
  case E_COLON:
  default:
    fatal_error ("What? %d\n", e->type);
    break;
  }
}


char *act_expr_to_string (list_t *id_list, Expr *e)
{
  char *buf;
  int len, sz;
  int iszero = 0;
  if (!e) {
    return Strdup ("");
  }
  sz = 1024;
  MALLOC (buf, char, sz);
  buf[0] = '\0';
  len = 0;

  _expr_to_string (&buf, &len, &sz, id_list, e, &iszero);

  char *ret = Strdup (buf);
  FREE (buf);
  return ret;
}


static void _prs_expr_to_string (char **buf, int *len, int *sz,
				 list_t *ids,
				 act_prs_expr_t *e, int isinvert,
				 int *isassoc)
{
  char c;
  int iszero = 0;
  int tmp = 0;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    if (*isassoc == 0) {
      iszero = 1;
    }
    if (e->u.e.l->type == e->type) {
      (*isassoc) = (*isassoc) + 1;
      _prs_expr_to_string (buf, len, sz, ids, e->u.e.l, isinvert, isassoc);
    }
    else {
      tmp = 0;
      _prs_expr_to_string (buf, len, sz, ids, e->u.e.l, isinvert, &tmp);
    }
    tmp = 0;
    _prs_expr_to_string (buf, len, sz, ids, e->u.e.r, isinvert, &tmp);
    if (isinvert) {
      c = _expr_type_char (e->type == ACT_PRS_EXPR_AND ? E_OR : E_AND);
    }
    else {
      c = _expr_type_char (e->type == ACT_PRS_EXPR_AND ? E_AND : E_OR);
    }
    if (iszero) {
      _expr_append_char (buf, len, sz, c);
      while (*isassoc) {
	_expr_append_char (buf, len, sz, c);
	(*isassoc) = (*isassoc) - 1;
      }
    }
    break;

  case ACT_PRS_EXPR_NOT:
    _prs_expr_to_string (buf, len, sz, ids, e->u.e.l, 1 - isinvert, isassoc);
#if 0    
    c = _expr_type_char (E_NOT);
    _expr_append_char (buf, len, sz, c);
#endif    
    break;
    
  case ACT_PRS_EXPR_VAR:
    _expr_to_var (buf, len, sz, ids, e->u.v.id);
    if (isinvert) {
      c = _expr_type_char (E_NOT);
      _expr_append_char (buf, len, sz, c);
    }
    //ret->u.ival.v.sz = act_expand_size (p->u.ival.v.sz, ns, s);
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    if (*len >= (*sz-2)) {
      REALLOC (*buf, char, 50 + *sz);
      *sz += 50;
    }
    (*buf)[*len] = 'i';
    if (isinvert) {
      (*buf)[*len+1] = (e->type == ACT_PRS_EXPR_TRUE ? '0' : '1');
    }
    else {
      (*buf)[*len+1] = (e->type == ACT_PRS_EXPR_TRUE ? '1' : '0');
    }
    (*buf)[*len+2] = '\0';
    *len = *len + 2;
    break;

  default:
    fatal_error ("What? %d\n", e->type);
    break;
  }
}

char *act_prs_expr_to_string (list_t *id_list,  act_prs_expr_t *e)
{
  char *buf;
  int len, sz;
  int iszero = 0;
  if (!e) {
    return Strdup ("");
  }
  sz = 1024;
  MALLOC (buf, char, sz);
  buf[0] = '\0';
  len = 0;

  _prs_expr_to_string (&buf, &len, &sz, id_list, e, 0, &iszero);

  char *ret = Strdup (buf);
  FREE (buf);
  return ret;
}

static void _add_id (list_t *ids, ActId *id)
{
  listitem_t *li;
  for (li = list_first (ids); li; li = list_next (li)) {
    if (id == (ActId *) list_value (li)) {
      return;
    }
    if (id->isEqual ((ActId *)list_value (li))) {
      return;
    }
  }
  list_append (ids, id);
}

static void _collect_ids_from_expr (list_t *ids, Expr *e)
{
  if (!e) return;
  
  switch (e->type) {
  case E_PLUS:
  case E_MULT:
  case E_AND:  case E_OR:  case E_XOR:
  case E_MINUS:
  case E_DIV:  case E_MOD:
  case E_LSL:  case E_LSR:  case E_ASR:
  case E_LT:  case E_GT:
  case E_LE:  case E_GE:
  case E_EQ:  case E_NE:
    _collect_ids_from_expr (ids, e->u.e.l);
    _collect_ids_from_expr (ids, e->u.e.r);
    break;
    
  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
    _collect_ids_from_expr (ids, e->u.e.l);
    break;

  case E_QUERY:
    _collect_ids_from_expr (ids, e->u.e.l);
    _collect_ids_from_expr (ids, e->u.e.r->u.e.l);
    _collect_ids_from_expr (ids, e->u.e.r->u.e.r);
    break;

  case E_INT:
  case E_TRUE:
  case E_FALSE:
    break;

  case E_PROBE:
  case E_VAR:
    _add_id (ids, (ActId *)e->u.e.l);
    break;

  case E_CONCAT:
    {
      Expr *f = e;
      while (f) {
	_collect_ids_from_expr (ids, f->u.e.l);
	f = f->u.e.r;
      }
    }
    break;
    
  case E_BITFIELD:
    /* var */
    _add_id (ids, (ActId *)e->u.e.l);
    break;

  case E_BUILTIN_BOOL:
    _collect_ids_from_expr (ids, e->u.e.l);
    break;
    
  case E_BUILTIN_INT:
    _collect_ids_from_expr (ids, e->u.e.l);
    break;

  case E_FUNCTION:
  case E_COMMA:
  case E_COLON:
  default:
    fatal_error ("What? %d (functions are not supported; inline them first)\n", e->type);
    break;
  }
}

void act_expr_collect_ids (list_t *l, Expr *e)
{
  if (!l) {
    warning ("act_expr_collect_ids: list was NULL");
    return;
  }
  return _collect_ids_from_expr (l, e);
}


int act_expr_bitwidth (int etype, int lw, int rw)
{
  int ret;
  int *width = &ret;
  
  switch (etype) {
  case E_AND:
  case E_OR:
  case E_XOR:
    WIDTH_UPDATE(WIDTH_MAX);
    break;

  case E_PLUS:
  case E_MINUS:
    WIDTH_UPDATE (WIDTH_MAX1);
    break;
    
  case E_MULT:
      WIDTH_UPDATE (WIDTH_SUM);
      break;
      
  case E_DIV: 
  case E_LSR:
  case E_ASR:
      WIDTH_UPDATE (WIDTH_LEFT);
      break;
      
  case E_MOD:
      WIDTH_UPDATE (WIDTH_RIGHT);
      break;
      
  case E_LSL:
      WIDTH_UPDATE (WIDTH_LSHIFT);
      break;

  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
    *width = 1;
    break;
    
  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    WIDTH_UPDATE (WIDTH_LEFT);
    break;
    
  case E_QUERY:
  case E_COLON:
    WIDTH_UPDATE (WIDTH_MAX);
    break;

  case E_ANDLOOP:
  case E_ORLOOP:
  case E_BITFIELD:
  case E_PROBE:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
  case E_FUNCTION:
  case E_INT:
  case E_VAR:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
  case E_ARRAY:
  case E_SUBRANGE:
  case E_SELF:
  case E_SELF_ACK:
  case E_CONCAT:
  case E_ENUM_CONST:
  default:
    return -1;
    break;
  }
  return ret;
}

int act_expr_intwidth (unsigned long v)
{
  return _int_width (v);
}


int act_expr_getconst_real (Expr *e, double *val)
{
  if (!e) return 0;
  if (e->type == E_INT) {
      *val = e->u.ival.v;
  }
  else if (e->type == E_REAL) {
    *val = e->u.f;
  }
  else {
    return 0;
  }
  return 1;
}

int act_expr_getconst_int (Expr *e, int *val)
{
  if (!e) return 0;
  if (e->type == E_INT) {
    *val = e->u.ival.v;
  }
  else if (e->type == E_REAL) {
    *val = e->u.f;
  }
  else {
    return 0;
  }
  return 1;
}
