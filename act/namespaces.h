/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011, 2018-2019 Rajit Manohar
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
#ifndef __NAMESPACES_H__
#define __NAMESPACES_H__

#include <common/hash.h>
#include <common/list.h>
#include <common/bitset.h>
#include <common/array.h>

class Act;
class ActBody;
class UserDef;
class Type;
class InstType;
class ActId;
class AExpr;
class AExprstep;
class ActNamespace;
class ActNamespaceiter;
class ActTypeiter;
class ValueIdx;
class act_connection;
class act_languages;
struct act_chp;
struct act_prs;
struct act_spec;
class Array;

class Scope {
 public:
  Scope(Scope *parent, int is_expanded = 0);
  ~Scope();

  Scope *Parent () { return up; }

  /* Local scope lookup only */
  InstType *Lookup (const char *s);
  InstType *Lookup (ActId *id, int err = 1); /**< only looks up a root
						id; default report an error */
  /* u = plain vanilla user-def type with potential template parameters */
  void refineBaseType (const char *s, InstType *u);
  
  InstType *FullLookup (const char *s); /**< return full lookup,
					   including in parent scopes */

  InstType *FullLookup (ActId *id, Array **aref);
  /**< return actual type of ID,  after full lookup including
     parent scopes. aref is used to store final array reference in
     id, if any */

  InstType *localLookup (ActId *id, Array **aref);

  /* 
     only for expanded scopes
     returns ValueIdx information for identifier
  */
  ValueIdx *LookupVal (const char *s);
  ValueIdx *FullLookupVal (const char *s);


  int Add (const char *s, InstType *it);
  void Del (const char *s);	/* used to delete loop index variables
				   */
  void FlushExpand ();  /* clear the table, and make sure the new
			   scope is expanded */

  void Merge (Scope *s);	/**< Merge in instances into the
				   current scope. This must be called
				   *before* any other instances have
				   been created.
				*/

  void Print (FILE *fp);

  /**
   * Create a new scope that is a child of the current scope
   */
  Scope *Push () { return new Scope (this); }

  /**
   * Delete current scope, returning parent [does this work?]
   */
  Scope *Pop () {
    Scope *x = this, *ret = this;
    ret = this->up;
    delete x;
    return ret;
  }

  /**
   * Associate scope with a user defined type 
   */
  void setUserDef (UserDef *_u) { u = _u; }
  void mkFunction () { is_function = 1; }
  int isFunction() { return is_function; }
  UserDef *getUserDef () { return u; }

  void setNamespace (ActNamespace *_ns) { ns = _ns; }
  ActNamespace *getNamespace () { return ns; }

  unsigned long AllocPInt(int count = 1);
  void DeallocPInt(unsigned long idx, int count = 1);
  void setPInt(unsigned long id, unsigned long val);
  int issetPInt (unsigned long id);
  unsigned long getPInt(unsigned long id);

  unsigned long AllocPInts(int count = 1);
  void DeallocPInts(unsigned long idx, int count = 1);
  int issetPInts (unsigned long id);
  long getPInts(unsigned long id);
  void setPInts(unsigned long id, long val);

  unsigned long AllocPReal(int count = 1);
  void DeallocPReal(unsigned long idx, int count = 1);
  int issetPReal (unsigned long id);
  double getPReal(unsigned long id);
  void setPReal(unsigned long id, double val);

  unsigned long AllocPBool(int count = 1);
  void DeallocPBool(unsigned long idx, int count = 1);
  int issetPBool (unsigned long id);
  int getPBool(unsigned long id);
  void setPBool(unsigned long id, int val);

  unsigned long AllocPType(int count = 1);
  void DeallocPType(unsigned long idx, int count = 1);
  int issetPType (unsigned long id);
  InstType *getPType(unsigned long id);
  void setPType(unsigned long id, InstType *val);

  /**< returns 1 if this is an expanded scope */
  int isExpanded () { return expanded; }

