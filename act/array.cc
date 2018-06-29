/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <act/types.h>
#include <act/inst.h>
#include <string.h>
#include "list.h"
#include "misc.h"


/*------------------------------------------------------------------------
 * Basic constructor
 *------------------------------------------------------------------------
 */
Array::Array()
{
  r = NULL;
  dims = 0;
  next = NULL;
  deref = 0;
  expanded = 0;
  range_sz = -1;
}

/*------------------------------------------------------------------------
 * Constructor: [e..f] single dimensional array
 *------------------------------------------------------------------------
 */
Array::Array (Expr *e, Expr *f)
{
  dims = 1;
  NEW (r, struct range);

  if (f == NULL) {
    r->u.ue.lo = NULL;
    r->u.ue.hi = e;
    deref = 1;
  }
  else {
    r->u.ue.lo = e;
    r->u.ue.hi = f;
    deref = 0;
  }
  expanded = 0;
  range_sz = -1;
  next = NULL;
}

/*------------------------------------------------------------------------
 * Constructor: [e..f] single dimensional array
 *------------------------------------------------------------------------
 */
Array::Array (int lo, int hi)
{
  dims = 1;
  NEW (r, struct range);
  r->u.ex.lo = lo;
  r->u.ex.hi = hi;
  r->u.ex.isrange = 1;
  deref = 0;
  expanded = 1;
  range_sz = -1;
  next = NULL;
}

/*------------------------------------------------------------------------
 *  Destructor
 *------------------------------------------------------------------------
 */
Array::~Array ()
{
  if (next) {
    delete next;
  }
  FREE (r);
}


/*------------------------------------------------------------------------
 *
 *  Array::Clone --
 *
 *   Deep copy of array
 *
 *------------------------------------------------------------------------
 */
Array *Array::Clone ()
{
  Array *ret;

  ret = new Array();

  ret->deref = deref;
  ret->expanded = expanded;
  if (next) {
    ret->next = next->Clone ();
  }
  else {
    ret->next = NULL;
  }
  ret->dims = dims;

  if (dims > 0) {
    MALLOC (ret->r, struct range, dims);
    for (int i= 0; i < dims; i++) {
      ret->r[i] = r[i];
    }
  }
  return ret;
}

/*------------------------------------------------------------------------
 *  Only clone this range 
 *------------------------------------------------------------------------
 */
Array *Array::CloneOne ()
{
  Array *ret;

  ret = new Array();

  ret->deref = deref;
  ret->expanded = expanded;
  ret->next = NULL;
  ret->dims = dims;

  if (dims > 0) {
    MALLOC (ret->r, struct range, dims);
    for (int i = 0; i < dims; i++) {
      ret->r[i] = r[i];
    }
  }
  return ret;
}


/*------------------------------------------------------------------------
 * Compare two arrays
 *   dims have to be compatible
 *   strict = 0 : just check ranges have the same sizes
 *   strict = 1 : the ranges must be the same
 *   strict = -1 : ranges have same sizes, except for the first one
 *------------------------------------------------------------------------
 */
int Array::isEqual (Array *a, int strict) 
{
  struct range *r1, *r2;
  int i;

  if (!isDimCompatible (a)) return 0;

  r1 = r;
  r2 = a->r;

  if (!expanded) {
    /* no distinction between strict and non-strict */
    for (i=0; i < dims; i++) {
      if ((r1[i].u.ue.lo && !r2[i].u.ue.lo) || (!r1[i].u.ue.lo && r2[i].u.ue.lo))
	return 0;
      if (r1[i].u.ue.lo && !expr_equal (r1[i].u.ue.lo, r2[i].u.ue.lo)) return 0;
      if (r1[i].u.ue.hi && !expr_equal (r1[i].u.ue.hi, r2[i].u.ue.hi)) return 0;
    }
  }
  else {
    for (i=(strict == -1 ? 1 : 0); i < dims; i++) {
      if (strict == 1) {
	if ((r1[i].u.ex.lo != r2[i].u.ex.lo) || (r1[i].u.ex.hi != r2[i].u.ex.hi))
	  return 0;
      }
      else {
	if ((r1[i].u.ex.hi - r1[i].u.ex.lo) != (r2[i].u.ex.hi - r2[i].u.ex.lo))
	  return 0;
      }
    }
  }
  if (next) {
    return next->isEqual (a->next, strict);
  }
  return 1;
}

/*------------------------------------------------------------------------
 * Check if two arrays are compatible
 *------------------------------------------------------------------------
 */
