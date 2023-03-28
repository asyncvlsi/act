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
#ifndef __LANG_H__
#define __LANG_H__

#include "expr.h"
#include <common/mstring.h>
#include <common/list.h>
#include <common/array.h>

class ActId;

/**
 * @file lang.h
 *
 * @brief Contains the data structures for each ACT sub-language
 */

/**
 * @class act_attr
 * @brief Attribute list associated with an instance.
*/
struct act_attr {
  const char *attr; ///< the name of the attribute
  Expr *e;	    ///< the expression that holds the value of the attribute
  struct act_attr *next;  ///< linked list next pointer
};

typedef struct act_attr act_attr_t;


/*------------------------------------------------------------------------
 *
 *  The PRS sublanguage: gate-level abstraction for the circuit
 *
 *------------------------------------------------------------------------
 */

/**
 * Enumeration type for the prs guards used in the prs sub-language
 * data structure.
 */
enum act_prs_expr_type {
  ACT_PRS_EXPR_AND = 0,      ///< "&" operator
  ACT_PRS_EXPR_OR  = 1,      ///< "|" operator
  ACT_PRS_EXPR_VAR = 2,      ///< leaf variable
  ACT_PRS_EXPR_NOT = 3,      ///< "~" operator
  ACT_PRS_EXPR_LABEL = 4,    ///< leaf "@label"
  ACT_PRS_EXPR_ANDLOOP = 5,  ///< replicated &-loop
  ACT_PRS_EXPR_ORLOOP = 6,   ///< replicated |-loop

  ACT_PRS_EXPR_TRUE = 7,     ///< constant "true"
  ACT_PRS_EXPR_FALSE = 8     ///< constant "false"
};


/**
 * Sizing specifier for variables
 */
typedef struct {
  Expr *w;              ///< transistor width
  Expr *l;		///< transistor length
  int flavor;		///< type of transistor (its flavor)
  Expr *folds;		///< number of folds, the folding specification if any
} act_size_spec_t;


/**
 * A production rule expression, corresponding to the guard for the
 * production rule
 */
typedef struct act_prs_expr {
  unsigned int type:4;		///< an act_prs_expr_type value
  union {
    struct {
      struct act_prs_expr *l;  ///< left pointer in expression tree
      struct act_prs_expr *r;  ///< right pointer in expression tree
      struct act_prs_expr *pchg; ///< internal precharge, only for an AND
      int pchg_type;		///< precharge type: 1 for +, 0 for -
    } e;			///< a standard PRS expression
    struct {
      ActId *id;		///< name of the variable
      act_size_spec_t *sz;	///< size specifier, if any
    } v;			///< a leaf variable
    struct {
      char *label;		///< name of the label
    } l;			///< a label
    struct {
      char *id;			///< loop iteration variable
      Expr *lo;                 ///< low value of loop range
      Expr *hi;		        ///< high value of loop range
      struct act_prs_expr *e;   ///< body of the loop
    } loop;			///< used for AND/OR loops
  } u;
} act_prs_expr_t;

/**
 * Enumeration type for elements of a prs sub-language body
 */
enum act_prs_lang_type {
  ACT_PRS_RULE = 0,   ///< a production rule
  ACT_PRS_GATE = 1,   ///< a transmission gate / pass transistor
  ACT_PRS_LOOP = 2,   ///< a loop of production rules
  ACT_PRS_TREE = 3,   ///< a tree { ... } block
  ACT_PRS_SUBCKT = 4, ///< a subckt { ... } block
  ACT_PRS_CAP = 5     ///< a capacitor
};

/**
 * Structure that holds a prs sub-language body. This consists
 * of a linked-list of individual items in the prs body.
 */
