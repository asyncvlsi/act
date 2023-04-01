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
#ifndef __ACT_TYPES_H__
#define __ACT_TYPES_H__

#include <act/basetype.h>
#include <act/namespaces.h>
#include <act/act_array.h>
#include <act/expr.h>
#include <common/mstring.h>
#include <act/inst.h>
#include <string.h>

class ActBody;
struct act_chp_lang;
struct act_chp;
struct act_prs;
struct act_spec;
struct act_attr;

/**
 * @class PInt
 *
 * @brief Integer parameter type.
 *
 */
class PInt : public Type {
  /**
   * @return the name for this type
   */
  const char *getName() { return "pint"; }

  /**
   * Expansion doesn't do anything
   */
  Type *Expand (ActNamespace */*ns*/, Scope */*s*/, int /*nt*/, inst_param */*u*/) {
    return this;
  }

  /**
   * Equality is actual pointer equality
   */
  int isEqual (const Type *t) const { return t == this ? 1 : 0; }
};


/**
 * @class PInts
 *
 * @brief Unused. Originally pint and pints were supposed to be
 * unsigned and signed integers. Now pint is a signed integer
 */
class PInts : public Type {
  const char *getName() { return "pints"; }
  Type *Expand (ActNamespace */*ns*/, Scope */*s*/, int /*nt*/, inst_param */*u*/) {
    return this;
  }
  int isEqual (const Type *t) const { return t == this ? 1 : 0; }
};


/**
 * @class PBool
 *
 * @brief Used for Boolean parameters
 */
class PBool : public Type {
  /**
   * @return the name of the type
   */
  const char *getName() { return "pbool"; }

  /**
   * @return the type itself---nothing to do for expansion
   */
  Type *Expand (ActNamespace */*ns*/, Scope */*s*/, int /*nt*/, inst_param */*u*/) {
    return this;
  }

  /**
   * Equality is pointer equality
   */
  int isEqual (const Type *t) const { return t == this ? 1 : 0; }
};


/**
 * @class PReal
 *
 * @brief Used for real parameters
 */
class PReal : public Type {
  /**
   * @return the name of the type
   */
  const char *getName() { return "preal"; }

  /**
   * @return the type itself--nothing to do for expansion
   */
  Type *Expand (ActNamespace */*ns*/, Scope */*s*/, int /*nt*/, inst_param */*u*/) {
    return this;
  }

  /**
   * Equality is pointer equality
   */
  int isEqual (const Type *t) const { return t == this ? 1 : 0; }
};

/**
 * @class PType
 *
 * @brief Used for ptype( ) parameters. ptype parameters take an
 * interface type as a template parameter.
 */
class PType : public Type {
public:
  /**
   * @return the name of the type
   */
  const char *getName();

  /**
   * Expand the ptype. The number of template parameters must be
   * exactly one and be a type.
   * @param ns is the namespace
   * @param s is the scope
   * @param nt is the number of template parameters
   * @param u are the parameters
   * @return the expanded PType
   */
  PType *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  PType() { i = NULL; name = NULL; };

  /* wrong */
  int isEqual (const Type *t) const { return t == this ? 1 : 0; }

  /**
   * @return the instance type associated with the ptype( )
   */
  InstType *getType () { return i; }

private:
  InstType *i;			///< used when it is expanded to hold
				///the ptype(...) parameter

  const char *name;		///< the name of the form ptype(inst)
				///after expansion

  friend class TypeFactory;
};

/**
 * @class Bool
 *
 * @brief Used to represent the built-in ACT bool datatype
 */
class Bool : public Type {
  /**
   * @return the name of the type
   */
  const char *getName() { return "bool"; }
  
  Type *Expand (ActNamespace */*ns*/, Scope */*s*/, int /*nt*/, inst_param */*u*/) {
    return this;
  }
  int isEqual (const Type *t) const { return t == this ? 1 : 0; }
};


/**
 * @class Int
 *
 * @brief Used to represent the built-in ACT int< > datatype. This is
 * also used to represent enumerations.
 */
class Int : public Type {
  /**
   * @return the name of the type
   */
  const char *getName();

  /**
   * Expand integer. This requires one template parameter that
   * specfies the bit-width of the integer data type.
   * @param ns is the namespace
   * @param s is the scope
   * @param nt is the number of template parameters
   * @param u has the single parameter
   */
  Int *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  /**
   * @return 1 if the type t is equal to this one, 0 otherwise
   */
  int isEqual (const Type *t) const;
  
  unsigned int kind:2;		///< 0 = unsigned, 1 = signed, 2 = enum
  int w;		        ///< bit-width for integers, the
				///number of values for enumeration
  
  const char *name;		///< string name for the type (for printing)
  
  friend class TypeFactory;
};

/**
 * @class Chan
 * 
 * @brief Used to represent the built-in ACT Paramterized chan(foo)
 * type or chan(foo,foo) bi-directional/exchange channel.
 */
class Chan : public Type {
  /**
   * @return the name of the type
   */
  const char *getName ();

  /**
   * Expand the channel type. The number of tempate parameters is
   * either 1 (normal channel) or 2 (bi-directional/exchange
   * channel). 
   *
   * @param ns is the namespace
   * @param s is the scope
   * @param nt is the number of template parameters
   * @param u holds type parameters
   */
  Chan *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  /**
   * @return 1 if t is equal to this type, 0 otherwise
   */
  int isEqual (const Type *t) const;
  

  const char *name;	      ///< the string name for the type
  InstType *p;		      ///< data type for expanded channel
  InstType *ack;	      ///< second data type for exchange channel

public:
  /**
   * @return the channel data type
   */
  InstType *datatype() const { return p; }

