/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#include <act/act.h>
#include <act/types.h>
#include <act/lang.h>
#include <act/inst.h>
#include <act/body.h>
#include <act/value.h>
#include <act/inline.h>
#include "prs.h"
#include <common/qops.h>
#include <common/config.h>
#include <string.h>

/*--- 
  chp expressions have a slightly different expansion, because
  they might use real arrays
 *---
 */
static Expr *chp_expr_expand (Expr *e, ActNamespace *ns, Scope *s)
{
  return expr_expand (e, ns, s, ACT_EXPR_EXFLAG_CHPEX);
}

void act_chp_free (act_chp_lang_t *c)
{
  listitem_t *li;
  act_chp_gc_t *tmpgc, *gc;
  if (!c) {
    return;
  }
  switch (c->type) {
  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      act_chp_free ((act_chp_lang_t *) list_value (li));
    }
    list_free (c->u.semi_comma.cmd);
    break;

  case ACT_HSE_FRAGMENTS:
    act_chp_free (c->u.frag.body);
    act_chp_free (c->u.frag.next);
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    gc = c->u.gc;
    while (gc) {
      act_chp_free (gc->s);
      expr_ex_free (gc->g);
      tmpgc = gc;
      gc = gc->next;
      FREE (tmpgc);
    }
    break;

  case ACT_CHP_FUNC:
    for (li = list_first (c->u.func.rhs); li; li = list_next (li)) {
      act_func_arguments_t *arg = (act_func_arguments_t *)list_value (li);
      if (!arg->isstring) {
	expr_ex_free (arg->u.e);
      }
      FREE (arg);
    }
    list_free (c->u.func.rhs);
    break;
     
  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    if (c->u.assign.id) {
      delete c->u.assign.id;
    }
    expr_ex_free (c->u.assign.e);
    break;

  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    if (c->u.comm.chan) {
      delete c->u.comm.chan;
    }
    if (c->u.comm.var) {
      delete c->u.comm.var;
    }
    expr_ex_free (c->u.comm.e);
    break;

  default:
    fatal_error ("CHP type %d: after expansion!\n", c->type);
    break;
  }
  FREE (c);
}

static int _chp_expanding_macro = 0;

void chp_expand_macromode (int v)
{
  _chp_expanding_macro = v;
}

