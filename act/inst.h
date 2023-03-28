/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2012, 2018-2019 Rajit Manohar
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
#ifndef __INST_H__
#define __INST_H__

#include <act/expr.h>
#include <act/basetype.h>

/**
 * @file inst.h
 *
 * @brief This contains the definition of InstType, the core data
 * structure used to hold the type of any instance in the ACT design.
 */

class Scope;
class Type;
class Array;
class AExpr;
class InstType;

/**
 * @class inst_para
 *
 * @brief This holds a single template parameter. A template parameter
 * is either an array expression, or an InstType.
 *
 * If the stored pointer is NULL, it means that the parameter was
 * omitted.
 */
struct inst_param {
  unsigned int isatype:1;	/**< 1 if type, 0 otherwise */
  union {
    AExpr *tp;			/**< template parameters, for
				   user-defined types;
				   could be a single expression for
				   int<>;
				*/

    InstType *tt;		/**< could be types themselves, for
				   channels; can also be a type
				   signature for ptypes
				*/
  } u;
};


/**
 * @class InstType
 *
 * @brief An instance type. 
 *
 * An instance type contains
 *   - a Base type (Type)
 *   - optional array ranges
 *   - direction flags
 *   - optional template parameters
 *
 * An instance type can be unexpanded or expanded.
 *   - When parsing, all instance types are "unexpanded."
 *   - When expanding, we evaluate all instance types and convert them
 * to expanded form.
 *
 * InstType pointers are recorded in various data strutures like the
 * instance table in a Scope. They are also used temporarily for
 * type-checking. InstType pointers held in other data structures are
 * marked as "cached", and deleting those pointers does nothing. Hence
 * it should always be safe to delete an InstType pointer even if its
 * use is recorded elsewhere.
 */
class InstType {
 public:

  /**
   * The standard constructor. create instance type with the specified
   * type as the root/base of the instance. By default an InstType is
   * marked "temporary" (i.e. uncached).
   * 
   * @param _s is the scope in which the type is created
   * @param _t is the base type
   * @param is_temp is used to create permanent InstType pointers
   */
  InstType (Scope *_s, Type *_t, int is_temp = 1);

  /**
   * This creates a duplicate of the InstType passed in, making sure
   * it is a temp type by default. You can optionally skip any array
   * specifiers for the type
   * @param i is the InstType to be copied
   * @param skip_array is set to 1 when you want the copy to omit any
   * array specifier. If skip_array is set to 0 and the type has an
   * array specifier, this is flagged as an error (assertion failure).
   */
  InstType (InstType *i, int skip_array = 0);

  /**
   * Free the type if it is an uncached (is_temp = 1) type. If the
   * type is marked as cached, then this desctructor does nothing.
   */
  ~InstType ();

  /**
   * Make this an arrayed type, where the array size is specified by
   * the argument
   * @param a is the Array specifier that corresponds to the
   * array dimensions to be added to this InstType
   */
  void MkArray (Array *a);

  /**
   * @return the Array specifier for this InstType
   */
  Array *arrayInfo () { return a; }

  /**
   * Remove the array specifier from the type by setting it to NULL
   */
  void clrArray () { a = NULL; }

  /**
   * Check if two instance types are identical. Note that this  DOES
   * NOT CHECK DIRECTION FLAGS! There are different degrees of
   * equality checking that are controlled by the parameter
   * `weak`. 
   *
   * The `weak` parameter only matters for array types. All cases check
   * that the # of dimensions match. The remaining check for equality
   * of array types is:
   *
   * Weak |  Meaning
   * -----| --------
   * 0    | check that the arrays are in fact equal and have the same index ranges (and range expressions).  Used for checking that port parameters are the same  in the definition and  declaration.
   * 1    | checks that arrays are compatible in terms of # of dimensions, rather than equal. Used to sanity check connections prior to expanding parameters. 
   * 2    | check that array ranges have the same size; they could have different index ranges. Used to check that two types can be connected.
   * 3    | check that array ranges have the same size, except for the first dimension. Used to check that two types can be concatenated in an array expression.
   * 
   * @param t is the InstType to compare with
   * @param weak is the weak parameter (default is zero)
   * @return 1 if equal, 0 otherwise.
   */
  int isEqual (InstType *t, int weak = 0);

  /**
   * The same as isEqual(), except it also checks that the direction
   * flags for the type match.
   */
  int isEqualDir (InstType *t, int weak = 0);

  
  Type *isConnectable (InstType *t, int weak = 0);
  /**< check if two types are compatible for connections.  Returns
     NULL if error, otherwise returns the most specific type. The weak
     flag has the same meaning as isEqual().
  */

  void Print (FILE *fp, int nl_mode = 0); /**< print inst type string */

  void sPrint (char *buf, int sz, int nl_mode = 0); /**< snprintf */

  Type *BaseType () const { return t; } /**< Return root/base type */

  /**
   * Used to create slots for template parameters 
   * @param n is the number of template parameter slots to be created.
   */
  void setNumParams (int n);
  
  /**
   * Append parameters to the list of current template parameters. 
   * @param ns are the additional parameters
   * @param a is an array of template parameters that are to be
   * appended.
   */
  void appendParams (int na, inst_param *a);

  /**
   * Sets the template parameter at the specified slot number to be an
   * array expression
   * @param pn is the slot number
   * @param a is the array expression. This does not copy it, but
   * keeps a reference to the parameter. Do not free!
   */
  void setParam (int pn, AExpr *a);