int Array::isDimCompatible (Array *a)
{
  struct range *r1, *r2;
  int i;

  if (dims != a->dims) return 0;
  if (deref != a->deref) return 0;
  if (isSparse() != a->isSparse()) return 0;
  
  return 1;
}


/*------------------------------------------------------------------------
 *  Increase size of this array by appending dimensions from "a"
 *------------------------------------------------------------------------
 */
void Array::Concat (Array *a)
{
  int i;

  Assert (isSparse() == 0, "Array::Concat() only supported for dense arrays");
  Assert (a->isSparse() == 0, "Array::Concat() only works for dense arrays");
  Assert (a->isExpanded () == expanded, "Array::Concat() must have same expanded state");
  
  dims += a->dims;

  if (dims == a->dims) {
    MALLOC (r, struct range, dims);
  }
  else {
    REALLOC (r, struct range, dims);
  }
  
  for (i=0; i < a->dims; i++) {
    r[dims - a->dims + i] = a->r[i];
  }

  /* if any part is not a deref, it is a subrange */
  if (a->deref == 0) {
    deref = 0;
  }

  range_sz = -1;
}


/*------------------------------------------------------------------------
 *  Given an array, this returns the linear offset within the array of
 *  the deref "a".
 *
 *   -1 if "a" is not within the bounds of the current range
 *------------------------------------------------------------------------
 */
int Array::in_range (Array *a)
{
  int sz;
  int offset;
  
  /* expanded only */
  /* this is true, but in_range is a private function 
    Assert (a->isDeref (), "Hmmm");
  */

  /* compute the size */
  if (range_sz == -1) {
    size();
  }

  sz = range_sz;
  offset = 0;
  
  for (int i=0; i < dims; i++) {
    int d;

    /*Assert (a->r[i].u.ex.lo == a->r[i].u.ex.hi, "What?");*/
    d = a->r[i].u.ex.lo;
    if (r[i].u.ex.lo > d || r[i].u.ex.hi < d) {
      return -1;
    }
    sz = sz / (r[i].u.ex.hi - r[i].u.ex.lo + 1);
    offset = offset + (d - r[i].u.ex.lo)*sz;

    /* this is here to handle the subrange case */
    d = a->r[i].u.ex.hi;
    if (r[i].u.ex.lo > d || r[i].u.ex.hi < d) {
      return -1;
    }
    
    Assert ((i != dims-1) || (sz == 1), "Wait a sec");
  }
  return offset;
}

int Array::in_range (int *a)
{
  int sz;
  int offset;
  
  /* expanded only */
  /* this is true, but in_range is a private function 
    Assert (a->isDeref (), "Hmmm");
  */

  /* compute the size */
  if (range_sz == -1) {
    size();
  }

  sz = range_sz;
  offset = 0;
  
  for (int i=0; i < dims; i++) {
    int d;

    d = a[i];
    if (r[i].u.ex.lo > d || r[i].u.ex.hi < d) {
      return -1;
    }
    sz = sz / (r[i].u.ex.hi - r[i].u.ex.lo + 1);
    offset = offset + (d - r[i].u.ex.lo)*sz;

    Assert ((i != dims-1) || (sz == 1), "Wait a sec");
  }
  return offset;
}


/*------------------------------------------------------------------------
 *  Finds offset of "a" within the current array. -1 if not found.
 *  This uses in_range as a helper function.
 *------------------------------------------------------------------------
 */
int Array::Offset (int *a)
{
  int offset;
  
  /* expanded only */
  Assert (expanded, "Hmm...");

  /*Assert (a->isDeref (), "Hmm...");*/

  offset = in_range (a);
  if (offset != -1) {
    return offset;
  }
  else {
    offset = next->Offset (a);
    if (offset == -1) {
      return -1;
    }
    else {
      return range_sz + offset;
    }
  }
}

int Array::Offset (Array *a)
{
  int offset;
  
  /* expanded only */
  Assert (expanded && a->isExpanded(), "Hmm...");

  /*Assert (a->isDeref (), "Hmm...");*/

  offset = in_range (a);
  if (offset != -1) {
    return offset;
  }
  else {
    offset = next->Offset (a);
    if (offset == -1) {
      return -1;
    }
    else {
      return range_sz + offset;
    }
  }
}


/*------------------------------------------------------------------------
 *  "a" is an ExpandRef'ed array
 *  Returns 1 if "a" is a valid de-reference or subrange reference for
 *  the array, 0 otherwise.
 *------------------------------------------------------------------------
 */
