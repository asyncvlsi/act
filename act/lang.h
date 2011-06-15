/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __LANG_H__
#define __LANG_H__

#include "expr.h"

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
  ACT_PRS_EXPR_ORLOOP = 6
};


#undef FET_TOKEN
#define FET_TOKEN(a,b)  b,

enum act_transistor_flavors {
#include "fets.def"
  ACT_FET_END
};

typedef struct {
  Expr *w, *l;		/* size, if any */
  enum act_transistor_flavors flavor;
  int subflavor;		/* special flavor override (-1 if missing) */
} act_size_spec_t;


typedef struct act_prs_expr {
  unsigned int type:3;
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
};


/**
 * XXX: Handshaking expansions
 */
struct act_hse_lang;


#include <act/types.h>

/*
 * Language body
 */
class ActBody_Lang : public ActBody {
 public:
  ActBody_Lang (act_prs *p) {
    t = LANG_PRS;
    lang = p;
  }
 private:
  enum {
    LANG_PRS
  } t;
  void *lang;
};



#endif /* __LANG_H__ */

