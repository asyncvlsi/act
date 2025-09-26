/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
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
#ifndef __ACT_ID_H__
#define __ACT_ID_H__

#include <stdio.h>
#include <common/mstring.h>
#include <act/expr.h>

class Array;
class Scope;
class ActNamespace;
class act_connection;
class ValueIdx;
class UserDef;
class Process;

/**
 * @class ActId
 *
 * @brief This class is used to store Act identifiers that correspond
 * to instances. Identifiers have an optional namespace prefix (in the
 * case of namespace global identifiers), followed by a list of
 * components of the idenfier.
 *
 * Each identifier component has a name, and an optional array
 * reference. Examples of identifiers include foo, name::foo, foo.bar,
 * foo[3].bar[7][8].b, foo[3..7], etc.
 *
 * In the case of something like "foo[3].bar.baz", "bar.baz" is viewed
 * as a sub-identifier of "foo[3]", and "foo[3]" is the root of the
 * identifier.  Similarly, "baz" is a sub-identifier of "bar.baz".
 *
 * All strings stored in any ActId are cached using the string_cache
 * mechanism in the common library.
 */
class ActId {
 public:

  /**
   * Create a simple ACT identifier with name "s" and an optional
   *  array reference.
   *
   * @param s is the name of the identifier
   * @param _a is the optional array specfier
   */
  ActId (const char *s, Array *_a = NULL);

  ~ActId (); ///< release storage

  /**
   * This can be used to check if any part of the identifier includes
   * a range specification as part of the array component
   *
   * @return 1 if this has a range specifier, 0 otherwise.
   */
  int isRange();
  
  /**
   * @return 1 if there is an array specifier as part of this id, but
   * it is not a range specifier. NOTE that this does not mean this
   * specifies a full dereference or that the dimensions match/etc.
   */
  int isDeref() { return !!(!isRange () && (a != NULL)); } 

  /**
   * Return a reference to the array associated with this piece of the
   * identifier.
   *
   * @return the array reference
   */
  Array *arrayInfo() { return a; }

  /**
   * Make the current identifier into identifier . id, assuming that
   * this ActId does not have a sub-identifier. If the ActId in fact
   * has a sub-identifier, an assertion failure will occur.
   *
   * @param id is the sub-identifer to be appended to the current
   * identifier.
   */
  void Append (ActId *id);

  /**
   * @return the simple identifier name for the root of the idenfier.
   */
  const char *getName () { return string_char (name); }

  /**
   * This method is used to return the sub-identifier.
   *
   * @return an ActId corresponding to the sub-identifier, NULL if
   * there isn't one.
   */
  ActId *Rest () { return next; }

  /**
   * Print ID to the specified output stream, stopping at the
   * specified point. Default is print the entire ID.
   *
   * Multi-dimensional arrays can be printed in two styles: either in
   * the form [a][b][c] (style = 0), or [a,b,c] (style = 1).
   *
   * @param fp is the output stream to be used
   * @param stop is the stopping criteria: the sub-identifier that
   * should not be printed.
   * @param style specifies the printing style
   * @param delim is the hierarchy delimiter
   */
  void Print (FILE *fp, ActId *stop = NULL, int style = 0, char delim = '.');

  /**
   * This is like Print(), except the ID is printed to a buffer
   *
   * @param buf is the output buffer
   * @param sz is the size of the output buffer
   * @param stop is the stopping condition
   * @param style is the array style
   * @param delim is the hierarchy separator
   */
  void sPrint (char *buf, int sz, ActId *stop = NULL, int style = 0,
	       char delim = '.'); 

  /**
   * Returns a deep copy of the ActID. Note that string pointers are
   * shared across all ActIDs, and clones are no different.
   *
   * @return a freshly allocated ActID
   */
  ActId *Clone (ActNamespace *orig = NULL, ActNamespace *newns = NULL);

  /**
   * If the ID can't be found in the current namespace but can be
   * found in the original namespace, add the namespace qualifier to the
   * ID and return a new one.
   * @param cur is the current namespace
   * @param orig is the original namespace
   * @return the updated ID, or the unmodified pointer
   */
  ActId *qualifyGlobals (ActNamespace *cur, ActNamespace *orig);
  bool isQualifyGlobals (ActNamespace *cur, ActNamespace *orig);
  
  void moveNS (ActNamespace *orig = NULL, ActNamespace *newns = NULL);

