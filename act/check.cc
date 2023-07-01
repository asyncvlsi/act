/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2015-2019 Rajit Manohar
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
#include <stdarg.h>
#include <act/types.h>
#include <act/inst.h>
#include <act/value.h>
#include <common/int.h>
#include "act_parse_int.h"
#include "act_walk.extra.h"


/*
 *
 * Typechecking code
 *
 */

static char typecheck_errmsg[10240];

void typecheck_err (const char *s, ...)
{
  va_list ap;

  va_start (ap, s);
  vsnprintf (typecheck_errmsg, 10239,  s, ap);
  typecheck_errmsg[10239] = '\0';
  va_end (ap);
}

static void typecheck_errappend (const char *s, ...)
{
  va_list ap;
  int len = strlen (typecheck_errmsg);

  va_start (ap, s);
  vsnprintf (typecheck_errmsg + len, 10239-len,  s, ap);
  typecheck_errmsg[10239] = '\0';
  va_end (ap);
}



const char *act_type_errmsg (void)
{
  return typecheck_errmsg;
}

#if 0
static void dumpflags(int f)
{
  if (f == T_ERR) { printf ("[t-ERR]"); return; }
  printf ("[v=%x]", f);
  if (T_BASETYPE_INT (f)) {
    printf ("[t-int]");
  }
  else if (T_BASETYPE (f) == T_REAL) { 
    printf ("[t-real]");
  }
  else if (T_BASETYPE_BOOL (f)) {
    printf ("[t-bool]");
  }
  else if (f & T_DATA) {
    printf ("[t-struct]");
  }
  if (f & T_STRICT) {
    printf ("[t-strict]");
  }
  if (f & T_PARAM) {
    printf ("[t-param]");
  }
  if (f & T_ARRAYOF) {
    printf ("[t-array]");
  }
}
#endif

static InstType *_act_get_var_type (Scope *s, ActId *id, ActId **retid,
				    int *strict)
{
  InstType *it;
  UserDef *u;
  int is_strict;

  Assert (s, "Scope?");
  Assert (id, "Identifier?");

  if (id->isNamespace()) {
    s = id->getNamespace()->CurScope();
    id = id->Rest();
  }
  
  it = s->Lookup (id->getName());
  if (!it) {
    it = s->FullLookup (id->getName());
    is_strict = 1;
  }
  else {
    is_strict = 0;
  }
  Assert (it, "This should have been caught during parsing!");

  if (id->Rest ()) {
    while (id->Rest()) {
      u = dynamic_cast<UserDef *>(it->BaseType ());
      Assert (u, "This should have been caught during parsing!");
      id = id->Rest();
      it = u->Lookup (id);
    }
  }
  else {
    /*printf ("strict check: %s\n", id->getName ());*/
    /* this may be a strict port param */
    u = s->getUserDef();
    if (u && u->isStrictPort (id->getName ())) {
      is_strict = 1;
      /*printf ("this is strict [%s]!\n", id->getName ());*/
    }
  }

  Assert (!(id->isDeref () && !it->arrayInfo()), "Check during parsing!");
  if (strict) {
    *strict = is_strict;
  }
  *retid = id;
  return it;
}  

static int _act_type_id_to_flags (InstType *it, ActId *id, int is_strict)
{
  Type *t = it->BaseType ();
  unsigned int arr = 0;
  Data *d;

  if (it->arrayInfo() && !id->isDeref()) {
    arr = T_ARRAYOF;
  }
  if (is_strict) {
    is_strict = T_STRICT;
  }

  if (TypeFactory::isPIntType (t)) {
    return T_INT|T_PARAM|is_strict|arr;
  }
  if (TypeFactory::isPIntsType (t)) {
    return T_INT|T_PARAM|is_strict|arr;
  }
  if (TypeFactory::isPBoolType (t)) {
    return T_BOOL|T_PARAM|is_strict|arr;
  }
  if (TypeFactory::isPRealType (t)) {
    return T_REAL|T_PARAM|is_strict|arr;
  }
  if (TypeFactory::isPTypeType (t)) {
    return T_PTYPE|T_PARAM|arr;
  }
  if (TypeFactory::isIntType (t)) {
    return T_INT|arr;
  }
  if (TypeFactory::isBoolType (t)) {
    return T_BOOL|arr;
  }
  if (TypeFactory::isStructure (t)) {
    return T_DATA|arr;
  }
  if (!TypeFactory::isDataType (t)) {
    if (TypeFactory::isChanType (t)) {
      return T_CHAN|arr;
    }
    if (TypeFactory::isProcessType (t)) {
      return T_PROC|arr;
    }
    if (TypeFactory::isInterfaceType (t)) {
      char *tmpbuf;
      MALLOC (tmpbuf, char, 10240);
      id->sPrint (tmpbuf, 10240);
      typecheck_err ("Identifier `%s' is an interface type", tmpbuf);
      FREE (tmpbuf);
    }
    else {
      char *tmpbuf;
      MALLOC (tmpbuf, char, 10240);
      id->sPrint (tmpbuf, 10240);
      typecheck_err ("Identifier `%s' is not a data type", tmpbuf);
      FREE (tmpbuf);
    }
    return T_ERR;
  }
  d = dynamic_cast<Data *>(t);
  if (d->isPureEnum()) {
    return T_DATA_ENUM|arr;
  }
  if (d->isEnum()) {
    return T_INT|arr;
  }
  if (TypeFactory::boolType (t) == 1) {
    return T_DATA_BOOL|arr;
  }
  else if (TypeFactory::boolType (t) == 0) {
    return T_DATA_INT|arr;
  }
  else {
    Assert (TypeFactory::isStructure (t), "What?");
    return T_DATA|arr;
  }
}

int act_type_var (Scope *s, ActId *id, InstType **xit)
{
  InstType *it;
  int is_strict;

  it = _act_get_var_type (s, id, &id, &is_strict);

  if (!it->arrayInfo() && id->arrayInfo()) {
    char *tmpbuf;
    Array *tmpa;
    tmpa = id->arrayInfo();
    id->setArray (NULL);
    MALLOC (tmpbuf, char, 10240);
    id->sPrint (tmpbuf, 10240);
    id->setArray (tmpa);
    typecheck_err ("Identifier `%s' is not an array type", tmpbuf);
    FREE (tmpbuf);
    return T_ERR;
  }
  if (it->arrayInfo() && id->arrayInfo()) {
    if (it->arrayInfo()->nDims() != id->arrayInfo()->nDims()) {
      char *tmpbuf;
      Array *tmpa;
      tmpa = id->arrayInfo();
      id->setArray (NULL);
      MALLOC (tmpbuf, char, 10240);
      id->sPrint (tmpbuf, 10240);
      id->setArray (tmpa);
      typecheck_err ("Identifier `%s' has an invalid number of array dimensions (expected %d v/s %d)", tmpbuf, it->arrayInfo()->nDims(), id->arrayInfo()->nDims());
      FREE (tmpbuf);
      return T_ERR;
    }
  }

  if (xit) {
    *xit = it;
  }
  return _act_type_id_to_flags (it, id, is_strict);
}


static InstType *_act_special_expr_insttype (Scope *s, Expr *e, int *islocal)
{
  InstType *it;
  
  if (e->type == E_VAR) {
    /* special case #1 */
    it = act_actual_insttype (s, (ActId *)e->u.e.l, islocal);
    return it;
  }
  else if (e->type == E_FUNCTION) {
    /* special case */
    UserDef *u = (UserDef *) e->u.fn.s;
    Assert (TypeFactory::isFuncType (u), "Hmm.");
    Function *fn = dynamic_cast<Function *>(u);
    return fn->getRetType();
  }
  else if (e->type == E_SELF) {
    /* special case */
    return s->Lookup ("self");
  }
  else if (e->type == E_ENUM_CONST) {
    /* special case */
    Data *d = (Data *) e->u.fn.s;
    d = d->Expand (d->getns(), d->getns()->CurScope(), 0, NULL);
    it = new InstType (d->getns()->CurScope(), d);
    return it;
  }
  return NULL;
}