typedef struct act_prs_lang {
  struct act_prs_lang *next;  ///< the linked list next pointer field
  unsigned int type:3;        ///< an act_prs_lang_type that describes
			      ///what the prs entry is
  union {
    struct {
      act_attr_t *attr;       ///< any attribute associated with the
			      ///production rule
      act_prs_expr_t *e;      ///< the guard of the production rule
      ActId *id;              ///< the variable on the right hand side
			      ///of the prs; re-used as a char * if it
			      ///is a label
      unsigned int arrow_type:2; ///< The kind of production rule:
				 ///normal (->) = 0, combinational
				 ///(=>) = 1, c-element (#>) = 2
      unsigned int dir:1;	///< direction for the right hand side
				///0 = -, 1 = +
      unsigned int label:1;	///< 1 if it has a label on the RHS, 0
				///otherwise
    } one;			///< information for one prs element,
				///used for ACT_PRS_RULE
    struct {
      act_attr_t *attr;		///< any attribute associated with the gate
      ActId *g;			///< the gate terminal (used for
				///n-type and transmission gate)
      ActId *s;			///< the source terminal
      ActId *d;			///< the drain terminal
      ActId *_g;		///< the complemented gate (used for
				///p-type and transmission gate)
      act_size_spec_t *sz;	///< sizing specification
    } p;			///< pass gate: use g for n-type, use
				///_g for p-type (full transmission
				///gate uses both). Used by
				///ACT_PRS_GATE and ACT_PRS_CAP (cap
				///uses s and d fields)
    struct {
      const char *id;		///< loop id
      Expr *lo;			///< low range (NULL means 0..hi-1)
      Expr *hi;			///< high range
      struct act_prs_lang *p;	///< loop body
    } l;			///< used for ACT_PRS_LOOP. Also used
				///by ACT_PRS_TREE and
				///ACT_PRS_SUBCKT. In this case,
				///id/lo/hi are all unused.
  } u;
} act_prs_lang_t;

/**
 * Structure that holds all the prs { } blocks in a particular
 * scope. This is a linked-list of blocks, each of which can have
 * their own voltage domain specification.
 */
struct act_prs {
  ActId *vdd;			///< power supply
  ActId *gnd;			///< ground
  ActId *psc;			///< p substrate terminal
  ActId *nsc;			///< n substrate terminal
  int leak_adjust;		///< 1 for "leakage adjustment" mode
				///during sizing
  act_prs_lang_t *p;		///< the actual prs sub-languge elements
  struct act_prs *next;		///< the next prs block
};



/*------------------------------------------------------------------------
 *
 *  The CHP sublanguage: behavioral description
 *
 *------------------------------------------------------------------------
 */


/**
 * Enumeartion tyoe to capture the elements of a chp sub-language body
 */
enum act_chp_lang_type {
  ACT_CHP_COMMA = 0,           ///< comma or parallel composition
  ACT_CHP_SEMI = 1,            ///< sequential composition
  ACT_CHP_SELECT = 2,          ///< selection statement
  ACT_CHP_SELECT_NONDET = 3,   ///< non-deterministic selection statement
  ACT_CHP_LOOP = 4,            ///< CHP while loop
  ACT_CHP_DOLOOP = 5,          ///< CHP do loop
  ACT_CHP_SKIP = 6,            ///< skip statement
  ACT_CHP_ASSIGN = 7,          ///< assignment
  ACT_CHP_SEND = 8,            ///< send operation
  ACT_CHP_RECV = 9,            ///< receive operation

  ACT_CHP_FUNC = 10,	       ///< used for log(...) logging statement

  ACT_CHP_SEMILOOP = 11,       ///< syntactic replication with
			       ///semi-colon; removed after expansion
  ACT_CHP_COMMALOOP = 12,      ///< syntactic replication with commas;
			       ///removed after expansion

  ACT_CHP_HOLE = 13,	      ///< a "hole"---only used by downstream tools

  ACT_CHP_ASSIGNSELF = 14,    ///< same as assign, used to indicate
			      ///self-assignment in some tools

  ACT_CHP_MACRO = 15,	     ///< a macro call, gets removed when
			     ///chp_expand() is called

  ACT_HSE_FRAGMENTS = 16     ///< used to support fragmented hse
};

#define ACT_CHP_STMT_END 17  ///< 1 more than the last enumeration of
			     ///act_chp_lang_type

struct act_chp_lang;


