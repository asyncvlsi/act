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
#include "misc.h"
#include "file.h"
#include "expr.h"

#include "prs.h"
#include "act_parse_int.h"
#include <act/types.h>
#include "act_walk.extra.h"

/*
 * ... and I thought I wouldn't have to write a parser any more

   prs_expr: prs_term "|" prs_expr
           | prs_term
           ;
   
   prs_term: prs_atom "&" [ "{" dir prs_expr "}" ] prs_term
           | prs_atom
           ;
   
   prs_atom: [ "~" ] prs_basic_block
           ;
   
   prs_basic_block: sized_id
                  | "(" prs_expr ")"
                  | "@" ID
                  | "(" prs_op ID ":" wint_expr
                      [ ".." wint_expr ] ":" prs_expr ")"
                  ;
   
   prs_op: "&" ":"
         | "|" ":"
         ;
   
   bool_expr_id: expr_id
               ;
   
   size_spec: "<" wint_expr [ "," ID ] ">"
            | "<" wint_expr "," wint_expr [ "," ID ] ">"
            |
            ;
   
   sized_id: bool_expr_id size_spec
           ;

   CONSISTENCY: MAKE SURE THIS IS CONSISTENT WITH lang.m4
   (size_spec, sized_id are shared!)

*/

#define STD_ERRMSG "Error parsing production rule"

/*
  Token list 
*/
static int LANGLE, RANGLE, TWIDDLE, LBRACE, RBRACE, AND, OR, COLON, COMMA,
  AT, DDOT, LPAR, RPAR, PLUS, MINUS, SEMI;

static act_prs_expr_t *_act_parse_prs_expr (LFILE *l);

void act_init_prs_expr (LFILE *l)
{
  /* base operators */
  TWIDDLE = file_addtoken (l, "~");
  AND = file_addtoken (l, "&");
  OR = file_addtoken (l, "|");

  PLUS = file_addtoken (l, "+");
  MINUS = file_addtoken (l, "-");

  /* brackets */
  LANGLE = file_addtoken (l, "<");
  RANGLE = file_addtoken (l, ">");
  LBRACE = file_addtoken (l, "{");
  RBRACE = file_addtoken (l, "}");
  LPAR = file_addtoken (l, "(");
  RPAR = file_addtoken (l, ")");

  /* separators */
  COLON = file_addtoken (l, ":");
  COMMA = file_addtoken (l, ",");
  AT = file_addtoken (l, "@");
  DDOT = file_addtoken (l, "..");
  SEMI = file_addtoken (l, ";");
}

int act_is_a_prs_expr (LFILE *l)
{
  if (file_sym (l) == AT ||
      file_sym (l) == f_id ||
      file_sym (l) == LPAR ||
      file_sym (l) == TWIDDLE) return 1;
  return 0;
}

static void _freeexpr (act_prs_expr_t *e)
{
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
  case ACT_PRS_EXPR_NOT:
    _freeexpr (e->u.e.l);
    _freeexpr (e->u.e.r);
    _freeexpr (e->u.e.pchg);
    break;
    
  case ACT_PRS_EXPR_VAR:
    if (e->u.v.id && expr_free_id)  {
      (*expr_free_id) (e->u.v.id);
    }
    if (e->u.v.sz) {
      if (e->u.v.sz->l) {
	expr_free (e->u.v.sz->l);
      }
      if (e->u.v.sz->w) {
	expr_free (e->u.v.sz->w);
      }
      if (e->u.v.sz->folds) {
	expr_free (e->u.v.sz->folds);
      }
      FREE (e->u.v.sz);
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    if (e->u.l.label) {
      FREE (e->u.l.label);
    }
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    if (e->u.loop.id) {
      FREE (e->u.loop.id);
    }
    if (e->u.loop.lo) {
      expr_free (e->u.loop.lo);
    }
    if (e->u.loop.hi) {
      expr_free (e->u.loop.hi);
    }
    _freeexpr (e->u.loop.e);
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;
    
  default:
    fatal_error ("what?");
    break;
  }
  FREE (e);
}

