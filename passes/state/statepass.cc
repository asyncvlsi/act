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


/*
 * Count all the local booleans, and create a map from connection
 * pointers corresponding to them to integers.
 *
 */
void *ActStatePass::local_op (Process *p)
{
  act_boolean_netlist_t *b;

  b = bp->getBNL (p);
  if (!b) {
    fatal_error ("Process `%s' does not have a booleanized view?",
		 p->getName());
  }

  Assert (b->cH, "Hmm");
  Assert (b->uH, "Hmm");

  /* 
     1. b->cH->n == local variables used
     2. b->uH->n == boolean passed to subcircuits but not a local variable
     3. port list: variables that are out of consideration

     Once we have the set of variables, then we need state only for
     output variables.
  */

  int nvars = b->cH->n + b->uH->n;

  for (int i=0; i < A_LEN (b->ports); i++) {
    if (b->ports[i].omit) continue;
    nvars--;
  }

  stateinfo_t *si;
  NEW (si, stateinfo_t);
  si->nbools = nvars;
  si->ibitwidth = 0;
  si->ichanwidth = 0;
  si->localbools = nvars;
  si->allbools = 0;
  si->map = NULL;
  si->imap = NULL;

  int idx = 0;

  si->map = ihash_new (8);

  /* map each connection pointer that corresponds to local state to an
     integer starting at zero
  */

  for (int i=0; i < b->cH->size; i++) {
    for (ihash_bucket_t *ib = b->cH->head[i]; ib; ib = ib->next) {
      int found = 0;
      for (int k=0; k < A_LEN (b->ports); k++) {
	if (ib->key == (long)b->ports[k].c) {
	  found = 1;
	  break;
	}
      }
      if (!found) {
	ihash_bucket_t *x = ihash_add (si->map, ib->key);
	x->i = idx++;
      }
    }
  }

  for (int i=0; i < b->uH->size; i++) {
    for (ihash_bucket_t *ib = b->uH->head[i]; ib; ib = ib->next) {
      int found = 0;
      for (int k=0; k < A_LEN (b->ports); k++) {
	if (ib->key == (long)b->ports[k].c) {
	  found = 1;
	  break;
	}
      }
      if (!found) {
	ihash_bucket_t *x = ihash_add (si->map, ib->key);
	x->i = idx++;
      }
    }
  }


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
	Assert (ti, "Hmm");

	/* map valueidx pointer to the current bool offset */
	ihash_bucket_t *ib = ihash_add (si->imap, (long)vx);
	ib->i = si->allbools;
	
	si->allbools += ti->allbools;
      }
    }
  }
  
  return si;
}

void ActStatePass::Print (FILE *fp)
{
  if (!completed()) {
    warning ("ActStatePass::Print() called without pass being run");
    return;
  }
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