/**
 *  Typecheck expression
 *
 *  @param s is the scope in which all identifiers are evaluated
 *  @param e is the expression to be typechecked
 *  @param width is set to the bit-width of the expression
 *  @param only_chan 0 if this is a normal expression, 
 *                   1 if this can only have channels,
 *                   2 if this can mix channels and variables
 *
 *  @return -1 on an error
 *
 *
 */
int act_type_expr (Scope *s, Expr *e, int *width, int only_chan)
{
  int lt, rt;
  int lw, rw;
  int flgs;
  if (!e) return T_ERR;

  /*  
  printf ("check: %s\n", expr_op_name (e->type));
  printf (" lt: %x  rt: %x\n", lt, rt);      
  */
#include "expr_width.h"  

  
#define EQUAL_LT_RT2(f,g,mode)						\
  do {									\
    lt = act_type_expr (s, e->u.e.l, &lw, only_chan);			\
    if (lt == T_ERR) return T_ERR;					\
    rt = act_type_expr (s, e->u.e.r, &rw, only_chan);			\
    if (rt == T_ERR) return T_ERR;					\
    if ((lt & T_ARRAYOF) || (rt & T_ARRAYOF)) {				\
      typecheck_err ("`%s': operator applied to array argument", expr_op_name (e->type)); \
      return T_ERR;							\
    }									\
    flgs = lt & rt & (T_PARAM|T_STRICT);				\
    if ((f & (T_BOOL|T_DATA_BOOL)) && (T_FIXBASETYPE (lt) == T_BOOL) && (T_FIXBASETYPE (rt) == T_BOOL)) { \
      return (((f) != (g) ? (g) : T_BOOL) & ~(T_PARAM|T_STRICT))|flgs;  \
    }                                                                   \
    if ((f & T_REAL) && T_BASETYPE_ISNUM(lt) && T_BASETYPE_ISNUM(rt)) { \
      if (T_BASETYPE(lt) == T_REAL || T_BASETYPE(rt) == T_REAL) {       \
        return (((f) != (g) ? (g) : T_REAL) & ~(T_PARAM|T_STRICT))|flgs; \
      }                                                                 \
      WIDTH_UPDATE(mode);						\
      return (((f) != (g) ? (g) : T_INT) & ~(T_PARAM|T_STRICT))|flgs;   \
    }                                                                   \
    if ((f & (T_INT|T_DATA_INT)) && (T_FIXBASETYPE(lt) == T_INT) && (T_FIXBASETYPE(rt) == T_INT)) { \
      WIDTH_UPDATE(mode);						\
      return (((f) != (g) ? (g) : T_INT) & ~(T_PARAM|T_STRICT))|flgs;   \
    }                                                                   \
  } while (0)

#define EQUAL_LT_RT(f,mode)	 EQUAL_LT_RT2((f),lt,mode)

#define INT_OR_REAL(mode)						\
  do {									\
    lt = act_type_expr (s, e->u.e.l, &lw, only_chan);			\
    if (lt == T_ERR) return T_ERR;					\
    rt = act_type_expr (s, e->u.e.r, &rw, only_chan);			\
    if (rt == T_ERR) return T_ERR;					\
    if ((lt & T_ARRAYOF) || (rt & T_ARRAYOF)) {				\
      typecheck_err ("`%s': operator applied to array argument", expr_op_name (e->type)); \
      return T_ERR;							\
    }									\
    flgs = lt & rt & (T_PARAM|T_STRICT);				\
    if (T_BASETYPE_ISNUM(lt) && T_BASETYPE_ISNUM(rt)) {			\
      if (T_BASETYPE (lt) == T_REAL || T_BASETYPE (rt) == T_REAL) {	\
	return T_REAL|flgs;						\
      }									\
      else {								\
	WIDTH_UPDATE(mode);						\
	return T_INT|flgs;						\
      }									\
    }									\
  } while (0)
  
  switch (e->type) {
    /* Boolean, binary or int */
  case E_AND:
  case E_OR:
    EQUAL_LT_RT(T_BOOL|T_INT|T_DATA_BOOL|T_DATA_INT, WIDTH_MAX);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs bool/bool or int/int", expr_op_name (e->type));
    return T_ERR;
    break;

  case E_ANDLOOP:
  case E_ORLOOP:
    lt = act_type_expr (s, e->u.e.r->u.e.l, &lw, 0);
    if (lt == T_ERR) return T_ERR;
    if (!T_BASETYPE_INT (lt) || (lt & T_ARRAYOF)) {
      typecheck_err ("Loop range is not of integer type");
      return T_ERR;
    }
    if (e->u.e.r->u.e.r->u.e.l) {
      lt = act_type_expr (s, e->u.e.r->u.e.r->u.e.l, &lw, 0);
      if (lt == T_ERR) return T_ERR;
      if (!T_BASETYPE_INT (lt) || (lt & T_ARRAYOF)) {
	typecheck_err ("Loop range is not of integer type");
	return T_ERR;
      }
    }
    lt = act_type_expr (s, e->u.e.r->u.e.r->u.e.r, &lw, only_chan);
    if (lt == T_ERR) return T_ERR;
    if (!T_BASETYPE_BOOL (lt) || (lt & T_ARRAYOF)) {
      typecheck_err ("Loop body is not of bool type");
      return T_ERR;
    }
    if (width) {
      *width = 1;
    }
    return T_BOOL | (lt & T_PARAM);
    break;

    /* Boolean, unary */
  case E_NOT:
    lt = act_type_expr (s, e->u.e.l, width, only_chan);
    if (lt == T_ERR) return T_ERR;
    if (lt & T_BOOL) {
      return lt;
    }
    typecheck_err ("`~' operator applied to a non-Boolean");
    return T_ERR;
    break;

    /* Binary, real or integer */
  case E_PLUS:
  case E_MINUS:
    INT_OR_REAL(WIDTH_MAX1);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments for add/sub; needs int/real for both", expr_op_name (e->type));
    return T_ERR;
    break;

  case E_MULT:
    INT_OR_REAL(WIDTH_SUM);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments for mult; needs int/real for both", expr_op_name (e->type));
    return T_ERR;
    break;
    
  case E_DIV:
    INT_OR_REAL(WIDTH_LEFT);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments for div; needs int/real for both", expr_op_name (e->type));
    return T_ERR;
    break;

    
  case E_MOD:
    INT_OR_REAL(WIDTH_RIGHT);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments for mod; needs int/real for both", expr_op_name (e->type));
    return T_ERR;
    break;

  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
    EQUAL_LT_RT2(T_REAL,T_BOOL, WIDTH_BOOL);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int or real/real", expr_op_name (e->type));
    return T_ERR;

    /* Unary, real or integer */
  case E_UMINUS:
    lt = act_type_expr (s, e->u.e.l, width, only_chan);
    if (lt == T_ERR) return T_ERR;
    if (T_BASETYPE_ISNUM (lt)) {
      return lt;
    }
    typecheck_err ("`!' operator applied to a Boolean");
    return T_ERR;
    break;

    /* Unary, integer or bool */
  case E_COMPLEMENT:
    lt = act_type_expr (s, e->u.e.l, width, only_chan);
    if (lt == T_ERR) return lt;
    if (T_BASETYPE_ISINTBOOL (lt)) {
      return lt;
    }
    typecheck_err ("`~' operator needs an int or bool");
    return T_ERR;

    /* Binary, integer only */
  case E_LSL:
    EQUAL_LT_RT(T_INT|T_DATA_INT, WIDTH_LSHIFT);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int", expr_op_name (e->type));

  case E_LSR:
  case E_ASR:
    EQUAL_LT_RT(T_INT|T_DATA_INT, WIDTH_LEFT);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int", expr_op_name (e->type));

  case E_XOR:
    EQUAL_LT_RT(T_INT, WIDTH_MAX);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int", expr_op_name (e->type));
    return T_ERR;

  case E_QUERY:
    lt = act_type_expr (s, e->u.e.l, &lw, only_chan);
    if (lt == T_ERR) return T_ERR;
    if (T_BASETYPE_BOOL(lt) && !( lt & T_ARRAYOF )) {
      char buf1[4000], buf2[4000];
      e = e->u.e.r;
      EQUAL_LT_RT(T_BOOL|T_REAL, WIDTH_MAX);

      sprint_uexpr (buf1, 4000, e->u.e.l);
      sprint_uexpr (buf2, 4000, e->u.e.r);
      
      typecheck_err ("Query expression: two options must have compatible types\n\tOpt1: %s\n\tOpt2: %s", buf1, buf2);
    }
    else {
      typecheck_err ("Query expr requires a boolean type");
    }
    return T_ERR;
    break;
    
  case E_COLON:
    fatal_error ("FIXME");
    break;

    /* binary, Integer or Bool or real */
  case E_EQ:
  case E_NE:
    EQUAL_LT_RT2(T_BOOL|T_REAL,T_BOOL, WIDTH_BOOL);
    /* special case for enumerations */
    {
      InstType *it1, *it2;
      it1 = _act_special_expr_insttype (s, e->u.e.l, NULL);
      if (it1) {
	it2 = _act_special_expr_insttype (s, e->u.e.r, NULL);
	if (it2) {
	  if (it1->isEqual (it2)) {
	    if (width) {
	      *width =1;
	    }
	    return T_BOOL;
	  }
	}
      }
    }
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs real/real, int/int, or bool/bool", expr_op_name (e->type));
    return T_ERR;
    break;

  case E_CONCAT:
    rw = 0;
    do {
      lt = act_type_expr (s, e->u.e.l, &lw, only_chan);
      if (lt == T_ERR) return T_ERR;
      if (!T_BASETYPE_ISINTBOOL (lt)) {
	typecheck_err ("{ } concat: all items must be int or bool");
      }
      e = e->u.e.r;
      rw += lw;
    } while (e);
    if (width) {
      *width = rw;
    }
    return T_INT;
    break;

  case E_BITFIELD:
    {
      InstType *xit;
      long lo, hi;
      lt = act_type_var (s, (ActId *)e->u.e.l, &xit);
      if (lt == T_ERR) return T_ERR;
      if (only_chan == 1 || (only_chan == 2 && !(T_BASETYPE_INT (lt)))) {
	if (TypeFactory::isChanType (xit)) {
	  if (xit->isExpanded()) {
	    if (xit->getDir() == Type::OUT) {
	      typecheck_err ("Channel expression requires an input port");
	      return T_ERR;
	    }
	    
	    if (!xit->arrayInfo() && ((ActId *)e->u.e.l)->Tail()->arrayInfo()) {
	      typecheck_err ("Identifier is not an array type");
	      return T_ERR;
	    }
	    if (xit->arrayInfo() && !((ActId *)e->u.e.l)->isDeref()) {
	      typecheck_err ("Array specifier not permitted in channel expression");
	      return T_ERR;
	    }
	    if (!TypeFactory::isBaseIntType (TypeFactory::getChanDataType (xit))) {
	      typecheck_err ("Bitfields permitted only on integer data values");
	      return T_ERR;
	    }
	    Assert (e->u.e.r, "What?");
	    Assert (e->u.e.r->u.e.r, "What?");
	    if (e->u.e.r->u.e.l && (!expr_is_a_const (e->u.e.r->u.e.l) ||
				    e->u.e.r->u.e.l->type != E_INT)) {
	      typecheck_err ("Bitfield can only use const integer arguments");
	      return T_ERR;
	    }
	    if (!expr_is_a_const (e->u.e.r->u.e.r) ||
		e->u.e.r->u.e.r->type != E_INT) {
	      typecheck_err ("Bitfield can only use const integer arguments");
	      return T_ERR;
	    }
	    hi = e->u.e.r->u.e.r->u.ival.v;
	    if (e->u.e.r->u.e.l) {
	      lo = e->u.e.r->u.e.l->u.ival.v;
	    }
	    else {
	      lo = hi;
	    }
	    if (hi < lo) {
	      typecheck_err ("Bitfield range is empty {%d..%d}", hi, lo);
	      return T_ERR;
	    }
	    if ((TypeFactory::bitWidth (xit) >= 0 && hi+1 > TypeFactory::bitWidth (xit)) || lo < 0) {
	      typecheck_err ("Bitfield range {%d..%d} is wider than operand (%d)",
			     hi, lo, TypeFactory::bitWidth (xit));
	      return T_ERR;
	    }
	    if (width) {
	      *width = hi - lo + 1;
	    }
	  }
	  else {
	    if (width) {
	      *width = 32;
	    }
	  }
	  return T_INT;
	}
      }
      else {
	if (T_BASETYPE_INT (lt)) {
	  if (xit->isExpanded()) {
	    Assert (e->u.e.r, "What?");
	    Assert (e->u.e.r->u.e.r, "What?");
	    if (e->u.e.r->u.e.l && (!expr_is_a_const (e->u.e.r->u.e.l) ||
				    e->u.e.r->u.e.l->type != E_INT)) {
	      typecheck_err ("Bitfield can only use const integer arguments");
	      return T_ERR;
	    }
	    if (!expr_is_a_const (e->u.e.r->u.e.r) ||
		e->u.e.r->u.e.r->type != E_INT) {
	      typecheck_err ("Bitfield can only use const integer arguments");
	      return T_ERR;
	    }
	    hi = e->u.e.r->u.e.r->u.ival.v;
	    if (e->u.e.r->u.e.l) {
	      lo = e->u.e.r->u.e.l->u.ival.v;
	    }
	    else {
	      lo = hi;
	    }
	    if (hi < lo) {
	      typecheck_err ("Bitfield range is empty {%d..%d}", hi, lo);
	      return T_ERR;
	    }
	    if ((TypeFactory::bitWidth (xit) >= 0 && hi+1 > TypeFactory::bitWidth (xit)) || lo < 0) {
	      typecheck_err ("Bitfield range {%d..%d} is wider than operand (%d)",
			     hi, lo, TypeFactory::bitWidth (xit));
	      return T_ERR;
	    }
	    if (width) {
	      *width = hi - lo + 1;
	    }
	  }
	  else {
	    if (width) {
	      *width = 32;
	    }
	  }
	  return lt;
	}
      }
    }
    typecheck_err ("Bitfield used with non-integer variable.");
    return T_ERR;
    break;

  case E_BUILTIN_BOOL:
    lt = act_type_expr (s, e->u.e.l, width, only_chan);
    if (lt == T_ERR) return T_ERR;
    if (lt & (T_BOOL|T_ARRAYOF|T_REAL)) {
      typecheck_err ("bool(.) requires an integer argument");
      return T_ERR;
    }
    if (lt & T_INT) {
      return (lt & ~T_INT) | T_BOOL;
    }
    typecheck_err ("Invalid use of bool(.)");
    return T_ERR;
    break;
    
  case E_BUILTIN_INT:
    lt = act_type_expr (s, e->u.e.l, width, only_chan);
    if (lt == T_ERR)  return T_ERR;
    if (lt & T_ARRAYOF) {
      typecheck_err ("int(.) can't accept array arguments");
      return T_ERR;
    }
    if ((lt & T_REAL) && (!(lt & T_PARAM) || e->u.e.r)) {
      typecheck_err ("int(.) can't accept real arguments");
      return T_ERR;
    }
    if (T_BASETYPE_BOOL (lt)) {
      if (e->u.e.r) {
	typecheck_err ("int(.) with a Boolean argument doesn't accept second arg");
	return T_ERR;
      }
      if (width) {
	*width = 1;
      }
      return (lt & ~(T_BOOL|T_DATA_BOOL))|T_INT;
    }
    if (lt & T_REAL) {
      if (width) {
	*width = 64;
      }
      return T_INT|T_PARAM;
    }
    if (T_BASETYPE_INT (lt)) {
      if (!e->u.e.r) {
	typecheck_err ("int(.) with int argument requires width argument:");
	return T_ERR;
      }
      rt = act_type_expr (s, e->u.e.r, NULL, only_chan);
      if (rt == T_ERR)  return T_ERR;
      if (!T_BASETYPE_INT(rt) || !(rt & T_PARAM) || (rt & T_ARRAYOF)) {
	typecheck_err ("int(.): second argument has to be an int parameter");
	return T_ERR;
      }
      if (expr_is_a_const (e->u.e.r)) {
	if (width) {
	  *width = e->u.e.r->u.ival.v;
	}
      }
      else {
	if (width) {
	  *width = 32;
	}
      }
      return lt;
    }
    typecheck_err ("int(.) takes only an int or bool argument");
    return T_ERR;
    break;
    
    /* UMMMM */
  case E_FUNCTION:
    /* typecheck all arguments; then return result type */
    {
      unsigned int ret = 0;
      UserDef *u = (UserDef *) e->u.fn.s;
      Assert (TypeFactory::isFuncType (u), "Hmm.");
      Function *fn = dynamic_cast<Function *>(u);
      InstType *rtype = fn->getRetType();
      int kind = 0;
      Expr *tmp = e->u.fn.r;
      Expr *e2;
      int strict_flag = T_STRICT;

      if (TypeFactory::isParamType (rtype)) {
	kind = 0;
      }
      else {
	kind = 1;
      }

      if (tmp && tmp->type == E_GT) {
	e2 = tmp->u.e.l;
	tmp = tmp->u.e.r;
      }
      else {
	e2 = NULL;
      }

      for (int i=0;
	   i < (kind == 0 ? fn->getNumParams() : fn->getNumPorts()); i++) {
	InstType *x = fn->getPortType (kind == 0 ? -(i+1) : i);
	InstType *y = act_expr_insttype (s, tmp->u.e.l, NULL, only_chan);

	if (!y) {
	  return T_ERR;
	}

	strict_flag &= act_type_expr (s, tmp->u.e.l, NULL, only_chan);

	if (only_chan) {
	  if (only_chan == 1) {
	    if (TypeFactory::isChanType (y)) {
	      y = TypeFactory::getChanDataType (y);
	    }
	    else if (tmp->u.e.l->type == E_VAR ||
		     !TypeFactory::isDataType (y)) {
	      typecheck_err ("Function `%s': arg #%d has an incompatible type",
			     fn->getName(), i);
	      return T_ERR;
	    }
	  }
	  else {
	    if (TypeFactory::isChanType (y)) {
	      y = TypeFactory::getChanDataType (y);
	    }
	  }
	}

	if (!x->isConnectable (y, 1)) {
	  if ((TypeFactory::isIntType (x) && TypeFactory::isPIntType (y))
	      ||
	      (TypeFactory::isBoolType (x) && TypeFactory::isPBoolType (y))) {
	    /* ok */
	  }
	  else {
	    typecheck_err ("Function `%s': arg #%d has an incompatible type",
			   fn->getName(), i);
	    return T_ERR;
	  }
	}
	tmp = tmp->u.e.r;
      }

      if (e2) {
	Assert (kind == 1, "What?");
	tmp = e2;
	for (int i=0; i < fn->getNumParams(); i++) {
	  InstType *x = fn->getPortType (-(i+1));
	  InstType *y = act_expr_insttype (s, tmp->u.e.l, NULL, only_chan);
	  strict_flag &= act_type_expr (s, tmp->u.e.l, NULL, 0);

	  if (!x->isConnectable (y, 1)) {
	    typecheck_err ("Function `%s': template arg #%d has an incompatible type",
			   fn->getName(), i);
	    return T_ERR;
	  }
	  tmp = tmp->u.e.r;
	}
      }
      /*-- provide return type --*/
      
      if (TypeFactory::isParamType(rtype)) {
	ret |= T_PARAM|strict_flag;
      }
      if (fn->getRetType()->arrayInfo()) {
	ret |= T_ARRAYOF;
      }
      if (ret & T_PARAM) {
	if (TypeFactory::isPIntType (rtype)) {
	  ret |= T_INT;
	}
	else if (TypeFactory::isPBoolType (rtype)) {
	  ret |= T_BOOL;
	}
	else if (TypeFactory::isPRealType (rtype)) {
	  ret |= T_REAL;
	}
	else {
	  Assert (0, "Unknown return type");
	}
      }
      else {
	if (TypeFactory::isIntType (rtype)) {
	  ret |= T_INT;
	  if (width) {
	    *width = TypeFactory::bitWidth (rtype);
	  }
	}
	else if (TypeFactory::isBoolType (rtype)) {
	  ret |= T_BOOL;
	  if (width) {
	    *width = 1;
	  }
	}
	else if (TypeFactory::isStructure (rtype)) {
	  ret |= T_DATA;
	  if (width) {
	    *width = -1;
	  }
	}
	else if (TypeFactory::isUserPureEnum (rtype)) {
	  ret |= T_DATA_ENUM;
	  if (width) {
	    *width = TypeFactory::bitWidth (rtype);
	  }
	}
	else if (TypeFactory::isUserEnum (rtype)) {
	  ret |= T_INT;
	  if (width) {
	    *width = TypeFactory::bitWidth (rtype);
	  }
	}
	else {
	  Assert (0, "Unknown return type");
	}
      }
      return ret;
    }
    break;

    /* leaves */
  case E_VAR:
    {
      InstType *xit;
      lt = act_type_var (s, (ActId *)e->u.e.l, &xit);
      if (lt == T_ERR) return T_ERR;
      if (only_chan) {
	if (lt == T_CHAN) {
	  ActId *tmp;
	  InstType *it = _act_get_var_type (s, (ActId *)e->u.e.l, &tmp, NULL);

	  if (it->getDir() == Type::OUT) {
	    typecheck_err ("Channel expression requires an input port");
	    return T_ERR;
	  }

	  if (!it->arrayInfo() && tmp->arrayInfo()) {
	    typecheck_err ("Identifier `%s' is not an array type", tmp->getName());
	    return T_ERR;
	  }

	  if (it->arrayInfo() && !tmp->isDeref()) {
	    typecheck_err ("Array specifier not permitted in channel expression");
	    return T_ERR;
	  }

	  if (TypeFactory::boolType (it) == 1) {
	    if (width) {
	      *width = 1;
	    }
	    return T_BOOL;
	  }
	  else {
	    if (width) {
	      if (xit->isExpanded()) {
		*width = TypeFactory::bitWidth (xit);
	      }
	      else {
		*width = 32;
	      }
	    }
	    return T_INT;
	  }
	}
	if (((lt & (T_INT|T_PARAM)) == (T_INT|T_PARAM)) && !(lt & T_ARRAYOF)) {
	  if (width) {
	    *width = 64;
	  }
	  return lt;
	}
	if (only_chan == 1) {
	  typecheck_err ("Channel expressions can't have non-channel variables");
	  return T_ERR;
	}
      }
      if (width) {
	if (xit->isExpanded()) {
	  *width = TypeFactory::bitWidth (xit);
	}
	else {
	  *width = 32;
	}
      }
      return lt;
    }
    break;

  case E_PROBE:
    if (only_chan == 1) {
      typecheck_err ("Probe not permitted in pure channel expression");
      return T_ERR;
    }
    lt = act_type_var (s, (ActId *)e->u.e.l, NULL);
    if (lt == T_ERR) return T_ERR;
    if (lt == T_CHAN) {
      if (width) {
	*width = 1;
      }
      return T_BOOL;
    }
    else {
      typecheck_err ("Probe operator applied to a non-channel identifier");
      return T_ERR;
    }
    break;

  case E_REAL:
    return (T_REAL|T_PARAM|T_STRICT);
    break;

  case E_INT:
    if (width) {
      int w = 0;
      unsigned long val;

      if (e->u.ival.v_extra) {
	BigInt *b = (BigInt *) e->u.ival.v_extra;
	*width = b->getWidth ();
      }
      else {
	if ((long)e->u.ival.v < 0) {
	  val = -((long)e->u.ival.v);
	}
	else {
	  val = e->u.ival.v;
	}
	while (val) {
	  w++;
	  val = val >> 1;
	}
	*width = (w+1);
      }
    }
    return (T_INT|T_PARAM|T_STRICT);
    break;

  case E_TRUE:
  case E_FALSE:
    if (width) {
      *width = 1;
    }
    return (T_BOOL|T_PARAM|T_STRICT);
    break;

  case E_SELF:
  case E_SELF_ACK:
    {
      ActId *tmpid;
      if (e->type == E_SELF) {
	tmpid = new ActId ("self");
      }
      else {
	tmpid = new ActId ("selfack");
      }
      InstType *xit;
      lt = act_type_var (s, tmpid, &xit);
      delete tmpid;
      if (lt == T_ERR) return T_ERR;
      if (width) {
	if (xit->isExpanded()) {
	  *width = TypeFactory::bitWidth (xit);
	}
	else {
	  if (T_BASETYPE_BOOL(lt)) {
	    *width = 1;
	  }
	  else {
	    *width = 32;
	  }
	}
      }
    }
    return lt;
    break;

  case E_TYPE:
    return T_PTYPE|T_PARAM|T_STRICT;
    break;
    
  case E_ARRAY:
  case E_SUBRANGE:
    {
      ValueIdx *vx = (ValueIdx *) e->u.e.l;
      int r;
      if (TypeFactory::isPIntType (vx->t) ||
	  TypeFactory::isPIntsType (vx->t)) {
	r = T_INT;
      }
      else if (TypeFactory::isPRealType (vx->t)) {
	r = T_REAL;
      }
      else if (TypeFactory::isPBoolType (vx->t)) {
	r = T_BOOL;
      }
      else {
	Assert (0, "Hmm");
      }
      return r|T_ARRAYOF;
    }

  case E_ENUM_CONST:
    {
      Data *d = (Data *) e->u.fn.s;
      if (d->isPureEnum()) {
	if (width) {
	  *width = -1;
	}
	return T_DATA_ENUM;
      }
      else {
	if (width) {
	  *width = TypeFactory::bitWidth (d);
	}
	return T_INT;
      }
    }
    break;
    
  default:
    fatal_error ("Unknown type!");
    typecheck_err ("`%d' is an unknown type for an expression", e->type);
    return T_ERR;
  }
  typecheck_err ("Should not be here");
  return T_ERR;
#undef EQUAL_LT_RT
}


