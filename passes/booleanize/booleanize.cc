/*************************************************************************
 *
 *  Copyright (c) 2019 Rajit Manohar
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
  v->used = 1;
  v->ischan = 0;
  v->isint = 0;
  v->input = 0;
  v->output = 0;
  v->extra = NULL;
  return v;
}

static act_booleanized_var_t *var_lookup (act_boolean_netlist_t *n,
					  act_connection *c)
{
  ihash_bucket_t *b;

  if (!c) return NULL;

  c = c->primary();

  b = ihash_lookup (n->cH, (long)c);
  if (!b) {
    b = ihash_add (n->cH, (long)c);
    b->v = var_alloc (n, c);
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
  return var_lookup (n, c);
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
    v = var_lookup (N, e->u.v.id->Canonical (N->cur));
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
      v->output = 1;
    }
    break;

  case ACT_PRS_GATE:
    if (p->u.p.g) {
      v = var_lookup (N, p->u.p.s);
      v->input = 1;
      v = var_lookup (N, p->u.p.d);
      v->input = 1;
      v = var_lookup (N, p->u.p.g);
      v->input = 1;
    }
    if (p->u.p._g) {
      v = var_lookup (N, p->u.p.s);
      v->input = 1;
      v = var_lookup (N, p->u.p.d);
      v->input = 1;
      v = var_lookup (N, p->u.p._g);
      v->input = 1;
    }

    for (attr = p->u.p.attr; attr; attr = attr->next) {
      if (strcmp (attr->attr, "output") == 0) {
	unsigned int v = attr->e->u.v;
	if (v & 0x1) {
	  act_booleanized_var_t *x = var_lookup (N, p->u.p.s);
	  x->output = 1;
	}
	if (v & 0x2) {
	  act_booleanized_var_t *x = var_lookup (N, p->u.p.d);
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

static void generate_hse_vars (act_boolean_netlist_t *N,
			       act_chp_lang_t *c)
{
  if (!c) return;
  act_booleanized_var_t *v;

  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
  case ACT_CHP_SELECT:
  case ACT_CHP_DOLOOP:
  case ACT_CHP_LOOP:
  case ACT_CHP_SKIP:
  case ACT_CHP_FUNC:
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

}

static void generate_dflow_vars (act_boolean_netlist_t *N,
				 act_dataflow_element *e)
{
  if (!e) return;
  act_booleanized_var_t *v;

}

static void process_prs_lang (act_boolean_netlist_t *N, act_prs *p)
{
  /* walk through each PRS block, and mark all variables that are
     there as used, as well as set their input/output flag */
  while (p) {
    act_prs_lang_t *prs;
    
    if (p->vdd) {
      act_booleanized_var_t *v = var_lookup (N, p->vdd);
      v->input = 1;
    }
    if (p->gnd) {
      act_booleanized_var_t *v = var_lookup (N, p->gnd);
      v->input = 1;
    }
    if (p->psc) {
      act_booleanized_var_t *v = var_lookup (N, p->psc);
      v->input = 1;
    }
    if (p->nsc) {
      act_booleanized_var_t *v = var_lookup (N, p->nsc);
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
  while (c) {
    if (c->vdd) {
      act_booleanized_var_t *v = var_lookup (N, c->vdd);
      v->input = 1;
    }
    if (c->gnd) {
      act_booleanized_var_t *v = var_lookup (N, c->gnd);
      v->input = 1;
    }
    if (c->psc) {
      act_booleanized_var_t *v = var_lookup (N, c->psc);
      v->input = 1;
    }
    if (c->nsc) {
      act_booleanized_var_t *v = var_lookup (N, c->nsc);
      v->input = 1;
    }
    generate_hse_vars (N, c->c);
    N->isempty = 0;
    c = c->next;
  }
}

static void process_chp_lang (act_boolean_netlist_t *N, act_chp *c)
{
  while (c) {
    if (c->vdd) {
      act_booleanized_var_t *v = var_lookup (N, c->vdd);
      v->input = 1;
    }
    if (c->gnd) {
      act_booleanized_var_t *v = var_lookup (N, c->gnd);
      v->input = 1;
    }
    if (c->psc) {
      act_booleanized_var_t *v = var_lookup (N, c->psc);
      v->input = 1;
    }
    if (c->nsc) {
      act_booleanized_var_t *v = var_lookup (N, c->nsc);
      v->input = 1;
    }
    generate_chp_vars (N, c->c);
    N->isempty = 0;
    c = c->next;
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
  N->uH = ihash_new (4);
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
  for (int i=0; i < n->cH->size; i++) {
    for (ihash_bucket_t *b = n->cH->head[i]; b; b = b->next) {
      act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;
      if (!v->output) {
	Assert (v->input == 1, "What?");
      }
      if (v->id->isglobal()) {
	A_NEWM (n->used_globals, act_connection *);
	A_NEXT (n->used_globals) = v->id;
	A_INC (n->used_globals);
      }
    }
  }
  
  for (int i=0; i < n->uH->size; i++) {
    for (ihash_bucket_t *b = n->uH->head[i]; b; b = b->next) {
      act_connection *c = (act_connection *)b->key;
      if (c->isglobal()) {
	int j;
	for (j=0; j < A_LEN (n->used_globals); j++) {
	  if (n->used_globals[j] == c)
	    break;
	}
	if (j == A_LEN (n->used_globals)) {
	  A_NEWM (n->used_globals, act_connection *);
	  A_NEXT (n->used_globals) = c;
	  A_INC (n->used_globals);
	}
      }
    }
  }

  if (subinst) {
    n->isempty = 0;
  }
  
  return n;
}



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
    v->used = 1;
    if (type == 0) {
      if (subinst->ports[*count].input) {
	v->input = 1;
      }
      else {
	v->output = 1;
      }
    }
    else {
      if (subinst->chpports[*count].input) {
	v->input = 1;
      }
      else {
	v->output = 1;
      }
    }
  }
  else {
    ihash_bucket_t *b;
    b = ihash_lookup (n->uH, (long)c);
    if (!b) {
      b = ihash_add (n->uH, (long)c);
      b->i = 0;
    }
    if (type == 0) {
      if (!subinst->ports[*count].input) {
	b->i = 1;
      }
    }
    else if (type == 1) {
      if (!subinst->chpports[*count].input) {
	b->i = 1;
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

  ihash_bucket_t *b;
  b = ihash_lookup (n->cH, (long)c);
  if (b) {
    act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;
    if (!v->used) {
      b = ihash_lookup (n->uH, (long)c);
      if (!b) {
	if (mode != 2) {
	  A_LAST (n->ports).omit = 1;
	}
	if (mode != 1) {
	  A_LAST (n->chpports).omit = 1;
	}
	return;
      }
      else {
	if (b->i) {
	  if (mode != 2) {
	    A_LAST (n->ports).input = 0;
	  }
	  if (mode != 1) {
	    A_LAST (n->chpports).input = 0;
	  }
	}
	else {
	  if (mode != 2) {
	    A_LAST (n->ports).input = 1;
	  }
	  if (mode != 1) {
	    A_LAST (n->chpports).input = 1;
	  }
	}
      }
    }
    v->used = 1;
    if (v->input && !v->output) {
      if (mode != 2) {
	A_LAST (n->ports).input = 1;
      }
      if (mode != 1) {
	A_LAST (n->chpports).input = 1;
      }
    }
    if (TypeFactory::isIntType (t)) {
      v->isint = 1;
      v->width = TypeFactory::bitWidth (t);
    }
    else if (TypeFactory::isChanType (t)) {
      v->ischan = 1;
      v->width = TypeFactory::bitWidth (t);
    }
  }
  else {
    /* connection pointers that were not found were also not used! */
    b = ihash_lookup (n->uH, (long)c);
    if (!b) {
      if (mode != 2) {
	A_LAST (n->ports).omit = 1;
      }
      if (mode != 1) {
	A_LAST (n->chpports).omit = 1;
      }
      return;
    }
    else {
      if (b->i) {
	if (mode != 2) {
	  A_LAST (n->ports).input = 0;
	}
	if (mode != 1) {
	  A_LAST (n->chpports).input = 0;
	}
      }
      else {
	if (mode != 2) {
	  A_LAST (n->ports).input = 1;
	}
	if (mode != 1) {
	  A_LAST (n->chpports).input = 1;
	}
      }
    }
  }

  if (mode != 2) {
    for (i=0; i < A_LEN (n->ports)-1; i++) {
      /* check to see if this is already in the port list;
	 make this faster with a map if necessary since it is O(n^2) */
      if (c == n->ports[i].c) {
	A_LAST (n->ports).omit = 1;
	return;
      }
    }
  }

  if (mode != 1) {
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
  
  if (!n) return;

  Assert (n->cH, "Hmm");

  for (int i=0; i < n->cH->size; i++) {
    for (b = n->cH->head[i]; b; b = b->next) {
      act_booleanized_var_t *v = (act_booleanized_var_t *) b->v;
      FREE (v);
    }
  }
  ihash_free (n->cH);
  ihash_free (n->uH);
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
	if (tmpa) {
	  delete tmpa;
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
