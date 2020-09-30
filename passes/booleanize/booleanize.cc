/*************************************************************************
 *
 *  Copyright (c) 2019-2020 Rajit Manohar
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
#include "booleanize.h"
#include <map>
#include <string.h>
#include "hash.h"
#include "qops.h"
#include "config.h"
#include <act/iter.h>

/*-- variables --*/

static act_booleanized_var_t *var_alloc (act_boolean_netlist_t *n,
					 act_connection *c)
{
  act_booleanized_var_t *v;
  
  Assert (c == c->primary(), "What?");

  NEW (v, act_booleanized_var_t);
  v->id = c;
  v->input = 0;
  v->output = 0;
  v->used = 0;
  v->ischan = 0;
  v->isint = 0;
  v->usedchp = 0;
  v->isglobal = 0;
  v->isport = 0;
  v->ischpport = 0;
  v->isfragmented = 0;
  v->extra = NULL;
  return v;
}

static act_booleanized_var_t *_var_lookup (act_boolean_netlist_t *n,
					  act_connection *c)
{
  ihash_bucket_t *b;

  if (!c) return NULL;

  c = c->primary();

  b = ihash_lookup (n->cH, (long)c);
  if (!b) {
    act_booleanized_var_t *v;
    b = ihash_add (n->cH, (long)c);
    v = var_alloc (n, c);
    b->v = v;
    if (c->isglobal()) {
      v->isglobal = 1;
    }
    ActId *tmp = c->toid();
    InstType *xit;
    act_type_var (n->cur, tmp, &xit);
    if (TypeFactory::isChanType (xit)) {
      v->ischan = 1;
      v->width = TypeFactory::bitWidth (xit);
    }
    else if (TypeFactory::isIntType (xit) ||
	     (TypeFactory::isDataType (xit) &&
	      !TypeFactory::boolType (xit))) {
      v->isint = 1;
      v->width = TypeFactory::bitWidth (xit);
    }
    delete xit;
    delete tmp;
  }
  return (act_booleanized_var_t *)b->v;
}

static act_booleanized_var_t *raw_lookup (act_boolean_netlist_t *n,
					  act_connection *c)
{
  ihash_bucket_t *b;

  if (!c) return NULL;

  c = c->primary();

  b = ihash_lookup (n->cH, (long)c);
  if (!b) {
    return NULL;
  }
  return (act_booleanized_var_t *)b->v;
}


static act_booleanized_var_t *var_lookup (act_boolean_netlist_t *n,
					  ActId *id)
{
  act_connection *c;

  c = id->Canonical (n->cur);
  return _var_lookup (n, c);
}

static int is_dynamic_id (ActId *id)
{
  int ret = 0;
  ActId *pr = id;
  if (id->arrayInfo()) {
    if (id->arrayInfo()->isDynamicDeref()) {
      ret = 1;
    }
  }
  id = id->Rest();
  while (id) {
    if (id->arrayInfo()) {
      if (id->arrayInfo()->isDynamicDeref()) {
	fprintf (stderr, "In examining ID: ");
	pr->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Only simple (first-level) dynamic derefs are supported");
      }
    }
    id = id->Rest();
  }
  return ret;
}

/*
 * mark variables by walking expressions
 */
static void walk_expr (act_boolean_netlist_t *N, act_prs_expr_t *e)
{
  act_booleanized_var_t *v;
  if (!e) return;

  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    walk_expr (N, e->u.e.l);
    walk_expr (N, e->u.e.r);
    if (e->u.e.pchg_type != -1) {
      walk_expr (N, e->u.e.pchg);
    }
    break;
    
  case ACT_PRS_EXPR_NOT:
    walk_expr (N, e->u.e.l);
    break;
    
  case ACT_PRS_EXPR_VAR:
    v = var_lookup (N, e->u.v.id);
    v->used = 1;
    v->input = 1;
    break;
      
  case ACT_PRS_EXPR_LABEL:
    break;
    
  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;
    
  default:
    fatal_error ("Unknown type");
    break;
  }
}

static void generate_prs_vars (act_boolean_netlist_t *N,
				act_prs_lang_t *p)
{
  if (!p) return;
  act_attr_t *attr;
  act_booleanized_var_t *v;

  switch (p->type) {
  case ACT_PRS_RULE:
    walk_expr (N, p->u.one.e);

    if (!p->u.one.label) {
      v = var_lookup (N, p->u.one.id);
      v->used = 1;
      v->output = 1;
    }
    break;

  case ACT_PRS_GATE:
    if (p->u.p.g) {
      v = var_lookup (N, p->u.p.s);
      v->used = 1;
      v->input = 1;
      v = var_lookup (N, p->u.p.d);
      v->used = 1;
      v->input = 1;
      v = var_lookup (N, p->u.p.g);
      v->used = 1;
      v->input = 1;
    }
    if (p->u.p._g) {
      v = var_lookup (N, p->u.p.s);
      v->used = 1;
      v->input = 1;
      v = var_lookup (N, p->u.p.d);
      v->used = 1;
      v->input = 1;
      v = var_lookup (N, p->u.p._g);
      v->used = 1;
      v->input = 1;
    }

    for (attr = p->u.p.attr; attr; attr = attr->next) {
      if (strcmp (attr->attr, "output") == 0) {
	unsigned int v = attr->e->u.v;
	if (v & 0x1) {
	  act_booleanized_var_t *x = var_lookup (N, p->u.p.s);
	  x->used = 1;
	  x->output = 1;
	}
	if (v & 0x2) {
	  act_booleanized_var_t *x = var_lookup (N, p->u.p.d);
	  x->used = 1;
	  x->output = 1;
	}
      }
    }
    break;
    
  case ACT_PRS_TREE:
    act_prs_lang_t *x;
    for (x = p->u.l.p; x; x = x->next) {
      generate_prs_vars (N, x);
    }
    break;
    
  case ACT_PRS_SUBCKT:
    /* handle elsewhere */
    warning("subckt { } in prs bodies is ignored; use defcell instead");
    for (act_prs_lang_t *x = p->u.l.p; x; x = x->next) {
      generate_prs_vars (N, x);
    }
    break;

  default:
    fatal_error ("Should not be here\n");
    break;
  }
}


