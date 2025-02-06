/*************************************************************************
 *
 *  Copyright (c) 2025 Rajit Manohar
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
#include <act/expr_api.h>
#include <act/expr_extra.h>
#include <act/act_id.h>
#include <common/hash.h>
#include <common/misc.h>
#include <common/int.h>

/*------------------------------------------------------------------------
 *
 *  Expressions can get large, especially after various expansion
 *  phases. The following converts it into a dag, sharing subtrees.
 *
 *------------------------------------------------------------------------
 */

static Expr *_expr_todag (struct cHashtable *H, Expr *e)
{
  Expr *ret = NULL;
  Expr *l, *r;
  chash_bucket_t *b;

  if (!e) return NULL;

  b = chash_lookup (H, e);
  if (b) {
    return (Expr *)b->v;
  }

#define REC_CALL(x)  _expr_todag(H, (x))

  NEW (ret, Expr);
  ret->type = e->type;
  ret->u.e.l = NULL;
  ret->u.e.r = NULL;

  switch (e->type) {
  case E_ANDLOOP:
  case E_ORLOOP:
  case E_PLUSLOOP:
  case E_MULTLOOP:
  case E_XORLOOP:
  case E_ARRAY:
  case E_SUBRANGE:
  case E_ENUM_CONST:
    Assert (0, "Should not be here!\n");
    break;
    
  case E_AND:
  case E_OR:
  case E_XOR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_DIV:
  case E_MOD:
  case E_GT:
  case E_GE:
  case E_LE:
  case E_LT:
  case E_EQ:
  case E_NE:
  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
  case E_QUERY:
  case E_COLON:
    ret->u.e.l = REC_CALL (e->u.e.l);
    ret->u.e.r = REC_CALL (e->u.e.r);
    break;
    
  case E_BITFIELD:
    ret->u.e.l = (Expr *)((ActId *)e->u.e.l)->Clone ();
    NEW (ret->u.e.r, Expr);
    ret->u.e.r->type = E_BITFIELD;
    // constants, so unique already and hashed
    ret->u.e.r->u.e.l = e->u.e.r->u.e.l;
    ret->u.e.r->u.e.r = e->u.e.r->u.e.r;
    break;

  case E_PROBE:
  case E_VAR:
    ret->u.e.l = (Expr *) ((ActId *)e->u.e.l)->Clone();
    break;

  case E_BUILTIN_INT:
    ret->u.e.l = REC_CALL (e->u.e.l);
    // constants, so unique already and hashed
    ret->u.e.r = expr_dup (e->u.e.r);
    break;
    
  case E_BUILTIN_BOOL:
    ret->u.e.l = REC_CALL (e->u.e.l);
    ret->u.e.r = NULL;
    break;
    
  case E_FUNCTION:
    // walk through the arguments; the needed bitwidth is specified by
    // the argument type!
    ret->u.fn.s = e->u.fn.s;
    ret->u.fn.r = NULL;
    {
      Expr *args = e->u.fn.r;
      Expr *tmp = NULL;
      while (args) {
	if (!ret->u.e.r) {
	  NEW (ret->u.e.r, Expr);
	  tmp = ret;
	}
	else {
	  NEW (tmp->u.e.r, Expr);
	  tmp = tmp->u.e.r;
	}
	tmp->type = E_LT;
	tmp->u.e.r = NULL;
	tmp->u.e.l = REC_CALL (args->u.e.l);
	args = args->u.e.r;
      }
    }
    break;

  case E_CONCAT:
    {
      Expr *e_save = e;
      Expr *tmp = ret;
      while (e) {
	tmp->u.e.l = REC_CALL (e->u.e.l);
	if (e->u.e.r) {
	  NEW (tmp->u.e.r, Expr);
	  tmp = tmp->u.e.r;
	  tmp->type = e->u.e.r->type;
	  tmp->u.e.r = NULL;
	  tmp->u.e.l = NULL;
	}
	e = e->u.e.r;
      }
      e = e_save;
    }
    break;

  case E_INT:
    ret = expr_dup (e);
    break;

  case E_REAL:
    Assert (0, "What?");
    break;

  case E_TRUE:
  case E_FALSE:
    break;

  case E_SELF:
  case E_SELF_ACK:
    break;

  case E_STRUCT_REF:
    Assert (0, "This should be called after unstruct()");
    break;
    
  default:
    fatal_error ("Unknown expression type (%d)!", e->type);
    break;
  }
  b = chash_lookup (H, ret);
  if (b) {
    if (ret->type == E_FUNCTION || ret->type == E_CONCAT) {
      Expr *tmp, *tmp2;
      if (ret->type == E_FUNCTION) {
	tmp = e->u.fn.r;
      }
      else {
	tmp = e->u.e.r;
      }
      while (tmp) {
	tmp2 = tmp->u.e.r;
	FREE (tmp);
	tmp = tmp2;
      }
    }
    FREE (ret);
    ret = (Expr *)b->v;
  }
  else {
    b = chash_add (H, ret);
    b->v = ret;
  }
  return ret;
}