/**
 * Data structure for guarded commands
 *
 *   id = NULL : normal guard
 *   id != NULL : syntactic replication, where "id" is the variable,
 *                lo and hi are the low and high values for the range
 *
 *   g  : guard expression, NULL means "else" for selection
 *        statements, and true for loops.
 *
 *   s  : statement (might be NULL in the case of [g]
 *
 *   next : used to construct list of guard -> statement, NULL
 *   terminated list.
 *
 */
typedef struct act_chp_gc {
  const char *id;		///< the loop id for syntactic replication
  Expr *lo, *hi;		
  Expr *g;			///< the guard
  struct act_chp_lang *s;	///< the statement
  struct act_chp_gc *next;	///< next pointer for linked-list
} act_chp_gc_t ;


/**
 * Data structure for the chp sub-language body
 */
typedef struct act_chp_lang {
  int type;			///< this is taken from act_chp_lang_type
  const char *label;		///< label for this item, normally NULL
  void *space;			///< this space for rent!
  union {
    struct {
      ActId *id;		///< variable on the LHS
      Expr *e;			///< expression on the RHS
    } assign;			///< assignment statement id := e

    /* A bi-directional channel can have both var and e that are non-NULL */
    struct {
      ActId *chan;		///< channel for communication
      ActId *var;		///< variable to be assigned
      Expr *e;			///< expression to be sent
      unsigned int flavor:2;	///< 0 = blank, 1 = up, 2 = down; used
				///for explicit two-phase CHP
      unsigned int convert:2;   ///< 0 = nothing, 1 = bool(.), 2 = int(.)
    } comm;			///< used for send/recv

    /* used for log(...) */
    struct {
      mstring_t *name;		///< function name
      list_t *rhs;	        ///< arguments, a list of
				///act_func_arguments
    } func;			///< currently only used for log(..) 

    /* used for process/type macros */
    struct {
      ActId *id;		///< the name of the instance
      mstring_t *name;		///< the macro name
      list_t *rhs;		///< the argument list, list of Expr *
    } macro;			///< macro call

    /* ;/, : this is the list of statements that are either comma or
       semi-colon separated */
    struct {
      list_t *cmd;		///< a list of act_chp_lang_t pointers
    } semi_comma;		///< used for comma and semicolon

    act_chp_gc_t *gc;		///< loop or select; also used for a
				///do-loop, where there is exactly one gc

    struct {
      const char *id;		///< loop variable
      Expr *lo, *hi;
      struct act_chp_lang *body; ///< body to be replicated
    } loop;			///< replication construct

    struct {
      const char *nextlabel;	///< next label
      list_t *exit_conds;	///< list of exit conditions, Expr *
				///(boolean expr) + char * (ID)
      struct act_chp_lang *body; ///< fragment body
      struct act_chp_lang *next; ///< next fragment
    } frag;			///< HSE fragments
    
  } u;
} act_chp_lang_t;


/**
 * Used to represent log(...) arguments
 */
typedef struct act_func_arguments {
  unsigned int isstring:1;	///< true if string, false otherwise
  union {
    mstring_t *s;		///< string 
    Expr *e;			///< or expression
  } u;
} act_func_arguments_t;


/**
 * CHP sub-language
 */
struct act_chp {
  ActId *vdd;			///< power supply
  ActId *gnd;			///< ground
  ActId *psc;			///< p-substrate terminal
  ActId *nsc;			///< n-substrate terminal
  act_chp_lang_t *c;		///< chp body
  int is_synthesizable;		///< 1 if this is synthesizable, 0 otherwise
};




/*------------------------------------------------------------------------
 *
 *  The specification sublanguage: used for verification and asserting
 *  invariants; also used to specify arbiters
 *
 *
 *------------------------------------------------------------------------
 */

#define ACT_SPEC_ISTIMING(x)  ((x)->type == -1 || (x)->type == -2 || (x)->type == -3 || (x)->type == -4)
#define ACT_SPEC_ISTIMINGFORK(x)  ((x)->type == -1 || (x)->type == -2)


/**
 * The specification sub-language
 */