  /**
   * @return the "acknowledge" data type, the backward direction
   */
  InstType *acktype() const { return ack; }

  /**
   * @return 1 if this is a bi-directional channel, 0 otherwise
   */
  int isBiDirectional() { return ack == NULL ? 0 : 1; }

  friend class TypeFactory;
};


class InstType;
class UserMacro;



/**
 *  @class UserDef
 *
 *  @brief UserDef stores information that is common to all
 *  user-defined types. User-defined types are more complex, because
 *  they can have definitions and template parameters. To handle this
 *  complexity and to reduce storage requirements, the "shared" part
 *  of a user-defined type is factored out into this class. That way
 *  the body of a channel x1of2 is shared with x1of2? and x1of2!, as
 *  well as an array of x1of2 (for example).
 *
 *  UserDef types can be either a Process, Data, Channel, Function, or
 *  Interface.
 *
 *  Parameters to the type are 
 *   - ports, which correspond to signals/hardware
 *   - template parameters, which correspond to circuit construction
 *  parameters
 *
 */
class UserDef : public Type {
 public:
  /**
   * @param ns is the namespace in which the type is defined
   */
  UserDef (ActNamespace *ns);

  /**
   * This moves all the information from the specified user-defined
   * type over into this one.
   * @param x is a previously created UserDef type
   */
  UserDef (UserDef *x);

  virtual ~UserDef (); ///< destructor, releases storage

  /**
   * Get file name where this was defined
   * @return the file name
   */
  const char *getFile() { return file; }

  /**
   * Set file name for this type
   * @param s is the file name
   */
  void setFile(const  char *s) { file = string_cache (s); }

  /**
   * @return the line number where this was defined
   */
  int getLine() { return lineno; }

  /**
   * Set the line number where this type was defined
   * @param num is the line number
   */
  void setLine(int num) { lineno = num; }
  
  /**
   * Specifies if this is an exported user-defined type or not
   *
   * @return 1 if this is an exported type, 0 otherwise
   */
  int IsExported () { return exported; }

  /**
   * Set this to be an exported type
   */
  void MkExported () { exported = 1; }

  /**
   * @return 1 if the type is expanded, or 0 if it is not
   */
  int isExpanded() const { return expanded; }
  

  /**
   * Add a new template ("meta") parameter
   *
   *  @param t is the type of the parameter
   *  @param id is the the identifier for this parameter
   *
   *  @return 1 on success, 0 on error (duplicate parameter name)
   */
  int AddMetaParam (InstType *t, const char *id);

  /**
   * Add a new port
   *
   *  @param t is the type of the port
   *  @param id is the the identifier for this parameter
   *
   *  @return 1 on success, 0 on error (duplicate parameter name)
   */
  int AddPort (InstType *t, const char *id);


  /**
   * Find a port. Returns the port number encoded in the following
   * way:
   *   - if the parameter is a port at position k in the port list,
   * the value returned is (k+1)
   *   - if the parameter is a port at position k in the
   * meta-parameter/template parameter list, the value returned is
   * -(k+1)
   *   - if the name is not in the port list, 0 is returned
   *
   * @param id is the name of the port
   * @return 0 if not found. Positive values mean ports; negative
   * values mean template parameters
   */
  int FindPort (const char *id);

  /**
   * Returns name of port at specified position. 0..k are the port
   * list, -1 to -(l+1) are the template parameters.
   * 
   * @param pos is the position of the port in the port list. Negative
   * numbers indicate meta-parameters, 0 and positive parameters
   * indicate physical port parameters.
   * @return the name of the specified port
   */
  const char *getPortName (int pos) const;

  
  /**
   * Returns the type of the port at specified position. 0..k are the port
   * list, -1 to -(l+1) are the template parameters.
   * 
   * @param pos is the position of the port in the port list. Negative
   * numbers indicate meta-parameters, 0 and positive parameters
   * indicate physical port parameters.
   * @return the name of the specified port
   */
  InstType *getPortType (int pos) const;

  /**
   * Used for overrides. Refine the type for the port parameter at the
   * specified position.
   * @param pos is 0..k for the port list (temlpate parameters are
   * never refined)
   * @param u is the updated instance type
   */
  void refinePortType (int pos, InstType *u);

  /**
   * @return the name of the type. For expanded types, this uses the
   * expanded type naming convention.
   */
  const char *getName ();
  
  /**
   * @return a freshly allocated string with full name (including
   * namespace) for the type.
   */
  char *getFullName(); 
  

  /**
   * Print the ACT name for the type.  WARNING: this does not do the
   * right thing for array template parameters.
   * @param fp is the output file
   */
  void printActName (FILE *fp);

  int isEqual (const Type *t) const;

  /**
   * Set the name of the type. Used to share the string pointer
   * between this data structure and the hash table that holds the
   * type name as well.
   * @param s is the new string pointer to use for the type name
   */
  void setName (const char *s) { name = s; }

  /**
   * Make a copy: copy over all fields from u, and clear the fields of
   * u at the same time.
   * @param u is the user-defined type to "take over"
   */
  void MkCopy (UserDef *u);

  /**
   * Compare user-defined types to see if the port parameters match
   * @param u is the user-defined type to compare with
   * @return 1 if the types are equal, 0 otherwise
   */
  int isEqual (const UserDef *u) const;

  /**
   * Setup parent relationship for the imlementation relation between
   * types.
   *
   * @param t is the parent type
   */
  void SetParent (InstType *t);

  /**
   * @return the parent type in the implementation hierarchy, if any
   */
  InstType *getParent () const { return parent; }

