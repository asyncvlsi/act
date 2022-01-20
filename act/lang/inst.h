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

class Scope;
class Type;
class Array;
class AExpr;
class InstType;

/*
 * Template parameters can be either a single expression or types
 */
struct inst_param {
  unsigned int isatype:1; /* 1 if type, 0 otherwise */
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
  /* if both are NULL, it means the parameter was omitted */
};


/**
 * Instance type
 *   - Base type
 *   - plus optional array ranges
 *   - plus optional template parameters
 *
 * An instance type can be unevaluated or evaluated.
 *   - When parsing, all instance types are "unevaluated".
 *   - When expanding, we evaluate all instance types.
 */
class InstType {
 public:

  InstType (Scope *_s, Type *_t, int is_temp = 1);   /**< create
							instance type
							with the
							specified type
							as the root of
							the instance */

  InstType (InstType *, int skip_array = 0);	/**< create a copy of
						   the inst type;
						   doesn't copy
						   array. Default is
						   to complain about arrays. */

  ~InstType ();

  void MkArray (Array *a);	/**< make this an array of t */
  Array *arrayInfo () { return a; }
  void clrArray () { a = NULL; }

  int isEqual (InstType *t, int weak = 0); /**< check if two instance types are
					      identical: DOES NOT
					      CHECK DIRECTION FLAGS! 
					   */
  int isEqualDir (InstType *t, int weak = 0); // also checks direction flags
  
  Type *isConnectable (InstType *t, int weak = 0);
  /**< check if two types are compatible for connections.  Returns
     NULL if error, otherwise returns the most specific type.
  */

  void Print (FILE *fp, int nl_mode = 0); /**< print inst type string */

  void sPrint (char *buf, int sz, int nl_mode = 0); /**< snprintf */

  Type *BaseType () const { return t; } /**< Return root type */

  void setNumParams (int n);  /**< Create template parameter values */
  
  void appendParams (int na, inst_param *a); /**< append params to the
						list */

  /* 
     All these functions create a reference to the pointer passed in;
     DO NOT FREE!
  */
  void setParam (int pn, AExpr *a); /**< Set template parameter to
				       value */
  void setParam (int pn, Expr *e);  /**< Set template parameter to
				       value */

  void setParam (int pn, InstType *t); /**< for ptype and chans */
  InstType *getTypeParam (int pn);
  AExpr *getAExprParam (int pn);
  int getNumParams() { return nt; }
  inst_param *allParams() { return u; }

  /**
   * Set direction flags
   */
  void SetDir (Type::direction d) { dir = d; }
  Type::direction getDir () { return dir; }

  void MkCached () { temp_type = 0; }
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

  int isExpanded() const;
  int israwExpanded() const { return expanded; }
  void mkExpanded() { expanded = 1; }

  ActNamespace *getNamespace() { return s->getNamespace(); }
  UserDef *getUserDef () { return s->getUserDef(); }

  int isParamAType (int k);

  Type *isRelated (InstType *it, InstType **common = NULL);

  /* XXX: Return 1 if any template parameter in inst_param involves global
     symbols */
  int hasinstGlobal() { return 0; }

  InstType *refineBaseType (InstType *update);
  InstType *refineBaseType (Type *update);

  void setPTypeID (char *s) { ptype_id = string_cache (s); }
  const  char *getPTypeID () { return ptype_id; }
  void setIfaceType (InstType *x) { iface_type = x; }
  InstType *getIfaceType() { return iface_type; }
  
 private:


  Type *t;			/**< root type */

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
  Array *a;			/**< array specification */
  int nt;			/**< template parameters, if any */
  inst_param *u;		/**< array of parameters */

  unsigned int temp_type:1;	/**< set if this is an uncached inst
				   type */
  unsigned int expanded:1;	/**< set if this is expanded */

  const char *ptype_id;		/**< for inst-types, this is the name
				   of the ptype */
  InstType *iface_type;		/**< if you're expanded, this is your
				   interface */

  friend class TypeFactory;
};


#endif /* __INST_H__ */
