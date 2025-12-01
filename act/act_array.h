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
#include <common/list.h>

class ActNamespace;
class Scope;
class Arraystep;
class ActId;
class InstType;
class ValueIdx;
class expr_pstruct;

/**
 *  @class Array
 *
 *  @brief Dense arrays, sparse arrays, and array dereferences
 *
 *  Used to represent a set of dense ranges/derefs If a sub-range
 *  specifier is not used, then the "deref" field is set to 1---it
 *  means the user wrote [x] rather than [x..y] for every dimension of
 *  the array.
 *
 *  This class is also used to represent a sparse array. In this case,
 *  the array is represented as a list of dense sub-arrays. The next
 *  field is used to build the linked list of dense components of the
 *  sparse array.
 *
 *  This class is also used to represent a "de-reference" as part of
 *  an identifier. De-references are either constant values, or may
 *  contain a variable (e.g. a de-reference within a CHP
 *  body). De-references with variables are called "dynamic", because
 *  their value cannot be determined statically.
 *
 *  The elements within an array are also numbered sequentially. This
 *  provides a linear addressing mechanism for the array elements,
 *  making it simple to write loops that iterate over the array. The
 *  numbers are from 0 to size()-1. This numbering can be used once
 *  the array is expanded for arrays that are not de-references.
 */
class Array {

  /**
   * @brief A range specifier that can include parameters/etc. This is
   * used during parsing, prior to expansion.
   *
   * An array specifier for a single dimension consists of a
   * pair of expressions, one for the low end of the range and one for
   * the high end of the range. 
   *
   * The lo field is NULL if it is a simple dense array
   * specifier. This case corresponds to the range being 0 to hi-1.
   * If lo is non-NULL, then the range corresponds to lo to hi, both
   * inclusive.
   */
  struct _act_array_expr_pair {
    Expr *lo, *hi;
  };

  /**
   * @brief A range specifier post expansion where all
   * parameters/etc. are substituted, resulting in a constant range.
   *
   * An array specifier for a single dimension consists of a
   * pair of constants, one for the low end of the range and one for
   * the high end of the range. 
   *
   * The range corresponds to lo .. hi, both inclusive.
   */
  struct _act_array_exprex_pair {
    long lo, hi;
  };

  /**
   * @brief Used to hold the result of an expanded array range
   * reference. 
   */
  struct _act_array_internal {
    unsigned int isrange:2;	///< it is a range... need this info.
				///  0 =  deref, 1 = range, 2 = dynamic
    union {
      struct _act_array_exprex_pair idx; ///< for case 0 and 1
      Expr *deref; ///< for case 2, dynamic de-reference
    };
  };


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
   * Updates the current array with the additional dimensions from
   * parameter a
   */
  void Concat (Array *a);

  /**
   * Same as Concat(), except append dimensions starting from position
   * 0, rather than adding to the dimensions at the end
   * @param a contains the argument to be merged into the current array
   */
  void preConcat (Array *a);

  /**
   * Check if two array derefs are identical. If the arrays are not
   * expanded, then the syntax of the expressions are checked in a
   * strict manner. If the arrays are expanded, then the values are
   * checked.
   *
   * @param a is the other array for comparison
   * @param strict is 1 if the low and high indices must match; 0
   * means the sizes must match.
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
   *
   * @param a is the other array that has to be combined with the
   * current one.
   */
  void Merge (Array *a);

  /**
   * Create a new one-dimensional dense unexpanded array.
   * @param e is an integer expression corresponding to the size of
   * the array
   * @param f is an integer expression. If f is not NULL, then the
   * array range is e..f; otherwise it is 0..(e-1).
   */
  Array (Expr *e, Expr *f = NULL);

  /**
   * Create a new one-dimensional dense expanded array.
   * @param lo is the low end of the range
   * @param hi is the high end of the range
   */
  Array (int lo, int hi);

  /**
   * Create a de-reference corresponding to the specified index
   * @param idx is the index of the de-reference
   */
  Array (int idx);