static int _exprhashfn (int sz,  void *key)
{
  int res = 0;
  if (!key) return 0;
  Expr *e = (Expr *) key;

  res = hash_function_continue (sz, (const unsigned char *) &e->type,
				sizeof (int), 0, 0);
  
  switch (e->type) {
  case E_VAR:
  case E_PROBE:
    res = ((ActId *)e->u.e.l)->getHash (res, sz);
    break;
    
  case E_BITFIELD:
    res = ((ActId *)e->u.e.l)->getHash (res, sz);
    res = hash_function_continue (sz, (const unsigned char *)
				  &e->u.e.r->u.e.l,
				  sizeof (Expr *), res, 1);
    res = hash_function_continue (sz, (const unsigned char *)
				  &e->u.e.r->u.e.r,
				  sizeof (Expr *), res, 1);
    break;

  case E_FUNCTION:
    res = hash_function_continue (sz, (const unsigned char *)
				  &e->u.fn.s,
				  sizeof (char *), res, 1);
    e = e->u.fn.r;
  case E_CONCAT:
    while (e) {
      res = hash_function_continue (sz, (const unsigned char *)
				    &e->u.e.l, sizeof (Expr *),
				    res, 1);
      e = e->u.e.r;
    }
    break;

  case E_AND:
  case E_OR:
  case E_XOR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_DIV:
  case E_MOD:
  case E_GT:
  case E_GE:
  case E_LE:
  case E_LT:
  case E_EQ:
  case E_NE:
  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
  case E_QUERY:
  case E_COLON:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    res = hash_function_continue (sz, (const unsigned char *) &e->u.e.l,
				  sizeof (Expr *), res, 1);
    res = hash_function_continue (sz, (const unsigned char *) &e->u.e.r,
				  sizeof (Expr *), res, 1);
    break;

  case E_INT:
  case E_TRUE:
  case E_FALSE:
  case E_SELF:
  case E_SELF_ACK:
    break;
    
  default:
    Assert (0, "unexpected expr!");
    break;
  }
  return res;
}

static int _exprmatchfn (void *key1, void *key2)
{
  if (key1 && !key2 || !key1 && key2) return 0;
  if (key1 == key2) return 1;
  Expr *e1 = (Expr *) key1;
  Expr *e2 = (Expr *) key2;
  if (e1->type != e2->type) return 0;
  if (e1->type == E_CONCAT || e1->type == E_FUNCTION) {
    if (e1->type == E_FUNCTION) {
      e1 = e1->u.fn.r;
      e2 = e2->u.fn.r;
    }
    while (e1 && e2) {
      if (e1->type != e2->type) return 0;
      if (e1->u.e.l != e2->u.e.l) return 0;
      e1 = e1->u.e.r;
      e2 = e2->u.e.r;
    }
    if (e1 || e2) return 0;
    return 1;
  }
  else {
    if (e1->type == E_VAR || e1->type == E_PROBE || e1->type == E_BITFIELD) {
      if (((ActId *)e1->u.e.l)->isEqual ((ActId *)e2->u.e.l)) {
	if (e1->type == E_BITFIELD) {
	  if (e1->u.e.r->u.e.l == e2->u.e.r->u.e.l &&
	      e1->u.e.r->u.e.r == e2->u.e.r->u.e.r) {
	    return 1;
	  }
	  return 0;
	}
	else {
	  return 1;
	}
      }
      return 0;
    }
    else {
      if (e1->type == E_TRUE || e1->type == E_FALSE ||
	  e1->type == E_SELF || e1->type == E_SELF_ACK) {
	return 1;
      }
      else if (e1->type == E_INT) {
	if (e1->u.ival.v != e2->u.ival.v) return 0;
	if (e1->u.ival.v_extra == e2->u.ival.v_extra) {
	  return 1;
	}
	else if (e1->u.ival.v_extra && e2->u.ival.v_extra) {
	  if (*((BigInt *)e1->u.ival.v_extra) ==
	      *((BigInt *)e2->u.ival.v_extra)) {
	    return 1;
	  }
	  return 0;
	}
	return 0;
      }
      if (e1->u.e.l == e2->u.e.l && e1->u.e.r == e2->u.e.r) return 1;
      return 0;
    }
  }
}

