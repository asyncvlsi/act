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
#include <act/expr_extra.h>
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
   * @return the unexpanded type from which this one was generated
   */
  UserDef *getUnexpanded();

  /**
   * Add a new template ("meta") parameter
   *
   *  @param t is the type of the parameter
   *  @param id is the the identifier for this parameter
   *  @param def is the default value
   *
   *  @return 1 on success, 0 on error (duplicate parameter name)
   */
  int AddMetaParam (InstType *t, const char *id, AExpr *def);

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

  /**
   * Same as printActName(), but prints into a buffer instead
   * @param buf is the output buffer
   * @param sz is the size of the buffer
   */
  void snprintActName (char *buf, int sz);

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
   * @return default parameter value
   */
  AExpr *getDefaultParam (int p) const { return pdefault[p]; }

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
   * @Param unmangle when set to true, this will unmangle the parent type
   */
  void PrintHeader (FILE *fp, const char *type, bool ummangle = false);

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

  static void mkRefineList(list_t **l, int idx = 1) {
    listitem_t *prev, *li;
    if (!*l) {
      *l = list_new ();
    }
    prev = NULL;
    for (li = list_first (*l); li; li = list_next (li)) {
      if (list_ivalue (li) == idx) {
	// already have it
	return;
      }
      if (list_ivalue (li) > idx) {
	listitem_t *tmp;
	NEW (tmp, listitem_t);
	tmp->idata = idx;
	tmp->next = li;
	if (prev) {
	  prev->next = tmp;
	}
	else {
	  (*l)->hd = tmp;
	}
	return;
      }
    }
    list_iappend (*l, idx);
  }

  /**
   * Mark this as a type that has a refinement body
   */
  void mkRefined(int idx = 1) {
    mkRefineList (&has_refinement, idx);
  }

  /**
   * return refinement list
   */
  list_t *getRefineList() { return has_refinement; }
  void setRefineList(list_t *l) {  has_refinement = l; }

  bool acceptRefine (int refsteps, int mysteps) {
    listitem_t *li;
    int k = 0;

    if (!has_refinement) {
      return false;
    }
    li = list_first (has_refinement);
    while (li && list_ivalue (li) <= refsteps) {
      k = list_ivalue (li);
      li = list_next (li);
    }
    if (k == mysteps) return true;
    return false;
  }

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
  AExpr **pdefault;		///< default parameters

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

  list_t *has_refinement;     ///< list of refinement levels in this type

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
   *    @param assume_input is set to true to allow the assumption
   *    that name.port is an input pin in case there is no direction flag.
   *
   *    @return the name of the newly created buffer instance, or NULL
   *    if it failed.
   *
   */
  const char *addBuffer (char *name, ActId *port, Process *buf, bool assume_input = false);

  /**
   * Similar to the single buffer addition, except that a list of
   * end-points are disconnected. Each of them must be connected to
   * the same primary name, must be disconnectable, and must all be
   * input pins
   */
  const char *addBuffer (Process *buf, list_t *inst_ports);

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


  /**
   * @return true if this is a generated function for a user macro,
   * false otherwise
   */
  bool isUserMethod() {
    for (int i=0; name[i]; i++) {
      if (name[i] == '/') return true;
    }
    return false;
  }
  
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
 * This type is used to implement user-defined enumerations,
 * structures, as well as implementations of int/bool/enum types.
 *
 */
class Data : public UserDef {
 public:
  Data (UserDef *u);
  virtual ~Data();

  int isEqual (const Type *t) const; ///< equality test

  /**
   * Convert this data type into an enumeration. Also indicate if this
   * enumeration can be used as an integer data type or not.
   * @param is_int is 1 to indicate the enumeration can be used in the
   * contexts where an integer is needed
   */
  void MkEnum (int is_int) { is_enum = 1; is_eint = is_int ? 1 : 0; }

  /**
   * @return 1 if this is an enumeration type, 0 otherwise
   */
  int isEnum () const  { return is_enum; }

  /**
   * @return 1 if this is an enumeration and one that cannot be used
   * in an integer context (a "pure" enumeration)
   */
  int isPureEnum() const { return (is_enum && !is_eint) ? 1 : 0; }

  /**
   * Add a new option to an enumeration data type
   * @param s is the name of the new enumeration option
   */
  void addEnum (const char *s) {
    if (!enum_vals) {
      enum_vals = list_new ();
    }
    list_append (enum_vals, s);
  }