static void generate_expr_vars (act_boolean_netlist_t *N, Expr *e, int ischp)
{
  if (!e) return;
  
  switch (e->type) {
  case E_AND:
  case E_OR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV:
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_NE:
  case E_EQ:
    generate_expr_vars (N, e->u.e.l, ischp);
    generate_expr_vars (N, e->u.e.r, ischp);
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
    generate_expr_vars (N, e->u.e.l, ischp);
    break;

  case E_BITFIELD:
    {
      act_booleanized_var_t *v;
      v = var_lookup (N, (ActId *)e->u.e.l);
      v->input = 1;
      if (ischp) {
	v->usedchp = 1;
      }
      else {
	v->used = 1;
      }
      v->isint = 1;
    }
    Assert (e->u.e.r, "What?");
    break;

  case E_QUERY:
    generate_expr_vars (N, e->u.e.l, ischp);
    Assert (e->u.e.r && e->u.e.r->u.e.l, "What?");
    Assert (e->u.e.r && e->u.e.r->u.e.r, "What?");
    generate_expr_vars (N, e->u.e.r->u.e.l, ischp);
    generate_expr_vars (N, e->u.e.r->u.e.r, ischp);
    break;

  case E_CONCAT:
    do {
      generate_expr_vars (N, e->u.e.l, ischp);
      e = e->u.e.r;
    } while (e);
    break;

  case E_FUNCTION:
    e = e->u.e.r;
    while (e) {
      generate_expr_vars (N, e->u.e.l, ischp);
      e = e->u.e.r;
    }
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
  case E_SELF:
    break;

  case E_PROBE:
  case E_VAR:
    {
      act_booleanized_var_t *v;
      act_dynamic_var_t *dv;

      /*-- check if the Act ID has a dynamic dereference; this is only
	   permitted in CHP bodies 
	   --*/
      if (!is_dynamic_id ((ActId *)e->u.e.l)) {
	v = var_lookup (N, (ActId *)e->u.e.l);
	if (e->type == E_VAR) {
	  v->input = 1;
	}
	if (ischp) {
	  v->usedchp = 1;
	}
	else {
	  v->used = 1;
	}
      }
      else {
	if (!ischp) {
	  fprintf (stderr, "ID: ");
	  ((ActId *)e->u.e.l)->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Dynamic de-reference not permitted in a non-CHP description");
	}
      }
    }
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return;
}

static act_dynamic_var_t *get_dynamic_id (act_boolean_netlist_t *N,
					  ActId *id)
{
  ihash_bucket_t *b;
  ActId *tmp = new ActId (id->getName());
  act_connection *c = tmp->Canonical (N->cur);
  delete tmp;

  b = ihash_lookup (N->cdH, (long)c);
  if (b) {
    return (act_dynamic_var_t *)b->v;
  }
  else {
    return NULL;
  }
}

static void _add_dynamic_id (act_boolean_netlist_t *N, ActId *id)
{
  ActId *tmp = new ActId (id->getName());
  act_connection *c = tmp->Canonical (N->cur);
  InstType *it;

  act_type_var (N->cur, tmp, &it);

  delete tmp;

  ihash_bucket_t *b;

  b = ihash_lookup (N->cdH, (long)c);
  if (b) {
    delete it;
    return;
  }
  else {
    act_dynamic_var_t *v;
    b = ihash_add (N->cdH, (long)c);
    NEW (v, act_dynamic_var_t);
    v->id = c;
    if (TypeFactory::boolType (it)) {
      v->isint = 0;
    }
    else {
      v->isint = 1;
      v->width = TypeFactory::bitWidth (it);
    }
    Assert (it->arrayInfo(), "What?");
    v->size = it->arrayInfo()->size();
    b->v = v;
  }
  delete it;
  return;
}


static void collect_chp_expr_vars (act_boolean_netlist_t *N, Expr *e)
{
  if (!e) return;
  
  switch (e->type) {
  case E_AND:
  case E_OR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV:
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_NE:
  case E_EQ:
    collect_chp_expr_vars (N, e->u.e.l);
    collect_chp_expr_vars (N, e->u.e.r);
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
    collect_chp_expr_vars (N, e->u.e.l);
    break;

  case E_BITFIELD:
    /* check ActId * e->u.e.l */
    Assert (e->u.e.r, "What?");
    break;

  case E_QUERY:
    collect_chp_expr_vars (N, e->u.e.l);
    Assert (e->u.e.r && e->u.e.r->u.e.l, "What?");
    Assert (e->u.e.r && e->u.e.r->u.e.r, "What?");
    collect_chp_expr_vars (N, e->u.e.r->u.e.l);
    collect_chp_expr_vars (N, e->u.e.r->u.e.r);
    break;

  case E_CONCAT:
    /* no dynamic vars */
    break;

  case E_FUNCTION:
    e = e->u.e.r;
    while (e) {
      collect_chp_expr_vars (N, e->u.e.l);
      e = e->u.e.r;
    }
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
  case E_SELF:
    break;

  case E_PROBE:
    if (is_dynamic_id ((ActId *)e->u.e.l)) {
      fprintf (stderr, "ID: ");
      ((ActId *)e->u.e.l)->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Dynamic de-reference only permitted for data types");
    }
    break;
    
  case E_VAR:
    if (is_dynamic_id ((ActId *)e->u.e.l)) {
      _add_dynamic_id (N, ((ActId *)e->u.e.l));
    }
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return;
}

static void generate_chp_expr_vars (act_boolean_netlist_t *N, Expr *e)
{
  generate_expr_vars (N, e, 1);
}

static void generate_hse_expr_vars (act_boolean_netlist_t *N, Expr *e)
{
  generate_expr_vars (N, e, 0);
}
  
static void generate_hse_vars (act_boolean_netlist_t *N,
			       act_chp_lang_t *c)
{
  if (!c) return;
  act_booleanized_var_t *v;

  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    {
      listitem_t *li;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	generate_hse_vars (N, (act_chp_lang_t *) list_value (li));
      }
    }
    break;
    
  case ACT_CHP_SELECT:
  case ACT_CHP_DOLOOP:
  case ACT_CHP_LOOP:
    {
      act_chp_gc_t *gc;
      for (gc = c->u.gc; gc; gc = gc->next) {
	generate_hse_expr_vars (N, gc->g);
	generate_hse_vars (N, gc->s);
      }
    }
    break;
    
  case ACT_CHP_SKIP:
    return;
    
  case ACT_CHP_FUNC:
    /* fix this later; these are built-in functions only */
    break;

  case ACT_CHP_ASSIGN:
    {
      act_booleanized_var_t *v;
      v = var_lookup (N, c->u.assign.id);
      Assert (v, "What?");
      v->used = 1;
      v->output = 1;
      generate_hse_expr_vars (N, c->u.assign.e);
    }
    break;
    
  default:
    fatal_error ("Sholud be expanded already?");
    break;
  }
}