/*
  Returns the insttype for the ID. 
  If islocal != NULL, set to 1 if this is a local id within the scope
*/
InstType *act_actual_insttype (Scope *s, ActId *id, int *islocal)
{
  InstType *it;

  if (id->isNamespace()) {
    s = id->getNamespace()->CurScope();
    id = id->Rest();
  }

  if (islocal) {
    *islocal = 1;
    if (s->Lookup (id->getName())) {
      UserDef *ux;
      ux = s->getUserDef ();
      if (ux && ux->isPort (id->getName())) {
	*islocal = 0;
      }
    }
    else {
      *islocal = 0;
    }
  }

  it = s->FullLookup (id->getName());
  Assert (it, "This should have been caught earlier!");

  while (id->Rest()) {
      /* this had better be an array deref if there is a array'ed type
	 involved */
      if (it->arrayInfo()) {
	if (!id->arrayInfo()) {
	  typecheck_err ("Port `%s.%s' access for an arrayed type is missing an array dereference", id->getName (), id->Rest()->getName());
	  return NULL;
	}
	if (it->arrayInfo()->nDims () != id->arrayInfo()->nDims ()) {
	  typecheck_err ("Port `%s': number of dimensions don't match type (%d v/s %d)", id->getName(), id->arrayInfo()->nDims(), it->arrayInfo()->nDims ());
	  return NULL;
	}
	/* for expanded types, we need to check that this is a
	   legitimate dereference! */
	if (!id->arrayInfo()->isDeref()) {
	  typecheck_err ("`.' applied to a subrange specifier");
	  return NULL;
	}
	if (s->isExpanded()) {
	  Assert (it->isExpanded(), "What on earth?");
	  Assert (id->arrayInfo()->isExpanded(), "What on earth2?");

	  if (!it->arrayInfo()->Validate (id->arrayInfo())) {
	    typecheck_err ("Array dereference out of range");
	    return NULL;
	  }
	}
      }
      else {
	if (id->arrayInfo()) {
	  typecheck_err ("Array de-reference for a non-arrayed type, port `%s'", id->getName ());
	  return NULL;
	}
      }
      UserDef *u;
      u = dynamic_cast<UserDef *>(it->BaseType ());
      Assert (u, "This should have been caught during parsing!");

      /* HERE: APPLY MAP! */
      if (it->getIfaceType()) {
	/* extract the real type */
	InstType *itmp = it->getIfaceType();
	Assert (itmp, "What?");
	Process *proc = dynamic_cast<Process *> (it->BaseType());
	Assert (proc, "What?");
	list_t *map = proc->findMap (itmp);
	if (!map) {
	  fatal_error ("Missing interface `%s' from process `%s'?",
		       itmp->BaseType()->getName(), proc->getName());
	}
	listitem_t *li;
#if 0
	printf ("ID rest: ");
	id->Rest()->Print(stdout);
	printf ("\n");
#endif	
	for (li = list_first (map); li; li = list_next (li)) {
	  char *from = (char *)list_value (li);
	  char *to = (char *)list_value (list_next(li));
	  if (strcmp (id->Rest()->getName(), from) == 0) {
	    ActId *nid = new ActId (to);
	    if (id->Rest()->arrayInfo()) {
	      nid->setArray (id->Rest()->arrayInfo());
	    }
	    /* XXX: ID CACHING */
	    nid->Append (id->Rest()->Rest());
	    id = nid;
	    break;
	  }
	  li = list_next (li);
	}
	if (!li) {
	  fatal_error ("Map for interface `%s' doesn't contain `%s'",
		       itmp->BaseType()->getName(), id->Rest()->getName());
	}
#if 0	
	printf ("NEW ID rest: ");
	id->Print(stdout);
	printf ("\n");
#endif	
      }
      else {
	id = id->Rest ();
      }
      it = u->Lookup (id);
  }
  /* we have insttype, and id: now handle any derefs */
  if (!id->arrayInfo ()) {
    /* easy case */
    return it;
  }
  if (!it->arrayInfo()) {
    typecheck_err ("Array de-reference for a non-arrayed type, port `%s'", id->getName ());
    return NULL;
  }
  
  if (it->arrayInfo ()->nDims () != id->arrayInfo()->nDims ()) {
    typecheck_err ("Port `%s': number of dimensions don't match type (%d v/s %d)", id->getName(), id->arrayInfo()->nDims (), it->arrayInfo()->nDims ());
    return NULL;
  }

  /* type is an array, there is a deref specified here */
  /* XXX: currently two cases handled correctly
       subrange specifier, full dimensions, return original type
       OR
       full deref
  */
  if (s->isExpanded()) {
    if (id->arrayInfo()->isDeref()) {
      InstType *it2 = new InstType (it, 1);
      it = it2;
    }
    else {
      /* subrange */
      InstType *it2 = new InstType (it, 1);
      Array *aret, *ida;

      ida = id->arrayInfo();
      aret = NULL;
      for (int i=0; i < ida->nDims(); i++) {
	if (ida->isrange (i)) {
	  Array *tmp = new Array (0, ida->range_size (i)-1);
	  tmp->mkArray ();
	  if (!aret) {
	    aret = tmp;
	  }
	  else {
	    aret->Concat (tmp);
	    delete tmp;
	  }
	}
      }
      Assert (aret, "Huh?");
      it2->MkArray (aret);
      it = it2;
    }
  }
  else {
    /* not expanded */
    if (id->arrayInfo()->effDims() == id->arrayInfo()->nDims()) {
      InstType *it2 = new InstType (it, 1);
      it2->MkArray (id->arrayInfo()->Clone ());
      it = it2;
    }
    else if (id->arrayInfo()->effDims() == 0) {
      InstType *it2 = new InstType (it, 1);
      /* no array! */
      it = it2;
    }
    else {
      InstType *it2 = new InstType (it, 1);
      Array *a = NULL, *ida;

      ida = id->arrayInfo();
      for (int i=0; i < ida->nDims(); i++) {
	Array *tmp;
	if (ida->lo(i) == NULL) continue;

	tmp = new Array (expr_dup (ida->lo(i)), expr_dup (ida->hi(i)));;
	tmp->mkArray ();
	if (!a) {
	  a = tmp;
	}
	else {
	  a->Concat (tmp);
	}
      }
      it2->MkArray (a);
      it = it2;
    }
  }
  return it;
}

