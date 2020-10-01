/*************************************************************************
 *
 *  Standard expression parsing library
 *
 *  Copyright (c) 1999-2010, 2019 Rajit Manohar
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
#include "expr.h"
#include "file.h"
#include "misc.h"

static int T[E_NUMBER] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1
};

pId *(*expr_parse_id)(LFILE *l) = NULL;
void (*expr_free_id) (void *) = NULL;
void (*expr_print_id)(pp_t *, void *) = NULL;
void (*expr_print_probe)(pp_t *, void *) = NULL;
Expr *(*expr_parse_basecase_num)(LFILE *l) = NULL;
Expr *(*expr_parse_basecase_bool)(LFILE *l) = NULL;
int (*expr_parse_newtokens)(LFILE *l) = NULL;

#define PUSH(x) file_push_position(x)
#define POP(x)  file_pop_position(x)
#define SET(x)  file_set_position(x)
#define INFO(x)

static int paren_count = 0;

static LFILE *Tl;

static int end_gt_mode = 0;

void expr_inc_parens (void)
{
  paren_count++;
}

void expr_dec_parens (void)
{
  paren_count--;
}

int expr_gettoken (int type)
{
  return T[type];
}

void expr_endgtmode (int v)
{
  end_gt_mode = v;
}

/*------------------------------------------------------------------------
 *
 *  expr_clear --
 *
 *   Clear tokens
 *
 *------------------------------------------------------------------------
 */
void
expr_clear (void)
{
  int i;

  for (i=0; i < E_NUMBER; i++)
    T[i] = -1;

}

/*------------------------------------------------------------------------
 *
 *  expr_init --
 *
 *   Initialize expression parser with standard tokens. Returns the
 *  number of tokens added.
 *
 *------------------------------------------------------------------------
 */
int expr_init (LFILE *l)
{
  int count = 0;

  expr_clear ();

#define INIT(tok,str)   count += !file_istoken (l, str);  expr_settoken(tok, file_addtoken (l, str))

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
#if 0
  INIT (E_LSR, ">>");
#endif
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
  
  return count;
}

/*------------------------------------------------------------------------
 *------------------------------------------------------------------------
 *
 *  (F is of type INT)
 *
 *          I ::= BE ? I : I | H
 *          H ::= HH | HH | H
 *          HH::= G | G ^ HH
 *          G ::= FF | FF & G 

 *      inequalities go here
 *          FF ::= F | F < F | F > F | F <= F | F >= F | F = F | F != F

 *          F ::= U << F | U >> F
 *          U ::= V + U | V - U | V
 *          V ::= W * V | W / V | W % V | W
 *          W ::= ID | -ID | (F) | -(F) | CONST
 *
 *   Bools:
 *          
 *          BE ::= B2 | B2 | BE
 *          B2 ::= B1 | B1 ^ B2
 *          B1 ::= B0 | B0 & B1
 *          B0 ::= B | F < F | F > F | F <= F | F >= F | F == F | F != F
 *          B ::= ID | ~ID | (BE) | ~(BE) | true | false | ~#BE | #BE
 *
 *------------------------------------------------------------------------
 *------------------------------------------------------------------------
 */


/*------------------------------------------------------------------------
 *
 *  expr_settokens --
 *
 *   Set parsing tokens
 *
 *------------------------------------------------------------------------
 */
void expr_settoken (int x, int v)
{
  Assert (0 <= x && x < E_NUMBER, "Invalid token number");
  T[x] = v;
}


int expr_parse_isany (LFILE *l)
{
  if (file_sym (l) == f_id || file_sym (l) == f_integer ||
      file_sym (l) == f_real ||
      file_sym (l) == T[E_LPAR] ||
      file_sym (l) == T[E_UMINUS] ||
      file_sym (l) == T[E_NOT] ||
      file_sym (l) == T[E_PROBE] ||
      file_sym (l) == T[E_CONCAT] ||
      (expr_parse_newtokens && (*expr_parse_newtokens)(l))) {
    return 1;
  }
  return 0;
}  