int Array::Validate (Array *a)
{
  if (!expanded || !a->isExpanded()) {
    fatal_error ("Array::Validate() should only be called for expanded arrays");
  }
#if 0
  if (!a->isDeref()) {
    act_error_ctxt (stderr);
    fprintf (stderr, " array: ");
    a->Print (stderr);
    fprintf (stderr, "\n");
    fatal_error ("Should be a de-reference!");
  }
#endif
  Assert (dims == a->nDims(), "dimensions don't match!");

  int i, d;

  for (i=0; i < dims; i++) {
    /* a is going to be from an ID */
    d = a->r[i].u.ex.hi;
    if (r[i].u.ex.lo > d || r[i].u.ex.hi < d) {
      if (next) {
	return next->Validate (a);
      }
      else {
	return 0;
      }
    }
    if (a->r[i].u.ex.lo != a->r[i].u.ex.hi) {
      /* subrange, check the extreme ends */
      d = a->r[i].u.ex.lo;
      if (r[i].u.ex.lo > d || r[i].u.ex.hi < d) {
	if (next) {
	  return next->Validate (a);
	}
	else {
	  return 0;
	}
      }
    }
  }
  return 1;
}

/*------------------------------------------------------------------------
 * Print out array
 *------------------------------------------------------------------------
 */
void Array::Print (FILE *fp)
{
  char buf[10240];
  sPrint (buf, 10240);
  fprintf (fp, "%s", buf);
}

void Array::sPrint (char *buf, int sz)
{
  Array *pr;
  int k = 0;
  int l;

#define PRINT_STEP				\
  do {						\
    l = strlen (buf+k);				\
    k += l;					\
    sz -= l;					\
    if (sz <= 0) return;			\
  } while (0)

  if (next) {
    snprintf (buf+k, sz, "[ ");
    PRINT_STEP;
  }
  pr = this;
  while (pr) {
    snprintf (buf+k, sz, "[");
    PRINT_STEP;
    for (int i=0; i < pr->dims; i++) {
      if (!expanded) {
	if (pr->r[i].u.ue.lo == NULL) {
	  sprint_expr (buf+k, sz, pr->r[i].u.ue.hi);
	  PRINT_STEP;
	}
	else {
	  sprint_expr (buf+k, sz, pr->r[i].u.ue.lo);
	  PRINT_STEP;
	  snprintf (buf+k, sz, "..");
	  PRINT_STEP;
	  sprint_expr (buf+k, sz, pr->r[i].u.ue.hi);
	  PRINT_STEP;
	}
      }
      else {
	if (pr->r[i].u.ex.isrange) {
	  if (pr->r[i].u.ex.lo == 0) {
	    snprintf (buf+k, sz, "%d", pr->r[i].u.ex.hi+1);
	    PRINT_STEP;
	  }
	  else {
	    snprintf (buf+k, sz, "%d..%d", pr->r[i].u.ex.lo, pr->r[i].u.ex.hi);
	    PRINT_STEP;
	  }
	}
	else {
	  snprintf (buf+k, sz, "%d", pr->r[i].u.ex.lo);
	  PRINT_STEP;
	}
      }
      if (i < pr->dims-1) {
	snprintf (buf+k, sz, ",");
	PRINT_STEP;
      }
    }
    snprintf (buf+k, sz, "]");
    PRINT_STEP;
    pr = pr->next;
  }
  if (this->next) {
    snprintf (buf+k, sz, " ]");
    PRINT_STEP;
  }
}

/*------------------------------------------------------------------------
 *  For arrays, counts the number of dimensions specified as [a..b]
 *  as opposed to [a]
 *------------------------------------------------------------------------
 */
int Array::effDims()
{
  int i;
  int count = 0;

  Assert (!expanded, "Not applicable to expanded arrays!");

  if (isSparse()) return nDims ();

  count = 0;
  for (i=0; i < dims; i++) {
    if (r[i].u.ue.lo == NULL) {
      /* skip */
    }
    else {
      count++;
    }
  }
  return count;
}


/*
 * @return the range of the d'th dimension of the array
 */
int Array::range_size(int d)
{
  int i;
  int count = 1;
  struct Array *a;

  Assert (expanded, "Only applicable to expanded arrays");
  Assert (0 <= d && d < dims, "Invalid dimension");
  Assert (!isSparse(), "Only applicable to dense arrays");

  return r[d].u.ex.hi - r[d].u.ex.lo + 1;
}