  /**
   * Sets the template parameter at the specified slot number to be a 
   * simple expression
   * @param pn is the slot number
   * @param e is the expression. This does not copy it, but
   * keeps a reference to the parameter. Do not free!
   */
  void setParam (int pn, Expr *e);

  /**
   * Sets the template parameter at the specified slot number to be an 
   * InstType. This is used for channels and ptype parameters.
   * @param pn is the slot number
   * @param t is the type. This does not copy it, but
   * keeps a reference to the parameter. Do not free!
   */
  void setParam (int pn, InstType *t);

  /**
   * @param pn is the slot number
   * @return the InstType at that slot number, assertion failure if it
   * is not a type
   */
  InstType *getTypeParam (int pn);

  /**
   * @param pn is the slot number
   * @return the array expression AExpr at that slot number, assertion
   * failure if it is not an array expression.
   */
  AExpr *getAExprParam (int pn);

  /**
   * @return the number of template parameters
   */
  int getNumParams() { return nt; }

  /**
   * @return direct access to the template parameter array
   */
  inst_param *allParams() { return u; }

  /**
   * Set direction flags
   * @param d is the direction flag to be used.
   */
  void SetDir (Type::direction d) { dir = d; }

  /**
   * @return the direction flag for this type
   */
  Type::direction getDir () { return dir; }

  /**
   * Mark this InstType as cached
   */
  void MkCached () { temp_type = 0; }

  /**
   * @return 1 if this is a temporary (uncached) type
   */
  int isTemp() { return temp_type; }

  void *operator new (size_t count ) { return malloc (count); }
  void operator delete (void *ptr) { InstType *t = (InstType *)ptr; if (t->temp_type == 0) return; free (ptr); }

  /**
   * Return a fresh instance type that is the expanded version of the
   * type we had originally!
   *   @param ns is the namespace in which any types should be looked
   *   up
   *   @param s is the expanded scope in which any parameters should
   *   be evaluated
   */
  InstType *Expand (ActNamespace *ns, Scope *s);

  /**
   * @return 1 if this is an expanded type or if this is a simple
   * parameter type, 0 otherwise
   */
  int isExpanded() const;

  /**
   * @return 1 if this is an expanded type, 0 otherwise
   */
  int israwExpanded() const { return expanded; }

  /**
   * Mark this type as expanded
   */
  void mkExpanded() { expanded = 1; }

  /**
   * @return the namespace where this instance was created.
   */
  ActNamespace *getNamespace() { return s->getNamespace(); }

  /**
   * @return the user-defined type where this instance was created, if
   * any 
  */
  UserDef *getUserDef () { return s->getUserDef(); }

  /**
   * @return 1 if the kth template parameter is a type
   */
  int isParamAType (int k);

  /**
   * Check if two types are related in the implementation relation
   * hierarchy. Array references are ignored.
   * @param it is the type to compare against
   * @param common is used to return the type where the relationship
   * was found
   * @return the base type of either the current InstType or it,
   * depending on which was the more specific type
   */
  Type *isRelated (InstType *it, InstType **common = NULL);

  /**
   * XXX: Return 1 if any template parameter in inst_param involves global
   * symbols. This may no longer be needed?
   */
  int hasinstGlobal() { return 0; }

  /**
   * Used to change the type signature during overrides.
   * @param update is the updated type.
   * @return an in-place updated (for temp types) or a fresh InstType
   * where the base type and template parameters are updated from the
   * specified updated type
   */
  InstType *refineBaseType (InstType *update);
  
  /**
   * Used to change the type signature during overrides.
   * @param update is the updated type.
   * @return an in-place updated (for temp types) or a fresh InstType
   * where the base type and template parameters are updated from the
   * specified updated type.
   */
  InstType *refineBaseType (Type *update);

  /**
   * set PType ID. When an instance is created using a "ptype" type,
   * we need a dummy place-holder name for the type itself. When this
   * is expanded out, the type is replaced with the actual value of
   * this parameter. Additionally, the Type pointer is actually set to
   * the interface so that one can determine that this is a ptype.
   * XXX: this can be cleaned up.
   * @param s is the name of the ID that is the ptype
   */
  void setPTypeID (char *s) { ptype_id = string_cache (s); }

  /**
   * @return PType ID
   */
  const  char *getPTypeID () { return ptype_id; }

  /**
   * Set the interface type
   * @param x is the interface type
   */
  void setIfaceType (InstType *x) { iface_type = x; }

  /**
   * @return the interface type
   */
  InstType *getIfaceType() { return iface_type; }
  
 private:


  Type *t;			/**< root/base type */

  /**
   * Direction flags for types. ? = in, ! = out. ?! and !? used to
   * specify modifiers in port parameters for channels and data
   */
  Type::direction dir;

  /**
   * Scope in which the expanded instance was created
   */
  Scope *s;

  /* the following are optional */
  Array *a;			/**< array specification, if any */
  int nt;			/**< template parameters, if any */
  inst_param *u;		/**< array of parameters, if any */

  unsigned int temp_type:1;	/**< set if this is an uncached inst
				   type */
  unsigned int expanded:1;	/**< set if this is expanded */

  const char *ptype_id;		/**< for inst-types, this is the name
				   of the ptype */
  InstType *iface_type;		/**< if you're expanded, this is your
				   interface (for ptypes) */

  friend class TypeFactory;
};


#endif /* __INST_H__ */
