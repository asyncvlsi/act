/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __ACT_BODY_H__
#define __ACT_BODY_H__


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

  int isEqual (ActId *other);

 private:
  mstring_t *name;		/**< name of the identifier */
  Array *a;			/**< array reference/dereference */
  ActId *next;			/**< any `.' reference */

  ValueIdx *rawValueIdx (Scope *);
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

  virtual ActBody *Clone() { return NULL; }

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

  InstType *getType () { return t; }
  const char *getName() { return id; }

  ActBody *Clone ();

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

  ActBody *Clone ();

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

  ActBody *Clone();
  
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

  ActBody *Clone();

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

  ActBody *Clone();

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

  ActBody *Clone();

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
  ActBody *Clone();
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
  ActBody *Clone();
  
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

  enum langtype {
    LANG_PRS,
    LANG_CHP,
    LANG_HSE,
    LANG_SPEC,

    LANG_SIZE,
    LANG_SPICE
  };

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
  
  ActBody_Lang (enum langtype _t, void *l) {
    t = _t;
    lang = l;
  }

  void Expand (ActNamespace *, Scope *);
  void Print (FILE *fp);
  ActBody *Clone();

 private:
  enum langtype t;
  void *lang;
};





#endif /* __ACT_BODY_H__ */
