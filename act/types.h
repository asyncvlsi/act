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
#include <mstring.h>
#include <act/inst.h>

class ActBody;
struct act_chp_lang;
struct act_chp;
struct act_prs;
struct act_spec;
struct act_attr;

class PInt : public Type {
  const char *getName() { return "pint"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
  int isEqual (Type *t) { return t == this ? 1 : 0; }
};

class PInts : public Type {
  const char *getName() { return "pints"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
  int isEqual (Type *t) { return t == this ? 1 : 0; }
};

class PBool : public Type { 
  const char *getName() { return "pbool"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
  int isEqual (Type *t) { return t == this ? 1 : 0; }
};

class PReal : public Type {
  const char *getName() { return "preal"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
  int isEqual (Type *t) { return t == this ? 1 : 0; }
};

class PType : public Type {
public:
  const char *getName();
  PType *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  PType() { i = NULL; name = NULL; };

  /* wrong */
  int isEqual (Type *t) { return t == this ? 1 : 0; }

  InstType *getType () { return i; }

private:
  InstType *i;			// when it is expanded
  const char *name;

  friend class TypeFactory;
};

class Bool : public Type {
  const char *getName() { return "bool"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
  int isEqual (Type *t) { return t == this ? 1 : 0; }
};

class Int : public Type {
  const char *getName();
  Int *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);
  
  int isEqual (Type *t);
  
  unsigned int kind:2;		// 0 = unsigned, 1 = signed, 2 = enum
  int w;
  const char *name;
  
  friend class TypeFactory;
};

/**
 * Chan class. Paramterized chan(foo) type
 */
class Chan : public Type {
  const char *getName ();
  Chan *Expand (ActNamespace *ns, Scope *s, int, inst_param *);

  int isEqual (Type *t);
  
  /* type info here */
  const char *name;
  InstType *p;			// data type for expanded channel

public:
  InstType *datatype() { return p; }

  friend class TypeFactory;
};


class InstType;




/**
 *  UserDef stores information that is common to all user-defined
 *  types. User-defined types are more complex, because they can have
 *  definitions and template parameters. To handle this complexity and
 *  to reduce storage requirements, the "shared" part of a
 *  user-defined type is factored out into this class. That way the
 *  body of a channel x1of2 is shared with x1of2? and x1of2!, as well
 *  as an array of x1of2 (for example).
 *
 */
class UserDef : public Type {
 public:
  UserDef (ActNamespace *ns); /**< constructor, initialize everything correctly */
  virtual ~UserDef (); /**< destructor, releases storage */

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

  int isExpanded() { return expanded; }
  

  /**
   * Add a new parameter
   *
   *  @param t is the type of the parameter
   *  @param id is the the identifier for this parameter
   *
   *  @return 1 on success, 0 on error (duplicate parameter name)
   *
   */
  int AddMetaParam (InstType *t, const char *id);

  /**
   * Allocate space for meta parameters
   *
   * @param id is the name of the meta parameter
   * 
   */
  void AllocMetaParam (const char *id);

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
   * Find a port
   *
   * @param id is the name of the port
   * @return 0 if not found. Positive values mean ports; negative
   * values mean template parameters
   */
  int FindPort (const char *id);

  /**
   * Returns name of port at specified position
   * 
   * @param pos is the position of the port in the port list. Negative
   * numbers indicate meta-parameters, 0 and positive parameters
   * indicate physical port parameters.
   * @return the name of the specified port
   */
  const char *getPortName (int pos);
  InstType *getPortType (int pos);
  void refinePortType (int pos, InstType *u);

  const char *getName (); /**< the name of the user-defined type */

  int isEqual (Type *t);

  /**
   * Set the name of the type
   */
  void setName (const char *s) { name = s; }

  /**
   * Make a copy: copy over all fields from u, and clear the fields of
   * u at the same time.
   */
  void MkCopy (UserDef *u);

  /**
   * Compare user-defined types to see if the port parameters match
   */
  int isEqual (UserDef *u);

  /**
   * Setup parent relationship
   *
   * @param t is the parent type
   * @param eq is 1 if this is an equality typing relationship, 0 if
   * subtyping
   *
   */
  void SetParent (InstType *t);
  InstType *getParent () { return parent; }

  /**
   * Specify if this type has been fully defined yet or not. It may
   * only be a declared type at this point.
   */
  int isDefined () { return defined; }
  void MkDefined () { defined = 1; }

  /**
   * Is this parameter a port?
   *
   * @param name is the name of the instance to be checked.
   *
   */
  int isPort(const char *name);

  /**
   * Returns the number of template parameters
   */
  int getNumParams () { return nt; }

  /**
   * Returns the number of ports
   */
  int getNumPorts () { return nports; }

  /**
   * Looks up an identifier within the scope of the body of the type
   */
  InstType *Lookup (ActId *id) { return I->Lookup (id, 0); }

  Scope *CurScope() { return I; }

  /**
   * @param name is the name of the port to be checked
   * @return 1 if this is a port name that is also in the "strict"
   * parameter list
   */
  int isStrictPort (const char *name);