/*
  Helper functions for parser
*/
static Expr *BE (void);
static Expr *I (void);
static int int_real_only;

static Expr *newexpr (void)
{
  Expr *e;
  NEW (e, Expr);
  e->u.e.l = NULL;
  e->u.e.r = NULL;
  return e;
}

static void efree (Expr *e)
{
  if (!e) return;

  switch (e->type) {
  case E_INT:
  case E_TRUE:
  case E_FALSE:
  case E_REAL:
    break;

  case E_FUNCTION:
    FREE (e->u.fn.s);
    efree (e->u.fn.r);
    break;

  case E_VAR:
  case E_PROBE:
    if (expr_free_id)
      (*expr_free_id) (e->u.e.l);
    break;

  case E_BITFIELD:
    if (expr_free_id)
      (*expr_free_id) (e->u.e.l);
    FREE (e->u.e.r);
    break;

  case E_RAWFREE:
    if (e->u.e.l)  FREE (e->u.e.l);
    if (e->u.e.r) efree (e->u.e.r);
    break;

  default:
    if (e->u.e.l) efree (e->u.e.l);
    if (e->u.e.r) efree (e->u.e.r);
    break;
  }
  FREE (e);
  return;
}

static int const_intexpr (Expr *e, unsigned long *v)
{
  unsigned long l, r;
  int bl, br;
  if (!e) return 0;

#define CONSTBINOP(typ,op)			\
  case typ:					\
    bl = const_intexpr (e->u.e.l, &l);		\
    if (!bl) return 0;				\
    br = const_intexpr (e->u.e.r, &r);		\
    if (!br) return 0;				\
    *v = l op r;				\
    return 1;					\
    break

  switch (e->type) {
    CONSTBINOP(E_AND,&);
    CONSTBINOP(E_OR,|);
    CONSTBINOP(E_PLUS,+);
    CONSTBINOP(E_MINUS,-);
    CONSTBINOP(E_MULT,*);
    CONSTBINOP(E_DIV,/);
    CONSTBINOP(E_MOD,%);

  case E_INT:
    *v = e->u.v;
    return 1;
    break;

  default:
    return 0;
  }
}

void expr_free (Expr *e) { efree (e); }

/* base case */
Expr *B (void)
{
  int not = 0;
  Expr *e, *f;
  pId *v;

  PUSH (Tl);
  while (file_have (Tl, T[E_NOT]))
    not = 1 - not;
  if (expr_parse_basecase_bool && ((e = (*expr_parse_basecase_bool)(Tl)))) {
    /* ok! */
    POP (Tl);
  }
  else {
    if (file_have (Tl, T[E_LPAR])) {
      paren_count++;
      e = BE ();
      paren_count--;
      if (file_have (Tl, T[E_RPAR])) {
	POP (Tl);
      }
      else {
	SET (Tl);
	POP (Tl);
	efree (e);
	return NULL;
      }
    }
    else if (strcmp (file_tokenstring (Tl), "true") == 0) {
      POP (Tl);
      file_getsym (Tl);
      e = newexpr ();
      e->type = E_TRUE;
      e->u.v = 1;
    }
    else if (strcmp (file_tokenstring (Tl), "false") == 0) {
      POP (Tl);
      file_getsym (Tl);
      e = newexpr ();
      e->type = E_FALSE;
      e->u.v = 0;
    }
    else {
      if (expr_parse_id) {
	v = (*expr_parse_id) (Tl);
      }
      else {
	v = NULL;
      }
      if (expr_parse_id && v) {
	POP (Tl);
	e = newexpr ();
	e->type = E_VAR;
	e->u.e.l = (Expr *) v;
      } 
      else if (file_have (Tl, T[E_PROBE])) {
	if (expr_parse_id && (v = (*expr_parse_id)(Tl))) {
	  POP (Tl);
	  e = newexpr ();
	  e->type = E_PROBE;
	  e->u.e.l = (Expr *)v;
	}
	else {
	  SET (Tl);
	  POP (Tl);
	  return NULL;
	}
      }
      else {
	SET (Tl);
	POP (Tl);
	return NULL;
      }
    }
  }
  if (not) {
    f = e;
    e = newexpr ();
    e->u.e.l = f;
    e->type = E_NOT;
  }
  return e;
}


