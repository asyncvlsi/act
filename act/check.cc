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
#include "act_parse_int.h"
#include "act_walk.extra.h"


/*
 *
 * Typechecking code
 *
 */

static char typecheck_errmsg[10240];

static void typecheck_err (const char *s, ...)
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

static void dumpflags(int f)
{
  if (f == T_ERR) { printf ("[t-ERR]"); return; }
  printf ("[v=%x]", f);
  if (T_BASETYPE (f) == T_INT) {
    printf ("[t-int]");
  }
  else if (T_BASETYPE (f) == T_REAL) { 
    printf ("[t-real]");
  }
  else if (T_BASETYPE (f) == T_BOOL) {
    printf ("[t-bool]");
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


int act_type_var (Scope *s, ActId *id)
{
  InstType *it;
  UserDef *u;
  Data *d;
  int is_strict;

  /*printf ("[scope=%x] check: var ", s); id->Print (stdout); printf ("\n");*/

  Assert (s, "Scope?");
  Assert (id, "Identifier?");
  it = s->Lookup (id->getName());
  if (!it) {
    it = s->FullLookup (id->getName());
    is_strict = T_STRICT;
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
      is_strict = T_STRICT;
      /*printf ("this is strict [%s]!\n", id->getName ());*/
    }
  }

  Assert (!(id->isDeref () && !it->arrayInfo()), "Check during parsing!");
  
  Type *t = it->BaseType ();
  unsigned int arr = 0;

  if (it->arrayInfo() && !id->isDeref()) {
    arr = T_ARRAYOF;
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
  if (TypeFactory::isIntType (t)) {
    return T_INT|arr;
  }
  if (TypeFactory::isBoolType (t)) {
    return T_BOOL|arr;
  }
  if (!TypeFactory::isDataType (t)) {
    if (TypeFactory::isChanType (t)) {
      return T_CHAN|arr;
    }
    if (TypeFactory::isProcessType (t)) {
      return T_PROC|arr;
    }
    typecheck_err ("Identifier `%s' is not a data type", id->getName ());
    return T_ERR;
  }
  d = dynamic_cast<Data *>(t);
  if (d->isEnum()) {
    return T_INT|arr;
  }
#if 0
  fatal_error ("FIX this. Check that the data type is an int or a bool");
  /* XXX: FIX THIS PLEASE */
  typecheck_err ("FIX this. Check data type.");
#endif
  return T_DATA|arr;
}


/**
 *  Typecheck expression
 *
 *  @param s is the scope in which all identifiers are evaluated
 *  @param e is the expression to be typechecked
 *  @return -1 on an error
 */
int act_type_expr (Scope *s, Expr *e)
{
  int lt, rt;
  int flgs;
  if (!e) return T_ERR;

  /*  
  printf ("check: %s\n", expr_operator_name (e->type));
  printf (" lt: %x  rt: %x\n", lt, rt);      
  */

#define EQUAL_LT_RT2(f,g)						\
  do {									\
    lt = act_type_expr (s, e->u.e.l);					\
    rt = act_type_expr (s, e->u.e.r);					\
    if (lt == T_ERR || rt == T_ERR) return T_ERR;			\
    if ((lt & T_ARRAYOF) || (rt & T_ARRAYOF)) {				\
      typecheck_err ("`%s': operator applied to array argument", expr_operator_name (e->type)); \
      return T_ERR;							\
    }									\
    flgs = lt & rt & (T_PARAM|T_STRICT);				\
    if ((f & T_BOOL) && T_BASETYPE (lt) == T_BOOL && T_BASETYPE (rt) == T_BOOL) { \
      return (((f) != (g) ? (g) : T_BOOL) & ~(T_PARAM|T_STRICT))|flgs;  \
    }                                                                   \
    if ((f & T_REAL) && T_BASETYPE_ISNUM(lt) && T_BASETYPE_ISNUM(rt)) { \
      if (T_BASETYPE(lt) == T_REAL || T_BASETYPE(rt) == T_REAL) {       \
        return (((f) != (g) ? (g) : T_REAL) & ~(T_PARAM|T_STRICT))|flgs; \
      }                                                                 \
      return (((f) != (g) ? (g) : T_INT) & ~(T_PARAM|T_STRICT))|flgs;   \
    }                                                                   \
    if ((f & T_INT) && T_BASETYPE(lt) == T_INT && T_BASETYPE(rt) == T_INT) { \
      return (((f) != (g) ? (g) : T_INT) & ~(T_PARAM|T_STRICT))|flgs;   \
    }                                                                   \
  } while (0)

#define EQUAL_LT_RT(f)	 EQUAL_LT_RT2((f),lt)

#define INT_OR_REAL							\
  do {									\
    lt = act_type_expr (s, e->u.e.l);					\
    rt = act_type_expr (s, e->u.e.r);					\
    if (lt == T_ERR || rt == T_ERR) return T_ERR;			\
    if ((lt & T_ARRAYOF) || (rt & T_ARRAYOF)) {				\
      typecheck_err ("`%s': operator applied to array argument", expr_operator_name (e->type)); \
      return T_ERR;							\
    }									\
    flgs = lt & rt & (T_PARAM|T_STRICT);				\
    if (T_BASETYPE_ISNUM(lt) && T_BASETYPE_ISNUM(rt)) {			\
      if (T_BASETYPE (lt) == T_REAL || T_BASETYPE (rt) == T_REAL) {	\
	return T_REAL|flgs;						\
      }									\
      else {								\
	return T_INT|flgs;						\
      }									\
    }									\
  } while (0)
  
  
  switch (e->type) {
    /* Boolean, binary or int */
  case E_AND:
  case E_OR:
    EQUAL_LT_RT(T_BOOL|T_INT);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs bool/bool or int/int", expr_operator_name (e->type));
    return T_ERR;
    break;

  case E_ANDLOOP:
  case E_ORLOOP:
    lt = act_type_expr (s, e->u.e.r->u.e.l);
    if (T_BASETYPE (lt) != T_INT || (lt & T_ARRAYOF)) {
      typecheck_err ("Loop range is not of integer type");
      return T_ERR;
    }
    if (e->u.e.r->u.e.r->u.e.l) {
      lt = act_type_expr (s, e->u.e.r->u.e.r->u.e.l);
      if (T_BASETYPE (lt) != T_INT || (lt & T_ARRAYOF)) {
	typecheck_err ("Loop range is not of integer type");
	return T_ERR;
      }
    }
    lt = act_type_expr (s, e->u.e.r->u.e.r->u.e.r);
    if (T_BASETYPE (lt) != T_BOOL || (lt & T_ARRAYOF)) {
      typecheck_err ("Loop body is not of bool type");
      return T_ERR;
    }
    return T_BOOL;
    break;

    /* Boolean, unary */
  case E_NOT:
    lt = act_type_expr (s, e->u.e.l);
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
  case E_MULT:
  case E_DIV:
  case E_MOD:
    INT_OR_REAL;
    /*EQUAL_LT_RT(T_INT|T_REAL);*/
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/real for both", expr_operator_name (e->type));
    /*typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int or real/real", expr_operator_name (e->type));*/
    return T_ERR;
    break;

  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
    EQUAL_LT_RT2(T_REAL,T_BOOL);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int or real/real", expr_operator_name (e->type));
    return T_ERR;

    /* Unary, real or integer */
  case E_UMINUS:
    lt = act_type_expr (s, e->u.e.l);
    if (lt == T_ERR) return T_ERR;
    if (T_BASETYPE_ISNUM (lt)) {
      return lt;
    }
    typecheck_err ("`!' operator applied to a Boolean");
    return T_ERR;
    break;

    /* Unary, integer or bool */
  case E_COMPLEMENT:
    lt = act_type_expr (s, e->u.e.l);
    if (lt == T_ERR) return lt;
    if (T_BASETYPE_ISINTBOOL (lt)) {
      return lt;
    }
    typecheck_err ("`~' operator needs an int or bool");
    return T_ERR;

    /* Binary, integer only */
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_XOR:
    EQUAL_LT_RT(T_INT);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int", expr_operator_name (e->type));
    return T_ERR;

    /* weird */
  case E_QUERY:
  case E_COLON:

    /* binary, Integer or Bool or real */
  case E_EQ:
  case E_NE:
    EQUAL_LT_RT2(T_BOOL|T_REAL,T_BOOL);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs real/real, int/int, or bool/bool", expr_operator_name (e->type));
    return T_ERR;
    break;

  case E_CONCAT:
    do {
      lt = act_type_expr (s, e->u.e.l);
      if (lt == T_ERR) return T_ERR;
      if (!T_BASETYPE_ISINTBOOL (lt)) {
	typecheck_err ("{ } concat: all items must be int or bool");
      }
      if (e->u.e.r) {
	e = e->u.e.r;
      }
    } while (e->type == E_CONCAT);
    return T_INT;
    break;

  case E_BITFIELD:
    if (T_BASETYPE (lt) == T_INT) {
      return lt;
    }
    typecheck_err ("Bitfield! Implement me please");
    return T_ERR;
    break;

    /* UMMMM */
  case E_FUNCTION:
    /* typecheck all arguments; then return result type */
    typecheck_err ("Function! Implement me please");
    return T_ERR;
    break;

    /* leaves */
  case E_VAR:
    lt = act_type_var (s, (ActId *)e->u.e.l);
    return lt;
    break;

  case E_PROBE:
    lt = act_type_var (s, (ActId *)e->u.e.l);
    if (lt == T_CHAN) {
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
    return (T_INT|T_PARAM|T_STRICT);
    break;

  case E_TRUE:
  case E_FALSE:
    return (T_BOOL|T_PARAM|T_STRICT);
    break;

  case E_SELF:
    {
      ActId *tmpid = new ActId ("self");
      lt = act_type_var (s, tmpid);
      delete tmpid;
    }
    return lt;
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
	  typecheck_err ("Port `.%s' access for an arrayed type is missing an array dereference", id->getName ());
	  return NULL;
	}
	if (it->arrayInfo()->nDims () != id->arrayInfo()->nDims ()) {
	  typecheck_err ("Port `.%s': number of dimensions don't match type (%s v/s %s)", id->arrayInfo()->nDims(), it->arrayInfo()->nDims ());
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
	  typecheck_err ("Array de-reference for a non-arrayed type, port `.%s'", id->getName ());
	  return NULL;
	}
      }
      UserDef *u;
      u = dynamic_cast<UserDef *>(it->BaseType ());
      Assert (u, "This should have been caught during parsing!");
      id = id->Rest ();
      it = u->Lookup (id);
  }
  /* we have insttype, and id: now handle any derefs */
  if (!id->arrayInfo ()) {
    /* easy case */
    return it;
  }
  if (!it->arrayInfo()) {
    typecheck_err ("Array de-reference for a non-arrayed type, port `.%s'", id->getName ());
    return NULL;
  }
  
  if (it->arrayInfo ()->nDims () != id->arrayInfo()->nDims ()) {
    typecheck_err ("Port `.%s': number of dimensions don't match type (%d v/s %d)", id->getName(), id->arrayInfo()->nDims (), it->arrayInfo()->nDims ());
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

	tmp = new Array (ida->lo(i), ida->hi(i));;
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

InstType *act_expr_insttype (Scope *s, Expr *e, int *islocal)
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
  if (e->type == E_VAR) {
    /* special case */
    it = act_actual_insttype (s, (ActId *)e->u.e.l, islocal);
    return it;
  }
  ret = act_type_expr (s, e);

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
      fatal_error ("Not sure how we got here");
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
      if (tmp == lhs->BaseType() && tmp != rhs->BaseType()) {
	return 2;
      }
      else if (tmp == rhs->BaseType() && tmp != lhs->BaseType()) {
	return 3;
      }
      Assert (tmp == lhs->BaseType() && tmp == rhs->BaseType(), "Hmm.");
      return 1;
    }
  }
  else {
    /* check dim compatibility */
    if ((tmp = lhs->isConnectable (rhs, 1))) {
      if (tmp == lhs->BaseType() && tmp != rhs->BaseType()) {
	return 2;
      }
      else if (tmp == rhs->BaseType() && tmp != lhs->BaseType()) {
	return 3;
      }
      Assert (tmp == lhs->BaseType() && tmp == rhs->BaseType(), "Hmm.");
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
  
  typecheck_errappend ("Types `%s' and `%s' are not compatible",
		       buf1, buf2);

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
    { InstType *it = act_expr_insttype (s, (Expr *)l, &xlocal);
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