  /**
   * @return the number of enumeration options for this data type
   */
  int numEnums() const {
    if (enum_vals) {
      return list_length (enum_vals);
    }
    return 0;
  }

  /**
   * This is used to map enumeration constants to their value
   * @param s is the name of the constant
   * @return the value of the enumeration constant specified, -1 if
   * not found.
   */
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

  /**
   * Set the specified method from the method table to the CHP.
   * @param t is the method type from the datatype_methods enumeration
   * @param h is the method body
   */
  void setMethod (int t, struct act_chp_lang *h) { methods[t] = h; }
  
  /**
   * @param t is the method type from the datatype_methods enumeration
   * @return the method body for the specified method
   */
  struct act_chp_lang *getMethod (int t) { return methods[t]; }

  /**
   * Copy over all the methods from the specified data type. This
   * uses a shallow copy of the method body
   * @param d is the Data type from which the method table should be copied
   */
  void copyMethods (Data *d);

  /**
   * Expand the type
   */
  Data *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  /**
   * Print the type
   */
  void Print (FILE *fp);

  /**
   * Returns # of booleans and # of ints needed to implement the
   * structure.
   * @param nbools used to return the # of bools in the structure
   * @param nints  used to return the # of int/enums in the structure
   */
  void getStructCount (int *nbools, int *nints);

  /**
   * Returns offset of field within the structure.
   * If sz is non-NULL, also returns the # of entries in case this is
   * a sub-structure. The offset corresponds to the index into the
   * structure where the field was found. For fields of the form
   * foo.bar, it is the sum of the offsets for each field (so the
   * offset for "foo", plus the offset of "bar" within "foo".
   * 
   * @param field is the ActId that describes the field
   * @param sz is used to return the size of the field (# of bools +
   * # of ints)
   * @param rit is used to return the type of the field (if non-NULL)
   * @return the offset of the field, -1 if not found
  */
  int getStructOffset (ActId *field, int *sz, InstType **rit = NULL);


  /**
   * Returns offset of field within the structure, separating the
   * integer offset as well as the Boolean offset.
   *
   * @param field is the ActId that describes the field
   * @param boff is used to return the Boolean offset
   * @param ioff is used to return the integer offset
   * @return 1 on success, 0 on failure
  */
  int getStructOffsetPair (ActId *field, int *ioffset, int *boffset);
  

  /**
   * Elaborate the structure into all the "leaf" field names, and also return
   * the types for each field. The type is either 0 or 1, 0 for bools
   * and 1 for ints. Array are expanded out.
   *
   * @param types should be an int array of size # of ints + # of
   * bools for the structure (see getStructCount())
   * @return an ActId array of the same size that contains all the
   * field names for the structure.
   */
  ActId **getStructFields (int **types);


  /**
   * For pure structures, synthesize the macro that converts an
   * integer of the correct bit-width to the structure
   */
  void synthStructMacro ();

private:
  void _get_struct_count (int *nbools, int *nints);
  void _get_struct_fields (ActId **a, int *types, int *pos, ActId *prefix);
  
  unsigned int is_enum:1;	///< 1 if this is an enumeration, 0
				///otherwise
  unsigned int is_eint:1;	///< 1 if this enum can be treated as an int

  struct act_chp_lang *methods[ACT_NUM_STD_METHODS]; ///< all the
						     ///user-defined
						     ///methods for
						     ///this type
  
  list_t *enum_vals;		///< the list of enumeration values
				///(char *) for enumeration types.
};


/**
 * @class Channel
 *
 * @brief User-defined channel type. Channels can be unidirectional or
 * bi-directional (sometimes called exchange channels).
 */
class Channel : public UserDef {
 public:
  Channel (UserDef *u);
  virtual ~Channel();

  /**
   * Similar to Data::setMethod()
   */
  void setMethod (int t, act_chp_lang *h) { methods[t] = h; }

  /**
   * Similar to setMethod(), but used to set methods that return
   * expressions
   * @param t is the method number (from the datatype_methods enumeration)
   * @param e is the expression to be used as the return value
   */
  void setMethod (int t, Expr *e) { emethods[t-ACT_NUM_STD_METHODS] = e; }

  /**
   * Similar to Data::getMethod()
   */
  act_chp_lang *getMethod(int t) { return methods[t]; }