/*
 * Update range size
 */
void Array::update_range (int d, int lo, int hi)
{
  Assert (expanded, "Huh?");
  Assert (0 <= d && d < dims, "Invalid dimension");
  Assert (!isSparse(), "Only applicable to dense arrays");
  r[d].u.ex.lo = lo;
  r[d].u.ex.hi = hi;
  range_sz = -1;
}

/*
 * @return number of elements in the array
 */
int Array::size()
{
  int i;
  int count = 1;
  struct Array *a;

  Assert (expanded, "Only applicable to expanded arrays");

  if (range_sz != -1) {
    count = range_sz;
  }
  else {
    for (i=0; i < dims; i++) {
      if (r[i].u.ex.hi < r[i].u.ex.lo) {
	count = 0;
	break;
      }
      else {
	count = count*(r[i].u.ex.hi-r[i].u.ex.lo+1);
      }
    }
    range_sz = count;
  }
  if (next) {
    count += next->size();
  }
  return count;
}


/*------------------------------------------------------------------------
 *
 *  Array::Expand --
 *
 *   Return an expanded array
 *
 *------------------------------------------------------------------------
 */
Array *Array::Expand (ActNamespace *ns, Scope *s, int is_ref)
{
  Array *ret;
  
  if (expanded) {
    /* eh, why am I here anyway */
    act_error_ctxt (stderr);
    warning ("Not sure why Array::Expand() was called");
    return this;
  }

  ret = new Array();
  ret->expanded = 1;
  if (deref) {
    ret->deref = 1;
  }
  ret->dims = dims;

  Assert (dims > 0, "What on earth is going on...");
  MALLOC (ret->r, struct range, dims);

  int i;

  for (i=0; i < dims; i++) {
    Expr *hval, *lval;

    Assert (r[i].u.ue.hi, "Invalid array range");
    hval = expr_expand (r[i].u.ue.hi, ns, s);

#if 0
    fprintf (stderr, "expr: ");
    print_expr (stderr, r[i].u.ue.hi);
    fprintf (stderr, " -> ");
    print_expr(stderr, hval);
    fprintf (stderr, "\n");
#endif
    
    if (hval->type != E_INT) {
      act_error_ctxt (stderr);
      fatal_error ("Array range value is a non-integer/non-constant value");
    }
    
    if (r[i].u.ue.lo) {
      lval = expr_expand (r[i].u.ue.lo, ns, s);
      if (lval->type != E_INT) {
	act_error_ctxt (stderr);
	fatal_error ("Array range value is a non-integer/non-constant value");
      }
      ret->r[i].u.ex.lo = lval->u.v;
      ret->r[i].u.ex.hi = hval->u.v;
      ret->r[i].u.ex.isrange = 1;
      FREE (lval);
    }
    else {
      if (is_ref) {
	ret->r[i].u.ex.lo = hval->u.v;
	ret->r[i].u.ex.hi = hval->u.v;
	ret->r[i].u.ex.isrange = 0;
      }
      else {
	ret->r[i].u.ex.lo = 0;
	ret->r[i].u.ex.hi = hval->u.v - 1;
	ret->r[i].u.ex.isrange = 1;
      }
    }
    FREE (hval);
  }

  if (next) {
    ret->next = next->Expand (ns, s, is_ref);
  }

  ret->range_sz = -1;

  return ret;
}



/*------------------------------------------------------------------------
 *
 *  Array::stepper --
 *
 *   Returns something that can be used to step the array
 *
 *------------------------------------------------------------------------
 */
Arraystep *Array::stepper (Array *sub)
{
  if (!expanded) {
    fatal_error ("Array::stepper() called for an unexpanded array");
  }
  return new Arraystep (this, sub);
}


/*------------------------------------------------------------------------
 * Arraystep functions
 *------------------------------------------------------------------------
 */
Arraystep::Arraystep (Array *a, Array *sub)
{
  int offset;
  
  base = a;
  if (sub) {
    subrange = sub->Clone ();
  }
  else {
    subrange = NULL;
  }
  
  MALLOC (deref, int, base->dims);
  for (int i = 0; i < base->dims; i++) {
    if (subrange) {
      deref[i] = subrange->r[i].u.ex.lo;
    }
    else {
      deref[i] = base->r[i].u.ex.hi + 1;
    }
  }
  if (subrange) {
    /* compute base index */
    idx = base->Offset (deref);
  }
  else {
    idx = 0;
  }
}

