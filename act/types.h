/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
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

#ifndef E_SELF
#define E_SELF 50
#endif

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
};

class PInts : public Type {
  const char *getName() { return "pints"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
};

class PBool : public Type { 
  const char *getName() { return "pbool"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
};

class PReal : public Type {
  const char *getName() { return "preal"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
};

class PType : public Type {
public:
  const char *getName();
  PType *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  PType() { i = NULL; name = NULL; };
  
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
};

class Int : public Type {
  const char *getName();
  Int *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);
  
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
  ~UserDef (); /**< destructor, releases storage */

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
   *  @param opt is 1 if this is one of the optional parameters.
   *
   *  @return 1 on success, 0 on error (duplicate parameter name)
   *
   * The function only succeeds if the opt parameter is initially 0,
   * then switches over to 1, and stays 1 for multiple calls to the
   * for the same userdef type.
   *
   */
  int AddMetaParam (InstType *t, const char *id, int opt = 0);

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

  const char *getName (); /**< the name of the user-defined type */

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
  void SetParent (InstType *t, unsigned int eq);

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

  void setBody (ActBody *x) { b = x; } /**< Set the body of the process */

  UserDef *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    Assert (0, "Don't call this ever");
  }
  
  UserDef *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u, int *cache_hit);

  /* append in the case of multiple bodies! */
  void setprs (act_prs *p) { lang.prs = p; }
  act_prs *getprs () { return lang.prs; }
  
  void sethse (act_chp *c) { lang.hse = c; }
  act_chp *gethse() { return lang.hse; }
  
  void setchp (act_chp *c) { lang.chp = c; }
  act_chp *getchp() { return lang.chp; }

  void setspec (act_spec *s) { lang.spec = s; }
  act_spec *getspec() { return lang.spec; }

  ActNamespace *getns() { return _ns; }

  InstType *root();

 protected:
  InstType *parent;		/**< Sub-typing relationship, if any */
  unsigned int parent_eq:1;	/**< 1 if this is an equality, 0 if it
				   is a subtype */
  
  unsigned int defined:1;	/**< 1 if this has been defined, 0
				   otherwise */
  unsigned int expanded:1;	/**< 1 if this has been expanded, 0
				   otherwise */

  unsigned int pending:1;	/**< 1 if this is currently being
				   expanded, 0 otherwise. */

  int nt;			/**< number of template parameters */
  int mt;			/**< always <= nt; corresponds to the
				   number of parameters that can
				   impact the type signature */
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

  /* languages */
  struct {
    act_prs *prs;
    act_chp *hse, *chp;
    act_spec *spec;
  } lang;
};

/**
 *
 * User-defined processes
 *
 */
class Process : public UserDef {
 public:
  Process (UserDef *u);		/**< Construct a process from a userdef */
  ~Process ();
  void MkCell () { is_cell = 1; } /**< Mark this as a cell */

  Process *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

  void Print (FILE *fp);

 private:
  unsigned int is_cell:1;	/**< 1 if this is a defcell, 0 otherwise  */
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
  
 private:
  InstType *ret_type;
};


enum datatype_methods {
    ACT_METHOD_SET = 0,
    ACT_METHOD_GET = 1,
    ACT_METHOD_SEND_REST = 2,
    ACT_METHOD_RECV_REST = 3,
    ACT_METHOD_SEND_PROBE = 4,
    ACT_METHOD_RECV_PROBE = 5
};

/**
 *
 * User-defined data types
 *
 */
class Data : public UserDef {
 public:
  Data (UserDef *u);
  ~Data();

  void MkEnum () { is_enum = 1; }
  int isEnum () { return is_enum; }

  void setMethod (datatype_methods t, struct act_chp_lang *h) {
    methods[t] = h;
  }
  struct act_chp_lang *getMethod (datatype_methods t) { return methods[t]; }
 
  Data *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);
  
  void Print (FILE *fp);

