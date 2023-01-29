/*************************************************************************
 *
 *  Faster expression parsing library
 *
 *  Copyright (c) 2021, 2019 Rajit Manohar
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
#include <string.h>
#include "fexpr.h"
#include <common/file.h>
#include <common/misc.h>
#include <common/list.h>
#include "act_parse_int.h"
#include "act_walk.extra.h"

static int T[E_NUMBER];
static int _prec_table[E_NUMBER];

Expr *(*expr_parse_basecase_extra)(LFILE *l) = NULL;

#define PUSH(x) file_push_position(x)
#define POP(x)  file_pop_position(x)
#define SET(x)  file_set_position(x)
#define INFO(x)

static LFILE *Tl;

static void fexpr_settoken (int x, int v);

/*------------------------------------------------------------------------
 *
 *  fexpr_init --
 *
 *   Initialize expression parser with standard tokens. Returns the
 *  number of tokens added.
 *
 *------------------------------------------------------------------------
 */
void act_init_fexpr (LFILE *l)
{
  int i;

#define INIT2(tok,str)   count += !file_istoken (l, str);  fexpr_settoken(tok, file_addtoken (l, str))
#define INIT(tok,str)   fexpr_settoken(tok, expr_gettoken (tok))

  INIT (E_AND, "&");
  INIT (E_OR, "|");
  INIT (E_NOT, "~");
  INIT (E_COMPLEMENT, "~");
  INIT (E_PLUS, "+");
  INIT (E_MINUS, "-");
  INIT (E_MULT, "*");
  INIT (E_DIV, "/");
  INIT (E_MOD, "%");
  INIT (E_LSL, "<<");
  INIT (E_ASR, ">>>");
  INIT (E_UMINUS, "-");
  INIT (E_QUERY, "?");
  INIT (E_LPAR, "(");
  INIT (E_RPAR, ")");
  INIT (E_XOR, "^");
  INIT (E_LT, "<");
  INIT (E_GT, ">");
  INIT (E_LE, "<=");
  INIT (E_GE, ">=");
  INIT (E_EQ, "=");
  INIT (E_NE, "!=");
  INIT (E_COLON, ":");
  INIT (E_PROBE, "#");
  INIT (E_COMMA, ",");
  INIT (E_CONCAT, "{");
  INIT (E_BITFIELD, "..");
  INIT (E_END, "}");

  for (i=0; i < E_NUMBER; i++) {
    _prec_table[i] = -1;
  }

  _prec_table[E_QUERY] = 5;
  _prec_table[E_COLON] = 5;

  _prec_table[E_OR] = 10;
  _prec_table[E_XOR] = 20;
  _prec_table[E_AND] = 30;

  _prec_table[E_LT] = 40;
  _prec_table[E_GT] = 40;
  _prec_table[E_LE] = 40;
  _prec_table[E_GE] = 40;
  _prec_table[E_EQ] = 40;
  _prec_table[E_NE] = 40;

  _prec_table[E_LSL] = 50;
  _prec_table[E_LSR] = 50;
  _prec_table[E_ASR] = 50;

  _prec_table[E_PLUS] = 60;
  _prec_table[E_MINUS] = 60;

  _prec_table[E_MULT] = 70;
  _prec_table[E_DIV] = 70;
  _prec_table[E_MOD] = 70;

  _prec_table[E_NOT] = 80;
  _prec_table[E_COMPLEMENT] = 80;
  _prec_table[E_UMINUS] = 80;
  
  return;
}


/*------------------------------------------------------------------------
 *
 *  fexpr_settoken --
 *
 *   Set parsing tokens
 *
 *------------------------------------------------------------------------
 */
static void fexpr_settoken (int x, int v)
{
  Assert (0 <= x && x < E_NUMBER, "Invalid token number");
  T[x] = v;
}

int act_is_a_fexpr (LFILE *l)
{
  if (file_sym (l) == f_id || file_sym (l) == f_integer ||
      file_sym (l) == f_real ||
      file_sym (l) == T[E_LPAR] ||
      file_sym (l) == T[E_UMINUS] ||
      file_sym (l) == T[E_NOT] ||
      file_sym (l) == T[E_COMPLEMENT] ||
      file_sym (l) == T[E_CONCAT] ||
      (expr_parse_newtokens && (*expr_parse_newtokens)(l))) {
    return 1;
  }
  return 0;
}

static Expr *expr_parse (void);

/*
  Helper functions for parser
*/
static Expr *newexpr (void)
{
  Expr *e;
  NEW (e, Expr);
  e->u.e.l = NULL;
  e->u.e.r = NULL;
  return e;
}