InstType *act_expr_insttype (Scope *s, Expr *e, int *islocal, int only_chan)
{
  int ret;
  InstType *it;

  if (islocal) {
    *islocal = 1;
  }

  if (!e) {
    typecheck_err ("NULL expression??");
    return NULL;
  }

  /* 
     The next four cases are the only valid expressions for
      - structures
      - pure enumerations
  */
  it = _act_special_expr_insttype (s, e, islocal);
  if (it) {
    return it;
  }

  /* analyze the expression tree */
  ret = act_type_expr (s, e, NULL, only_chan);

  if (ret == T_ERR) {
    return NULL;
  }
  ret &= ~T_STRICT;
  if (ret == (T_INT|T_PARAM)) {
    return TypeFactory::Factory()->NewPInt();
  }
  else if (ret == T_INT) {
    return TypeFactory::Factory()->NewInt (s, Type::NONE, 0, const_expr (32));
  }
  else if (ret == (T_BOOL|T_PARAM)) {
    return TypeFactory::Factory()->NewPBool ();
  }
  else if (ret == T_BOOL) {
    return TypeFactory::Factory()->NewBool (Type::NONE);
  }
  else if (ret == (T_REAL|T_PARAM)) {
    return TypeFactory::Factory()->NewPReal ();
  }
  else if (ret == (T_PTYPE|T_PARAM)) {
    Assert (e->type == E_TYPE, "What?");
    return (InstType *)e->u.e.l;
  }
  else if (ret & T_ARRAYOF) {
    if (e->type == E_ARRAY) {
      return ((ValueIdx *)e->u.e.l)->t;
    }
    else if (e->type == E_SUBRANGE) {
      ValueIdx *vx = (ValueIdx *)e->u.e.l;
      InstType *it = new InstType (vx->t, 1);
      //it->MkArray (((Array *)e->u.e.r->u.e.r)->Clone());
      it->MkArray (((Array *)e->u.e.r->u.e.r)->Reduce());
      return it;
    }
    else {
      // error
      return NULL;
    }
  }
  else {
    fatal_error ("Not sure what to do now");
  }
  return NULL;
}