private:
  unsigned int is_enum:1;	/**< 1 if this is an enumeration, 0 otherwise */
  struct act_chp_lang *methods[2]; /**< set and get methods for this data type */
};

class Channel : public UserDef {
 public:
  Channel (UserDef *u);
  ~Channel();
  
  void setMethod (datatype_methods t, act_chp_lang *h) { methods[t] = h; }
  act_chp_lang *getMethod(datatype_methods t) { return methods[t]; }

  Channel *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);
  
  void Print (FILE *fp);
  
 private:
  struct act_chp_lang *methods[6];
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

  /* 1 only on Chan, not on userdefined channels */
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
};



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

  Expr *Eval (ActNamespace *ns, Scope *s, int is_lval = 0);
  /**< evaluating an ID returns either: just the ID itself, for
     non-meta parameters (also for meta parameters if is_lval=1)
     or the value of the parameter for meta-parameters. */

  /**< 
     Find canonical root identifier in the current scope.
  */
  act_connection *Canonical (Scope *s);


  void setArray (Array *_a) { a = _a; }
  void prune () { next = NULL; }

 private:
  mstring_t *name;		/**< name of the identifier */
  Array *a;			/**< array reference/dereference */
  ActId *next;			/**< any `.' reference */

  ValueIdx *rawValueIdx (Scope *);
};


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


/*------------------------------------------------------------------------
 *
 *
 * Act body types
 *
 *
 *------------------------------------------------------------------------
 */

class ActBody {
 public:
  ActBody () { next = NULL; }
  void Append (ActBody *b);
  ActBody *Tail ();
  ActBody *Next () { return next; }

  virtual void Expand (ActNamespace *, Scope *) { fatal_error ("Need to define Expand() method!"); }

  void Expandlist (ActNamespace *, Scope *);

  virtual void Print (FILE *fp) { }

 private:
  ActBody *next;
};


/**
 * Instance 
 */
class ActBody_Inst : public ActBody {
 public:
  ActBody_Inst(InstType *, const char *);
  void Expand (ActNamespace *, Scope *);
  Type *BaseType ();
  void Print (FILE *fp);

 private:
  InstType *t;
  const char *id;
};

class ActBody_Attribute : public ActBody {
public:
  ActBody_Attribute(const char *_inst, act_attr *_a, Array *_arr = NULL) {
    inst = _inst; a = _a; arr = _arr;
  }
  void Expand (ActNamespace *, Scope *);
  //void Print (FILE *fp);

private:
  const char *inst;
  act_attr *a;
  Array *arr;
};


class ActBody_Conn : public ActBody {
 public:
  ActBody_Conn(ActId *id1, AExpr *ae) {
    type = 0;
    u.basic.lhs = id1;
    u.basic.rhs = ae;
  }

  ActBody_Conn(AExpr *id1, AExpr *id2) {
    type = 1;
    u.general.lhs = id1;
    u.general.rhs = id2;
  }

  void Print (FILE *fp);
  void Expand (ActNamespace *, Scope *);
  
 private:
  union {
    struct {
      ActId *lhs;
      AExpr *rhs;
    } basic;
    struct {
      AExpr *lhs, *rhs;
    } general;
  } u;
  unsigned int type:1; /**< 0 = basic, 1 = general */
};

class ActBody_Loop : public ActBody {
 public:

  enum type {
    SEMI,
    COMMA,			// unused
    AND,			// unused
    OR,				// unused
    BAR				// unused
  };
    
  ActBody_Loop (ActBody_Loop::type _t, 
		const char *_id, 
		Expr *_lo, /* NULL if this is a 0..hi-1 loop */
		Expr *_hi,
		ActBody *_b) {
    t = _t;
    id = _id;
    lo = _lo;
    hi = _hi;
    b = _b;
  }

  void Expand (ActNamespace *, Scope *);

  void Print (FILE *fp);

 private:
  ActBody_Loop::type t;			/**< type of loop */
  const char *id;			/**< loop variable */
  Expr *lo, *hi;		/**< range */
  ActBody *b;			/**< body of the loop */
};