/* -- full base case for anything -- */
static Expr *expr_basecase (void)
{
  Expr *e, *f;

  if (strcmp (file_tokenstring (Tl), "true") == 0) {
    file_getsym (Tl);
    e = newexpr ();
    e->type = E_TRUE;
  }
  else if (strcmp (file_tokenstring (Tl), "false") == 0) {
    file_getsym (Tl);
    e = newexpr ();
    e->type = E_FALSE;
  }
  else if (file_have (Tl, f_integer)) {
    e = newexpr ();
    e->type = E_INT;
    e->u.ival.v = file_integer (Tl);
    e->u.ival.v_extra = NULL;
  }
  else if (file_have (Tl, f_real)) {
    e = newexpr ();
    e->type = E_REAL;
    e->u.f = file_real (Tl);
  }
  else if (expr_parse_basecase_extra &&
	   ((e = (*expr_parse_basecase_extra)(Tl)))) {
    return e;
  }
  else {
    if (file_sym (Tl) == T[E_LPAR]) {
      PUSH (Tl);
      file_getsym (Tl);
      e = expr_parse ();
      if (file_have (Tl, T[E_RPAR])) {
	POP (Tl);
      }
      else {
	SET (Tl);
	POP (Tl);
	expr_free (e);
	return NULL;
      }
    }
    else {
      pId *v;
      
      PUSH (Tl);
      if (expr_parse_id && (v = ((*expr_parse_id) (Tl))) &&
	  (file_sym (Tl) != T[E_LPAR])) {
	int flg; 
	e = newexpr ();
	e->type = E_VAR;
	e->u.e.l = (Expr *) v;

	/* optional bitfield extraction */
	flg = file_flags (Tl);
	file_setflags (Tl, flg | FILE_FLAGS_NOREAL);
	
	if (file_have (Tl, T[E_CONCAT])) {
	  Expr *bf;
	  /* { constexpr .. constexpr } | { constexpr } */
	  f = expr_parse ();
	  if (!f) {
	    SET (Tl); 
	    POP (Tl);
	    expr_free (e);
	    file_setflags (Tl, flg);
	    return NULL;
	  }
	  NEW (bf, Expr);
	  bf->type = E_BITFIELD;
	  bf->u.e.l = f;
	  bf->u.e.r = NULL;
	  if (file_have (Tl, T[E_BITFIELD])) {
	    f = expr_parse ();
	    if (!f) {
	      SET (Tl);
	      POP (Tl);
	      expr_free (e);
	      expr_free (bf);
	      file_setflags (Tl, flg);
	      return NULL;
	    }
	    bf->u.e.r = f;
	  }
	  if (!file_have (Tl, T[E_END])) {
	    SET (Tl);
	    POP (Tl);
	    expr_free (e);
	    expr_free (bf);
	    file_setflags (Tl, flg);
	    return NULL;
	  }
	  file_setflags (Tl, flg);
	  e->type = E_BITFIELD;
	  e->u.e.r = bf;
	  f = e->u.e.r->u.e.l;
	  e->u.e.r->u.e.l = e->u.e.r->u.e.r;
	  e->u.e.r->u.e.r = f;
	}
	else {
	  file_setflags (Tl, flg);
	}
	POP (Tl);
      } 
      else if (file_sym (Tl) == T[E_CONCAT]) {
	file_getsym (Tl);
	/* concatenation:
	   { expr, expr, expr, expr, expr, ... }
	*/
	Expr *ret;
	ret = e = newexpr ();
	e->type = E_CONCAT;
	e->u.e.l = expr_parse ();
      
	if (!e->u.e.l) {
	  expr_free (ret);
	  SET (Tl);
	  POP (Tl);
	  return NULL;
	}
	while (file_have (Tl, T[E_COMMA])) {
	  e->u.e.r = newexpr ();
	  e = e->u.e.r;
	  e->type = E_CONCAT;
	  e->u.e.l = expr_parse ();
	  e->u.e.r = NULL;
	  if (!e->u.e.l) {
	    expr_free (ret);
	    SET (Tl);
	    POP (Tl);
	    return NULL;
	  }
	}
	if (file_have (Tl, T[E_END])) {
	  e = ret;
	  if (ret->u.e.r) {
	    POP (Tl);
	  }
	  else {
	    expr_free (ret);
	    SET (Tl);
	    POP (Tl);
	    return NULL;
	  }
	}
	else {
	  expr_free (ret);
	  SET (Tl);
	  POP (Tl);
	  return NULL;
	}
      }
      else {
	if (v && expr_free_id) {
	  (*expr_free_id) (v);
	}
	SET (Tl);
	if (file_have (Tl, f_id) && file_sym (Tl) == T[E_LPAR]) {
	  e = newexpr ();
	  e->type = E_FUNCTION;
	  e->u.fn.s = Strdup (file_prev (Tl));
	  e->u.fn.r = NULL;
	  f = e;
	  file_mustbe (Tl, T[E_LPAR]);
	  if (file_sym (Tl) != T[E_RPAR]) {
	    do {
	      f->u.e.r = newexpr (); /* wow! rely that this is the same
                                      space as u.fn.r */
	      f = f->u.e.r;
	      f->type = E_LT;
	      f->u.e.r = NULL;
	      f->u.e.l = expr_parse ();
	      if (!f->u.e.l) {
		expr_free (e);
		SET (Tl);
		POP (Tl);
		return NULL;
	      }
	    } while (file_have (Tl, T[E_COMMA]));
	    if (file_sym (Tl) != T[E_RPAR]) {
	      expr_free (e);
	      SET (Tl);
	      POP (Tl);
	      return NULL;
	    }
	  }
	  /* success! */
	  POP (Tl);
	  file_getsym (Tl);
	}
	else {
	  SET (Tl);
	  POP (Tl);
	  return NULL;
	}
      }
    }
  }
  return e;
}


