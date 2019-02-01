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
#include <stdio.h>
#include "act_walk_X.h"
#include <act/lang.h>
#include "config.h"

static char **fet_flavors = NULL;
static int num_fets = -1;
int act_fet_string_to_value (const char *s)
{
  if (num_fets == -1) {
    num_fets = config_get_table_size ("act.fet_flavors");
    fet_flavors = config_get_table_string ("act.fet_flavors");
  }
  for (int i=0; i < num_fets; i++) {
    if (strcmp (s, fet_flavors[i]) == 0)
      return i;
  }
  return -1;
}

const char *act_fet_value_to_string (int f)
{
  if (num_fets == -1) {
    num_fets = config_get_table_size ("act.fet_flavors");
    fet_flavors = config_get_table_string ("act.fet_flavors");
  }
  if (f < 0 || f >= num_fets) {
    return NULL;
  }
  return fet_flavors[f];
}

#define NULL_WRAP(nm,t) ActRet *act_wrap_X_##nm (t v) { return NULL; }

NULL_WRAP(double,double)
NULL_WRAP(int,int)
NULL_WRAP(extern, void *)
NULL_WRAP(ActNamespace_p, ActNamespace *)
NULL_WRAP(Type_p, Type *)
NULL_WRAP(UserDef_p, UserDef *)
NULL_WRAP(ActBody_Select_p, ActBody_Select *)
NULL_WRAP(ActBody_Select_gc_p, ActBody_Select_gc *)
NULL_WRAP(act_size_spec_t_p, act_size_spec_t *)

/*
  nm = mangled type name used by pgen
  t = actual type
  suffix = name of the field in the ActRet union
  kind = R_<kind> enumeration value

  Creates a generic wrapper that uses the union defined in
  treetypes.h to create a pointer that is returned.
*/
#define GENERIC_WRAP(nm,t,suffix,kind)		\
  ActRet *act_wrap_X_##nm(t v) {		\
  ActRet *r;					\
  NEW (r, ActRet);				\
  r->type = (kind);				\
  r->u.suffix = v;				\
  return r;					\
  }

GENERIC_WRAP(string,const char *,str,R_STRING)
GENERIC_WRAP(list,list_t *,l,R_LIST)
GENERIC_WRAP(Type__direction,Type::direction,dir,R_DIR)
GENERIC_WRAP(Expr_p, Expr *, exp, R_EXPR)
GENERIC_WRAP(AExpr_p, AExpr *, ae, R_AEXPR)
GENERIC_WRAP(Array_p, Array *, array, R_ARRAY)
GENERIC_WRAP(ActId_p, ActId *, id, R_ID)
GENERIC_WRAP(act_attr_t_p, act_attr_t *, attr, R_ATTR)
GENERIC_WRAP(act_prs_lang_t_p, act_prs_lang_t *, prs, R_PRS_LANG)
GENERIC_WRAP(act_chp_lang_t_p, act_chp_lang_t *, chp, R_CHP_LANG)
GENERIC_WRAP(act_spec_p, act_spec *, spec, R_SPEC_LANG)
GENERIC_WRAP(act_chp_gc_t_p, act_chp_gc *, gc, R_CHP_GC)
GENERIC_WRAP(act_func_arguments_t_p, act_func_arguments_t *, func, R_CHP_FUNC)
GENERIC_WRAP(InstType_p, InstType *, inst, R_INST_TYPE)
GENERIC_WRAP(ActBody_p, ActBody *,body,R_ACT_BODY)

/* special case for built-in expression type wrapping */
ActRet *act_wrap_X_expr (Expr *e)
{
  return act_wrap_X_Expr_p (e);
}


/*------------------------------------------------------------------------
 *
 * Helper functions
 *
 *------------------------------------------------------------------------
 */


/* return a constant expression with specified value */
Expr *const_expr (int val)
{
  Expr *e, *f;

  NEW (e, Expr);
  e->type = E_INT;
  e->u.v = val;

  f = TypeFactory::NewExpr (e);
  FREE (e);
  
  return f;
}

