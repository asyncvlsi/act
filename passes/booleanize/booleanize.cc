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

static std::map<Process *, act_boolean_netlist_t *> *netmap = NULL;

static int black_box_mode;

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

static act_boolean_netlist_t *walk_netgraph (Act *a, Process *proc)
{
  act_prs *p; 
  act_boolean_netlist_t *N;
  Scope *cur;

  /* 
     XXX: Need to add: chp, hse
  */

  if (proc) {
    p = proc->getprs();
    cur = proc->CurScope();
  }
  else {
    /* global namespace */
    p = ActNamespace::Global()->getprs();
    cur = ActNamespace::Global()->CurScope();
  }

  NEW (N, act_boolean_netlist_t);

  A_INIT (N->ports);
  A_INIT (N->instports);
  
  N->p = proc;
  N->cur = cur;
  N->visited = 0;
  N->cH = ihash_new (32);
  N->uH = ihash_new (4);
  N->isempty = 1;

  if (!p) {
    return N;
  }

  /* walk through each PRS block */
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

  /* now walk through the variables. if it is not an output, it is an
     input! */
  for (int i=0; i < N->cH->size; i++) {
    for (ihash_bucket_t *b = N->cH->head[i]; b; b = b->next) {
      act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;
      if (!v->output) {
	v->input = 1;
      }
    }
  }  

  return N;
}

static void generate_netbools (Act *a, Process *p)
{
  int subinst = 0;
  Assert (p->isExpanded(), "Process must be expanded!");

  if (netmap->find(p) != netmap->end()) {
    /* handled this already */
    return;
  }

  /* Create netlist for all sub-processes */
  ActInstiter i(p->CurScope());

  /* handle all processes instantiated by this one */
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      generate_netbools (a, dynamic_cast<Process *>(vx->t->BaseType()));
      subinst = 1;
    }
  }

  act_boolean_netlist_t *n = walk_netgraph (a, p);

  (*netmap)[p] = n;

  if (subinst) {
    n->isempty = 0;
  }

  return;
}




static void mark_c_used (act_boolean_netlist_t *n,
			 act_boolean_netlist_t *subinst,
			 act_connection *c,
			 int *count)
{
  act_booleanized_var_t *v = raw_lookup (n, c);

  A_NEW (n->instports, act_connection *);
  A_NEXT (n->instports) = c;
  A_INC (n->instports);

  if (v) {
    v->used = 1;
    if (subinst->ports[*count].input) {
      v->input = 1;
    }
    else {
      v->output = 1;
    }
  }
  else {
    ihash_bucket_t *b;
    b = ihash_lookup (n->uH, (long)c);
    if (!b) {
      b = ihash_add (n->uH, (long)c);
      b->v = NULL;
    }
    if (!subinst->ports[*count].input) {
      b->v = (void *)1;
    }
  }
}


static void append_bool_port (act_boolean_netlist_t *n, act_connection *c)
{
  int i;

  A_NEWM (n->ports, struct netlist_bool_port);
  A_NEXT (n->ports).c = c;
  A_NEXT (n->ports).omit = 0;
  A_NEXT (n->ports).input = 0;
  A_INC (n->ports);

  if (c->isglobal()) {
    /* globals do not need to be in the port list */
    A_LAST (n->ports).omit = 1;
    return;
  }

  if (black_box_mode && n->isempty) {
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
	A_LAST (n->ports).omit = 1;
	return;
      }
      else {
	if (b->v) {
	  A_LAST (n->ports).input = 0;
	}
	else {
	  A_LAST (n->ports).input = 1;
	}
      }
    }
    if (v->input && !v->output) {
      A_LAST (n->ports).input = 1;
    }
  }
  else {
    /* connection pointers that were not found were also not used! */
    b = ihash_lookup (n->uH, (long)c);
    if (!b) {
      A_LAST (n->ports).omit = 1;
      return;
    }
    else {
      if (b->v) {
	A_LAST (n->ports).input = 0;
      }
      else {
	A_LAST (n->ports).input = 1;
      }
    }
  }
  
  for (i=0; i < A_LEN (n->ports)-1; i++) {
    /* check to see if this is already in the port list;
       make this faster with a map if necessary since it is O(n^2) */
    if (c == n->ports[i].c) {
      A_LAST (n->ports).omit = 1;
      return;
    }
  }
}


