/*************************************************************************
 *
 *  Copyright (c) 2012-2018 Rajit Manohar
 *  All Rights Reserved
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
union inst_param {
  AExpr *tp;			/**< template parameters, for
				   user-defined types;
				   could be a single expression for
				   int<>;
				*/

  InstType *tt;			/**< could be types themselves, for
				   channels; can also be a type
				   signature for ptypes
				*/

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
				   identical */

  void Print (FILE *fp);	/**< print inst type string */

  void sPrint (char *buf, int sz); /**< snprintf */

  Type *BaseType ()  { return t; } /**< Return root type */

  void setNumParams (int n);  /**< Create template parameter values */

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

  int isExpanded();
  int israwExpanded() { return expanded; }
  void mkExpanded() { expanded = 1; }

  ActNamespace *getNamespace() { return s->getNamespace(); }
  UserDef *getUserDef () { return s->getUserDef(); }

  int isParamAType (int k);
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

  friend class TypeFactory;
};



#endif /* __INST_H__ */
