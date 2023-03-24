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

/**
 * @class ActBody
 *
 * @brief This class is used to hold the contents of the body of any
 * user-defined type or namespace. This is the data structure created
 * during parsing, and stored in the unexpanded versions of namespaces
 * and user-defined types.
 *
 * This data structure corresponds to a list of items in the body of a
 * namespace or user-defined type. This is never used directly, but
 * rather used as the base class for all the different types of body
 * items defined. The common functionality across all ActBody derived
 * classes is implemented here.
 */
class ActBody {
 public:
  ActBody (int line); ///< @param line is the line number
		      ///corresponding to this ActBody item
  
  virtual ~ActBody();

  /**
   * Append a new ActBody to the end of the list of items
   * @param b is the ActBody to be appended
   */
  void Append (ActBody *b);

  /**
   * Insert b right after the current body element in the list of
   * elements 
   * @param b is the list of items to be inserted into the current
   * body
   */
  void insertNext (ActBody *b);

  /**
   * This returns the last element in the body list
   * @return the last element of the list.
   */
  ActBody *Tail ();

  /** @return the next element in the body item list
   */
  ActBody *Next () { return next; }

  /**
   * @return a deep copy of the body item list
   */
  virtual ActBody *Clone() { return NULL; }

  /**
   * Expand out the current body item, updating the necessary data structures
   * as the expansion process proceeds.
   */
  virtual void Expand (ActNamespace *, Scope *) { fatal_error ("Need to define Expand() method!"); }

  /**
   * Traverse the body item list, expanding each element of the list
   */
  void Expandlist (ActNamespace *, Scope *);

  /**
   * Print the body item list to the specified file
   */
  virtual void Print (FILE */*fp*/) { }

  /**
   * Look through for the specified list of names, and update their
   * instance type to the new one specified. This is used to support
   * overrides.
   * @param namelist is a list of names (char * list)
   * @param it is the updated instance type for the specified identifiers
   */
  void updateInstType (list_t *namelist, InstType *it);

  /**
   * @return the saved away line number associated with a body
   */
  int getLine () { return _line; }

protected:
  int _line; ///< saved away line number information

private:
  ActBody *next; ///< next pointer for linked list of body items
};


/**
 * @class ActBody_Inst
 *
 * @brief This class is used to hold information about an instance in
 * the body of a namespace and/or user-defined type.
 */
class ActBody_Inst : public ActBody {
 public:
  /**
   * An instance on the specified line, consisting of a type specifier
   * and string corresponding to the name of the instance. Note that
   * array specifiers will be part of the type.
   */
  ActBody_Inst(int line, InstType *, const char *);

  /**
   * Expand the instance, and add it to the scope table
   */
  void Expand (ActNamespace *, Scope *);

  /**
   * @return the "base" type for the instance in this particular body
   * item
   */
  Type *BaseType ();

  /**
   * Print the instance item to a file
   * @param fp is the file pointer
   */
  void Print (FILE *fp);

  /**
   * @return the actual instance type for this body item
   */
  InstType *getType () { return t; }

  /**
   * @return the name of the identifier being instantiated
   */
  const char *getName() { return id; }

  /**
   * Update the instance type with a new type. This is used by
   * overrides.
   * @param u is the new instance type
   */
  void updateInstType (InstType *u);

  /**
   * Make a deep copy of this instance
   */
  ActBody *Clone ();

 private:
  InstType *t;   ///< the type
  const char *id; ///< the name of the identifier to be instantiated
};


/**
 * @class ActBody_Attribute
 *
 * @brief This is used to record attributes associated with an
 * instance name.
 */
class ActBody_Attribute : public ActBody {
public:
  /**
   * Create a body item that corresponds to an attribute associated
   * with an instance.
   *
   * @param line is the line number
   * @param _inst is the name of the instance that has the attribute
   * @param _a is the attribute associated with the instance
   * @param _arr is the array for the instance, if any
   */
  ActBody_Attribute(int line,
		    const char *_inst, act_attr *_a, Array *_arr = NULL)
    : ActBody (line)
  {
    inst = _inst; a = _a; arr = _arr;
  }

  /**
   * Expand the body item
   */
  void Expand (ActNamespace *, Scope *);
  //void Print (FILE *fp);

  /**
   * @return a deep copy of this body item
   */
  ActBody *Clone ();

private:
  const char *inst;
  act_attr *a;
  Array *arr;
};


/**
 * @class ActBody_Conn
 *
 * @brief This is used to record a connection in the input design
 * file. 
 */
class ActBody_Conn : public ActBody {
 public:

  /**
   * Record a connection between an identifier and an array expression
   * @param line is the line number
   * @param id1 is the LHS
   * @param ae is the array expression
   */
  ActBody_Conn(int line, ActId *id1, AExpr *ae) : ActBody (line) {
    type = 0;
    u.basic.lhs = id1;
    u.basic.rhs = ae;
  }

