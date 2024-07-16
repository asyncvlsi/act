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
#include "act_parse_int.h"
#include <act/lang.h>
#include <act/act.h>
#include <common/config.h>
#include <string.h>

static char **dev_flavors = NULL;
static int num_devs = -1;

int act_dev_string_to_value (const char *s)
{
  if (num_devs == -1) {
    num_devs = config_get_table_size ("act.dev_flavors");
    dev_flavors = config_get_table_string ("act.dev_flavors");
  }
  for (int i=0; i < num_devs; i++) {
    if (strcmp (s, dev_flavors[i]) == 0)
      return i;
  }
  return -1;
}

const char *act_dev_value_to_string (int f)
{
  if (num_devs == -1) {
    num_devs = config_get_table_size ("act.dev_flavors");
    dev_flavors = config_get_table_string ("act.dev_flavors");
  }
  if (f < 0 || f >= num_devs) {
    return NULL;
  }
  return dev_flavors[f];
}

#define NULL_WRAP(nm,t) ActRet *act_wrap_X_##nm (t v) { return NULL; }

NULL_WRAP(double,double)
//NULL_WRAP(int,int)
NULL_WRAP(extern, void *)
NULL_WRAP(ActNamespace_p, ActNamespace *)
NULL_WRAP(Type_p, Type *)
NULL_WRAP(UserDef_p, UserDef *)
NULL_WRAP(ActBody_Select_p, ActBody_Select *)
NULL_WRAP(ActBody_Select_gc_p, ActBody_Select_gc *)
NULL_WRAP(act_size_spec_t_p, act_size_spec_t *)
NULL_WRAP(act_refine_p, act_refine *)
NULL_WRAP(inst_param_p, inst_param *);
NULL_WRAP(act_dataflow_element_p, act_dataflow_element *);
NULL_WRAP(act_dataflow_order_p, act_dataflow_order *);
NULL_WRAP(act_dataflow_loop_p, act_dataflow_loop *);
NULL_WRAP(void_p, void *);

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

GENERIC_WRAP(int,int,ival,R_INT)
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
Expr *const_expr (long val)
{
  Expr *e, *f;

  NEW (e, Expr);
  e->type = E_INT;
  e->u.ival.v = val;
  e->u.ival.v_extra = NULL;

  f = TypeFactory::NewExpr (e);
  FREE (e);
  
  return f;
}

Expr *const_expr_bool (int val)
{
  Expr *e, *f;

  NEW (e, Expr);
  e->type = (val ? E_TRUE : E_FALSE);

  f = TypeFactory::NewExpr (e);
  FREE (e);
  
  return f;
}

