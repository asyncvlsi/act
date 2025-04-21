/*************************************************************************
 *
 *  Copyright (c) 2020 Rajit Manohar
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
#include <act/expr.h>
#include <act/expr_extra.h>
#include <common/file.h>
#include <common/misc.h>
#include <common/mstring.h>
#include <string.h>

void *act_parse_a_fexpr (LFILE *);

static int tokand, tokor, lpar, rpar, ddot, colon;
static int tokxor, tokplus, tokmult;
static int double_colon, comma;
static int inttok, booltok;
static int langle, rangle;
static int dot;

static void do_init (LFILE *l)
{
  static int init = 0;
  if (!init) {
    tokand = expr_gettoken (E_AND);
    tokor = expr_gettoken (E_OR);
    tokxor = expr_gettoken (E_XOR);
    tokplus = expr_gettoken (E_PLUS);
    tokmult = expr_gettoken (E_MULT);
    lpar = expr_gettoken (E_LPAR);
    rpar = expr_gettoken (E_RPAR);
    ddot = expr_gettoken (E_BITFIELD);
    colon = expr_gettoken (E_COLON);
    langle = expr_gettoken (E_LT);
    rangle = expr_gettoken (E_GT);
    double_colon = file_addtoken (l, "::");
    comma = file_addtoken (l, ",");
    inttok = file_addtoken (l, "int");
    booltok = file_addtoken (l, "bool");
    dot = file_addtoken (l, ".");
    init = 1;
  }
}
  

static Expr *_parse_expr_func2 (LFILE *l, int first_fn)
{
  Expr *e, *f;
  Expr *templ = NULL;
  int found_dcolon;
  int pushed = 0;
  
  do_init(l);
  
  if ((first_fn && (file_sym (l) == double_colon)) || file_sym (l) == f_id) {

#define PRINT_STEP				\
  do {						\
    len = strlen (buf+k);			\
    k += len;					\
    sz -= len;					\
    if (sz <= 1) {				\
      return NULL;				\
    }						\
  } while (0)

    /* could be a function call! */
    char buf[10240];
    int sz, len, k;
    NEW (e, Expr);
    e->type = E_FUNCTION;
    e->u.e.l = NULL;
    e->u.e.r = NULL;

    k = 0;
    sz = 10239;


    /* look for initial :: */
    if (file_have (l, double_colon)) {
      found_dcolon = 1;
      snprintf (buf, 10240, "::");
    }
    else {
      found_dcolon = 0;
      buf[0] = '\0';
    }
    PRINT_STEP;


    if (!found_dcolon) {
      pushed = 1;
      file_push_position (l);
    }
    
    /* look for ::-separated identifiers */
    while (file_have (l, f_id)) {
      snprintf (buf+k, sz, "%s", file_prev (l));
      PRINT_STEP;
      if (!file_have (l, double_colon)) {
	break;
      }
      else {
	found_dcolon = 1;
      }
      snprintf (buf+k, sz, "%s", file_prev (l));
      PRINT_STEP;
    }

    /* look for optional template parameters */
    if (file_have (l, langle)) {
      Expr *tt;

      if (!first_fn) {
	/* no template parameters allowed in method calls */
	if (pushed) {
	  file_set_position (l);
	  file_pop_position (l);
	}
	return NULL;
      }
      expr_endgtmode (1);
      NEW (templ, Expr);
      templ->type = E_LT;
      templ->u.e.r = NULL;
      templ->u.e.l = expr_parse_any (l);
      if (!templ->u.e.l) {
	FREE (templ);
	FREE (e);
	expr_endgtmode (0);
	if (pushed) {
	  file_set_position (l);
	  file_pop_position (l);
	}
	return NULL;
      }
      tt = templ;
      
      while (file_have (l, comma)) {
	NEW (tt->u.e.r, Expr);
	tt = tt->u.e.r;
	tt->type = E_LT;
	tt->u.e.r = NULL;
	tt->u.e.l = expr_parse_any (l);
	if (!tt->u.e.l) {
	  expr_free (templ);
	  FREE (e);
	  expr_endgtmode (0);
	  if (pushed) {
	    file_set_position (l);
	    file_pop_position (l);
	  }
	  return NULL;
	}
      }
      expr_endgtmode (0);
      if (!file_have (l, rangle)) {
	expr_free (templ);
	FREE (e);
	if (pushed) {
	  file_set_position (l);
	  file_pop_position (l);
	}
	return NULL;
      }
    }

    if (!file_have (l, lpar)) {
      if (templ) {
	expr_free (templ);
	FREE (e);
	if (pushed) {
	  file_set_position (l);
	  file_pop_position (l);
	}
	return NULL;
      }
      else if (found_dcolon && file_have (l, dot)) {
	if (file_have (l, f_id)) {
	  e->type = E_ENUM_CONST;
	  e->u.fn.s = Strdup (buf);
	  e->u.fn.r = (Expr *) string_cache (file_prev (l));
	  if (pushed) {
	    file_pop_position (l);
	  }
	  return e;
	}
      }
      else if (!found_dcolon && file_have (l, dot)) {
	if (pushed) {
	  file_set_position (l);
	  file_pop_position (l);
	  pushed = 0;
	}
	/* first part of the ID is in buf */
	Assert (expr_parse_id, "What?");
	pId *v = (*expr_parse_id) (l);
	if (!v) {
	  FREE (e);
	  return NULL;
	}
	/* add template parsing here! */
	if (!file_have (l, lpar)) {
	  FREE (e);
	  Assert (expr_free_id, "What?");
	  (*expr_free_id) (v);
	  return NULL;
	}
	expr_inc_parens ();
	e->type = E_USERMACRO;
	NEW (e->u.e.l, Expr);
	e->u.e.l->type = E_VAR;
	e->u.e.l->u.e.l = (Expr *)v;
	e->u.e.l->u.e.r = NULL;
	e->u.e.r = NULL;
	f = e;
	if (file_sym (l) != rpar) {
	  do {
	    if (f == e) {
	      NEW (e->u.e.r, Expr);
	      f = e->u.e.r;
	    }
	    else {
	      NEW (f->u.e.r, Expr);
	      f = f->u.e.r;
	    }
	    f->type = E_LT;
	    f->u.e.r = NULL;
	    f->u.e.l = expr_parse_any (l);
	    if (!f->u.e.l) {
	      expr_free (e);
	      expr_dec_parens ();
	      return NULL;
	    }
	  } while (file_have (l, comma));
	  if (file_sym (l) != rpar) {
	    expr_free (e);
	    expr_dec_parens ();
	    if (pushed) {
	      file_set_position (l);
	      file_pop_position (l);
	    }
	    return NULL;
	  }
	  /* success! */
	  file_getsym (l);
	  expr_dec_parens ();
	  return e;
	}
	else {
	  /* eat the token */
	  file_getsym (l);
	}
	return e;
      }
      if (pushed) {
	file_set_position (l);
	file_pop_position (l);
      }
      FREE (e);
      return NULL;
    }
    e->u.fn.s = Strdup (buf);
    e->u.fn.r = NULL;
    f = e;
    expr_inc_parens ();
    if (!file_have (l, rpar)) {
      do {
	NEW (f->u.e.r, Expr);
	f = f->u.e.r;
	f->type = E_LT;
	f->u.e.r = NULL;
	f->u.e.l = expr_parse_any (l);
	if (!f->u.e.l) {
	  expr_dec_parens ();
	  expr_free (e);
	  if (templ) {
	    expr_free (templ);
	  }
	  if (pushed) {
	    file_set_position (l);
	    file_pop_position (l);
	  }
	  return NULL;
	}
      } while (file_have (l, comma));
      if (!file_have (l, rpar)) {
	expr_dec_parens ();
	expr_free (e);
	if (templ) {
	  expr_free (templ);
	}
	if (pushed) {
	  file_set_position (l);
	  file_pop_position (l);
	}
	return NULL;
      }
    }
    if (templ) {
      NEW (f, Expr);
      f->type = E_GT;
      f->u.e.l = templ;
      f->u.e.r = e->u.fn.r;
      e->u.fn.r = f;
    }
    expr_dec_parens ();
  }
  else {
    if (pushed) {
      file_set_position (l);
      file_pop_position (l);
    }
    return NULL;
  }
  if (pushed) {
    file_pop_position (l);
  }
  return e;
}