void act_chp_macro_check (Scope *s, ActId *id)
{
  if (_chp_expanding_macro) {
    if (!s->localLookup (id, NULL)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "\tIdentifier is not accessible from macro: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
}

/*
 * This is a chp variable to be read. So it can have array
 * dereferences
 */
static ActId *expand_var_read (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;

  /* this needs to be changed  to: expand as much as possible, not
     full expand. It needs to be a non-array variable type.
  */
  idtmp = id->Expand (ns, s);
  
  etmp = idtmp->Eval (ns, s);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;

}
/*
 * This is a chp variable to be written. So it can have array
 * dereferences
 */
ActId *expand_var_write (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;

  /* this needs to be changed  to: expand as much as possible, not
     full expand. It needs to be a non-array variable type.
  */
  idtmp = id->ExpandCHP (ns, s);
  
  etmp = idtmp->EvalCHP (ns, s, 1);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}


static ActId *expand_var_chan (ActId *id, ActNamespace *ns, Scope *s)
{
  return expand_var_read (id, ns, s);
}


struct single_err {
  act_connection *c;
  int count;
};

class rw_set_error {
 public:
  rw_set_error() {
    A_INIT (errs_);
  }
  ~rw_set_error() {
    A_FREE (errs_);
  }

  void addErr (act_connection *c) {
    for (int i=0; i < A_LEN (errs_); i++) {
      if (errs_[i].c == c) {
	errs_[i].count++;
	return;
      }
    }
    A_NEW (errs_, struct single_err);
    A_NEXT (errs_).c = c;
    A_NEXT (errs_).count = 1;
    A_INC (errs_);
  }

  bool isErr() {
    return A_LEN (errs_) != 0 ? true : false;
  }

  void printErrs() {
    warning ("potential write conflict in CHP description");
    for (int i=0; i < A_LEN (errs_); i++) {
      ActId *idtmp = errs_[i].c->toid();
      fprintf (stderr, "   Variable: `");
      idtmp->Print (stderr);
      fprintf (stderr, "'");
      if (errs_[i].count > 1) {
	fprintf (stderr, " (count=%d)", errs_[i].count);
      }
      fprintf (stderr, "\n");
      delete idtmp;
    }
  }

 private:
  A_DECL (struct single_err, errs_);
};

class rw_sets {
 public:
  rw_sets() {
    A_INIT (reads);
    A_INIT (writes);
  }

  ~rw_sets() {
    A_FREE (reads);
    A_FREE (writes);
  }

  void addRead (act_connection *c) {
    for (int i=0; i < A_LEN (reads); i++) {
      if (reads[i] == c)
	return;
    }
    A_NEW (reads, act_connection *);
    A_NEXT (reads) = c;
    A_INC (reads);
  }

  bool isWrite (act_connection *c) {
    for (int i=0; i < A_LEN (writes); i++) {
      if (writes[i] == c) {
	return true;
      }
    }
    return false;
  }

  void addWrite (act_connection *c) {
    if (isWrite (c)) {
      return;
    }
    A_NEW (writes, act_connection *);
    A_NEXT (writes) = c;
    A_INC (writes);
  }

  act_connection *isConflict (rw_sets &r) {
    for (int i=0; i < A_LEN (reads); i++) {
      if (r.isWrite (reads[i])) {
	return reads[i];
      }
    }
    for (int i=0; i < A_LEN (writes); i++) {
      if (r.isWrite (writes[i])) {
	return writes[i];
      }
    }
    return nullptr;
  }

private:

  A_DECL (act_connection *, reads);
  A_DECL (act_connection *, writes);
};


static void _add_dynamic_var (struct Hashtable **H, ActId *id)
{
  if (!id) return;
  if (id->isDynamicDeref ()) {
    if (!*H) {
      *H = hash_new (4);
    }
    if (!hash_lookup (*H, id->getName())) {
      hash_add (*H, id->getName());
    }
  }
}

static void _compute_dynamic_vars (struct Hashtable **H, Expr *e)
{
  if (!e) {
    return;
  }

  switch (e->type) {

  case E_ANDLOOP:
  case E_ORLOOP:
    break;

  case E_AND:
  case E_OR:
  case E_XOR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV:
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
    _compute_dynamic_vars (H, e->u.e.l);
    _compute_dynamic_vars (H, e->u.e.r);
    break;

  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    _compute_dynamic_vars (H, e->u.e.l);
    break;

  case E_QUERY:
    _compute_dynamic_vars (H, e->u.e.l);
    _compute_dynamic_vars (H, e->u.e.r->u.e.l);
    _compute_dynamic_vars (H, e->u.e.r->u.e.r);
    break;

  case E_COLON:
    break;

  case E_BITFIELD:
    _add_dynamic_var (H, (ActId *)e->u.e.l);
    break;

  case E_PROBE:
    _add_dynamic_var (H, (ActId *)e->u.e.l);
    break;

  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    _compute_dynamic_vars (H, e->u.e.l);
    break;

  case E_FUNCTION:
    e = e->u.fn.r;
    while (e) {
      _compute_dynamic_vars (H, e->u.e.l);
      e = e->u.e.r;
    }
    break;

  case E_CONCAT:
    while (e) {
      _compute_dynamic_vars (H, e->u.e.l);
      e = e->u.e.r;
    }
    break;

  case E_VAR:
    _add_dynamic_var (H, (ActId *)e->u.e.l);
    break;

  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
    break;

  case E_ARRAY:
  case E_SUBRANGE:
  case E_SELF:
  case E_SELF_ACK:
    break;

  default:
    fatal_error ("Unknown expression type (%d)!", e->type);
    break;
  }
}

static void _compute_dynamic_vars (struct Hashtable **H,
				   act_chp_lang_t *c)
{
  listitem_t *li;
  act_chp_gc_t *gc;

  if (!c) return;
  switch (c->type) {
  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      _compute_dynamic_vars (H, (act_chp_lang_t *) list_value (li));
    }
    break;

  case ACT_HSE_FRAGMENTS:
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (gc = c->u.gc; gc; gc = gc->next) {
      _compute_dynamic_vars (H, gc->g);
      _compute_dynamic_vars (H, gc->s);
    }
    break;

  case ACT_CHP_FUNC:
  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    _compute_dynamic_vars (H, c->u.assign.e);
    _add_dynamic_var (H, c->u.assign.id);
    break;

  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    _add_dynamic_var (H, c->u.comm.chan);
    _add_dynamic_var (H, c->u.comm.var);
    _compute_dynamic_vars (H, c->u.comm.e);
    break;

  default:
    fatal_error ("CHP type %d: after expansion!\n", c->type);
    break;
  }
}

static struct Hashtable *_localdynamic = NULL;

static void _add_var (rw_sets &rw, ActId *id, ActNamespace *ns, Scope *s,
		      int iswrite)
{
  act_connection *c;
  if (!id) return;
  if (_localdynamic && hash_lookup (_localdynamic, id->getName())) {
    ActId *tmp = new ActId (id->getName());
    c = tmp->Canonical (s);
    delete tmp;
  }
  else {
    c = id->Canonical (s);
  }
  if (iswrite) {
    rw.addWrite (c);
  }
  else {
    rw.addRead (c);
  }
}

static void _compute_rw_sets (rw_sets &rw,
			      Expr *e,
			      ActNamespace *ns,
			      Scope *s)
{
  if (!e) {
    return;
  }

  switch (e->type) {

  case E_ANDLOOP:
  case E_ORLOOP:
    break;

  case E_AND:
  case E_OR:
  case E_XOR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV:
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
    _compute_rw_sets (rw, e->u.e.l, ns, s);
    _compute_rw_sets (rw, e->u.e.r, ns, s);
    break;

  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    _compute_rw_sets (rw, e->u.e.l, ns, s);
    break;

  case E_QUERY:
    _compute_rw_sets (rw, e->u.e.l, ns, s);
    _compute_rw_sets (rw, e->u.e.r->u.e.l, ns, s);
    _compute_rw_sets (rw, e->u.e.r->u.e.r, ns, s);
    break;

  case E_COLON:
    break;

  case E_BITFIELD:
    _add_var (rw, (ActId *)e->u.e.l, ns, s, 0);
    break;

  case E_PROBE:
    _add_var (rw, (ActId *)e->u.e.l, ns, s, 0);
    break;

  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    _compute_rw_sets (rw, e->u.e.l, ns, s);
    break;

  case E_FUNCTION:
    e = e->u.fn.r;
    while (e) {
      _compute_rw_sets (rw, e->u.e.l, ns, s);
      e = e->u.e.r;
    }
    break;

  case E_CONCAT:
    while (e) {
      _compute_rw_sets (rw, e->u.e.l, ns, s);
      e = e->u.e.r;
    }
    break;

  case E_VAR:
    _add_var (rw, (ActId *)e->u.e.l, ns, s, 0);
    break;

  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
    break;

  case E_ARRAY:
  case E_SUBRANGE:
  case E_SELF:
  case E_SELF_ACK:
    break;

  default:
    fatal_error ("Unknown expression type (%d)!", e->type);
    break;
  }
}

static void _compute_rw_sets (rw_sets &rw,
			      act_chp_lang_t *c,
			      ActNamespace *ns,
			      Scope *s)
{
  listitem_t *li;
  act_chp_gc_t *gc;

  if (!c) return;
  switch (c->type) {
  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      _compute_rw_sets (rw, (act_chp_lang_t *) list_value (li), ns, s);
    }
    break;

  case ACT_HSE_FRAGMENTS:
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (gc = c->u.gc; gc; gc = gc->next) {
      _compute_rw_sets (rw, gc->g, ns, s);
      _compute_rw_sets (rw, gc->s, ns, s);
    }
    break;

  case ACT_CHP_FUNC:
  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    _compute_rw_sets (rw, c->u.assign.e, ns, s);
    _add_var (rw, c->u.assign.id, ns, s, 1);
    break;

  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    _add_var (rw, c->u.comm.chan, ns, s, 1);
    _add_var (rw, c->u.comm.var, ns, s, 1);
    _compute_rw_sets (rw, c->u.comm.e, ns, s);
    break;

  default:
    fatal_error ("CHP type %d: after expansion!\n", c->type);
    break;
  }
}


