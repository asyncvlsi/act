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
#ifndef __ACT_ARRAY_H__
#define __ACT_ARRAY_H__

#include <act/expr.h>
#include "list.h"

class ActNamespace;
class Scope;
class Arraystep;
class ActId;
class InstType;
class ValueIdx;

/**
 *
 *  Dense arrays, sparse arrays, and array dereferences
 *
 *  Used to represent a set of dense ranges/derefs
 *  If a sub-range specifier is not used, then the "deref" field is
 *  set to 1---it means the user wrote [x] rather than [x..y] for every
 *  dimension of the array.
 *
 */
class Array {
 public:
  /**
   * \return 1 if this is a sparse array specification, 0 otherwise
   */
  int isSparse() { return next == NULL ? 0 : 1; }

  /**
   * \return 1 if this is an array dereference, rather than a
   * sub-array specification
   */
  int isDeref() { return deref; }

  /**
   * Force this to be an array, not a deref. Used to override type
   * specifier for compact foo f[n] which is shorthand for foo[0..n-1]
   */
  void mkArray() { deref = 0; }

  /**
   * \return the number of dimensions of this array/sub-array/dereference
   */
  int nDims() { return dims; }

  /**
   * \return the effective number of dimensions, taking partial
   * de-references into account. If this is a sparse array, it is just
   * nDims()
   */
  int effDims();

  /**
   * Append array dimensions from the argument into the current
   * array. The current array must be dense.
   *
   * @param a contains the arguments to be merged, which must be a
   * dense array
   *  
   * Updates the current array with the additional dimensions from a
   */
  void Concat (Array *a);

  /**
   * Same as Concat(), except append dimensions starting from position
   * 0, rather than adding to the dimensions at the end
   */
  void preConcat (Array *a);

  /**
   * Check if two array derefes are identical
   * @param strict is 1 if the low and high indices must match; 0
   * means the sizes must match.
   *
   */
  int isEqual (Array *a, int strict);

  /**
   * Check if two arrays have compatible parameters (matching
   * dimensions)
   */
  int isDimCompatible (Array *a);

  /**
   * Merge a second array into this one. Used to construct sparse
   * array types. Both must be expanded.
   */
  void Merge (Array *a);

  /**
   * Create a new one-dimensional dense array.
   * @param e is an integer expression corresponding to the size of
   * the array
   * @param f is an integer expression. If f is not NULL, then the
   * array range is e..f; otherwise it is 0..(e-1).
   */
  Array (Expr *e, Expr *f = NULL);
  Array (int lo, int hi);

  ~Array ();

  void Print (FILE *fp, int style = 0);  // style = 0: [num][num];
					 // otherwise [num,num]
  void sPrint (char *buf, int sz, int style = 0);

  int sPrintOne (char *buf, int sz, int style = 0);
  void PrintOne (FILE *fp, int style = 0);
  Array *Next() { return next; }

  Array *Clone ();		/* returns a deep copy */
  Array *CloneOne ();		/* only copy current range */
  Array *Reduce();		// return a deep copy, but elide
				// dimensions that are derefs

  int isExpanded() { return expanded; }	
  /* returns 1 if expanded array, 0 otherwise */

  int size(); /**< returns total number of elements */
  int range_size(int d); /**< returns size of a particular dimension */
  void update_range (int d, int lo, int hi); /**< set range */
  int isrange (int d) { return r[d].u.ex.isrange; }

  /* only for unexpanded ranges */
  Expr *lo (int d) { return r[d].u.ue.lo; }
  Expr *hi (int d) { return r[d].u.ue.hi; }

  /**< return expanded array, is_ref: 1 if it is on the rhs of an ID,
        0 if it is for a type */
  Array *Expand (ActNamespace *ns, Scope *s, int is_ref = 0);
  Array *ExpandRef (ActNamespace *ns, Scope *s) { return Expand (ns, s, 1); }


  int Validate (Array *a);	// check that the array deref is a
				// valid deref for the array!


  int Offset (Array *a);	// return the offset within the array
				// for deref a, -1 if there isn't one.
  int Offset (int *a);

  Array *unOffset (int offset);	// returns array deref corresponding
				// to the offset

  /*
   * Stepper/iterator functionality
   */
  Arraystep *stepper(Array *sub = NULL); // returns an "iterator" for this
				// array. why not call it an iterator?
				// because it isn't...
				// sub = subrange within this array


private:
  Array ();			/* for deep copy only */

