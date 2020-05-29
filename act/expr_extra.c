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
#include <file.h>
#include <misc.h>
#include <string.h>

/*--- replicated here ---*/

#define E_ANDLOOP (E_END + 21) 
#define E_ORLOOP (E_END + 22)


static int tokand, tokor, lpar, rpar, ddot, colon;
static int double_colon, comma;


static void do_init (LFILE *l)
{
  static int init = 0;
  if (!init) {
    tokand = expr_gettoken (E_AND);
    tokor = expr_gettoken (E_OR);
    lpar = expr_gettoken (E_LPAR);
    rpar = expr_gettoken (E_RPAR);
    ddot = expr_gettoken (E_BITFIELD);
    colon = expr_gettoken (E_COLON);
    double_colon = file_addtoken (l, "::");
    comma = file_addtoken (l, ",");
    init = 1;
  }
}
  

static Expr *_parse_expr_func (LFILE *l)
{
  Expr *e, *f;
  int etype;
  
  do_init(l);
  
  if (file_sym (l) == double_colon || file_sym (l) == f_id) {

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

    k = 0;
    sz = 10239;
    if (file_have (l, double_colon)) {
      snprintf (buf, 10240, "::");
    }
    else {
      buf[0] = '\0';
    }
    PRINT_STEP;
    
    while (file_have (l, f_id)) {
      snprintf (buf+k, sz, "%s", file_prev (l));
      PRINT_STEP;
      if (!file_have (l, double_colon)) {
	break;
      }
      snprintf (buf+k, sz, "%s", file_prev (l));
      PRINT_STEP;
    }

    if (!file_have (l, lpar)) {
      FREE (e);
      return NULL;
    }
    e->u.fn.s = Strdup (buf);
    e->u.fn.r = NULL;
    f = e;
    if (!file_have (l, rpar)) {
      do {
	NEW (f->u.e.r, Expr);
	f = f->u.e.r;
	f->type = E_LT;
	f->u.e.r = NULL;
	f->u.e.l = expr_parse_any (l);
	if (!f->u.e.l) {
	  expr_free (e);
	  return NULL;
	}
      } while (file_have (l, comma));
      if (!file_have (l, rpar)) {
	expr_free (e);
	return NULL;
      }
    }
  }
  else {
    return NULL;
  }
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
  else if (!(e = _parse_expr_func (l))) {
    file_set_position (l);
    file_pop_position (l);
    return NULL;
  }
  file_pop_position (l);
  return e;
}


Expr *act_parse_expr_intexpr_base (LFILE *l)
{
  Expr *e;

  do_init(l);
  
  file_push_position (l);
  e = _parse_expr_func (l);
  if (!e) {
    file_set_position (l);
  }
  file_pop_position (l);
  return e;
}

int act_expr_parse_newtokens (LFILE *l)
{
  do_init(l);

  return (file_sym (l) == double_colon);
}