/*
 *  For rules of the form:
 *
 *    cur ::= next | next TOKEN cur
 */
#define EMIT_BINOP(token,cur,next)		\
   static Expr *cur (void)			\
   {						\
     Expr *e, *f;				\
						\
     e = next ();				\
     if (!e || T[token] == -1) return e;	\
						\
     while (file_have (Tl, T[token])) {		\
       f = newexpr ();				\
       f->type = token;				\
       f->u.e.l = e;				\
       f->u.e.r = next ();			\
       if (!f->u.e.r) {				\
	 efree (f);				\
	 return NULL;				\
       }					\
       e = f;					\
     }						\
     return e;					\
   }

/*
 *  For rules of the form:
 *
 *    cur ::= next | next OP cur   
 */
#define EMIT_BINOP_SEQ(array,cur,next)			\
   static Expr *cur (void)				\
   {							\
     Expr *e, *f;					\
     int i;						\
							\
     e = next ();					\
     if (!e) return e;					\
     for (i=0; i < sizeof(array)/sizeof(array[0]); i++)	\
       if (T[array[i]] != -1)				\
	 break;						\
     if (i == sizeof(array)/sizeof(array[0])) return e; \
							\
   myloop:						\
     for (i=0; i < sizeof(array)/sizeof(array[0]); i++)	\
       if (T[array[i]] == file_sym (Tl))		\
	 break;						\
     if (i == sizeof(array)/sizeof(array[0])) return e;	\
     f = newexpr ();					\
     f->type = array[i];				\
     f->u.e.l = e;					\
     file_getsym (Tl);					\
     f->u.e.r = next ();				\
     if (!f->u.e.r) {					\
       efree (f);					\
       return NULL;					\
     }							\
     e = f;						\
     goto myloop;					\
   }


/*
 *  for rules of the form:
 *
 *     cur ::= other | next OP next
 */
#define EMIT_BINOP_SEQ1(array,cur,next,other)		\
   static Expr *cur (void)				\
   {							\
     Expr *e, *f;					\
     int i;						\
							\
     PUSH (Tl);						\
     e = next ();					\
     if (!e) {						\
        SET (Tl);					\
        POP (Tl);					\
        return other();					\
     }							\
     for (i=0; i < sizeof(array)/sizeof(array[0]); i++)	\
       if (T[array[i]] != -1)				\
	 break;						\
     if (i == sizeof(array)/sizeof(array[0])) {		\
          SET (Tl);					\
          POP (Tl);					\
          efree (e);					\
          return other();				\
     }							\
     for (i=0; i < sizeof(array)/sizeof(array[0]); i++)	\
       if (T[array[i]] == file_sym (Tl))		\
	 break;						\
     if (i == sizeof(array)/sizeof(array[0])) {		\
          SET (Tl);					\
          POP (Tl);					\
          efree (e);					\
          return other();				\
     }							\
     f = newexpr ();					\
     f->type = array[i];				\
     f->u.e.l = e;					\
     file_getsym (Tl);					\
     f->u.e.r = next ();				\
     if (!f->u.e.r) {					\
       SET (Tl);					\
       POP (Tl);					\
       efree (f);					\
       return other();					\
     }							\
     POP (Tl);						\
     e = f;						\
     return e;						\
   }


static int _intcomp[] = { E_LT, E_GT, E_LE, E_GE, E_EQ, E_NE };
static int _shifts[] = { E_LSL, E_LSR, E_ASR };
static int _addsub[] = { E_PLUS, E_MINUS };
static int _muldiv[] = { E_MULT, E_DIV, E_MOD };

static Expr *F (void);