struct act_spec {
  int isrequires;	///< 1 if this is a requires clause;
			///otherwise it is ensures or there's
                        ///no flag
  
  /**
   *  The type field. For values 0..n, this is the index into the
   * table of valid spec directives specified in the ACT global
   * configuration file. 
   *
   * When type = -1/-2, it is a timing fork. In that case, given a
   * directive of the form `timing lhs : first < [expr] second`, 
   *  - count = 4
   *  - ids[0] = lhs, ids[1] = first, ids[2] = second
   *  - ids[3] = NULL or an Expr * for the timing margin
   */
  int type;
  
  int count;   ///< the number of ids; -1 = all nodes in the process
  ActId **ids; ///< the array of identifiers in the spec directive

  /**
   * Flags for timing directives
   *  - 0x03 : 0 = no dir, 1 = +, 2 = -
   *  - 0x04 : 1 = ?, 0 = nothing
   *  - 0x08 : 1 = +1, 0 = current iteration. Can only be
   *    set for ids[1] and ids[2].
   */  
  int *extra;  
  struct act_spec *next;
};

class ActBody;


/*------------------------------------------------------------------------
 *
 *  The refinement sublanguage
 *
 *------------------------------------------------------------------------
 */

/**
 * The refinement sub-language just contains an ActBody
 */
struct act_refine {
  ActBody *b;			///< the body of the refine { ... }
};


/*------------------------------------------------------------------------
 *
 *  The gate sizing sublanguage
 *
 *------------------------------------------------------------------------
 */

/**
 * An individual sizing directive
 */
struct act_sizing_directive {
  ActId *id;			///< the signal to be sized
  int flav_up;			///< transistor flavor for pull-up
  int flav_dn;			///< transistor flavor for pull-down
  Expr *eup;			///< drive strength for pull-up
  Expr *edn;			///< drive strength for pull-down
  Expr *upfolds;		///< number of folds for pull-up
  Expr *dnfolds;		///< number of folds for pull-down
  
  /*-- sizing loop: NOTE: loop_id == NULL meanas the rest of the
       fields are garbage  --*/
  const char *loop_id;		///< for a loop in the sizing body
  Expr *lo, *hi;		
  A_DECL (act_sizing_directive, d); ///< loop body
};

/**
 * The sizing { ... } body data 
 */
struct act_sizing {
  // p_n_mode  0/1  0 = default, 1 = sqrt sizing
  // unit_n 5
  int p_specified;		///< 1 if p_n_mode is specified, 0 otherwise
  int unit_n_specified;		///< 1 if unit_n specified, 0 otherwise
  int leak_adjust_specified;	///< 1 if leak_adjust specified, 0 otherwise
  Expr *p_n_mode_e;		///< p_n_mode expression
  Expr *unit_n_e;		///< unit_n expression
  Expr *leak_adjust_e;		///< leak_adjust expression
  int p_n_mode;			///< p_n_mode after expansion (default 0)
  int unit_n;			///< unit_n after expansion
  int leak_adjust;		///< leak_adjust after expansion (default 0)
  A_DECL (act_sizing_directive, d); ///< all sizing directives
  act_sizing *next;		    ///< next sizing body
};


/*------------------------------------------------------------------------
 *
 *  The initialization sublanguage, used to specify the initial state
 *  and reset protocol.
 *
 *------------------------------------------------------------------------
 */

/**
 * The Initialize { ... } body. Only used in the global namespace.
 */
struct act_initialize {
  list_t *actions;		///< the list of actions. This is a
				///list of act_chp_lang_t pointers
  act_initialize *next;		///< next initialize block
};

/*------------------------------------------------------------------------
 *
 * The dataflow sublanguage
 *
 *------------------------------------------------------------------------
 */

/**
 * The type field used for dataflow elements
 */
enum act_dataflow_element_types {
 ACT_DFLOW_FUNC = 0,		///< function dataflow element
 ACT_DFLOW_SPLIT = 1,		///< split dataflow element
 ACT_DFLOW_MERGE = 2,		///< merge dataflow element
 ACT_DFLOW_MIXER = 3,		///< mixer dataflow element
 ACT_DFLOW_ARBITER = 4,		///< arbiter dataflow element
 ACT_DFLOW_CLUSTER = 5,		///< a cluster of dataflow elements
 ACT_DFLOW_SINK = 6		///< a dataflow sink
};