  /**
   * Print out user defined type 
   */
  virtual void Print (FILE *fp) { }
  void PrintHeader (FILE *fp, const char *type);

  void setBody (ActBody *x) { b = x; } /**< Set the body of the
					  process */
  void AppendBody (ActBody *x);
  ActBody *getBody () { return b; }
  
  UserDef *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    Assert (0, "Don't call this ever");
  }
  
  UserDef *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u, int *cache_hit);

  /* append in the case of multiple bodies! */

  ActNamespace *getns() { return _ns; }
  InstType *root();

  act_prs *getprs ();
  act_spec *getspec ();
  act_languages *getlang () { return lang; }

  int isLeaf(); /**< if there are no sub-circuits, return 1 */

 protected:
  InstType *parent;		/**< implementation relationship, if any */
  
  unsigned int defined:1;	/**< 1 if this has been defined, 0
				   otherwise */
  unsigned int expanded:1;	/**< 1 if this has been expanded, 0
				   otherwise */

  unsigned int pending:1;	/**< 1 if this is currently being
				   expanded, 0 otherwise. */

  act_languages *lang;
  
  int nt;			/**< number of template parameters */
  InstType **pt;		/**< parameter types */
  const char **pn;		/**< parameter names */
  int exported;			/**< 1 if the type is exported, 0 otherwise */

  int nports;			/**< number of ports */
  InstType **port_t;		/**< port types */
  const char **port_n;		/**< port names */

  Scope *I;			/**< instances */

  const char *name;		/**< Name of the user-defined type */

  ActBody *b;			/**< body of user-defined type */
  ActNamespace *_ns;		/**< namespace within which this type is defined */

  UserDef *unexpanded;		/**< unexpanded type, if any **/

  int level;		  /**< default modeling level for the type **/
};


/**
 *
 * Interfaces
 *
 *  Looks like a process. Body is empty.
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
 * User-defined processes
 *
 */
class Process : public UserDef {
 public:
  Process (UserDef *u);		/**< Construct a process from a userdef */
  virtual ~Process ();
  void MkCell () { is_cell = 1; } /**< Mark this as a cell */
  int isCell() { return is_cell; }

  Process *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  void Print (FILE *fp);

  int isBlackBox();

  /* add an interface with an id mapping */
  void addIface (InstType *iface, list_t *imap);
  int hasIface (InstType *x, int weak); // weak check for interface equality
  list_t *findMap (InstType *iface);
  void mkRefined() { has_refinement = 1; }
  int hasRefinment() { return has_refinement; }
  
 private:
  unsigned int is_cell:1;	/**< 1 if this is a defcell, 0 otherwise  */
  list_t *ifaces;		/**< list of interfaces, map pairs */
  int has_refinement;		/**< 1 if there is a refinement body */
};


/**
 *
 * Functions
 *
 *  Looks like a process. The ActBody consists of a chp body,
 *  nothing else.
 *
 */
class Function : public UserDef {
 public:
  Function (UserDef *u);
  ~Function ();

  void setRetType (InstType *i) { ret_type = i; }
  InstType *getRetType () { return ret_type; }

  Function *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  Expr *eval (ActNamespace *ns, int nargs, Expr **args);
  Expr *toInline (int nargs, Expr **args);

  int isExternal ();
  
 private:
  InstType *ret_type;
};



#define ACT_NUM_STD_METHODS 8
#define ACT_NUM_EXPR_METHODS 2

extern const char *act_builtin_method_name[ACT_NUM_STD_METHODS];
extern const char *act_builtin_method_expr[ACT_NUM_EXPR_METHODS];
extern const int act_builtin_method_boolret[ACT_NUM_EXPR_METHODS];

enum datatype_methods {
    ACT_METHOD_SET = 0,
    ACT_METHOD_GET = 1,
    ACT_METHOD_SEND_REST = 2,
    ACT_METHOD_RECV_REST = 3,
    ACT_METHOD_SEND_UP = 4,
    ACT_METHOD_RECV_UP = 5,
    ACT_METHOD_SEND_INIT = 6,
    ACT_METHOD_RECV_INIT = 7,
    ACT_METHOD_SEND_PROBE = 0 + ACT_NUM_STD_METHODS,
    ACT_METHOD_RECV_PROBE = 1 + ACT_NUM_STD_METHODS
};

/**
 *
 * User-defined data types
 *
 */
class Data : public UserDef {
 public:
  Data (UserDef *u);
  virtual ~Data();

  void MkEnum () { is_enum = 1; }
  int isEnum () { return is_enum; }

  void setMethod (int t, struct act_chp_lang *h) {  methods[t] = h; }
  struct act_chp_lang *getMethod (int t) { return methods[t]; }
  void copyMethods (Data *d);
 
  Data *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);
  