static int _inv_tok (int sym)
{
  int i;
  for (i=0; i < E_NUMBER; i++) {
    if (T[i] == sym) {
      if (i == E_NOT) {
	i = E_COMPLEMENT;
      }
      return i;
    }
  }
  return -1;
}


static void _release (list_t *opstk, list_t *resstk)
{
  listitem_t *li;
  
  list_free (opstk);

  for (li = list_first (resstk); li; li = list_next (li)) {
    Expr *tmp = (Expr *) list_value (li);
    expr_free (tmp);
  }
  list_free (resstk);
}

#if 0
static int _exactly_one (list_t *l)
{
  if (list_isempty (l)) return 0;
  if (list_next (list_first (l)) == NULL) return 1;
  return 0;
}
#endif

static int _unary_op (int tok)
{
  return (tok == E_NOT || tok == E_COMPLEMENT || tok == E_UMINUS) ? 1 : 0;
}

/* return 1 if success, 0 if fail, -1 if early termination */
static int _unwind_op (int top, list_t *opstk, list_t *resstk)
{
  Expr *l, *r;
  Expr *e;

  if (list_isempty (resstk)) {
    SET (Tl);
    POP (Tl);
    _release (opstk, resstk);
    return 0;
  }

  r = (Expr *) stack_pop (resstk);

  if (_unary_op (top)) {
    e = newexpr ();
    e->type = top;
    e->u.e.l = r;
    stack_push (resstk, e);
    return 1;
  }
  
  if (list_isempty (resstk)) {
    SET (Tl);
    POP (Tl);
    expr_free (r);
    _release (opstk, resstk);
    return 0;
  }    
  l = (Expr *) stack_pop (resstk);
  
  e = newexpr ();
  e->type = top;
  e->u.e.l = l;
  e->u.e.r = r;

  stack_push (resstk, e);

  return 1;
}

static int _stack_top_op (list_t *stk)
{
  if (list_isempty (stk)) {
    return -1;
  }
  else {
    return list_ivalue (list_first (stk));
  }
}

//#define EXPR_VERBOSE

#ifdef EXPR_VERBOSE
static void _print_tok (int tok)
{
  printf ("%d (%s) prec: %d",
	  tok, tok == -1 ? "x" : file_tokenname (Tl, T[tok]),
	  tok == -1 ? -2 : _prec_table[tok]);
}
#endif