  /**
   * @return similar to getMethod(), but returns the expression for
   * e-methods 
   */
  Expr *geteMethod(int t) { return emethods[t-ACT_NUM_STD_METHODS]; }

  /**
   * Copy methods over from the specified channel. This copies
   * pointers over---i.e. a shallow copy
   * @param c is the Channel from whom methods should be copied over
   */
  void copyMethods (Channel *c);

  Channel *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);
  
  void Print (FILE *fp);

  /**
   * Check the direction flags for an identifier that is part of the
   * channel. 
   *
   * Given that the id has the direction specified, what is the
   * channel direction?  1 = input, 2 = output, 3 = both, 0 = undetermined
   * id is typically a fragmented piece of the channel, and hence this
   * involves examining the channel methods as well.
   *
   * @param id is the part of the channel to be inspected
   * @param isinput is 1 to check on the direction of the type
   * assuming a "?" operation, 0 if it is a "!" operation
   * @return 0, 1, 2, or 3 (see above).
   */
  int chanDir (ActId *id, int isinput);

  /**
   * One end of a channel is active and the other is passive. Only one
   * end of the channel can be probed, and this checks if a probe has
   * been defined that dictates the active v/s passive end of the
   * channel.
   *
   * @return 1 if the send of this channel must be active, 0 if it
   * must be passive, -1 if it is not determined by the channel type
   */
  int mustbeActiveSend ();
  
  /**
   * One end of a channel is active and the other is passive. Only one
   * end of the channel can be probed, and this checks if a probe has
   * been defined that dictates the active v/s passive end of the
   * channel.
   *
   * @return 1 if the receive on this channel must be active, 0 if it
   * must be passive, -1 if it is not determined by the channel type
   */
  int mustbeActiveRecv ();
  
  /**
   * For this to work the channel must have a parent definition.
   * @return 1 if this is a bidirectional channel, 0 otherwise
   */
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
  struct act_chp_lang *methods[ACT_NUM_STD_METHODS]; ///< methods table
  Expr *emethods[ACT_NUM_EXPR_METHODS];		     ///< emethods table
};


struct act_inline_table;


/**
 * @class UserMacro
 *
 * @brief Used to hold a user-defined macro. These macros provide more
 * convenient methods to interface with processes and data types
 */
class UserMacro {
public:
  /**
   * Create a new user-defined macro associated with a user-defined
   * type.
   * @param u is the user-defined type that holds the macro
   * @param name is the name of the macro
   */
  UserMacro (UserDef *u, const char *name);
  ~UserMacro ();

  void Print (FILE *fp);

  /**
   * Expand a user-defined macro. The is_proc flag is used because
   * process user-macros and data type user-macros are slightly
   * different: a process macro inherits direction flags for the
   * environment of the process rather than the process itself
   */
  UserMacro *Expand (UserDef *ux, ActNamespace *ns, Scope *s, int is_proc);

  /**
   * Add a port to the macro.
   * @param it is the type of the macro port
   * @param name is the name of the port
   * @return 1 on success, 0 on error (duplicate port name).
   */
  int addPort (InstType *it, const char *name);

  /**
   * @return the name of the macro
   */
  const char *getName () { return _nm; }

  /**
   * @return the number of ports for the macro
   */
  int getNumPorts() const { return nports; }


  /**
   * @return the number of template parameters for the macro
   */
  int getNumParams() const { return 0; }

  /**
   * Returns the name of a port for this macro
   * @param i is the port number
   * @return the name of the specified port
   */
  const char *getPortName (int i) const { return port_n[i]; }

  /**
   * Returns the type of a port for this macro
   * @param i is the port number
   * @return the type of the specified port
   */
  InstType *getPortType (int i) const { return port_t[i]; }

  /**
   * Sets the body of the macro to be the specified value
   */
  void setBody (struct act_chp_lang *);

  /**
   * Sets the body for function types
   */
  void setActBody (ActBody *b) { _b = b; }

  /**
   * Given an instance name and bindings for all the macro ports in
   * the act_inline_table, return the CHP body fragment that is the
   * result of the macro expansion
   * @param instnm is the name of the instance
   * @param tab is the binding table
   * @return the chp body fragment that results from the substitution.
   */
  struct act_chp_lang *substitute (ActId *instnm, act_inline_table *tab);


  /**
   * Sets the return type for the macro to the specified instance type
   * @param it is the return type for the macro
   */
  void setRetType (InstType *it);