  /**
   * Specify if this type has been fully defined yet or not. It may
   * only be a declared type at this point.
   * @return 1 if the type has been defined, 0 if it has only been
   * declared.
   */
  int isDefined () { return defined; }

  /**
   * Mark this type as defined
   */
  void MkDefined () { defined = 1; }

  /**
   * Is this parameter a port?
   *
   * @param name is the name of the instance to be checked.
   * @return 1 if name is a port, 0 otherwise
   */
  int isPort(const char *name);

  /**
   * @return the number of template parameters
   */
  int getNumParams () const { return nt; }

  /**
   * Template parameters may be explicitly declared, or inherited by
   * un-specified template parameters in parent types. The "remaining"
   * template parameters are the fresh ones for this user-defined
   * type.
   *
   * @return the number of remaining template parameters
   */
  int getRemainingParams() const { return nt - inherited_templ; }

  /**
   * @return the number of ports
   */
  int getNumPorts () const { return nports; }

  /**
   * Looks up an identifier within the scope of the body of the type
   * @param id is the ActId to look up
   * @return the type for the identifier in the local scope, or NULL
   * if it does not exist. Essentially a direct way to call Lookup()
   * for the scope.
   */
  InstType *Lookup (ActId *id) { return I->Lookup (id, 0); }
  
  /**
   * Looks up an identifier within the scope of the body of the type
   * @param nm is the name to look up
   * @return the type for the identifier in the local scope, or NULL
   * if it does not exist. Essentially a direct way to call Lookup()
   * for the scope.
   */
  InstType *Lookup (const char *nm) { return I->Lookup (nm); }

  /**
   * @return the scope pointer for the user-defined type
   */
  Scope *CurScope() { return I; }

  /**
   * THIS IS NOT USED NOW. Always returns 1.
   * @param name is the name of the port to be checked
   * @return 1 if this is a port name that is also in the "strict"
   * parameter list
   */
  int isStrictPort (const char *name);

  /**
   * Print out user defined type 
   */
  virtual void Print (FILE */*fp*/) { }

  /**
   * Print out the header for the type. This function can be used for
   * all the different ways a UserDef can be used. The type field is
   * used to print out one of the following strings: "defproc",
   * "deftype", "defchan", "interface", "function".
   * @param fp is the output file
   * @param type is the string for the kind of user-defined type. 
   */
  void PrintHeader (FILE *fp, const char *type);

  /**
   * Directly set the body of the user-defined type 
   * @param x the body to assign to this user-defined type
   */
  void setBody (ActBody *x) { b = x; }

  /**
   * Append to the current body of the user-defined type
   * @param x is the additional body to append
   */
  void AppendBody (ActBody *x);

  /**
   * @return the current body of the user-defined type
   */
  ActBody *getBody () { return b; }

  /**
   * Here in case someone called the wrong Expand!
   */
  UserDef *Expand (ActNamespace */*ns*/, Scope */*s*/, int /*nt*/, inst_param */*u*/) {
    Assert (0, "Don't call this ever");
  }

  /**
   * Expand the user-defined type
   * @param ns is the namespace
   * @param s is the scope
   * @param nt is the number of specified template parameters
   * @param u holds the template parameters
   * @param cache_hit returns 1 on a hit (i.e. found a previously
   * expanded identical type that is being returned), 0 otherwise
   * @param is_process is 0 by default, 1 for processes, 2 for
   * functions
   * @return expanded type either from the construction, or from the cache
   */
  UserDef *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u, int *cache_hit, int is_process = 0);

  /**
   * @return the namespace in which the user-defined type was defined
   */
  ActNamespace *getns() { return _ns; }

  /**
   * This should only be called for user-defined data types or
   * channels.
   *
   * @return the root type for this one in the implementation
   * hierarchy. NULL if there is no root non user-defined type.
   */
  InstType *root() const;

  /**
   * @return the prs body (NULL if it does not exist)
   */
  act_prs *getprs ();

  /**
   * @return the spec body (NULL if it does not exist)
   */
  act_spec *getspec ();

  /**
   * @return all the sub-language bodies, NULL if there are none
   */
  act_languages *getlang () { return lang; }

  /**
   * Only useful for process types.
   * @return 1 if there are no sub-circuits within this type, 0 otherwise
   */
  int isLeaf();

  /**
   * Mark this as a type that has a refinement body
   */
  void mkRefined() { has_refinement = 1; }

  /**
   * @return 1 if this has a refinement body, 0 otherwise
   */
  int hasRefinement() { return has_refinement; }

  /**
   * Create a new user-defined macro
   * @param name is the name of the macro
   * @return the blank macro
   */
  UserMacro *newMacro (const char *name);

  /**
   * Return a user-defined macro
   * @param name is the name of the macro
   * @return the user-defined macro (NULL if it doesn't exist)
   */
  UserMacro *getMacro (const char *name);

 protected:
  InstType *parent;		///< implementation relationship, if any
  
  unsigned int defined:1;	///< 1 if this has been defined, 0 otherwise
  unsigned int expanded:1;	///< 1 if this has been expanded, 0 otherwise

  unsigned int pending:1;	///< 1 if this is currently being expanded, 0 otherwise

  unsigned int exported:1;      ///< 1 if the type is exported, 0 otherwise

  act_languages *lang;		///< sub-languages within this type
  
  int nt;			///< number of template parameters
  InstType **pt;		///< parameter types
  const char **pn;		///< parameter names

  int nports;			///< number of ports
  InstType **port_t;		///< port types
  const char **port_n;		///< port names

  Scope *I;			///< instances

  const char *name;		///< Name of the user-defined type

  ActBody *b;			///< body of user-defined type
  ActNamespace *_ns;		///< namespace within which this type is defined 

  UserDef *unexpanded;		///< unexpanded type, if any

  int level;		        ///< default modeling level for the type

  const char *file; ///< file name (if known) where this was defined
  int lineno;       ///< line number (if known) where this was defined
  int has_refinement;	      ///< 1 if there is a refinement body

  int inherited_templ;       ///< number of inherited template parameters
  inst_param **inherited_param;  ///< the inherited parameters

  /**
   * Used to print macros in the type
   * @param fp is the output file
   * @return 1 if there are some macros, 0 otherwise
   */
  int emitMacros (FILE *fp);

  A_DECL (UserMacro *, um);   ///< user-defined macros
};