Expr *const_expr_real (double d)
{
  Expr *e;
  NEW (e, Expr);
  e->type = E_REAL;
  e->u.f = d;
  return e;
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


int _act_id_is_true_false (ActId *id)
{
  if (!id) return 0;
  if (id->Rest()) return 0;
  if (id->arrayInfo()) return 0;
  if (strcmp (id->getName(), "true") == 0 ||
      strcmp (id->getName(), "false") == 0) return 1;
  return 0;
}

int _act_id_is_enum_const (ActOpen *os, ActNamespace *ns, ActId *id)
{
  UserDef *u;
  if (!id) return 0;
  if (!id->Rest()) return 0;
  if (id->Rest()->Rest()) return 0;
  if (id->arrayInfo() || id->Rest()->arrayInfo()) return 0;
  ns = os->findType (ns, id->getName());
  if (!ns) {
    return 0;
  }
  u = ns->findType (id->getName());
  Assert (u, "What?");
  if (TypeFactory::isUserEnum (u)) {
    return 1;
  }
  return 0;
}

Data *_act_id_to_enum_type (ActOpen *os, ActNamespace *ns, ActId *id)
{
  Data *d;
  // we can assume that it is an enumeration
  ns = os->findType (ns, id->getName());
  d = dynamic_cast <Data *> (ns->findType (id->getName()));
  Assert (d, "What?");
  if (d->enumVal (id->Rest()->getName()) == -1) {
    return NULL;
  }
  return d;
}

/* external */
ActId *act_walk_X_expr_id (ActTree *cookie, pId *n);

static int find_colon (char *s)
{
  int i = 0;
  while (s[i]) {
    if (s[i] == ':')
      return i;
    i++;
  }
  return -1;
}

static Expr *walk_fn_args (ActTree *cookie, Expr *e, int *nargs)
{
  Expr *rtmp;
  Expr *etmp;
  int args = 0;
  Expr *ret = NULL;

  rtmp = ret;
  etmp = e;
  while (etmp) {
    args++;
    if (ret == NULL) {
      NEW (ret, Expr);
      rtmp = ret;
    }
    else {
      NEW (rtmp->u.e.r, Expr);
      rtmp = rtmp->u.e.r;
    }
    rtmp->type = etmp->type;
    rtmp->u.e.r = NULL;
    rtmp->u.e.l = act_walk_X_expr (cookie, etmp->u.e.l);
    etmp = etmp->u.e.r;
  }
  *nargs = args;
  return ret;
}

static int _single_real_arg (Expr *args)
{
  while (args && args->type == E_GT) {
    args = args->u.e.r;
  }
  if (args && args->u.e.r == NULL) {
    return 1;
  }
  return 0;
}

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

  case E_ANDLOOP:
  case E_ORLOOP:
  case E_PLUSLOOP:
  case E_MULTLOOP:
  case E_XORLOOP:
    if (cookie->scope->Lookup ((char *)e->u.e.l->u.e.l)) {
      struct act_position p;
      p.l = cookie->line;
      p.c = cookie->column;
      p.f = cookie->file;
      act_parse_err (&p, "Identifier `%s' already exists in current scope",
		     (char *)e->u.e.l->u.e.l);
    }
    cookie->scope->Add ((char *)e->u.e.l->u.e.l, cookie->tf->NewPInt());
    NEW (ret->u.e.l, Expr);
    ret->u.e.l->type = e->u.e.l->type;
    ret->u.e.l->u.e.l = (Expr *)Strdup ((char *)e->u.e.l->u.e.l);
    ret->u.e.l->u.e.r = NULL;
    NEW (ret->u.e.r, Expr);
    ret->u.e.r->u.e.l = act_walk_X_expr (cookie, e->u.e.r->u.e.l);
    NEW (ret->u.e.r->u.e.r, Expr);
    ret->u.e.r->u.e.r->u.e.l = act_walk_X_expr (cookie, e->u.e.r->u.e.r->u.e.l);
    ret->u.e.r->u.e.r->u.e.r = act_walk_X_expr (cookie, e->u.e.r->u.e.r->u.e.r);
    cookie->scope->Del ((char *)e->u.e.l->u.e.l);
    break;

  case E_INT:
    ret->u.ival.v = e->u.ival.v;
    ret->u.ival.v_extra = NULL;
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
    else if (strcmp (id->getName(), "selfack") == 0 && id->arrayInfo() == NULL
	     && id->Rest() == NULL) {
      delete id;
      ret->type = E_SELF_ACK;
    }
    else if (ret->type == E_VAR &&
	     _act_id_is_enum_const (cookie->os, cookie->curns, id)) {
      ret->u.fn.s = (char *) _act_id_to_enum_type (cookie->os, cookie->curns, id);
      if (!ret->u.fn.s) {
	struct act_position p;
	p.l = cookie->line;
	p.c = cookie->column;
	p.f = cookie->file;
	act_parse_err (&p, "Name `%s' is not part of the enumeration type `%s'",
		       id->Rest()->getName(), id->getName());
      }
      ret->u.fn.r = (Expr *) id->Tail()->getName();
      delete id;
      ret->type = E_ENUM_CONST;
    }
    break;

  case E_TRUE: break;
  case E_FALSE:break;

  case E_QUERY:
    ret->u.e.l = act_walk_X_expr (cookie, e->u.e.l);
    NEW (ret->u.e.r, Expr);
    ret->u.e.r->type = e->u.e.r->type;
    ret->u.e.r->u.e.l = act_walk_X_expr (cookie, e->u.e.r->u.e.l);
    ret->u.e.r->u.e.r = act_walk_X_expr (cookie, e->u.e.r->u.e.r);
    break;

  case E_BITFIELD:
    ret->u.e.l = (Expr *) act_walk_X_expr_id (cookie, (pId *)e->u.e.l);
    NEW (ret->u.e.r, Expr);
    ret->u.e.r->type = E_BITFIELD;
    ret->u.e.r->u.e.l = act_walk_X_expr (cookie, e->u.e.r->u.e.l);
    ret->u.e.r->u.e.r = act_walk_X_expr (cookie, e->u.e.r->u.e.r);
    break;
    
  case E_CONCAT:
    {
      Expr *tmp;
      tmp = ret;
      while (e) {
	tmp->u.e.l = act_walk_X_expr (cookie, e->u.e.l);
	if (e->u.e.r) {
	  NEW (tmp->u.e.r, Expr);
	  tmp = tmp->u.e.r;
	  tmp->type = E_CONCAT;
	  tmp->u.e.l = NULL;
	  tmp->u.e.r = NULL;
	}
	e = e->u.e.r;
      }
    }
    break;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    ret->u.e.l = act_walk_X_expr (cookie, e->u.e.l);
    if (e->u.e.r) {
      ret->u.e.r = act_walk_X_expr (cookie, e->u.e.r);
    }
    else {
      ret->u.e.r = NULL;
      if (e->type == E_BUILTIN_INT) {
	UserDef *ux = NULL;
	struct act_position p;
	p.l = cookie->line;
	p.c = cookie->column;
	p.f = cookie->file;
	if (ret->u.e.l->type == E_VAR) {
	  // check if this is a structure!
	  ActId *id = (ActId *)ret->u.e.l->u.e.l;
	  Array *aref = NULL;
	  InstType *it = cookie->scope->FullLookup (id, &aref);

	  if (it) {
	    if (TypeFactory::isPureStruct (it)) {
	      if (it->arrayInfo () && (!aref || !aref->isDeref())) {
		act_parse_err (&p, "int(.) operator applied to an array!");
	      }
	      ux = dynamic_cast<UserDef *> (it->BaseType());
	    }
	  }
	}
	else if (ret->u.e.l->type == E_FUNCTION) {
	  Function *f = (Function *)ret->u.e.l->u.fn.s;
	  // check if the function returns a structure!
	  if (TypeFactory::isPureStruct (f->getRetType())) {
	    if (f->getRetType()->arrayInfo()) {
	      act_parse_err (&p, "int(.) operator applied to an array!");
	    }
	    ux = dynamic_cast<UserDef *> (f->getRetType()->BaseType());
	  }
	}
	if (ux) {
	  /* substitute this with a special macro expression! */
	  UserMacro *um;
	  um = ux->getMacro ("int");
	  if (!um) {
	    um = ux->newMacro (string_cache ("int"));
	    um->mkBuiltin ();
	    um->setRetType
	      (cookie->tf->NewInt (cookie->scope,
				   Type::direction::NONE,
				   0,
				   const_expr (32)));
	  }
	  ret->type = E_USERMACRO;
	  ret->u.fn.r = ret->u.e.l;
	  ret->u.fn.s = (char *) um;
	}
      }
    }
    break;

  case E_FUNCTION:
  case E_ENUM_CONST:
    /* we lookup the function or enumeration const in scope! */
    {
      ActNamespace *ns;
      char *prev;
      int i;
      int args = 0;
      int args2 = 0;

      i = find_colon (e->u.fn.s);
      if (i == -1) {
	ns = cookie->curns;
	i = 0;
      }
      else if (i == 0) {
	ns = ActNamespace::Global();
	i += 2;
      }
      else {
	e->u.fn.s[i] = '\0';
	ns = cookie->os->find (cookie->curns, e->u.fn.s);
	if (!ns) {
	  struct act_position p;
	  p.l = cookie->line;
	  p.c = cookie->column;
	  p.f = cookie->file;
	  act_parse_err (&p, "Could not find namespace `%s'", e->u.fn.s);
	}
	e->u.fn.s[i] = ':';
	i += 2;
      }
      prev = e->u.fn.s + i;
      while ((i = find_colon (prev)) != -1) {
	ActNamespace *tmp;
	prev[i] = '\0';
	tmp = ns->findNS (prev);
	prev[i] = ':';
	if (!tmp) {
	  struct act_position p;
	  p.l = cookie->line;
	  p.c = cookie->column;
	  p.f = cookie->file;
	  act_parse_err (&p, "Could not find `%s' in namespace `%s'", prev,
			 ns->Name());
	}
	ns = tmp;
	prev = prev + i + 2;
      }
      UserDef *u = ns->findType (prev);
      if (!u) {
	  struct act_position p;
	  p.l = cookie->line;
	  p.c = cookie->column;
	  p.f = cookie->file;
	  act_parse_err (&p, "Could not find `%s' in namespace `%s'", prev,
			 ns->Name());
      }
      if (TypeFactory::isUserEnum (u)) {
	ret->u.fn.s = (char *) u;
	ret->u.fn.r = e->u.fn.r;
	// ... and we are done
	return ret;
      } else if (!TypeFactory::isFuncType (u)) {
	struct act_position p;
	p.l = cookie->line;
	p.c = cookie->column;
	p.f = cookie->file;
	if (TypeFactory::isPureStruct (u)) {
	  if (!e->u.fn.r || !_single_real_arg (e->u.fn.r)) {
	    act_parse_err (&p, "`%s': built-in conversion from int to pure structure requires exactly one argument!", e->u.fn.s);
	  }
	  UserMacro *um;
	  um = u->getMacro (u->getName());
	  if (!um) {
	    um = u->newMacro (u->getName());
	    um->mkBuiltin ();
	    um->setRetType (new InstType (cookie->scope, u, 0));
	  }
	  ret->type = E_USERMACRO;
	  ret->u.fn.s = (char *) um;
	}
	else {
	  act_parse_err (&p, "`%s' is not a function type", e->u.fn.s);
	}
      }
      if (ret->type != E_USERMACRO) {
	ret->u.fn.s = (char *) u;
      }
      int is_templ;

      if (e->u.fn.r && e->u.fn.r->type == E_GT) {
	NEW (ret->u.fn.r, Expr);
	ret->u.fn.r->type = E_GT;
	ret->u.fn.r->u.e.l = walk_fn_args (cookie, e->u.fn.r->u.e.l, &args);
	ret->u.fn.r->u.e.r = walk_fn_args (cookie, e->u.fn.r->u.e.r, &args2);
	is_templ = 1;
      }
      else {
	ret->u.fn.r = walk_fn_args (cookie, e->u.fn.r, &args);
	args2 = 0;
	is_templ = 0;
      }

      if (ret->type == E_USERMACRO) {
	// no more checking
      }
      else {
	Function *uf = dynamic_cast<Function *>(u);
	Assert (uf, "What?");

	if (TypeFactory::isParamType (uf->getRetType())) {
	  if (is_templ) {
	    struct act_position p;
	    p.l = cookie->line;
	    p.c = cookie->column;
	    p.f = cookie->file;
	    act_parse_err (&p, "`%s': template parameters for non-templated function", e->u.fn.s);
	  }
	}
	else {
	  if (u->getNumParams() > 0) {
	    if (!is_templ) {
	      struct act_position p;
	      p.l = cookie->line;
	      p.c = cookie->column;
	      p.f = cookie->file;
	      act_parse_err (&p, "`%s': missing template parameters", e->u.fn.s);
	    }
	  }
	  else if (is_templ) {
	    struct act_position p;
	    p.l = cookie->line;
	    p.c = cookie->column;
	    p.f = cookie->file;
	    act_parse_err (&p, "`%s': template parameters for non-templated function", e->u.fn.s);
	  }
	}
      
	if (is_templ) {
	  if (args != u->getNumParams()) {
	    struct act_position p;
	    p.l = cookie->line;
	    p.c = cookie->column;
	    p.f = cookie->file;
	    act_parse_err (&p, "`%s': incorrect number of template arguments (expected %d, got %d)", e->u.fn.s, u->getNumParams(), args);
	  }
	  if (args2 != u->getNumPorts()) {
	    struct act_position p;
	    p.l = cookie->line;
	    p.c = cookie->column;
	    p.f = cookie->file;
	    act_parse_err (&p, "`%s': incorrect number of parameter arguments (expected %d, got %d)", e->u.fn.s, u->getNumPorts(), args2);
	  }
	}
	else {
	  if (args != (u->getNumParams() + u->getNumPorts())) {
	    struct act_position p;
	    p.l = cookie->line;
	    p.c = cookie->column;
	    p.f = cookie->file;
	    act_parse_err (&p, "`%s': incorrect number of arguments (expected %d, got %d)", e->u.fn.s, u->getNumPorts() + u->getNumParams(), args);
	  }
	}
      }
    }
    break;

  case E_USERMACRO:
    {
      int special_id;
      int args = 0;
      struct act_position p;
      UserMacro *um;
      ActId *tmp, *prev;
      InstType *it;
      UserDef *u;
      Scope *sc;
      p.l = cookie->line;
      p.c = cookie->column;
      p.f = cookie->file;
      special_id = cookie->special_id;
      cookie->special_id = 1;
      tmp = act_walk_X_expr_id (cookie, (pId *) e->u.fn.s);
      cookie->special_id = special_id;

      ret->u.fn.s = NULL;
      NEW (ret->u.fn.r, Expr);
      ret->u.fn.r->u.e.l = (Expr *) tmp;
      ret->u.fn.r->type = E_LT;
      ret->u.fn.r->u.e.r = walk_fn_args (cookie, e->u.fn.r, &args);

      // extract user macro!

      sc = cookie->scope;
      u = NULL;
      prev = NULL;
      while (tmp->Rest()) {
	it = sc->Lookup (tmp->getName());
	Assert (it, "This should have been caught earlier!");
	u = dynamic_cast<UserDef *> (it->BaseType());
	Assert (u, "This should have been caught earlier!");
	sc = u->CurScope();
	prev = tmp;
	tmp = tmp->Rest();
      }
      Assert (u && prev, "This should have been caught earlier!");
      um = u->getMacro (tmp->getName());
      Assert (um, "Didn't find user macro?");

      if (um->getNumPorts () != args) {
	act_parse_err (&p, "User macro ``%s'': incorrect number of arguments (expected %d, got %d)", tmp->getName(), um->getNumPorts(), args);
      }

      // ok: (1) e.u.fn.s should be the usermacro
      //     (2) e.u.fn.r augmented with the id!
      prev->prune();
      delete tmp;
      ret->u.fn.s = (char *) um;
    }
    break;

  default:
    fatal_error ("-- unknown expression type --");
    break;
  }
  return ret;
}

int _act_shadow_warning (void)
{
  return Act::shadow;
}