Arraystep::~Arraystep()
{
  if (subrange) {
    delete subrange;
  }
  FREE (deref);
}

/*------------------------------------------------------------------------
 *
 *  Step the array index
 *
 *------------------------------------------------------------------------
 */
void Arraystep::step()
{
  if (!base) return;

  if (subrange) {
    /* ok, we need to do a step */
    for (int i = base->dims - 1; i >= 0; i--) {
      if (subrange->r[i].u.ex.lo == subrange->r[i].u.ex.hi)
	continue;
      deref[i]++;
      if (deref[i] <= subrange->r[i].u.ex.hi) {
	/* we're done */
	idx = base->Offset (deref);
	return;
      }
    }
    idx = -1;
    return;
  }

  /*-- not a subrange --*/
  
  for (int i = base->dims - 1; i >= 0; i--) {
    deref[i]++;
    if (deref[i] <= base->r[i].u.ex.hi) {
      /* we're done */
      idx++;
      return;
    }
    /* ok, we've wrapped. need to increase the next index */
    deref[i] = base->r[i].u.ex.lo;
  }

  /* we're here, we've overflowed this range */
  base = base->next;
  if (!base) {
    /* we're done! */
    idx = -1;
    return;
  }
  
  /* set the index to the lowest one in the next range */
  for (int i = 0; i < base->dims; i++) {
    deref[i] = base->r[i].u.ex.lo;
  }
  idx++;
}

/*------------------------------------------------------------------------
 *
 *  Am I at the end?
 *
 *------------------------------------------------------------------------
 */
int Arraystep::isend()
{
  if (base == NULL) return 1;
  else return 0;
}

/*------------------------------------------------------------------------
 *
 *  Arraystep::Print --
 *
 *   Print current index to the specified file
 *
 *------------------------------------------------------------------------
 */
void Arraystep::Print (FILE *fp)
{
  int i;
  
  fprintf (fp, "[");
  for (i=0; i < base->dims; i++) {
    if (i != 0) {
      fprintf (fp, ",");
    }
    fprintf (fp, "%d", deref[i]);
  }
  fprintf (fp, "]");
}


/*------------------------------------------------------------------------
 * Step through an AExpr. The assumption is that the AExpr is already
 * expanded
 *------------------------------------------------------------------------
 */
AExprstep::AExprstep (AExpr *a)
{
  stack = list_new ();
  cur = a;
  type = 0;
  step();
}


AExprstep::~AExprstep ()
{
  list_free (stack);
}

void AExprstep::step()
{
  switch (type) {
  case 0:
    break;
  case 1:
    /* nothing to see here, need to continue traversing the AExpr */
    FREE (u.const_expr);
    u.const_expr = NULL;
    break;
  case 2:
    /* check if we are out of identifiers */
    if (u.id.a && !u.id.a->isend()) {
      u.id.a->step();
      return;
    }
    if (u.id.a) {
      /* finished with the stepper */
      delete u.id.a;
    }
    break;
  case 3:
    if (u.vx.a && !u.vx.a->isend()) {
      u.vx.a->step();
      return;
    }
    if (u.vx.a) {
      delete u.vx.a;
    }
    break;
  default:
    fatal_error ("This is not possible");
    return;
  }

  /* here to get the next thing */
  type = 0;

  if (!cur) {
    if (stack_isempty (stack)) {
      /* nothing left */
      return;
    }
    cur = (AExpr *) stack_pop (stack);
  }

  while (cur) {
    switch (cur->t) {
    case AExpr::EXPR:
      {
	Expr *xe = (Expr *) cur->l;
	Assert (xe->type == E_VAR || expr_is_a_const (xe) ||
		xe->type == E_ARRAY || xe->type == E_SUBRANGE, "What?");
	if (expr_is_a_const (xe)) {
	  /* return the constant! */
	  type = 1;
	  u.const_expr = xe;
	  /* now advance the state */
	}
	else if (xe->type == E_VAR) {
	  /* now we need to figure out what this ID actually is */
	  InstType *it;
	  type = 2;
	  u.id.act_id = (ActId *)xe->u.e.l;
	  u.id.s = NULL;
	  u.id.a = NULL;
	  it = (InstType *)xe->u.e.r;

	  if (!it->arrayInfo() || /* type is not an array */
	      /* or type is an array, but we have a full deref */
	    (u.id.act_id->arrayInfo() && u.id.act_id->arrayInfo()->isDeref())) {
	    /* single variable */
	  }
	  else {
	    /* array reference */
	    if (!u.id.act_id->arrayInfo()) {
	      /* dense array */
	      u.id.a = it->arrayInfo()->stepper();
	    }
	    else {
	      /* sparse array */
	      u.id.a = it->arrayInfo()->stepper (u.id.act_id->arrayInfo());
	    }
	  }
	}
	else if (xe->type == E_ARRAY || xe->type == E_SUBRANGE) {
	  /* array slice */
	  type = 3;
	  u.vx.vx = (ValueIdx *) xe->u.e.l;
	  if (xe->type == E_ARRAY) {
	    u.vx.s = (Scope *) xe->u.e.r;
	    u.vx.a = u.vx.vx->t->arrayInfo()->stepper ();
	  }
	  else {
	    u.vx.s = (Scope *) xe->u.e.r->u.e.l;
	    u.vx.a = u.vx.vx->t->arrayInfo()->stepper ((Array *)xe->u.e.r->u.e.r);
	  }
	}
	else {
	  Assert (0," Should not be here ");
	}
	/* we've consumed cur */
	cur = NULL;
	return;
      }
      break;

    case AExpr::CONCAT:
    case AExpr::COMMA:
      if (cur->r) {
	stack_push (stack, cur->r);
      }
      cur = cur->l;
      break;
    default:
      fatal_error ("Huh");
      break;
    }
  }
}