static void generate_chp_vars (act_boolean_netlist_t *N,
			       act_chp_lang_t *c)
{
  if (!c) return;
  act_booleanized_var_t *v;

  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    {
      listitem_t *li;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	generate_chp_vars (N, (act_chp_lang_t *) list_value (li));
      }
    }
    break;
    
  case ACT_CHP_SELECT:
  case ACT_CHP_DOLOOP:
  case ACT_CHP_LOOP:
    {
      act_chp_gc_t *gc;
      for (gc = c->u.gc; gc; gc = gc->next) {
	generate_chp_expr_vars (N, gc->g);
	generate_chp_vars (N, gc->s);
      }
    }
    break;
    
  case ACT_CHP_SKIP:
    return;
    
  case ACT_CHP_FUNC:
    /* XXX: fix this later; these are built-in functions only */
    break;

  case ACT_CHP_ASSIGN:
    {
      act_booleanized_var_t *v;
      v = var_lookup (N, c->u.assign.id);
      Assert (v, "What?");
      v->usedchp = 1;
      v->output = 1;
      generate_chp_expr_vars (N, c->u.assign.e);
    }
    break;

  case ACT_CHP_SEND:
    {
      act_booleanized_var_t *v;
      listitem_t *li;
      v = var_lookup (N, c->u.comm.chan);
      Assert (v, "What?");
      v->usedchp = 1;
      v->output = 1;
      for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
	generate_chp_expr_vars (N, (Expr *) list_value (li));
      }
    }
    break;

  case ACT_CHP_RECV:
    {
      act_booleanized_var_t *v;
      listitem_t *li;
      v = var_lookup (N, c->u.comm.chan);
      Assert (v, "What?");
      v->usedchp = 1;
      v->input = 1;
      for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
	v = var_lookup (N, (ActId *) list_value (li));
	Assert (v, "what?");
	v->usedchp = 1;
	v->output = 1;
      }
    }
    break;
    
  default:
    fatal_error ("Sholud be expanded already?");
    break;
  }
}

static void collect_chp_dynamic_vars (act_boolean_netlist_t *N,
				      act_chp_lang_t *c)
{
  if (!c) return;

  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    {
      listitem_t *li;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	collect_chp_dynamic_vars (N, (act_chp_lang_t *) list_value (li));
      }
    }
    break;
    
  case ACT_CHP_SELECT:
  case ACT_CHP_DOLOOP:
  case ACT_CHP_LOOP:
    {
      act_chp_gc_t *gc;
      for (gc = c->u.gc; gc; gc = gc->next) {
	collect_chp_expr_vars (N, gc->g);
	collect_chp_dynamic_vars (N, gc->s);
      }
    }
    break;
    
  case ACT_CHP_SKIP:
    return;
    
  case ACT_CHP_FUNC:
    /* XXX: fix this later; these are built-in functions only */
    break;

  case ACT_CHP_ASSIGN:
    {
      if (is_dynamic_id (c->u.assign.id)) {
	_add_dynamic_id (N, c->u.assign.id);
      }
      collect_chp_expr_vars (N, c->u.assign.e);
    }
    break;

  case ACT_CHP_SEND:
    {
      listitem_t *li;
      if (is_dynamic_id (c->u.comm.chan)) {
	fprintf (stderr, "ID: ");
	c->u.comm.chan->Print (stderr);
	fatal_error ("Dynamic reference not permitted for channels");
      }
      for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
	collect_chp_expr_vars (N, (Expr *) list_value (li));
      }
    }
    break;

  case ACT_CHP_RECV:
    {
      listitem_t *li;
      if (is_dynamic_id (c->u.comm.chan)) {
	fprintf (stderr, "ID: ");
	c->u.comm.chan->Print (stderr);
	fatal_error ("Dynamic reference not permitted for channels");
      }
      for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
	/* check list_value (li) */
      }
    }
    break;
    
  default:
    fatal_error ("Sholud be expanded already?");
    break;
  }
}