static int stype_line_no, stype_col_no;
static char *stype_file_name;

void type_set_position (int l, int c, char *n)
{
  stype_line_no = l;
  stype_col_no = c;
  stype_file_name = n;
}

static void type_errctxt (int expanded, const char *msg)
{
  if (expanded) {
    act_error_ctxt (stderr);
  }
  else {
    act_position p;
    p.l = stype_line_no;
    p.c = stype_col_no;
    p.f = stype_file_name;
    act_parse_msg (&p, msg);
  }
}

/* 
   are these two types compatible? connectable?

   skip_last_array = 1 : for array dimensions, skip the last dimension
   check. Check that # of dimensions are equal, and for expanded
   types, check all dims are equal except for the first one.


   Return value:
     1 = they are compatible
     0 = they are not compatible
     2 = they are compatible, lhs is more specific
     3 = they are compatible, rhs is more specific
*/
int type_connectivity_check (InstType *lhs, 
			     InstType *rhs, 
			     int skip_last_array)
{
  struct act_position p;
  Type *tmp;
  
  if (lhs == rhs) return 1;

  typecheck_errmsg[0] = '\0';

#if 0
  printf ("check: ");
  lhs->Print (stdout);
  printf (" -vs- ");
  rhs->Print (stdout);
  printf ("\n");
  fflush (stdout);
#endif

  if (lhs->isExpanded ()) {
    /* the connectivity check is now strict */
    if ((tmp = lhs->isConnectable (rhs, (skip_last_array ? 3: 2)))) {
      if (TypeFactory::isPTypeType(tmp)) {
	/* special case */
	return 1;
      }
      if (tmp->isEqual(lhs->BaseType()) && !tmp->isEqual(rhs->BaseType())) {
	return 2;
      }
      else if (tmp->isEqual(rhs->BaseType()) && !tmp->isEqual(lhs->BaseType())) {
	return 3;
      }
      Assert (tmp->isEqual (lhs->BaseType()) && tmp->isEqual(rhs->BaseType()), "Hmm.");
      return 1;
    }
  }
  else {
    /* check dim compatibility */
    if ((tmp = lhs->isConnectable (rhs, 1))) {
      if (TypeFactory::isPTypeType(tmp)) {
	/* special case */
	return 1;
      }
      if (tmp->isEqual (lhs->BaseType()) && !tmp->isEqual(rhs->BaseType())) {
	return 2;
      }
      else if (tmp->isEqual (rhs->BaseType()) && !tmp->isEqual (lhs->BaseType())) {
	return 3;
      }
      Assert (tmp->isEqual (lhs->BaseType()) && tmp->isEqual (rhs->BaseType()), "Hmm.");
      return 1;
    }
  }

  if (!lhs->israwExpanded()) {
    p.l = stype_line_no;
    p.c = stype_col_no;
    p.f = stype_file_name;
  }
  /* append this */
  char buf1[1024], buf2[1024];
  lhs->sPrint (buf1, 1024);
  rhs->sPrint (buf2, 1024);

  if (strcmp (buf1, buf2) != 0) {
    typecheck_errappend ("Types `%s' and `%s' are not compatible", buf1, buf2);
  }

  return 0;
}