  /**
   * Returns an expanded ID where any parameters are substituted and
   * any expressions that can be evaluated are evaluated.
   *
   * @param ns is the namespace in which the ID was found
   * @param s is the evaluation scope
   * @return A freshly allocated ActId corresponding to the expanded
   * identifier.
   */
  ActId *Expand (ActNamespace *ns, Scope  *s);

  /**
   * Similar to Expand(), except the identifier comes from the CHP
   * context. This means it can have array expressions that contain
   * other identifers.
   *
   * @param ns is the namespace in which the ID was found
   * @param s is the evaluation scope
   * @return A freshly allocated ActId corresponding to the expanded
   * identifer.
   */
  ActId *ExpandCHP (ActNamespace *ns, Scope *s);

  /**
   *  Evaluate identifier: It either returns an evaluated identifier
   *  (i.e. all items expanded out), or in the case of something that
   *  isn't an lval, the value of the parameter.
   *
   *  Return expression types are:
   *    for lval: E_VAR
   *    otherwise: E_VAR (for non-parameter types)
   *               E_TRUE, E_FALSE, E_TYPE, E_REAL, E_INT,
   *               E_ARRAY, E_SUBRANGE -- for parameter types
   *
   *  The CHP variant of the call permits an identifier to have a
   *  dynamic de-reference. Non-CHP array references must be fully
   *  evaluated to constant expressions.
   *
   * @param ns is the namespace where the evaluation is performed
   * @param s is the evaluation scope
   * @param is_lval is 0 for identifiers that appear in an expression
   * context, and 1 otherwise (for "left values").
   * @param is_chp is 1 for identifiers that are within a CHP
   * body, 0 otherwise.
   * @return The evaluated Expr data structure.
   */
  Expr *Eval (ActNamespace *ns, Scope *s, int is_lval = 0, int is_chp = 0);

  /**
   * A short-hand for Eval(), to make it easier to read.
   *
   * @param ns same as in Eval()
   * @param s  same as in Eval()
   * @param is_lval same as in Eval()
   * @return The evaluated expression, in a CHP context.
   */
  Expr *EvalCHP (ActNamespace *ns, Scope *s, int is_lval = 0) {
    return Eval (ns, s, is_lval, 1);
  }

  /**
   * Find canonical root identifier in the current scope. When an
   * identifier is connected to others, one of them is the "canonical"
   * identifier. This returns its connection pointer.
   *
   * @param s is the scope
   * @returns the act_connection pointer, which is the canonical/unique
   * representation for any identifier with all connections resolved.
   */
  act_connection *Canonical (Scope *s);

  /**
   *
   *  Return the connection pointer corresponding to this particular
   * id, NULL if something unexpected occurred.
   *
   * Note: we assume this connection pointer already exists, and this
   * is only used for either foo or foo.bar
   *
   * TODO : generalize this if needed later
   *
   * @param s is the scope
   * @return the connection pointer corresponding to my ID
  */
  act_connection *myConnection (Scope *s);


  /**
   * This method is used to replace the array de-reference of the
   * identifier with the newly specified one. It can also be used to
   * remove the array specifier by setting it to NULL.
   *
   * @param _a is the new array specifier for this identifier
   */
  void setArray (Array *_a) { a = _a; }

  /**
   * This prunes the sub-identifer by setting it to NULL / empty
   */
  void prune () { next = NULL; }

  /**
   * Checks if this identifier is equal to the one passed in.
   *
   * @param other is the identifier to compare against
   * @return 1 if equal, zero otherwise.
   */
  int isEqual (ActId *other);

  /**
   * @return the sub-identifier reference for the current identifier
   */
  ActId *Tail();

  /**
   * This method returns the ValueIdx pointer for the
   * identifier.
   *
   * @return the root valueidx pointer
   */
  ValueIdx *rootVx (Scope *);

  /**
   * This method is used to check if this ID has any array
   * index that is not a constant value after expansion. Such indicies
   * can only be resolved at hardware runtime (i.e. dynamically).
   *
   * @return 1 if the ID contains a dynamic de-reference, 0 otherwise.
   */
  int isDynamicDeref ();