static void generate_dflow_vars (act_boolean_netlist_t *N,
				 act_dataflow_element *e)
{
  if (!e) return;
  act_booleanized_var_t *v;

  switch (e->t) {
  case ACT_DFLOW_FUNC:
    generate_chp_expr_vars (N, e->u.func.lhs);
    v = var_lookup (N, e->u.func.rhs);
    Assert (v, "Hmm");
    v->usedchp = 1;
    v->output = 1;
    break;

  case ACT_DFLOW_SPLIT:
  case ACT_DFLOW_MERGE:
  case ACT_DFLOW_MIXER:
  case ACT_DFLOW_ARBITER:
    if (e->u.splitmerge.guard) {
      v = var_lookup (N, e->u.splitmerge.guard);
      if (v) {
	v->usedchp = 1;
	v->input = 1;
      }
    }
    v = var_lookup (N, e->u.splitmerge.single);
    Assert (v, "WHat?");
    v->usedchp = 1;
    if (e->t == ACT_DFLOW_SPLIT) {
      v->input = 1;
    }
    else {
      v->output = 1;
    }
    for (int i=0; i < e->u.splitmerge.nmulti; i++) {
      v = var_lookup (N, e->u.splitmerge.multi[i]);
      if (v) {
	v->usedchp = 1;
	if (e->t == ACT_DFLOW_SPLIT) {
	  v->output = 1;
	}
	else {
	  v->input = 1;
	}
      }
    }
    break;

  default:
    fatal_error ("Unknown type");
    break;
  }
  return;
}

static void process_prs_lang (act_boolean_netlist_t *N, act_prs *p)
{
  /* walk through each PRS block, and mark all variables that are
     there as used, as well as set their input/output flag */
  while (p) {
    act_prs_lang_t *prs;
    
    if (p->vdd) {
      act_booleanized_var_t *v = var_lookup (N, p->vdd);
      v->used = 1;
      v->input = 1;
    }
    if (p->gnd) {
      act_booleanized_var_t *v = var_lookup (N, p->gnd);
      v->used = 1;
      v->input = 1;
    }
    if (p->psc) {
      act_booleanized_var_t *v = var_lookup (N, p->psc);
      v->used = 1;
      v->input = 1;
    }
    if (p->nsc) {
      act_booleanized_var_t *v = var_lookup (N, p->nsc);
      v->used = 1;
      v->input = 1;
    }
    
    for (prs = p->p; prs; prs = prs->next) {
      generate_prs_vars (N, prs);
      N->isempty = 0;
    }
    p = p->next;
  }
}

static void process_hse_lang (act_boolean_netlist_t *N, act_chp *c)
{
  if (c) {
    if (c->vdd) {
      act_booleanized_var_t *v = var_lookup (N, c->vdd);
      v->used = 1;
      v->input = 1;
    }
    if (c->gnd) {
      act_booleanized_var_t *v = var_lookup (N, c->gnd);
      v->used = 1;
      v->input = 1;
    }
    if (c->psc) {
      act_booleanized_var_t *v = var_lookup (N, c->psc);
      v->used = 1;
      v->input = 1;
    }
    if (c->nsc) {
      act_booleanized_var_t *v = var_lookup (N, c->nsc);
      v->used = 1;
      v->input = 1;
    }
    generate_hse_vars (N, c->c);
    N->isempty = 0;
    //c = c->next;
  }
}

static void process_chp_lang (act_boolean_netlist_t *N, act_chp *c)
{
  if (c) {
    if (c->vdd) {
      act_booleanized_var_t *v = var_lookup (N, c->vdd);
      v->usedchp = 1;
      v->input = 1;
    }
    if (c->gnd) {
      act_booleanized_var_t *v = var_lookup (N, c->gnd);
      v->usedchp = 1;
      v->input = 1;
    }
    if (c->psc) {
      act_booleanized_var_t *v = var_lookup (N, c->psc);
      v->usedchp = 1;
      v->input = 1;
    }
    if (c->nsc) {
      act_booleanized_var_t *v = var_lookup (N, c->nsc);
      v->usedchp = 1;
      v->input = 1;
    }
    collect_chp_dynamic_vars (N, c->c);
    generate_chp_vars (N, c->c);
    N->isempty = 0;
    //c = c->next;
  }
}

static void process_dflow_lang (act_boolean_netlist_t *N, act_dataflow *df)
{
  if (df) {
    listitem_t *li;
    for (li = list_first (df->dflow); li; li = list_next (li)) {
      act_dataflow_element *e = (act_dataflow_element *)list_value (li);
      generate_dflow_vars (N, e);
      N->isempty = 0;
    }
  }
}

static act_boolean_netlist_t *process_local_lang (Act *a, Process *proc)
{
  act_boolean_netlist_t *N;
  Scope *cur;
  act_languages *lang;

  if (proc) {
    lang = proc->getlang();
    cur = proc->CurScope();
  }
  else {
    /* global namespace */
    lang = ActNamespace::Global()->getlang();
    cur = ActNamespace::Global()->CurScope();
  }

  /*-- initialize netlist --*/

  NEW (N, act_boolean_netlist_t);

  A_INIT (N->ports);
  A_INIT (N->chpports);
  A_INIT (N->instports);
  A_INIT (N->instchpports);
  A_INIT (N->nets);
  A_INIT (N->used_globals);
  
  N->p = proc;
  N->cur = cur;
  N->visited = 0;
  N->cH = ihash_new (32);
  N->cdH = ihash_new (4);
  N->isempty = 1;

  
  /*-- process all local variables that are used --*/
  if (lang) {
    process_prs_lang (N, lang->getprs());
    process_hse_lang (N, lang->gethse());
    process_chp_lang (N, lang->getchp());
    process_dflow_lang (N, lang->getdflow());
  }
  return N;
}