InstType *AExpr::getInstType (Scope *s, int *islocal, int expanded)
{
  InstType *cur, *tmp;
  AExpr *ae;
  int count;
  int xlocal;

  if (islocal) {
    *islocal = 1;
  }
  
  count = 0;
  switch (t) {
  case AExpr::CONCAT:
  case AExpr::COMMA:
    cur = l->getInstType (s, &xlocal, expanded);
    if (!cur) return NULL;
    if (islocal) {
      *islocal &= xlocal;
    }

    if (t == AExpr::CONCAT && !cur->arrayInfo()) {
      act_error_ctxt (stderr);
      fatal_error ("Concatenations are for arrays only!");
    }

    ae = r;
    if (t == AExpr::CONCAT && expanded) {
      Array *xa;
      /* find the last dimension size */
      Assert (cur->isExpanded(), "Should be expanded!");
      xa = cur->arrayInfo();
      Assert (xa, "Huh?");
      if (xa->isSparse()) {
	act_error_ctxt (stderr);
	fatal_error ("Cannot have a sparse array within an array expression");
      }
      count = xa->range_size(0);
    }
    else {
      count = 1;
    }
    while (ae) {
      Assert (ae->GetLeft (), "Hmm");
      tmp = ae->GetLeft ()->getInstType (s, &xlocal, expanded);
      if (islocal) {
	*islocal &= xlocal;
      }

      if (!tmp) {
	delete cur;
	return NULL;
      }
      
      if (tmp->arrayInfo() && tmp->arrayInfo()->isSparse()) {
	type_errctxt (expanded, "Cannot have a sparse array within an array expression");
	exit (1);
      }

      if (!tmp->arrayInfo() && t == AExpr::CONCAT) {
	type_errctxt (expanded, "Array concatenation requires arrays");
	fprintf (stderr, "\n   array expr: ");
	this->Print (stderr);
	fprintf (stderr, "\n   sub-expr: ");
	ae->GetLeft()->Print (stderr);
	fprintf (stderr, " isn't an array\n");
	exit (1);
	delete cur;
	delete tmp;
	return NULL;
      }

      if (type_connectivity_check (cur, tmp, (t == AExpr::CONCAT ? 1 : 0)) != 1) {
	type_errctxt (expanded, "Array expression, components are not compatible");
	fprintf (stderr, " (");
	cur->Print (stderr);
	fprintf (stderr, "  v/s  ");
	tmp->Print (stderr);
	fprintf (stderr, ")\n\t%s\n", act_type_errmsg());
	exit (1);
#if 0
	delete cur;
	delete tmp;
	return NULL;
#endif	
      }
      
      /* cur has the base type.
	 for comma expresisons, the next type simply is a higher
	 dimensional array */
      
      if (!expanded || (t == AExpr::COMMA)) {
	count++;
      }
      else {
	Array *xa;
	Assert (tmp->isExpanded(), "What on earth?");
	xa = tmp->arrayInfo();
	Assert (xa, "Huh?");
	count += xa->range_size (0);
      }
      delete tmp;
      ae = ae->GetRight ();
    }
    
    /* count = # of items in the list */
    
    if (t == AExpr::CONCAT) {
      Array *xa;
      
      if (!expanded) {
	return cur;
      }
      /* if it is expanded, we need to return a new type with the
	 correct size for the last range, i.e., "count" */
      tmp = new InstType (cur, 1);
      xa = cur->arrayInfo ();
      xa = xa->Clone ();
      xa->update_range (0, 0, count-1);
      tmp->MkArray (xa);
      
      delete cur;
      return tmp;
    }
    else {
      /* comma expression */
      
      Array *a, *tmpa;

      /* we have to promote the dimensions. we now have ``count'' items */
      tmp = new InstType (cur, 1);
      a = cur->arrayInfo ();

      if (expanded) {
	Assert (cur->isExpanded(), "Hmm");
	tmpa = new Array (0, count-1);
      }
      else {
	tmpa = new Array (const_expr (count));
      }
      tmpa->mkArray ();

      if (!a) {
	a = tmpa;
      }
      else {
	if (expanded) {
	  Assert (a->isExpanded(), "Hmm");
	}
	else {
	  Assert (!a->isExpanded(), "Hmm");
	}
	a = a->Clone ();
	a->preConcat (tmpa);
	delete tmpa;
      }
      tmp->MkArray (a);

      delete cur;
      return tmp;
    }
    break;

  case AExpr::EXPR:
    { InstType *it = act_expr_insttype (s, (Expr *)l, &xlocal, 0);
      if (islocal) {
	*islocal &= xlocal;
      }
      if (expanded && it) {
	it->mkExpanded();
      }
      return it;
    }
     break;

#if 0
  case AExpr::SUBRANGE:
    fatal_error ("Should not be here");
    return act_actual_insttype (s, (ActId *)l, NULL);
#endif

  default:
    fatal_error ("Unimplemented?, %d", t);
    break;
  }
  return NULL;
}

