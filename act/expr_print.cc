/*************************************************************************
 *
 *  Copyright (c) 2025 Rajit Manohar
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
#include <act/act.h>
#include <common/int.h>
#include <string.h>
#include <string>
#include "expr_width.h"

/*
 *
 * Expression functions: printing
 *
 */


#define PRINT_STEP				\
  do {						\
    len = strlen (buf+k);			\
    k += len;					\
    sz -= len;					\
    if (sz <= 1) return;			\
  } while (0)

#define ADD_CHAR(ch)				\
  do {						\
    buf[k] = ch;				\
    k++;					\
    buf[k] = '\0';				\
    sz -= 1;					\
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

    /* yes, this can actually be non-associative... */
  case E_PLUS:  EMIT_BIN_NONASSOC (8, "+"); break;
  case E_MINUS: EMIT_BIN_NONASSOC (8, "-"); break;

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
  case E_ORLOOP:
  case E_PLUSLOOP:
  case E_MULTLOOP:
  case E_XORLOOP:
    snprintf (buf+k, sz, "(X");
    if (sz > 2) {
      if (e->type == E_ANDLOOP) {
	buf[k+1] = '&';
      }
      else if (e->type == E_ORLOOP) {
	buf[k+1] = '|';
      }
      else if (e->type == E_PLUSLOOP) {
	buf[k+1] = '+';
      }
      else if (e->type == E_MULTLOOP) {
	buf[k+1] = '*';
      }
      else if (e->type == E_XORLOOP) {
	buf[k+1] = '^';
      }
    }
    PRINT_STEP;
    snprintf (buf+k, sz, "%s:", (char *)e->u.e.l->u.e.l);
    PRINT_STEP;
    _print_expr (buf+k, sz, e->u.e.r->u.e.l, 1, -1);
    PRINT_STEP;
    if (e->u.e.r->u.e.r->u.e.l) {
      snprintf (buf+k, sz, ".u.");
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
      int is_special_struct = 0;
      Assert (f, "Hmm.");

      if (f->isUserMethod()) {
	int pos;
	const char *nm = f->getName();

	// if it is "int<>" or "struct<>struc
	{
	  int i;
	  for (i=0; nm[i]; i++) {
	    if (nm[i] == '/') break;
	  }
	  i++;
	  if (strcmp (nm + i, "int") == 0 ||
	      strcmp (nm + i, "int<>") == 0) {
	    // int(...)
	    snprintf (buf+k, sz, "int(");
	    PRINT_STEP;
	    if (prec < 0) {
	      sprint_uexpr (buf+k, sz, e->u.fn.r->u.e.l);
	    }
	    else {
	      sprint_expr (buf+k, sz, e->u.fn.r->u.e.l);
	    }
	    PRINT_STEP;
	    snprintf (buf+k, sz, ")");
	    PRINT_STEP;
	    return;
	  }
	  else {
	    UserDef *parent = f->CurScope()->Parent()->getUserDef();
	    Assert (parent, "What?");

	    int cnt = 0;
	    while (nm[i + cnt] && nm[i + cnt] != '<') {
	      cnt++;
	    }
	    
	    if (strncmp (nm + i, parent->getName(), cnt) == 0) {
	      is_special_struct = 1;
	    }
	  }
	}

	if (!is_special_struct) {
	  Assert (e->u.fn.r &&
		  e->u.fn.r->type == E_LT &&
		  (!e->u.fn.r->u.e.l || e->u.fn.r->u.e.l->type == E_VAR), "What?");
	  ActId *id = (ActId *) (e->u.fn.r->u.e.l ?
				 e->u.fn.r->u.e.l->u.e.l : NULL);
	  if (id) {
	    id->sPrint (buf+k, sz);
	  }
	  pos = 0;
	  while (nm[pos] != '/') {
	    pos++;
	  }
	  pos++;
	  PRINT_STEP;
	  if (id) {
	    ADD_CHAR('.');
	  }
	  while (nm[pos] && nm[pos] != '<') {
	    ADD_CHAR (nm[pos]);
	    pos++;
	  }
	  ADD_CHAR('(');
	  tmp = e->u.fn.r->u.e.r;
	}
	else {
	  if (f->getns() && f->getns() != ActNamespace::Global()) {
	    char *s = f->getns()->Name ();
	    snprintf (buf+k, sz, "%s::", s);
	    PRINT_STEP;
	    FREE (s);
	  }
#if 0	  
	  pos = 0;
	  while (nm[pos] != '/') {
	    pos++;
	  }
	  pos++;
	  while (nm[pos] && nm[pos] != '<') {
	    ADD_CHAR (nm[pos]);
	    pos++;
	  }
	  if (nm[pos+1] != '>') {
	    while (nm[pos]) {
	      ADD_CHAR (nm[pos]);
	      pos++;
	    }
	  }
#endif
	  if (ActNamespace::Act()) {
	    ActNamespace::Act()->msnprintfproc (buf+k, sz, f->CurScope()->Parent()->getUserDef(), 1);
	  }
	  else {
	    snprintf (buf+k, sz, "%s", f->CurScope()->Parent()->getUserDef()->getName());
	  }
	  PRINT_STEP;
#if 0	  
	  pos = 0;
	  while (nm[pos] != '/') {
	    ADD_CHAR (nm[pos]);
	    pos++;
	  }
#endif	  
	  ADD_CHAR('(');
	  if (e->u.fn.r->type == E_GT) {
	    Assert (0, "This cannot happen!");
	    if (prec < 0) {
	      sprint_uexpr (buf+k, sz, e->u.fn.r->u.e.r->u.e.l);
	    }
	    else {
	      sprint_expr (buf+k, sz, e->u.fn.r->u.e.r->u.e.l);
	    }
	  }
	  else {
	    if (prec < 0) {
	      sprint_uexpr (buf+k, sz, e->u.fn.r->u.e.l);
	    }
	    else {
	      sprint_expr (buf+k, sz, e->u.fn.r->u.e.l);
	    }
	  }
	  PRINT_STEP;
	  snprintf (buf+k, sz, ")");
	  return;
	}
      }

      if (f->isUserMethod() && is_special_struct ||
	  !f->isUserMethod()) {
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

  case E_USERMACRO:
    {
      UserMacro *um = (UserMacro *)e->u.fn.s;
      Expr *tmp;

      if (um->isBuiltinMacro()) {
	// different data structure!
	if (um->isBuiltinStructMacro()) {
	  UserDef *ux = um->Parent();
	  if (ux->getns() && ux->getns() != ActNamespace::Global()) {
	    char *s = ux->getns()->Name();
	    snprintf (buf+k, sz, "%s::", s);
	    PRINT_STEP;
	    FREE (s);
	  }
	  if (ActNamespace::Act()) {
	    ActNamespace::Act()->msnprintfproc (buf+k, sz, ux, 1);
	  }
	  else {
	    snprintf (buf+k, sz, "%s", ux->getName());
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
	  if (e->u.fn.r->type == E_GT) {
	    if (prec < 0) {
	      sprint_uexpr (buf+k, sz, e->u.fn.r->u.e.r->u.e.l);
	    }
	    else {
	      sprint_expr (buf+k, sz, e->u.fn.r->u.e.r->u.e.l);
	    }
	  }
	  else {
	    if (prec < 0) {
	      sprint_uexpr (buf+k, sz, e->u.fn.r->u.e.l);
	    }
	    else {
	      sprint_expr (buf+k, sz, e->u.fn.r->u.e.l);
	    }
	  }
	}
	else {
	  snprintf (buf+k, sz, "%s(", um->getName());
	  PRINT_STEP;
	  if (prec < 0) {
	    sprint_uexpr (buf+k, sz, e->u.fn.r);
	  }
	  else {
	    sprint_expr (buf+k, sz, e->u.fn.r);
	  }
	}
	PRINT_STEP;
	snprintf (buf+k, sz, ")");
	return;
      }
      //Assert (e->u.fn.r->u.e.l->type == E_VAR, "What?");
      //ActId *x = (ActId *)e->u.fn.r->u.e.l->u.e.l;
      //x->sPrint (buf+k, sz);
      if (prec < 0) {
	sprint_uexpr (buf+k, sz, e->u.fn.r->u.e.l);
      }
      else {
	sprint_expr (buf+k, sz, e->u.fn.r->u.e.l);
      }
      PRINT_STEP;
      snprintf (buf+k, sz, ".");
      PRINT_STEP;
      snprintf (buf+k, sz, "%s", um->getName());
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
	tmp = e->u.fn.r->u.e.r->u.e.r;
      }
      else {
	tmp = e->u.fn.r->u.e.r;
      }
      int correction = um->getNumPorts();
      if (um->isFunction() && TypeFactory::isParamType (um->getRetType())) {
	correction -= um->Parent()->getNumParams();
      }
      while (tmp && correction > 0) {
	if (prec < 0) {
	  sprint_uexpr (buf+k, sz, tmp->u.e.l);
	}
	else {
	  sprint_expr (buf+k, sz, tmp->u.e.l);
	}
	PRINT_STEP;
	tmp = tmp->u.e.r;
	correction--;
	if (tmp && correction > 0) {
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
    snprintf (buf+k, sz, " {");
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

  case E_STRUCT_REF:
    if (prec < 0) {
      sprint_uexpr (buf+k, sz, e->u.e.l);
    }
    else {
      sprint_expr (buf+k, sz, e->u.e.l);
    }
    PRINT_STEP;
    snprintf (buf+k, sz, ".");
    PRINT_STEP;
    if (prec < 0) {
      sprint_uexpr (buf+k, sz, e->u.e.r);
    }
    else {
      sprint_expr (buf+k, sz, e->u.e.r);
    }
    PRINT_STEP;
    break;

  case E_ENUM_CONST:
    {
      Data *d = (Data *) e->u.fn.s;
      snprintf (buf+k, sz, "%s.", d->getName());
      PRINT_STEP;
      snprintf (buf+k, sz, "%s", (char *) e->u.fn.r);
      PRINT_STEP;
    }
    break;

  case E_PSTRUCT:
    {
      PStruct *ps = (PStruct *)e->u.e.r;
      struct expr_pstruct *v = (struct expr_pstruct *) e->u.e.l;
      snprintf (buf+k, sz, "%s{", ps->getName());
      PRINT_STEP;
      int nb, ni, nr, nt;
      ps->getCounts (&nb, &ni, &nr, &nt);
      bool first = true;
      for (int i=0; i < nb; i++) {
	if (!first) {
	  snprintf (buf+k, sz, ",");
	  PRINT_STEP;
	  first = false;
	}
	snprintf (buf+k, sz, "%c", v->pbool[i] ? 't' : 'f');
	PRINT_STEP;
      }
      for (int i=0; i < ni; i++) {
	if (!first) {
	  snprintf (buf+k, sz, ",");
	  PRINT_STEP;
	  first = false;
	}
	snprintf (buf+k, sz, "%lu", v->pint[i]);
	PRINT_STEP;
      }
      for (int i=0; i < nr; i++) {
	if (!first) {
	  snprintf (buf+k, sz, ",");
	  PRINT_STEP;
	  first = false;
	}
	snprintf (buf+k, sz, "%g", v->preal[i]);
	PRINT_STEP;
      }
      for (int i=0; i < nt; i++) {
	if (!first) {
	  snprintf (buf+k, sz, ",");
	  PRINT_STEP;
	  first = false;
	}
	char tbuf[1024];
	((InstType *)v->ptype[i])->sPrint (tbuf, 1024);
	snprintf (buf+k, sz, "%s", tbuf);
	PRINT_STEP;
      }
      snprintf (buf+k, sz, "}");
      PRINT_STEP;
    }
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
	  xe->type != E_TYPE && xe->type != E_PSTRUCT) {
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
AExpr *AExpr::Clone(ActNamespace *orig, ActNamespace *newns)
{
  AExpr *newl, *newr;

  newl = NULL;
  newr = NULL;
  if (l) {
    if (t != AExpr::EXPR) {
      newl = l->Clone (orig, newns);
    }
    else {
      newl = (AExpr *) expr_update (expr_predup ((Expr *)l), orig, newns);
    }
  }
  if (r) {
    newr = r->Clone (orig, newns);
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

  case E_PSTRUCT:
    if (e->u.e.l) {
      struct expr_pstruct *p = (struct expr_pstruct *) e->u.e.l;
      if (p->pbool) {
	FREE (p->pbool);
      }
      if (p->preal) {
	FREE (p->preal);
      }
      if (p->pint) {
	FREE (p->pint);
      }
      if (p->ptype) {
	FREE (p->ptype);
      }
      FREE (p);
    }
    break;

  case E_FUNCTION:
  case E_USERMACRO:
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
    FREE (e->u.e.r); // l, r fields are constants
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

static void _expr_to_label (char **buf, int *len, int *sz,
			    list_t *ids, const char *v)
{
  int x = 0;
  listitem_t *li;

  for (li = list_first (ids); li; li = list_next (li)) {
    if (v == (const char *) list_value (li)) {
      break;
    }
    x++;
  }
  if (*len >= (*sz-5)) {
    REALLOC ((*buf), char, (50 + *sz));
    *sz += 50;
  }
  (*buf)[*len] = 'v';
  *len = *len + 1;
  (*buf)[*len] = 'l';
  *len = *len + 1;
  snprintf (*buf + *len, *sz - *len, "%d", x);
  *len += strlen (*buf + *len);
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

  case E_USERMACRO:
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

  case ACT_PRS_EXPR_LABEL:
    _expr_to_label (buf, len, sz, ids, e->u.l.label);
    if (isinvert) {
      c = _expr_type_char (E_NOT);
      _expr_append_char (buf, len, sz, c);
    }
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

  case E_USERMACRO:
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
  case E_PLUSLOOP:
  case E_MULTLOOP:
  case E_XORLOOP:
  case E_BITFIELD:
  case E_PROBE:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
  case E_FUNCTION:
  case E_USERMACRO:
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
  int w = 0;
  while (v) {
    v = v >> 1;
    w++;
  }
  if (w == 0) { w = 1; }
  return w;
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


Data *act_expr_is_structure (Scope *s, Expr *e, int *error)
{
  InstType *it;
  UserDef *u;
  if (!e) {
    if (error) { *error = 5; }
    return NULL;
  }
  if (!s->isExpanded()) {
    if (error) { *error = 4; }
    return NULL;
  }
  if (error) {
    *error = 0;
  }
  if (e->type == E_VAR) {
     it = s->FullLookup ((ActId *)e->u.e.l, NULL);
     if (!it && error) {
       *error = 2;
     }
  }
  else if (e->type == E_FUNCTION) {
     it = ((Function *)e->u.fn.s)->getRetType ();
  }
  else {
     it = NULL;
    if (error) {
      *error = 1;
    }
  }
  if (!it) return NULL;
  u = dynamic_cast<UserDef *>(it->BaseType());
  if (!u || !TypeFactory::isStructure (u)) {
    if (error) {
      *error = 3;
    }
    return NULL;
  }
  return dynamic_cast<Data *> (u);
}


Expr *act_expr_var (ActId *id)
{
  Expr *e;
  NEW (e, Expr);
  e->type = E_VAR;
  e->u.e.l = (Expr *) id;
  e->u.e.r = NULL;
  return e;
}

Function *_act_fn_replace (ActNamespace *orig, ActNamespace *newns, Function *f);
UserDef *_act_userdef_replace (ActNamespace *orig, ActNamespace *newns,
			       UserDef *u);


Expr *expr_update (Expr *e, ActNamespace *orig, ActNamespace *newns)
{
  Expr *ret, *te;
  ActId *xid;
  Expr *tmp;
  int pc;
  int lw, rw;
  
  if (!e) return NULL;
  if (!orig) return e;

#define EXP_UPDATE(x)  if (x) { x = expr_update (x, orig, newns); }

  switch (e->type) {
  case E_ANDLOOP:
  case E_ORLOOP:
  case E_PLUSLOOP:
  case E_MULTLOOP:
  case E_XORLOOP:
    EXP_UPDATE (e->u.e.r->u.e.l); // hi
    EXP_UPDATE (e->u.e.r->u.e.r->u.e.l); // lo
    EXP_UPDATE (e->u.e.r->u.e.r->u.e.r); // expr
    break;
    
  case E_AND:
  case E_OR:
  case E_XOR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
    EXP_UPDATE (e->u.e.l);
    EXP_UPDATE (e->u.e.r);
    break;
    
  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    EXP_UPDATE (e->u.e.l);
    break;
    
  case E_QUERY:
    EXP_UPDATE (e->u.e.l);
    EXP_UPDATE (e->u.e.r->u.e.l);
    EXP_UPDATE (e->u.e.r->u.e.r);

  case E_COLON:
    EXP_UPDATE (e->u.e.l);
    EXP_UPDATE (e->u.e.r);
    break;

  case E_BITFIELD:
    ((ActId *)e->u.e.l)->moveNS (orig, newns);
    EXP_UPDATE (e->u.e.r->u.e.l);
    EXP_UPDATE (e->u.e.r->u.e.r);
    break;

  case E_PROBE:
    ((ActId *)e->u.e.l)->moveNS (orig, newns);
    break;

  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    EXP_UPDATE (e->u.e.l);
    EXP_UPDATE (e->u.e.r);
    break;
  
  case E_FUNCTION:
    {
      Expr *tmp;
      e->u.fn.s = (char *) _act_fn_replace (orig, newns, (Function *)e->u.fn.s);
      tmp = e->u.fn.r;
      if (tmp && tmp->type == E_GT) {
	tmp = tmp->u.e.l;
	while (tmp) {
	  EXP_UPDATE (tmp->u.e.l);
	  tmp = tmp->u.e.r;
	}
	tmp = e->u.fn.r->u.e.r;
      }
      else {
	tmp = e->u.fn.r;
      }
      while (tmp) {
	EXP_UPDATE (tmp->u.e.l);
	tmp = tmp->u.e.r;
      }
    }
    break;

  case E_USERMACRO:
    {
      UserMacro *um = (UserMacro *)e->u.fn.s;
      UserDef *u = um->Parent ();
      u = _act_userdef_replace (orig, newns, u);
      if (u != um->Parent ()) {
	e->u.fn.s = (char *) u->getMacro (um->getName());
	if (um->isBuiltinMacro()) {
	  Assert (0, "FIXME builtinmacros-update");
	}
	else {
	  Expr *tmp;
	  EXP_UPDATE (e->u.fn.r->u.e.l);
	  tmp = e->u.fn.r;
	  if (tmp && tmp->type == E_GT) {
	    tmp = tmp->u.e.l;
	    while (tmp) {
	      EXP_UPDATE (tmp->u.e.l);
	      tmp = tmp->u.e.r;
	    }
	    tmp = e->u.fn.r->u.e.r;
	  }
	  while (tmp) {
	    EXP_UPDATE (tmp->u.e.l);
	    tmp = tmp->u.e.r;
	  }
	}
      }
    }
    break;
      
  case E_VAR:
    ((ActId *)e->u.e.l)->moveNS (orig, newns);
    break;

  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
  case E_ARRAY:
  case E_SUBRANGE:
  case E_SELF:
  case E_SELF_ACK:
    break;

  case E_CONCAT:
    {
      Expr *tmp = e;
      while (tmp) {
	EXP_UPDATE (tmp->u.e.l);
	tmp = tmp->u.e.r;
      }
    }
    break;

  case E_ENUM_CONST:
    {
      Data *d = (Data *) e->u.fn.s;
      UserDef *ux = _act_userdef_replace (orig, newns, d);
      Data *dx = dynamic_cast<Data *> (ux);
      Assert (dx, "What!");
      if (dx != d) {
	e->u.fn.s = (char *) dx;
      }
    }
    break;
    
  default:
    fatal_error ("Unknown expression type (%d)!", e->type);
    break;
  }
  return e;
}




/*
 *  Dag printing
 *
 */
static int _print_dag_expr (struct pHashtable *H,
			    int *count,
			    char *buf, int sz, const Expr *e)
{
  int k = 0;
  int len;
  phash_bucket_t *b;

  if (!e) return -1;
  if (sz <= 1) return -1;

  b = phash_lookup (H, e);
  if (b) {
    return b->i;
  }
  b = phash_add (H, e);
  b->i = *count;
  *count = *count + 1;

#define REC_CALL(x) \
  _print_dag_expr (H, count, buf+k, sz, (x))

#undef PRINT_STEP
#define PRINT_STEP				\
  do {						\
    len = strlen (buf+k);			\
    k += len;					\
    sz -= len;					\
    if (sz <= 1) return b->i;			\
  } while (0)

#undef ADD_CHAR
#define ADD_CHAR(ch)				\
  do {						\
    buf[k] = ch;				\
    k++;					\
    buf[k] = '\0';				\
    sz -= 1;					\
    if (sz <= 1) return b->i;			\
  } while (0)

#undef EMIT_BIN
#define EMIT_BIN(sym)							\
  do {									\
    int n1, n2;								\
    n1 = REC_CALL (e->u.e.l);						\
    PRINT_STEP;								\
    n2 = REC_CALL (e->u.e.r);						\
    PRINT_STEP;								\
    snprintf (buf+k, sz, "@%d <- @%d %s @%d; ", b->i, n1, (sym), n2);	\
    PRINT_STEP;								\
  } while (0)

#undef EMIT_UNOP
#define EMIT_UNOP(sym)						\
  do {								\
    int n1;							\
    n1 = REC_CALL (e->u.e.l);					\
    PRINT_STEP;							\
    snprintf (buf+k, sz, "@%d <- %s @%d; ", b->i, sym, n1);	\
    PRINT_STEP;							\
  } while (0)

#define EMIT_BASE(body)				\
  do {						\
    snprintf (buf+k, sz, "@%d <- ", b->i);	\
    PRINT_STEP;					\
    do {					\
      body;					\
    } while (0);				\
    PRINT_STEP;					\
    snprintf (buf+k, sz, "; ");			\
    PRINT_STEP;					\
  } while (0)

  switch (e->type) {
  case E_PROBE:
    EMIT_BASE(
	      snprintf (buf+k, sz, "#");
	      PRINT_STEP;
	      ((ActId *)(e->u.e.l))->sPrint (buf+k, sz);
	       );
    break;

  case E_NOT: EMIT_UNOP("~"); break;
  case E_COMPLEMENT: EMIT_UNOP("~"); break;
  case E_UMINUS: EMIT_UNOP("-"); break;

  case E_MULT: EMIT_BIN ("*"); break;
  case E_DIV:  EMIT_BIN ("/"); break;
  case E_MOD:  EMIT_BIN ("%"); break;
  case E_PLUS:  EMIT_BIN ("+"); break;
  case E_MINUS: EMIT_BIN ("-"); break;
  case E_LSL: EMIT_BIN ("<<"); break;
  case E_LSR: EMIT_BIN (">>"); break;
  case E_ASR: EMIT_BIN (">>>"); break;
  case E_LT:  EMIT_BIN ("<"); break;
  case E_GT:  EMIT_BIN (">"); break;
  case E_LE:  EMIT_BIN ("<="); break;
  case E_GE:  EMIT_BIN (">="); break;
  case E_EQ:  EMIT_BIN ("="); break;
  case E_NE:  EMIT_BIN ("!="); break;
  case E_AND: EMIT_BIN ("&"); break;
  case E_XOR: EMIT_BIN ("^"); break;
  case E_OR: EMIT_BIN ("|"); break;

  case E_QUERY: /* prec = 3 */
    {
      int n1, n2, n3;
      n1 = REC_CALL(e->u.e.l);
      PRINT_STEP;
      n2 = REC_CALL(e->u.e.r->u.e.l);
      PRINT_STEP;
      n3 = REC_CALL(e->u.e.r->u.e.r);
      PRINT_STEP;
      snprintf (buf+k, sz, "@%d <- @%d ? @%d : @%d; ",
		b->i, n1, n2, n3);
    }
    break;

  case E_INT:
    EMIT_BASE(if (e->u.ival.v_extra) {
	std::string s = ((BigInt *)e->u.ival.v_extra)->sPrint ();
	snprintf (buf+k, sz, "0x%s", s.c_str());
      }
      else {
	snprintf (buf+k, sz, "%lu", e->u.ival.v);
      }
      );
    break;

  case E_REAL:
    EMIT_BASE(snprintf (buf+k, sz, "%g", e->u.f););
    break;

  case E_TRUE:
    EMIT_BASE (snprintf (buf+k, sz, "true"););
    break;

  case E_FALSE:
    EMIT_BASE (snprintf (buf+k, sz, "false"););
    break;

  case E_VAR:
    EMIT_BASE (((ActId *)e->u.e.l)->sPrint (buf+k, sz););
    break;

  case E_FUNCTION:
    {
      UserDef *u = (UserDef *)e->u.fn.s;
      Function *f = dynamic_cast<Function *>(u);
      Expr *tmp;
      int is_special_struct = 0;
      Assert (f, "Hmm.");
      int num, ii;
      int *ids;

      if (e->u.fn.r && e->u.fn.r->type == E_GT) {
	tmp = e->u.fn.r->u.e.r;
      }
      else {
	tmp = e->u.fn.r;
      }
      num = 0;
      while (tmp) {
	num++;
	tmp = tmp->u.e.r;
      }

      if (num > 0) {
	MALLOC (ids, int, num);
	if (e->u.fn.r && e->u.fn.r->type == E_GT) {
	  tmp = e->u.fn.r->u.e.r;
	}
	else {
	  tmp = e->u.fn.r;
	}
	for (ii=0; ii < num; ii++) {
	  ids[ii] = REC_CALL(tmp->u.e.l);
	  PRINT_STEP;
	  tmp = tmp->u.e.r;
	}
      }

      snprintf (buf+k, sz, "@%d <- ", b->i);
      PRINT_STEP;

      if (f->isUserMethod()) {
	int pos;
	const char *nm = f->getName();

	// if it is "int<>" or "struct<>struc
	{
	  int i;
	  for (i=0; nm[i]; i++) {
	    if (nm[i] == '/') break;
	  }
	  i++;
	  if (strcmp (nm + i, "int") == 0 ||
	      strcmp (nm + i, "int<>") == 0) {
	    // int(...)
	    snprintf (buf+k, sz, "int(");
	    PRINT_STEP;
	    sprint_uexpr (buf+k, sz, e->u.fn.r->u.e.l);
	    PRINT_STEP;
	    snprintf (buf+k, sz, ");");
	    PRINT_STEP;
	    return b->i;
	  }
	  else {
	    UserDef *parent = f->CurScope()->Parent()->getUserDef();
	    Assert (parent, "What?");

	    int cnt = 0;
	    while (nm[i + cnt] && nm[i + cnt] != '<') {
	      cnt++;
	    }
	    if (strncmp (nm + i, parent->getName(), cnt) == 0) {
	      is_special_struct = 1;
	    }
	  }
	}

	if (!is_special_struct) {
	  Assert (e->u.fn.r &&
		  e->u.fn.r->type == E_LT &&
		  e->u.fn.r->u.e.l->type == E_VAR, "What?");

	  ActId *id = (ActId *) e->u.fn.r->u.e.l->u.e.l;
	  id->sPrint (buf+k, sz);
	  pos = 0;
	  while (nm[pos] != '/') {
	    pos++;
	  }
	  pos++;
	  PRINT_STEP;
	  ADD_CHAR('.');
	  while (nm[pos] && nm[pos] != '<') {
	    ADD_CHAR (nm[pos]);
	    pos++;
	  }
	  ADD_CHAR('(');
	  tmp = e->u.fn.r->u.e.r;
	}
	else {
	  if (f->getns() && f->getns() != ActNamespace::Global()) {
	    char *s = f->getns()->Name ();
	    snprintf (buf+k, sz, "%s::", s);
	    PRINT_STEP;
	    FREE (s);
	  }
	  if (ActNamespace::Act()) {
	    ActNamespace::Act()->msnprintfproc (buf+k, sz, f->CurScope()->Parent()->getUserDef(), 1);
	  }
	  else {
	    snprintf (buf+k, sz, "%s", f->CurScope()->Parent()->getUserDef()->getName());
	  }
	  PRINT_STEP;
	  ADD_CHAR('(');
	  if (e->u.fn.r->type == E_GT) {
	    Assert (0, "This cannot happen!");
	  }
	  else {
	    sprint_uexpr (buf+k, sz, e->u.fn.r->u.e.l);
	  }
	  PRINT_STEP;
	  snprintf (buf+k, sz, ");");
	  return b->i;
	}
      }

      if (f->isUserMethod() && is_special_struct ||
	  !f->isUserMethod()) {
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
	  sprint_uexpr (buf+k, sz, tmp->u.e.l);
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

      }
      
      for (ii=0; ii < num; ii++) {
	snprintf (buf+k, sz, "@%d", ids[ii]);
	PRINT_STEP;
	if (ii != num-1) {
	  ADD_CHAR(',');
	}
      }
      snprintf (buf+k, sz, "); ");
      PRINT_STEP;
      if (num > 0) {
	FREE (ids);
      }
    }
    break;

  case E_ARRAY:
  case E_SUBRANGE:
  case E_ANDLOOP:
  case E_ORLOOP:
  case E_PLUSLOOP:
  case E_MULTLOOP:
  case E_XORLOOP:
  case E_TYPE:
  case E_USERMACRO:
    snprintf (buf+k, sz, "EType %d found!", e->type);
    PRINT_STEP;
    //fatal_error ("EType %d found after expansion", e->type);
    break;

  case E_SELF:
    EMIT_BASE (snprintf (buf+k, sz, "self"););
    break;

  case E_SELF_ACK:
    EMIT_BASE (snprintf (buf+k, sz, "selfack"););
    break;

  case E_BUILTIN_BOOL:
    {
      int n1 = REC_CALL(e->u.e.l);
      PRINT_STEP;
      snprintf (buf+k, sz, "@%d <- bool(@%d); ", b->i, n1);
      PRINT_STEP;
    }
    break;

  case E_BUILTIN_INT:
    {
      int n1 = REC_CALL (e->u.e.l);
      PRINT_STEP;
      snprintf (buf+k, sz, "@%d <- int(@%d", b->i, n1);
      PRINT_STEP;
      if (e->u.e.r) {
	snprintf (buf+k, sz, ",");
	PRINT_STEP;
	sprint_uexpr (buf+k, sz, e->u.e.r);
	PRINT_STEP;
      }
      snprintf (buf+k, sz, "); ");
      PRINT_STEP;
    }
    break;

  case E_BITFIELD:
    EMIT_BASE(
	      ((ActId *)e->u.e.l)->sPrint (buf+k, sz);
	      PRINT_STEP;
	      snprintf (buf+k, sz, "{");
	      PRINT_STEP;
	      if (e->u.e.r->u.e.l) {
		sprint_uexpr (buf+k, sz, e->u.e.r->u.e.r);
		PRINT_STEP;
		snprintf (buf+k, sz, "..");
		PRINT_STEP;
		sprint_uexpr (buf+k, sz, e->u.e.r->u.e.l);
	      }
	      else {
		sprint_uexpr (buf+k, sz, e->u.e.r->u.e.r);
	      }
	      PRINT_STEP;
	      snprintf (buf+k, sz, "}");
	      PRINT_STEP;
	      );
    break;

  case E_CONCAT:
    {
      int num = 0;
      int *ids;
      const Expr *tmp;
      int ii;
      tmp = e;
      while (tmp) {
	num++;
	tmp = tmp->u.e.r;
      }
      MALLOC (ids, int, num);
      tmp = e;
      for (ii=0; ii < num; ii++) {
	ids[ii] = REC_CALL (tmp->u.e.l);
	tmp = tmp->u.e.r;
      }
      EMIT_BASE(
	       snprintf (buf+k, sz, " {");
	       PRINT_STEP;
	       for (ii=0; ii < num; ii++) {
		 if (ii != 0) {
		   snprintf (buf+k, sz, ",");
		   PRINT_STEP;
		 }
		 snprintf (buf+k, sz, "@%d", ids[ii]);
		 PRINT_STEP;
	       }
	       snprintf (buf+k, sz, "}");
	       PRINT_STEP;
	       );
      FREE (ids);
    }
    break;

  case E_STRUCT_REF:
    {
      int n1 = REC_CALL (e->u.e.l);
      EMIT_BASE (
		 snprintf (buf+k, sz, "@%d.", n1);
		 PRINT_STEP;
		 sprint_uexpr (buf+k, sz, e->u.e.r);
		 PRINT_STEP;
		 );
    }
    break;

  default:
    fatal_error ("Unhandled case %d!\n", e->type);
    break;
  }
  return b->i;
}

void print_dag_expr (FILE *fp, const Expr *e)
{
  char *buf;
  int bufsz = 10240;
  int n;
  struct pHashtable *H;

  H = phash_new (8);

  fprintf (fp, "{{ ");
  MALLOC (buf, char, bufsz);
  while (1) {
    buf[0] = '\0';
    buf[bufsz-1] = '\0';
    buf[bufsz-2] = '\0';
    n = 0;
    n = _print_dag_expr (H, &n, buf, bufsz, e);
    if (buf[bufsz-2] == '\0') {
      fprintf (fp, "%s @%d }}", buf, n);
      FREE (buf);
      phash_free (H);
      return;
    }
    phash_clear (H);
    bufsz *= 2;
    REALLOC (buf, char, bufsz);
  }
}