static void flatten_ports_to_bools (act_boolean_netlist_t *n, ActId *prefix,
				    Scope *s, UserDef *u)
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
      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	while (!step->isend()) {
	  Array *t = step->toArray ();
	  tail->setArray (t);
	  flatten_ports_to_bools (n, sub, s,
				  dynamic_cast<UserDef *>(it->BaseType ()));
	  delete t;
	  tail->setArray (NULL);
	  step->step();
	}
      }
      else {
	flatten_ports_to_bools (n, sub, s,
				dynamic_cast<UserDef *>(it->BaseType ()));
      }
    }
    else if (TypeFactory::isBoolType (it) || TypeFactory::isChanType (it) ||
	     TypeFactory::isIntType (it)) {
      /* now check! */
      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	while (!step->isend()) {
	  Array *t = step->toArray ();
	  tail->setArray (t);
	  c = sub->Canonical (s);
	  Assert (c == c->primary (), "What?");
	  append_bool_port (n, c);
	  delete t;
	  tail->setArray (NULL);
	  step->step();
	}
	delete step;
      }
      else {
	c = sub->Canonical (s);
	Assert (c == c->primary(), "What?");
	append_bool_port (n, c);
      }
    }
    else {
      fatal_error ("This cannot handle int/enumerated types; everything must be reducible to bool");
    }
    delete sub;
  }
}



static void rec_update_used_flags (act_boolean_netlist_t *n,
				   act_boolean_netlist_t *subinst,
				   ActId *prefix,
				   Scope *s, UserDef *u, int *count)
{
  int i;

  Assert (u, "Hmm...");
  Assert (prefix, "Hmm");
  
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
      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	while (!step->isend()) {
	  Array *t = step->toArray ();
	  tail->setArray (t);
	  rec_update_used_flags (n, subinst, sub, s,
				 dynamic_cast<UserDef *>(it->BaseType ()),
				 count);
	  delete t;
	  tail->setArray (NULL);
	  step->step();
	}
      }
      else {
	rec_update_used_flags (n, subinst, sub, s,
			       dynamic_cast<UserDef *>(it->BaseType ()), count);

      }
    }
    else if (TypeFactory::isBoolType (it) || TypeFactory::isChanType (it) ||
	     TypeFactory::isIntType (it)) {
      /* now check! */
      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	while (!step->isend()) {
	  Assert (*count < A_LEN (subinst->ports), "What?");
	  if (!subinst->ports[*count].omit) {
	    Array *t = step->toArray ();
	    tail->setArray (t);
	    c = sub->Canonical (s);
	    Assert (c == c->primary (), "What?");

	    /* mark c as used */
	    mark_c_used (n, subinst, c, count);

	    delete t;
	    tail->setArray (NULL);
	  }
	  *count = *count + 1;
	  step->step();
	}
	delete step;
      }
      else {
	Assert (*count < A_LEN (subinst->ports), "Hmm");
	if (!subinst->ports[*count].omit) {
	  c = sub->Canonical (s);
	  Assert (c == c->primary(), "What?");

	  /* mark c as used */
	  mark_c_used (n, subinst, c, count);

	}
	*count = *count + 1;
      }
    }
    else {
      fatal_error ("This cannot handle int/enumerated types; everything must be reducible to bool");
    }
    delete sub;
  }
}