/**
 * Print out a list of strings to the specified file, separated by
 * "::". Used to display a namespace name.
 *
 * \param fp is the FILE to which this should be printed
 * \param l is a list of strings (char *)
 *
 */
void print_ns_string (FILE *fp, list_t *l)
{
  listitem_t *li;

  for (li = list_first (l); li; li = list_next (li)) {
    if (li != list_first (l)) {
      fprintf (fp, "::");
    }
    fprintf (fp, "%s", (char *)list_value (li));
  }
  return;
}


/* external */
ActId *act_walk_X_expr_id (ActTree *cookie, pId *n);

/*------------------------------------------------------------------------
 *
 * Expression walk
 *
 *------------------------------------------------------------------------
 */
Expr *act_walk_X_expr (ActTree *cookie, Expr *e)
{
  ActId *id;
  Expr *ret;

  if (!e) return e;

  NEW (ret, Expr);
  ret->type = e->type;
  ret->u.e.l = NULL;
  ret->u.e.r = NULL;
  switch (e->type) {
#define BINOP						\
    ret->u.e.l = act_walk_X_expr (cookie, e->u.e.l);	\
    ret->u.e.r = act_walk_X_expr (cookie, e->u.e.r);

#define UOP						\
    ret->u.e.l = act_walk_X_expr (cookie, e->u.e.l);

  case E_AND:  BINOP; break;
  case E_OR:   BINOP; break;
  case E_PLUS: BINOP; break;
  case E_MINUS:BINOP; break;
  case E_MULT: BINOP; break;
  case E_DIV:  BINOP; break;
  case E_MOD:  BINOP; break;
  case E_LSL:  BINOP; break;
  case E_LSR:  BINOP; break;
  case E_ASR:  BINOP; break;
  case E_XOR:  BINOP; break;
  case E_LT:   BINOP; break;
  case E_GT:   BINOP; break;
  case E_LE:   BINOP; break;
  case E_GE:   BINOP; break;
  case E_EQ:   BINOP; break;
  case E_NE:   BINOP; break;
  case E_NOT:  UOP; break;
  case E_COMPLEMENT: UOP; break;
  case E_UMINUS:     UOP; break;

  case E_INT:
    ret->u.v = e->u.v;
    break;

  case E_REAL:
    ret->u.f = e->u.f;
    break;

  case E_PROBE:
  case E_VAR:
    ret->u.e.l = (Expr *) act_walk_X_expr_id (cookie, (pId *)e->u.e.l);

    /* we need to replace keywords */
    id = (ActId *)ret->u.e.l;
    if (strcmp (id->getName(), "true") == 0 && id->arrayInfo() == NULL &&
	id->Rest() == NULL) {
      delete id;
      ret->type = E_TRUE;
    }
    else if (strcmp (id->getName(), "false") == 0 && id->arrayInfo() == NULL &&
	     id->Rest() == NULL) {
      delete id;
      ret->type = E_FALSE;
    }
    else if (strcmp (id->getName(), "self") == 0 && id->arrayInfo() == NULL &&
	     id->Rest() == NULL) {
      delete id;
      ret->type = E_SELF;
    }
    break;

  case E_TRUE: break;
  case E_FALSE:break;

  case E_QUERY:
    ret->u.e.l = act_walk_X_expr (cookie, e->u.e.l);
    ret->u.e.r->u.e.l = act_walk_X_expr (cookie, e->u.e.r->u.e.l);
    ret->u.e.r->u.e.r = act_walk_X_expr (cookie, e->u.e.r->u.e.r);
    break;

  case E_BITFIELD:
    ret->u.e.l = (Expr *) act_walk_X_expr_id (cookie, (pId *)e->u.e.l);
    /* e->u.e.r is the bitfield... const expression */
    break;

  case E_FUNCTION:
    ret->u.fn.s = Strdup (e->u.fn.s);
    ret->u.fn.r = (Expr *) act_walk_X_expr (cookie, e->u.fn.r);
    break;
    
  default:
    fatal_error ("-- unknown expression type --");
    break;
  }
  return ret;
}



