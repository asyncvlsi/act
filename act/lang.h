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

/*------------------------------------------------------------------------
 *
 *  PRS language
 *
 *------------------------------------------------------------------------
 */

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
  int flavor;
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
      act_prs_expr_t *e, *eopp;
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


/**
 * CHP
 */
enum act_chp_lang_type {
  ACT_CHP_COMMA, ACT_CHP_SEMI, ACT_CHP_SELECT, ACT_CHP_LOOP, ACT_CHP_SKIP,
  ACT_CHP_ASSIGN, ACT_CHP_SEND, ACT_CHP_RECV, ACT_CHP_FUNC
};

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
  union {
    struct {
      ActId *id;
      Expr *e;
    } assign;
    struct {
      ActId *chan;
      list_t *rhs;		/* expression list or ID list for
				   send vs recv */
    } comm;
    struct {
      mstring_t *name;		/* function name */
      list_t *rhs;		/* arguments */
    } func;
    struct {
      list_t *cmd;
    } semi_comma;
    act_chp_gc_t *gc;			/* loop or select */
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
};


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
	       */
  struct act_spec *next;
};

class ActBody;

struct act_refine {
  ActBody *b;
};

void prs_print (FILE *, act_prs *);
void chp_print (FILE *, act_chp *);
void chp_print (FILE *fp, act_chp_lang_t *c);
void hse_print (FILE *, act_chp *);
void spec_print (FILE *, act_spec *);
void refine_print (FILE *, act_refine *);

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
  }
  void Print (FILE *fp) {
    if (chp) { chp_print (fp, chp); }
    if (hse) { hse_print (fp, hse); }
    if (prs) { prs_print (fp, prs); }
    if (spec) { spec_print (fp, spec); }
    if (refine) { }
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

  act_languages *Expand (ActNamespace *ns, Scope *s);

 private:
  act_chp *chp, *hse;
  act_prs *prs;
  act_spec *spec;
  act_refine *refine;
};


act_chp *chp_expand (act_chp *, ActNamespace *, Scope *);
act_prs *prs_expand (act_prs *, ActNamespace *, Scope *);
act_spec *spec_expand (act_spec *, ActNamespace *, Scope *);
void refine_expand (act_refine *, ActNamespace *, Scope *);

const char *act_spec_string (int type);
const char *act_dev_value_to_string (int);
int act_dev_string_to_value (const char *s);

act_attr_t *inst_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s);

act_chp_lang_t *chp_expand (act_chp_lang_t *, ActNamespace *, Scope *);
void act_print_size (FILE *fp, act_size_spec_t *sz);

#endif /* __LANG_H__ */
