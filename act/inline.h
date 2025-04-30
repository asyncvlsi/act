/*************************************************************************
 *
 *  Copyright (c) 2021, 2025 Rajit Manohar
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
#ifndef __ACT_INLINE_H__
#define __ACT_INLINE_H__

#include <act/expr.h>
#include <act/act_id.h>

class Data;
struct act_inline_table;
struct act_inline_value {
  act_inline_value() {
    is_just_id = 0;
    is_struct = 0;
    struct_count = 0;
    is_array = 0;
    array_sz = 0;
    u.val = NULL;
  }

  void clear() {
    if (!isSimple()) {
      FREE (u.arr);
      u.arr = NULL;
    }
  }

  Expr *getVal() { return u.val; }
  
  bool isSimple() {
    if ((is_struct == 0 && is_array == 0) || is_just_id) return true;
    return false;
  }

  void elaborateStructId (Data *d, Array *a = NULL); 
  
  int numStructElems() { return struct_count; }
  int numArrayElems() { return array_sz; }

  // check if there is some legitimate binding stored here
  bool isValid() {
    if (isSimple()) {
      // the binding is stored in u.val
      if (u.val == NULL) return false;
      if (is_just_id) {
	if (u.val->type != E_VAR) return false;
      }
      return true;
    }
    if (struct_count == 0 && array_sz == 0) return false;
    if (u.arr == NULL) return false;
    return true;
  }

  // check if everthing is valid
  bool isValidFull() {
    if (!isValid()) return false;
    if (!is_just_id) {
      int tot = (struct_count == 0 ? 1 : struct_count) *
	(array_sz == 0 ? 1 : array_sz);

      if (!isSimple()) {
	for (int i=0; i < tot; i++) {
	  if (u.arr[i] == NULL) return false;
	}
      }
    }
    return true;
  }

  void Print (FILE *fp);

  unsigned int is_struct:1;	/* is this a structure? */
  unsigned int is_array:1;	/* this is an array */
  unsigned int is_just_id:1;	/* special case, ID */
  unsigned int struct_count:30;	/* size of flattened struct for sanity checking */
  unsigned int array_sz:30;	/* array size, if any */

  union {
    Expr *val;			/* single value */
    Expr **arr;			/* flattened array of values */
  } u;
};

/**
 * @file inline.h
 *
 * @brief Helper functions to handle function inlining. Used to
 * inline simple functions (without internal loops) and return a
 * single expression that corresponds to the inlined function. This is
 * done by keeping track of current bindings of all variables to a
 * symbolic expression, and updating the symbolic expression as you
 * go through the function definition.
 */

/**
 * Create a new inline table linked to a parent in the current scope
 * @param sc is the current scope
 * @param parent is the parent inline table
 * @param ismacro is set to true if this is used for macro expansion,
 * false otherwise
 * @param allow_dag is set to true if expression dags are permitted, false
 * otherwise. This disables cloning expressions.
 *
 * If parent is non-NULL, then the ismacro and allow_dag flags are
 * inherited from the parent.
 *
 * @return a new inline table
 */
act_inline_table *act_inline_new (Scope *sc, act_inline_table *parent,
				  bool ismacro = false,
				  bool allow_dag = false);

/**
 * Release storage for a previously allocated inline table
 */
void act_inline_free (act_inline_table *);

/**
 * Set an identifier to an expression. An expression array is provided
 * in case the identifier corresponds to a structure. The inline table
 * bindings are updated.
 *
 * @param Hs is the current inline binding table
 * @param id is the identifier to be set
 * @param e is the array of expressions for values (e[0] will contain
 * the value if the  * identifier is simple one)
 */
void act_inline_setval (act_inline_table *Hs, ActId *id, act_inline_value v);

/**
 * @param Hs is the current inline binding table
 * @param s is the name of the identifier
 * @return the binding for an identifier from the table
 */
act_inline_value act_inline_getval (act_inline_table *Hs, const char *s);

/**
 * @param tab is the current inline binding table
 * @param s is the identifier to search for
 * @returns 1 if there is a binding present, 0 otherwise
 */
int act_inline_isbound (act_inline_table *, const char *s);

/**
 * Return the result of inlining all function calls into the
 * expression specified. The result might be a structure, so this
 * returns an array of expressions.
 */
act_inline_value act_inline_expr (act_inline_table *, Expr *, int recurse_fn = 1);


/**
 * Given inline tables corresponding to a set of conditions (e.g. via
 * a selection statement), this computes unified expressions that
 * combine the guard to select the correct expression result.
 */
act_inline_table *act_inline_merge_tables (int nT, act_inline_table **T, Expr **cond);

#endif /*  __ACT_INLINE_H__ */