static void _check_concurrent_conflicts (act_chp_lang_t *c,
					 ActNamespace *ns,
					 Scope *s)
{
  listitem_t *li;
  act_chp_gc_t *gc;

  if (!c) return;
  switch (c->type) {
  case ACT_CHP_SEMI:
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      _check_concurrent_conflicts ((act_chp_lang_t *) list_value (li), ns, s);
    }
    break;

  case ACT_CHP_COMMA:
    {
      int count = 0;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	count++;
      }
      if (count < 2) {
	return;
      }
      rw_sets rw[count];
      rw_set_error errs;
      count = 0;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	_compute_rw_sets (rw[count], (act_chp_lang_t *)list_value (li), ns, s);
	count++;
      }
      for (int i=0; i < count; i++) {
	for (int j = i+1; j < count; j++) {
	  act_connection *tmp;
	  tmp = rw[i].isConflict (rw[j]);
	  if (tmp) {
	    errs.addErr (tmp);
	  }
	  tmp = rw[j].isConflict (rw[i]);
	  if (tmp) {
	    errs.addErr (tmp);
	  }
	}
      }
      if (errs.isErr()) {
	act_error_ctxt (stderr);
	errs.printErrs();
      }
    }
    break;

  case ACT_HSE_FRAGMENTS:
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (gc = c->u.gc; gc; gc = gc->next) {
      _check_concurrent_conflicts (gc->s, ns, s);
    }
    break;

  case ACT_CHP_SKIP:
  case ACT_CHP_ASSIGN:
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
  case ACT_CHP_FUNC:
    break;

  default:
    fatal_error ("CHP type %d: after expansion!\n", c->type);
    break;
  }
}

act_chp *chp_expand (act_chp *c, ActNamespace *ns, Scope *s)
{
  act_chp *ret;
  if (!c) return NULL;

  NEW (ret, act_chp);
  
  ret->vdd = expand_var_read (c->vdd, ns, s);
  ret->gnd = expand_var_read (c->gnd, ns, s);
  ret->psc = expand_var_read (c->psc, ns, s);
  ret->nsc = expand_var_read (c->nsc, ns, s);
  _act_chp_is_synth_flag = 1;
  ret->c = chp_expand (c->c, ns, s);
  ret->is_synthesizable = _act_chp_is_synth_flag;

  struct Hashtable *H = NULL;
  _compute_dynamic_vars (&H, ret->c);
  _localdynamic = H;

  if (ret->c && ret->c->type == ACT_CHP_COMMA) {
    listitem_t *li;
    for (li = list_first (ret->c->u.semi_comma.cmd); li; li = list_next (li)) {
      _check_concurrent_conflicts ((act_chp_lang_t *) list_value (li), ns, s);
    }
  }
  else {
    _check_concurrent_conflicts (ret->c, ns, s);
  }
  if (_localdynamic) {
    hash_free (_localdynamic);
    _localdynamic = NULL;
  }

  return ret;
}

static int _has_probe (Expr *e)
{
  if (!e) return 0;
  switch (e->type) {
  case E_AND:
  case E_OR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    return _has_probe (e->u.e.l) || _has_probe (e->u.e.r);
    break;
    
  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    return _has_probe (e->u.e.l);
    break;

  case E_QUERY:
    return _has_probe (e->u.e.l) ||
      _has_probe (e->u.e.r->u.e.l) ||
      _has_probe (e->u.e.r->u.e.r);
    break;
    
  case E_FUNCTION:
    {
      Expr *tmp = e->u.fn.r;
      Assert (!tmp || tmp->type == E_LT, "Function elaboration?");
      while (tmp) {
	if (_has_probe (tmp->u.e.l)) {
	  return 1;
	}
	tmp = tmp->u.e.r;
      }
    }
    return 0;
    break;

  case E_PROBE:
    return 1;
    break;

  case E_VAR:
  case E_TRUE:
  case E_FALSE:
  case E_INT:
    return 0;
    break;

  case E_BITFIELD:
    return _has_probe (e->u.e.r->u.e.l) ||
      _has_probe (e->u.e.r->u.e.r);
    break;
    
  case E_CONCAT:
    while (e) {
      if (_has_probe (e->u.e.l)) {
	return 1;
      }
      e = e->u.e.r;
    }
    return 0;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    return _has_probe (e->u.e.l);
    break;

  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    return 1;
    break;
  }
}
    

static Expr *_chp_fix_nnf (Expr *e, int invert)
{
  Expr *t;

  if (!e) return e;
  
  switch (e->type) {
  case E_AND:
  case E_OR:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, invert);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, invert);
    if (invert) {
      if (e->type == E_AND) {
	e->type = E_OR;
      }
      else {
	e->type = E_AND;
      }
    }
    break;

  case E_NOT:
    t = _chp_fix_nnf (e->u.e.l, 1-invert);
    FREE (e);
    e = t;
    break;

  case E_XOR:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, invert);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    break;

  case E_LT:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_GE;
    break;
    
  case E_GT:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_LE;
    break;
    
  case E_LE:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_GT;
    break;
    
  case E_GE:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_LT;
    break;

  case E_EQ:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_NE;
    break;
    
  case E_NE:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_EQ;
    break;
    
    
  case E_COMPLEMENT:
  case E_UMINUS:
    Assert (invert == 0, "What?");
    e->u.e.l = _chp_fix_nnf (e->u.e.l, invert);
    break;

  case E_QUERY:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r->u.e.l = _chp_fix_nnf (e->u.e.r->u.e.l, invert);
    e->u.e.r->u.e.r = _chp_fix_nnf (e->u.e.r->u.e.r, invert);
    break;
    

  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    if (invert) {
      fatal_error ("Unexpected type %d with inversion?", e->type);
    }
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    break;
    
  case E_FUNCTION:
    if (invert) {
      NEW (t, Expr);
      t->type = E_NOT;
      t->u.e.l = e;
      t->u.e.r = NULL;
    }
    {
      Expr *tmp = e->u.fn.r;
      Assert (!tmp || tmp->type == E_LT, "Function elaboration?");
      while (tmp) {
	if (_has_probe (tmp->u.e.l)) {
	  warning ("Probes in function arguments not correctly supported");
	}
	tmp = tmp->u.e.r;
      }
    }
    if (invert) {
      e = t;
    }
    break;

  case E_PROBE:
    if (invert) {
      NEW (t, Expr);
      t->type = E_NOT;
      t->u.e.l = e;
      t->u.e.r = NULL;
      e = t;
    }
    break;

  case E_VAR:
    if (invert) {
      NEW (t, Expr);
      t->type = E_NOT;
      t->u.e.l = e;
      t->u.e.r = NULL;
      e = t;
    }
    break;

  case E_BUILTIN_BOOL:
    if (invert) {
      NEW (t, Expr);
      t->type = E_NOT;
      t->u.e.l = e;
      t->u.e.r = NULL;
      e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
      e = t;
    }
    else {
      e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    }
    break;

  case E_TRUE:
    if (invert) e->type = E_FALSE;
    break;
    
  case E_FALSE:
    if (invert) e->type = E_TRUE;
    break;

  case E_CONCAT:
    if (invert) {
      fatal_error ("Unexpected type %d with inversion?", e->type);
    }
    break;

  case E_BUILTIN_INT:
  case E_BITFIELD:
  case E_INT:
    if (invert) {
      fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    }
    break;
    
  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    break;
  }
  return e;
}