static Expr *expr_parse (void)
{
  list_t *stk_op, *stk_res;
  Expr *e;
  int top_op;
#ifdef EXPR_VERBOSE
  static int depth = 0;
#endif  
  int query_op = 0;
  int last_was_tok = 1;

#ifdef EXPR_VERBOSE
  printf ("[%d] >>start\n", ++depth);
#endif  

  PUSH (Tl);
  
  stk_op = list_new ();
  stk_res = list_new ();

  while (1) {
    int tok = _inv_tok (file_sym (Tl));
 
    if (tok == E_QUERY) {
      query_op++;
    }
    else if (tok == E_COLON) {
      query_op--;
    }
    else if (tok == E_MINUS && last_was_tok) {
      tok = E_UMINUS;
    }
    
    if (query_op < 0) {
       break;
    }

#ifdef EXPR_VERBOSE
    printf ("[%d] looking-at `%s': ", depth, file_tokenstring (Tl));
    _print_tok (tok);
    printf ("\n");
#endif

    if (tok == -1) {
      last_was_tok = 0;
    }
    else {
      last_was_tok = 1;
    }

    if (tok != -1 && (_prec_table[tok] < 0)) {
      tok = -1;
      last_was_tok = 0;
#if 0
      printf (" --> not an op!\n");
#endif      
    }
    
    if (tok == -1 /* not an expression operator */ ) {
      /* is it one of the base cases? */

#if 0
      printf ("[%d] try basecase\n", depth);
#endif      
      e = expr_basecase ();
      if (!e) {
#if 0
	printf (" --> not a base case!\n");
#endif	
	/*-- no something completely different, quit -- */
	break;
      }
#ifdef EXPR_VERBOSE
      printf ("[%d] push base\n", depth);
#endif      
      stack_push (stk_res, e);
    }
    else {
      /* check out the top of the operand stack */
      top_op = _stack_top_op (stk_op);
      file_getsym (Tl);

#ifdef EXPR_VERBOSE
      printf ("[%d]      top-of-stack: ", depth);
      _print_tok (top_op);
      printf ("\n");
#endif
      while (top_op != -1 &&
	     ((_prec_table[top_op] > _prec_table[tok]) ||
	      (_prec_table[top_op] == _prec_table[tok] &&
	       top_op != E_QUERY && top_op != E_COLON))) {
	/* while there's something on the top of the op stack that
	   has higher precedence than the input token, unwind the stack */

	if (!_unwind_op (top_op, stk_op, stk_res)) {
#ifdef EXPR_VERBOSE
	  printf ("[%d] <fail>\n", depth--);
#endif	  
	  return NULL;
	}
#ifdef EXPR_VERBOSE
	printf ("[%d] unwind op: ", depth);
	_print_tok (top_op);
	printf ("\n");
#endif

	if (top_op == E_COLON) {
	  stack_pop (stk_op);

	  top_op = _stack_top_op (stk_op);
	  
	  if (top_op != E_QUERY) {
	    SET (Tl);
	    POP (Tl);
	    _release (stk_op, stk_res);
#ifdef EXPR_VERBOSE
	    printf ("[%d] <fail>\n", depth--);
#endif	    
	    return NULL;
	  }
	  if (!_unwind_op (top_op, stk_op, stk_res)) {
#ifdef EXPR_VERBOSE
	    printf ("[%d] <fail>\n", depth--);
#endif	    
	    return NULL;
	  }
	}
	else if (top_op == E_QUERY) {
	  SET (Tl);
	  POP (Tl);
	  _release (stk_op, stk_res);
#ifdef EXPR_VERBOSE
	  printf ("[%d] <fail>\n", depth--);
#endif	  
	  return NULL;
	}

	/* handled the top operand */
	stack_pop (stk_op);
	top_op = _stack_top_op (stk_op);
      }
      if (tok == E_COLON && _stack_top_op (stk_op) == E_COLON) {
	if (!_unwind_op (top_op, stk_op, stk_res)) {
#ifdef EXPR_VERBOSE
	  printf ("[%d] <fail>\n", depth--);
#endif
	  return NULL;
	}
	stack_pop (stk_op);
	top_op = _stack_top_op (stk_op);
	if (top_op != E_QUERY) {
	  SET (Tl);
	  POP (Tl);
	  _release (stk_op, stk_res);
#ifdef EXPR_VERBOSE
	  printf ("[%d] <fail>\n", depth--);
#endif
	  return NULL;
	}
	if (!_unwind_op (top_op, stk_op, stk_res)) {
#ifdef EXPR_VERBOSE
	  printf ("[%d] <fail>\n", depth--);
#endif
	  return NULL;
	}
	stack_pop (stk_op);
	top_op = _stack_top_op (stk_op);
      }
#ifdef EXPR_VERBOSE
      printf ("[%d] push-op: ", depth);
      _print_tok (tok);
      printf ("\n");
#endif      
      stack_ipush (stk_op, tok);
    }
  }

  /* check out the top of the operand stack */
  top_op = _stack_top_op (stk_op);

  /* unwind the rest of the operations */
  while (top_op != -1) {
    if (!_unwind_op (top_op, stk_op, stk_res)) {
#ifdef EXPR_VERBOSE
      printf ("[%d] <fail>\n", depth--);
#endif      
      return NULL;
    }

#ifdef EXPR_VERBOSE
    printf ("[%d] unwind op: ", depth);
    _print_tok (top_op);
    printf ("\n");
#endif    

    if (top_op == E_COLON) {
      stack_pop (stk_op);

      top_op = _stack_top_op (stk_op);
	  
      if (top_op != E_QUERY) {
	SET (Tl);
	POP (Tl);
	_release (stk_op, stk_res);
#ifdef EXPR_VERBOSE
	printf ("[%d] <fail>\n", depth--);
#endif	
	return NULL;
      }
      if (!_unwind_op (top_op, stk_op, stk_res)) {
#ifdef EXPR_VERBOSE
	printf ("[%d] <fail>\n", depth--);
#endif	
	return NULL;
      }
#ifdef EXPR_VERBOSE
      printf ("[%d] unwind op: ", depth);
      _print_tok (top_op);
      printf ("\n");
#endif      
    }
    else if (top_op == E_QUERY) {
      SET (Tl);
      POP (Tl);
      _release (stk_op, stk_res);
#ifdef EXPR_VERBOSE
      printf ("[%d] <fail>\n", depth--);
#endif      
      return NULL;
    }
    
    /* handled the top operand */
    stack_pop (stk_op);
    top_op = _stack_top_op (stk_op);
  }
  
  if (list_length (stk_op) != 0 || list_length (stk_res) != 1) {
    SET (Tl);
    POP (Tl);
    _release (stk_op, stk_res);
#ifdef EXPR_VERBOSE
    printf ("[%d] <fail>\n", depth--);
#endif    
    return NULL;
  }
  e = (Expr *) stack_pop (stk_res);
  _release (stk_op, stk_res);
#ifdef EXPR_VERBOSE
  printf ("[%d] success!\n", depth--);
#endif
  POP (Tl);
  return e;
}