  /**
   * @return the return type of the macro function; NULL if it is not
   * a function macro.
   */
  InstType *getRetType () { return rettype; }

  /**
   * @return the dummy function pointer associated with this macro, if
   * the macro is a function, NULL otherwise
   */
  Function *getFunction() { return _exf; }


  /**
   * Update function argument type using the new data type pointer
   * that is persistent
   */
  void updateFn (UserDef *u);

  /**
   * @return true if this is a special user macro. These are used for
   * the built-in int(.) and struct(.) conversion macros
   */
  bool isBuiltinMacro() { return _builtinmacro; }
  
  bool isBuiltinStructMacro() {
    return _builtinmacro && !(strcmp (_nm, "int") == 0);
  }

  /**
   * Mark this as a special built-in macro!
   */
  void mkBuiltin() { _builtinmacro = true; }


  /**
   * @return the user-defined type that hosts this macro
   */
  UserDef *Parent() { return parent; }

  /**
   * Set parent pointer: used to set during elaboration
   */
  void setParent(UserDef *u) { parent = u; }
  
  /**
   * Populate CHP for buil-in macros
   */
  void populateCHP();

private:
  const char *_nm;	     ///< name of the macro
  UserDef *parent;	     ///< user-defined type with this macro

  InstType *rettype;	     ///> for function macros, return
			     ///> type. NULL for non-function macros

  int nports;		     ///< number of ports
  InstType **port_t;	     ///< port types
  const char **port_n;	     ///< port names

  struct act_chp_lang *c;    ///< body

  Function *_exf;	     ///< expanded function corresponding to
                             ///  a function macro.

  ActBody *_b;

  bool _builtinmacro;	     ///< set to true if this is a special
			     ///built-in macro whose argument is a complete
			     ///expression

};


/**
 * @class TypeFactory
 *
 * @brief This is the class used to create all instance types. It also
 * caches types to reduce memory usage.
 *
 * All types should be created using the static TypeFactory
 * methods. In addition, the TypeFactory provides methods that can be
 * used to inspect attributes of types (e.g. is this a user-defined
 * type?)
 *
 */
class TypeFactory {
 private:
  static TypeFactory *tf;	///< there can only be one TypeFactory
				///in the system, and this is it.
  
  /**
   * Built-in parameter type for a pint: only one copy of this type
   * can exist, since there are no parameters.
   */
  static InstType *pint;
  
  /**
   * Built-in parameter type for a pints: only one copy of this type
   * can exist, since there are no parameters. NOTE: THIS IS UNUSED
   */
  static InstType *pints;
  
  /**
   * Built-in parameter type for a preal: only one copy of this type
   * can exist, since there are no parameters.
   */
  static InstType *preal;

  /**
   * Built-in parameter type for a pbool: only one copy of this type
   * can exist, since there are no parameters.
   */
  static InstType *pbool;

  /**
   * Five types of bool types: NONE, IN, OUT, INOUT, OUTIN.
   */
  static InstType *bools[5];

  /**
   * Constant expr E_TRUE. This is used to cache expressions.
   */
  static Expr *expr_true;

  /**
   * Constant expr E_FALSE. This is used to cache expressions.
   */
  static Expr *expr_false;

  /**
   * Constant expr E_INT. This is used to cache constant integer
   * expressions. The hash table maps integers to the Expr *.
   */
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
   * @return the unique pointer to a pint
   */
  InstType *NewPInt ()  { return pint; }

  /**
   * @return the unique pointer to a pints
   */
  InstType *NewPInts ()  { return pints; }

  /**
   * @return the unique pointer to a pbool
   */
  InstType *NewPBool () { return pbool; }

  /**
   * @return the unique pointer to a preal
   */
  InstType *NewPReal () { return preal; }

  /**
   * @return the unique pointer to the ptype InstType
   */
  InstType *NewPType (Scope *s, InstType *t);

  /**
   * @return the unique pointer to a Ptype
   */
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
   * \param l is the data type for the channel
   * \param ack is the "ack" type for the channel (bi-directional
   * channels)
   * \return a unique pointer to the specified channel type
   */
  InstType *NewChan (Scope *s, Type::direction dir, InstType *l, InstType *ack);

  /**
   * Similar to NewChan(), but returns the Chan type rather than an InstType.
   */
  Chan *NewChan (InstType *l, InstType *ack);