static pHashtable *pmap = NULL;
static int _expr_has_probes = 0;
static int _expr_has_nonlocal_vars = 0;

static Expr *_process_probes (Expr *e)
{
  phash_iter_t iter;
  phash_bucket_t *b;
  
  if (pmap->n > 0) {
    _expr_has_probes = 1;
    phash_iter_init (pmap, &iter);
    while ((b = phash_iter_next (pmap, &iter))) {
      if (b->i == 0) {
	Expr *t;
	act_connection *c = (act_connection *)b->key;
	NEW (t, Expr);
	t->type = E_AND;
	t->u.e.r = e;
	e = t;
	NEW (t->u.e.l, Expr);
	t = t->u.e.l;
	t->type = E_PROBE;
	t->u.e.l = (Expr *)c->toid();
	t->u.e.r = NULL;
      }
    }
    phash_clear (pmap);
  }
  return e;
}


static Expr *_chp_add_probes (Expr *e, ActNamespace *ns, Scope *s, int isbool)
{
  Expr *t;
  phash_bucket_t *b;
  act_connection *c;

  if (!e) return e;
  
  switch (e->type) {
  case E_AND:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, isbool);
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, isbool);
    if (isbool) {
      e = _process_probes (e);
    }
    break;

  case E_OR:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, isbool);
    if (isbool) {
      e->u.e.l = _process_probes (e->u.e.l);
    }
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, isbool);
    if (isbool) {
      e->u.e.r = _process_probes (e->u.e.r);
    }
    break;

  case E_NOT:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, isbool);
    if (isbool) {
      e = _process_probes (e);
    }
    break;

  case E_XOR:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, isbool);
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, isbool);
    if (isbool) {
      e = _process_probes (e);
    }
    break;
    
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 0);
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, 0);
    e = _process_probes (e);
    break;

  case E_COMPLEMENT:
  case E_UMINUS:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 0);
    break;

  case E_QUERY:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 1);
    e->u.e.r->u.e.l = _chp_add_probes (e->u.e.r->u.e.l, ns, s, isbool);
    e->u.e.r->u.e.r = _chp_add_probes (e->u.e.r->u.e.r, ns, s, isbool);
    break;

  case E_PROBE:
    c = ((ActId *)e->u.e.l)->Canonical (s);
    b = phash_lookup (pmap, c);
    if (!b) {
      b = phash_add (pmap, c);
      b->i = 1;
    }
    break;
    
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 0);
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, 0);
    break;
    
  case E_FUNCTION:
    /*-- nothing to do --*/
    break;

  case E_BITFIELD:
  case E_VAR:
    /*--  check if this is an channel! --*/
    act_chp_macro_check (s, (ActId *)e->u.e.l);
    {
      InstType *it =  s->FullLookup ((ActId *)e->u.e.l, NULL);
      if (TypeFactory::isChanType (it)) {
	c = ((ActId *)e->u.e.l)->Canonical (s);
	b = phash_lookup (pmap, c);
	if (b) {
	  /*-- ok has been recorded already --*/
	}
	else {
	  b = phash_add (pmap, c);
	  b->i = 0;		/* 0 = used as a variable */
	}
      }
      else {
	if (((ActId *)e->u.e.l)->isNonLocal (s)) {
	  _expr_has_nonlocal_vars = 1;
	}
      }
    }
    break;


  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 0);
    break;

  case E_CONCAT:
    {
      Expr *tmp = e;
      while (tmp) {
	tmp->u.e.l = _chp_add_probes (tmp->u.e.l, ns, s, 0);
	tmp = tmp->u.e.r;
      }
    }
    break;
    
  case E_TRUE:
  case E_FALSE:
  case E_INT:
    break;
    
  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    break;
  }
  return e;
}

static int _expr_has_any_probes (Expr *e)
{
  if (!e) return 0;
  
  switch (e->type) {
  case E_AND:
  case E_XOR:
  case E_OR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    if (_expr_has_any_probes (e->u.e.l)) return 1;
    if (_expr_has_any_probes (e->u.e.r)) return 1;
    break;

  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    if (_expr_has_any_probes (e->u.e.l)) return 1;
    break;

  case E_QUERY:
    if (_expr_has_any_probes (e->u.e.l)) return 1;
    if (_expr_has_any_probes (e->u.e.r->u.e.l)) return 1;
    if (_expr_has_any_probes (e->u.e.r->u.e.r)) return 1;
    break;

  case E_PROBE:
    return 1;
    break;
    
  case E_BITFIELD:
  case E_VAR:
    return 0;
    break;


  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    if (_expr_has_any_probes (e->u.e.l)) return 1;
    break;

  case E_CONCAT:
    {
      Expr *tmp = e;
      while (tmp) {
	if (_expr_has_any_probes (tmp->u.e.l)) return 1;
	tmp = tmp->u.e.r;
      }
    }
    break;
    
  case E_TRUE:
  case E_FALSE:
  case E_INT:
    return 0;
    break;
    
  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    break;
  }
  return 0;
}

int act_expr_has_neg_probes (Expr *e)
{
  if (!e) return 0;
  
  switch (e->type) {
  case E_AND:
  case E_XOR:
  case E_OR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    if (act_expr_has_neg_probes (e->u.e.l)) return 1;
    if (act_expr_has_neg_probes (e->u.e.r)) return 1;
    break;

  case E_NOT:
    if (e->u.e.l->type == E_PROBE) {
      return 1;
    }
    break;
    
  case E_COMPLEMENT:
  case E_UMINUS:
    if (act_expr_has_neg_probes (e->u.e.l)) return 1;
    break;

  case E_QUERY:
    if (act_expr_has_neg_probes (e->u.e.l)) return 1;
    if (act_expr_has_neg_probes (e->u.e.r->u.e.l)) return 1;
    if (act_expr_has_neg_probes (e->u.e.r->u.e.r)) return 1;
    break;

  case E_PROBE:
    return 0;
    break;
    
  case E_BITFIELD:
  case E_VAR:
    return 0;
    break;


  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    if (act_expr_has_neg_probes (e->u.e.l)) return 1;
    break;

  case E_CONCAT:
    {
      Expr *tmp = e;
      while (tmp) {
	if (act_expr_has_neg_probes (tmp->u.e.l)) return 1;
	tmp = tmp->u.e.r;
      }
    }
    break;
    
  case E_TRUE:
  case E_FALSE:
  case E_INT:
    return 0;
    break;
    
  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    break;
  }
  return 0;
}