static act_prs_expr_t *atom (LFILE *l)
{
  int neg = 0;
  act_prs_expr_t *e, *f;
  pId *id;
  int t;
  unsigned int flags;

  e = NULL;
  while (file_have (l, TWIDDLE)) {
    neg = 1 - neg;
  }
  if (file_have (l, LPAR)) {
    if (file_have (l, AND)) {
      t = ACT_PRS_EXPR_ANDLOOP;
    }
    else if (file_have (l, OR)) {
      t = ACT_PRS_EXPR_ORLOOP;
    }
    else {
      e = _act_parse_prs_expr (l);
      if (!file_have (l, RPAR)) {
	act_parse_seterr (l, STD_ERRMSG ": missing right parentheses");
	_freeexpr (e);
	return NULL;
      }
      goto done;
    }
    flags = file_flags (l);
    file_setflags (l, flags | FILE_FLAGS_NOREAL);

    /* okay */
    NEW (e, act_prs_expr_t);
    e->type = t;
    if (!file_have (l, f_id)) {
      FREE (e);
      file_setflags (l, flags);
      act_parse_seterr (l, STD_ERRMSG ": missing identifier for and/or loop");
      return NULL;
    }
    e->u.loop.id = Strdup (file_prev (l));
    if (!file_have (l, COLON)) {
      FREE (e->u.loop.id);
      FREE (e);
      file_setflags (l, flags);
      act_parse_seterr (l, STD_ERRMSG ": missing colon in and/or loop");
      return NULL;
    }
    e->u.loop.lo = expr_parse_int (l);
    if (!e->u.loop.lo) {
      FREE (e->u.loop.id);
      FREE (e);
      file_setflags (l, flags);
      act_parse_seterr (l, STD_ERRMSG ": error parsing range in and/or loop");
      return NULL;
    }
    if (file_have (l, DDOT)) {
      e->u.loop.hi = expr_parse_int (l);
      if (!e->u.loop.hi) {
	expr_free (e->u.loop.lo);
	FREE (e->u.loop.id);
	FREE (e);
	file_setflags (l, flags);
	act_parse_seterr (l, STD_ERRMSG ": error parsing range in and/or loop");
	return NULL;
      }
    }
    else {
      e->u.loop.hi = NULL;
    }
    file_setflags (l, flags);
    if (!file_have (l, COLON)) {
      if (e->u.loop.hi) expr_free (e->u.loop.hi);
      if (e->u.loop.lo) expr_free (e->u.loop.lo);
      FREE (e->u.loop.id);
      FREE (e);
      act_parse_seterr (l, STD_ERRMSG ": error parsing range in and/or loop");
      return NULL;
    }
    e->u.loop.e = _act_parse_prs_expr (l);
    if (!e->u.loop.e) {
      if (e->u.loop.hi) expr_free (e->u.loop.hi);
      if (e->u.loop.lo) expr_free (e->u.loop.lo);
      FREE (e->u.loop.id);
      FREE (e);
      act_parse_seterr (l, STD_ERRMSG ": error parsing body of and/or loop");
      return NULL;
    }
    if (!file_have (l, RPAR)) {
      _freeexpr (e);
      act_parse_seterr (l, STD_ERRMSG ": missing right parentheses");
      return NULL;
    }
    goto done;
  }
  else if (file_have (l, AT)) {
    /* A label! */
    if (file_have (l, f_id)) {
      NEW (e, act_prs_expr_t);
      e->type = ACT_PRS_EXPR_LABEL;
      e->u.l.label = Strdup (file_prev (l));
    }
    else {
      act_parse_seterr (l, STD_ERRMSG ": error in @-label");
      return NULL;
    }
  }
  else if ((id = (*expr_parse_id) (l))) {
    /* sizes? */
    NEW (e, act_prs_expr_t);
    e->type = ACT_PRS_EXPR_VAR;
    e->u.v.id = (ActId *)id;
    e->u.v.sz = NULL;
    if (file_have (l, LANGLE)) {
      act_size_spec_t *sz;

      NEW (sz, act_size_spec_t);
      sz->w = expr_parse_real (l);
      sz->l = NULL;
      sz->flavor = 0;
      sz->folds = NULL;
      
      if (!sz->w) {
	FREE (sz);
	_freeexpr (e);
	act_parse_seterr (l, STD_ERRMSG ": error in width syntax");
	return NULL;
      }
      e->u.v.sz = sz;
      if (file_have (l, COMMA)) {
	if (file_sym (l) == f_id &&
	    (act_dev_string_to_value (file_tokenstring (l)) != -1)) {
	  file_mustbe (l, f_id);
	  sz->flavor = act_dev_string_to_value (file_prev (l));
	  if (sz->flavor == -1) {
	    act_parse_seterr (l, STD_ERRMSG ": unknown transistor flavor");
	    _freeexpr (e);
	    return NULL;
	  }
	}
	else {
	  sz->l = expr_parse_real (l);
	  if (!sz->l) {
	    _freeexpr (e);
	    act_parse_seterr (l, STD_ERRMSG ": error in length syntax");
	    return NULL;
	  }
	  if (file_have (l, COMMA)) {
	    if (!file_have (l, f_id)) {
	      _freeexpr (e);
	      act_parse_seterr (l, STD_ERRMSG ": missing transistor flavor");
	      return NULL;
	    }
	    sz->flavor = act_dev_string_to_value (file_prev (l));
	    if (sz->flavor == -1) {
	      act_parse_seterr (l, STD_ERRMSG ": unknown transistor flavor");
	      _freeexpr (e);
	      return NULL;
	    }
	  }
	}
      }
      if (file_have (l, SEMI)) {
	sz->folds = expr_parse_int (l);
	if (!sz->folds) {
	  _freeexpr (e);
	  act_parse_seterr (l, STD_ERRMSG ": unknown transistor flavor");
	  return NULL;
	}
      }
      if (!file_have (l, RANGLE)) {
	_freeexpr (e);
	act_parse_seterr (l, STD_ERRMSG ": missing `>'");
	return NULL;
      }
    }
  }
  else {
    act_parse_seterr (l, STD_ERRMSG ": missing id");
    return NULL;
  }
 done:
  if (neg) {
    NEW (f, act_prs_expr_t);
    f->type = ACT_PRS_EXPR_NOT;
    f->u.e.l = e;
    f->u.e.r = NULL;
    f->u.e.pchg = NULL;
    f->u.e.pchg_type = -1;
    e = f;
  }
  return e;
}