  /**
   * Record a connection between two array expressions
   * @param line is the line number
   * @param id1 is the LHS array expression
   * @param id2 is the RHS array expression
   */
  ActBody_Conn(int line, AExpr *id1, AExpr *id2) : ActBody (line) {
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

/**
 * @class ActBody_Loop
 *
 * @brief This holds information about loops that has to be
 * unrolled. Loops can be of different types.
 */
class ActBody_Loop : public ActBody {
 public:

  /**
   * Loop for circuit construction. 
   * @param line is the line number
   * @param _id is the loop variable
   * @param _lo is the start index (NULL if this is a 0..hi-1 loop)
   * @param _hi is the high index for the loop
   * @param _b is the body to be replicated
   */
  ActBody_Loop (int line,
		const char *_id, 
		Expr *_lo, /* NULL if this is a 0..hi-1 loop */
		Expr *_hi,
		ActBody *_b) : ActBody (line) {
    id = _id;
    lo = _lo;
    hi = _hi;
    b = _b;
  }

  void Expand (ActNamespace *, Scope *);

  void Print (FILE *fp);

  ActBody *Clone();

  /**
   * @return the loop body
   */
  ActBody *getBody() { return b; }

 private:
  const char *id;		/**< loop variable */
  Expr *lo, *hi;		/**< range */
  ActBody *b;			/**< body of the loop */
};


/**
 * @class ActBody_Select_gc
 *
 * @brief This is used to represent the list of guards and statements
 * for conditional circuit construction
 */
class ActBody_Select_gc {
 public:
  /**
   * Create a guard / ActBody combination
   * @param _g is the guard
   * @param _s is the ActBody
   */
  ActBody_Select_gc (Expr *_g, ActBody *_s) {
    id = NULL;
    lo = NULL;
    hi = NULL;
    g = _g;
    s = _s;
    next = NULL;
  }

  /**
   * This is used to represent a guarded command with an internal loop
   * that is unrolled to construct all the guards of the form:
   *
   *   ( [] i : lo .. hi : G ->  S )
   *
   * @param _id is the loop index
   * @param _lo is the low index (NULL if omittted)
   * @param _hi is the high index (the high index is _hi-1 if _lo is
   * NULL)
   * @param _g is the guard
   * @param _s is the body
   */
  ActBody_Select_gc (const char *_id, Expr *_lo, Expr *_hi,
		     Expr *_g, ActBody *_s) {
    id = _id;
    lo = _lo;
    hi = _hi;
    g = _g;
    s = _s;
    next = NULL;
  }

  /**
   * Set "s" to be the tail of the list of guarded commands 
   * @param s is the tail
   */
  void Append (ActBody_Select_gc *s) {
    next = s;
  }

  /**
   * @return 1 if the guard is an "else", 0 otherwise
   */
  int isElse() { return g == NULL ? 1 : 0; }

  /**
   * @return the body of this selection fragment
   */
  ActBody *getBody() { return s; }

  /**
   * @return the next element in the guarded command list
   */
  ActBody_Select_gc *getNext() { return next; }

  /**
   * @return a deep copy
   */
  ActBody_Select_gc *Clone ();

private:
  const char *id;		/**< loop index */
  /**
   * Loop range. If this is NULL, then the range is from 0 to hi-1.
   */
  Expr *lo, *hi;		/**< loop range */
  Expr *g;			/**< guard */
  ActBody *s;			/**< statement */
  ActBody_Select_gc *next;		/**< rest of the selection */

  friend class ActBody_Select;
  friend class ActBody_Genloop;
};



/**
 * @class ActBody_Select
 *
 * @brief This is used to contain the complete selection statement in
 * the core ACT language.
 */
class ActBody_Select : public ActBody {
 public:
  /**
   * @param line is the line number
   * @param _gc is the list of guarded commands
   */
  ActBody_Select (int line, ActBody_Select_gc *_gc) : ActBody (line) {
    gc = _gc;
  }

  void Expand (ActNamespace *, Scope *);

  ActBody *Clone();

  /**
   * @return the list of guarded commands
   */
  ActBody_Select_gc *getGC() { return gc; }

private:
  ActBody_Select_gc *gc;	///< the guarded command list
};


/**
 * @class ActBody_Genloop
 *
 * @brief This is used to contain a generalized loop in the ACT body.
 */
class ActBody_Genloop : public ActBody {
public:
  /**
   * @param line is the line number
   * @param _gc is the body of the loop (the list of guarded commands)
   */
  ActBody_Genloop (int line, ActBody_Select_gc *_gc) : ActBody (line) {
    gc = _gc;
  }
  void Expand (ActNamespace *, Scope *);
  ActBody *Clone();

  ActBody_Select_gc *getGC() { return gc; }

private:
  ActBody_Select_gc *gc;
};


/**
 * @class ActBody_Assertion
 *
 * @brief This is used to store assertions in the body. There are
 * three types of assertions:
 * 1. A boolean expression is true (with an optional message)
 * 2. id1 === id2 or  id1 !=== id2 : check for connections
 * 3. Internal assertion to check overrides are valid.
 */
class ActBody_Assertion : public ActBody {
public:
  /**
   * @param line is the line number
   * @param _e is the Boolean expression asserted
   * @param _msg is any user-specified message
   */
  ActBody_Assertion (int line, Expr *_e, const char *_msg = NULL)
    : ActBody (line) {
    type = 0;
    u.t0.e = _e;
    u.t0.msg = _msg;
  }
  
  /**
   * @param line is the line number
   * @param _id1 is one identifier
   * @param _id2 is the second identifier
   * @param op is 0 to asserting connectivity, 1 to assert disconnections
   * @param _msg is any user-specified message
   */
  ActBody_Assertion (int line, ActId *_id1, ActId *_id2, int op,
		     const char *_msg = NULL) : ActBody (line) {
    type = 2;
    u.t2.id1 = _id1;
    u.t2.id2 = _id2;
    u.t2.msg = _msg;
    u.t2.op = op;
  }

  /**
   * Internal check for overrides
   */
  ActBody_Assertion (int line, InstType *nu, InstType *old) : ActBody (line) {
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


/**
 * @class ActBody_OverrideAssertion
 *
 * @brief This is added for override checks that can be completed only
 * during the expansion phase. The added complexity is that overrides
 * may depend on the identifier being created (it may be conditionally
 * created, for example). The name_check field is used to check that
 * the identifier in fact exists before testing for overrides.
 */
class ActBody_OverrideAssertion : public ActBody {
public:
  /**
   * @param line is the line number
   * @param name_check is the instance name that must exist before
   * testing for overrides
   * @param it is the original type
   * @param chk is the overridden type
   */
  ActBody_OverrideAssertion (int line,
			     const char *name_check,
			     InstType *it, InstType *chk)
    : ActBody (line) {
    _name_check = name_check;
    _orig_type = it;
    _new_type = chk;
  }
  ~ActBody_OverrideAssertion () { }

  void Expand (ActNamespace *, Scope *);
  ActBody_OverrideAssertion *Clone();
private:
  InstType *_orig_type, *_new_type;
  const char *_name_check;
};


/**
 * @class ActBody_Print
 *
 * @brief This is used to display messages during expansion.
 */
class ActBody_Print : public ActBody {
public:
  /**
   * @param line is the line number
   * @param _l is a list of items to be printed, each of which is a
   * act_func_arguments_t pointer.
   */
  ActBody_Print (int line, list_t *_l) : ActBody (line) {
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



/**
 * @class ActBody_Namespace
 *
 * @brief This is used to switch namespaces during the expansion
 * phase.
 */
class ActBody_Namespace : public ActBody {
public:
  /**
   * @param _ns is the new namespace to be switched to
   */
  ActBody_Namespace (ActNamespace *_ns) : ActBody (-1) {
    ns = _ns;
  }

  void Expand (ActNamespace *, Scope *);
  ActBody *Clone();
  ActNamespace *getNS () { return ns; }
  
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


/**
 * @class ActBody_Lang
 *
 * @brief This is a language body.
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

  ActBody_Lang (int line, act_prs *p) : ActBody (line) {
    t = LANG_PRS;
    lang = p;
  }
  ActBody_Lang (int line, act_chp *c, int ishse = 0) : ActBody (line) {
    if (ishse) {
      t = LANG_HSE;
    }
    else {
      t = LANG_CHP;
    }
    lang = c;
  }

  ActBody_Lang (int line, act_spec *s) : ActBody (line) {
    t = LANG_SPEC;
    lang = s;
  }
  
  ActBody_Lang (int line, enum langtype _t, void *l) : ActBody (line) {
    t = _t;
    lang = l;
  }

  ActBody_Lang (int line, act_refine *r) : ActBody (line) {
    t = LANG_REFINE;
    lang = r;
  }

  ActBody_Lang (int line, act_sizing *s) : ActBody (line) {
    t = LANG_SIZE;
    lang = s;
  }

  ActBody_Lang (int line, act_initialize *init) : ActBody (line) {
    t = LANG_INIT;
    lang = init;
  }

  ActBody_Lang (int line, act_dataflow *dflow) : ActBody (line) {
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


/**
 * Standard function to setup loops for syntactic replication.
 * @param ns is the namespace
 * @param s is the scope
 * @param id is the loop identifier
 * @param lo is the start index (or NULL)
 * @param hi is the high index (which is hi-1 if lo is NULL)
 * @param vx is used to return the ValueIdx for the loop id
 * @param ilo is used to return the actual low value of the range
 * @param ihi is used to return the actual high value of the range
 */
void act_syn_loop_setup (ActNamespace *ns, Scope *s,
			    const char *id, Expr *lo, Expr *hi,
			    
			    /* outputs */
			 ValueIdx **vx, int *ilo, int *ihi);

/**
 * Standard function to tear down setup loops for syntactic replication.
 * @param ns is the namespace
 * @param s is the scope
 * @param id is the loop identifier
 * @param vx is the ValueIdx for the loop id
 */
void act_syn_loop_teardown (ActNamespace *ns, Scope *s,
			    const char *id, ValueIdx *vx);


#endif /* __ACT_BODY_H__ */