  ~Array ();

  /**
   * Print the array out to the specified file. Arrays can be printed
   * in two styles:  [num][num]  (style=0), or [num,num] (style=1).
   *
   * @param fp is the output file
   * @param style is the printing style
   */
  void Print (FILE *fp, int style = 0);

  /**
   * Similar to Print(), but writes the output to a buffer.
   *
   * @param buf is the output buffer
   * @param sz is the size of the output buffer
   * @param style is the printing style
   */
  void sPrint (char *buf, int sz, int style = 0);

  /**
   * This prints out the first dense component of a sparse array.
   * @param buf is the output buffer
   * @param sz is the size of the output buffer
   * @param style is the printing style
   * @return the number of characters of the buffer that were used
   */
  int sPrintOne (char *buf, int sz, int style = 0);

  /**
   * This is similar to sPrintOne(), except it prints the output to a
   * file.
   * @param fp is the output file
   * @param style is the printing style
   */
  void PrintOne (FILE *fp, int style = 0);

  /**
   * @return the next section of the sparse array, NULL if there isn't
   * one.
   */
  Array *Next() { return next; }

  Array *Clone (ActNamespace *orig = NULL,
		ActNamespace *newns = NULL); ///< returns a deep copy of the entire array
  void moveNS (ActNamespace *orig = NULL,
	       ActNamespace *newns = NULL); ///< moves to new ns

  Array *CloneOne ();		///< only return a deep copy of the
				///current dense range

  Array *fixGlobalParams (ActNamespace *cur, ActNamespace *orig);
  

  Array *Reduce();		///< return a deep copy, but elide
				///dimensions that are derefs

  int isExpanded() { return expanded; }	
  /**< returns 1 if this is an expanded array, 0 otherwise */

  int size(); /**< returns total number of elements. Only works for
		 expanded arrays */
  
  int range_size(int d); /**< returns size of a particular
			    dimension. Only works for expanded arrays. */

  /**
   * Set the range for dimension d to lo .. hi. Only works for
   * expanded arrays.
   * @param d is the dimension of the range to be updated
   * @param lo is the new low range
   * @param hi is the new high range
   */
  void update_range (int d, int lo, int hi);

  /**
   * @return 1 if dimension d is in fact a range, 0 otherwise
   */
  int isrange (int d) { return (r[d].u.ex.isrange == 1); }

  /**
   * Returns the lo expression for dimension d
   * This only works for unexpanded arrays.
   * @param d is the dimension of interest
   */
  Expr *lo (int d) { return r[d].u.ue.lo; }
  
  /**
   * Returns the hi expression for dimension d
   * This only works for unexpanded arrays.
   * @param d is the dimension of interest
   */
  Expr *hi (int d) { return r[d].u.ue.hi; }

  /**
   * Return expanded array.
   *
   * @param ns is the namespace
   * @param s is the scope for evaluation
   * @param is_ref 1 if it is on the rhs of an ID, 0 if it is for a type.
   */
  Array *Expand (ActNamespace *ns, Scope *s, int is_ref = 0);

  /**
   * Returns an expanded array for an Array that is part of an
   * ActId. This simply calls Expand() with is_ref set to 1.
   * @param ns is the namespace 
   * @param s is the evaluation scope
   * @return new expanded array
   */
  Array *ExpandRef (ActNamespace *ns, Scope *s) { return Expand (ns, s, 1); }

  /**
   * Returns an expanded array for an Array that is part of an ActId
   * in a CHP body. This permits dynamic references.
   * @param ns is the namespace 
   * @param s is the evaluation scope
   * @return the expanded array
   */
  Array *ExpandRefCHP (ActNamespace *ns, Scope *s);

  /**
   * Validate that the array indices specified in the argument are
   * valid indicies for the array. This must be called on an expanded
   * array.
   *
   * @param a is the Array that contains the indices of interest
   * @return 1 if the de-reference is valid, 0 otherwise
   */
  int Validate (Array *a);