/**
 *
 * @class Interface
 *
 * @brief Holds the interface definition. Looks like a process. Body
 * is empty.
 *
 */
class Interface : public UserDef {
 public:
  Interface (UserDef *u);
  ~Interface ();

  Interface *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);
};


/**
 *
 * @class Process
 *
 * @brief User-defined processes.
 *
 */
class Process : public UserDef {
 public:
  /**
   * Construct a process from a UserDef
   * @param u is the UserDef
   */
  Process (UserDef *u);
  virtual ~Process ();

  /**
   * This process is actually a defcell
   */
  void MkCell () { is_cell = 1; }

  /**
   * @return 1 if this is a cell, 0 otherwise
   */
  int isCell() { return is_cell; }

  Process *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  void Print (FILE *fp);

  /**
   * @return 1 if this is a black-box definition, 0 otherwise
   */
  int isBlackBox();

  /**
   * A low level blackbox: might have CHP, HSE, or dataflow models,
   * but does not have any circuits (prs) and is a leaf cell. Low
   * level black boxes are used to provide a simulation model, but
   * where the circuit implementation is provided by a macro. This is
   * used for memories, for example.
   *
   * @return 1 if this is a low-level black box, 0 otherwise
   */
  int isLowLevelBlackBox ();	
				
  /**
   * Add an interface specification that this process exports. Note
   * that a process can export multiple interfaces.
   *
   * @param iface is the interface exported
   * @param imap is the name mapping from the process ports to the
   * interface ports
   */
  void addIface (InstType *iface, list_t *imap);

  /**
   * Weak check for interface equality
   */
  int hasIface (InstType *x, int weak);

  /**
   * @return the port name map for the interface exported by this
   * process
   */
  list_t *findMap (InstType *iface);

  /*-- edit API --*/

  /**
   *  Take "name" and replace its base type with "t"
   *  For this to succeed, "t" must have exactly the same port list 
   *  as the orginal type for "name"
   * 
   *  @return true on success, false on failure.
   */
  bool updateInst (char *name, Process *t);

  /**
   *
   *  Buffer insertion
   *
   *    Take name.port, disconnect it from whatever it is driving,
   *    and then create a new instance "buf" (has to have one input
   *    port and one output port).
   *
   *    port has to be either a simple id or id[num]
   *
   *    @param name is the instance name
   *    @param port is the port within the instance
   *    @param buf is the buffer type
   *
   *    @return the name of the newly created buffer instance, or NULL
   *    if it failed.
   *
   */
  const char *addBuffer (char *name, ActId *port, Process *buf);

  /**
   * Similar to the single buffer addition, except that a list of
   * end-points are disconnected. Each of them must be connected to
   * the same primary name, must be disconnectable, and must all be
   * input pins
   */
  const char *addBuffer (Process *buf, list_t *inst_ports);

  /**
   * @return the unexpanded type from which this one was generated
   */
  Process *getUnexpanded();

  /** 
   * record the usage of a global signal 
   * @param id is the global signal name
   */
  void recordGlobal (ActId *id);

  /**
   * @param id is the name of the signal
   * @return 1 if the global signal was found in the recorded global
   * signal list, 0 otherwise.
   */
  int findGlobal (ActId *id);

  /**
   * @param s is the name of the signal
   * @return 1 if the global signal was found in the recorded global
   * signal list, 0 otherwise 
   */
  int findGlobal (const char *s);
    
 private:
  unsigned int is_cell:1;	///< 1 if this is a defcell, 0 otherwise 
  list_t *ifaces;		///< a mixed list of interface, map
				///pairs. The map is also a list of
				///oldname, newname pairs

  list_t *used_globals;		///< list of used globals as ActId
				///pointers

  int bufcnt;			///< used to generate unique buffer
				///names for buffer insertion
};


/**
 *
 * @class Function
 *
 * @brief This holds information about ACT functions. ACT functions
 * are of two types:
 *   - parameter functions: all arguments and return types are
 * parameter types. These functions are evaluated at expansion time
 * and replaced by constant values in the circuit.
 *   - run-time functions: all arguments/return values are
 * non-parameter types. These functions are used in CHP and dataflow
 * bodies.
 *
 * Functions can also be externally defined in C.
 *
 * Looks like a process. The ActBody consists of a chp body, nothing
 * else.
 *
 * A function can be one that is amenable to a simple inlining
 * operation. This is the case for any function that does not have an
 * internal loop, since conditional assignments can be converted into
 * a normal expression using the "? : " operator.
 *
 */
class Function : public UserDef {
 public:
  Function (UserDef *u);
  ~Function ();

  /**
   * Set the return type for the function
   * @param i is the return type
   */
  void setRetType (InstType *i) { ret_type = i; }

  /**
   * @return the function return type
   */
  InstType *getRetType () { return ret_type; }

  /**
   * Expand the function
   */
  Function *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  /**
   * Print the function
   */
  void Print (FILE *fp);