act_boolean_netlist_t *ActBooleanizePass::_create_local_bools (Process *p)
{
  act_boolean_netlist_t *n;
  int subinst = 0;
  Scope *sc;

  sc = p ? p->CurScope() : ActNamespace::Global()->CurScope();

  /*--- 
    initialize netlist with used flags for all local languages that
    specify circuits
    ---*/
  n = process_local_lang (a, p);

  ActInstiter i(sc);

  /*-- mark ports to instances as used --*/
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      Process *sub = dynamic_cast<Process *>(vx->t->BaseType());
      Assert (sub, "What?");
      subinst = 1;
      if (p) {
	update_used_flags (n, vx, p);
      }
    }
  }

  /*-- create elaborated port list --*/
  if (p) {
    flatten_ports_to_bools (n, NULL, sc, p, 0);
  }

  /*-- collect globals --*/
  ihash_iter_t iter;
  ihash_bucket_t *b;
  ihash_iter_init (n->cH, &iter);
  while ((b = ihash_iter_next (n->cH, &iter))) {
    act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;
    if (!v->output) {
      Assert (v->input == 1, "What?");
    }
    if (v->id->isglobal()) {
      A_NEWM (n->used_globals, act_connection *);
      A_NEXT (n->used_globals) = v->id;
      A_INC (n->used_globals);
    }

    /*-- now check if this is fragmented --*/
    act_connection *c = (act_connection *)b->key;
    if (!c->hasSubconnections()) {
      v->isfragmented = 0;
    }
    else {
      if (c->vx) {
	if (!c->vx->t->arrayInfo()) {
	  v->isfragmented = 1;
	}
	else {
	  for (int i=0; i <  c->numSubconnections(); i++) {
	    if (c->hasSubconnections (i)) {
	      if (c->a[i]->a) {
		v->isfragmented = 1;
		break;
	      }
	    }
	  }
	}
      }
      else {
	v->isfragmented = 1;
      }
    }
  }

  if (subinst) {
    n->isempty = 0;
  }

  return n;
}


/*
 * type = 0 : boolean flag
 * type = 1 : chp flag
 */
static void mark_c_used (act_boolean_netlist_t *n,
			 act_boolean_netlist_t *subinst,
			 act_connection *c,
			 int *count, int type)
{
  act_booleanized_var_t *v = raw_lookup (n, c);

  if (type == 0) {
    A_NEW (n->instports, act_connection *);
    A_NEXT (n->instports) = c;
    A_INC (n->instports);
  }
  else {
    A_NEW (n->instchpports, act_connection *);
    A_NEXT (n->instchpports) = c;
    A_INC (n->instchpports);
  }

  if (v) {
    if (type == 0) {
      v->used = 1;
      if (subinst->ports[*count].input) {
	v->input = 1;
      }
      else {
	v->output = 1;
      }
    }
    else {
      v->usedchp = 1;
      if (subinst->chpports[*count].input) {
	v->input = 1;
      }
      else {
	v->output = 1;
      }
    }
  }
  else {
    v = _var_lookup (n, c);
    if (type == 0) {
      v->used = 1;
      if (!subinst->ports[*count].input) {
	v->output = 1;
      }
      else {
	v->input = 1;
      }
    }
    else if (type == 1) {
      v->usedchp = 1;
      if (!subinst->chpports[*count].input) {
	v->output = 1;
      }
      else {
	v->input = 1;
      }
    }
    if (c->isglobal()) {
      int i;
      for (i=0; i < A_LEN (n->used_globals); i++) {
	if (c == n->used_globals[i])
	  break;
      }
      if (i == A_LEN (n->used_globals)) {
	A_NEWM (n->used_globals, act_connection *);
	A_NEXT (n->used_globals) = c;
	A_INC (n->used_globals);
      }
    }
  }
}


void *ActBooleanizePass::local_op (Process *p, int mode)
{
  if (mode == 0) {
    return _create_local_bools (p);
  }
  else if (mode == 1) {
    _createNets (p);
    return getMap (p);
  }
  else {
    fatal_error ("Unknown mode?");
    return NULL;
  }
}

/*
 *  mode = 0 : both chp and bool
 *  mode = 1 : bool only
 *  mode = 2 : chp only
 */