int AExprstep::isend()
{
  if (type == 0 && !cur && stack_isempty (stack)) {
    return 1;
  }
  return 0;
}


unsigned long AExprstep::getPInt()
{
  unsigned long v;
  Assert (type != 0, "AExprstep::getPInt() called without step or on end");

  v = 0;
  switch (type) {
  case 1:
    Assert (u.const_expr->type == E_INT, "Typechecking...");
    v = u.const_expr->u.v;
    break;

  case 2:
    Assert (0, "getPInt() called, but looks like a raw identifier");
    break;
    
  case 3:
    Assert (u.vx.s->issetPInt (u.vx.vx->u.idx + u.vx.a->index()), "Should have been caught earlier");
    return u.vx.s->getPInt (u.vx.vx->u.idx + u.vx.a->index());
    break;

  default:
    fatal_error ("Hmm");
    break;
  }
  return v;
}

long AExprstep::getPInts()
{
  long v;
  Assert (type != 0, "AExprstep::getPInts() called without step or on end");

  v = 0;
  switch (type) {
  case 1:
    Assert (u.const_expr->type == E_INT, "Typechecking...");
    v = u.const_expr->u.v;
    break;

  case 2:
    Assert (0, "getPInts() called, but looks like a raw identifier");
    break;
    
  case 3:
    Assert (u.vx.s->issetPInts (u.vx.vx->u.idx + u.vx.a->index()), "Should have been caught earlier");
    return u.vx.s->getPInts (u.vx.vx->u.idx + u.vx.a->index());
    break;

  default:
    fatal_error ("Hmm");
    break;
  }
  return v;
}

double AExprstep::getPReal()
{
  double v;
  Assert (type != 0, "AExprstep::getPReal() called without step or on end");

  v = 0;
  switch (type) {
  case 1:
    Assert (u.const_expr->type == E_REAL, "Typechecking...");
    v = u.const_expr->u.f;
    break;

  case 2:
    Assert (0, "getPReal() called, but looks like a raw identifier");
    break;
    
  case 3:
    Assert (u.vx.s->issetPReal (u.vx.vx->u.idx + u.vx.a->index()), "Should have been caught earlier");
    return u.vx.s->getPReal (u.vx.vx->u.idx + u.vx.a->index());
    break;

  default:
    fatal_error ("Hmm");
    break;
  }
  return v;
}


int AExprstep::getPBool()
{
  int v;
  Assert (type != 0, "AExprstep::getPBool() called without step or on end");

  v = 0;
  switch (type) {
  case 1:
    Assert (u.const_expr->type == E_TRUE || u.const_expr->type == E_FALSE, "Typechecking...");
    v = (u.const_expr->type == E_TRUE ? 1 : 0);
    break;

  case 2:
    Assert (0, "getPBool() called, but looks like a raw identifier");
    break;
    
  case 3:
    Assert (u.vx.s->issetPBool (u.vx.vx->u.idx + u.vx.a->index()), "Should have been caught earlier");
    return u.vx.s->getPBool (u.vx.vx->u.idx + u.vx.a->index());
    break;

  default:
    fatal_error ("Hmm");
    break;
  }
  return v;
}