  /**
   * Returns a unique pointer to the instance type specified
   * \param it is the input instance type. This MAY BE FREE'D
   * \param s is the scope in which the user-defined type is being created
   * \return a unique pointer to the specified instance type
   */
  InstType *NewUserDef (Scope *s, InstType *it);


  /**
   * Returns a unique pointer to a constant expression. This
   * expression pointer is cached so this should never be free'ed.
   * @return a cached expression pointer that is equal to the one
   * passed in.
   */
  static Expr *NewExpr (Expr *e);

  /**
   * @return the TypeFactory used by the program/library.
   */
  static TypeFactory *Factory() { return tf; }
  
  /** 
   * Initialize and allocate the first type factory object.
   */
  static void Init ();


  /**
   * Determines if the specified type is a data type or not. Note: if
   * the type is a structure, then isDataType will return 0.
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid data type, 0 otherwise
   */
  static int isDataType (const Type *t);

  /** see the other isDataType() method */
  static int isDataType (const InstType *t);

  /**
   * Determines if the specified type is a built-in int<> type.
   *
   * @param t is the type to be inspected
   * @return 1 if it is an int<>, 0 otherwise
   */
  static int isIntType (const Type *t);
  
  /** see the other isIntType() method */
  static int isIntType (const InstType *t);

  /**
   * Determines if the specified type is a built-in pint type.
   *
   * @param t is the type to be inspected
   * @return 1 if it is a pint, 0 otherwise
   */
  static int isPIntType (const Type *t);

  /** see other isPIntType() method */
  static int isPIntType (const InstType *t);

  /**
   * Determines if the specified type is a built-in pints type.
   *
   * @param t is the type to be inspected
   * @return 1 if it is a pints, 0 otherwise
   */
  static int isPIntsType (const Type *t);

  /** see other isPIntsType() method */
  static int isPIntsType (const InstType *t);

  /**
   * Determines if the specified type is a built-in bool type.
   *
   * @param t is the type to be inspected
   * @return 1 if it is a bool, 0 otherwise
   */
  static int isBoolType (const Type *t);

  /** see other isBoolType() method */
  static int isBoolType (const InstType *it);
  
  /**
   * Determines if the specified type is a built-in pbool type.
   *
   * @param t is the type to be inspected
   * @return 1 if it is a pbool, 0 otherwise
   */
  static int isPBoolType (const Type *t);

  /** see other isPBoolType() method */
  static int isPBoolType (const InstType *it);

  /**
   * Determines if the specified type is a built-in preal type.
   *
   * @param t is the type to be inspected
   * @return 1 if it is a preal, 0 otherwise
   */
  static int isPRealType (const Type *t);

  /** see other isPRealType() method */
  static int isPRealType (const InstType *it);

  /**
   * Determines if the specified type is a user-defined type
   *
   * @param t is the type to be inspected
   * @return 1 if it is a UserDef type, 0 otherwise
   */
  static int isUserType (const Type *t);
  
  /** see other isUserType() method */
  static int isUserType (const InstType *it);

  /**
   * Determines if the specified type is a channel type or not. This
   * returns 1 if it is a user-defined channel or a built-in channel type.
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid channel type, 0 otherwise
   */
  static int isChanType (const Type *t);

  /** see other isChanType() method */
  static int isChanType (const InstType *it);

  /**
   * Determines if the specified type is a built-in chan(...) type.
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid built-in channel type, 0 otherwise
   */
  static int isExactChanType (const Type *t);

  /** see other isExactChanType() method */
  static int isExactChanType (const InstType *it);

  /**
   * Determines if the specified type is a valid type to be included
   * within the data type for a built-in chan(..). 
   *
   * Assumes that "t" is in fact a data type as a pre-condition.
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid channel data type, 0 otherwise
   */
  static int isValidChannelDataType (const Type *t);
  
  /** see other isValidChannelDataType() method */
  static int isValidChannelDataType (const InstType *t);

  /**
   * Determines if the specified type is a process type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid process/cell, 0 otherwise
   */
  static int isProcessType (const Type *t);
  
  /** see other isProcessType() method */
  static int isProcessType (const InstType *it);

  /**
   * Determines if the specified type is a function type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid function, 0 otherwise
   */
  static int isFuncType (const Type *t);

  /** see other isFuncType() method */
  static int isFuncType (const InstType *it);


  /**
   * Determines if the specified type is an interface type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid process/cell, 0 otherwise
   */
  static int isInterfaceType (const Type *t);