int act_type_conn (Scope *s, AExpr *ae, AExpr *rae)
{
  int ret;

  Assert (s, "NULL scope");
  Assert (ae, "NULL AExpr");
  Assert (rae, "NULL AExpr");
  typecheck_errmsg[0] = '\0';

#if 0
  printf ("Here: checking: ");
  ae->Print (stdout);
  printf (" v/s ");
  rae->Print (stdout);
  printf ("\n");
#endif

  int lhslocal;
  InstType *lhs = ae->getInstType (s, &lhslocal);
  if (!lhs) return 0;

  int rhslocal;
  InstType *rhs = rae->getInstType (s, &rhslocal);
  if (!rhs) {
    delete lhs;
    return 0;
  }

#if 0
  printf ("Types: ");
  lhs->Print (stdout);
  printf (" -vs- ");
  rhs->Print (stdout);
  printf ("\n");
  fflush (stdout);
#endif  
  
  /* now check type compatibility between it, id, and rhs */
  ret = type_connectivity_check (lhs, rhs);

#if 0
  printf ("Ok, here, result=%d\n", ret);
  fflush (stdout);
#endif

  delete lhs;
  delete rhs;

#if 0
  printf ("ret=%d, lhslocal=%d, rhslocal=%d\n", ret, lhslocal, rhslocal);
#endif

  if (ret == 3 && !lhslocal) {
    typecheck_err ("Connection is compatible, but requires non-local instance (LHS) to be specialized");
    return 0;
  }
  if (ret == 2 && !rhslocal) {
    typecheck_err ("Connection is compatible, but requires non-local instance (RHS) to be specialized");
    return 0;
  }
  return ret;
}

int act_type_conn (Scope *s, ActId *id, AExpr *rae)
{
  int ret;

  Assert (s, "NULL scope");
  Assert (id, "NULL id");
  Assert (rae, "NULL AExpr");
  typecheck_errmsg[0] = '\0';

  /*
  printf ("Here2: checking: ");
  id->Print (stdout);
  printf (" v/s ");
  rae->Print (stdout);
  printf ("\n");
  */

  int lhslocal;
  InstType *lhs = act_actual_insttype (s, id, &lhslocal);

  if (!lhs) return T_ERR;

  int rhslocal;
  InstType *rhs = rae->getInstType (s, &rhslocal);

  if (!rhs) {
    delete lhs;
    return T_ERR;
  }

  /*
  printf ("Types: "); fflush (stdout);
  lhs->Print (stdout);
  printf (" -vs- "); fflush (stdout);
  rhs->Print (stdout);
  printf ("\n");
  fflush (stdout);
  */

  /* now check type compatibility between it, id, and rhs */
  ret = type_connectivity_check (lhs, rhs);

  delete lhs;
  delete rhs;

  /*
  printf ("Ok, here2\n");
  fflush (stdout);
  */
#if 0
  printf ("ret=%d, lhslocal=%d, rhslocal=%d\n", ret, lhslocal, rhslocal);
#endif
  
  if (ret == 3 && !lhslocal) {
    typecheck_err ("Connection is compatible, but requires non-local instance (LHS) to be specialized");
    return 0;
  }
  if (ret == 2 && !rhslocal) {
    typecheck_err ("Connection is compatible, but requires non-local instance (RHS) to be specialized");
    return 0;
  }
  return ret;
}


