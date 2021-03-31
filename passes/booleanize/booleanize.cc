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
#include <common/hash.h>
#include <common/qops.h>
#include <common/config.h>
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
  v->chanflag = 0;
  v->proc_in = -1;
  v->proc_out = -1;
  v->isint = 0;
  v->usedchp = 0;
  v->isglobal = 0;
  v->isport = 0;
  v->ischpport = 0;
  v->isfragmented = 0;
  v->extra = NULL;
  v->width = 1;
  v->w2 = 0;
  return v;
}

static act_booleanized_var_t *_var_lookup (act_boolean_netlist_t *n,
					  act_connection *c)
{
  phash_bucket_t *b;

  if (!c) return NULL;

  c = c->primary();

  b = phash_lookup (n->cH, c);
  if (!b) {
    act_booleanized_var_t *v;
    b = phash_add (n->cH, c);
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
	      (TypeFactory::boolType (xit) == 0))) {
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
  phash_bucket_t *b;

  if (!c) return NULL;

  c = c->primary();

  b = phash_lookup (n->cH, c);
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

static void visit_var (act_boolean_netlist_t *N, ActId *id, int isinput)
{
  act_booleanized_var_t *v;
  
  v = var_lookup (N, id);
  v->used = 1;

  if (isinput == 1) {
    v->input = 1;
  }
  else if (isinput == 0) {
    v->output = 1;
  }

  UserDef *u;
  
  if ((u = id->isFragmented (N->cur))) {
    ActId *tmp = id->unFragment (N->cur);
    v = var_lookup (N, tmp);
    v->usedchp = 1;

    Channel *uc = dynamic_cast<Channel *>(u);
    if (uc) {
      int iodir;
      
      while (tmp) {
	tmp = tmp->Rest();
	id = id->Rest ();
      }
      Assert (id, "What?");

      iodir = uc->chanDir (id, isinput);

      if (iodir & 1) {
	v->input = 1;
      }
      if (iodir & 2) {
	v->output = 1;
      }
    }
    else {
      if (isinput == 1) {
	v->input = 1;
      }
      else if (isinput == 0) {
	v->output = 1;
      }
    }
    delete tmp;
  }
}

static void visit_bool_rec (act_boolean_netlist_t *N,
			    UserDef *u, ActId *id, int isinput)
{
  ActId *tl;

  tl = id;
  while (tl->Rest()) {
    tl = tl->Rest();
  }
  
  for (int i=0; i < u->getNumPorts(); i++) {
    const char *nm = u->getPortName (i);
    InstType *it = u->getPortType (i);
    ActId *extra = new ActId (nm);
    ActId *prev;

    tl->Append (extra);
    prev = tl;
    
    tl = tl->Rest();

    if (TypeFactory::isBoolType (it)) {
      /* mark used */
      if (it->arrayInfo()) {
	Arraystep *s = it->arrayInfo()->stepper();
	while (!s->isend()) {
	  Array *a = s->toArray ();

	  tl->setArray (a);
	  visit_var (N, id, isinput);
	  tl->setArray (NULL);

	  delete a;
	  
	  s->step();
	}
	delete s;
      }
      else {
	visit_var (N, id, isinput);
      }
    }
    else if (TypeFactory::isUserType (it)) {
      UserDef *nu = dynamic_cast<UserDef *>(it->BaseType());
      Assert (nu, "What?");
      /* mark recursively */
      if (it->arrayInfo()) {
	Arraystep *s = it->arrayInfo()->stepper();
	while (!s->isend()) {
	  Array *a = s->toArray();
	  tl->setArray (a);
	  visit_bool_rec (N, nu, id, isinput);
	  tl->setArray (NULL);
	  delete a;
	  s->step();
	}
	delete s;
      }
      else {
	visit_bool_rec (N, nu, id, isinput);
      }
    }
    prev->prune();
    delete extra;
    tl = prev;
  }
}

static void visit_channel_ports (act_boolean_netlist_t *N,
				 Channel *ch, ActId *id, int isinput)
{
  ActId *tl;

  tl = id;
  while (tl->Rest()) {
    tl = tl->Rest();
  }
  
  /*-- now for each member of the channel, figure out the direction and
     then call the boolean visitor recursively --*/
  for (int i=0; i < ch->getNumPorts(); i++) {
    const char *nm = ch->getPortName (i);
    InstType *it = ch->getPortType (i);
    ActId *extra = new ActId (nm);
    ActId *prev;
    int dir;

    dir = ch->chanDir (extra, isinput);
    if (dir & 2) {
      dir = 0;
    }
    else if (dir & 1) {
      dir = 1;
    }
    else {
      dir = 0;
    }

    tl->Append (extra);
    prev = tl;
    
    tl = tl->Rest();

    if (TypeFactory::isBoolType (it)) {
      /* mark used */
      if (it->arrayInfo()) {
	Arraystep *s = it->arrayInfo()->stepper();
	while (!s->isend()) {
	  Array *a = s->toArray ();
	  tl->setArray (a);
	  visit_var (N, id, dir);
	  tl->setArray (NULL);
	  delete a;
	  s->step();
	}
	delete s;
      }
      else {
	visit_var (N, id, dir);
      }
    }
    else if (TypeFactory::isUserType (it)) {
      UserDef *nu = dynamic_cast<UserDef *>(it->BaseType());
      Assert (nu, "What?");
      /* mark recursively */
      if (it->arrayInfo()) {
	Arraystep *s = it->arrayInfo()->stepper();
	while (!s->isend()) {
	  Array *a = s->toArray();
	  tl->setArray (a);
	  visit_bool_rec (N, nu, id, dir);
	  tl->setArray (NULL);
	  delete a;
	  s->step();
	}
	delete s;
      }
      else {
	visit_bool_rec (N, nu, id, dir);
      }
    }
    prev->prune();
    delete extra;
    tl = prev;
  }
}


static act_connection *visit_chp_var (act_boolean_netlist_t *N, ActId *id, int isinput)
{
  act_booleanized_var_t *v;

  if (!id) return NULL;
  
  v = var_lookup (N, id);
  v->usedchp = 1;

  if (isinput == 1) {
    v->input = 1;
  }
  else if (isinput == 0) {
    v->output = 1;
  }

  UserDef *u = id->canFragment (N->cur);
  Channel *ch = NULL;
  if (u) {
    ch = dynamic_cast<Channel *>(u);
  }
  if (ch) {
    /* recursively access all booleans */
    visit_channel_ports (N, ch, id, isinput);
  }
  return v->id;
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
    visit_var (N, e->u.v.id, 1);
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
      visit_var (N, p->u.one.id, 0);
    }
    break;

  case ACT_PRS_GATE:
    visit_var (N, p->u.p.s, 1);
    visit_var (N, p->u.p.d, 1);
    if (p->u.p.g) {
      visit_var (N, p->u.p.g, 1);
    }
    if (p->u.p._g) {
      visit_var (N, p->u.p._g, 1);
    }

    for (attr = p->u.p.attr; attr; attr = attr->next) {
      if (strcmp (attr->attr, "output") == 0) {
	unsigned int v = attr->e->u.v;
	if (v & 0x1) {
	  visit_var (N, p->u.p.s, 0);
	}
	if (v & 0x2) {
	  visit_var (N, p->u.p.d, 0);
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
    act_error_ctxt (stderr);
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

static int _block_id;

#define _set_chan_passive_recv(x) _set_chan_dir ((x), 1)
#define _set_chan_passive_send(x) _set_chan_dir ((x), 2)

static void _set_chan_dir (act_booleanized_var_t *v, int dir)
{
  if (v->chanflag != 0 && (v->chanflag != dir)) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Channel: ");
    v->id->toid()->Print (stderr);
    fprintf (stderr, "\n");
    fatal_error ("Both ends of the channel are probed.");
  }
  v->chanflag = dir;
}

static void update_chp_expr_vars (act_boolean_netlist_t *N, Expr *e)
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
    update_chp_expr_vars (N, e->u.e.l);
    update_chp_expr_vars (N, e->u.e.r);
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
    update_chp_expr_vars (N, e->u.e.l);
    break;

  case E_BITFIELD:
    {
      act_booleanized_var_t *v;

      if (!((ActId *)e->u.e.l)->isDynamicDeref()) {
	v = var_lookup (N, (ActId *)e->u.e.l);
	if (v->ischan) {
	  _set_chan_passive_recv (v);
	}
      }
    }
    break;

  case E_QUERY:
    update_chp_expr_vars (N, e->u.e.l);
    update_chp_expr_vars (N, e->u.e.r->u.e.l);
    update_chp_expr_vars (N, e->u.e.r->u.e.r);
    break;

  case E_CONCAT:
    do {
      update_chp_expr_vars (N, e->u.e.l);
      e = e->u.e.r;
    } while (e);
    break;

  case E_FUNCTION:
    e = e->u.e.r;
    while (e) {
      update_chp_expr_vars (N, e->u.e.l);
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
    /*-- check if the Act ID has a dynamic dereference; this is only
      permitted in CHP bodies 
      --*/
    if (!((ActId *)e->u.e.l)->isDynamicDeref()) {
      act_booleanized_var_t *v;
      v = var_lookup (N, (ActId *)e->u.e.l);
      if (e->type == E_VAR) {
	v->input = 1;
	if (v->ischan) {
	  _set_chan_passive_recv (v);
	}
      }
      else {
	/* probe */
	if (v->input && !v->output) {
	  _set_chan_passive_recv (v);
	}
	else if (v->output && !v->input) {
	  _set_chan_passive_send (v);
	}
	else if (v->input && v->output) {
	  if (v->proc_in == _block_id) {
	    _set_chan_passive_recv (v);
	  }
	  else if (v->proc_out == _block_id) {
	    _set_chan_passive_send (v);
	  }
	  else {
	    act_error_ctxt (stderr);
	    warning ("Probe, send, and receive in different processes?");
	  }
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

      if (!((ActId *)e->u.e.l)->isDynamicDeref()) {
	if (ischp) {
	  visit_chp_var (N, (ActId *)e->u.e.l, 1);
	  v = var_lookup (N, (ActId *)e->u.e.l);
	  v->isint = 1;
	}
	else {
	  visit_var (N, (ActId *)e->u.e.l, 1);
	}
      }
      else {
	if (!ischp) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "ID: ");
	  ((ActId *)e->u.e.l)->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Dynamic de-reference not permitted in a non-CHP description");
	}
      }
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

      /*-- check if the Act ID has a dynamic dereference; this is only
	   permitted in CHP bodies 
	   --*/
      if (!((ActId *)e->u.e.l)->isDynamicDeref()) {
	v = var_lookup (N, (ActId *)e->u.e.l);
	if (ischp) {
	  if (e->type == E_VAR) {
	    visit_chp_var (N, (ActId *)e->u.e.l, 1);
	  }
	  else {
	    visit_chp_var (N, (ActId *)e->u.e.l, -1);
	  }
	}
	else {
	  Assert (e->type == E_VAR, "What?");
	  visit_var (N, (ActId *)e->u.e.l, 1);
	}
      }
      else {
	if (!ischp) {
	  act_error_ctxt (stderr);
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
  phash_bucket_t *b;
  ActId *tmp = new ActId (id->getName());
  act_connection *c = tmp->Canonical (N->cur);
  delete tmp;

  b = phash_lookup (N->cdH, c);
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

  if (id->Rest()) {
    act_error_ctxt (stderr);
    fprintf (stderr, "ID: ");
    id->Print (stderr);
    fprintf (stderr, "\n");
    fatal_error ("Only simple dynamic de-references permitted.");
  }

  act_type_var (N->cur, tmp, &it);

  phash_bucket_t *b;

  b = phash_lookup (N->cdH, c);
  if (b) {
    delete tmp;
    delete it;
    return;
  }
  else {
    act_dynamic_var_t *v;
    b = phash_add (N->cdH, c);
    NEW (v, act_dynamic_var_t);
    v->id = c;
    v->aid = tmp;
    v->width = 1;
    if (TypeFactory::boolType (it)) {
      v->isint = 0;
    }
    else {
      v->isint = 1;
      v->width = TypeFactory::bitWidth (it);
    }
    Assert (it->arrayInfo(), "What?");
    v->a = it->arrayInfo();
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
    if (((ActId *)e->u.e.l)->isDynamicDeref()) {
      _add_dynamic_id (N, ((ActId *)e->u.e.l));
    }
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
    while (e) {
      collect_chp_expr_vars (N, e->u.e.l);
      e = e->u.e.r;
    }
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
    if (((ActId *)e->u.e.l)->isDynamicDeref()) {
      act_error_ctxt (stderr);
      fprintf (stderr, "ID: ");
      ((ActId *)e->u.e.l)->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Dynamic de-reference only permitted for data types");
    }
    break;
    
  case E_VAR:
    if (((ActId *)e->u.e.l)->isDynamicDeref()) {
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
  case ACT_CHP_SELECT_NONDET:
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
      visit_var (N, c->u.assign.id, 0);
      generate_hse_expr_vars (N, c->u.assign.e);
    }
    break;
    
  default:
    fatal_error ("Should be expanded already?");
    break;
  }
}

static void update_chp_probes (act_boolean_netlist_t *N,
			       act_chp_lang_t *c)
{
  int pblock = _block_id;
  int changed = 0;
  if (!c) return;
  act_booleanized_var_t *v;

  switch (c->type) {
  case ACT_CHP_COMMA:
    if (pblock == -1) {
      _block_id = 0;
      changed = 1;
    }
  case ACT_CHP_SEMI:
    if (pblock == -1 && c->type == ACT_CHP_SEMI) {
      _block_id = -2;
    }
    {
      listitem_t *li;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	update_chp_probes (N, (act_chp_lang_t *) list_value (li));
	if (changed) {
	  _block_id++;
	}
      }
    }
    break;
    
  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_DOLOOP:
  case ACT_CHP_LOOP:
    if (pblock == -1) {
      _block_id = -2;
    }
    {
      act_chp_gc_t *gc;
      for (gc = c->u.gc; gc; gc = gc->next) {
	update_chp_expr_vars (N, gc->g);
	update_chp_probes (N, gc->s);
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
      if (!c->u.assign.id->isDynamicDeref()) {
	v = var_lookup (N, c->u.assign.id);
	Assert (v, "What?");

      }
      update_chp_expr_vars (N, c->u.assign.e);
    }
    break;

  case ACT_CHP_SEND:
    {
      listitem_t *li;
      li = list_first (c->u.comm.rhs);
      if (li) {
	update_chp_expr_vars (N, (Expr *) list_value (li));
      }
    }
    break;

  case ACT_CHP_RECV:
    {
      listitem_t *li;
      if (list_first (c->u.comm.rhs) &&
	  list_next (list_first (c->u.comm.rhs))) {
	update_chp_expr_vars
	  (N, (Expr *) list_value (list_next (list_first (c->u.comm.rhs))));
      }
    }
    break;
    
  default:
    fatal_error ("Sholud be expanded already?");
    break;
  }
  _block_id = pblock;
}



static void generate_chp_vars (act_boolean_netlist_t *N,
			       act_chp_lang_t *c)
{
  int pblock = _block_id;
  int changed = 0;
  if (!c) return;
  act_booleanized_var_t *v;

  switch (c->type) {
  case ACT_CHP_COMMA:
    if (pblock == -1) {
      _block_id = 0;
      changed = 1;
    }
  case ACT_CHP_SEMI:
    if (pblock == -1 && c->type == ACT_CHP_SEMI) {
      _block_id = -2;
    }
    {
      listitem_t *li;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	generate_chp_vars (N, (act_chp_lang_t *) list_value (li));
	if (changed) {
	  _block_id++;
	}
      }
    }
    break;
    
  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_DOLOOP:
  case ACT_CHP_LOOP:
    if (pblock == -1) {
      _block_id = -2;
    }
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
      if (c->u.assign.id->isDynamicDeref()) {
	_add_dynamic_id (N, c->u.assign.id);
      }
      else {
	visit_chp_var (N, c->u.assign.id, 0);
      }
      generate_chp_expr_vars (N, c->u.assign.e);
    }
    break;

  case ACT_CHP_SEND:
    {
      act_booleanized_var_t *v;
      listitem_t *li;

      visit_chp_var (N, c->u.comm.chan, 0);
      v = var_lookup (N, c->u.comm.chan);
      Assert (v, "What?");

      if (v->id->getDir() == Type::direction::IN) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Channel: ");
	v->id->toid()->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Send action on an input port!");
      }
      
      if (v->proc_out == -1 || v->proc_out == _block_id) {
	v->proc_out = _block_id;
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "Channel: ");
	v->id->toid()->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Send action in multiple concurrent blocks!");
      }
      li = list_first (c->u.comm.rhs);
      if (li) {
	generate_chp_expr_vars (N, (Expr *) list_value (li));
	li = list_next (li);
	if (li) {
	  if (((ActId *) list_value (li))->isDynamicDeref()) {
	    _add_dynamic_id (N, (ActId *) list_value (li));
	  }
	  else {
	    visit_chp_var (N, (ActId *) list_value (li), 0);
	  }
	}		   
      }
    }
    break;

  case ACT_CHP_RECV:
    {
      act_booleanized_var_t *v;
      listitem_t *li;
      visit_chp_var (N, c->u.comm.chan, 1);
      v = var_lookup (N, c->u.comm.chan);
      Assert (v, "What?");

      if (v->id->getDir() == Type::direction::OUT) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Channel: ");
	v->id->toid()->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Receive action on an output port!");
      }
      
      if (v->proc_in == -1 || v->proc_in == _block_id) {
	v->proc_in = _block_id;
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "Channel: ");
	v->id->toid()->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Receive action in multiple concurrent blocks!");
      }
      li = list_first (c->u.comm.rhs);
      if (li) {
	if (((ActId *) list_value (li))->isDynamicDeref()) {
	  _add_dynamic_id (N, (ActId *) list_value (li));
	}
	else {
	  visit_chp_var (N, (ActId *) list_value (li), 0);
	}
	li = list_next (li);
	if (li) {
	  generate_chp_expr_vars (N, (Expr *) list_value (li));
	}
      }
    }
    break;
    
  default:
    fatal_error ("Sholud be expanded already?");
    break;
  }
  _block_id = pblock;
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
  case ACT_CHP_SELECT_NONDET:
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
      if (c->u.assign.id->isDynamicDeref()) {
	_add_dynamic_id (N, c->u.assign.id);
      }
      collect_chp_expr_vars (N, c->u.assign.e);
    }
    break;

  case ACT_CHP_SEND:
    {
      listitem_t *li;
      if (c->u.comm.chan->isDynamicDeref()) {
	fprintf (stderr, "ID: ");
	c->u.comm.chan->Print (stderr);
	fatal_error ("Dynamic reference not permitted for channels");
      }
      li = list_first (c->u.comm.rhs);
      if (li) {
	collect_chp_expr_vars (N, (Expr *) list_value (li));
	li = list_next (li);
	if (li) {
	  if ( ((ActId *)list_value (li))->isDynamicDeref()) {
	    _add_dynamic_id (N, (ActId *)list_value (li));
	  }
	}
      }
    }
    break;

  case ACT_CHP_RECV:
    {
      listitem_t *li;
      if (c->u.comm.chan->isDynamicDeref()) {
	fprintf (stderr, "ID: ");
	c->u.comm.chan->Print (stderr);
	fatal_error ("Dynamic reference not permitted for channels");
      }
      li = list_first (c->u.comm.rhs);
      if (li) {
	if (((ActId *)list_value (li))->isDynamicDeref()) {
	  _add_dynamic_id (N, (ActId *)list_value (li));
	}
	li = list_next (li);
	if (li) {
	  collect_chp_expr_vars (N, (Expr *) list_value (li));
	}
      }
    }
    break;
    
  default:
    fatal_error ("Should be expanded already?");
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
    visit_chp_var (N, e->u.func.rhs, 0);
    break;

  case ACT_DFLOW_SPLIT:
  case ACT_DFLOW_MERGE:
  case ACT_DFLOW_MIXER:
  case ACT_DFLOW_ARBITER:
    if (e->u.splitmerge.guard) {
      visit_chp_var (N, e->u.splitmerge.guard, 1);
    }

    if (e->t == ACT_DFLOW_SPLIT) {
      visit_chp_var (N, e->u.splitmerge.single, 1);
    }
    else {
      visit_chp_var (N, e->u.splitmerge.single, 0);
    }
    
    for (int i=0; i < e->u.splitmerge.nmulti; i++) {
      if (e->t == ACT_DFLOW_SPLIT) {
	visit_chp_var (N, e->u.splitmerge.multi[i], 0);
      }
      else {
	visit_chp_var (N, e->u.splitmerge.multi[i], 1);
      }
    }
    break;

  case ACT_DFLOW_CLUSTER:
    {
      listitem_t *li;
      for (li = list_first (e->u.dflow_cluster); li; li = list_next (li)) {
	generate_dflow_vars (N, (act_dataflow_element *)list_value (li));
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
    _block_id = -1;
    generate_chp_vars (N, c->c);
    _block_id = -1;
    update_chp_probes (N, c->c);
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
  N->cH = phash_new (32);
  N->cdH = phash_new (4);
  N->isempty = 1;

  N->nH = NULL;

  
  /*-- process all local variables that are used --*/
  if (lang) {
    process_prs_lang (N, lang->getprs());
    process_hse_lang (N, lang->gethse());
    process_chp_lang (N, lang->getchp());
    process_dflow_lang (N, lang->getdflow());
  }
  return N;
}


static int _check_all_subconns (act_connection *c)
{
  if (c->hasSubconnections()) {
    for (int i=0; i < c->numSubconnections(); i++) {
      if (c->hasSubconnections (i)) {
	if (c->a[i] != c->a[i]->primary()) {
	  return 1;
	}
	return _check_all_subconns (c->a[i]);
      }
    }
  }
  return 0;
}

act_boolean_netlist_t *ActBooleanizePass::_create_local_bools (Process *p)
{
  act_boolean_netlist_t *n;
  int subinst = 0;
  int fail;
  Scope *sc;

  sc = p ? p->CurScope() : ActNamespace::Global()->CurScope();

  /*--- 
    initialize netlist with used flags for all local languages that
    specify circuits
    ---*/
  n = process_local_lang (a, p);


  /*--
    Check that dynamic bits are valid
    --*/
  phash_iter_t iter;
  phash_bucket_t *b;
  phash_iter_init (n->cdH, &iter);
  fail = 0;
  while ((b = phash_iter_next (n->cdH, &iter))) {
    act_dynamic_var_t *v;
    act_connection *c;
    ValueIdx *vx;
    v = (act_dynamic_var_t *) b->v;
    int newfail = 0;

    if (v->id->isglobal()) {
      newfail = 1;
      act_error_ctxt (stderr);
      warning ("Global dynamic arrays are not supported.");
    }
    if (!newfail && p) {
      ActId *tmp;
      tmp = v->id->toid();
      if (p->FindPort (tmp->getName()) != 0) {
	newfail = 1;
	act_error_ctxt (stderr);
	warning ("Dynamic arrays cannot be ports.");
      }
      delete tmp;
    }
    if (!newfail) {
      newfail = _check_all_subconns (v->id);
      if (newfail) {
	act_error_ctxt (stderr);
	warning ("Local dynamic arrays cannot have other partial connections");
      }
    }
    if (newfail) {
      fprintf (stderr, " ID: ");
      v->aid->Print (stderr);
      fprintf (stderr, "\n");
      fail = 1;
    }
  }
  if (fail) {
    fatal_error ("Cannot proceed. Dynamic arrays used in unsupported ways.");
  }

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
  phash_iter_init (n->cH, &iter);
  while ((b = phash_iter_next (n->cH, &iter))) {
    act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;
    if (!v->output) {
      /* if a channel isn't properly defined, no flags might be set
	 for a chp variable 
      */
      Assert (v->usedchp == 1 || v->input == 1, "What?");
    }
    if (v->id->isglobal()) {
      int i;
      for (i=0; i < A_LEN (n->used_globals); i++) {
	if (n->used_globals[i] == v->id) break;
      }
      if (i == A_LEN (n->used_globals)) {
	A_NEWM (n->used_globals, act_connection *);
	A_NEXT (n->used_globals) = v->id;
	A_INC (n->used_globals);
      }
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
  phash_bucket_t *b;
  b = phash_lookup (n->cH, c);
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

      Assert (!TypeFactory::isStructure (t), "What?");
	
      if (TypeFactory::isIntType (t) ||
	  (TypeFactory::isDataType (t) && (TypeFactory::boolType (t) == 0))) {
	v->isint = 1;
	v->width = TypeFactory::bitWidth (t);
      }
      else if (TypeFactory::isChanType (t)) {
	v->ischan = 1;
	v->width = TypeFactory::bitWidth (t);
	v->w2 = TypeFactory::bitWidthTwo (t);
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
	      mark_c_used (n, subinst, c, count2, 1);
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
      act_booleanized_var_t *vs =
	raw_lookup (subinst, subinst->used_globals[i]);
      Assert (vs, "What?");
      A_NEWM (n->used_globals, act_connection *);
      A_NEXT (n->used_globals) = subinst->used_globals[i];
      A_INC (n->used_globals);

      act_booleanized_var_t *v = raw_lookup (n, subinst->used_globals[i]);
      if (!v) {
	v = _var_lookup (n, subinst->used_globals[i]);
	*v = *vs;
      }
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
  phash_bucket_t *b;
  phash_iter_t iter;
  
  if (!n) return;

  Assert (n->cH, "Hmm");

  phash_iter_init (n->cH, &iter);
  while ((b = phash_iter_next (n->cH, &iter))) {
    act_booleanized_var_t *v = (act_booleanized_var_t *) b->v;
    FREE (v);
  }
  phash_iter_init (n->cdH, &iter);
  while ((b = phash_iter_next (n->cdH, &iter))) {
    act_dynamic_var_t *v = (act_dynamic_var_t *) b->v;
    delete v->aid;
    FREE (v);
  }
  phash_free (n->cH);
  phash_free (n->cdH);
  A_FREE (n->ports);
  A_FREE (n->instports);
  A_FREE (n->nets);
  FREE (n);

  if (n->nH) {
    phash_free (n->nH);
  }
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
  _create_nets_run = 0;
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

  if (n->nH) return;
  n->nH = phash_new (8);

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
  if (!_create_nets_run) {
    run_recursive (p, 1);
  }
  _create_nets_run = 1;
}

int ActBooleanizePass::addNet (act_boolean_netlist_t *n, act_connection *c)
{
  int i;
  phash_bucket_t *b;

  b = phash_lookup (n->nH, c);
  if (b) {
    return b->i;
  }
  b = phash_add (n->nH, c);

  A_NEW (n->nets, act_local_net_t);
  A_NEXT (n->nets).net = c;
  A_NEXT (n->nets).skip = 0;
  A_NEXT (n->nets).port = 0;
  A_INIT (A_NEXT (n->nets).pins);
  A_INC (n->nets);

  b->i = A_LEN (n->nets)-1;

  return b->i;
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


act_dynamic_var_t *ActBooleanizePass::isDynamicRef (act_boolean_netlist_t *n,
						    act_connection *c)
{
  phash_bucket_t *b;

  if (!c) {
    return NULL;
  }
  
  c = c->primary();
  
  do {
    while (c->parent) {
      c = c->parent;
    }
    c = c->primary();
  } while (c->parent);
  
  if ((b = phash_lookup (n->cdH, c))) {
    return (act_dynamic_var_t *) b->v;
  }
  return NULL;
}


act_dynamic_var_t *ActBooleanizePass::isDynamicRef (act_boolean_netlist_t *n,
						    ActId *id)
{
  act_connection *cx;
  if (id->isDynamicDeref ()) {
    ValueIdx *vx = id->rootVx (n->cur);
    Assert (vx->connection(), "What?");
    cx = vx->connection();
  }
  else {
    cx = id->Canonical (n->cur);
  }
  return ActBooleanizePass::isDynamicRef (n, cx);
}