static Expr *_prepend_probes (list_t *l, Expr *e)
{
  listitem_t *li;
  Expr *ret, *cur, *pexpr, *prev;
  if (list_isempty (l)) {
    return e;
  }
  ret = NULL;
  prev = NULL;
  while (!list_isempty (l)) {
    pexpr = (Expr *)list_delete_head (l);
    if (!ret) {
      NEW (ret, Expr);
      cur = ret;
    }
    else {
      NEW (cur->u.e.r, Expr);
      prev = cur;
      cur = cur->u.e.r;
    }
    cur->type = E_AND;
    cur->u.e.l = pexpr;
    cur->u.e.r = NULL;
  }
  if (e) {
    cur->u.e.r = e;
  }
  else {
    if (prev) {
      prev->u.e.r = cur->u.e.l;
    }
    else {
      ret = cur->u.e.l;
    }
    FREE (cur);
  }
  return ret;
}

/* 
   input is an NNF expression
*/
static Expr *_chp_group_probes (list_t *pl, Expr *e)
{
  Expr *l, *r;
  if (!e) return e;

  switch (e->type) {
  case E_AND:
    l = _chp_group_probes (pl, e->u.e.l);
    r = _chp_group_probes (pl, e->u.e.r);
    if (l && r) {
      e->u.e.l = l;
      e->u.e.r = r;
    }
    else if (l) {
      FREE (e);
      e = l;
    }
    else if (r) {
      FREE (e);
      e = r;
    }
    else {
      FREE (e);
      e = NULL;
    }
    break;

  case E_OR:
    if (!list_isempty (pl)) {
      if (_expr_has_any_probes (e->u.e.l) || _expr_has_any_probes (e->u.e.r)) {
	warning ("Illegal probe usage: all probes must be included in the top-level clause");
      }
      return e;
    }
    l  = _chp_group_probes (pl, e->u.e.l);
    if (!list_isempty (pl)) {
      l = _prepend_probes (pl, l);
    }
    Assert (list_isempty (pl), "Hmm");

    r = _chp_group_probes (pl, e->u.e.r);
    if (!list_isempty (pl)) {
      r = _prepend_probes (pl, r);
    }
    Assert (list_isempty (pl), "What?");

    e->u.e.l = l;
    e->u.e.r = r;
    break;

  case E_PROBE:
    list_append (pl, e);
    e = NULL;
    break;

  case E_NOT:
    break;

  default:
    if (_expr_has_any_probes (e)) {
      warning ("Illegal probe usage: all probes must be in the top-level clause");
    }
    break;
  }
  return e;
}



static Expr *_chp_fix_guardexpr (Expr *e, ActNamespace *ns, Scope *s)
{
  e = _chp_fix_nnf (e, 0);

  _expr_has_nonlocal_vars = 0;
  pmap = phash_new (4);
  e = _chp_add_probes (e, ns, s, 1);
  if (pmap->n > 0) {
    _expr_has_probes = 1;
    _expr_has_nonlocal_vars = 1;
  }
  phash_free (pmap);

  if (_expr_has_probes) {
    list_t *tmp = list_new ();
    e = _chp_group_probes (tmp, e);
    if (!list_isempty (tmp)) {
      e = _prepend_probes (tmp, e);
    }
    list_free (tmp);
  }
  return e;
}