  /** see other isInterfaceType() method */
  static int isInterfaceType (const InstType *it);
  
  /**
   * Determines if the specified type is a ptype or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid ptype type, 0 otherwise
   */
  static int isPTypeType (const Type *t);

  /** see other isPTypeType() method */
  static int isPTypeType (const InstType *it);


  /**
   * Determines if the specified type is a parameter type (pint,
   * pints (UNUSED), pbool, preal, ptype).
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid ptype type, 0 otherwise
   */
  static int isParamType (const Type *t);

  /** see other isParamType() method */
  static int isParamType (const InstType *it);

  /**
   * Determines the bit-width of the type.
   * The type must be either a channel or a data type. If it isn't,
   * the function returns -1.
   * If the type is an enumeration, the bitwidth is the smallest # of
   * bits needed to hold the enumeration count.
   *
   * @return the bit-width of the type, or the type sent over the
   * channel in the case of channel types.
   */
  static int bitWidth (const Type *t);

  /** see other bitWidth() method */
  static int bitWidth (const InstType *t);

  /**
   * Bit-width of a type that also works for structures. In the case
   * of structures, the bit-width is the sum of the bit-widths of all
   * fields. The type must be either a data or channel type.
   *
   * @return the total bit-width to hold the type, similar to the
   * normal bitWidth method.
   */
  static int totBitWidth (const Type *t);

  /**
   * See other totBitWidth method
   */
  static int totBitWidth (const InstType *t);


  /**
   * Special version of totBitWidth that works for any user defined
   * type and sums the bit-widths of all int/bool fields.
   *
   * @return the total bit-width to hold the type, similar to the
   * normal bitWidth method.
   */
  static int totBitWidthSpecial (const Type *t);

  /**
   * See other totBitWidth method
   */
  static int totBitWidthSpecial (const InstType *t);
  

  /**
   * For bidirectional channels only. Returns 0 for normal channels,
   * -1 for non-channels. This returns the bit-width (like the
   * bitWidth() method) for the acknowledge type for the channel.
   *
   * @return the bit-width of the acknowledgment end of the channel.
   */
  static int bitWidthTwo (const Type *t);

  /** see other bitWidthTwo() method */
  static int bitWidthTwo (const InstType *t);

  /**
   * See totBitWidth and bitWidthTwo
   */
  static int totBitWidthTwo (const Type *t);

  /**
   * See other totBitWidthTwo
   */
  static int totBitWidthTwo (const InstType *t);

  /**
   * 1 if a boolean within a channel or
   * a boolean type, -1 if NULL parent
   *
   * @return 1 if this is a Boolean data type or a channel that
   * carries a Boolean data type.
   */
  static int boolType (const Type *t); 

  /** see other boolType() method */
  static int boolType (const InstType *t);

  /**
   * A UserDef that is a Data type can hold a structure. A data type
   * is a structure when it does not implement a built-in data type.
   * Note that currently structures cannot contain arrays.
   * @return 1 if this is a structure, 0 otherwise
   */
  static int isStructure (const Type *t);

  /** see other isStructure() method */
  static int isStructure (const InstType *it);


  /**
   * @return 1 if the type is a structure with pure data fields only
   * (recursive)
   */
  static int isPureStruct (const Type *t);

  /** see other isPureStruct() method */
  static int isPureStruct (const InstType *it);

  /**
   * Is this a user-defined enumeration?
   * @return 1 if this is a user-defined enumeration type, 0 otherwise
   */
  static int isUserEnum (const Type *t);

  /** see other isUserEnum() method */
  static int isUserEnum (const InstType *it);
  
  /**
   * Is this a user-defined pure enumeration (one that cannot be used
   * as an integer)
   * @return 1 if this is a user-defined pure enumeration type, 0 otherwise
   */
  static int isUserPureEnum (const Type *t);

  /** see other isUserPureEnum() method */
  static int isUserPureEnum (const InstType *it);

  /**
   * Used to determine the # of values in the enumeration. Return
   * enumeartion width, -1 if not a nenumeration. Works for channels
   * too, except it refers to the type within the channel.
   * 
   * @return the number of values in the enumeration, or the
   * enumeration for the channel type.
   */
  static int enumNum (const Type *t);
  static int enumNum (const InstType *t);

  /**
   * Test if a type is an enumeration (user-defined or built-in)
   *
   * @return 1 if this is a user-defined or built-in enumeration
   */
  static int isEnum (const Type *t);