void ActBooleanizePass::append_base_port (act_boolean_netlist_t *n,
					  act_connection *c, Type *t,
					  int mode)
{
  int i;
  int dir = c->getDir();

  if (mode != 2) {
    A_NEWM (n->ports, struct netlist_bool_port);
    A_NEXT (n->ports).c = c;
    A_NEXT (n->ports).omit = 0;
    A_NEXT (n->ports).input = 0;
    if (dir == Type::IN) {
      A_NEXT (n->ports).input = 1;
    }
    else if (dir == Type::OUT) {
      A_NEXT (n->ports).input = 0;
    }
    A_NEXT (n->ports).netid = -1;
    A_INC (n->ports);
  }

  if (mode != 1) {
    A_NEWM (n->chpports, struct netlist_bool_port);
    A_NEXT (n->chpports).c = c;
    A_NEXT (n->chpports).omit = 0;
    A_NEXT (n->chpports).input = 0;
    if (dir == Type::IN) {
      A_NEXT (n->chpports).input = 1;
    }
    else if (dir == Type::OUT) {
      A_NEXT (n->chpports).input = 0;
    }
    A_NEXT (n->chpports).netid = -1;
    A_INC (n->chpports);
  }

  if (c->isglobal()) {
    /* globals do not need to be in the port list */
    if (mode != 2) {
      A_LAST (n->ports).omit = 1;
    }
    if (mode != 1) {
      A_LAST (n->chpports).omit = 1;
    }
    return;
  }

  if (black_box_mode && n->p->isBlackBox()) {
    /* assume this is needed! */
    return;
  }

  int bool_done = 0;
  int chp_done = 0;
  ihash_bucket_t *b;
  b = ihash_lookup (n->cH, (long)c);
  if (b) {
    act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;
    if (!v->used) {
      if (mode != 2) {
	A_LAST (n->ports).omit = 1;
	bool_done = 1;
      }
    }
    if (!v->usedchp) {
      if (mode != 1) {
	A_LAST (n->chpports).omit = 1;
	chp_done = 1;
      }
    }
    if (mode != 2 && !bool_done) {
      v->used = 1;
      v->isport = 1;
      A_LAST (n->ports).input = (v->input && !v->output) ? 1 : 0;
    }
    if (mode != 1 && !chp_done) {
      v->usedchp = 1;
      v->ischpport = 1;
      A_LAST (n->chpports).input = (v->input && !v->output) ? 1 : 0;

      if (TypeFactory::isIntType (t) ||
	  (TypeFactory::isDataType (t) && !TypeFactory::boolType (t))) {
	v->isint = 1;
	v->width = TypeFactory::bitWidth (t);
      }
      else if (TypeFactory::isChanType (t)) {
	v->ischan = 1;
	v->width = TypeFactory::bitWidth (t);
      }
    }
  }
  else {
    /* connection pointers that were not found were also not used! */
    if (mode != 2) {
      A_LAST (n->ports).omit = 1;
    }
    if (mode != 1) {
      A_LAST (n->chpports).omit = 1;
    }
    return;
  }

  if (!bool_done && mode != 2) {
    for (i=0; i < A_LEN (n->ports)-1; i++) {
      /* check to see if this is already in the port list;
	 make this faster with a map if necessary since it is O(n^2) */
      if (c == n->ports[i].c) {
	A_LAST (n->ports).omit = 1;
	return;
      }
    }
  }

  if (!chp_done && mode != 1) {
    for (i=0; i < A_LEN (n->chpports)-1; i++) {
      /* check to see if this is already in the port list;
	 make this faster with a map if necessary since it is O(n^2) */
      if (c == n->chpports[i].c) {
	A_LAST (n->chpports).omit = 1;
	return;
      }
    }
  }
}


/*
       n : the current netlist where the ports have to be flattened
  prefix : the prefix to the current instance
       s : the parent scope of the current user defined object
       u : the current user-defined object
   nochp : flag saying we've broken down a channel so skip for chp
*/

void ActBooleanizePass::flatten_ports_to_bools (act_boolean_netlist_t *n,
						ActId *prefix,
						Scope *s, UserDef *u,
						int nochp)
{
  int i;

  Assert (u, "Hmm...");
  
  for (i=0; i < u->getNumPorts(); i++) {
    InstType *it;
    const char *name;
    ActId *sub, *tail;
    act_connection *c;

    name = u->getPortName (i);
    it = u->getPortType (i);

    if (prefix) {
      sub = prefix->Clone ();

      tail = sub;
      while (tail->Rest()) {
	tail = tail->Rest();
      }
      tail->Append (new ActId (name));
      tail = tail->Rest();
    }
    else {
      sub = new ActId (name);
      tail = sub;
    }
      
    /* if it is a complex type, we need to traverse it! */
    if (TypeFactory::isUserType (it)) {
      Arraystep *step;
      if (it->arrayInfo()) {
	step = it->arrayInfo()->stepper();
      }
      else {
	step = NULL;
      }
      if (!step || !step->isend()) {
	do {
	  Array *t;
	  if (step) {
	    t = step->toArray ();
	  }
	  else {
	    t = NULL;
	  }
	  tail->setArray (t);
	  if (TypeFactory::isChanType (it)) {
	    flatten_ports_to_bools (n, sub, s,
				    dynamic_cast<UserDef *>(it->BaseType ()),
				    1);
	    c = sub->Canonical (s);
	    Assert (c == c->primary(), "What?");
	    append_base_port (n, c, it->BaseType(), 2);
	  }
	  else {
	    flatten_ports_to_bools (n, sub, s,
				    dynamic_cast<UserDef *>(it->BaseType ()),
				    nochp);
	  }
	  tail->setArray (NULL);
	  if (t) {
	    delete t;
	  }
	  if (step) {
	    step->step();
	  }
	}
	while (step && !step->isend());
      }
      if (step) {
	delete step;
      }
    }
    else if (TypeFactory::isBoolType (it) || TypeFactory::isChanType (it) ||
	     TypeFactory::isIntType (it)) {
      Type *itbase = it->BaseType();
      Arraystep *step;
      if (it->arrayInfo()) {
	step = it->arrayInfo()->stepper();
      }
      else {
	step = NULL;
      }
      if (!step || !step->isend()) {
	do {
	  Array *t;
	  if (step) {
	    t = step->toArray ();
	  }
	  else {
	    t = NULL;
	  }
	  tail->setArray (t);
	  c = sub->Canonical (s);
	  Assert (c == c->primary (), "What?");
	  if (nochp) {
	    /* only bools */
	    append_base_port (n, c, itbase, 1);
	  }
	  else {
	    /* both bools and chp */
	    append_base_port (n, c, itbase, 0);
	  }
	  tail->setArray (NULL);
	  if (t) {
	    delete t;
	  }
	  if (step) {
	    step->step();
	  }
	} while (step && !step->isend());
      }
      if (step) {
	delete step;
      }
    }
    else {
      fatal_error ("This cannot handle non-int/bool/chan types; everything must be reducible to a built-in type.");
    }
    delete sub;
  }
}


/*
 *
 *  Recursive update of port used flags
 *
 *        n : netlist where the update should be done
 *  subinst : netlist of current instance
 *   prefix : prefix to current instance
 *        s : scope in which the instance exists
 *        u : the user-defined type corresponding to the prefix
 *    count : in/out used to track instports to make things simpler
 *            for other tools
 *   counr2 : same as count, for chp ports
 *
 *
 */