act_chp_lang_t *chp_expand (act_chp_lang_t *c, ActNamespace *ns, Scope *s)
{
  act_chp_lang_t *ret;
  act_chp_gc_t *gchd, *gctl, *gctmp, *tmp;
  listitem_t *li;
  ValueIdx *vx;
  int expr_has_else, expr_has_probes, expr_has_nonlocal;
  
  if (!c) return NULL;
  NEW (ret, act_chp_lang_t);
  ret->label = c->label;
  ret->space = NULL;
  ret->type = c->type;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    ret->u.semi_comma.cmd = list_new ();
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      list_append (ret->u.semi_comma.cmd, chp_expand ((act_chp_lang_t *)list_value (li), ns, s));
    }
    break;

  case ACT_CHP_COMMALOOP:
  case ACT_CHP_SEMILOOP:
    {
      int ilo, ihi;
      act_syn_loop_setup (ns, s, c->u.loop.id, c->u.loop.lo, c->u.loop.hi,
		      &vx, &ilo, &ihi);

      if (c->type == ACT_CHP_COMMALOOP) {
	ret->type = ACT_CHP_COMMA;
      }
      else {
	ret->type = ACT_CHP_SEMI;
      }
      ret->u.semi_comma.cmd = list_new ();
      for (int iter=ilo; iter <= ihi; iter++) {
	s->setPInt (vx->u.idx, iter);
	list_append (ret->u.semi_comma.cmd,
		     chp_expand (c->u.loop.body, ns, s));
	
      }
      act_syn_loop_teardown (ns, s, c->u.loop.id, vx);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    gchd = NULL;
    gctl = NULL;
    expr_has_probes = 0;
    expr_has_else = 0;
    expr_has_nonlocal = 0;
    for (gctmp = c->u.gc; gctmp; gctmp = gctmp->next) {
      if (gctmp->id) {
	int ilo, ihi;

	act_syn_loop_setup (ns, s, gctmp->id, gctmp->lo, gctmp->hi,
			&vx, &ilo, &ihi);
	
	for (int iter=ilo; iter <= ihi; iter++) {
	  s->setPInt (vx->u.idx, iter);
	  NEW (tmp, act_chp_gc_t);
	  tmp->next = NULL;
	  tmp->id = NULL;
	  tmp->g = chp_expr_expand (gctmp->g, ns, s);
	  _expr_has_probes = 0;
	  tmp->g = _chp_fix_guardexpr (tmp->g, ns, s);
	  expr_has_probes = _expr_has_probes || expr_has_probes;
	  expr_has_nonlocal = expr_has_nonlocal || _expr_has_nonlocal_vars;
	  if (tmp->g && expr_is_a_const (tmp->g) && tmp->g->type == E_FALSE) {
	    FREE (tmp);
	  }
	  else {
	    tmp->s = chp_expand (gctmp->s, ns, s);
	    q_ins (gchd, gctl, tmp);
	  }
	}
	act_syn_loop_teardown (ns, s, gctmp->id, vx);
      }
      else {
	NEW (tmp, act_chp_gc_t);
	tmp->id = NULL;
	tmp->next = NULL;
	tmp->g = chp_expr_expand (gctmp->g, ns, s);
	_expr_has_probes = 0;
	tmp->g = _chp_fix_guardexpr (tmp->g, ns, s);
	expr_has_probes = _expr_has_probes || expr_has_probes;
	expr_has_nonlocal = expr_has_nonlocal || _expr_has_nonlocal_vars;
	if (!tmp->g && gctmp != c->u.gc) {
	  /*-- null guard in head is the loop shortcut --*/
	  expr_has_else = 1;
	}
	if (tmp->g && expr_is_a_const (tmp->g) && tmp->g->type == E_FALSE &&
	    c->type != ACT_CHP_DOLOOP) {
	  FREE (tmp);
	}
	else {
	  tmp->s = chp_expand (gctmp->s, ns, s);
	  q_ins (gchd, gctl, tmp);
	}
      }
    }
    if (!gchd) {
      /* ok this is false -> skip */
      NEW (tmp, act_chp_gc_t);
      tmp->id = NULL;
      tmp->next = NULL;
      
      tmp->g = const_expr_bool (0);
      NEW (tmp->s, act_chp_lang);
      tmp->s->type = ACT_CHP_SKIP;
      q_ins (gchd, gctl, tmp);
    }
    if (expr_has_else && expr_has_probes) {
      act_error_ctxt (stderr);
      fatal_error ("Cannot have an else clause when guards contain channels");
    }
    if (c->type == ACT_CHP_DOLOOP || c->type == ACT_CHP_LOOP) {
      if (expr_has_nonlocal) {
	act_error_ctxt (stderr);
	fatal_error ("Cannot have non-local variables/probes in loop guards");
      }
    }
    if (c->type != ACT_CHP_DOLOOP) {
      if (gchd->next == NULL && (!gchd->g || expr_is_a_const (gchd->g))) {
	/* loops and selections that simplify to a single guard that
	   is constant */
	if (c->type == ACT_CHP_LOOP) {
	  if (gchd->g && gchd->g->type == E_FALSE) {
	    /* whole thing is a skip */
	    /* XXX: need chp_free */
	    ret->u.gc = gchd;
	    act_chp_free (ret);
	    NEW (ret, act_chp_lang);
	    ret->type = ACT_CHP_SKIP;
	    return ret;
	  }
	}
	else {
	  if (!gchd->g || gchd->g->type == E_TRUE) {
	    /* whole thing is the body */
	    
	    FREE (ret);
	    ret = gchd->s;
	    FREE (gchd);
	    if (!ret) {
	      NEW (ret, act_chp_lang);
	      ret->type = ACT_CHP_SKIP;
	    }
	    return ret;
	  }
	}
      }
    }
    ret->u.gc = gchd;
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
  case ACT_CHP_ASSIGNSELF:
    ret->u.assign.id = expand_var_write (c->u.assign.id, ns, s);
    {
      ActId *tmp = ret->u.assign.id->stripArray ();
      /* 
	 Check if this is a dynamic array; if it is, we just need the
	 type and nothing else
      */
      if (tmp->isDynamicDeref()) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Structure dynamic array assignment currently unsupported.\n");
	fprintf (stderr, "\tVariable: ");
	ret->u.assign.id->Print (stderr);
	fprintf (stderr, "\n");
	exit (1);
      }

      act_connection *d = tmp->Canonical (s);
      if (d->getDir() == Type::direction::IN) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Assignment to an input variable.\n");
	fprintf (stderr, "\tVariable: ");
	ret->u.assign.id->Print (stderr);
	fprintf (stderr, "\n");
	exit (1);
      }
      delete tmp;
    }
    act_chp_macro_check (s, ret->u.assign.id);
    ret->u.assign.e = chp_expr_expand (c->u.assign.e, ns, s);
    break;
    
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    ret->u.comm.chan = expand_var_chan (c->u.comm.chan, ns, s);
    act_chp_macro_check (s, ret->u.comm.chan);
    {
      if (ret->u.comm.chan->isDynamicDeref()) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Dynamic channel arrays are unsupported.\n");
	fprintf (stderr, "\tVariable: ");
	ret->u.comm.chan->Print (stderr);
	fprintf (stderr, "\n");
	exit (1);
      }
      
      act_connection *d = ret->u.comm.chan->Canonical (s);

      if ((c->type == ACT_CHP_SEND && d->getDir() == Type::direction::IN &&
	   _chp_expanding_macro != 2) ||
	  (c->type == ACT_CHP_SEND && d->getDir() == Type::direction::OUT &&
	   _chp_expanding_macro == 2)) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Send operation on an input channel.\n");
	fprintf (stderr, "\tChannel: ");
	ret->u.comm.chan->Print (stderr);
	fprintf (stderr, "\n");
	exit (1);
      }
      else if ((c->type == ACT_CHP_RECV && d->getDir() == Type::direction::OUT && _chp_expanding_macro != 2) ||
	       (c->type == ACT_CHP_RECV && d->getDir() == Type::direction::IN && _chp_expanding_macro == 2)) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Receive operation on an output channel.\n");
	fprintf (stderr, "\tChannel: ");
	ret->u.comm.chan->Print (stderr);
	fprintf (stderr, "\n");
	exit (1);
      }
    }
    ret->u.comm.flavor = c->u.comm.flavor;
    ret->u.comm.convert = c->u.comm.convert;
    if (c->u.comm.var) {
      ret->u.comm.var = expand_var_write (c->u.comm.var, ns, s);
      act_chp_macro_check (s, ret->u.comm.var);
      
      ActId *tmp = ret->u.comm.var->stripArray();

      if (tmp->isDynamicDeref()) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Structure dynamic array access currently unsupported.\n");
	fprintf (stderr, "\tVariable: ");
	ret->u.comm.var->Print (stderr);
	fprintf (stderr, "\n");
	exit (1);
      }
      
      act_connection *d = tmp->Canonical (s);
      if (d->getDir() == Type::direction::IN) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Communication action writes an input variable.\n");
	fprintf (stderr, "\tChannel: ");
	ret->u.comm.chan->Print (stderr);
	fprintf (stderr, "; variable: ");
	ret->u.comm.var->Print (stderr);
	fprintf (stderr, "\n");
	exit (1);
      }
      delete tmp;
    }
    else {
      ret->u.comm.var = NULL;
    }
    if (c->u.comm.e) {
      ret->u.comm.e = chp_expr_expand (c->u.comm.e, ns, s);
    }
    else {
      ret->u.comm.e = NULL;
    }
    break;
    
  case ACT_CHP_FUNC:
    ret->u.func.name = c->u.func.name;
    ret->u.func.rhs = list_new ();
    for (li = list_first (c->u.func.rhs); li; li = list_next (li)) {
      act_func_arguments_t *arg, *ra;
      NEW (arg, act_func_arguments_t);
      ra = (act_func_arguments_t *) list_value (li);
      arg->isstring = ra->isstring;
      if (ra->isstring) {
	arg->u.s = ra->u.s;
      }
      else {
	arg->u.e = chp_expr_expand (ra->u.e, ns, s);
      }
      list_append (ret->u.func.rhs, arg);
    }
    break;

  case ACT_CHP_MACRO:
    {
      ActId *x = expand_var_read (c->u.macro.id, ns, s);
      if (x->isNamespace()) {
	act_error_ctxt (stderr);
	fatal_error ("CHP macro and namespace globals don't mix");
      }
      InstType *it = s->FullLookup (x, NULL);
      delete x;
      x = NULL;
      
	
      Assert (TypeFactory::isUserType (it), "This should have been caught earlier!");
      UserDef *u = dynamic_cast <UserDef *> (it->BaseType());
      Assert (u, "Hmm");
      UserMacro *um = u->getMacro (string_char (c->u.macro.name));
      if (!um) {
	char buf[1024];
	c->u.macro.id->sPrint (buf, 1024);
	act_error_ctxt (stderr);
	fprintf (stderr, "Macro name `%s' not found for instance `%s' (type `%s')\n",
		 string_char (c->u.macro.name), buf, u->getName());
	exit (1);
      }

      if (um->getNumPorts() !=
	  (c->u.macro.rhs ? list_length (c->u.macro.rhs) : 0)) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Macro ``%s'': mismatch in number of arguments (requires %d)\n",
		 um->getName(), um->getNumPorts());
	exit (1);
      }

      /* expand all parameters */
      Scope *tsc = new Scope (s, 1);
      act_inline_table *tab;
      tab = act_inline_new (tsc, NULL);
      if (um->getNumPorts() > 0) {
	listitem_t *li = list_first (c->u.macro.rhs);
	for (int i=0; i < um->getNumPorts(); i++) {
	  Expr **etmp;
	  MALLOC (etmp, Expr *, 1);
	  etmp[0] = chp_expr_expand ((Expr *)list_value (li), ns, s);
	  int tr;

	  /* -- typecheck -- */
	  tr = act_type_expr (s, etmp[0], NULL);
	  if (!T_BASETYPE_ISINTBOOL (tr)) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Typechecking failed in macro (%s) argument #%d\n", um->getName(), i);
	    if (tr == T_ERR) {
	      fprintf (stderr, "  %s\n", act_type_errmsg());
	    }
	    fprintf (stderr, "\tType must be int or bool\n");
	    exit (1);
	  }

	  if ((T_BASETYPE_INT (tr) && !TypeFactory::isIntType (um->getPortType (i))) ||
	      (T_BASETYPE_BOOL (tr) && !TypeFactory::isBoolType (um->getPortType (i)))) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Typechecking failed in macro (%s) argument #%d\n\t", um->getName(), i);
	    fprintf (stderr, "\tint/bool mismatch\n");
	    exit (1);
	  }	    
	  
	  
	  tsc->Add (um->getPortName (i), um->getPortType (i));
	  ActId *tmp = new ActId (um->getPortName (i));
	  act_inline_setval (tab, tmp, etmp);
	  delete tmp;
	  FREE (etmp);
	  li = list_next (li);
	}
      }

      /* now bind parameters to symbols */
      FREE (ret);
      ret = um->substitute (c->u.macro.id, tab);

      /*-- re-expand, do all the checks --*/
      {
	int tmp = Act::double_expand;
	act_chp_lang_t *oret = ret;
	Act::double_expand = 0;
	ret = chp_expand (oret, ns, s);
	Act::double_expand = tmp;
	act_chp_free (oret);
      }
      
      delete tsc;
      act_inline_free (tab);
    }
    break;

  case ACT_HSE_FRAGMENTS:
    ret->u.frag.nextlabel = c->u.frag.nextlabel;
    ret->u.frag.body = chp_expand (c->u.frag.body, ns, s);
    ret->u.frag.next = chp_expand (c->u.frag.next, ns, s);
    break;
    
  default:
    fatal_error ("Unknown chp type %d", ret->type);
    break;
  }
  return ret;
}