static Expr *FF (void)				
{							
  Expr *e, *f;					
  int i;
  int tmp = int_real_only;

  if (int_real_only) {
    return F();
  }
							
  PUSH (Tl);
  e = F ();
  if (!e) {
    SET (Tl);
    POP (Tl);
    return NULL;
  }
  for (i=0; i < sizeof(_intcomp)/sizeof(_intcomp[0]); i++)
    if (T[_intcomp[i]] != -1)				
      break;						
  if (i == sizeof(_intcomp)/sizeof(_intcomp[0])) {
    POP (Tl);
    return e;
  }							
  for (i=0; i < sizeof(_intcomp)/sizeof(_intcomp[0]); i++)	
    if (T[_intcomp[i]] != -1 && T[_intcomp[i]] == file_sym (Tl))
      break;
  if (i == sizeof(_intcomp)/sizeof(_intcomp[0])) {
    POP (Tl);
    return e;
  }
  f = newexpr ();
  f->type = _intcomp[i];
  f->u.e.l = e;
  file_getsym (Tl);
  int_real_only = 1;
  f->u.e.r = F ();
  int_real_only = tmp;
  if (!f->u.e.r) {
    FREE (f);
    efree (e);
    SET (Tl);
    POP (Tl);
    return F();
  }
  /* success. But if it is endgt mode, and we have ">" and the
     following token is a ";" or ",", we undo this and only return the
     left pointer! */
  if ((f->type == E_GT) && (end_gt_mode == 1)) {
    int have_comma = 0;
    int have_semi = 0;
    int comma_token = -1;
    int semi_token = -1;
 
    have_comma = file_istoken (Tl, ",");
    if (have_comma) {
      comma_token = file_addtoken (Tl, ",");
    }
    have_semi = file_istoken (Tl, ";");
    if (have_semi) {
      semi_token = file_addtoken (Tl, ";");
    }
    if ((have_comma && (file_sym (Tl) == comma_token)) ||
	(have_semi && (file_sym (Tl) == semi_token)) ||
	(file_sym (Tl) == T[E_RPAR] && paren_count == 0)) {
      /* unwind */
      SET (Tl);
      POP (Tl);
      efree (f);
      return F ();
    }
  }
  POP (Tl);						
  e = f;						
  return e;
}



//EMIT_BINOP_SEQ1 (_intcomp,FF,F,F)
EMIT_BINOP_SEQ1 (_intcomp,B0,F,B)
EMIT_BINOP (E_AND, B1, B0)
EMIT_BINOP (E_XOR, B2, B1)
EMIT_BINOP (E_OR, BE, B2)

