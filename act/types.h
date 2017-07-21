/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */

#ifndef __ACT_TYPES_H__
#define __ACT_TYPES_H__

#include <act/namespaces.h>
#include <act/expr.h>
#include <mstring.h>

#ifndef E_SELF
#define E_SELF 50
#endif

class ActNamespace;
class ActBody;
struct act_chp_lang;
union inst_param;

/**
 * The base class for all types in the system
 */
class Type {
 public:
  Type() { };
  ~Type() { };

  /**
   * @return a string corresponding to the full type name for the
   * specified type. This should not be free'd.
   */
  virtual const char *getName () = 0;
  
  /**
   * @return an expanded type
   */
  virtual Type *Expand (ActNamespace *, Scope *, int, inst_param *) = 0;

  /**
   * Initialize static members. This also calls the static functions
   * for the TypeFactory
   */
  static void Init();
 
  enum direction {
    NONE = 0,
    IN = 1,
    OUT = 2,
    INOUT = 3,
    OUTIN = 4
  };

  static const char *dirstring (direction d) {
    switch (d) {
    case Type::NONE: return ""; break;
    case Type::IN: return "?"; break;
    case Type::OUT: return "!"; break;
    case Type::INOUT: return "?!"; break;
    case Type::OUTIN: return "!?"; break;
    }
    return "-err-";
  };

 protected:
  friend class TypeFactory;
};

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
  const char *getName() { return "ptype"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
};

class Bool : public Type {
  const char *getName() { return "bool"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
};

class Int : public Type {
  const char *getName() { return "int"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
};

class Ints : public Type {
  const char *getName() { return "ints"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
     return this;
  }
};

class Enum : public Type {
  const char *getName() { return "enum"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
    return this;
  }
};

/**
 * Chan class. Paramterized chan(...) type
 */
class Chan : public Type {
  const char *getName () { return "chan"; }
  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u) {
     return this;
  }
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
   * @param pos is the position of the port in the port list
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


  void setBody (ActBody *x) { b = x; } /**< Set the body of the process */

  Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u);

 protected:
  InstType *parent;		/**< Sub-typing relationship, if any */
  unsigned int parent_eq:1;	/**< 1 if this is an equality, 0 if it
				   is a subtype */
  
  unsigned int defined:1;	/**< 1 if this has been defined, 0
				   otherwise */
  unsigned int expanded:1;	/**< 1 if this has been expanded, 0
				   otherwise */

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

  void setMethodset (struct act_chp_lang *h) { set = h; }
  struct act_chp_lang *getMethodset () { return set; }
  void setMethodget (struct act_chp_lang *h) { get = h; }
  struct act_chp_lang *getMethodget () { return get; }
 
 private:
  unsigned int is_enum:1;	/**< 1 if this is an enumeration, 0 otherwise */
  struct act_chp_lang *set, *get;   /**< set and get methods for this data type */
};

class Channel : public UserDef {
 public:
  Channel (UserDef *u);
  ~Channel();
  
  void setMethodsend (act_chp_lang *h) { send = h; }
  act_chp_lang *getMethodsend() { return send; }

  void setMethodrecv (act_chp_lang *h) { recv = h; }
  act_chp_lang *getMethodrecv() { return recv; }

 private:
  act_chp_lang *send, *recv;
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
   * Return unique pointer to the parameterized type
   */
  InstType *NewPInt ()  { return pint; }
  InstType *NewPInts ()  { return pints; }
  InstType *NewPBool () { return pbool; }
  InstType *NewPReal () { return preal; }
  InstType *NewPType (Scope *s, InstType *t);

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
  InstType *NewChan (Scope *s, Type::direction dir, int n, InstType **l);

  /**
   * Returns a unique pointer to the instance type specified
   * \param it is the input instance type. This MAY BE FREE'D
   * \param s is the scope in which the user-defined type is being created
   * \return a unique pointer to the specified instance type
   */
  InstType *NewUserDef (Scope *s, InstType *it);


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

  static int isIntType (Type *t);
  static int isIntsType (Type *t);

  static int isPIntType (Type *t);
  static int isPIntsType (Type *t);

  static int isBoolType (Type *t);
  static int isPBoolType (Type *t);

  static int isPRealType (Type *t);

  static int isUserType (Type *t);

  /**
   * Determines if the specified type is a channel type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid channel type, 0 otherwise
   */
  static int isChanType (Type *t);

  /**
   * Determines if the specified type is a process type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid process/cell, 0 otherwise
   */
  static int isProcessType (Type *t);

  /**
   * Determines if the specified type is a function type or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid function, 0 otherwise
   */
  static int isFuncType (Type *t);

  /**
   * Determines if the specified type is a ptype or not
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid ptype type, 0 otherwise
   */
  static int isPtypeType (Type *t);


  /**
   * Determines if the specified type is a parameter type
   *
   * @param t is the type to be inspected
   * @return 1 if it is a valid ptype type, 0 otherwise
   */
  static int isParamType (Type *t);
};


/**
 *
 *  Dense arrays, sparse arrays, and array dereferences
 *
 *  Used to represent a set of dense ranges/derefs
 *  If a sub-range specifier is not used, then the "deref" field is
 *  set to 1---it means the user wrote [x] rather than [x..y] for every
 *  dimension of the array.
 *
 */
class Array {
 public:
  /**
   * \return 1 if this is a sparse array specification, 0 otherwise
   */
  int isSparse() { return next == NULL ? 0 : 1; }