  /**
   * Validate that the array indices specified in the argument are
   * valid indicies for the array. This must be called on an expanded
   * array. Skip any dynamic references.
   *
   * @param a is the Array that contains the indices of interest
   * @return 1 if the de-reference is valid, 0 otherwise
   */
  int weakValidate (Array *a);

  /**
   * Checks to see if this array includes a dynamic de-reference
   *
   * @return 1 if this is a dynamic de-reference, 0 otherwise
   */
  int isDynamicDeref ();
  int isDynamicDeref (int idx);

  /**
   * Must be called on an expanded array that is in fact an array
   * dereference.
   * @param idx is the dimension of the array of interest
   * @return the Expr corresponding to the de-reference at the
   * specified dimension
   */
  Expr *getDeref (int idx);

  /**
   * Must be called on an expanded array that is in fact an array
   * dereference, and where the de-reference is non-const
   * @param idx is the dimension of the array of interest
   * @param e is the updated expression for the de-reference
   */
  void setDeref (int idx, Expr *e);

  /**
   * Given an array deference or subrange, returns the offset for the
   * first element of the sub-range/dereference in the standard linear
   * numbering of the elements of the array
   *
   * @param a is the de-reference/sub-range specifier to look for
   * @return the integer offset in the array numbering, or -1 if the
   * specified array reference is out of range
   */
  int Offset (Array *a);

  /**
   * Same as the other Offset() function, except the de-reference
   * corresponds to a set of constant indices.
   * @param a is the de-reference index
   * @return -1 if not found, or the offset in the linear numbering of
   * the array
   */
  int Offset (int *a);

  /**
   * Convert a linear offset into an array into the corresponding
   * array de-reference
   * @param offset is the offset for the element of interest
   * @return the array de-reference corresponding to the offset
   */
  Array *unOffset (int offset);

  /**
   * Stepper/iterator functionality. This returns an array "stepper"
   * that allows you to step through the elements of the array. If a
   * sub-range specifier is included, then the stepper only steps
   * through the subrange within the array.
   * @param sub is the sub-range specifier, NULL if not needed
   * @return An Arraystep class that can be used to step through the
   * elements of the array.
   */
  Arraystep *stepper(Array *sub = NULL);


  /**
   * Helper for hashing
   */
  unsigned int getHash (unsigned int prev, unsigned long sz);

private:
  Array ();			/**< for deep copy only */

  int in_range (Array *a);	///< offset within a range, -1 if missing
  int in_range (int *a);	///< offset within a range, -1 if missing

  int dims;			/**< number of dimensions */

  /**
   * This holds the range for a particular dimension. It can be either
   * an expanded or unexpanded range
   */
  struct range {
    union {
      struct _act_array_expr_pair ue; ///< unexanded range
      struct _act_array_internal ex;  ///< expanded range
    } u;
  } *r;				/**< range for each dimension */

  /**
   * When we have an array of non-strict user-defined processes, each
   * range entry can change the InstType field in the expanded array
   */
  InstType *_ex_new_nonstrict;

  void dumprange (struct range *r); /**< used for debugging */

  /**
   * Used to merge a range into the current array. This is the ordered
   * merge algorithm used to have a canonical representation of a
   * sparse array as a collection of dense blocks.
   *
   * @param idx is the dimension to be merged
   * @param prev is the previous (if any) pointer for the array
   * @param m is the new range to be merged in
   */
  void _merge_range (int idx, Array *prev, struct range *m);

  /**
   * Checks if two ranges have a non-empty intersection
   * @param a is one of the two ranges
   * @param b is one of the two ranges
   * @return 1 if the ranges are overlapping, 0 otherwise
   */
  int overlapping (struct range *a, struct range *b);


  unsigned int range_sz;	/**< cache: size of the range; only
				   for expanded arrays */

  Array *next;			/**< for sparse arrays, used to link
				   together dense blocks for each
				   component of the sparse array. */
  
