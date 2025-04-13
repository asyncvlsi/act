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

#include <vector>
#include <set>
#include <common/hash.h>
#include <common/list.h>
#include <common/bitset.h>
#include <common/array.h>

/**
 * @file namespaces.h
 *
 * @brief This contains the defintions of the ActNamespace class as
 * well as the Scope classes. These are the primary data structures
 * used to hold a design.
 */

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


/**
 * @class Scope
 *
 * @brief This is the data structure that holds all instances and
 * their associated types within a scope. Each user-defined type has
 * an associated Scope, and so does each ActNamespace.
 *
 * Scopes also know about their parent (enclosing) scope. So a process
 * definition would have a scope whose parent is the scope of the
 * namespace in which it is defined. A namespace scope would have a
 * parent scope that is the namespace within which it was created. The
 * top-level scope of the entire design is the built-in global
 * namespace.
 *
 * Scopes can be either expanded or unexpanded. Expanded scopes and
 * unexpanded scopes have different internals.
 */
class Scope {
 public:

  /**
   * @param parent is the parent scope for this namespace
   * @param is_expanded is 1 if this is an expanded scope, 0 otherwise
   */
  Scope(Scope *parent, int is_expanded = 0);
  ~Scope();

  /**
   * @return the parent scope
   */
  Scope *Parent () { return up; }

  /**
   * Lookup an identifier in the specified scope only. Restrict the
   * scope to a local lookup---don't look in parent scopes.
   *
   * @param s is the name to lookup
   * @return the InstType for the specified name, NULL if not found
   */
  InstType *Lookup (const char *s);

  /**
   * Similar to Lookup(), but takes an ActId as a parameter. Only
   * looks up the root of the identifier. By default
   * this reports an error if the ActId has a sub-identifier. This
   * error is suppressed by the err flag.
   * @param id the ActId to lookup in the local scope
   * @param err 1 if a fatal error should be reported, 0 to ignore the
   * error
   * @return the InstType for the specified name if it exists, NULL if
   * not found
   */
  InstType *Lookup (ActId *id, int err = 1);
  
  /**
   * Look up the local variable specified, and if it exists then
   * refine its base type using the new type information. Used to
   * update scope tables while processing overrides.
   * @param s is the name of the instance whose type is to be updated
   * @param u is the updated type for the instance
   */
  void refineBaseType (const char *s, InstType *u);

  /**
   * Similar to lookup, but instead of just looking in the local scope
   * also look at the parent scopes as well---i.e. any visible scope
   * where the name can be found
   * @param s is the name to be looked up
   * @return the InstType for the name if found, NULL otherwise
   */
  InstType *FullLookup (const char *s);

  /**
   * Do a lookup and see if the lookup succeeded in a namespace or in
   * a user-defined type
   * @param s is the name to be looked up
   * @return 1 if this was in namespace (i.e. a  namespace global), 0
   * otherwise 
   */
  int isGlobal (const char *s);

  /**
   * To a complete lookup of the ActId. The (possibly NULL) array
   * reference at the end of the ID is returned in aref, if aref is
   * non-NULL. 
   *
   * @param id is the identifier to be looked up in the current scope
   * (and its parents if necessary)
   * @param aref is used to return the array specifier in the ID
   * @return the actual type of the identifier
   */
  InstType *FullLookup (ActId *id, Array **aref);

  /**
   * Only looks in the current scope for the ID. However, the type
   * returned is the actual type of the full ID (not just the root),
   * and aref is used to return any array reference associated with
   * the specified id.
   *
   * @param id is the ID to lookup in the local scope
   * @param aref returns the array reference in the id, if any
   * @return the type of the id, if found in the current scope.
   */
  InstType *localLookup (ActId *id, Array **aref);

  /**
   * Only for expanded scopes. This returns ValueIdx information for
   * identifier.
   * @param s is the name to lookup 
   * @return the ValueIdx associated with s in the current scope
   */
  ValueIdx *LookupVal (const char *s);

  /**
   * Like LookupVal(), but also looks in the parent scope if you can't
   * find the specified name in the current scope.
   * @param s is the name to lookup
   * @return the ValueIdx for the specified name
   */
  ValueIdx *FullLookupVal (const char *s);

  /**
   * Like FullLookupVal(), but also returns the scope
   */
  ValueIdx *FullLookupValSc (const char *s, Scope **sc);