  /**
   * Evaluate the function. This should only be called for parameter
   * functions. The arguments must all be constant values
   * (i.e. already evaluated by the caller).
   *
   * @param ns is the namespace 
   * @param nargs are the number of arguments passed into the function
   * @param args is the list of arguments
   * @return the result of evaluating the expression
   */
  Expr *eval (ActNamespace *ns, int nargs, Expr **args);

  /**
   * Symbolic evaluation of a function. Returns an array of
   * expressions. The # of expressions corresponds to the # of
   * flattened components in a structure. This does not work if the
   * function has an internal loop. The isSimpleInline() method can be
   * used to determine if this function can be called.
   *
   * @param nargs are the number of arguments
   * @param args are the arguments
   * @return the symbolic result
   */
  Expr **toInline (int nargs, Expr **args);

  /**
   * @return 1 if this is an external function, 0 otherwise
   */
  int isExternal ();
  
  /**
   * @return 1 if this is a simple inline---an inline that can be
   * expressed as a simple expression.
   */
  
  int isSimpleInline () { return is_simple_inline; }

  /**
   * This computes the "simple inline" flag
   */
  void chkInline ();
  
 private:
  InstType *ret_type;		///< holds return type
  int is_simple_inline;		///< holds the simple inline flag

  void _chk_inline (Expr *e);	///< used to check simple inline
  void _chk_inline (struct act_chp_lang *c); ///< used to check simple inline
};


#define ACT_NUM_STD_METHODS 8    ///< number of standard methods for
				 ///data/channel types

#define ACT_NUM_EXPR_METHODS 2  ///< number of "expression" methods
				///that can be specified

/**
 * A pre-defined array that holds all the method names supported for
 * channel and data types.
 */
extern const char *act_builtin_method_name[ACT_NUM_STD_METHODS];

/**
 * A pre-defined array that holds all the method names corresponding
 * to methods that return an expression
 */
extern const char *act_builtin_method_expr[ACT_NUM_EXPR_METHODS];

/**
 * For each built-in method that returns an expression, this is 1 if
 * the expression return type is Boolean, 0 otherwise.
 */
extern const int act_builtin_method_boolret[ACT_NUM_EXPR_METHODS];


/**
 * This is used as an index into a methods table for channel and
 * user-defined types.
 */
enum datatype_methods {
    ACT_METHOD_SET = 0,		///< the set method used for channel
				///and data types
    
    ACT_METHOD_GET = 1,		///< the get method used for channel
				///and data types
    
    ACT_METHOD_SEND_REST = 2,	///< the reset part of the sender
				///handshake, if any
    
    ACT_METHOD_RECV_REST = 3,	///< the reset part of the receive
				///handshake, if any
    
    ACT_METHOD_SEND_UP = 4,	///< the second part of the first half
				///of the send handshake, if any
    
    ACT_METHOD_RECV_UP = 5,	///< the second part of the first half
				///of the receive handshake, if any
    
    ACT_METHOD_SEND_INIT = 6,	///< on Reset, the initialization for
				///the sender end of the channel
    
    ACT_METHOD_RECV_INIT = 7,	///< on Reset, the initialization for
				///the receiver end of the channel
    
    ACT_METHOD_SEND_PROBE = 0 + ACT_NUM_STD_METHODS, ///< expression
						     ///method for
						     ///sender probe
    
    ACT_METHOD_RECV_PROBE = 1 + ACT_NUM_STD_METHODS ///< expression
						    ///method for recver probe
};


/**
 *
 * @class Data
 * 
 * @brief A user-defined data types
 *
 * A data type can implement a built-in data type (int/bool/enum), or
 * be a structure.
 *
 */
class Data : public UserDef {
 public:
  Data (UserDef *u);
  virtual ~Data();

  int isEqual (const Type *t) const;
  void MkEnum (int is_int) { is_enum = 1; is_eint = is_int ? 1 : 0; }
  int isEnum () const  { return is_enum; }
  int isPureEnum() const { return (is_enum && !is_eint) ? 1 : 0; }
  void addEnum (const char *s) {
    if (!enum_vals) {
      enum_vals = list_new ();
    }
    list_append (enum_vals, s);
  }
  int numEnums() const {
    if (enum_vals) {
      return list_length (enum_vals);
    }
    return 0;
  }
  
  int enumVal (const char *s) const {
    int i = 0;
    if (!enum_vals) return -1;
    for (listitem_t *li = list_first (enum_vals); li; li = list_next (li)) {
      if (strcmp ((const char *)list_value (li), s) == 0) {
	return i;
      }
      i++;
    }
    return -1;
  }

  void setMethod (int t, struct act_chp_lang *h) { methods[t] = h; }
  struct act_chp_lang *getMethod (int t) { return methods[t]; }
  void copyMethods (Data *d);
 
  Data *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);
  
  void Print (FILE *fp);

  /* returns # of booleans and # of ints needed to implement the
     structure */
  void getStructCount (int *nbools, int *nints);

  /* returns offset of field within  the structure.
     if sz is non-NULL, also returns the # of entries in case this is
     a sub-structure
  */
  int getStructOffset (ActId *field, int *sz);
  ActId **getStructFields (int **types);

private:
  void _get_struct_count (int *nbools, int *nints);
  void _get_struct_fields (ActId **a, int *types, int *pos, ActId *prefix);
  
  unsigned int is_enum:1;	/**< 1 if this is an enumeration, 0
				   otherwise */
  unsigned int is_eint:1;	/**< 1 if this enum can be treated as
				   an int */

  struct act_chp_lang *methods[ACT_NUM_STD_METHODS]; /**< set and
							get methods
							for this data type */
  list_t *enum_vals;
};