  int in_range (Array *a);	// offset within a range, -1 if
				// missing
  int in_range (int *a);	// same thing

  int dims;			/**< number of dimensions */

  /* this is */
  struct range {
    union {
      struct {
	Expr *lo, *hi;
      } ue;			/* unexpanded */
      struct {
	unsigned int isrange:1;	// it is a range... need this info, sadly.
	int lo, hi;	       
      } ex;			/* expanded.
				   when an array is attached to an
				   ActId, this is the correct value;
				   in other contexts you have things
				   like 0..val-1
				*/

    } u;
  } *r;				/**< range for each dimension */


  void dumprange (struct range *r);
  void _merge_range (int idx, Array *prev, struct range *m);
  int overlapping (struct range *a, struct range *b);


  unsigned int range_sz;	/**< cache: size of the range; only
				   for expanded arrays */

  Array *next;			/**< for sparse arrays */
  unsigned int deref:1;		/**< 1 if this is a dereference, 0
				   otherwise */
  unsigned int expanded:1;	/**< 1 if this is expanded, 0
				   otherwise */

  friend class Arraystep;
};

/*
 * Class for stepping through an array
 */
class Arraystep {
public:
  Arraystep (Array *a, Array *sub = NULL);
  ~Arraystep ();
  void step();
  int index() { return idx; }
  int index(Array *b) { return b->Offset (deref); }
  int isend();		// returns 1 on an end of array, 0 otherwise

  int typesize() { return base->size(); }

  char *string(int style = 0);
  Array *toArray();

  void Print (FILE *fp, int style = 0);
  
private:
  int idx;
  int *deref;
  Array *base;
  Array *subrange;		// subrange, if any
};


/**
 * Array expressions
 *
 */
class AExprstep;

class AExpr {
 public:
  /**
   * An array expression is either a basic expression, a concatenation,
   * or a comma
   */
  enum type {
    CONCAT, COMMA, /*SUBRANGE,*/ EXPR
  };
  
  AExpr (ActId *e);
  AExpr (Expr *e);
  AExpr (type t, AExpr *l, AExpr *r);
  ~AExpr();

  int isEqual (AExpr *a); /**< Check if two array expressions are
			     equal */

  AExpr *GetLeft () { return l; }
  AExpr *GetRight () { return r; }
  void SetRight (AExpr *a) { r = a; }
  
  void Print (FILE *fp);
  void sPrint (char *buf, int sz);

  AExpr *Clone ();

  InstType *getInstType (Scope *, int *islocal, int expanded = 0);

  AExpr *Expand (ActNamespace *, Scope *, int is_lval = 0);
  /**< expand out all parameters */

  ActId *toid ();

  int isBase() { return t == EXPR ? 1 : 0; }

  AExprstep *stepper();  /* return stepper! */

 private:
  enum type t;
  AExpr *l, *r;

  friend class AExprstep;
};
 
/*
 * Class for stepping through an array expr
 */
class AExprstep {
public:
  AExprstep (AExpr *a);
  ~AExprstep ();
  void step();
  int isend();		  // returns 1 on an end of array, 0 otherwise

  /* get a value */
  unsigned long getPInt();
  long getPInts();
  double getPReal();
  int getPBool();
  InstType *getPType();

  /* get an identifier */
  void getID (ActId **id, int *idx, int *typesize);
  void getsimpleID (ActId **id, int *idx, int *typesize);

  int isSimpleID ();

  /* XXX: later */
  void Print (FILE *fp);
  
private:
  list_t *stack;		// stack of AExprs
  AExpr *cur;

  union {
    Expr *const_expr;		// current constant expression, or:
    struct {
      /* this is used for non-parameter identifiers */
      ActId *act_id;		// identifier
      Arraystep *a;		// array deref within the id, in case
				// it is an array
      int issimple:1;		// 1 if this is a raw id
    } id;
    struct {
      /* this is used for arrays of parameters */
      ValueIdx *vx;		// base value idx
      Scope *s;			// scope in which to evaluate
      Arraystep *a;		// if it is an array or subrange
    } vx;
  } u;
  unsigned int type:2;		// 0 = none, 1 = const, 2 = id,
				// 3 = value array
  
};





#endif /* __ACT_ARRAY_H__ */