  unsigned int deref:1;		/**< 1 if this is a dereference, 0
				   otherwise */
  
  unsigned int expanded:1;	/**< 1 if this is expanded, 0
				   otherwise */

  unsigned int refcnt;

  static struct cHashtable *arrH; /**< hash table used to canonicalize
				     array pointers after expansion */

  friend class Arraystep;
};

/**
 * @class Arraystep
 *
 * @brief Class for stepping through an array.
 *
 * This class is used for an ordered traversal of an array/sub-range
 * of an array. Given an Array, we can use this in the following
 * fashion:
 *
 * ```
 * Array *a = ...
 * 
 * Arraystep *as = a->stepper(); // to step through the entire array
 * 
 * while (!as->isend()) {
 *   int idx = as->index(); // the index of the element
 *   ...
 *   Array *elem = as->toArray(); // the array de-reference for the element
 *   ...
 *   as->step(); // advance the stepper
 * }
 * delete as; // done with stepper
 * ```
 */
class Arraystep {
public:
  /**
   * Construct a stepper for the array a, with an optional sub-range
   * specifier sub
   * @param a the array to be stepped through
   * @param sub is an optional sub-range of the array a of interest
   */
  Arraystep (Array *a, Array *sub = NULL);
  
  ~Arraystep ();
  
  void step(); ///< advance to the next element in the array
  
  int index() { return idx; } ///< the linear index of the current
			      ///element in the array being stepped through
  
  int index(Array *b) { return b->Offset (deref); } ///< the index of
						    ///the current
						    ///de-reference
						    ///within array b
  
  int isend();		///< @return 1 on an end of array, 0 otherwise

  int typesize() { return base->size(); } ///< @return the size of the
					  ///array being stepped through

  char *string(int style = 0); ///< @return a freshly allocated string
			       ///corresponding to the current
			       ///de-reference.
  
  Array *toArray(); ///< convert the current element de-reference into
		    ///an Array class.

  /**
   * Print the current array de-reference to the specified file
   * @param fp is the output file
   * @param style is the array printing style
   */
  void Print (FILE *fp, int style = 0);
  
private:
  int idx;  ///< used to track the current index
  int *deref; ///< used to track the current array de-reference
  Array *base; ///< the base array 
  Array *subrange;		///< the subrange, if any
  Array *insubrange;		///< part of subrange walker state
};


class AExprstep;

/**
 * @class AExpr
 *
 * @brief Array expressions.
 *
 * This is used to hold the array expression syntax for ACT.
 */
class AExpr {
 public:
  /**
   * An array expression is either a basic expression, a concatenation,
   * or a comma
   */
  enum type {
    CONCAT, COMMA, /*SUBRANGE,*/ EXPR
  };

  AExpr (ActId *e); ///< return an array expression corresponding to
		    ///the ID
  
  AExpr (Expr *e); ///< return an array expression corresponding to an
		   ///Expr
  
  AExpr (type t, AExpr *l, AExpr *r); ///< combine two array
				      ///expressions into a new one
				      ///based on the specified type
  
  ~AExpr();

  int isEqual (AExpr *a); /**< Check if two array expressions are
			     equal */

  AExpr *GetLeft () { return l; } ///< access left component
  AExpr *GetRight () { return r; } ///< access right component
  
  void SetRight (AExpr *a) { r = a; } ///< assign right component to a
  
  void Print (FILE *fp);  ///< print array expression
  void sPrint (char *buf, int sz); ///< print array expression to string

  AExpr *Clone (ActNamespace *orig = NULL, ActNamespace *newns = NULL); ///< deep copy of array expression

  AExpr *fixGlobalParams (ActNamespace *cur, ActNamespace *orig);

  
  /**
   * Return an InstType corresponding to this array expression
   * @param s is the scope
   * @param islocal if non-NULL, used to return 1 if this array
   * expression only has local ID references, 0 otherwise
   * @param expanded is used if the instance type should expect
   * expanded types. In this case the correct array dimensions are
   * computed.
   * @return the newly allocated InstType corresponding to the type of
   * the array expression.
   */
  InstType *getInstType (Scope *s, int *islocal, int expanded = 0);