  /**
   *  Add a new identifier to the scope.
   *
   *  @param s is a string corresponding to the identifier being added
   *  to the scope
   *  @param it is the instantiation type 
   *
   *  @return 0 on a failure, 1 on success.
   */
  int Add (const char *s, InstType *it);

  /**
   * Delete a name from the current scope. This is used to delete loop
   * index variables.
   * @param s is the name to be deleted.
   */
  void Del (const char *s);

  /**
   * Clear the scope table, and mark this as an expanded scope.
   */
  void FlushExpand ();

  /**
   * Merge in instances into the scope. This must be called *before*
   * any other instances have been created.
   */
  void Merge (Scope *s);

  /**
   * Print out the scope
   * @param fp is the output file
   * @param all_inst if set to true, all instances are printed; if
   * false, then instances in the parent scope are not printed
   */
  void Print (FILE *fp, bool all_inst = false);

  /**
   * Associate scope with a user defined type.
   * @param _u is the user-defined type to be associated with this scope
   */
  void setUserDef (UserDef *_u) { u = _u; }

  /**
   * Use to indicate that this scope is part of a function
   */
  void mkFunction () { is_function = 1; }

  /**
   * Check if this is a function scope
   */
  int isFunction() { return is_function; }

  /**
   * Use to permit assignments to sub-scope parameter types. Used for
   * pstruct types
   */
  void allowSubscopeBind() { allow_sub = 1; }

  /**
   * @return the user-defined type this scope is for, if it
   * exists. NULL means this is not associated with a user-defined type.
   */
  UserDef *getUserDef () { return u; }

  /**
   * Set the namespace associated with this scope
   * @param _ns is the namespace to be associated with the scope
   */
  void setNamespace (ActNamespace *_ns) { ns = _ns; }

  /**
   * @return the namespace associated with the scope, NULL if there
   * isn't one
   */
  ActNamespace *getNamespace () { return ns; }

  /**
   * Allocate space to hold a pint in the scope
   * @param count is the number of pints to allocate
   * @return the index for the first new pint allocated.
   */
  unsigned long AllocPInt(int count = 1);

  /**
   * De-allocate pints starting from the index. This only works if
   * you are de-allocating from the end of the allocated pint
   * array. Otherwise it is silently ignored.
   * @param idx is index at which to de-allocate
   * @param count is the number to deallocate
   */
  void DeallocPInt(unsigned long idx, int count = 1);

  /**
   * Set the pint at index id to the specified value
   * @param id is the index of the pint
   * @param val is the value to set
   */
  void setPInt(unsigned long id, unsigned long val);

  /**
   * Check if the pint is in fact set
   * @param id is the index of the pint
   * @return 1 if it is set, 0 otherwise
   */
  int issetPInt (unsigned long id);

  /**
   * @param id is the index of the pint
   * @return the value of the pint at the specified index
   */
  unsigned long getPInt(unsigned long id);

  unsigned long AllocPInts(int count = 1);
  void DeallocPInts(unsigned long idx, int count = 1);
  int issetPInts (unsigned long id);
  long getPInts(unsigned long id);
  void setPInts(unsigned long id, long val);

  /**
   * Like AllocPInt(), but for preals
   */
  unsigned long AllocPReal(int count = 1);

  /**
   * Like DeallocPInt(), but for preals
   */
  void DeallocPReal(unsigned long idx, int count = 1);

  /**
   * Like issetPInt(), but for preals
   */
  int issetPReal (unsigned long id);

  /**
   * Like getPInt(), but for preals
   */
  double getPReal(unsigned long id);
  
  /**
   * Like setPInt(), but for preals
   */
  void setPReal(unsigned long id, double val);

  /**
   * Like AllocPInt(), but for pbools
   */
  unsigned long AllocPBool(int count = 1);

  /**
   * Like DeallocPInt(), but for pbools
   */
  void DeallocPBool(unsigned long idx, int count = 1);

  /**
   * Like issetPInt(), but for pbools
   */
  int issetPBool (unsigned long id);

  /**
   * Like getPInt(), but for pbools
   */
  int getPBool(unsigned long id);
  
  /**
   * Like setPInt(), but for pbools
   */
  void setPBool(unsigned long id, int val);

  /**
   * Like AllocPInt(), but for ptypes
   */
  unsigned long AllocPType(int count = 1);