#ifdef EXPR_VERBOSE
static void expr_print (Expr *e)
{
  if (!e) return;
  switch (e->type) {
  case E_AND: case E_OR: case E_PLUS: case E_MINUS:
  case E_MULT: case E_DIV: case E_MOD: case E_LSL:
  case E_XOR: case E_LT: case E_GT: case E_LE: case E_GE:
  case E_EQ: case E_NE:
  case E_LSR: case E_ASR:
    printf ( "(");
    expr_print (e->u.e.l);
    printf ( " %s ", file_tokenname (Tl, T[e->type]));
    expr_print (e->u.e.r);
    printf (")");
    break;

  case E_NOT:
    printf ("!(");
    expr_print (e->u.e.l);
    printf ( ")");
    break;

  case E_COMPLEMENT:
    printf ("~(");
    expr_print (e->u.e.l);
    printf (")");
    break;

  case E_UMINUS:
    printf ( "-(");
    expr_print ( e->u.e.l);
    printf ( ")");
    break;
    
  case E_INT:
    printf ("%lu", e->u.ival.v);
    break;

  case E_QUERY:
    printf ( "(");
    expr_print ( e->u.e.l);
    printf ( "?");
    expr_print ( e->u.e.r->u.e.l);
    printf ( ":");
    expr_print ( e->u.e.r->u.e.r);
    printf ( ")");
    break;

  case E_TRUE:
    printf ( "true");
    break;

  case E_FALSE:
    printf ("false");
    break;

  case E_VAR:
    printf ("[var %p]", e->u.e.l);
    break;

  case E_FUNCTION:
    printf ("[func %s]", e->u.fn.s);
    break;

  case 58:
    printf ("int(");
    expr_print (e->u.e.l);
    if (e->u.e.r) {
      printf (",");
      expr_print (e->u.e.r);
    }
    printf (")");
    break;

  default:
    fatal_error ("Unhandled case %d!\n", e->type);
    break;
  }
}
#endif

  
void *act_parse_a_fexpr (LFILE *l)
{
  int count = 0;
  Expr *e;

  INIT2 (E_LSR, ">>");

  file_push_position (l);
  
  Tl = l;
  e = expr_parse ();
  
  if (count) {
    file_deltoken (l, ">>");
  }

  if (!e) {
    act_parse_seterr (l, "Error parsing expression");
    file_set_position (l);
  }
  file_pop_position (l);

#ifdef EXPR_VERBOSE
  if (e) {
    printf ("GOT: ");
    expr_print (e);
    printf ("\n");
  }
#endif

  return e;
}

void act_free_a_fexpr (void *v)
{
  expr_free ((Expr *)v);
}

void *act_walk_X_fexpr (ActTree *t, void *v)
{
  return act_walk_X_expr (t, (Expr *)v);
}