/*------------------------------------------------------------------------
 * 
 * CHP assignmable typechecking rules
 *
 *------------------------------------------------------------------------
 */
int type_chp_check_assignable (InstType *lhs, InstType *rhs)
{
  Assert (lhs && rhs, "NULL argument to type_check_assignable()");

  if (!TypeFactory::isDataType (lhs) && !TypeFactory::isStructure (lhs) &&
      !TypeFactory::isPBoolType (lhs) && !TypeFactory::isPIntType (lhs)) {
    typecheck_err ("Assignable variable requires data types!");
    return 0;
  }
  if (TypeFactory::isPBoolType (lhs)) {
    if (TypeFactory::isPBoolType (rhs)) {
      return 1;
    }
    typecheck_err ("pbool assignment requires pbool expressions");
    return 0;
  }
  else if (TypeFactory::isPIntType (lhs)) {
    if (TypeFactory::isPIntType (rhs)) {
      return 1;
    }
    typecheck_err ("pint assignment requires pint expressions");
    return 0;
  }
  else {
    if (TypeFactory::isChanType (rhs)) {
      rhs = TypeFactory::getChanDataType (rhs);
    }
    if (TypeFactory::isBaseBoolType (lhs)) {
      if (TypeFactory::isBaseBoolType (rhs) || TypeFactory::isPBoolType (rhs)) {
	return 1;
      }
      typecheck_err ("Bool/non-bool assignment is not permitted");
      return 0;
    }
    else if (TypeFactory::isBaseIntType (lhs)) {
      if (TypeFactory::isBaseIntType (rhs) || TypeFactory::isPIntType (rhs)) {
	return 1;
      }
      typecheck_err ("Int/non-int assignment is not permitted");
      return 0;
    }
    else if (TypeFactory::isStructure (lhs)) {
      if (TypeFactory::isStructure (rhs)) {
	if (type_connectivity_check (lhs, rhs, 0)) {
	  return 1;
	}
	else {
	  typecheck_err ("Incompatible structures");
	  return 0;
	}
      }
      typecheck_err ("Structure/non-structure types are incompatible");
      return 0;
    }
    else if (TypeFactory::isUserEnum (lhs)) {
      if (TypeFactory::isUserEnum (rhs)) {
	if (type_connectivity_check (lhs, rhs, 0)) {
	  return 1;
	}
	else {
	  typecheck_err ("Incompatible enumerations");
	  return 0;
	}
      }
      typecheck_err ("Enumeration/non-enumeration types are incompatible");
      return 0;
    }
    else {
      Assert (0, "What case is this?");
      return 0;
    }
  }
}

/*------------------------------------------------------------------------
 *
 * Typecheck channel with expression / id pair
 *
 *------------------------------------------------------------------------
 */
int act_type_chan (Scope *sc, Chan *ch, int is_send, Expr *e, ActId *id,
		   int override_id)
{
  int ret = 1;
  int it1_override, it2_override;
  InstType *it1, *it2;
  int id_type;

  it1_override = -1;
  it2_override = override_id;

  if (e) {
    it1 = act_expr_insttype (sc, e, NULL, 0);
  }
  else {
    it1 = NULL;
  }
  if (id) {
    id_type = act_type_var (sc, id, &it2);
    if (id_type == T_ERR) {
      typecheck_err ("Could not find variable type");
      if (it1) {
	delete it1;
      }
      return 0;
    }
    if (!(id_type & T_ARRAYOF)) {
      it2 = new InstType (it2, 1);
      it2->MkCached ();
    }
  }
  else {
    it2 = NULL;
  }
  if (!is_send) {
    InstType *tmp = it1;
    it1 = it2;
    it2 = tmp;

    int xtmp = it1_override;
    it1_override = it2_override;
    it2_override = xtmp;
  }
    
  if (it1) {
    if (TypeFactory::isChanType (it1)) {
      it1 = TypeFactory::getChanDataType (it1);
    }
    ret = 0;
    if (!TypeFactory::isDataType (it1) && !TypeFactory::isStructure (it1) &&
	!TypeFactory::isPIntType (it1) && !TypeFactory::isPBoolType (it1)) {
      typecheck_err ("Channels require data types!");
    }
    else {
      if (TypeFactory::isBoolType (ch->datatype())) {
	if ((TypeFactory::isBaseBoolType (it1) || TypeFactory::isPBoolType (it1)) || (it1_override == 0 && TypeFactory::isBaseIntType (it1))) {
	  ret = 1;
	}
	else {
	  typecheck_err ("Boolean/non-boolean types are incompatible.");
	}
      }
      else if (TypeFactory::isIntType (ch->datatype())) {
	if (TypeFactory::isBaseIntType (it1) || TypeFactory::isPIntType (it1)
	    || (it1_override == 1 && TypeFactory::isBaseBoolType(it1))) {
	  ret = 1;
	}
	else {
	  typecheck_err ("Integer/non-integer types are incompatible.");
	}
      }
      else if (TypeFactory::isStructure (ch->datatype())) {
	if (TypeFactory::isStructure (it1)) {
	  if (type_connectivity_check (it1, ch->datatype(), 0)) {
	    ret = 1;
	  }
	}
	else {
	  typecheck_err ("Structure/non-structure types are incompatible.");
	}
      }
    }
  }
  if (!ret) {
    if (it1) { delete it1; }
    if (it2) { delete it2; }
    return 0;
  }
  
  if (it2) {
    if (!ch->acktype()) {
      if (it1) { delete it1; }
      if (it2) { delete it2; }
      return 0;
    }
    if (TypeFactory::isChanType (it2)) {
      it2 = TypeFactory::getChanDataType (it2);
    }
    ret = 0;
    if (!TypeFactory::isDataType (it2) && !TypeFactory::isStructure (it2) &&
	!TypeFactory::isPIntType (it2) &&
	!TypeFactory::isPBoolType (it2)) {
      typecheck_err ("Channels require data types!");
    }
    else {
      if (TypeFactory::isBoolType (ch->acktype())) {
	if (TypeFactory::isBaseBoolType (it2) || TypeFactory::isPBoolType (it2)
	    || (it2_override == 0 && TypeFactory::isBaseIntType (it2))) {
	  ret = 1;
	}
	else {
	  typecheck_err ("Boolean/non-boolean types are incompatible.");
	}
      }
      else if (TypeFactory::isIntType (ch->acktype())) {
	if (TypeFactory::isBaseIntType (it2) || TypeFactory::isPIntType (it2)
	    || (it2_override == 1 && TypeFactory::isBaseBoolType (it2))) {
	  ret = 1;
	}
	else {
	  typecheck_err ("Integer/non-integer types are incompatible.");
	}
      }
      else if (TypeFactory::isStructure (ch->acktype())) {
	if (TypeFactory::isStructure (it2)) {
	  if (type_connectivity_check (it2, ch->acktype(), 0)) {
	    ret = 1;
	  }
	}
	else {
	  typecheck_err ("Structure/non-structure types are incompatible.");
	}
      }
    }
  }
  if (it1) { delete it1; }
  if (it2) { delete it2; }
  return ret;
}  