InstType *AExprstep::getPType()
{
  InstType *v;
  Assert (type != 0, "AExprstep::getPType() called without step or on end");

  v = NULL;
  switch (type) {
  case 1:
    Assert (u.const_expr->type == E_TYPE, "Typechecking...");
    v = (InstType *) u.const_expr->u.e.l;
    break;

  case 2:
    Assert (0, "getPType() called, but looks like a raw identifier");
    break;
    
  case 3:
    Assert (0, "No ptype arrays yet");
    break;

  default:
    fatal_error ("Hmm");
    break;
  }
  return v;
}



int Array::overlapping (struct range *a, struct range *b)
{
  for (int i=0; i < dims; i++) {
    if (a[i].u.ex.lo > b[i].u.ex.hi) return 0;
    if (a[i].u.ex.hi < b[i].u.ex.lo) return 0;
  }
  return 1;
}


/*
  from 0..idx-1, ranges are overlapping perfectly. so keep those
  indicies intact.

  at idx, there is overlap.
*/
#define MIN(a,b) ((a) < (b) ? (a) : (b))

void Array::dumprange (struct range *r)
{
  int i;
  
  fprintf (stderr, "[");
  for (i=0; i < dims; i++) {
    if (i != 0) fprintf (stderr, ",");
    fprintf (stderr, "%d..%d", r[i].u.ex.lo, r[i].u.ex.hi);
  }
  fprintf (stderr, "]");
}
  

void Array::_merge_range (int idx, Array *prev, struct range *mr)
{
#if 0
  fprintf (stderr, "Merging: ");
  Print (stderr);
  fprintf (stderr, " with: ");
  dumprange (mr);
  fprintf (stderr, "\n");
#endif
  
 start: /* basically a tail recursion */
  for (; idx < dims; idx++) {
    if ((r[idx].u.ex.lo != mr[idx].u.ex.lo) ||
	(r[idx].u.ex.hi != mr[idx].u.ex.hi))
      break;
  }
  if (idx == dims) {
    FREE (mr);
    return;
  }
  
  /* YYY: write this 

     [a..b] [c..d]

     if a is the min: (if c is the min, swap the two)

     (A)   a..c-1 c..d d+1..b
        OR
     (B)   a..c-1 c..b b+1..d

     INSERT
     (1) [a..c-1] + [rest dims of LHS] inserted

     (A) INSERT
       (2) [c..d] + {{recursive insert of rest of the dims}}
       (3) [d+1..b] + [rest dims of LHS]

     (B) INSERT
       (2) [c..b] + {{recursive insert of rest of the dims}}
       (3) [b+1..d] + [rest of dims of RHS]
  */
  if (mr[idx].u.ex.lo < r[idx].u.ex.lo) {
    struct range *tr;
    /* swap mr and r */
    tr = r;
    r = mr;
    mr = tr;
  }

  Array *tmp;

#if 0
  fprintf (stderr, "Index=%d\n", idx);
  fprintf (stderr, "lhs=");
  dumprange (r);
  fprintf (stderr, "; rhs=");
  dumprange (mr);
  fprintf (stderr, "\n");
#endif  

  while (1) {
    /* if the ranges are now disjoint, we can just insert and quit */
    if (r[idx].u.ex.hi < mr[idx].u.ex.lo) {
      tmp = CloneOne();
      for (int i=0; i < dims; i++) {
	tmp->r[i] = mr[i];
      }
      tmp->next = next;
      next = tmp;
      FREE (mr);
      return;
    }
    
    if (r[idx].u.ex.lo < mr[idx].u.ex.lo) {
      /* range split, and re-try */
      tmp = CloneOne();
      Assert (r[idx].u.ex.hi >= mr[idx].u.ex.lo, "Eh?");
      r[idx].u.ex.hi = mr[idx].u.ex.lo-1;
      tmp->r[idx].u.ex.lo = mr[idx].u.ex.lo;
      tmp->next = next;
      next = tmp;
      FREE (mr);
      continue;
    }
    else if (r[idx].u.ex.lo == mr[idx].u.ex.lo) {
      /* find the shared part of this dimension */
      if (r[idx].u.ex.hi < mr[idx].u.ex.hi) {
	tmp = CloneOne();
	for (int i=0; i < dims; i++) {
	  tmp->r[i] = mr[i];
	}
	tmp->r[idx].u.ex.lo = r[idx].u.ex.hi+1;
	tmp->next = next;
	next = tmp;
	mr[idx].u.ex.hi = r[idx].u.ex.hi;
	goto start;
      }
      else if (mr[idx].u.ex.hi < r[idx].u.ex.hi) {
	tmp = CloneOne();
	tmp->r[idx].u.ex.lo = mr[idx].u.ex.hi+1;
	tmp->next = next;
	next = tmp;
	r[idx].u.ex.hi = mr[idx].u.ex.hi;
	goto start;
      }
      else {
	Assert (0, "Should not be here");
      }
    }
    else if (r[idx].u.ex.lo > mr[idx].u.ex.lo) {
      Assert (0, "This should have been fixed earlier?");
    }
  }
  FREE (mr);
  return;
}