typedef struct  {
  act_dataflow_element_types t;	///< the type
  union {
    struct {
      Expr *lhs;		///< expression
      ActId *rhs;		///< channel output
      Expr *nbufs;		///< # of buffers
      int istransparent:1;	///< transparent v/s opaque
      Expr *init;		///< initial token, if any on the output
    } func;			///< used for ACT_DFLOW_FUNC
    struct {
      ActId *guard;		///< the condition
      ActId **multi;		///< set of channels (the multiple
				///channel end)
      int nmulti;		///< number of channels in multi
      ActId *single;		///< the single channel end
      ActId *nondetctrl;	///< channel for non-deterministic
				///< control out and mixer output
    } splitmerge;		///< used by split, merge, mixer,
				///arbiter. The elements have a
				///"single" data channel and, and a
				///"multi" data channel end.
    struct {
      ActId *chan;		///< the input channel
    } sink;			///< a dataflow sink
    list_t *dflow_cluster;	///< a cluster is a list of dataflow elements
  } u;
} act_dataflow_element;

/**
 * An order directive in the dataflow language
 */
struct act_dataflow_order {
  list_t *lhs;			///< first list of ActId pointers (channels)
  list_t *rhs;			///< second list of ActId pointers (channels)
};


/**
 * The dataflow sub-language
 */
struct act_dataflow {
  list_t *dflow;		///< list of dataflow elements

  /**
   * This field allows for hierarchical optimization without requiring full
   * ACT program analysis.
   *
   * Suppose you have a dataflow channels c and d in process foo, and c and d
   * are passed in to a sub-process bar, and internally in that sub-process d
   * is computed from c.
   *
   * The optimizations in the dataflow body in foo need to know that d
   * depends on c. Otherwise, it might re-structure the dataflow graph and
   * create a synchronization point where d and c might have to be produced
   * concurrently (or worse). If it did that, the sub-process path would
   * result in deadlock.
   *
   * It is a list of act_dataflow_order elements, and each of which is a pair
   * of lists. Each of those lists is a list of IDs. The directive looks like:
   *
   *       a1, a2, ..., aN < b1, b2, ..., bM
   *
   * which means there is a dependency from ai to bj for all pairs.
   */
  list_t *order;
};

void prs_print (FILE *, act_prs *);
void chp_print (FILE *, act_chp *);
void chp_print (FILE *fp, act_chp_lang_t *c);
void hse_print (FILE *, act_chp *);
void spec_print (FILE *, act_spec *);
void refine_print (FILE *, act_refine *);
void sizing_print (FILE *, act_sizing *);
void initialize_print (FILE *, act_initialize *);
void dflow_print (FILE *, act_dataflow *);
void dflow_print (FILE *, act_dataflow_element *);

class ActNamespace;
class Scope;


/**
 * @class act_languages
 *
 * @brief This holds all the sub-langugae bodies in a
 * namespace/user-defined type definition
 */
class act_languages {
public:
  act_languages () {
    prs = NULL;
    chp = NULL;
    hse = NULL;
    spec = NULL;
    refine = NULL;
    sizing = NULL;
    init = NULL;
    dflow = NULL;
  }
  void Print (FILE *fp) {
    if (chp) { chp_print (fp, chp); }
    if (hse) { hse_print (fp, hse); }
    if (prs) { prs_print (fp, prs); }
    if (spec) { spec_print (fp, spec); }
    if (sizing) { sizing_print (fp, sizing); }
    if (refine) { }
    if (init) { initialize_print (fp, init); }
    if (dflow) { dflow_print (fp, dflow); }
  }

  act_chp *getchp () { return chp; }
  void setchp (act_chp *x) { chp = x; }

  act_chp *gethse () { return hse; }
  void sethse (act_chp *x) { hse = x; }

