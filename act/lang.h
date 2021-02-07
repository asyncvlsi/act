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
#include <mstring.h>
#include <list.h>
#include <array.h>

class ActId;

/*
  Attributes
*/
struct act_attr {
  const char *attr;
  Expr *e;
  struct act_attr *next;
};

typedef struct act_attr act_attr_t;


/*------------------------------------------------------------------------
 *
 *  The PRS sublanguage: gate-level abstraction for the circuit
 *
 *------------------------------------------------------------------------
 */
enum act_prs_expr_type {
  ACT_PRS_EXPR_AND = 0,
  ACT_PRS_EXPR_OR  = 1,
  ACT_PRS_EXPR_VAR = 2,
  ACT_PRS_EXPR_NOT = 3,
  ACT_PRS_EXPR_LABEL = 4,
  ACT_PRS_EXPR_ANDLOOP = 5,
  ACT_PRS_EXPR_ORLOOP = 6,

  ACT_PRS_EXPR_TRUE = 7,
  ACT_PRS_EXPR_FALSE = 8
};


typedef struct {
  Expr *w, *l;		/* size, if any */
  int flavor;		/* type of transistor */
  Expr *folds;		/* folding, if any */
} act_size_spec_t;


typedef struct act_prs_expr {
  unsigned int type:4;
  union {
    struct {
      struct act_prs_expr *l, *r;
      struct act_prs_expr *pchg; /* precharge, for an AND */
      int pchg_type;		/* 1 for +, 0 for - */
    } e;			/* expression */
    struct {
      ActId *id;		/* variable */
      act_size_spec_t *sz;	/* size, if any */
    } v;
    struct {
      char *label;
    } l;
    struct {
      char *id;			/* loop variable */
      Expr *lo, *hi;
      struct act_prs_expr *e;
    } loop;
  } u;
} act_prs_expr_t;

enum act_prs_lang_type {
  ACT_PRS_RULE = 0,
  ACT_PRS_GATE = 1,
  ACT_PRS_LOOP = 2,
  ACT_PRS_TREE = 3,
  ACT_PRS_SUBCKT = 4
};

typedef struct act_prs_lang {
  struct act_prs_lang *next;
  unsigned int type:3;
  union {
    struct {
      act_attr_t *attr;  /* after, weak, and unstab need u/d versions */
      act_prs_expr_t *e;
      ActId *id; /* is a char * if it is a label */
      unsigned int arrow_type:2; /* -> = 0, => = 1, #> = 2 */
      unsigned int dir:1;	/* 0 = -, 1 = + */
      unsigned int label:1;	/* 1 if it is a label, 0 otherwise */
    } one;			/* one prs */
    struct {
      act_attr_t *attr;
      ActId *g, *s, *d, *_g;
      act_size_spec_t *sz;
    } p;			/* pass gate: use g for n-type
				   use _g for p-type (full uses both)
				*/
    struct {
      const char *id;		/* loop id */
      Expr *lo, *hi;		/* range */
      struct act_prs_lang *p;	/* body */
    } l;			/* loop */
    /* l gets re-used for "tree"... 
        The p field is used for the prs
	The lo field is used for the expr
       l gets re-used for "subckt"...
        The p field is used for the prs
	The id field is used for the name of the subckt
	For loops, lo == NULL if it's 0..hi-1. Otherwise it is lo..hi
	
     */
  } u;
} act_prs_lang_t;

struct act_prs {
  ActId *vdd, *gnd, *psc, *nsc;
  act_prs_lang_t *p;
  struct act_prs *next;
};



/*------------------------------------------------------------------------
 *
 *  The CHP sublanguage: behavioral description
 *
 *
 *------------------------------------------------------------------------
 */

enum act_chp_lang_type {
  ACT_CHP_COMMA = 0,
  ACT_CHP_SEMI = 1,
  ACT_CHP_SELECT = 2,
  ACT_CHP_SELECT_NONDET = 3,
  ACT_CHP_LOOP = 4,
  ACT_CHP_DOLOOP = 5,
  ACT_CHP_SKIP = 6,
  ACT_CHP_ASSIGN = 7,
  ACT_CHP_SEND = 8,
  ACT_CHP_RECV = 9,
  ACT_CHP_FUNC = 10,
  ACT_CHP_SEMILOOP = 11,
  ACT_CHP_COMMALOOP = 12,
  ACT_CHP_HOLE = 13
};

#define ACT_CHP_STMTEND 14  /* 1 more than the last enumeration above */

struct act_chp_lang;

typedef struct act_chp_gc {
  const char *id;
  Expr *lo, *hi;
  Expr *g;			/* guard */
  struct act_chp_lang *s;	/* statement */
  struct act_chp_gc *next;
} act_chp_gc_t ;

typedef struct act_chp_lang {
  int type;
  const char *label;		// label for this item, normally NULL
  void *space;			// this space for rent!
  union {
    struct {
      ActId *id;
      Expr *e;
    } assign;
    struct {
      ActId *chan;
      list_t *rhs;		/* expression for send, id for recv.
				   for bidirectional channels, send
				   has expr followed by id, and recv
				   has id followed by expr
				*/
      int flavor;		/* up, down, blank */
    } comm;
    struct {
      mstring_t *name;		/* function name */
      list_t *rhs;		/* arguments */
    } func;
    struct {
      list_t *cmd;
    } semi_comma;

    act_chp_gc_t *gc;			/* loop or select;
					   also used for a do-loop,
					   where there is exactly one gc
					 */
    struct {
      const char *id;
      Expr *lo, *hi;
      struct act_chp_lang *body;
    } loop;			/* syntactic replication */
    
  } u;
} act_chp_lang_t;