static void _chp_print (FILE *fp, act_chp_lang_t *c, int prec = 0)
{
  int lprec;
  
  if (!c) return;

  if (c->label) {
    fprintf (fp, "%s:", c->label);
  }
  
  switch (c->type) {
  case ACT_CHP_COMMALOOP:
  case ACT_CHP_SEMILOOP:
    fprintf (fp, "(");
    if (c->type == ACT_CHP_COMMALOOP) {
      fprintf (fp, ",");
    }
    else {
      fprintf (fp, ";");
    }
    fprintf (fp, "%s", c->u.loop.id);
    fprintf (fp, ":");
    print_expr (fp, c->u.loop.lo);
    if (c->u.loop.hi) {
      fprintf (fp, "..");
      print_expr (fp, c->u.loop.hi);
    }
    fprintf (fp, ":");
    _chp_print (fp, c->u.loop.body);
    fprintf (fp, ")");
    break;
    
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    {
      listitem_t *li;

      lprec = (c->type == ACT_CHP_SEMI ? 0 : 1);

      if (prec > lprec) {
	fprintf (fp, "(");
      }

      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	_chp_print (fp, (act_chp_lang_t *)list_value (li), lprec);
	if (list_next (li)) {
	  if (c->type == ACT_CHP_COMMA) {
	    fprintf (fp, ",");
	  }
	  else {
	    fprintf (fp, ";");
	  }
	}
      }

      if (prec > lprec) {
	fprintf (fp, ")");
      }

    }
    break;

  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    fprintf (fp, "*");
  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    fprintf (fp, "[");
    if (c->type == ACT_CHP_SELECT_NONDET) {
      fprintf (fp, "|");
    }
    {
      act_chp_gc_t *gc = c->u.gc;

      if (c->type == ACT_CHP_DOLOOP) {
	fprintf (fp, " ");
	_chp_print (fp, gc->s, 0);
	fprintf (fp, " <- ");
	if (gc->g) {
	  print_uexpr (fp, gc->g);
	}
	else {
	  fprintf (fp, "true");
	}
      }
      else {
	while (gc) {
	  if (!gc->g) {
	    if (c->type == ACT_CHP_LOOP) {
	      fprintf (fp, "true");
	    }
	    else {
	      fprintf (fp, "else");
	    }
	  }
	  else {
	    print_uexpr (fp, gc->g);
	  }
	  if (gc->s) {
	    fprintf (fp, " -> ");
	    _chp_print (fp, gc->s, 0);
	  }
	  if (gc->next) {
	    fprintf (fp, " [] ");
	  }
	  gc = gc->next;
	}
      }
    }
    if (c->type == ACT_CHP_SELECT_NONDET) {
      fprintf (fp, "|");
    }
    fprintf (fp, "]");
    break;
    
  case ACT_CHP_SKIP:
    fprintf (fp, "skip");
    break;

  case ACT_CHP_ASSIGN:
  case ACT_CHP_ASSIGNSELF:
    c->u.assign.id->Print (fp);
    fprintf (fp, ":=");
    print_uexpr (fp, c->u.assign.e);
    break;
    
  case ACT_CHP_SEND:
    c->u.comm.chan->Print (fp);
    fprintf (fp, "!");
    if (c->u.comm.flavor == 1) {
      fprintf (fp, "+");
    }
    else if (c->u.comm.flavor == 2) {
      fprintf (fp, "-");
    }
    {
      listitem_t *li;
      if (c->u.comm.e) {
	print_uexpr (fp, c->u.comm.e);
      }
      if (c->u.comm.var) {
	fprintf (fp, "?");
	if (c->u.comm.flavor == 1) {
	  fprintf (fp, "+");
	}
	else if (c->u.comm.flavor == 2) {
	  fprintf (fp, "-");
	}
	if (c->u.comm.convert == 1) {
	  fprintf (fp, "bool(");
	}
	else if (c->u.comm.convert == 2) {
	  fprintf (fp, "int(");
	}
	c->u.comm.var->Print (fp);
	if (c->u.comm.convert != 0) {
	  fprintf (fp, ")");
	}
      }
    }
    break;
    
  case ACT_CHP_RECV:
    c->u.comm.chan->Print (fp);
    fprintf (fp, "?");
    if (c->u.comm.flavor == 1) {
      fprintf (fp, "+");
    }
    else if (c->u.comm.flavor == 2) {
      fprintf (fp, "-");
    }
    {
      if (c->u.comm.var) {
	if (c->u.comm.convert == 1) {
	  fprintf (fp, "bool(");
	}
	else if (c->u.comm.convert == 2) {
	  fprintf (fp, "int(");
	}
	c->u.comm.var->Print (fp);
	if (c->u.comm.convert != 0) {
	  fprintf (fp, ")");
	}
      }
      if (c->u.comm.e) {
	fprintf (fp, "!");
	if (c->u.comm.flavor == 1) {
	  fprintf (fp, "+");
	}
	else if (c->u.comm.flavor == 2) {
	  fprintf (fp, "-");
	}
	print_uexpr (fp, c->u.comm.e);
      }
    }
    break;

  case ACT_CHP_FUNC:
    fprintf (fp, "%s(", string_char (c->u.func.name));
    for (listitem_t *li = list_first (c->u.func.rhs); li; li = list_next (li)) {
      act_func_arguments_t *a = (act_func_arguments_t *) list_value (li);
      if (li != list_first (c->u.func.rhs)) {
	fprintf (fp, ",");
      }
      if (a->isstring) {
	fprintf (fp, "\"%s\"", string_char (a->u.s));
      }
      else {
	print_uexpr (fp, a->u.e);
      }
    }
    fprintf (fp, ")");
    break;

  case ACT_CHP_HOLE: /* to support verification */
    fprintf (fp, "_");
    break;
    
  case ACT_CHP_MACRO:
    c->u.macro.id->Print (fp);
    fprintf (fp, ".%s(", string_char (c->u.macro.name));
    if (c->u.macro.rhs) {
      listitem_t *li;
      for (li = list_first (c->u.macro.rhs); li; li = list_next (li)) {
	print_expr (fp, (Expr *)list_value (li));
	if (list_next (li)) {
	  fprintf (fp, ",");
	}
      }
    }
    fprintf (fp, ")");
    break;

  case ACT_HSE_FRAGMENTS:
    do {
      _chp_print (fp, c->u.frag.body);
      fprintf (fp, " : %s", c->u.frag.nextlabel);
      c = c->u.frag.next;
      if (c) {
	fprintf (fp, ";\n%s : ", c->label);
      }
    } while (c);
    break; 
    
  default:
    fatal_error ("Unknown type");
    break;
  }
}