class Channel : public UserDef {
 public:
  Channel (UserDef *u);
  virtual ~Channel();
  
  void setMethod (int t, act_chp_lang *h) { methods[t] = h; }
  void setMethod (int t, Expr *e) { emethods[t-ACT_NUM_STD_METHODS] = e; }
  act_chp_lang *getMethod(int t) { return methods[t]; }
  Expr *geteMethod(int t) { return emethods[t-ACT_NUM_STD_METHODS]; }
  void copyMethods (Channel *c);

  Channel *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);
  
  void Print (FILE *fp);

  int chanDir (ActId *id, int isinput);
  // given that the id has the direction specified, what is the
  // channel direction?  1 = input, 2 = output, 3 = both, 0 = undetermined
  // id is typically a fragmented piece of the channel, and hence this
  // involves examining the channel methods as well.

  int mustbeActiveSend ();
  // return 1 if send has to be active, 0 if it has to be passive, -1
  // if not determined by the channel type.

  int mustbeActiveRecv ();
  // return 1 if recv has to be active, 0 if it has to be passive, -1
  // if not determined by the channel type.

  int isBiDirectional() {
    Assert (parent, "what?");
    Channel *p = dynamic_cast<Channel *>(parent->BaseType());
    if (p) {
      return p->isBiDirectional();
    }
    else {
      Chan *p = dynamic_cast<Chan *>(parent->BaseType());
      Assert (p, "What?");
      return p->isBiDirectional();
    }
  }
 private:
  struct act_chp_lang *methods[ACT_NUM_STD_METHODS];
  Expr *emethods[ACT_NUM_EXPR_METHODS];
};

struct act_inline_table;

class UserMacro {
public:
  UserMacro (UserDef *u, const char *name);
  ~UserMacro ();

  void Print (FILE *fp);
  UserMacro *Expand (UserDef *ux, ActNamespace *ns, Scope *s, int is_proc);

  int addPort (InstType *it, const char *name);

  const char *getName () { return _nm; }
  int getNumPorts() const { return nports; }
  const char *getPortName (int i) const { return port_n[i]; }
  InstType *getPortType (int i) const { return port_t[i]; }

  void setBody (struct act_chp_lang *);

  struct act_chp_lang *substitute (ActId *instnm, act_inline_table *tab); 

private:
  const char *_nm;
  UserDef *parent;		/**< user-defined type with this macro */

  int nports;			/**< number of ports */
  InstType **port_t;		/**< port types */
  const char **port_n;		/**< port names */

  struct act_chp_lang *c;	/**< body */
};



class TypeFactory {
 private:
  static TypeFactory *tf;
  /**
   * Built-in parameter types: only one copy of these can exist, since
   * they do not have any parameters themselves
   */
  static InstType *pint, *pints;
  static InstType *preal;
  static InstType *pbool;

  /**
   * Five types of bool types: NONE, IN, OUT, INOUT, OUTIN 
   */
  static InstType *bools[5];

  /**
   * Const exprs
   */
  static Expr *expr_true;
  static Expr *expr_false;
  static struct iHashtable *expr_int;

  /**
   * Hash table for integer types parameterized by bit-width and
   * direction flags.
   */
  static struct cHashtable *inthash;
  
  /**
   * Hash table for enumeration types parameterized by the range of
   * the enumeration and direction flags.
   */
  static struct cHashtable *enumhash;

  /**
   * Hash table for ptype types. Uses channel hash functions.
   */
  static struct cHashtable *ptypehash;

  /**
   * Hash table for const exprs 
   */
  static struct cHashtable *ehash;

  /**
   * Helper functions for hash table operations.
   * ALSO used for enumeration hashes.
   */
  static int inthashfn (int, void *);
  static int intmatchfn (void *, void *);
  static void *intdupfn (void *);
  static void intfreefn (void *);

  /**
   * Hash table for channel types parameterized by direction, and type
   * list
   */
  static struct cHashtable *chanhash;

  /**
   * Helper functions for hash table operations
   */
  static int chanhashfn (int, void *);
  static int chanmatchfn (void *, void *);
  static void *chandupfn (void *);
  static void chanfreefn (void *);

 public:
  TypeFactory();
  ~TypeFactory();

  void *operator new (size_t);
  void operator delete (void *);

  /** 
   * Return unique pointer to the parameterized type
   */
  InstType *NewPInt ()  { return pint; }
  InstType *NewPInts ()  { return pints; }
  InstType *NewPBool () { return pbool; }
  InstType *NewPReal () { return preal; }
  InstType *NewPType (Scope *s, InstType *t);
  PType *NewPType (InstType *t);

  /**
   * Return unique pointer to the bool type
   * \param dir is the direction flag for the type
   * \return a unique Bool * corresponding to the type
   */
  InstType *NewBool (Type::direction dir);

  /**
   * Return unique pointer to the int type
   *
   * \param dir is the direction flag for the type
   * \param w is an integer expression that specifies the width of the
   * integer (if it is a constant expression, the type can be
   * finalized)
   * \param s is the scope in which this integer is created. This
   * scope is used to evaluate any variables specified in w
   * \param sig is 1 if this is a signed integer, 0 otherwise
   * \return a unique pointer to the specified type
   */
  InstType *NewInt (Scope *s, Type::direction dir, int sig, Expr *w);
  Int *NewInt (int sig, int w);

  /**
   * Return unique pointer to the enum type
   *
   * \param dir is the direction flag for the type
   * \param w is an integer expression that specifies the width of the
   * integer (if it is a constant expression, the type can be
   * finalized)
   * \param s is the scope in which this integer is created. This
   * scope is used to evaluate any variables specified in w
   * \return a unique pointer to the specified type
   */
  InstType *NewEnum (Scope *s, Type::direction dir, Expr *w);