  /** see other isEnum() method */
  static int isEnum (const InstType *t);
  
  /**
   * This is used to check if a user type that is rooted in a bool or
   * a bool itself
   * @return 1 if the base type is a bool, 0 otherwise
   */
  static int isBaseBoolType (const Type *t);

  /** see other isBaseBoolType() method */
  static int isBaseBoolType (const InstType *t);

  /**
   * This is used to check if a user type that is rooted in an int/int-enum or
   * an int/int-enum itself. An "int-enum" is an enum that can also be
   * treated as an int. 
   * @return 1 if the base type is a bool, 0 otherwise
   */
  static int isBaseIntType (const Type *t);

  /** see other isBaseIntType() method */
  static int isBaseIntType (const InstType *t);

  /**
   * @return the data type for the channel specified, NULL on an error
   */
  static InstType *getChanDataType (const InstType *t);

  /**
   * @return the ack type for the channel specified (the data type for
   * the acknowledge end in the case of a bi-directional channel),
   * NULL on error.
   */
  static InstType *getChanAckType (const InstType *t);
};

#include <act/typecheck.h>

/*------------------------------------------------------------------------
 *
 *  Helper functions for expression manipulation
 *
 *------------------------------------------------------------------------
 */

/**
 * Check if two expressions are equal. This is a structural test.
 *
 * @param a is the first expression
 * @param b is the second expression
 * @return 1 if they are equal, 0 otherwise
 */
int expr_equal (const Expr *a, const Expr *b);

/**
 * Print an expression to a file. This is used to print parameter
 * expressions (e.g. in the core ACT language), and not chp/dataflow
 * expressions.
 *
 * @param fp is the output file
 * @param e is the expression
 */
void print_expr (FILE *fp, const Expr *e);

/**
 * Print an expression to a string buffer. This is used to print
 * parameter expressions (e.g. in the core ACT language), and not
 * chp/dataflow expressions.
 *
 * @param buf is the output buffer 
 * @param sz is the size of the output buffer
 * @param e is the expression
 */
void sprint_expr (char *buf, int sz, const Expr *e);


/**
 * Print an expression to a file. This is used to print unsigned
 * expressions from chp/dataflow expressions.
 *
 * @param fp is the output file
 * @param e is the expression
 */
void print_uexpr (FILE *fp, const Expr *e);


/**
 * Print an expression to an output buffer. This is used to print unsigned
 * expressions from chp/dataflow expressions.
 *
 * @param buf is the output buffer 
 * @param sz is the size of the output buffer
 * @param e is the expression
 */
void sprint_uexpr (char *buf, int sz, const Expr *e);

/**
 * Check if the expression is a simple constant leaf expression
 *
 * @param e is the expression to be checked
 * @return 1 if it is a constant, 0 otherwise
 */
int expr_is_a_const (Expr *e);

/**
 * Duplicate an expression that is a constant. Some constant
 * expressions are cached by the TypeFactory. If that is the case,
 * then this simply returns the same pointer. Otherwise, it actually
 * allocates new memory and returns a duplicate of the constant
 * expression.
 *
 * @param e the constant expression to be duplicated
 * @return the duplicated expression (may or may not be a deep copy)
 */
Expr *expr_dup_const (Expr *e);

/**
 * This translates the E_... options into the string corresponding to
 * the expression operator
 *
 * @return a string constant for the expression operator
*/
const char *expr_op_name (int);

/* unified expression expansion code, with flags to control
   different variations */
#define ACT_EXPR_EXFLAG_ISLVAL   0x1 ///< if set, this expression must
				     ///be an lvalue (i.e. a variable)

#define ACT_EXPR_EXFLAG_PARTIAL  0x2 ///< if set, partial constant
				     ///propagation during expansion
				     ///is used

#define ACT_EXPR_EXFLAG_CHPEX    0x4 ///< if set, this uses CHP
				     ///expansion mode

#define ACT_EXPR_EXFLAG_DUPONLY  0x8 ///< this just duplicates the
				     ///expression

#define ACT_EXPR_EXFLAG_PREEXDUP 0x10 ///< flag is like DUP, only
                                      ///this is prior to expansion!

extern int _act_chp_is_synth_flag;   ///< this flag is set as a
				     ///side-effect of expression
				     ///expansion. Set to 0 if the
				     ///expression calls a
				     ///non-synthesizable function.