  act_prs *getprs () { return prs; }
  void setprs (act_prs *x) { prs = x; }

  act_spec *getspec () { return spec; }
  void setspec (act_spec *x) { spec = x; }

  act_refine *getrefine() { return refine; }
  void setrefine (act_refine *x) { refine = x; }

  act_sizing *getsizing() { return sizing; }
  void setsizing(act_sizing *s) { sizing = s; }

  act_initialize *getinit() { return init; }
  void setinit(act_initialize *s) { init = s; }

  act_dataflow *getdflow() { return dflow; }
  void setdflow (act_dataflow *s) { dflow = s; }

  act_languages *Expand (ActNamespace *ns, Scope *s);

  /**
   * @return 1 if the language block has some circuit description
   * language, 0 otherwise
  */
  int hasCktLang () {
    if (chp || hse || prs || dflow) return 1;
    return 0;
  }

  /**
   * @return 1 if the language is at a level of detail sufficient to
   * generate a netlist (i.e. a prs sub-language exists), 0 otherwise
  */
  int hasNetlistLang () {
    if (prs) return 1;
    return 0;
  }
  
 private:
  act_chp *chp, *hse;
  act_prs *prs;
  act_spec *spec;
  act_refine *refine;
  act_sizing *sizing;
  act_initialize *init;
  act_dataflow *dflow;
};


act_initialize *initialize_expand (act_initialize *, ActNamespace *, Scope *);
act_chp *chp_expand (act_chp *, ActNamespace *, Scope *);
void chp_check_channels (act_chp_lang_t *c, Scope *s);
act_prs *prs_expand (act_prs *, ActNamespace *, Scope *);
act_spec *spec_expand (act_spec *, ActNamespace *, Scope *);
void refine_expand (act_refine *, ActNamespace *, Scope *);
act_sizing *sizing_expand (act_sizing *, ActNamespace *, Scope *);
act_dataflow *dflow_expand (act_dataflow *, ActNamespace *, Scope *);

const char *act_spec_string (int type);
const char *act_dev_value_to_string (int);
int act_dev_string_to_value (const char *s);

act_attr_t *inst_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s);
act_attr_t *prs_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s);

void chp_expand_macromode (int mode);
act_chp_lang_t *chp_expand (act_chp_lang_t *, ActNamespace *, Scope *);
void act_print_size (FILE *fp, act_size_spec_t *sz);


act_prs_expr_t *act_prs_complement_rule (act_prs_expr_t *e);
act_prs_expr_t *act_prs_celement_rule (act_prs_expr_t *e);
void act_print_one_prs (FILE *fp, act_prs_lang_t *p);

typedef void *(*ACT_VAR_CONV) (void *, void *);

/*
  at_hash : hash table from label strings to act_prs_lang_t pointers
  All variable pointers at the leaves are converted using the conv_var
  function that is an argument. First argument of the function is the
  cookie, the second is the ID pointer for the expression.
*/
act_prs_expr_t *act_prs_expr_nnf (void *cookie,
				  struct Hashtable *at_hash,
				  act_prs_expr_t *e,
				  ACT_VAR_CONV conv_var);

char *act_prs_expr_to_string (list_t *id_list,  act_prs_expr_t *e);
char *act_expr_to_string (list_t *id_list, Expr *e);

/* 
   Pass in a non-NULL list of ActId *'s.

   Collects any new ids and adds them to the list l
   This list is suitable for use in act_expr_to_strnig and
   act_prs_expr_to_string
*/
void act_expr_collect_ids (list_t *l, Expr *e);

void act_prs_expr_free (act_prs_expr_t *e);
void act_chp_free (act_chp_lang_t *);
void act_chp_macro_check (Scope *s, ActId *id);

ActId *expand_var_write (ActId *id, ActNamespace *ns, Scope *s);

int act_hse_direction (act_chp_lang_t *, ActId *);

act_size_spec_t *act_expand_size (act_size_spec_t *sz, ActNamespace *ns, Scope *s);

/* only use this on guard expressions */
int act_expr_has_neg_probes (Expr *e);


#endif /* __LANG_H__ */