  /**
     Add a binding function
  */
  void BindParam (const char *s, InstType *tt);
  void BindParam (ActId *id, InstType *tt);
  void BindParam (const char *s, AExpr *ae);
  void BindParam (ActId *id, AExpr *ae);

  void BindParam (ActId *id, AExprstep *aes, int idx = -1);

  void playBody (ActBody *b); /* create instances in the scope based
				 on what is in the body */

  const char *getName();
  
 private:
  struct Hashtable *H;		/* maps names to InstTypes, if
				   unexpanded; maps to ValueIdx if expanded. */
  Scope *up;
  
  UserDef *u;			/* if it is a user-defined type */
  unsigned int expanded:1;	/* if it is an expanded scope */
  ActNamespace *ns;		/* if it is a namespace scope */

  int is_function;		/* 1 if this is a function scope */

  /* values that are per scope, rather than per instance */
  A_DECL (unsigned long, vpint);
  bitset_t *vpint_set;

  A_DECL (long, vpints);
  bitset_t *vpints_set;

  A_DECL (double, vpreal);
  bitset_t *vpreal_set;

  A_DECL (InstType *, vptype);
  bitset_t *vptype_set;

  unsigned long vpbool_len;
  bitset_t *vpbool;
  bitset_t *vpbool_set;

  friend class ActInstiter;
  friend class ActUniqProcInstiter;
};


/**
 *   @file namespaces.h
 *         This contains the definitions for a namespace
 *
 */

/**
 *   The ActNamespace class holds all the information about a
 *   namespace.
 */
class ActNamespace {
 public:
  /**
   *  Create a new namespace in the global scope
   *
   *  @param s is the name of the namespace
   *
   */
  ActNamespace (const char *s);

  /**
   * Create a new namespace in the specified scope
   *
   * @param s is the name of the new namespace
   * @param ns is the parent namespace
   */
  ActNamespace (ActNamespace *ns, const char *s);

  ~ActNamespace ();

  /**
   *  Gets the name of the namespace
   *
   *  @return string corresponding to the namespace name
   */
  const char *getName () { return self_bucket->key; }

  /**
   * Lookup namespaces in the current namespace scope
   *
   * @param s is the name of the namespace to be found
   * @return namespace if found, NULL otherwise
   */
  ActNamespace *findNS (const char *s);


  /**
   * Lookup user-defined types in the current namespace
   *
   * @param s is the name of the type to be found
   * @return Type * if found, NULL otherwise
   */
  UserDef *findType (const char *s);

  /**
   * Lookup instance name in the current namespace
   *
   * @return instance pointer if found, NULL otherwise
   */
  InstType *findInstance (const char *s);


  /**
   * Lookup names.
   *
   * @return 0 if unused name, 1 if it is already a type, and 2 if it
   * is already a namespace, 3 if it is already an instance
   */
  int findName (const char *s);


  /**
   * Specifies if this is an exported namespace or not
   *
   * @return 1 if this is an exported namespace, 0 otherwise
   */
  int isExported () { return exported; }

  /**
   * Set this to be an exported namespace
   */
  void MkExported () { exported = 1; }
  void clrExported () { exported = 0; }

  /**
   * Provides parent namespace
   * @return parent namespace 
   */
  ActNamespace *Parent () { return parent; }

  /**
   * Returns a freshly allocated string containing the full path to
   * the namespace; append "::" at the end if add_colon is true
   */
  char *Name (bool add_colon = false);

  /**
   * Unlink the namespace from its parent. This function is required
   * for supporting namespace renaming
   */
  void Unlink ();

  /** 
   * Link namespace into parent
   *
   * @param up is the parent namespace
   * @param name is the name of the namespace
   */
  void Link (ActNamespace *up, const char *name);


  /**
   * Create a new user-defined type in this namespace
   *
   * @param u is a pointer to the userdefined type
   * @param nm is the name of the type
   *
   * @return 1 if successful, 0 otherwise
   */
  int CreateType (const char *nm, UserDef *u);