  /**
   * \return 1 if this is an array dereference, rather than a
   * sub-array specification
   */
  int isDeref() { return deref; }

  /**
   * Force this to be an array, not a deref. Used to override type
   * specifier for compact foo f[n] which is shorthand for foo[0..n-1]
   */
  void mkArray() { deref = 0; }

  /**
   * \return the number of dimensions of this array/sub-array/dereference
   */
  int nDims() { return dims; }

  /**
   * \return the effective number of dimensions, taking partial
   * de-references into account. If this is a sparse array, it is just
   * nDims()
   */
  int effDims();

  /**
   * Append array dimensions from the argument into the current
   * array. The current array must be dense.
   *
   * @param a contains the arguments to be merged, which must be a
   * dense array
   *  
   * Updates the current array with the additional dimensions from a
   */
  void Concat (Array *a);

  /**
   * Check if two array derefes are identical
   * @param strict is 1 if the low and high indices must match; 0
   * means the sizes must match.
   *
   */
  int isEqual (Array *a, int strict);

  /**
   * Check if two arrays have compatible parameters (matching
   * dimensions)
   */
  int isDimCompatible (Array *a);

  /**
   * Create a new one-dimensional dense array.
   * @param e is an integer expression corresponding to the size of
   * the array
   * @param f is an integer expression. If f is not NULL, then the
   * array range is e..f; otherwise it is 0..(e-1).
   */
  Array (Expr *e, Expr *f = NULL);

  ~Array ();

  void Print (FILE *fp);

  Array *Clone ();		/* returns a deep copy */

  int isExpanded() { return expanded; }	
  /* returns 1 if expanded array, 0 otherwise */

  int size(); /**< returns total number of elements */

 private:

  Array ();			/* for deep copy only */

  int dims;			/**< number of dimensions */

  /* this is */
  struct range {
    union {
      struct {
	Expr *lo, *hi;
      } ue;			/* unexpanded */
      struct {
	int lo, hi;
      } ex;			/* expanded */
    } u;
  } *r;				/**< range for each dimension */

  Array *next;			/**< for sparse arrays */
  unsigned int deref:1;		/**< 1 if this is a dereference, 0
				   otherwise */
  unsigned int expanded:1;	/**< 1 if this is expanded, 0 otherwise */
};


/**
 * Array expressions
 *
 */
class AExpr {
 public:
  /**
   * An array expression is either a basic expression, a concatenation,
   * or a comma
   */
  enum type {
    CONCAT, COMMA, /*SUBRANGE,*/ EXPR
  };
  
  AExpr (ActId *e);
  AExpr (Expr *e);
  AExpr (type t, AExpr *l, AExpr *r);
  ~AExpr();

  int isEqual (AExpr *a); /**< Check if two array expressions are
			     equal */

  AExpr *GetLeft () { return l; }
  AExpr *GetRight () { return r; }
  void SetRight (AExpr *a) { r = a; }
  
  void Print (FILE *fp);

  AExpr *Clone ();

  InstType *getInstType (Scope *);
  InstType *getExpInstType (Scope *); /**< expanded type */

  long Expand (ActNamespace *, Scope *); /**< return index into value
					    space based on the size
					    of the array */

 private:
  enum type t;
  AExpr *l, *r;
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

  void Print (FILE *fp, ActId *stop = NULL); /**< print ID to the
						specified output stream,
						stopping at the
						specified
						point. Default is
						print the entire ID */

  ActId *Clone ();

 private:
  mstring_t *name;		/**< name of the identifier */
  Array *a;			/**< array reference/dereference */
  ActId *next;			/**< any `.' reference */
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

#if 0
  double xr;			/**< expanded real value */

  long xi;			/**< expanded integer or boolean
				   value, or index for more complex
				   objects */

  InstType *xt;			/**< expanded type value */
#endif
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

  virtual void Expand (ActNamespace *, Scope *, int meta_only = 0) { fatal_error ("Need to define Expand() method!"); }

 private:
  ActBody *next;
};


/**
 * Instance 
 */
class ActBody_Inst : public ActBody {
 public:
  ActBody_Inst(InstType *, const char *);
  void Expand (ActNamespace *, Scope *, int meta_only = 0);
  Type *BaseType ();

 private:
  InstType *t;
  const char *id;
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

  void Expand (ActNamespace *, Scope *, int meta_only = 0);
  
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
    COMMA,
    AND,
    OR,
    BAR
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

  void Expand (ActNamespace *, Scope *, int meta_only = 0);

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

  void Expand (ActNamespace *, Scope *, int meta_only = 0);

 private:
  ActBody_Select_gc *gc;
};

/*
  Typecheck expression: returns -1 on failure
*/
#define T_ERR         -1

#define T_STRICT     0x20
#define T_PARAM      0x40

#define T_INT        0x1
#define T_REAL       0x2
#define T_BOOL       0x3
#define T_PROC       0x4
#define T_CHAN       0x5
#define T_DATA       0x6
#define T_SELF       0x7   /* special type, "self" */

int act_type_expr (Scope *, Expr *);
int act_type_var (Scope *, ActId *);
int act_type_conn (Scope *, ActId *, AExpr *);
int act_type_conn (Scope *, AExpr *, AExpr *);
const char *act_type_errmsg (void);

void print_expr (FILE *fp, Expr *e);
void type_set_position (int l, int c, char *n);

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