  /**
   * Like DeallocPInt(), but for ptypes
   */
  void DeallocPType(unsigned long idx, int count = 1);

  /**
   * Like issetPInt(), but for ptypes
   */
  int issetPType (unsigned long id);

  /**
   * Like getPInt(), but for ptypes
   */
  InstType *getPType(unsigned long id);

  /**
   * Like setPInt(), but for ptypes
   */
  void setPType(unsigned long id, InstType *val);

  /**
   * @return 1 if this is an expanded scope, 0 otherwise
   */
  int isExpanded () { return expanded; }

  /**
   * Bind a name to a type. Allocates space for it if needed.
   * @param s is the name
   * @param tt is the type to bind to the name
   */
  void BindParam (const char *s, InstType *tt);

  /**
   * Bind a name to a type. Must be a local name
   * @param id is the name
   * @param tt is the type to use
   */
  void BindParam (ActId *id, InstType *tt);

  /**
   * Bind a name to the array expression
   * @param s is the name to be set
   * @param ae contains the array expression
   */
  void BindParam (const char *s, AExpr *ae);

  /**
   * Bind a name to the array expression
   * @param id is the name
   * @param ae is the array expression
   */
  void BindParam (ActId *id, AExpr *ae);

  /**
   * Bind a name at the specified offset (in idx) to the current
   * element from the AExpr stepper AExprstep
   */
  void BindParam (ActId *id, AExprstep *aes, int idx = -1);


  /**
   * Same as BindParam, but permit the id to be in a parent scope.
   */
  void BindParamFull (ActId *id, AExprstep *aes, int idx = -1);
  void BindParamFull (ActId *id, InstType *tt);
  void BindParamFull (ActId *id, AExpr *ae);
  
  /**
   * Create instances given the body
   * @param b is the body to be processed.
   */
  void playBody (ActBody *b); /* create instances in the scope based
				 on what is in the body */

  /**
   * @return the name of the user-defined type or namespace
   * corresponding to this scope. Returns -unknown- if neither exist.
   */
  const char *getName();


  /**
   * Update parent scope pointer. Warning: don't use unlesss you know
   * what you are doing! This is used because synthetic functions
   * generated during macro expansion have to be handled correctly.
   */
  void updateParent(Scope *s) { up = s; }

  /**
   * @return the list of value ids of ports of this user process,
   * empty vector if there are none
   */
  const std::vector<ValueIdx*> getPorts();

  /**
   * @return the list of value ids of user type instances of this user
   * process, empty vector if there are none
   */
  const std::vector<ValueIdx*> getUserDefInst ();

  /**
   * @return the list of value ids of primitive type instances of this
   * user process, empty vector if there are none
   */
  const std::vector<ValueIdx*> getPrimitiveInst ();

  /**
   * @return the list of symbol names available in the parent process,
   * empty if none available (none defined or no parent)
   */
  const std::set<const char*> getParentSymbols ();

  /**
   * Print the connections specified by the cx pointer. Normally this
   * only prints the essential connections---i.e. if cx is a
   * primary. The force flag can be used to override this, forcing
   * redundant connections to be printed.
   * @param fp is the output file
   * @param cx is the connection 
   * @param force is used to force the printing of non-primary connections
   */
  static void printConnections (FILE *fp, act_connection *cx, bool force = false);


  /**
   * This is used to find a fresh name with a specified prefix.
   * @param prefix is the prefix of the name
   * @param count  is a read/write integer that is updated until you
   * find a name called prefix<num> that is not in the scope
   *
   * Use this to find the next integer value that can be used.
   */
  void findFresh (const char *prefix, int *count);


  /**
   * This creates a "shallow copy" of the Scope. It will clone all the
   * tables within the scope, but not clone any Scope pointers,
   * namespace pointers, or userdef pointers. In other words, it only
   * replicates the instance table. Note that this  can ONLY be called
   * for an unexpanded scope.
   * @return the newly created scope, NULL if the operation failed
   * because the scope has already been expanded.
   */
  Scope *localClone ();
  
 private:
  struct Hashtable *H;		/* maps names to InstTypes, if
				   unexpanded; maps to ValueIdx if expanded. */
  Scope *up;
  
