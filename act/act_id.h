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
 * Act Identifier
 *
 */
class ActId {
 public:
  ActId (const char *s, Array *_a = NULL);
  ~ActId ();

  int isRange();		/**< 1 if any of the arrays specified
				   as part of this id have a
				   range specifier 
				*/
  
  /**
   * @return 1 if there is an array specifier as part of this id, but
   * it is not a range specifier. NOTE that this does not mean this
   * specifies a full dereference or that the dimensions match/etc.
   */
  int isDeref() { return !!(!isRange () && (a != NULL)); } 

  Array *arrayInfo() { return a; }

  void Append (ActId *id);	/**< make the current identifier into
				   identifier . id  (assuming next is
				   NULL right now) */

  const char *getName () { return string_char (name); } /**< return root
							name */

  ActId *Rest () { return next; }

  void Print (FILE *fp, ActId *stop = NULL, int style = 0);
  /**< print ID to the specified output stream, stopping at the
     specified point. Default is print the entire ID */
  void sPrint (char *buf, int sz, ActId *stop = NULL, int style = 0); 
  
  ActId *Clone ();

  ActId *Expand (ActNamespace *ns, Scope  *s); /**< expand ID */
  ActId *ExpandCHP (ActNamespace *ns, Scope *s); /**< expand CHP ID */

  Expr *Eval (ActNamespace *ns, Scope *s, int is_lval = 0, int is_chp = 0);
  /**< evaluating an ID returns either: just the ID itself, for
     non-meta parameters (also for meta parameters if is_lval=1)
     or the value of the parameter for meta-parameters. */

  Expr *EvalCHP (ActNamespace *ns, Scope *s, int is_lval = 0) {
    return Eval (ns, s, is_lval, 1);
  }
  /**< evaluating an ID returns either: just the ID itself, for
     non-meta parameters (also for meta parameters if is_lval=1)
     or the value of the parameter for meta-parameters. The CHP
     variant permits the ID to have a dynamic dereference */

  /**< 
     Find canonical root identifier in the current scope.
  */
  act_connection *Canonical (Scope *s);

  /**<
     Return the connection pointer corresponding to this particular
     id, NULL if something unexpected occurred.

     Note: we assume this connection pointer already exists, and this
     is only used for either foo or foo.bar

     TODO : generalize this if needed later
  */
  act_connection *myConnection (Scope *s);


  void setArray (Array *_a) { a = _a; }
  void prune () { next = NULL; }

  int isEqual (ActId *other);

  int isExpanded ();

  ActId *Tail();

  ValueIdx *rootVx (Scope *);	// return the root ValueIdx

  int isDynamicDeref ();	// check if this ID has any array
				// index that is not a constant value
				// after expansion

  UserDef *isFragmented (Scope *); // is this an ID that corresponds to a
                                   // fragmented int/chan?

  ActId *unFragment (Scope *);	// for a fragmented ID, return the
				// unfragmented piece
  
  UserDef *canFragment (Scope *); // returns the userdef that can be
				  // used to fragment the id, or NULL
				  // if the id cannot be fragmented.
  
  act_connection *rootCanonical (Scope *); // return root of the
					   // canonical pointer for
					   // this object

  int isNonLocal (Scope *s);	// return 1 if this ID is a non-local
				// identifier, 0 otherwise. Non-local
				// = port or global 

  static ActId *parseId (char *s, const char delim1, const char arrayL,
			 const char arrayR, const char delim2);
  static ActId *parseId (const char *s, const char delim1, const char arrayL,
			 const char arrayR, const char delim2);

  static ActId *parseId (const char *s);
  static ActId *parseId (char *s);

  /*-- precondition: this ID path exists in the process --*/
  /* 
     For an ID a.b.c.d.e where the prefix a.b.c are processes, and
     d.e are not, this returns "c.d.e" as the rest, and the type of
     "a.b" as the process that contains "c.d.e". If none of them are
     processes, it returns the ID and the process argument.

     So looking up the return value in the return process guarantees
     it is a non-process.
  */
  ActId *nonProcSuffix (Process *p, Process **ret);

  /*
    Check that any array indices are valid and this is a non-arrayed
    identifier.
  */
  int validateDeref (Scope *sc);

  /*
    Return a freshly allocated ID without any final array dereference
  */
  ActId *stripArray ();

  /*
    Return 1 if this is a namespace prefix, 0 otherwise.
    This can only be true for the first part of an ActId
  */
  int isNamespace();

  /*
    Returns the ActNamespace pointer for a namespace global
  */
  ActNamespace *getNamespace();

 private:
  mstring_t *name;		/**< name of the identifier */
  Array *a;			/**< array reference/dereference */
  ActId *next;			/**< any `.' reference */

  ValueIdx *rawValueIdx (Scope *);

};


// WARNING: for internal use only.
// When extending sparse arrays, the init flag used for connection
// initialization only applies to the existing array blocks. Some
// additional initialization is needed for uesr-defined types.
// elem_num = element # to be initialized, -1 = nothing
void _act_int_import_connections (act_connection *cx, UserDef *ux, Array *a, int elem_num);

#endif /* __ACT_ID_H__ */