class ActBody_Select_gc {
 public:
  ActBody_Select_gc (Expr *_g, ActBody *_s) {
    g = _g;
    s = _s;
    next = NULL;
  }
  void Append (ActBody_Select_gc *s) {
    next = s;
  }
#if 0
  ~ActBody_Select_gc () {
    if (next) {
      delete next;
    }
    delete s;
  }
#endif
 private:
  Expr *g;			/**< guard */
  ActBody *s;			/**< statement */
  ActBody_Select_gc *next;		/**< rest of the selection */

  friend class ActBody_Select;
};

class ActBody_Select : public ActBody {
 public:
  ActBody_Select (ActBody_Select_gc *_gc) {
    gc = _gc;
  }
#if 0
  ~ActBody_Select () {
    if (gc) {
      delete gc;
    }
  }
#endif

  void Expand (ActNamespace *, Scope *);

 private:
  ActBody_Select_gc *gc;
};

class ActBody_Assertion : public ActBody {
public:
  ActBody_Assertion (Expr *_e, const char *_msg = NULL) {
    e = _e;
    msg = _msg;
  }
  ~ActBody_Assertion () {
    if (e) {
      expr_free (e);
    }
  }
  void Expand (ActNamespace *, Scope *);
private:
  Expr *e;
  const char *msg;
};
  

class ActBody_Namespace : public ActBody {
public:
  ActBody_Namespace (ActNamespace *_ns) {
    ns = _ns;
  }

  void Expand (ActNamespace *, Scope *);
  
private:
  ActNamespace *ns;
};


struct act_prs;
struct act_chp;

/*
 * Language body
 */
class ActBody_Lang : public ActBody {
 public:
  ActBody_Lang (act_prs *p) {
    t = LANG_PRS;
    lang = p;
  }
  ActBody_Lang (act_chp *c, int ishse = 0) {
    if (ishse) {
      t = LANG_HSE;
    }
    else {
      t = LANG_CHP;
    }
    lang = c;
  }

  ActBody_Lang (act_spec *s) {
    t = LANG_SPEC;
    lang = s;
  }

  void Expand (ActNamespace *, Scope *);
  void Print (FILE *fp);

 private:
  enum {
    LANG_PRS,
    LANG_CHP,
    LANG_HSE,
    LANG_SPEC,

    LANG_SIZE,
    LANG_SPICE
  } t;
  void *lang;
};


/*
  Typecheck expression: returns -1 on failure
*/
#define T_ERR         -1

#define T_STRICT     0x20   // a port parameter that is in the
			    // "strict" list... i.e. not optional,
			    // i.e. before the vertical bar
#define T_PARAM      0x40   // a parameter type pint/... etc

#define T_INT        0x1
#define T_REAL       0x2
#define T_BOOL       0x4
#define T_PROC       0x5
#define T_CHAN       0x6
#define T_DATA       0x7
#define T_SELF       0x8   /* special type, "self" */
#define T_ARRAYOF    0x10

int act_type_expr (Scope *, Expr *);
int act_type_var (Scope *, ActId *);
int act_type_conn (Scope *, ActId *, AExpr *);
int act_type_conn (Scope *, AExpr *, AExpr *);
const char *act_type_errmsg (void);

void print_expr (FILE *fp, Expr *e);
void sprint_expr (char *buf, int sz, Expr *e);
int expr_is_a_const (Expr *e);
void type_set_position (int l, int c, char *n);
InstType *act_expr_insttype (Scope *s, Expr *e);
InstType *actual_insttype (Scope *s, ActId *id);

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

/*
  Push expansion context 
*/
void act_error_push (const char *s, const char *file, int line);
void act_error_update (const char *file, int line); // set file to
						    // NULL to keep
						    // the file name
void act_error_pop ();
void act_error_ctxt (FILE *);

#endif /* __ACT_TYPES_H__ */