static act_prs_expr_t *term (LFILE *l)
{
  act_prs_expr_t *e, *f;
  static int in_pchg = 0;

  f = e = atom (l);
  if (!e) return NULL;

  while (file_have (l, AND)) {
    NEW (f, act_prs_expr_t);
    f->type = ACT_PRS_EXPR_AND;
    f->u.e.l = e;
    f->u.e.pchg = NULL;
    f->u.e.pchg_type = -1;
    f->u.e.r = NULL;

    if (file_have (l, LBRACE)) {
      if (in_pchg) {
	/* can't have a pchg of a pchg! */
	_freeexpr (f);
	return NULL;
      }
      /* pchg! */
      if (file_have (l, PLUS)) {
	f->u.e.pchg_type = 1;
      }
      else if (file_have (l, MINUS)) {
	f->u.e.pchg_type = 0;
      }
      else {
	_freeexpr (f);
	return NULL;
      }
      in_pchg++;
      f->u.e.pchg = _act_parse_prs_expr (l);
      in_pchg--;
      if (!f->u.e.pchg) {
	_freeexpr (f);
	return NULL;
      }
      if (!file_have (l, RBRACE)) {
	_freeexpr (f);
	return NULL;
      }
    }
    else {
      f->u.e.pchg_type = -1;
    }

    f->u.e.r = atom(l);
    if (!f->u.e.r) {
      _freeexpr (f);
      return NULL;
    }
    e = f;
  }
  return f;
}


static act_prs_expr_t *_act_parse_prs_expr (LFILE *l)
{
  act_prs_expr_t *e, *f;

  f = e = term (l);
  if (!e) return NULL;

  while (file_have (l, OR)) {
    NEW (f, act_prs_expr_t);
    f->type = ACT_PRS_EXPR_OR;
    f->u.e.l = e;
    f->u.e.pchg = NULL;
    f->u.e.pchg_type = -1;
    f->u.e.r = term(l);
    if (!f->u.e.r) {
      _freeexpr (f);
      return NULL;
    }
    e = f;
  }
  return f;
}

void *act_parse_a_prs_expr (LFILE *l)
{
  act_prs_expr_t *e;

  file_push_position (l);

  e = _act_parse_prs_expr (l);
  if (!e) {
    act_parse_seterr (l, STD_ERRMSG);
    file_set_position (l);
  }
  file_pop_position (l);

  return e;
}


/*------------------------------------------------------------------------
 *
 * Walk expression tree
 *
 *------------------------------------------------------------------------
 */
ActId *act_walk_X_expr_id (ActTree *, Node_expr_id *);


/*
  CONSISTENCY: equivalent TO bool_expr_id.
*/
static ActId *_process_id (ActTree *a, ActId *id)
{
  int t;
  struct act_position p;
  p.l = a->line;
  p.c = a->column;
  p.f = a->file;

  Assert (id, "Hmm");

  id = act_walk_X_expr_id (a, (pId *)id);

  if ((id->Rest() == NULL) && (id->arrayInfo() == NULL) &&
      ((strcmp (id->getName(), "true") == 0) ||
       (strcmp (id->getName(), "false") == 0))) {
    return id;
  }
  
  t = act_type_var (a->scope, id, NULL);
  if (t != T_BOOL) {
    act_parse_msg (&p, "Identifier `");
    id->Print (stderr, NULL);
    fprintf (stderr, "' is not of type bool");
    exit (1);
  }
  return id;
}