typedef struct act_func_arguments {
  unsigned int isstring:1;	/* true if string, false otherwise */
  union {
    mstring_t *s;
    Expr *e;
  } u;
} act_func_arguments_t;

struct act_chp {
  ActId *vdd, *gnd, *psc, *nsc;
  act_chp_lang_t *c;
  int is_synthesizable;
  //struct act_chp *next;
};




/*------------------------------------------------------------------------
 *
 *  The specification sublanguage: used for verification and asserting
 *  invariants; also used to specify arbiters
 *
 *
 *------------------------------------------------------------------------
 */

struct act_spec {
  int isrequires;		/* 1 if this is a requires clause */
  
  int type;    /* -1 = timing. in this case, 
		  directive: timing lhs : first < [expr] second
		  (a) count = 4
		  (b) ids[0] = lhs, ids[1] = first, ids[2] = second
		  (c) ids[3] = NULL or an Expr * for the timing margin
		*/
  int count;
  ActId **ids;
  int *extra;  /*  Flags

		   0x03 : 0 = no dir, 1 = +, 2 = -
		   0x04 : 1 = ?, 0 = nothing
		   0x08 : 1 = +1, 0 = current iteration. Can only be
 		          set for ids[1] and ids[2].
	       */
  struct act_spec *next;
};

class ActBody;


/*------------------------------------------------------------------------
 *
 *  The refinement sublanguage
 *
 *------------------------------------------------------------------------
 */
struct act_refine {
  ActBody *b;
};


/*------------------------------------------------------------------------
 *
 *  The gate sizing sublanguage
 *
 *
 *------------------------------------------------------------------------
 */
struct act_sizing_directive {
  ActId *id;
  int flav_up, flav_dn;
  Expr *eup, *edn;
  Expr *upfolds, *dnfolds;

  /*-- sizing loop: NOTE: loop_id == NULL meanas the rest of the
       fields are garbage  --*/
  const char *loop_id;
  Expr *lo, *hi;
  A_DECL (act_sizing_directive, d);
};
  
struct act_sizing {
  // p_n_mode  0/1  0 = default, 1 = sqrt sizing
  // unit_n 5
  int p_specified, unit_n_specified;
  Expr *p_n_mode_e, *unit_n_e;
  int p_n_mode;
  int unit_n;
  A_DECL (act_sizing_directive, d);
  act_sizing *next;
};


/*------------------------------------------------------------------------
 *
 *  The initialization sublanguage, used to specify the initial state
 *  and reset protocol.
 *
 *
 *------------------------------------------------------------------------
 */
struct act_initialize {
  list_t *actions;
  act_initialize *next;
};


/*------------------------------------------------------------------------
 *
 * The dataflow sublanguage
 *
 *
 *------------------------------------------------------------------------
 */
enum act_dataflow_element_types {
 ACT_DFLOW_FUNC = 0,
 ACT_DFLOW_SPLIT = 1,
 ACT_DFLOW_MERGE = 2,
 ACT_DFLOW_MIXER = 3,
 ACT_DFLOW_ARBITER = 4
};

typedef struct  {
  act_dataflow_element_types t;
  union {
    struct {
      Expr *lhs;		// expression
      ActId *rhs;		// channel output
      Expr *nbufs;		// # of buffers
      int istransparent:1;	// transparent v/s opaque
      Expr *init;		// initial token, if any on the output
    } func;
    struct {
      ActId *guard;		// condition
      ActId **multi;		// set of channels
      int nmulti;
      ActId *single;		// single channel
      ActId *nondetctrl;	// channel for non-deterministic
				// control out
    } splitmerge;
  } u;
} act_dataflow_element;

struct act_dataflow {
  list_t *dflow;
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

class ActNamespace;
class Scope;

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

  /*
    Return 1 if the language block has some circuit description
    language 
  */
  int hasCktLang () {
    if (chp || hse || prs || dflow) return 1;
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

act_chp_lang_t *chp_expand (act_chp_lang_t *, ActNamespace *, Scope *);
void act_print_size (FILE *fp, act_size_spec_t *sz);


act_prs_expr_t *act_prs_complement_rule (act_prs_expr_t *e);
act_prs_expr_t *act_prs_celement_rule (act_prs_expr_t *e);


typedef void *(*ACT_VAR_CONV) (void *, void *);

/*
  at_hash : hash table from label strings to act_prs_lang_t pointers
*/
act_prs_expr_t *act_prs_expr_nnf (void *cookie,
				  struct Hashtable *at_hash,
				  act_prs_expr_t *e,
				  ACT_VAR_CONV conv_var);

void act_prs_expr_free (act_prs_expr_t *e);
void act_chp_free (act_chp_lang_t *);

ActId *expand_var_write (ActId *id, ActNamespace *ns, Scope *s);

int act_hse_direction (act_chp_lang_t *, ActId *);


#endif /* __LANG_H__ */