/* base case */
static Expr *W (void)
{
  int uminus = 0;
  int tilde = 0;
  Expr *e, *f;
  void *v;
  unsigned long x, y;

  PUSH (Tl);
  if (file_sym (Tl) == T[E_UMINUS]) {
    while (file_have (Tl, T[E_UMINUS]))
      uminus = 1 - uminus;
  }
  else if (file_sym (Tl) == T[E_COMPLEMENT]) {
    while (file_have (Tl, T[E_COMPLEMENT]))
      tilde = 1 - tilde;
  }

  if (expr_parse_basecase_num && ((e = (*expr_parse_basecase_num)(Tl)))) {
    /* ok! */
    POP (Tl);
  }
  else {
    if (file_have (Tl, T[E_LPAR])) {
      paren_count++;
      e = I();
      paren_count--;
      if (file_have (Tl, T[E_RPAR])) {
	POP (Tl);
      }
      else {
	SET (Tl);
	POP (Tl);
	efree (e);
	return NULL;
      }
    }
    else if (file_have (Tl, f_integer)) {
      POP (Tl);
      e = newexpr ();
      e->type = E_INT;
      if (uminus) {
	e->u.v = -file_integer (Tl);
      }
      else if (tilde) {
	e->u.v = ~file_integer (Tl);
      }
      else {
	e->u.v = file_integer (Tl);
      }
      uminus = 0;
      tilde = 0;
    }
    else if (file_have (Tl, f_real)) {
      POP (Tl);
      e = newexpr ();
      e->type = E_REAL;
      if (uminus) {
	e->u.f = -file_real (Tl);
      }
      else {
	e->u.f = file_real (Tl);
      }
      uminus = 0;
    }
    else if (strcmp (file_tokenstring (Tl), "true") == 0) {
      POP (Tl);
      file_getsym (Tl);
      e = newexpr ();
      e->type = E_TRUE;
      e->u.v = 1;
    }
    else if (strcmp (file_tokenstring (Tl), "false") == 0) {
      POP (Tl);
      file_getsym (Tl);
      e = newexpr ();
      e->type = E_FALSE;
      e->u.v = 0;
    }
    else if (expr_parse_id && (v = (*expr_parse_id)(Tl)) && 
	     /* not a function call */ (file_sym (Tl) != T[E_LPAR])) {
      int flg; 
      e = newexpr ();
      e->type = E_VAR;
      e->u.e.l = (Expr *) v;
      /* optional bitfield extraction */
      flg = file_flags (Tl);
      file_setflags (Tl, flg | FILE_FLAGS_NOREAL);
      if (file_have (Tl, T[E_CONCAT])) {
	/* { constexpr .. constexpr } | { constexpr } */
	f = I();
	if (!const_intexpr (f, &x)) {
	  SET (Tl); 
	  POP (Tl);
	  efree (e);
	  file_setflags (Tl, flg);
	  return NULL;
	}
	efree (f);
	y = x;
	if (file_have (Tl, T[E_BITFIELD])) {
	  f = I();
	  if (!const_intexpr (f, &y)) {
	    SET (Tl);
	    POP (Tl);
	    efree (e);
	  }
	  efree (f);
	}
	if (!file_have (Tl, T[E_END])) {
	  SET (Tl);
	  POP (Tl);
	  efree (e);
	  file_setflags (Tl, flg);
	  return NULL;
	}
	file_setflags (Tl, flg);
	f = newexpr ();
	f->u.e.l = (void *)x;
	f->u.e.r = (void *)y;
	f->type = E_BITFIELD;
	e->type = E_BITFIELD;
	e->u.e.r = f;
#if 0
	if (y > x) {
	  warning ("Bitfield operation {%d..%d} needs %d <= %d\n",
		   (int)x, (int)y, (int)x, (int)y);
	  efree (e);
	  SET (Tl);
	  POP (Tl);
	  return NULL;
	}
#endif
	f = e->u.e.r->u.e.l;
	e->u.e.r->u.e.l = e->u.e.r->u.e.r;
	e->u.e.r->u.e.r = f;
	POP (Tl);
      }
      else {
	file_setflags (Tl, flg);
	POP (Tl);
      }
    }
    else if (file_have (Tl, T[E_PROBE])) {
      if (expr_parse_id && (v = (*expr_parse_id)(Tl))) {
	POP (Tl);
	e = newexpr ();
	e->type = E_PROBE;
	e->u.e.l = (Expr *)v;
      }
      else {
	SET (Tl);
	POP (Tl);
	return NULL;
      }
    }
    else if (file_have (Tl, T[E_CONCAT])) {
      /* concatenation:
	 { expr, expr, expr, expr, expr, ... }
      */
      Expr *ret;
      ret = e = newexpr ();
      e->type = E_CONCAT;
      e->u.e.l = I();
      
      if (!e->u.e.l) {
	efree (ret);
	SET (Tl);
	POP (Tl);
	return NULL;
      }
      while (file_have (Tl, T[E_COMMA])) {
	e->u.e.r = newexpr ();
	e = e->u.e.r;
	e->type = E_CONCAT;
	e->u.e.l = I();
	e->u.e.r = NULL;
	if (!e->u.e.l) {
	  efree (ret);
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
	  efree (ret);
	  SET (Tl);
	  POP (Tl);
	  return NULL;
	}
      }
      else {
	efree (ret);
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
	paren_count++;
	if (file_sym (Tl) != T[E_RPAR]) {
	  do {
	    f->u.e.r = newexpr (); /* wow! rely that this is the same
				      space as u.fn.r */
	    f = f->u.e.r;
	    f->type = E_LT;
	    f->u.e.r = NULL;
	    f->u.e.l = I (); /* int expr, argument */
	    if (!f->u.e.l) {
              paren_count--;
	      efree (e);
	      SET (Tl);
	      POP (Tl);
	      return NULL;
	    }
	  } while (file_have (Tl, T[E_COMMA]));
	  paren_count--;
	  if (file_sym (Tl) != T[E_RPAR]) {
	    efree (e);
	    SET (Tl);
	    POP (Tl);
	    return NULL;
	  }
	}
        else {
           paren_count--;
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
  if (uminus || tilde) {
    f = e;
    e = newexpr ();
    e->u.e.l = f;
    if (uminus) {
      e->type = E_UMINUS;
    }
    else {
      e->type = E_COMPLEMENT;
    }
  }
  return e;
}

static Expr *G (void);

EMIT_BINOP_SEQ (_muldiv, V, W);
EMIT_BINOP_SEQ (_addsub, U, V);
EMIT_BINOP_SEQ (_shifts, F, U);
//EMIT_BINOP (E_AND, G, F);
EMIT_BINOP (E_XOR, HH, G);
EMIT_BINOP (E_OR, H, HH);

static Expr *G (void)
{
  Expr *e, *f;

  if (int_real_only) {
    e = F ();
  }
  else {
    e = FF ();
  }
  if (!e || T[E_AND] == -1) return e;
  
  while (file_have (Tl, T[E_AND])) {
    f = newexpr ();
    f->type = E_AND;
    f->u.e.l = e;
    if (int_real_only) {
      f->u.e.r = F ();
    }
    else {
      f->u.e.r = FF ();
    }
    if (!f->u.e.r) {
      efree (f);
      return NULL;
    }
    e = f;
  }
  return e;
}


static Expr *I (void)
{
  Expr *e, *f;


  if (T[E_QUERY] == -1 || T[E_COLON] == -1)
    return H();
  
  PUSH (Tl);
  if ((e = BE()) && file_have (Tl, T[E_QUERY])) {
    f = newexpr ();
    f->type = E_QUERY;
    f->u.e.l = e;
    e = f;
    e->u.e.r = newexpr ();
    e->u.e.r->type = E_COLON;
    e->u.e.r->u.e.l = I();
    if (!e->u.e.r->u.e.l) {
      goto fail;
    }
    if (file_have (Tl, T[E_COLON])) {
      e->u.e.r->u.e.r = I();
      if (!e->u.e.r->u.e.r)
	goto fail;
      POP (Tl);
      return e;
    }
  }
 fail:
  SET (Tl);
  POP (Tl);
  if (e) efree (e);
  return H();
}


/*------------------------------------------------------------------------
 *
 *  expr_parse --
 *
 *   Parse a Boolean expression from the input token stream.
 *
 *------------------------------------------------------------------------
 */
Expr *expr_parse_bool (LFILE *l)
{
  int count = 0;
  Expr *e;

  int_real_only = 0;
  INIT (E_LSR, ">>");

  Tl = l;
  e = BE();

  if (count) {
    file_deltoken (l, ">>");
  }

  return e;
}

Expr *expr_parse_int (LFILE *l)
{
  int count = 0;
  unsigned int flags;
  Expr *x;

  int_real_only = 1;
  flags = file_flags (l);
  file_setflags (l, flags | FILE_FLAGS_NOREAL);

  INIT (E_LSR, ">>");

  Tl = l;
  x = I ();
  file_setflags (l, flags);

  if (count) {
    file_deltoken (l, ">>");
  }
  
  return x;
}

Expr *expr_parse_real (LFILE *l)
{
  int count = 0;
  Expr *e;

  int_real_only = 1;
  INIT (E_LSR, ">>");

  Tl = l;
  e = I();

  if (count) {
    file_deltoken (l, ">>");
  }

  return e;
}


static const char *opsym (int t)
{
  return file_tokenname (Tl, T[t]);
}

const char *expr_operator_name (int t)
{
  return opsym (t);
}

void expr_print (pp_t *pp, Expr *e)
{
  if (!e) return;
  switch (e->type) {
  case E_AND: case E_OR: case E_PLUS: case E_MINUS:
  case E_MULT: case E_DIV: case E_MOD: case E_LSL:
  case E_XOR: case E_LT: case E_GT: case E_LE: case E_GE:
  case E_EQ: case E_NE:
    pp_puts (pp, "(");
    expr_print (pp, e->u.e.l);
    pp_puts (pp, opsym (e->type));
    expr_print (pp, e->u.e.r);
    pp_puts (pp, ")");
    break;

  case E_LSR:
    pp_puts (pp, "(((unsigned long)");
    expr_print (pp, e->u.e.l);
    pp_puts (pp, ")");
    pp_puts (pp, opsym (e->type));
    expr_print (pp, e->u.e.r);
    pp_puts (pp, ")");
    break;
    
  case E_ASR:
    pp_puts (pp, "(((signed long)");
    expr_print (pp, e->u.e.l);
    pp_puts (pp, ")");
    pp_puts (pp, opsym (e->type));
    expr_print (pp, e->u.e.r);
    pp_puts (pp, ")");
    break;

  case E_NOT:
    pp_puts (pp, "!(");
    expr_print (pp, e->u.e.l);
    pp_puts (pp, ")");
    break;

  case E_COMPLEMENT:
    pp_puts (pp, "~(");
    expr_print (pp, e->u.e.l);
    pp_puts (pp, ")");
    break;

  case E_UMINUS:
    pp_puts (pp, "-(");
    expr_print (pp, e->u.e.l);
    pp_puts (pp, ")");
    break;
    
  case E_INT:
    pp_printf_raw (pp, "%uU", e->u.v);
    break;

  case E_QUERY:
    pp_puts (pp, "(");
    expr_print (pp, e->u.e.l);
    pp_puts (pp, "?");
    expr_print (pp, e->u.e.r->u.e.l);
    pp_puts (pp, ":");
    expr_print (pp, e->u.e.r->u.e.r);
    pp_puts (pp, ")");
    break;

  case E_TRUE:
    pp_printf_raw (pp, "1");
    break;

  case E_FALSE:
    pp_printf_raw (pp, "0");
    break;
    
  case E_PROBE:
    if (expr_print_probe)
      (*expr_print_probe) (pp, (void*)e->u.e.l);
    break;

  case E_VAR:
    if (expr_print_id)
      (*expr_print_id) (pp, (void*)e->u.e.l);
    break;

  case E_FUNCTION:
    pp_puts (pp, e->u.fn.s);
    pp_puts (pp, "(");
    for (e = e->u.fn.r; e; e = e->u.e.r) {
      expr_print (pp, e->u.e.l);
      if (e->u.e.r)
	pp_puts (pp, ", ");
    }
    pp_puts (pp, ")");
    break;

  case E_BITFIELD:
    if (expr_print_id) {
      unsigned long mask;

      pp_puts (pp, "((");
      (*expr_print_id) (pp, (void*)e->u.e.l);
      pp_puts (pp, ">> ");
      pp_printf_raw (pp, "%lu", (unsigned long)(e->u.e.r->u.e.l));
      pp_puts (pp, ")");
      mask = (1UL << ((unsigned long)(e->u.e.r->u.e.r)-(unsigned long)(e->u.e.r->u.e.l)+1))-1;
      pp_printf_raw (pp, "& %lu)", mask);
    }
    break;
  default:
    fatal_error ("Unhandled case!\n");
    break;
  }
}


Expr *expr_parse_any (LFILE *l)
{
  int count = 0;
  Expr *e;

  int_real_only = 0;
  
  INIT (E_LSR, ">>");

  Tl = l;
  e = I();

  if (!e) {
    e = BE ();
  }

  if (count) {
    file_deltoken (l, ">>");
  }

  return e;
}