/*
  CONSISTENCY: equivalent to wint_expr
*/
static Expr *_wint_expr (ActTree *a, Expr *e)
{
  int tc;
  struct act_position p;

  if (!e) return NULL;

  p.l = a->line;
  p.c = a->column;
  p.f = a->file;

  e = act_walk_X_expr (a, e);
  tc = act_type_expr (a->scope, e, NULL);
  if (tc == T_ERR) {
    act_parse_msg (&p, "Typechecking failed on expression!");
    fprintf (stderr, "\n\t%s\n", act_type_errmsg ());
    exit (1);
  }
  if (a->strict_checking && ((tc & T_STRICT) == 0)) {
    act_parse_err (&p, "Expression in port parameter list can only use strict template parameters");
  }
  if (!(tc & T_INT)) {
    act_parse_err (&p, "Expression must be of type pint");
  }
  if (!(tc & T_PARAM)) {
    act_parse_err (&p, "Expression must be of type pint");
  }
  return e;
}

/*
  CONSISTENCY: equivalent to wreal_expr
*/
static Expr *_wnumber_expr (ActTree *a, Expr *e)
{
  int tc;
  struct act_position p;

  if (!e) return NULL;

  p.l = a->line;
  p.c = a->column;
  p.f = a->file;

  e = act_walk_X_expr (a, e);
  tc = act_type_expr (a->scope, e, NULL);
  if (tc == T_ERR) {
    act_parse_msg (&p, "Typechecking failed on expression!");
    fprintf (stderr, "\n\t%s\n", act_type_errmsg ());
    exit (1);
  }
  if (a->strict_checking && ((tc & T_STRICT) == 0)) {
    act_parse_err (&p, "Expression in port parameter list can only use strict template parameters");
  }
  if (!(tc & (T_INT|T_REAL))) {
    act_parse_err (&p, "Expression must be of type int or real");
  }
  if (!(tc & T_PARAM)) {
    act_parse_err (&p, "Expression must be of type pint or preal");
  }
  return e;
}

static act_size_spec_t *_process_sz (ActTree *a, act_size_spec_t *sz)
{
  act_size_spec_t *ret;
  if (!sz) return NULL;
  NEW (ret, act_size_spec_t);
  ret->w = _wnumber_expr (a, sz->w);
  ret->l = _wnumber_expr (a, sz->l);
  ret->folds = _wnumber_expr (a, sz->folds);
  ret->flavor = sz->flavor;
  return ret;
}

static act_prs_expr_t *_act_walk_expr (ActTree *a, act_prs_expr_t *e)
{
  act_prs_expr_t *ret;

  if (!e) return NULL;
  NEW (ret, act_prs_expr_t);
  ret->type = e->type;
  ret->u.e.l = NULL;
  ret->u.e.r = NULL;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = _act_walk_expr (a, e->u.e.l);
    ret->u.e.r = _act_walk_expr (a, e->u.e.r);
    ret->u.e.pchg = _act_walk_expr (a, e->u.e.pchg);
    ret->u.e.pchg_type = e->u.e.pchg_type;
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v.id = _process_id (a, e->u.v.id);
    if (ret->u.v.id->Rest() == NULL && ret->u.v.id->arrayInfo() == NULL) {
      if (strcmp (ret->u.v.id->getName(), "true") == 0) {
	delete ret->u.v.id;
	ret->type = ACT_PRS_EXPR_TRUE;
      }
      else if (strcmp (ret->u.v.id->getName(), "false") == 0) {
	delete ret->u.v.id;
	ret->type = ACT_PRS_EXPR_FALSE;
      }
    }
    if (ret->type == ACT_PRS_EXPR_VAR) {
      ret->u.v.sz = _process_sz (a, e->u.v.sz);
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l.label = (char *)string_cache (e->u.l.label);
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    if (a->scope->Lookup (e->u.loop.id)) {
      struct act_position p;
      p.l = a->line;
      p.c = a->column;
      p.f = a->file;
      act_parse_err (&p, "Identifier `%s' already exists in current scope", e->u.loop.id);
    }
    a->scope->Add (e->u.loop.id, a->tf->NewPInt ());
    ret->u.loop.e = _act_walk_expr (a, e->u.loop.e);
    ret->u.loop.lo = _wint_expr (a, e->u.loop.lo);
    ret->u.loop.hi = _wint_expr (a, e->u.loop.hi);
    ret->u.loop.id = (char *)string_cache (e->u.loop.id);
    a->scope->Del (e->u.loop.id);
    break;

  default:
    Assert (0, "Should not be here");
    break;
  }
  return ret;
}

void *act_walk_X_prs_expr (ActTree *a, void *v)
{
  return _act_walk_expr (a, (act_prs_expr_t *)v);
}


void act_free_a_prs_expr (void *v)
{
  _freeexpr ((act_prs_expr_t *)v);
}