  /* edit type */
  int EditType (const char *s, UserDef *u);

  /**
   * Scope corresponding to this namespace
   * 
   * @return current scope
   */
  Scope *CurScope () { return I; }


  /**
   * Expand meta parameters
   */
  void Expand ();

  /**
   * Print text representation
   */
  void Print (FILE *fp);


  /**
   * Initialize the namespace module.
   */
  static void Init ();

  /**
   * Returns global namespace
   */
  static ActNamespace *Global () { return global; }
  static class Act *Act() { return act; }

  static void setAct (class Act *a);

  void setBody (ActBody *b) { B = b; }
  void AppendBody (ActBody *b);
  ActBody *getBody () { return B; }

  act_prs *getprs ();
  act_spec *getspec ();
  act_languages *getlang() { return lang; }

  list_t *getSubNamespaces ();
  list_t *getProcList();
  list_t *getDataList();
  list_t *getChanList();

 private:
  act_languages *lang;

  /**
   * hash table entry for this namespace
   */
  hash_bucket_t *self_bucket;

  /**
   * hash table for namespaces nested within this one
   */
  struct Hashtable *N;

  /**
   * hash table of all the types defined within this namespace
   *  When a type foo in the namespace is expanded to foo<x,y,z>, then
   *  the expanded version is also stored in this hash table.
   *
   */
  struct Hashtable *T;

  /**
   *  hash table of all the instances within this namespace.
   */
  Scope *I;

  /**
   * namespace body.
   */
  ActBody *B;

  /**
   * if the namespace is nested, this is a pointer to the parent.
   */
  ActNamespace *parent;	       	

  /**
   * if this namespace is exported, set to 1; otherwise zero.
   */
  unsigned int exported;	

  /**
   * pointer to the global namespace
   */
  static ActNamespace *global;
  static class Act *act;

  /**
   * used while creating the global namespace
   */
  static int creating_global;

  /**
   * Internal initialization function shared by the two constructors
   *
   * @param parent is the parent namespace
   * @param s is the name of the new namespace
   */
  void _init (ActNamespace *parent, const char *s);


  friend class Act;
  friend class ActNamespaceiter;
  friend class ActTypeiter;
};



/**
 *  Functions to manage namespace search paths
 *
 *  There are two open commands:
 *     open foo;
 *       this adds "foo" to the search path, and allows access to
 *       items that would be accessible in the foo namespace context
 *       (w.r.t. exports)
 *
 *    open foo -> bar;
 *       this renames foo to bar (without changing the export definitions)
 */
class ActOpen {
 public:
  ActOpen();
  ~ActOpen();

  /**
   * Opens a namespace (optionally as a new name)
   *
   * @param ns is the namespace to be opened
   * @param newname is the new name to be assigned to the
   * namespace. If NULL, then the namespace is simply opened
   *
   * @return 0 if the new namespace name already exists and therefore could
   * not be created, otherwise 1 on a success.
   */
  int Open(ActNamespace *ns, const char *newname = NULL);


  /**
   * Find namespace in the current context, given the context of opens
   *
   * @param cur is the current default namespace
   * @param s is the name of the namespace to be located
   * @return a list of namespaces, if found, NULL otherwise
   */
  list_t *findAll (ActNamespace *cur, const char *s);
  ActNamespace *find (ActNamespace *cur, const char *s);

  /**
   * Find namespace that contains the specified type, given the
   * context of opens
   *
   * @param cur is the current default namespace
   * @param s is the name of the type to be located
   * @return namespace if found, NULL otherwise
   */
  ActNamespace *findType (ActNamespace *cur, const char *s);
  

 private:
  /**
   * list of "open" namespaces (a list of ActNamespace *)
   */
  list_t *search_path;
};
  
#endif /* __NAMESPACES_H__ */