static Expr *_parse_id_or_func (LFILE *l)
{
  Expr *e = _parse_expr_func2 (l, 0);
  if (e) return e;
  Assert (expr_parse_id, "What?");

  pId *v = (*expr_parse_id) (l);
  if (!v) {
    return NULL;
  }
  NEW (e, Expr);
  e->type = E_VAR;
  e->u.e.l = (Expr *) v;
  e->u.e.r = NULL;

  return e;
}
  
static Expr *_parse_expr_func (LFILE *l)
{
  Expr *e = _parse_expr_func2 (l, 1);
  if (!e) {
    return NULL;
  }

  file_push_position (l);
  while (file_have (l, dot)) {
    Expr *f = _parse_id_or_func (l);
    Expr *tmp;
    char *fn;
    if (!f) {
      file_set_position (l);
      file_pop_position (l);
      return e;
    }
    /*
      f could be: E_VAR, E_USERMACRO, or E_FUNCTION
    */
    if (e->type == E_USERMACRO) {
      switch (f->type) {
      case E_VAR:
	/* usermacro . var -> struct ref */
	NEW (tmp, Expr);
	tmp->type = E_STRUCT_REF;
	tmp->u.e.l = e;
	tmp->u.e.r = f;
	e = tmp;
	break;

      case E_USERMACRO:
	/*
	  usermacro . usermacro ->

             usermacro
              /       \
        struct ref   args
	  /   |
	 um   ID from "f" (the . usermacro)
	*/
	NEW (tmp, Expr);
	tmp->type = E_STRUCT_REF;
	tmp->u.e.l = e;
	Assert (f->u.e.l && f->u.e.l->type == E_VAR, "What?");
	tmp->u.e.r = f->u.e.l;
	/* f is a user macro, and so this struct ref extracts the
	   prefix as the structure reference, and the actual macro name
	   in the parent is the last field!
	*/
	f->u.e.l = tmp;
	e = f;
	break;

      case E_FUNCTION:
        /*
         usermacro . function ->
             usermacro
             /       \
        struct ref    args  <-- contains USERMACRO2, the function name
        */
	f->type = E_USERMACRO;
	tmp = f->u.fn.r; // args
	/* f->u.fn.s -> macro name, this is stashed in the args field
	   with a special flag
	*/
	fn = f->u.fn.s;
	f->u.e.l = e;
	NEW (f->u.e.r, Expr);
	f->u.e.r->type = E_USERMACRO2;
	f->u.e.r->u.fn.s = (char *)fn;
	f->u.e.r->u.fn.r = tmp;
	/* if the ARGS type for a usermacro is usermacro2, then the l
	   field is actually the macro name and we don't have to strip
	   anything out from the usermacro l field
	*/
	e = f;
	break;

      default:
	Assert (0, "This should not happen");
	break;
      }
    }
    else if (e->type == E_FUNCTION) {
      switch (f->type) {
      case E_VAR:
	/* function. var -> struct ref */
	NEW (tmp, Expr);
	tmp->type = E_STRUCT_REF;
	tmp->u.e.l = e;
	tmp->u.e.r = f;
	e = tmp;
	break;

      case E_USERMACRO:
	/*
          function . usermacro ->
             usermacro
	    /	     \
         struct ref  args
	*/
	NEW (tmp, Expr);
	tmp->type = E_STRUCT_REF;
	tmp->u.e.l = e;
	tmp->u.e.r = f->u.e.l;
	f->u.e.l = tmp;
	e = f;
	break;

      case E_FUNCTION:
	/*
          function . function ->
             usermacro
	     /        \
          struct ref   arg <- contains usermacro2
	*/
	NEW (tmp, Expr);
	tmp->type = E_USERMACRO;
	tmp->u.e.l = e;
	tmp->u.e.r = f;
	f->type = E_USERMACRO2;
	e = tmp;
	break;

      default:
	Assert (0, "This should not happen");
	break;
      }
    }
    else {
      Assert (0, "Should not be here");
    }
    file_pop_position (l);
    if (e->type == E_STRUCT_REF) {
      return e;
    }
    file_push_position (l);
  }
  file_pop_position (l);
  return e;
}