  void Print (FILE *fp);

private:
  unsigned int is_enum:1;	/**< 1 if this is an enumeration, 0 otherwise */
  struct act_chp_lang *methods[ACT_NUM_STD_METHODS]; /**< set and get methods for this data type */
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
  
  
 private:
  struct act_chp_lang *methods[ACT_NUM_STD_METHODS];
  Expr *emethods[ACT_NUM_EXPR_METHODS];
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
  InstType *NewChan (Scope *s, Type::direction dir, InstType *l);
  Chan *NewChan (InstType *l);

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
  static int isDataType (Type *t);
  static int isDataType (InstType *t);

  static int isIntType (Type *t);
  static int isIntType (InstType *t);

  static int isPIntType (Type *t);
  static int isPIntType (InstType *t);

  static int isPIntsType (Type *t);
  static int isPIntsType (InstType *t);

  static int isBoolType (Type *t);
  static int isBoolType (InstType *it);
  
  static int isPBoolType (Type *t);
  static int isPBoolType (InstType *it);

  static int isPRealType (Type *t);
  static int isPRealType (InstType *it);

  static int isUserType (Type *t);
  static int isUserType (InstType *it);

  /**
   * Determines if the specified type is a channel type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid channel type, 0 otherwise
   */
  static int isChanType (Type *t);
  static int isChanType (InstType *it);

  /* 1 only on ``chan(...)'', not on userdefined channels */
  static int isExactChanType (Type *t);
  static int isExactChanType (InstType *it);

  /**
   * Determines if the specified type is a process type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid process/cell, 0 otherwise
   */
  static int isProcessType (Type *t);
  static int isProcessType (InstType *it);

  /**
   * Determines if the specified type is a function type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid function, 0 otherwise
   */
  static int isFuncType (Type *t);
  static int isFuncType (InstType *it);


  /**
   * Determines if the specified type is an interface type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid process/cell, 0 otherwise
   */
  static int isInterfaceType (Type *t);
  static int isInterfaceType (InstType *it);
  
  /**
   * Determines if the specified type is a ptype or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid ptype type, 0 otherwise
   */
  static int isPTypeType (Type *t);
  static int isPTypeType (InstType *it);


  /**
   * Determines if the specified type is a parameter type
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid ptype type, 0 otherwise
   */
  static int isParamType (Type *t);
  static int isParamType (InstType *it);

  /**
   * Determines the bit-width of the type
   * The type must be either a channel or a data type. If it isn't,
   * the function returns -1
   */
  static int bitWidth (Type *t);
  static int bitWidth (InstType *t);
  
  static int boolType (Type *t); // 1 if a boolean within a channel or
				 // a boolean type, -1 if NULL parent
  static int boolType (InstType *t);

  static int isStructure (Type *t);
  static int isStructure (InstType *it);
  
};




/*
  Typecheck expression: returns -1 on failure
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
#define T_PTYPE      0x10
#define T_ARRAYOF    0x20

#define T_FIXBASETYPE(x)  ((((x) & 0x1f) == T_DATA_BOOL) ? T_BOOL : ((((x) & 0x1f) == T_DATA_INT) ? T_INT : ((x) & 0x1f)))
#define T_BASETYPE(x) ((x) & 0x1f)
#define T_BASETYPE_ISNUM(x) (T_FIXBASETYPE (x) == T_INT || T_BASETYPE (x) == T_REAL)
#define T_BASETYPE_ISINTBOOL(x) ((T_FIXBASETYPE (x) == T_INT) || (T_FIXBASETYPE (x) == T_BOOL))

int act_type_expr (Scope *, Expr *, int *width, int only_chan = 0);
int act_type_var (Scope *, ActId *, InstType **xit);
int act_type_conn (Scope *, ActId *, AExpr *);
int act_type_conn (Scope *, AExpr *, AExpr *);
const char *act_type_errmsg (void);

void print_expr (FILE *fp, Expr *e);
void sprint_expr (char *buf, int sz, Expr *e);
int expr_is_a_const (Expr *e);
Expr *expr_dup_const (Expr *e);
void type_set_position (int l, int c, char *n);
InstType *act_expr_insttype (Scope *s, Expr *e, int *islocal);
InstType *act_actual_insttype (Scope *s, ActId *id, int *islocal);

int type_connectivity_check (InstType *lhs, InstType *rhs, int skip_last_array = 0);
int expr_equal (Expr *a, Expr *b);
Expr *expr_expand (Expr *e, ActNamespace *ns, Scope *s, int is_lval = 0);

/* for expanded expressions */
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

/* 
   For loops:
      e->l->l = id, e->r->l = lo, e->r->r->l = hi, e->r->r->r = expr

      WARNING: replicated in expr_extra.c
*/
#define E_ANDLOOP (E_END + 21) 
#define E_ORLOOP (E_END + 22)
#define E_BUILTIN_BOOL (E_END + 23)
#define E_BUILTIN_INT  (E_END + 24)
#define E_NEWEND  E_END + 25

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

void typecheck_err (const char *s, ...);

extern "C" {

Expr *act_parse_expr_syn_loop_bool (LFILE *l);
Expr *act_parse_expr_intexpr_base (LFILE *l);
int act_expr_parse_newtokens (LFILE *l);
  
}

#endif /* __ACT_TYPES_H__ */
