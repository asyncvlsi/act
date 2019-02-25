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
      act_attr_t *attr;
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


/**
 * CHP
 */
enum act_chp_lang_type {
  ACT_CHP_COMMA, ACT_CHP_SEMI, ACT_CHP_SELECT, ACT_CHP_LOOP, ACT_CHP_SKIP,
  ACT_CHP_ASSIGN, ACT_CHP_SEND, ACT_CHP_RECV, ACT_CHP_FUNC
};

struct act_chp_lang;

typedef struct act_chp_gc {
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
  int type;  /* -1 = timing */
  int count;
  ActId **ids;
  int *extra;
  struct act_spec *next;
};

class ActNamespace;
class Scope;

act_chp *chp_expand (act_chp *, ActNamespace *, Scope *);
act_chp_lang_t *chp_expand (act_chp_lang_t *, ActNamespace *, Scope *);
act_prs_lang_t *prs_expand (act_prs_lang_t *, ActNamespace *, Scope *);
act_prs *prs_expand (act_prs *, ActNamespace *, Scope *);
act_spec *spec_expand (act_spec *, ActNamespace *, Scope *);
void prs_print (FILE *, act_prs *);

const char *act_spec_string (int type);

act_attr_t *inst_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s);

#endif /* __LANG_H__ */