  /**
   * An identifier can correspond to a "fragmented" integer or channel
   * type. This would be because the identifier accesses part of a
   * channel or integer data type via their port lists. This returns
   * the user-defined type that this identifier fragments, if any.
   *
   * @return NULL if this is not a fragmented integer/channel, or the
   * user-defined type corresponding to the fragmented type otherwise.
   */
  UserDef *isFragmented (Scope *);

  /** 
   * If an ID is fragmented, this returns the prefix of the identifier
   * that corresponds to the unfragmented part.
   * @return a new ActId corresponding to the unfragmented part of the
   * ID, NULL if this is not possible.
   */
  ActId *unFragment (Scope *);


  /**
   * If this identifier is a reference to a user-defined type, then
   * you may be able to fragment it further into ports/etc. This
   * method can be used to return the type for the identifier.
   * @return the user-defined type pointer, or NULL if this identifier
   * does not correspond to a user-defined type.
   */
  UserDef *canFragment (Scope *);

  /**
   * This finds the canonical connection pointer for the object, and
   * then traverses it to the canonical root. For example if you have
   * an identifier like x.y.z, then the "root" of it would be "x".
   * @return the root of the canonical pointer for the object
   */
  act_connection *rootCanonical (Scope *);

  /**
   * A non-local ID is a port or global.
   * @return 1 if this ID is a non-local identifier, 0 otherwise. 
   */
  int isNonLocal (Scope *s);	

  static ActId *parseId (char *s, const char delim1, const char arrayL,
			 const char arrayR, const char delim2);
  static ActId *parseId (const char *s, const char delim1, const char arrayL,
			 const char arrayR, const char delim2);

  static ActId *parseId (const char *s);
  static ActId *parseId (char *s);

  /**
   *  Precondition: this ID path exists in the process.
   *
   * For an ID a.b.c.d.e where the prefix a.b.c are processes, and
   * d.e are not, this returns "c.d.e" as the rest, and the type of
   * "a.b" as the process that contains "c.d.e". If none of them are
   * processes, it returns the ID and the process argument.
   *
   * So looking up the return value in the return process guarantees
   * it is a non-process.
   *
   * @param p is the process where the identifier exists
   * @param ret is used to return the process type
   * @return the suffix of the identifier.
  */
  ActId *nonProcSuffix (Process *p, Process **ret);

  /**
   * Check that any array indices are valid and this is a non-arrayed
   * identifier.
   * @return 1 if validation succeeded, 0 otherwise
   */
  int validateDeref (Scope *sc);

  /**
   * This returns a freshly allocated ID where the final array
   * de-reference from the current ID has been removed.
   *
   * @return the freshly allocated ID without any final array dereference
   */
  ActId *stripArray ();

  /**
   * Return 1 if this ActId is a namespace prefix, 0 otherwise.
   * This can only be true for the first part of an ActId
   * @returns 1 if this is a namespace, 0 otherwise.
   */
  int isNamespace();

  /**
   * Returns the ActNamespace pointer for a namespace global.
   * @returns NULL if this is not a namespace global ID, or the
   * namespace pointer otherwise.
  */
  ActNamespace *getNamespace();

  /**
   * Replace the namespace in the ID with a new one
   * @param ns is the new namespace that replaces the namespace within
   * the ID.
   */
  void updateNamespace (ActNamespace *ns);

  /**
   * Used for generic hash functions
   */
  unsigned int getHash (unsigned int inH, unsigned long sz);

 private:
  mstring_t *name;		/**< name of the identifier */
  Array *a;			/**< array reference/dereference */
  ActId *next;			/**< any `.' reference */

  ValueIdx *rawValueIdx (Scope *); /**< used to return the ValueIdx
				      for this ID, potentially
				      allocating the initial
				      connection pointer if needed */

  static struct cHashtable *idH; /**< used to canonicalize ID pointers
				    post-expansion */


  // this is used for the remaining of the type, i.e. the "."
  // fields. Here we only check arrays.
  ActId *_qualifyGlobals (ActNamespace *cur, ActNamespace *orig);
  bool _isQualifyGlobals (ActNamespace *cur, ActNamespace *orig);
};


// WARNING: for internal use only.
// When extending sparse arrays, the init flag used for connection
// initialization only applies to the existing array blocks. Some
// additional initialization is needed for uesr-defined types.
// elem_num = element # to be initialized, -1 = nothing
void _act_int_import_connections (act_connection *cx, UserDef *ux, Array *a, int elem_num);

#endif /* __ACT_ID_H__ */