void ActBooleanizePass::rec_update_used_flags (act_boolean_netlist_t *n,
					       act_boolean_netlist_t *subinst,
					       ActId *prefix,
					       Scope *s, UserDef *u,
					       int *count, int *count2)
{
  int i;

  Assert (u, "Hmm...");
  Assert (prefix, "Hmm");

  /*
   *
   * If a type is a channel, we stop at the channel abstraction for
   * CHP ports.
   *
   */
  
  for (i=0; i < u->getNumPorts(); i++) {
    InstType *it;
    const char *name;
    ActId *sub, *tail;
    act_connection *c;

    name = u->getPortName (i);
    it = u->getPortType (i);

    sub = prefix->Clone ();

    tail = sub;
    while (tail->Rest()) {
      tail = tail->Rest();
    }
    tail->Append (new ActId (name));
    tail = tail->Rest();
      
    /* if it is a complex type, we need to traverse it! */
    if (TypeFactory::isUserType (it)) {
      Arraystep *step;
      if (it->arrayInfo()) {
	step = it->arrayInfo()->stepper();
      }
      else {
	step = NULL;
      }
      if (!step || !step->isend()) {
	do {
	  Array *t;
	  if (step) {
	    t = step->toArray();
	  }
	  else {
	    t = NULL;
	  }
	  tail->setArray (t);

	  int *cnt = count2;
	  if (TypeFactory::isChanType (it)) {
	    /* at this point, we don't add to chp ports */
	    cnt = NULL;
	  }
	  rec_update_used_flags (n, subinst, sub, s,
				 dynamic_cast<UserDef *>(it->BaseType ()),
				 count, cnt);

	  if (!cnt && count2) {
	    /* we set the chp port here, as we are in a base case */
	    Assert (*count2 < A_LEN (subinst->chpports), "What?");
	    if (!subinst->chpports[*count2].omit) {
	      c = sub->Canonical (s);
	      Assert (c == c->primary (), "What?");
	      /* mark c as used */
	      mark_c_used (n, subinst, c, count, 1);
	    }
	    *count2 = *count2 + 1;
	  }

	  tail->setArray (NULL);
	  if (t) {
	    delete t;
	  }
	  if (step) {
	    step->step();
	  }
	} while (step && !step->isend());
      }
      if (step) {
	delete step;
      }
    }
    else if (TypeFactory::isBoolType (it) || TypeFactory::isChanType (it) ||
	     TypeFactory::isIntType (it)) {
      /* now check! */
      Arraystep *step;
      if (it->arrayInfo()) {
	step = it->arrayInfo()->stepper();
      }
      else {
	step = NULL;
      }
      if (!step || !step->isend()) {
	do {
	  Array *t;
	  if (step) {
	    t = step->toArray();
	  }
	  else {
	    t = NULL;
	  }
	  Assert (*count < A_LEN (subinst->ports), "What?");
	  tail->setArray (t);
	  if (!subinst->ports[*count].omit) {
	    c = sub->Canonical (s);
	    Assert (c == c->primary (), "What?");
	    /* mark c as used */
	    mark_c_used (n, subinst, c, count, 0);
	  }
	  if (count2 && !subinst->chpports[*count2].omit) {
	    c = sub->Canonical (s);
	    Assert (c == c->primary(), "What?");
	    mark_c_used (n, subinst, c, count2, 1);
	  }
	  tail->setArray (NULL);
	  *count = *count + 1;
	  if (count2) {
	    *count2 = *count2 + 1;
	  }
	  if (t) {
	    delete t;
	  }
	  if (step) {
	    step->step();
	  }
	} while (step && !step->isend());
      }
      if (step) {
	delete step;
      }
    }
    else {
      fatal_error ("This cannot handle int/enumerated types; everything must be reducible to bool");
    }
    delete sub;
  }

  /* globals propagate up the hierarchy */
  for (i=0; i < A_LEN (subinst->used_globals); i++) {
    int j;
    for (j=0; j < A_LEN (n->used_globals); j++) {
      if (n->used_globals[j] == subinst->used_globals[i])
	break;
    }
    if (j == A_LEN (n->used_globals)) {
      A_NEWM (n->used_globals, act_connection *);
      A_NEXT (n->used_globals) = subinst->used_globals[i];
      A_INC (n->used_globals);
    }
  }
}



void ActBooleanizePass::update_used_flags (act_boolean_netlist_t *n,
					   ValueIdx *vx, Process *p)
{
  ActId *id;
  int count, count2;
  act_boolean_netlist_t *subinst;
  Scope *sc;

  /* vx is a process instance in process p.
     
     Update used flags in the current netlist n, based on connections
     to the ports of vx
   */

  sc = p ? p->CurScope() : ActNamespace::Global()->CurScope();
  
  id = new ActId (vx->getName());
  subinst = (act_boolean_netlist_t *)
    getMap (dynamic_cast<Process *>(vx->t->BaseType()));
  Assert (subinst, "What?");

  Arraystep *as;

  if (vx->t->arrayInfo()) {
    as = vx->t->arrayInfo()->stepper();
  }
  else {
    as = NULL;
  }

  if (!as || !as->isend()) {
    do {
      Array *t;
      if (as) {
	t = as->toArray();
      }
      else {
	t = NULL;
      }

      id->setArray (t);
      count = 0;
      count2 = 0;
      rec_update_used_flags (n, subinst, id, sc,
			     dynamic_cast<UserDef *>(vx->t->BaseType()),
			     &count, &count2);
      id->setArray (NULL);

      if (t) {
	delete t;
      }
      if (as) {
	as->step();
      }
    } while (as && !as->isend());
  }
  if (as) {
    delete as;
  }
  delete id;
}