Expr *act_parse_expr_syn_loop_bool (LFILE *l)
{
  Expr *e, *f;
  int etype;
  
  do_init(l);

  file_push_position (l);

  if (file_have (l, lpar)) {
    if (file_have (l, tokand)) {
      etype = E_ANDLOOP;
    }
    else if (file_have (l, tokor)) {
      etype = E_ORLOOP;
    }
    else {
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    NEW (e, Expr);
    e->type = etype;
    e->u.e.l = NULL;
    e->u.e.r = NULL;

    if (!file_have (l, f_id)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    NEW (e->u.e.l, Expr);
    e->u.e.l->type = E_RAWFREE;
    e->u.e.l->u.e.l = (Expr *)Strdup (file_prev (l));
    e->u.e.l->u.e.r = NULL;

    if (!file_have (l, colon)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    if (!(f = expr_parse_int (l))) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    NEW (e->u.e.r, Expr);
    e->u.e.r->type = etype;
    e->u.e.r->u.e.l = f;

    NEW (e->u.e.r->u.e.r, Expr);
    e->u.e.r->u.e.r->type = etype;
    e->u.e.r->u.e.r->u.e.l = NULL;
    e->u.e.r->u.e.r->u.e.r = NULL;

    if (file_have (l, ddot)) {
      if (!(f = expr_parse_int (l))) {
	expr_free (e);
	file_set_position (l);
	file_pop_position (l);
	return NULL;
      }
      e->u.e.r->u.e.r->u.e.l = f;
    }
    if (!file_have (l, colon)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    if (!(e->u.e.r->u.e.r->u.e.r = expr_parse_bool (l))) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    if (!file_have (l, rpar)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
  }
  else if (file_have_keyw (l, "bool")) {
    if (!file_have (l, lpar)) {
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    expr_inc_parens ();
    NEW (e, Expr);
    e->type = E_BUILTIN_BOOL;
    e->u.e.l = expr_parse_int (l);
    e->u.e.r = NULL;
    if (!e->u.e.l) {
      expr_dec_parens ();
      FREE (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    expr_dec_parens ();
    if (!file_have (l, rpar)) {
      file_set_position (l);
      file_pop_position (l);
      expr_free (e);
      return NULL;
    }
  }
  else {
    e = _parse_expr_func (l);
    if (!e) {
      file_set_position (l);
    }
  }
  file_pop_position (l);
  return e;
}

Expr *act_parse_expr_syn_loop_int (LFILE *l)
{
  Expr *e, *f;
  int etype;
  
  do_init(l);

  file_push_position (l);

  if (file_have (l, lpar)) {
    if (file_have (l, tokand)) {
      etype = E_ANDLOOP;
    }
    else if (file_have (l, tokor)) {
      etype = E_ORLOOP;
    }
    else if (file_have (l, tokplus)) {
      etype = E_PLUSLOOP;
    }
    else if (file_have (l, tokmult)) {
      etype = E_MULTLOOP;
    }
    else if (file_have (l, tokxor)) {
      etype = E_XORLOOP;
    }
    else {
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    NEW (e, Expr);
    e->type = etype;
    e->u.e.l = NULL;
    e->u.e.r = NULL;

    if (!file_have (l, f_id)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    NEW (e->u.e.l, Expr);
    e->u.e.l->type = E_RAWFREE;
    e->u.e.l->u.e.l = (Expr *)Strdup (file_prev (l));
    e->u.e.l->u.e.r = NULL;

    if (!file_have (l, colon)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    if (!(f = expr_parse_int (l))) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    NEW (e->u.e.r, Expr);
    e->u.e.r->type = etype;
    e->u.e.r->u.e.l = f;

    NEW (e->u.e.r->u.e.r, Expr);
    e->u.e.r->u.e.r->type = etype;
    e->u.e.r->u.e.r->u.e.l = NULL;
    e->u.e.r->u.e.r->u.e.r = NULL;

    if (file_have (l, ddot)) {
      if (!(f = expr_parse_int (l))) {
	expr_free (e);
	file_set_position (l);
	file_pop_position (l);
	return NULL;
      }
      e->u.e.r->u.e.r->u.e.l = f;
    }
    if (!file_have (l, colon)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    if (!(e->u.e.r->u.e.r->u.e.r = expr_parse_int (l))) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    if (!file_have (l, rpar)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
  }
  else {
    e = _parse_expr_func (l);
    if (!e) {
      file_set_position (l);
    }
  }
  file_pop_position (l);
  return e;
}


Expr *act_parse_expr_intexpr_base (LFILE *l)
{
  Expr *e, *f;
  int etype;

  do_init(l);
  
  file_push_position (l);

  if (file_have (l, lpar)) {
    if (file_have (l, tokand)) {
      etype = E_ANDLOOP;
    }
    else if (file_have (l, tokor)) {
      etype = E_ORLOOP;
    }
    else if (file_have (l, tokplus)) {
      etype = E_PLUSLOOP;
    }
    else if (file_have (l, tokmult)) {
      etype = E_MULTLOOP;
    }
    else if (file_have (l, tokxor)) {
      etype = E_XORLOOP;
    }
    else {
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    NEW (e, Expr);
    e->type = etype;
    e->u.e.l = NULL;
    e->u.e.r = NULL;

    if (!file_have (l, f_id)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    NEW (e->u.e.l, Expr);
    e->u.e.l->type = E_RAWFREE;
    e->u.e.l->u.e.l = (Expr *)Strdup (file_prev (l));
    e->u.e.l->u.e.r = NULL;

    if (!file_have (l, colon)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    if (!(f = expr_parse_int (l))) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    NEW (e->u.e.r, Expr);
    e->u.e.r->type = etype;
    e->u.e.r->u.e.l = f;

    NEW (e->u.e.r->u.e.r, Expr);
    e->u.e.r->u.e.r->type = etype;
    e->u.e.r->u.e.r->u.e.l = NULL;
    e->u.e.r->u.e.r->u.e.r = NULL;

    if (file_have (l, ddot)) {
      if (!(f = expr_parse_int (l))) {
	expr_free (e);
	file_set_position (l);
	file_pop_position (l);
	return NULL;
      }
      e->u.e.r->u.e.r->u.e.l = f;
    }
    if (!file_have (l, colon)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    if (!(e->u.e.r->u.e.r->u.e.r = expr_parse_int (l))) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    if (!file_have (l, rpar)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
  }
  if (file_have_keyw (l, "int")) {
    if (!file_have (l, lpar)) {
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    NEW (e, Expr);
    expr_inc_parens ();
    e->type = E_BUILTIN_INT;
    e->u.e.l = expr_parse_bool (l);
    if (!e->u.e.l || (file_sym (l) != rpar)) {
      expr_free (e->u.e.l);
      file_set_position (l);
      Assert (file_have_keyw (l, "int"), "What?");
      Assert (file_have (l, lpar), "What?");
      e->u.e.l = expr_parse_any (l);
    }
    e->u.e.r = NULL;
    if (!e->u.e.l) {
      expr_dec_parens ();
      FREE (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    if (!file_have (l, rpar)) {
      if (file_have (l, comma)) {
	e->u.e.r = expr_parse_int (l);
      }
      if (!e->u.e.r) {
	expr_dec_parens ();
	file_set_position (l);
	file_pop_position (l);
	expr_free (e);
	return NULL;
      }
      if (!file_have (l, rpar)) {
	expr_dec_parens ();
	expr_free (e);
	file_set_position (l);
	file_pop_position (l);
	return NULL;
      }
    }
    expr_dec_parens ();
  }
  else {
    e = _parse_expr_func (l);
    if (!e) {
      file_set_position (l);
    }
  }
  file_pop_position (l);
  return e;
}

static Expr *f_parse_expr_syn_loop_bool (LFILE *l)
{
  Expr *e, *f;
  int etype;
  
  do_init(l);

  file_push_position (l);

  if (file_have (l, lpar)) {
    if (file_have (l, tokand)) {
      etype = E_ANDLOOP;
    }
    else if (file_have (l, tokor)) {
      etype = E_ORLOOP;
    }
    else if (file_have (l, tokplus)) {
      etype = E_PLUSLOOP;
    }
    else if (file_have (l, tokmult)) {
      etype = E_MULTLOOP;
    }
    else if (file_have (l, tokxor)) {
      etype = E_XORLOOP;
    }
    else {
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    NEW (e, Expr);
    e->type = etype;
    e->u.e.l = NULL;
    e->u.e.r = NULL;

    if (!file_have (l, f_id)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    NEW (e->u.e.l, Expr);
    e->u.e.l->type = E_RAWFREE;
    e->u.e.l->u.e.l = (Expr *)Strdup (file_prev (l));
    e->u.e.l->u.e.r = NULL;

    if (!file_have (l, colon)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    if (!(f = (Expr *) act_parse_a_fexpr (l))) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }

    NEW (e->u.e.r, Expr);
    e->u.e.r->type = etype;
    e->u.e.r->u.e.l = f;

    NEW (e->u.e.r->u.e.r, Expr);
    e->u.e.r->u.e.r->type = etype;
    e->u.e.r->u.e.r->u.e.l = NULL;
    e->u.e.r->u.e.r->u.e.r = NULL;

    if (file_have (l, ddot)) {
      if (!(f = (Expr *) act_parse_a_fexpr (l))) {
	expr_free (e);
	file_set_position (l);
	file_pop_position (l);
	return NULL;
      }
      e->u.e.r->u.e.r->u.e.l = f;
    }
    if (!file_have (l, colon)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    if (!(e->u.e.r->u.e.r->u.e.r = act_parse_a_fexpr (l))) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    if (!file_have (l, rpar)) {
      expr_free (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
  }
  else if (file_have_keyw (l, "bool")) {
    if (!file_have (l, lpar)) {
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    expr_inc_parens ();
    NEW (e, Expr);
    e->type = E_BUILTIN_BOOL;
    e->u.e.l = act_parse_a_fexpr (l);
    e->u.e.r = NULL;
    if (!e->u.e.l) {
      expr_dec_parens ();
      FREE (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    expr_dec_parens ();
    if (!file_have (l, rpar)) {
      file_set_position (l);
      file_pop_position (l);
      expr_free (e);
      return NULL;
    }
  }
  else {
    e = _parse_expr_func (l);
    if (!e) {
      file_set_position (l);
    }
  }
  file_pop_position (l);
  return e;
}


Expr *act_expr_any_basecase (LFILE *l)
{
  Expr *e;

  do_init(l);
  
  file_push_position (l);

  if (file_have_keyw (l, "int")) {
    if (!file_have (l, lpar)) {
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    NEW (e, Expr);
    expr_inc_parens ();
    e->type = E_BUILTIN_INT;
    e->u.e.l = (Expr *) act_parse_a_fexpr (l);
    if (!e->u.e.l) {
      expr_dec_parens ();
      FREE (e);
      file_set_position (l);
      file_pop_position (l);
      return NULL;
    }
    e->u.e.r = NULL;
    if (!file_have (l, rpar)) {
      if (file_have (l, comma)) {
	e->u.e.r = (Expr *) act_parse_a_fexpr (l);
      }
      if (!e->u.e.r) {
	expr_dec_parens ();
	file_set_position (l);
	file_pop_position (l);
	expr_free (e);
	return NULL;
      }
      if (!file_have (l, rpar)) {
	expr_dec_parens ();
	expr_free (e);
	file_set_position (l);
	file_pop_position (l);
	return NULL;
      }
      else {
	file_pop_position (l);
      }
    }
    else {
      file_pop_position (l);
    }
    expr_dec_parens ();
  }
  else {
    file_pop_position (l);
    return f_parse_expr_syn_loop_bool (l);
  }
  return e;
}

int act_expr_parse_newtokens (LFILE *l)
{
  do_init(l);

  return (file_sym (l) == double_colon || file_sym (l) == inttok ||
	  file_sym (l) == booltok);
}

int act_expr_free_default (Expr *e)
{
  if (!e) return 0;
  if (e->type == E_ENUM_CONST) {
    FREE (e->u.fn.s);
    return 1;
  }
  else if (e->type == E_USERMACRO2) {
    FREE (e->u.e.l);
    act_expr_free_default (e->u.e.r);
    return 1;
  }
  return 0;
}


int act_expr_could_be_struct (Expr *e)
{
  if (!e) return 0;
  if (e->type == E_VAR || e->type == E_STRUCT_REF ||
      e->type == E_FUNCTION || e->type == E_USERMACRO) {
    return 1;
  }
  return 0;
}