/**
 * Used to expand an expression. The flags are used to specify how the
 * expansion is performed. A duplicate expression is returned that
 * corresponds to the expanded expression. Parameter constants are
 * substituted, and for CHP/dataflow expressions, a BigInt is attached
 * to each constant value with the appropriate bit-width.
 *
 * @param e is the expression to be expanded
 * @param ns is the namespace
 * @param s is the evaluation scope
 * @param flag is the ACT_EXPR_EXFLAG_... flag
 * @return the expanded expression
 */
Expr *expr_expand (Expr *e, ActNamespace *ns, Scope *s, unsigned int flag = 0x2);

/**
 * A macro that just calls expr_expand() with the right flags.
 */
#define expr_dup(e) expr_expand ((e), NULL, NULL, ACT_EXPR_EXFLAG_DUPONLY)

#define expr_predup(e) expr_expand ((e), NULL, NULL, ACT_EXPR_EXFLAG_PREEXDUP|ACT_EXPR_EXFLAG_DUPONLY)

/**
 * Free an expanded expression
 */
void expr_ex_free (Expr *);

/**
 * Helper function for bit-width determination
 * @param etype is the expression type (E_AND, etc.)
 * @param lw is the width of the left arg
 * @param rw is the width of the right arg
 * @return the resulting bitwidth by ACT expression rules
 */
int act_expr_bitwidth (int etype, int lw, int rw);

/**
 * Helper function for bit-width determination
 * @param v is the integer
 * @return bitwidth for integer 
 */
int act_expr_intwidth (unsigned long v);

/**
 * Helper function for extracting a real value from a constant
 * expression. The expression has to be of type E_INT or E_REAL to
 * extract its value.
 * @param e is th expression
 * @param val is used to return the value
 * @return 1 if value extracted, 0 otherwise
 */
int act_expr_getconst_real (Expr *e, double *val);

/**
 * Helper function for extracting an integer value from a constant
 * expression. The expression has to be of type E_INT or E_REAL to
 * extract its value.
 * @param e is th expression
 * @param val is used to return the value
 *      
 * @return 1 if value extracted, 0 otherwise
 */
int act_expr_getconst_int (Expr *e, int *val);

/**
 * Helper function for creating an expression that's an ID
 * @param id is the ActId
 * @return the expression for it
 */
Expr *act_expr_var (ActId *id);

/**
 * Helper function: used to get a structure type from an
 * expression. This only works in the expanded scenarios.
 * @param s is the scope for evaluation
 * @param e is the expression
 * @param error is used to return the error code, if provided. Error
 * codes are: 0 = no error, 1 = invalid expression type, 2 = can't
 * find ID, 3 = not a structure, 4 = not expanded scope, 5 = e is NULL
 * @return the Data type if it is a structure, NULL otherwise
 */
Data *act_expr_is_structure (Scope *s, Expr *e, int *error = NULL);

/*
  External functions for core act library must 
  be of the form

  long function_name (int nargs, long *args);
*/

struct ExtLibs;

/**
 * Given an external library sturcture, look up the function within
 * the specified namespace in the external library
 *
 * @param el is the external library structure
 * @param ns is the namespace in which the function exists
 * @param f is the name of the function
 * @return the function pointer, if found, and NULL otherwise
 */
void *act_find_dl_func (struct ExtLibs *el, ActNamespace *ns, const char *f);

/**
 * Query the ACT configuration to see if an external function table
 * exists given the configuration prefix string
 *
 * @param prefix is the configuration prefix string
 * @return the list of external libraries available according to the
 * configuration file
 */
struct ExtLibs *act_read_extern_table (const char *prefix);



/**
 * Push expansion error context
 * 
 * @param s is the context (process name, instance name, etc)
 * @param file is the file name
 * @param line is the line number
 */
void act_error_push (const char *s, const char *file, int line);

/**
 * Update the file name and line number for the top-level error
 * context. If the file pointer is NULL, then the previous one is
 * preserved.
 *
 * @param file is the updated file name
 * @param line is the updated line number
 *
 */
void act_error_update (const char *file, int line); // set file to
						    // NULL to keep
						    // the file name

void act_error_pop ();				    ///< pop the error
						    ///context

void act_error_ctxt (FILE *);	///< print the error context to the
				///output stream

const char *act_error_top ();	///< return the message for the
				///top-level error

void act_error_setline (int line); ///< set the current line number 

#endif /* __ACT_TYPES_H__ */