void ActBooleanizePass::free_local (void *v)
{
  act_boolean_netlist_t *n = (act_boolean_netlist_t *)v;
  int i;
  ihash_bucket_t *b;
  ihash_iter_t iter;
  
  if (!n) return;

  Assert (n->cH, "Hmm");

  ihash_iter_init (n->cH, &iter);
  while ((b = ihash_iter_next (n->cH, &iter))) {
    act_booleanized_var_t *v = (act_booleanized_var_t *) b->v;
    FREE (v);
  }
  ihash_free (n->cH);
  A_FREE (n->ports);
  A_FREE (n->instports);
  A_FREE (n->nets);
  FREE (n);
}

ActBooleanizePass::ActBooleanizePass(Act *a) : ActPass(a, "booleanize")
{
  /* setup config option */
  if (config_exists ("net.black_box_mode")) {
    black_box_mode = config_get_int ("net.black_box_mode");
  }
  else {
    black_box_mode = 1;
    config_set_default_int ("net.black_box_mode", 1);
  }
}

ActBooleanizePass::~ActBooleanizePass()
{
}


int ActBooleanizePass::run (Process *p)
{
  return ActPass::run (p);
}


act_boolean_netlist_t *ActBooleanizePass::getBNL (Process *p)
{
  if (!completed()) {
    fatal_error ("Called ActBooleanizePass::getBNL() while the pass was running!");
  }
  return (act_boolean_netlist_t *)getMap (p);
}


/*-- local operator for creating all nets --*/
void ActBooleanizePass::_createNets (Process *p)
{
  act_boolean_netlist_t *n = (act_boolean_netlist_t *) getMap (p);
  Assert (n, "What?");

  ActInstiter i(p ? p->CurScope() : a->Global()->CurScope());

  int iport = 0;
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = (*i);
    act_boolean_netlist_t *sub;
    Process *instproc;
    Arraystep *as;
    
    if (!TypeFactory::isProcessType (vx->t)) continue;

    instproc = dynamic_cast<Process *>(vx->t->BaseType());
    sub = (act_boolean_netlist_t *) getMap (instproc);
    Assert (sub, "What?");

    if (vx->t->arrayInfo()) {
      as = vx->t->arrayInfo()->stepper();
    }
    else {
      as = NULL;
    }
    if (!as || !as->isend()) {
      do {
	Array *tmpa;
	tmpa = as ? as->toArray() : NULL;

	for (int j=0; j < A_LEN (sub->ports); j++) {
	  if (sub->ports[j].omit) continue;

	  int netid = addNet (n, n->instports[iport]);

	  if (sub->ports[j].netid == -1) {
	    /* there is nothing to be done here */
	    addPin (n, netid, vx->getName(), tmpa, sub->ports[j].c);
	  }
	  else {
	    importPins (n, netid, vx->getName(), tmpa,
			&sub->nets[sub->ports[j].netid]);
	  }
	  for (int k=0; k < A_LEN (n->ports); k++) {
	    if (n->ports[k].c == n->instports[iport]) {
	      n->ports[k].netid = netid;
	      break;
	    }
	  }
	  iport++;
	}

	/*-- global nets --*/
	for (int j=0; j < A_LEN (sub->nets); j++) {
	  if (!sub->nets[j].net->isglobal()) continue;
	  int k;
	  for (k=0; k < A_LEN (sub->ports); k++) {
	    if (sub->ports[k].c == sub->nets[j].net) break;
	  }
	  if (k == A_LEN (sub->ports)) {
	    /* global net, not in port list */
	    int netid = addNet (n, sub->nets[j].net);
	    importPins (n, netid, vx->getName(), tmpa, &sub->nets[j]);
	    sub->nets[j].skip = 1;
	  }
	}
	if (as) {
	  as->step();
	}
      } while (as && !as->isend());
    }
    if (as) {
      delete as;
    }
  }
  Assert (iport == A_LEN (n->instports), "What?");

  for (int i=0; i < A_LEN (n->ports); i++) {
    if (n->ports[i].netid != -1) {
      n->nets[n->ports[i].netid].port = 1;
    }
  }
}

void ActBooleanizePass::createNets (Process *p)
{
  run_recursive (p, 1);
}


int ActBooleanizePass::addNet (act_boolean_netlist_t *n, act_connection *c)
{
  int i;
  for (i=0; i < A_LEN (n->nets); i++) {
    if (n->nets[i].net == c) return i;
  }
  A_NEW (n->nets, act_local_net_t);
  A_NEXT (n->nets).net = c;
  A_NEXT (n->nets).skip = 0;
  A_NEXT (n->nets).port = 0;
  A_INIT (A_NEXT (n->nets).pins);
  A_INC (n->nets);
  return A_LEN (n->nets)-1;
}

void ActBooleanizePass::addPin (act_boolean_netlist_t *n,
				int netid,
				const char *name, Array *a,
				act_connection *pin)
{
  Assert (0 <= netid && netid < A_LEN (n->nets), "What?");

  A_NEWM (n->nets[netid].pins, act_local_pin_t);

  ActId *inst = new ActId (name);
  inst->setArray (a);
  A_NEXT (n->nets[netid].pins).inst = inst;
  A_NEXT (n->nets[netid].pins).pin = pin;
  A_INC (n->nets[netid].pins);
}

void ActBooleanizePass::importPins (act_boolean_netlist_t *n,
				    int netid,
				    const char *name, Array *a,
				    act_local_net_t *net)
{
  Assert (0 <= netid && netid < A_LEN (n->nets), "What?");

  for (int i=0; i < A_LEN (net->pins); i++) {
    ActId *inst;
    /* orig name becomes <name>.<orig> */
    A_NEW (n->nets[netid].pins, act_local_pin_t);

    inst = new ActId (name);
    inst->setArray (a);
    inst->Append (net->pins[i].inst);
    A_NEXT (n->nets[netid].pins).inst = inst;
    A_NEXT (n->nets[netid].pins).pin = net->pins[i].pin;
    A_INC (n->nets[netid].pins);
  }
}