void chp_print (FILE *fp, act_chp_lang_t *c)
{
  _chp_print (fp, c);
}

void chp_print (FILE *fp, act_chp *chp)
{
  if (chp) {
    fprintf (fp, "chp {\n");
    chp_print (fp, chp->c);
    fprintf (fp, "\n}\n");
    //chp = chp->next;
  }
}

static void chp_check_var (ActId *id, Scope *s)
{
  if (id->isFragmented (s)) {
    act_error_ctxt (stderr);
    fprintf (stderr, "In CHP, fragmented access `");
    id->Print (stderr);
    fprintf (stderr, "' to base identifier `");
    id = id->unFragment (s);
    id->Print (stderr);
    fprintf (stderr, "'\n");
    fatal_error ("Cannot access channel/integer fragments in CHP!");
  }
}

static void chp_check_expr (Expr *e, Scope *s)
{
  if (!e) return;
  
  switch (e->type) {
  case E_AND:
  case E_OR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    chp_check_expr (e->u.e.l, s);
    chp_check_expr (e->u.e.r, s);
    break;

  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    chp_check_expr (e->u.e.l, s);
    break;

  case E_QUERY:
    chp_check_expr (e->u.e.l, s);
    chp_check_expr (e->u.e.r->u.e.l, s);
    chp_check_expr (e->u.e.r->u.e.r, s);
    break;

  case E_FUNCTION:
    Assert (!e->u.fn.r || e->u.fn.r->type == E_LT, "Function expansion");
    e = e->u.fn.r;
    while (e) {
      chp_check_expr (e->u.e.l, s);
      e = e->u.e.r;
    }
    break;

  case E_PROBE:
    break;

  case E_VAR:
    chp_check_var ((ActId *)e->u.e.l, s);
    break;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    chp_check_expr (e->u.e.l, s);
    break;

  case E_TRUE:
  case E_FALSE:
  case E_INT:
    break;

  case E_CONCAT:
    while (e) {
      chp_check_expr (e->u.e.l, s);
      e = e->u.e.r;
    }
    break;
    
  case E_BITFIELD:
    chp_check_var ((ActId *)e->u.e.l, s);
    break;

  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    break;
  }
}

void chp_check_channels (act_chp_lang_t *c, Scope *s)
{
  listitem_t *li;
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      chp_check_channels ((act_chp_lang_t *) list_value (li), s);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (act_chp_gc_t *gctmp = c->u.gc; gctmp; gctmp = gctmp->next) {
      chp_check_channels (gctmp->s, s);
      chp_check_expr (gctmp->g, s);
    }
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    chp_check_var (c->u.assign.id, s);
    chp_check_expr (c->u.assign.e, s);
    break;
    
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    if (c->u.comm.e) {
      chp_check_expr (c->u.comm.e, s);
    }
    if (c->u.comm.var) {
      chp_check_var (c->u.comm.var, s);
    }
    break;

  default:
    break;
  }
}