  /**
   * @returns true if the array expression is strict
   */
  bool getStrictFlag (Scope *s);

  AExpr *Expand (ActNamespace *, Scope *, int is_lval = 0);
  /**< expand out all parameters */

  /**
   * For this to work, the array expression must correspond to a
   * single ActId only.
   * @return the ActId corresponding to this array expression
   */
  ActId *toid ();

  /**
   * @return 1 if this is the "base case" for an array expression
   * (i.e. a simple expression), 0 otherwise
   */
  int isBase() { return t == EXPR ? 1 : 0; }

  /**
   * Array expressions can be quite complex. This returns an array
   * expression stepper (similar to Arraystep) that walks through the
   * components of the array expression element by element.
   * @return the array expression stepper that can walk through this
   * array expression.
   */
  AExprstep *stepper();

  InstType *isType (); ///< return NULL if this is not a type
		       ///(E_TYPE), the actual type otherwise.
  
  int isArrayExpr();  ///< return 1 if this is an ID that evaluated to
		      ///an array expression (E_ARRAY), 0 otherwise

 private:
  enum type t;			///< simple/compound expression type

  /**
   * an element of the expression for compound array expressions. l is
   * also used as an Expr * for the base case
   */
  AExpr *l, *r;   ///< an elements of the expression for compound
		  ///array expressions

  friend class AExprstep;
};
 
/**
 * @class AExprstep
 * 
 * @brief Class for stepping through an array expression
 * element-by-element.
 *
 * The value accessor method depends on the type of the array
 * expression. Only one of them will work correctly, and it is up to
 * the user to call the correct one.
 */
class AExprstep {
public:
  AExprstep (AExpr *a); ///< construct an array expression stepper
  ~AExprstep ();
  void step();  ///< advance by one element
  int isend();	///<  returns 1 on an end of array, 0 otherwise

  unsigned long getPInt(); ///< get the current pint value
  long getPInts(); ///< get the current signed pint(not used)
  double getPReal(); ///< get the current preal value
  int getPBool(); ///< get the current pbool value
  InstType *getPType(); ///< get the current ptype value
  expr_pstruct *getPStruct(); ///< get the current pstruct value

  /**
   * get the current identifier. If the identifier is an array
   * element, then *idx is used to return the linear offset within the
   * array, and *typesize is used to return the size of the current
   * range
   * @param id used to return the ID
   * @param idx used to return the index
   * @param typesize used to return the size of the range
   */
  void getID (ActId **id, int *idx, int *typesize);

  /**
   * This permits a dense ID to be returned as a single simple
   * ID. Otherwise it is similar to getID(). This is used to support
   * array to array direct connections.
   */
  void getsimpleID (ActId **id, int *idx, int *typesize);

  /**
   * @return 1 if this is a simple ID, 0 otherwise.
   */
  int isSimpleID ();


private:
  list_t *stack;		///< stack of AExprs to be processed
  AExpr *cur;			///< current element being processed

  union {
    Expr *const_expr;		///< current constant expression, or:
    struct {
      /* this is used for non-parameter identifiers */
      ActId *act_id;		///< identifier
      Arraystep *a;		///< array deref within the id, in case
				///< it is an array
      unsigned int issimple:1;		///< 1 if this is a raw id
    } id;
    struct {
      /* this is used for arrays of parameters */
      ValueIdx *vx;		///< base value idx
      Scope *s;			///< scope in which to evaluate
      Arraystep *a;		///< if it is an array or subrange
    } vx;
  } u;
  unsigned int type:2;		///< 0 = none, 1 = const, 2 = id,
				/// 3 = value array
};

#endif /* __ACT_ARRAY_H__ */