  /**
   * Return unique pointer to the chan type
   *
   * \param dir is the direction flag for the type
   * \param s is the scope in which the channel is created
   * \param n are the number of types in the type list for the channel
   * \param l is an array that corresponds to the list of types for
   * the channel
   * \return a unique pointer to the specified channel type
   */
  InstType *NewChan (Scope *s, Type::direction dir, InstType *l, InstType *ack);
  Chan *NewChan (InstType *l, InstType *ack);

  /**
   * Returns a unique pointer to the instance type specified
   * \param it is the input instance type. This MAY BE FREE'D
   * \param s is the scope in which the user-defined type is being created
   * \return a unique pointer to the specified instance type
   */
  InstType *NewUserDef (Scope *s, InstType *it);


  /**
   * Returns a unique pointer to a constant expression
   */
  static Expr *NewExpr (Expr *e);

  static TypeFactory *Factory() { return tf; }
  
  /** 
   * Initialize and allocate the first type factory object 
   */
  static void Init ();


  /**
   * Determines if the specified type is a data type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid data type, 0 otherwise
   */
  static int isDataType (const Type *t);
  static int isDataType (const InstType *t);

  static int isIntType (const Type *t);
  static int isIntType (const InstType *t);

  static int isPIntType (const Type *t);
  static int isPIntType (const InstType *t);

  static int isPIntsType (const Type *t);
  static int isPIntsType (const InstType *t);

  static int isBoolType (const Type *t);
  static int isBoolType (const InstType *it);
  
  static int isPBoolType (const Type *t);
  static int isPBoolType (const InstType *it);

  static int isPRealType (const Type *t);
  static int isPRealType (const InstType *it);

  static int isUserType (const Type *t);
  static int isUserType (const InstType *it);

  /**
   * Determines if the specified type is a channel type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid channel type, 0 otherwise
   */
  static int isChanType (const Type *t);
  static int isChanType (const InstType *it);

  /* 1 only on ``chan(...)'', not on userdefined channels */
  static int isExactChanType (const Type *t);
  static int isExactChanType (const InstType *it);

  /* 1 only for  valid data types within a chan(...) 
     Assumes that "t" is in fact a data type as a pre-condition.
   */
  static int isValidChannelDataType (const Type *t);
  static int isValidChannelDataType (const InstType *t);

  /**
   * Determines if the specified type is a process type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid process/cell, 0 otherwise
   */
  static int isProcessType (const Type *t);
  static int isProcessType (const InstType *it);

  /**
   * Determines if the specified type is a function type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid function, 0 otherwise
   */
  static int isFuncType (const Type *t);
  static int isFuncType (const InstType *it);


  /**
   * Determines if the specified type is an interface type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid process/cell, 0 otherwise
   */
  static int isInterfaceType (const Type *t);
  static int isInterfaceType (const InstType *it);
  
  /**
   * Determines if the specified type is a ptype or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid ptype type, 0 otherwise
   */
  static int isPTypeType (const Type *t);
  static int isPTypeType (const InstType *it);


  /**
   * Determines if the specified type is a parameter type
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid ptype type, 0 otherwise
   */
  static int isParamType (const Type *t);
  static int isParamType (const InstType *it);

  /**
   * Determines the bit-width of the type
   * The type must be either a channel or a data type. If it isn't,
   * the function returns -1
   */
  static int bitWidth (const Type *t);
  static int bitWidth (const InstType *t);

  /**
   * For bidirectional channels only. Returns 0 for normal channels,
   * -1 for non-channels
   */
  static int bitWidthTwo (const Type *t);
  static int bitWidthTwo (const InstType *t);
  
  static int boolType (const Type *t); // 1 if a boolean within a channel or
				// a boolean type, -1 if NULL parent
  static int boolType (const InstType *t);

  static int isStructure (const Type *t);
  static int isStructure (const InstType *it);

  /**
   * Is this a user-defined enumeration?
   */
  static int isUserEnum (const Type *t);
  static int isUserEnum (const InstType *it);
  static int isUserPureEnum (const Type *t);
  static int isUserPureEnum (const InstType *it);

  /**
   * Return enumeartion width, -1 if not a nenumeration
   */
  static int enumNum (const Type *t);
  static int enumNum (const InstType *t);

  /**
   * Return 1 if this is a user-defined or built-in enumeration
   */
  static int isEnum (const Type *t);
  static int isEnum (const InstType *t);
  
  /*-- a user type that is rooted in a bool or a bool --*/
  static int isBaseBoolType (const Type *t);
  static int isBaseBoolType (const InstType *t);

  /*-- a user type that is rooted in an int or an int --*/
  static int isBaseIntType (const Type *t);
  static int isBaseIntType (const InstType *t);

  /*-- extract data type field from a channel --*/
  static InstType *getChanDataType (const InstType *t);
  static InstType *getChanAckType (const InstType *t);
  
};




/*------------------------------------------------------------------------
 *
 *
 *  Typechecking identifiers and expression: returns -1 on failure
 *
 *
 *------------------------------------------------------------------------
 */
#define T_ERR         -1

#define T_STRICT     0x40   // a port parameter that is in the
			    // "strict" list... i.e. not optional,
			    // i.e. before the vertical bar
#define T_PARAM      0x80   // a parameter type pint/... etc

#define T_INT        0x1
#define T_REAL       0x2
#define T_BOOL       0x4
#define T_PROC       0x5
#define T_CHAN       0x6
#define T_DATA_INT   0x7
#define T_DATA_BOOL  0x8
#define T_SELF       0x9   /* special type, "self" */
#define T_DATA       0xa   /* structure */
#define T_DATA_ENUM  0xb   /* enum that is not an int */
#define T_PTYPE      0x10
#define T_ARRAYOF    0x20

