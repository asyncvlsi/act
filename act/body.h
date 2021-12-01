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
#ifndef __ACT_BODY_H__
#define __ACT_BODY_H__

#include <act/act_id.h>

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
  ActBody ();
  virtual ~ActBody();
  
  void Append (ActBody *b);
  ActBody *Tail ();
  ActBody *Next () { return next; }

  virtual ActBody *Clone() { return NULL; }

  virtual void Expand (ActNamespace *, Scope *) { fatal_error ("Need to define Expand() method!"); }

  void Expandlist (ActNamespace *, Scope *);

  virtual void Print (FILE */*fp*/) { }

  void updateInstType (list_t *namelist, InstType *it);

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
  void updateInstType (InstType *u);

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

  ActBody *getBody() { return b; }

 private:
  ActBody_Loop::type t;			/**< type of loop */
  const char *id;			/**< loop variable */
  Expr *lo, *hi;		/**< range */
  ActBody *b;			/**< body of the loop */
};

class ActBody_Select_gc {
 public:
  ActBody_Select_gc (const char *_id, Expr *_lo, Expr *_hi,
		     Expr *_g, ActBody *_s) {
    id = _id;
    lo = _lo;
    hi = _hi;
    g = _g;
    s = _s;
    next = NULL;
  }
  ActBody_Select_gc (Expr *_g, ActBody *_s) {
    id = NULL;
    lo = NULL;
    hi = NULL;
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
  int isElse() { return g == NULL ? 1 : 0; }

  ActBody *getBody() { return s; }
  ActBody_Select_gc *getNext() { return next; }

  ActBody_Select_gc *Clone ();

private:
  const char *id;
  Expr *lo, *hi;
  Expr *g;			/**< guard */
  ActBody *s;			/**< statement */
  ActBody_Select_gc *next;		/**< rest of the selection */

  friend class ActBody_Select;
  friend class ActBody_Genloop;
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

  ActBody_Select_gc *getGC() { return gc; }

private:
  ActBody_Select_gc *gc;
};

class ActBody_Genloop : public ActBody {
 public:
  ActBody_Genloop (ActBody_Select_gc *_gc) {
    gc = _gc;
  }
  void Expand (ActNamespace *, Scope *);
  ActBody *Clone();

  ActBody_Select_gc *getGC() { return gc; }

private:
  ActBody_Select_gc *gc;
};

class ActBody_Assertion : public ActBody {
public:
  ActBody_Assertion (Expr *_e, const char *_msg = NULL) {
    type = 0;
    u.t0.e = _e;
    u.t0.msg = _msg;
  }
  ActBody_Assertion (ActId *_id1, ActId *_id2, int op,
		     const char *_msg = NULL) {
    type = 2;
    u.t2.id1 = _id1;
    u.t2.id2 = _id2;
    u.t2.msg = _msg;
    u.t2.op = op;
  }
  ActBody_Assertion (InstType *nu, InstType *old) {
    type = 1;
    u.t1.nu = nu;
    u.t1.old = old;
  }
  ~ActBody_Assertion () {
    if (type == 0) {
      if (u.t0.e) {
	expr_free (u.t0.e);
      }
    }
  }
  void Expand (ActNamespace *, Scope *);
  ActBody *Clone();
private:
  union {
    struct {
      Expr *e;
      const char *msg;
    } t0;
    struct {
      InstType *nu, *old;
    } t1;
    struct {
      int op;
      ActId *id1, *id2;
      const char *msg;
    } t2;
  } u;
  int type;
};

class ActBody_Print : public ActBody {
public:
  ActBody_Print (list_t *_l) {
    l = _l;
  }
  ~ActBody_Print () {
    list_free (l);
  }
  void Expand (ActNamespace *, Scope *);
  ActBody *Clone();
private:
  list_t *l;
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
struct act_refine;
struct act_spec;
struct act_sizing;
struct act_initialize;
struct act_dataflow;

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
    LANG_REFINE,
    LANG_SIZE,
    LANG_INIT,
    LANG_DFLOW
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

  ActBody_Lang (act_refine *r) {
    t = LANG_REFINE;
    lang = r;
  }

  ActBody_Lang (act_sizing *s) {
    t = LANG_SIZE;
    lang = s;
  }

  ActBody_Lang (act_initialize *init) {
    t = LANG_INIT;
    lang = init;
  }

  ActBody_Lang (act_dataflow *dflow) {
    t = LANG_DFLOW;
    lang = dflow;
  }
  
  void Expand (ActNamespace *, Scope *);
  void Print (FILE *fp);
  ActBody *Clone();

  void *getlang() { return lang; }
  enum langtype gettype() { return t; }

 private:
  enum langtype t;
  void *lang;
};


/* standard setup/teardown functions for syntactic replication */
void act_syn_loop_setup (ActNamespace *ns, Scope *s,
			    const char *id, Expr *lo, Expr *hi,
			    
			    /* outputs */
			 ValueIdx **vx, int *ilo, int *ihi);

void act_syn_loop_teardown (ActNamespace *ns, Scope *s,
			    const char *id, ValueIdx *vx);


#endif /* __ACT_BODY_H__ */
