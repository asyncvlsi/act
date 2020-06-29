/*************************************************************************
 *
 *  This file is part of the ACT library
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
#include <stdio.h>
#include <string.h>
#include <map>
#include <utility>
#include <config.h>
#include <act/passes/statepass.h>

static act_connection *_inv_hash (struct iHashtable *H, int idx)
{
  for (int i=0; i < H->size; i++) {
    for (ihash_bucket_t *ib = H->head[i]; ib; ib = ib->next) {
      if (ib->i == idx) {
	return (act_connection *)ib->key;
      }
    }
  }
  return NULL;
}

void *ActStatePass::local_op (Process *p, int mode)
{
  if (mode == 0) {
    return countBools (p);
  }
  else if (mode == 1) {
    printLocal (_fp, p);
  }
  return getMap (p);
}

/*
 * Count all the local booleans, and create a map from connection
 * pointers corresponding to them to integers.
 *
 * FIXME: deal with globals correctly.
 *
 *
 */
stateinfo_t *ActStatePass::countBools (Process *p)
{
  act_boolean_netlist_t *b;

  b = bp->getBNL (p);
  if (!b) {
    fatal_error ("Process `%s' does not have a booleanized view?",
		 p->getName());
  }

  if (_black_box_mode && p && p->isBlackBox()) {
    /* Black box module */
    return NULL;
  }

  Assert (b->cH, "Hmm");

  /* 
     1. b->cH->n == local variables used
     2. port list: variables that are out of consideration

     Once we have the set of variables, then we need state only for
     output variables.
  */

  int nvars = b->cH->n;

  for (int i=0; i < A_LEN (b->ports); i++) {
    if (b->ports[i].omit) continue;
    nvars--;
  }
  /* globals are not handled by this mechanism */
  nvars -= A_LEN (b->used_globals);

  stateinfo_t *si;
  NEW (si, stateinfo_t);

  /* init for hse/prs mode */
  si->nbools = nvars;
  si->localbools = nvars;
  si->allbools = 0;
  si->map = NULL;
  si->imap = NULL;
  si->nportbools = b->cH->n - nvars - A_LEN (b->used_globals);
  si->ismulti = 0;

  /* init for chp mode */
  A_INIT (si->vars);
  A_INIT (si->lvars);
  A_INIT (si->pvars);
  A_INIT (si->allvars);
  si->chpmap = NULL;
  si->chpimap = NULL;

#if 0
  printf ("%s: start \n", p->getName());
#endif  

  bitset_t *tmpbits;
  bitset_t *inpbits;
  
  if (si->localbools + si->nportbools > 0) {
    si->multi = bitset_new (si->localbools + si->nportbools);
    tmpbits = bitset_new (si->localbools + si->nportbools);
    inpbits = bitset_new (si->localbools + si->nportbools);
  }
  else {
    si->multi = NULL;
    tmpbits = NULL;
    inpbits = NULL;
  }

  int idx = 0;

  si->map = ihash_new (8);

  /* map each connection pointer that corresponds to local state to an
     integer starting at zero
  */

  for (int i=0; i < b->cH->size; i++) {
    for (ihash_bucket_t *ib = b->cH->head[i]; ib; ib = ib->next) {
      int found = 0;
      act_booleanized_var_t *v = (act_booleanized_var_t *) ib->v;
      int ocount = 0;
      for (int k=0; k < A_LEN (b->ports); k++) {
	if (b->ports[k].omit) continue;
	if (ib->key == (long)b->ports[k].c) {
	  found = 1;
	  break;
	}
	ocount++;
      }
      if (!found && !v->id->isglobal()) {
	ihash_bucket_t *x = ihash_add (si->map, ib->key);
	x->i = idx++;
	ocount = x->i + si->nportbools;
      }

#if 0
      ActId *id = ((act_connection *)ib->key)->toid();
      printf ("   var: ");
      id->Print (stdout);
      printf (" [out=%d]", v->output ? 1 : 0);
      printf ("\n");
#endif      
      if (!v->id->isglobal()) {
	if (v->output) {
	  if (bitset_tst (tmpbits, ocount)) {
	    /* found multi driver! */
	    bitset_set (si->multi, ocount);
#if 0
	    printf ("     -> multi-driver!\n");
#endif
	  }
	  else {
	    bitset_set (tmpbits, ocount);
	  }
	}
	else {
	  /* an input; mark it */
	  bitset_set (inpbits, ocount);
	}
      }
#if 0
      delete id;
#endif      
    }
  }

  Assert (idx == si->localbools, "What?");

#if 0
  printf ("%s: stats: %d local; %d port\n", p->getName(),
	  si->localbools, si->nportbools);
#endif


  /* sum up instance state, and compute offsets for each instance */
  si->allbools = si->localbools;
  si->imap = ihash_new (8);

  ActInstiter i(p ? p->CurScope() : ActNamespace::Global()->CurScope());

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      Process *x = dynamic_cast<Process *>(vx->t->BaseType());
      if (x->isExpanded()) {
	stateinfo_t *ti = (stateinfo_t *) getMap (x);
	int n_sub_bools;

	if (ti) {
	  n_sub_bools = ti->allbools;
	}
	else {
	  /* black box */
	  act_boolean_netlist_t *bn = bp->getBNL (x);
	  Assert (bn, "What?");
	  n_sub_bools = 0;
	}
	/* map valueidx pointer to the current bool offset */
	ihash_bucket_t *ib = ihash_add (si->imap, (long)vx);
	ib->i = si->allbools;
	if (vx->t->arrayInfo()) {
	  si->allbools += vx->t->arrayInfo()->size()*n_sub_bools;
	}
	else {
	  si->allbools += n_sub_bools;
	}
      }
    }
  }

  /* now check for multi-drivers due to instances */
  int instcnt = 0;
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      Process *x = dynamic_cast<Process *>(vx->t->BaseType());
      if (x->isExpanded()) {
	int ports_exist;
	act_boolean_netlist_t *sub;
	stateinfo_t *subsi;

	sub = bp->getBNL (x);
	subsi = getStateInfo (x);
	ports_exist = 0;
	for (int j=0; j < A_LEN (sub->ports); j++) {
	  if (sub->ports[j].omit == 0) {
	    ports_exist = 1;
	    break;
	  }
	}

	if (ports_exist) {
	  int sz;
	  if (vx->t->arrayInfo()) {
	    sz = vx->t->arrayInfo()->size();
	  }
	  else {
	    sz = 1;
	  }
	  
	  while (sz > 0) {
	    sz--;
	    for (int j=0; j < A_LEN (sub->ports); j++) {
	      act_connection *c;
	      ihash_bucket_t *bi;
	      int ocount;
	      if (sub->ports[j].omit) continue;

	      c = b->instports[instcnt];
	      bi = ihash_lookup (si->map, (long)c);
	      if (bi) {
		ocount = bi->i + si->nportbools;
	      }
	      else {
		ocount = 0;
		for (int k=0; k < A_LEN (b->ports); k++) {
		  if (b->ports[k].omit) continue;
		  if (c == b->ports[k].c) {
		    break;
		  }
		  ocount++;
		}
		Assert (ocount < si->nportbools, "What?");
	      }
	      
	      if (!sub->ports[j].input) {
		if (bitset_tst (tmpbits, ocount)) {
		  /* found multi driver! */
		  bitset_set (si->multi, ocount);
		  if (subsi) {
		    /* could be NULL, if it is a black box */
		    subsi->ismulti = 1;
		  }
#if 0
		  printf (" *multi-driver: ");
		  ActId *id = c->toid();
		  id->Print (stdout);
		  delete id;
		  printf ("\n");
#endif		  
		}
		else {
		  bitset_set (tmpbits, ocount);
		}
	      }
	      else {
		bitset_set (inpbits, ocount);
	      }
	      instcnt++;
	    }
	  }
	}
      }
    }
  }

  if (Act::warn_no_local_driver) {
    /* now check if there is some local state that is actually never
       driven! */
    for (int i=0; i < si->localbools; i++) {
      if (bitset_tst (inpbits, i + si->nportbools) &&
	  !bitset_tst (tmpbits, i + si->nportbools)) {
	act_connection *tmpc = _inv_hash (si->map, i);
	Assert (tmpc, "How did we get here?");
	ActId *tmpid = tmpc->toid();
	fprintf (stderr, "WARNING: Process `%s': local variable `",
		 p ? p->getName() : "-toplevel-");
	tmpid->Print (stderr);
	fprintf (stderr, "': no driver\n");
	delete tmpid;
      }
    }
  }
  
  if (tmpbits) {
    bitset_free (tmpbits);
    bitset_free (inpbits);
  }