#define T_FIXBASETYPE(x)  ((((x) & 0x1f) == T_DATA_BOOL) ? T_BOOL : ((((x) & 0x1f) == T_DATA_INT) ? T_INT : ((x) & 0x1f)))

#define T_BASETYPE(x) ((x) & 0x1f)

#define T_BASETYPE_ISNUM(x) (T_FIXBASETYPE (x) == T_INT || T_BASETYPE (x) == T_REAL)

#define T_BASETYPE_ISINTBOOL(x) ((T_FIXBASETYPE (x) == T_INT) || (T_FIXBASETYPE (x) == T_BOOL))

#define T_BASETYPE_INT(x) (T_FIXBASETYPE(x) == T_INT)

#define T_BASETYPE_BOOL(x) (T_FIXBASETYPE(x) == T_BOOL)


int act_type_expr (Scope *, Expr *, int *width, int only_chan = 0);
int act_type_var (Scope *, ActId *, InstType **xit);
int act_type_chan (Scope *sc, Chan *ch, int is_send, Expr *e, ActId *id,
		   int override_id);

int act_type_conn (Scope *, ActId *, AExpr *);
int act_type_conn (Scope *, AExpr *, AExpr *);
int type_connectivity_check (InstType *lhs, InstType *rhs, int skip_last_array = 0);
int type_chp_check_assignable (InstType *lhs, InstType *rhs);

InstType *act_expr_insttype (Scope *s, Expr *e, int *islocal, int only_chan);
InstType *act_actual_insttype (Scope *s, ActId *id, int *islocal);

void type_set_position (int l, int c, char *n);
const char *act_type_errmsg (void);


/*------------------------------------------------------------------------
 *
 *  Helper functions for expression manipulation
 *
 *------------------------------------------------------------------------
 */
int expr_equal (const Expr *a, const Expr *b);

void print_expr (FILE *fp, const Expr *e);
void sprint_expr (char *buf, int sz, const Expr *e);

/* unsigned variations of the functions above */
void print_uexpr (FILE *fp, const Expr *e);
void sprint_uexpr (char *buf, int sz, const Expr *e);

int expr_is_a_const (Expr *e);
Expr *expr_dup_const (Expr *e);

/* 
   returns a string constant for the expression operator 
*/
const char *expr_op_name (int);

/* unified expression expansion code, with flags to control
   different variations */
#define ACT_EXPR_EXFLAG_ISLVAL   0x1
#define ACT_EXPR_EXFLAG_PARTIAL  0x2
#define ACT_EXPR_EXFLAG_CHPEX    0x4
#define ACT_EXPR_EXFLAG_DUPONLY  0x8

extern int _act_chp_is_synth_flag;
Expr *expr_expand (Expr *e, ActNamespace *ns, Scope *s, unsigned int flag = 0x2);

#define expr_dup(e) expr_expand ((e), NULL, NULL, ACT_EXPR_EXFLAG_DUPONLY)

/* free an expanded expression */
void expr_ex_free (Expr *);

/*-- more options for expanded expressions --*/

#define E_TYPE  (E_END + 10)  /* the "l" field will point to an InstType */

#define E_ARRAY (E_END + 11) /* an expanded paramter array
				- the l field will point to the ValueIdx
				- the r field will point to the Scope 
			     */

#define E_SUBRANGE (E_END + 12) /* like array, but it is a subrange
				   - l points to the ValueIdx
				   - r points to another Expr whose
				         l points to Scope
					 r points to the array range
				*/

#define E_SELF (E_END + 20)
#define E_SELF_ACK (E_END + 19)

/* 
   For loops:
      e->l->l = id, e->r->l = lo, e->r->r->l = hi, e->r->r->r = expr
      WARNING: replicated in expr_extra.c
*/
#define E_ANDLOOP (E_END + 21) 
#define E_ORLOOP (E_END + 22)
#define E_BUILTIN_BOOL (E_END + 23)
#define E_BUILTIN_INT  (E_END + 24)

/*
  ENUM_CONST during parsing only used for
     ::foo::bar::baz.N  
     u.fn.s field == string to enum
     u.fn.r field = string for N
     
  After the "walk" re-writing, this is used as:
     u.fn.s = enum type pointer (Data *)
     u.fn.r field = string corresponding to enum element

 After type-checking, this is eliminated and replaced with an 
 int const.
*/
#define E_ENUM_CONST   (E_END + 25)


#define E_NEWEND  E_END + 26

/*
  Push expansion context 
*/
void act_error_push (const char *s, const char *file, int line);
void act_error_update (const char *file, int line); // set file to
						    // NULL to keep
						    // the file name
void act_error_pop ();
void act_error_ctxt (FILE *);
const char *act_error_top ();
void act_error_setline (int line);

/*
  External functions for core act library must 
  be of the form

  long function_name (int nargs, long *args);
*/

struct ExtLibs;
void *act_find_dl_func (struct ExtLibs *el, ActNamespace *ns, const char *f);
struct ExtLibs *act_read_extern_table (const char *prefix);


void typecheck_err (const char *s, ...);

extern "C" {

Expr *act_parse_expr_syn_loop_bool (LFILE *l);
Expr *act_parse_expr_intexpr_base (LFILE *l);
Expr *act_expr_any_basecase (LFILE *l);
int act_expr_parse_newtokens (LFILE *l);
int act_expr_free_default (Expr *);  

}

#endif /* __ACT_TYPES_H__ */