static void update_used_flags (act_boolean_netlist_t *n,
			       ValueIdx *vx, Process *p)
{
  ActId *id;
  int count;
  act_boolean_netlist_t *subinst;
  /* vx is a process instance in process p.
     
     Update used flags in the current netlist n, based on connections
     to the ports of vx
   */
  id = new ActId (vx->getName());

  subinst = netmap->find (dynamic_cast <Process *>(vx->t->BaseType()))->second;
  
  if (vx->t->arrayInfo()) {
    /* ok, we need an outer loop now */
    Arraystep *as = vx->t->arrayInfo()->stepper();

    while (!as->isend()) {
      Array *t = as->toArray();
      id->setArray (t);
      count = 0;
      rec_update_used_flags (n, subinst,
			     id, p->CurScope(),
			     dynamic_cast<UserDef *>(vx->t->BaseType()), &count);
      delete t;
      id->setArray (NULL);
      as->step();
    }
    delete as;
  }
  else {
    count = 0;
    rec_update_used_flags (n, subinst,
			   id, p->CurScope (),
			   dynamic_cast<UserDef *>(vx->t->BaseType()), &count);
  }
  delete id;
}


static void create_bool_ports (Act *a, Process *p)
{
  Assert (p->isExpanded(), "Process must be expanded!");
  if (netmap->find(p) == netmap->end()) {
    fprintf (stderr, "Could not find process `%s'", p->getName());
    fprintf (stderr, " in created netlists; inconstency!\n");
    fatal_error ("Internal inconsistency or error in pass ordering!");
  }
  act_boolean_netlist_t *n = netmap->find (p)->second;
  if (n->visited) return;
  n->visited = 1;

  /* emit sub-processes */
  ActInstiter i(p->CurScope());

  /* handle all processes instantiated by this one */
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      create_bool_ports (a, dynamic_cast<Process *>(vx->t->BaseType()));
      update_used_flags (n, vx, p);
    }
  }
  flatten_ports_to_bools (n, NULL, p->CurScope(), p);
  return;
}



void act_booleanize_netlist (Act *a, Process *p)
{
  std::map<Process *, act_boolean_netlist_t *> *tmp;

  tmp = (std::map<Process *, act_boolean_netlist_t *> *) a->aux_find ("booleanize");
  if (tmp) {
    /* XXX: need to free the netlist */
    delete tmp;
  }
  netmap = new std::map<Process *, act_boolean_netlist_t *>();

  if (!p) {
    ActNamespace *g = ActNamespace::Global();
    ActInstiter i(g->CurScope());

    for (i = i.begin(); i != i.end(); i++) {
      ValueIdx *vx = *i;
      if (TypeFactory::isProcessType (vx->t)) {
	Process *x = dynamic_cast<Process *>(vx->t->BaseType());
	if (x->isExpanded()) {
	  generate_netbools (a, x);
	}
      }
    }
    
    /*-- generate netlist for any prs in the global scope --*/
    act_boolean_netlist_t *N = walk_netgraph (a, NULL);
    (*netmap)[NULL] = N;
  }
  else {
    generate_netbools (a, p);
  }

  a->aux_add ("booleanize", netmap);

  /*-- Now create Boolean ports --*/
  if (config_exists ("net.black_box_mode")) {
    black_box_mode = config_get_int ("net.black_box_mode");
  }
  else {
    black_box_mode = 0;
  }
  
  /* clear visited flag */
  std::map<Process *, act_boolean_netlist_t *>::iterator it;
  for (it = netmap->begin(); it != netmap->end(); it++) {
    act_boolean_netlist_t *n = it->second;
    n->visited = 0;
  }
  if (!p) {
    ActNamespace *g = ActNamespace::Global();
    ActInstiter i(g->CurScope());

    for (i = i.begin(); i != i.end(); i++) {
      ValueIdx *vx = *i;
      if (TypeFactory::isProcessType (vx->t)) {
	Process *x = dynamic_cast<Process *>(vx->t->BaseType());
	if (x->isExpanded()) {
	  create_bool_ports (a, x);
	}
      }
    }
  }
  else {
    create_bool_ports (a, p);
  }
  for (it = netmap->begin(); it != netmap->end(); it++) {
    act_boolean_netlist_t *n = it->second;
    n->visited = 0;
  }
  
  netmap = NULL;
}