void Array::Merge (Array *a)
{
  Array *tmp, *prev;
  int i;

  Assert (expanded && a->isExpanded(), "Needs to be expanded!");

  if (dims != a->nDims()) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Sparse array extension has incompatible dimensions\n");
    fprintf (stderr, " Original: ");
    Print (stderr);
    fprintf (stderr, "; extension: ");
    a->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }

  Assert (!a->isSparse(), "Can only merge a dense range into a sparse array");

  for (tmp = this; tmp; tmp = tmp->next) {
    if (overlapping (tmp->r, a->r)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Sparse array: overlap in range in instantiation\n");
      fprintf (stderr, "  Original: ");
      Print (stderr);
      fprintf (stderr, "; adding: ");
      a->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  
  prev = NULL;
  tmp = this;
  /* 
     invariant:
        for all items in the range list upto the current one 
	so far have indices < the lowest index in a's index set
  */

  do {
    int adjacent;
    int idx;

    adjacent = 0;
    idx = -1;

    /* check special case */
    for (i=0; i < dims; i++) {
      if (a->r[i].u.ex.lo == tmp->r[i].u.ex.lo &&
	  a->r[i].u.ex.hi == tmp->r[i].u.ex.hi) continue;
      if (adjacent == 0) {
	if (a->r[i].u.ex.lo == (tmp->r[i].u.ex.hi + 1)) {
	  adjacent = 1;
	  idx = i;
	}
	else if (tmp->r[i].u.ex.lo == (a->r[i].u.ex.hi+1)) {
	  adjacent = -1;
	  idx = i;
	}
	else {
	  break;
	}
      }
      else {
	break;
      }
    }
    if (i == dims) {
      Assert (adjacent != 0, "Should have been caught earlier!");
      /* simple concatenation */
      if (adjacent == 1) {
	tmp->r[idx].u.ex.hi = a->r[idx].u.ex.hi;
      }
      else {
	Assert (adjacent == -1, "hmm");
	tmp->r[idx].u.ex.lo = a->r[idx].u.ex.lo;
      }
      tmp->range_sz = -1;
      return;
    }

    /*

     First, compute overlap with first range:
     - if empty, then figure out if this range is before or after
     --> insert
     done

     Otherwise: split range
    */
    for (i=0; i < dims; i++) {
      if (a->r[i].u.ex.lo != tmp->r[i].u.ex.lo ||
	  a->r[i].u.ex.hi != tmp->r[i].u.ex.hi)
	break;
    }

    Assert (i != dims, "What?");
    
    if (a->hi(i) < tmp->lo(i)) {
      Array *m = a->Clone();
      /* insert to the left, done */
      if (!prev) {
	struct range *rx;       
	/* swap the range pointers! */
	rx = tmp->r;
	tmp->r = m->r;
	m->r = rx;
	
	m->next = next;
	next = m;
	range_sz = -1;
      }
      else {
	m->next = tmp;
	prev->next = m;
	range_sz = -1;
      }
      return;
    }
    else if (a->lo(i) > hi(i)) {
      /* go right */
      prev = tmp;
      tmp = tmp->next;
    }
    else {
      struct range *tmpr;
      MALLOC (tmpr, struct range, dims);
      for (int j=0; j < dims; j++) {
	tmpr[j] = a->r[j];
      }
      tmp->_merge_range (i, prev, tmpr);
#if 0      
      fprintf (stderr, "After merge: ");
      Print (stderr);
      fprintf (stderr, "\n");
#endif      
      return;
    }
  } while (tmp);

  /* otherwise it goes on the end */
  /* insert to the right */
  prev->next = a->Clone();
}


ActId *AExpr::toid ()
{
  Expr *e;
  Assert (t == AExpr::EXPR, "What?");
  e = (Expr *)l;
  Assert (e && e->type == E_VAR, "What?");
  return (ActId *)e->u.e.l;
}