static void *_exprdupfn (void *key)
{
  return key; // alias!
}

static void _exprfreefn (void *key)
{
  // do nothing, since we aliased the key
}




Expr *expr_dag (Expr *e)
{
  struct cHashtable *H;
  H = chash_new (4);
  H->hash = _exprhashfn;
  H->match = _exprmatchfn;
  H->dup = _exprdupfn;
  H->free = _exprfreefn;

  e = _expr_todag (H, e);
  chash_free (H);
  return e;
}

static void _collect_dag (struct cHashtable *H, Expr *e)
{
  chash_bucket_t *b;

  if (!e) return;

  b = chash_lookup (H, e);
  if (b) {
    return;
  }
  b = chash_add (H, e);

#undef REC_CALL
#define REC_CALL(x)  _collect_dag  (H, (x))

  switch (e->type) {
  case E_ANDLOOP:
  case E_ORLOOP:
  case E_PLUSLOOP:
  case E_MULTLOOP:
  case E_XORLOOP:
  case E_ARRAY:
  case E_SUBRANGE:
  case E_ENUM_CONST:
    Assert (0, "Should not be here!\n");
    break;
    
  case E_AND:
  case E_OR:
  case E_XOR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_DIV:
  case E_MOD:
  case E_GT:
  case E_GE:
  case E_LE:
  case E_LT:
  case E_EQ:
  case E_NE:
  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
  case E_QUERY:
  case E_COLON:
    REC_CALL (e->u.e.l);
    REC_CALL (e->u.e.r);
    break;
    
  case E_BITFIELD:
  case E_PROBE:
  case E_VAR:
    break;

  case E_BUILTIN_INT:
    REC_CALL (e->u.e.l);
    break;
    
  case E_BUILTIN_BOOL:
    REC_CALL (e->u.e.r);
    break;
    
  case E_FUNCTION:
    {
      Expr *args = e->u.fn.r;
      while (args) {
	REC_CALL (args->u.e.l);
	args = args->u.e.r;
      }
    }
    break;

  case E_CONCAT:
    while (e) {
      REC_CALL (e->u.e.l);
      e = e->u.e.r;
    }
    break;

  case E_INT:
  case E_TRUE:
  case E_FALSE:
  case E_SELF:
  case E_SELF_ACK:
    break;

  case E_STRUCT_REF:
    Assert (0, "This should be called after unstruct()");
    break;
    
  default:
    fatal_error ("Unknown expression type (%d)!", e->type);
    break;
  }
  return;
}


void expr_dag_free (Expr *e)
{
  struct cHashtable *H;
  chash_bucket_t *b;
  chash_iter_t it;
  H = chash_new (4);
  H->hash = _exprhashfn;
  H->match = _exprmatchfn;
  H->dup = _exprdupfn;
  H->free = _exprfreefn;

  _collect_dag (H, e);

  chash_iter_init (H, &it);
  while ((b = chash_iter_next (H, &it))) {
    Expr *e = (Expr *) b->key;
    if (e->type == E_FUNCTION || e->type == E_CONCAT) {
      Expr *t;
      if (e->type == E_FUNCTION) {
	t = e;
	e = e->u.fn.r;
	FREE (t);
      }
      while (e) {
	t = e;
	e = e->u.e.r;
	FREE (t);
      }
    }
    else {
      if (e->type == E_VAR || e->type == E_PROBE || e->type == E_BITFIELD) {
	delete ((ActId *)e->u.e.l);
	if (e->type == E_BITFIELD) {
	  FREE (e->u.e.r);
	}
      }
      FREE (e);
    }
  }
  chash_free (H);
  return;
}