#if 0
  printf ("%s: done\n\n", p->getName());
#endif  
  
  return si;
}

ActStatePass::ActStatePass (Act *a) : ActPass (a, "collect_state")
{
  /*-- need the booleanize pass --*/
  ActPass *ap = a->pass_find ("booleanize");
  if (!ap) {
    bp = new ActBooleanizePass (a);
  }
  else {
    bp = dynamic_cast<ActBooleanizePass *>(ap);
    Assert (bp, "What?");
  }
  AddDependency ("booleanize");
  _black_box_mode = config_get_int ("net.black_box_mode");
}

void ActStatePass::free_local (void *v)
{
  stateinfo_t *s = (stateinfo_t *)v;

  if (s->map) {
    ihash_free (s->map);
    s->map = NULL;
  }
  if (s->imap) {
    ihash_free (s->imap);
    s->imap = NULL;
  }
  FREE (s);
}

int ActStatePass::run (Process *p)
{
  return ActPass::run (p);
}

void ActStatePass::Print (FILE *fp, Process *p)
{
  if (!completed()) {
    warning ("ActStatePass::Print() called without pass being run");
    return;
  }
  _fp = fp;
  run_recursive (p, 1);
}

stateinfo_t *ActStatePass::getStateInfo (Process *p)
{
  if (!completed()) {
    return NULL;
  }
  void *v = getMap (p);
  return (stateinfo_t *) v;
}

void ActStatePass::printLocal (FILE *fp, Process *p)
{
  stateinfo_t *si;

  si = getStateInfo (p);

  fprintf (fp, "--- Process: %s ---\n", p ? p->getName() : "-toplevel-");

  if (!si) {
    act_boolean_netlist_t *bn = bp->getBNL (p);
    Assert (bn, "Hmm");
    fprintf (fp, "  ** black box **\n");
    fprintf (fp, "  portbools: %d\n", A_LEN (bn->ports));
  }
  else {
    fprintf (fp, "   nbools = %d, nvars = %d\n",
	     si->nbools, A_LEN (si->vars));
    fprintf (fp, "  localbools: %d\n", si->localbools);
    fprintf (fp, "  portbools: %d\n", si->nportbools);
    fprintf (fp, "  ismulti: %d\n", si->ismulti);
    fprintf (fp, "  all booleans (incl. inst): %d\n", si->allbools);
  }
  fprintf (fp, "--- End Process: %s ---\n", p->getName());
}