  UserDef *u;			/* if it is a user-defined type */
  unsigned int expanded:1;	/* if it is an expanded scope */
  unsigned int allow_sub:1;     /* if 1, allows binding to sub-scopes;
				   used for pstruct types */
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
 * @class ActNamespace
 *
 * @brief The ActNamespace class holds all the information about a
 * namespace.
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

  /**
   * Clear the exported flag
   */
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
   * @param s is the name of the type
   *
   * @return 1 if successful, 0 otherwise
   */
  int CreateType (const char *s, UserDef *u);

  /**
   * Edit type: replace the type definition for the type name with the
   * updated definition.
   *
   * @param u is the new type definition
   * @param s is the name of the type to be edited
   * @return 1 if successful, 0 if the type name was not found.
   */
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
   * Convert any enum Data to an int
   */
  void enum2Int ();

  /**
   * Print text representation
   */
  void Print (FILE *fp);

  /**
   * Returns 1 if it is expanded
   */
  int isExpanded() { return I->isExpanded(); }


  /**
   * Initialize the namespace module.
   */
  static void Init ();

  /**
   * @return the global namespace
   */
  static ActNamespace *Global () { return global; }

  /**
   * @return the Act pointer
   */
  static class Act *Act() { return act; }

  /**
   * Set the Act pointer
   */
  static void setAct (class Act *a);

  /**
   * Replace the body with the one specified
   * @param b is the new body
   */
  void setBody (ActBody *b) { B = b; }

  /**
   * Append to the current body
   * @param b is the body to be appended
   */
  void AppendBody (ActBody *b);

  /**
   * @return the current body of the namespace
   */
  ActBody *getBody () { return B; }

  /**
   * helper function to return the prs body
   */
  act_prs *getprs ();

  /**
   * helper function to return the spec body
   */
  act_spec *getspec ();

  /**
   * @return the language bodies within this namespace
   */
  act_languages *getlang() { return lang; }

  /**
   * @return a list of char *'s of the names of the sub-namespaces
   * nested within this one
   */
  list_t *getSubNamespaces ();

  /**
   * @return a list of process names
   */
  list_t *getProcList();

  /**
   * @return a list of user-defined data type names
   */
  list_t *getDataList();

  /**
   * @return a list of user-defined channel names
   */
  list_t *getChanList();


  /**
   * Create a clone of the namespace and any associated
   * sub-namespaces. Pointers to namespaces within the hierarchy are
   * also updated to the clone, whereas those to other namespaces
   * remain unchanged. This clone operation only works prior to
   * expansion. The clone operation also cannot be applied to the
   * global namespace.
   *
   * @param parent is the parent namespace into which this is to be
   * cloned. This is a copy of the namespace
   * @param name is the namespace name to be used for the clone.
   * @param root is the root of the namespace tree being cloned.
   *
   */
  void Clone (ActNamespace *root,
	      ActNamespace *parent, const char *name);


  /**
   * Check if a particular user-defined type exists in a scope. If it
   * does, then return the list of namespace names that can be used to
   * access it.
   * @return NULL if it doesn't exist in the namespace, or a list of
   * namespace names used to access it otherwise.
   */
  list_t *findNSPath (ActNamespace *ns);
  list_t *findNSPath (UserDef *u);

 private:
  
  act_languages *lang; ///< the sub-languages in the namespace

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
   *  hash table of all the instances within this namespace,
   *  encapsulated within the Scope
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

  /**
   * pointer to the Act class
   */
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


  /**
   * Substitute globals within the namespace
   *
   * @param subst is used to return a list of user-defined types where
   * the globals were substituted.
   * @param it is the type of the global
   * @param s is the name of the global
   */
  void _subst_globals (list_t *subst, InstType *it, const char *s);

  /* second phase */
  void _subst_globals_addconn (list_t *subst, listitem_t *start,
			       InstType *it, const char *s);

  /**
   * After a namespace has been cloned, we need to replace all types
   * that were within the original namespace with a type that is
   * within the cloned namespace.
   *
   * @param root is the root of the original cloned namespace
   * @param newroot is the new root of the cloned namespace
   * @param newns is the new namespace where we need to apply the update
   */
  void _updateClonedTypes (ActNamespace *root, ActNamespace *newroot,
			   ActNamespace *newns);

  friend class Act;
  friend class ActNamespaceiter;
  friend class ActTypeiter;
};



/**
 * @class ActOpen
 *
 * @brief Functions to manage namespace search paths.
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
