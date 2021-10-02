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
#include <common/config.h>
#include <act/passes/statepass.h>

static act_connection *_inv_hash (struct pHashtable *H, int idx)
{
  phash_iter_t iter;
  phash_bucket_t *ib;
  
  phash_iter_init (H, &iter);
  while ((ib = phash_iter_next (H, &iter))) {
    if (ib->i == idx) {
      return (act_connection *)ib->key;
    }
  }
  return NULL;
}

void *ActStatePass::local_op (Process *p, int mode)
{
  if (mode == 0) {
    return countLocalState (p);
  }
  else if (mode == 1) {
    printLocal (_fp, p);
  }
  return getMap (p);
}


void ActStatePass::getStructCount (Data *d, state_counts *sc)
{
  int nb, ni;
  d->getStructCount (&nb, &ni);
  sc->addInt (ni);
  sc->addBool (nb);
  return;
}

/*
 * Count all the local booleans, and create a map from connection
 * pointers corresponding to them to integers.
 *
 * FIXME: deal with globals correctly.
 *
 *
 */
stateinfo_t *ActStatePass::countLocalState (Process *p)
{
  act_boolean_netlist_t *b;

  b = bp->getBNL (p);
  if (!b) {
    act_error_ctxt (stderr);
    fatal_error ("Process `%s' does not have a booleanized view?",
		 p->getName());
  }

  if (_black_box_mode && p && p->isBlackBox()) {
    /* Black box module */
    return NULL;
  }

  Assert (b->cH, "Hmm");
  Assert (b->cdH, "Hmm...");

  /* 
     The booleanized vars come in two flavors:
      - the raw booleans
      - the chp booleans

     1. b->cH->n == local variables used + global bools used in this process
     2. port list: variables that are out of consideration

     Once we have the set of variables, then we need state only for
     output variables.
  */

  state_counts sc;

  int bool_count = 0;
  int chp_count = 0;

  /* count non-global variables that are used */
  state_counts alt_port;
  
  phash_bucket_t *hb;
  phash_iter_t iter;

  /*-- dynamic bools are assumed to be used in both chp and non-chp --*/
  /* dynamic bools cannot be in ports, so these must be local */
  phash_iter_init (b->cdH, &iter);
  while ((hb = phash_iter_next (b->cdH, &iter))) {
    act_dynamic_var_t *v;
    v = (act_dynamic_var_t *)hb->v;
    if (v->isstruct) {
      state_counts ns;
      getStructCount (v->isstruct, &ns);
      Assert (ns.numChans() == 0, "Dynamic structures with channels!");
      chp_count += ns.numInts()*v->a->size();
      bool_count += ns.numBools()*v->a->size();
    }
    else if (v->isint) {
      chp_count += v->a->size();
    }
    else {
      bool_count += v->a->size();
    }
  }
  
  phash_iter_init (b->cH, &iter);
  while ((hb = phash_iter_next (b->cH, &iter))) {
    act_booleanized_var_t *v = (act_booleanized_var_t *)hb->v;

    if (ActBooleanizePass::isDynamicRef (b, v->id)) {
      /*-- already counted --*/
      continue;
    }
    
    if (v->used && !v->isglobal) {
      bool_count++;
      if (v->isport) {
	alt_port.addBool();
      }
    }
    
    if (!v->used && v->usedchp && !v->isglobal) {
      /* variables that are used in chp that are not used in the
	 booleanized version */
      chp_count++;
      if (v->ischpport) {
	if (v->isint) {
	  alt_port.addInt();
	}
	else if (v->ischan) {
	  alt_port.addChan();
	}
	else {
	  /* extra chp bools */
	  alt_port.addCHPBool();
	}
      }
    }
  }

  stateinfo_t *si;
  NEW (si, stateinfo_t);

  state_counts zero;

  si->bnl = b;

  si->all = zero;
  si->local = zero;
  si->ports = alt_port;
  
  si->map = NULL;
  si->ismulti = 0;

  /* init for hse/prs mode: only track bools 

     orig value is zero, so this sets the # of bools
   */
  si->local.addBool (bool_count - si->ports.numBools());

  /* init for chp mode: bools, ints, chans; note that fractured ints
     are tracked as fractured ints. */

  si->chp_ismulti = 0;
  si->inst = NULL;

  int nportchptot = si->ports.numCHPVars();
  int localchp = chp_count - nportchptot;

#if 0
  printf ("%s: start \n", p->getName());
#endif

  bitset_t *tmpbits;
  bitset_t *inpbits;

  if (si->local.numBools() + si->ports.numBools() > 0) {
    si->multi = bitset_new (si->local.numBools() + si->ports.numBools());
    tmpbits = bitset_copy (si->multi);
    inpbits = bitset_copy (si->multi);
  }
  else {
    si->multi = NULL;
    tmpbits = NULL;
    inpbits = NULL;
  }

  bitset_t *tmpchp;
  bitset_t *inpchp;
  bitset_t *chpmulti;

  if (localchp + nportchptot > 0) {
    chpmulti = bitset_new (localchp + nportchptot);
    tmpchp = bitset_copy (chpmulti);
    inpchp = bitset_copy (chpmulti);
  }
  else {
    chpmulti = NULL;
    tmpchp = NULL;
    inpchp = NULL;
  }

  int idx = 0;
  int chpidx = 0;

  si->map = phash_new (8);

  /* map each connection pointer that corresponds to local state to an
     integer starting at zero
  */

  /*
    Start with dynamic arrays. These are always local.
  */
  phash_bucket_t *pb;

  phash_iter_init (b->cdH, &iter);
  while ((pb = ihash_iter_next (b->cdH, &iter))) {
    /*---
      XXX: what if a process has a dynamic array in CHP and then a
      static array in the prs?
      UNHANDLED SITUATION.
      ---*/

    act_dynamic_var_t *v = (act_dynamic_var_t *) pb->v;
    phash_bucket_t *x;
    if (v->isint) {
      x = phash_add (si->map, pb->key);
      x->i = chpidx;
      Assert (v->a, "Huh?");
      chpidx += v->a->size();
      si->all.addInt (v->a->size());

      /* record info for multi-driver */
      for (int i=0; i < v->a->size(); i++) {
	if (bitset_tst (tmpchp, chpidx - 1 - i)) {
	  bitset_set (chpmulti, chpidx - 1 - i);
	}
	else {
	  bitset_set (tmpchp, chpidx - 1 - i);
	}
      }
    }
    else {
      x = phash_add (si->map, pb->key);
      x->i = idx;
      idx += v->a->size();
      for (int i=0; i < v->a->size(); i++) {
	if (bitset_tst (tmpbits, idx - 1 - i)) {
	  bitset_set (si->multi, idx - 1 - i);
	}
	else {
	  bitset_set (tmpbits, idx - 1 - i);
	}
	/* XXX: check if this index is actually in the cH hash; if so, we
	   have a problem */
      }
    }
  }

  phash_iter_init (b->cH, &iter);
  while ((pb = phash_iter_next (b->cH, &iter))) {
    int found = 0;
    act_booleanized_var_t *v = (act_booleanized_var_t *) pb->v;
    act_dynamic_var_t *dv;
    int ocount = 0;

    if ((dv = ActBooleanizePass::isDynamicRef (b, v->id))) {
      /*-- already handled and allocated by dynamic variable analysis! --*/
      if (v->id->parent == dv->id) {
	/* direct array ref! */
	ocount = dv->id->suboffset (v->id);
      }
      else {
	act_error_ctxt (stderr);
	fatal_error ("Accessing pieces of a dynamic reference...");
      }

      if (dv->isint) {
	phash_bucket_t *x = phash_add (si->map, pb->key);
	phash_bucket_t *y = phash_lookup (si->map, dv->id);
	Assert (y, "what?!");
	x->i = y->i + ocount; /* offset */
      }
      else {
	phash_bucket_t *x = phash_add (si->map, pb->key);
	phash_bucket_t *y = phash_lookup (si->map, dv->id);
	Assert (y, "what?!");
	x->i = y->i + ocount; /* offset */
      }
      continue;
    }

    if (v->used) {
      /* boolean state */
      if (v->isport) {
	/*-- in the port list; so port state, not local state --*/
	for (int k=0; k < A_LEN (b->ports); k++) {
	  if (b->ports[k].omit) continue;
	  if (pb->key == (unsigned long)b->ports[k].c) {
	    found = 1;
	    break;
	  }
	  ocount++;
	}
	Assert (found, "What?");
	phash_bucket_t *x = ihash_add (si->map, pb->key);

	/* port index is a negative value */
	x->i = ocount - si->ports.numBools();
      }
      else if (!v->isglobal) {
	/*-- globals not handled here --*/
	phash_bucket_t *x = phash_add (si->map, pb->key);
	x->i = idx++;
	ocount = x->i + si->ports.numBools();
      }

      /*
	 ocount is used to set entries in the bitvector to check
	 for missing drivers or multi drivers.
	 Bit-vector format is: [ports] [rest of the state]
      */

#if 0
      ActId *id = ((act_connection *)pb->key)->toid();
      printf ("   var: ");
      id->Print (stdout);
      printf (" [out=%d]", v->output ? 1 : 0);
      printf ("\n");
#endif

      /*-- look for multi-drivers: globals are ignored --*/
      if (!v->isglobal) {
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
      
    if (!v->used && v->usedchp) {
      /* extra chp state */
      if (v->ischpport) {
	for (int k=0; k < A_LEN (b->chpports); k++) {
	  if (b->chpports[k].omit) continue;
	  if (pb->key == (long)b->chpports[k].c) {
	    found = 1;
	    break;
	  }
	  {
	    phash_bucket_t *xb = phash_lookup (b->cH, b->chpports[k].c);
	    act_booleanized_var_t *xv = (act_booleanized_var_t *)xb->v;
	    if (!xv->used) {
	      /* if it is used in the boolean pass, it's already
		 counted there */
	      ocount++;
	    }
	  }
	}
	Assert (found, "What?");
      }
      else if (!v->isglobal) {
	phash_bucket_t *x = phash_add (si->map, pb->key);
	x->i = chpidx++;
	ocount = x->i + nportchptot;

	if (v->ischan) {
	  si->all.addChan();
	}
	else if (v->isint) {
	  si->all.addInt();
	}
	else {
	  si->all.addCHPBool();
	}
      }

#if 0
      ActId *id = ((act_connection *)pb->key)->toid();
      printf ("   var: ");
      id->Print (stdout);
      printf (" [out=%d]", v->output ? 1 : 0);
      printf ("\n");
#endif      
      if (!v->isglobal) {
	if (v->output) {
	  if (bitset_tst (tmpchp, ocount)) {
	    /* found multi driver! */
	    bitset_set (chpmulti, ocount);
#if 0
	    printf ("     -> multi-driver!\n");
#endif
	  }
	  else {
	    bitset_set (tmpchp, ocount);
	  }
	}
	else {
	  /* an input; mark it */
	  bitset_set (inpchp, ocount);
	}
      }
#if 0
      delete id;
#endif      
    }
  }

  Assert (idx == si->local.numBools(), "What?");
  Assert (chpidx == localchp, "What?");
  Assert (localchp == si->all.numCHPVars(), "What?");

  si->local.addCHPBool (si->all.numCHPBools());
  si->local.addInt (si->all.numInts());
  si->local.addChan (si->all.numChans());
  si->all.addBool (si->local.numBools());

#if 0
  printf ("%s: stats: %d local; %d port\n", p->getName(),
	  si->localbools, si->nportbools);
#endif

  ActInstiter i(p ? p->CurScope() : ActNamespace::Global()->CurScope());

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      Process *x = dynamic_cast<Process *>(vx->t->BaseType());
      if (x->isExpanded()) {
	stateinfo_t *ti = (stateinfo_t *) getMap (x);
	state_counts n_sub;

	if (ti) {
	  n_sub = ti->all;

	  if (_inst_offsets) {
	    if (!si->inst) {
	      si->inst = phash_new (4);
	    }
	    phash_bucket_t *b;
	    b = phash_add (si->inst, vx);
	    state_counts *sc = new state_counts;
	    *sc = si->all;
	    b->v = sc;
	  }
	}
	else {
	  /* black box */
	  act_boolean_netlist_t *bn = bp->getBNL (x);
	  Assert (bn, "What?");
	  n_sub = zero;
	}
	int sz;
	/* map valueidx pointer to the current bool offset */
	if (vx->t->arrayInfo()) {
	  sz = vx->t->arrayInfo()->size();
	}
	else {
	  sz = 1;
	}
	si->all.addVar (n_sub, sz);
      }
    }
  }

#if 0
  printf ("%s: fullstats\n", p->getName());
  printf ("  %d allbools\n", si->allbools);
  printf ("  %d chp_allbool\n", si->chp_all.bools);
  printf ("  %d chp_allchan\n", si->chp_all.chans);
  printf ("  %d chp_allint\n", si->chp_all.ints);
#endif
  

  /* now check for multi-drivers due to instances */
  int instcnt = 0;
  int chpinstcnt = 0;
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
	      phash_bucket_t *bi;
	      int ocount;
	      if (sub->ports[j].omit) continue;

	      c = b->instports[instcnt];
	      bi = phash_lookup (si->map, c);

	      if (c->isglobal()) {
		/* ignore globals here */
		continue;
	      }
	      
	      if (bi) {
		ocount = bi->i + si->ports.numBools();
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
		Assert (ocount < si->ports.numBools(), "What?");
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

	ports_exist = 0;
	for (int j=0; j < A_LEN (sub->chpports); j++) {
	  if (sub->chpports[j].omit == 0) {
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
	    for (int j=0; j < A_LEN (sub->chpports); j++) {
	      act_connection *c;
	      phash_bucket_t *bi;
	      int ocount;
	      if (sub->chpports[j].omit) continue;

	      c = b->instchpports[chpinstcnt];

	      /* -- ignore globals -- */
	      if (c->isglobal()) continue;

	      phash_bucket_t *xb = phash_lookup (b->cH, c);
	      if (xb) {
		act_booleanized_var_t *xv = (act_booleanized_var_t *) xb->v;
		if (xv->used) {
		/* handled in earlier pass */
		  continue;
		}
	      }
		
	      bi = phash_lookup (si->map, c);
	      if (bi) {
		ocount = bi->i + nportchptot;
	      }
	      else {
		ocount = 0;
		for (int k=0; k < A_LEN (b->chpports); k++) {
		  if (b->chpports[k].omit) continue;
		  if (c == b->chpports[k].c) {
		    break;
		  }
		  ocount++;
		}
		Assert (ocount < nportchptot, "What?");
	      }
	      
	      if (!sub->chpports[j].input) {
		if (bitset_tst (tmpchp, ocount)) {
		  /* found multi driver! */
		  bitset_set (chpmulti, ocount);
		  if (subsi) {
		    /* could be NULL, if it is a black box */
		    subsi->chp_ismulti = 1;
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
		  bitset_set (tmpchp, ocount);
		}
	      }
	      else {
		bitset_set (inpchp, ocount);
	      }
	      chpinstcnt++;
	    }
	  }
	}
      }
    }
  }

  if (Act::no_local_driver) {
    int err_ctxt = 0;
    /* now check if there is some local state that is actually never
       driven! */
    for (int i=0; i < si->local.numBools(); i++) {
      if (bitset_tst (inpbits, i + si->ports.numBools()) &&
	  !bitset_tst (tmpbits, i + si->ports.numBools())) {
	act_connection *tmpc = _inv_hash (si->map, i);
	Assert (tmpc, "How did we get here?");
	ActId *tmpid = tmpc->toid();
	if (!err_ctxt) {
	  act_error_ctxt (stderr);
	  err_ctxt = 1;
	}
	fprintf (stderr, "WARNING: Process `%s': local variable `",
		 p ? p->getName() : "-toplevel-");
	tmpid->Print (stderr);
	fprintf (stderr, "': no driver\n");
	delete tmpid;
      }
    }

    for (int i=0; i < localchp; i++) {
      if (bitset_tst (inpchp, i + nportchptot) &&
	  !bitset_tst (tmpchp, i + nportchptot)) {
	act_connection *tmpc = _inv_hash (si->map, i);
	Assert (tmpc, "How did we get here?");
	ActId *tmpid = tmpc->toid();
	if (!err_ctxt) {
	  act_error_ctxt (stderr);
	  err_ctxt = 1;
	}
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

  if (tmpchp) {
    bitset_free (tmpchp);
    bitset_free (inpchp);
    bitset_free (chpmulti);
  }

  /* re-do CHP numbering in the map! */

  state_counts c_idx;

  phash_iter_init (b->cdH, &iter);
  while ((pb = ihash_iter_next (b->cdH, &iter))) {
    act_dynamic_var_t *v = (act_dynamic_var_t *) pb->v;
    phash_bucket_t *x;
    if (v->isstruct) {
      state_counts ts;
      x = phash_lookup (si->map, pb->key);
      Assert (x, "What?");
      x->i = c_idx.numBools();
      x = phash_lookup (si->map, (act_connection *)(((unsigned long)pb->key)|1));
      Assert (!x, "Hmm");
      x = phash_add (si->map, (act_connection *)(((unsigned long)pb->key)|1));
      x->i = c_idx.numInts();
      getStructCount (v->isstruct, &ts);
      c_idx.addVar (ts, v->a->size());
    }
    else if (v->isint) {
      x = phash_lookup (si->map, pb->key);
      Assert (x, "What?");
      x->i = c_idx.numInts();
      c_idx.addInt (v->a->size());
    }
    else {
      c_idx.addBool (v->a->size());
      /* bool, no problem */
    }
  }

  phash_iter_init (b->cH, &iter);
  while ((pb = phash_iter_next (b->cH, &iter))) {
    int found = 0;
    act_booleanized_var_t *v = (act_booleanized_var_t *) pb->v;
    int ocount = 0;

    if (v->used) {
      /* in the normal bool count, so proper numbering: we're done */
      continue;
    }

    if (v->usedchp) {
      /* extra chp state */
      if (v->ischpport) {
	for (int k=0; k < A_LEN (b->chpports); k++) {
	  if (b->chpports[k].omit) continue;
	  if (pb->key == (unsigned long)b->chpports[k].c) {
	    found = 1;
	    break;
	  }
	  {
	    phash_bucket_t *xb = phash_lookup (b->cH, b->chpports[k].c);
	    act_booleanized_var_t *xv = (act_booleanized_var_t *)xb->v;
	    if (!xv->used) {
	      /* if it is used in the boolean pass, it's already
		 counted there */
	      if (v->ischan) {
		if (xv->ischan) {
		  ocount++;
		}
	      }
	      else if (v->isint) {
		if (xv->isint) {
		  ocount++;
		}
	      }
	      else if (!(xv->isint || xv->ischan)) {
		ocount++;
	      }
	    }
	  }
	}
	Assert (found, "What?");
	phash_bucket_t *x = phash_add (si->map, pb->key);
	Assert (x, "What?");
	if (v->ischan) {
	  x->i = ocount - si->ports.numChans();
	}
	else if (v->isint) {
	  x->i = ocount - si->ports.numInts();
	}
	else {
	  x->i = ocount - si->ports.numAllBools();
	}
      }
      else if (!v->isglobal) {
	phash_bucket_t *x = phash_lookup (si->map, pb->key);
	if (v->ischan) {
	  x->i = c_idx.numChans();
	  c_idx.addChan();
	}
	else if (v->isint) {
	  x->i = c_idx.numInts();
	  c_idx.addInt();
	}
	else {
	  x->i = si->all.numBools() + c_idx.numCHPBools();
	  c_idx.addCHPBool();
	}
      }
    }
  }
  
#if 0
  printf ("  bool-only:\n");
  printf ("   port: %d; local: %d; all: %d\n", si->nportbools,
	  si->localbools, si->allbools);
  printf ("  chp-extra:\n");
  printf ("   bool:: port: %d; all: %d\n",
	  si->nportchp.bools, si->chp_all.bools);
  printf ("   int::  port: %d; all: %d\n",
	  si->nportchp.ints, si->chp_all.ints);
  printf ("   chan:: port: %d; all: %d\n",
	  si->nportchp.chans, si->chp_all.chans);
  printf ("%s: done\n\n", p->getName());
#endif  

  return si;
}

ActStatePass::ActStatePass (Act *a, int inst_offset) : ActPass (a, "collect_state", 1)
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
  _inst_offsets = inst_offset;
}

void ActStatePass::free_local (void *v)
{
  stateinfo_t *s = (stateinfo_t *)v;

  if (s->map) {
    ihash_free (s->map);
    s->map = NULL;
  }
  if (s->multi) {
    bitset_free (s->multi);
  }
  if (s->inst) {
    phash_free (s->inst);
  }
  
  FREE (s);
}

int ActStatePass::run (Process *p)
{
  int res = ActPass::run (p);

  /*-- set root stateinfo for global variables --*/
  _root_si = getStateInfo (p);

  if (!_root_si) {
    return res;
  }

  /*-- compute global sizes and add mapping to top-level state table --*/

  act_boolean_netlist_t *nl = _root_si->bnl;

  for (int i=0; i < A_LEN (nl->used_globals); i++) {
    act_booleanized_var_t *v;
    act_dynamic_var_t *dv;
    phash_bucket_t *b;

    b = phash_lookup (nl->cdH, nl->used_globals[i]);
    if (b) {
      dv = (act_dynamic_var_t *) b->v;
      if (dv->isint) {
	_globals.addInt (dv->a->size());
      }
      else {
	_globals.addBool (dv->a->size());
      }
    }
    else {
      b = phash_lookup (nl->cH, nl->used_globals[i]);
      Assert (b, "What?");
      v = (act_booleanized_var_t *) b->v;
      Assert (v, "What?");
      if (v->ischan) { 
	_globals.addChan();
      }
      else if (v->isint) {
	_globals.addInt();
      }
      else {
	_globals.addBool();
      }
    }
  }

  /*-- add state info maps for globals --*/
  state_counts idx;
  
  for (int i=0; i < A_LEN (nl->used_globals); i++) {
    act_booleanized_var_t *v;
    act_dynamic_var_t *dv;
    phash_bucket_t *b;

    b = phash_lookup (nl->cdH, nl->used_globals[i]);
    if (b) {
      dv = (act_dynamic_var_t *) b->v;
      b = phash_add (_root_si->map, nl->used_globals[i]);
      if (dv->isint) {
	b->i = idx.numInts() - _globals.numInts();
	idx.addInt (dv->a->size());
      }
      else {
	b->i = idx.numBools() - _globals.numBools();
	idx.addBool (dv->a->size());
      }
    }
    else {
      b = phash_lookup (nl->cH, nl->used_globals[i]);
      Assert (b, "What?");
      v = (act_booleanized_var_t *) b->v;
      Assert (v, "What?");
      b = phash_add (_root_si->map, nl->used_globals[i]);
      if (v->ischan) {
	b->i = idx.numChans() - _globals.numChans();
	idx.addChan ();
      }
      else if (v->isint) {
	b->i = idx.numInts() - _globals.numInts();
	idx.addInt ();
      }
      else {
	b->i = idx.numBools() - _globals.numBools();
	idx.addBool();
      }
    }
  }
  
  return res;
}

void ActStatePass::Print (FILE *fp, Process *p)
{
  if (!completed()) {
    warning ("ActStatePass::Print() called without pass being run");
    return;
  }
  _fp = fp;
  run_recursive (p, 1);
  printf ("Globals: %d bools\n", _globals.numBools());
}

stateinfo_t *ActStatePass::getStateInfo (Process *p)
{
  if (!completed()) {
    return NULL;
  }
  void *v;
  if (p) {
    v = getMap (p);
  }
  else {
    v = _root_si;
  }
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
	     si->local.numBools(), si->local.numCHPVars());
    fprintf (fp, "  localbools: %d\n", si->local.numBools());
    fprintf (fp, "  portbools: %d\n", si->ports.numBools());
    fprintf (fp, "  ismulti: %d\n", si->ismulti);
    fprintf (fp, "  all booleans (incl. inst): %d\n", si->all.numBools());
  }
  fprintf (fp, "--- End Process: %s ---\n", p->getName());
}


/*
 *------------------------------------------------------------------------
 *
 *  Returns offset for connection within the state context
 *
 *    Non-negative offset  : local state offset
 *    Even negative offset : global variable; index is (-off)/2 - 1
 *    Odd negative offset  : port; index is (-off+1)/2 - 1
 *
 * Returns: 1 on success, 0 on error
 * Side effects: none
 *
 *------------------------------------------------------------------------
 */
int ActStatePass::getTypeOffset (stateinfo_t *si, act_connection *c,
				 int *offset, int *type, int *width)
{
  phash_bucket_t *b;

  if (!si) {
    return 0;
  }

  Assert (si && c && offset, "What?");

  /*-- check if this is a dynamic array --*/
  b = phash_lookup (si->bnl->cdH, c);
  if (b) {
    act_dynamic_var_t *dv = (act_dynamic_var_t *)b->v;
    b = phash_lookup (si->map, c);
    Assert (b, "What?");
    if (dv->isint) {
      if (type) {
	*type = 1;
      }
      if (width) {
	*width = dv->width;
      }
    }
    else {
      if (type) {
	*type = 0;
      }
      if (width) {
	*width = 1;
      }
    }
    Assert (b, "What?");
    *offset = b->i;
    return 1;
  }

  /*-- otherwise... --*/

  b = phash_lookup (si->bnl->cH, c);
  if (!b) {
    return 0;
  }
  Assert (b, "No connection in conn hash?");
  act_booleanized_var_t *v =  (act_booleanized_var_t *)b->v;

  /* set type and width */
  if (type) {
    if (v->ischan) {
      if (v->input) {
	*type = 2;
      }
      else {
	*type = 3;
      }
      if (width) {
	*width = v->width;
      }
    }
    else if (v->isint) {
      *type = 1;
      if (width) {
	*width = v->width;
      }
    }
    else {
      *type = 0;
      if (width) {
	*width = 1;
      }
    }
  }

  int glob;
  if (c->isglobal()) {
    glob = 1;
    si = _root_si;
  }
  else {
    glob = 0;
  }

  b = phash_lookup (si->map, c);
  if (!b) {
    return 0;
  }

  if (glob) {
    /* global variables get even negative IDs */
    Assert (b->i < 0, "Should have already been negative!");
    *offset = 2*b->i;
    return 1;
  }

  if (b->i < 0) {
    /* local port index: odd negative index */
    *offset = 2*b->i + 1;
  }
  else {
    *offset = b->i;
  }
  return 1;
}


int ActStatePass::getTypeDynamicStructOffset (stateinfo_t *si,
					      act_connection *c,
					      int *offset_i, int *offset_b)
{
  phash_bucket_t *b;

  if (!si) {
    return 0;
  }

  Assert (si && c && offset_i && offset_b, "What?");

  /*-- check if this is a dynamic array --*/
  b = phash_lookup (si->bnl->cdH, c);
  if (b) {
    act_dynamic_var_t *dv = (act_dynamic_var_t *)b->v;
    if (!dv->isstruct) {
      return 0;
    }
    b = phash_lookup (si->map, c);
    if (b) {
      *offset_b = b->i;
    }
    else {
      *offset_b = -1;
    }
    b = phash_lookup (si->map, (act_connection *)(((unsigned long)c)|1));
    if (b) {
      *offset_i = b->i;
    }
    else {
      *offset_i = -1;
    }
    if (*offset_i == -1 && *offset_b == -1) {
      return 0;
    }
    return 1;
  }
  return 0;
}

act_connection *ActStatePass::getConnFromOffset (stateinfo_t *si, int off, int type, int *doff)
{
  if (!si) {
    return NULL;
  }

  *doff = -1;

  if (isGlobalOffset (off)) {
    off = globalIdx (off);
    si = rootStateInfo ();
    if (off >= A_LEN (si->bnl->used_globals)) {
      return NULL;
    }
    else {
      return si->bnl->used_globals[off];
    }
  }
  else if (isPortOffset (off)) {
    off = portIdx (off);
    /* -- first booleanized ports, then chp ports -- */
    for (int i=A_LEN (si->bnl->chpports)-1; i >= 0; i--) {
      if (si->bnl->chpports[i].omit) continue;
      if (off == 0) {
	return si->bnl->chpports[i].c;
      }
      off--;
    }
    for (int i=A_LEN (si->bnl->ports)-1; i >= 0; i--) {
      if (si->bnl->ports[i].omit) continue;
      if (off == 0) {
	return si->bnl->ports[i].c;
      }
      off--;
    }
    /* something went wrong */
    return NULL;
  }
  else {
    act_connection *c;
    phash_bucket_t *b;
    phash_iter_t bi;

    phash_iter_init (si->map, &bi);

    /* -- check variables -- */
    while ((b = phash_iter_next (si->map, &bi))) {
      if (b->i == off) {
	c = (act_connection *)b->key;
	
	/* check if c has the right type! */
	phash_bucket_t *xb;
	act_booleanized_var_t *v;
	xb = phash_lookup (si->bnl->cH, c);

	if (xb) {
	  v = (act_booleanized_var_t *)xb->v;
	  if (v->isint && type == 1) {
	    return c;
	  }
	  else if (v->ischan && (type == 2 || type == 3)) {
	    return c;
	  }
	  else if (type == 0) {
	    return c;
	  }
	  /*-- else continue searching --*/
	}
	else {
	  act_dynamic_var_t *dv;
	  xb = phash_lookup (si->bnl->cdH, c);
	  Assert (xb, "What?!");
	  dv = (act_dynamic_var_t *)xb->v;
	  if (type == 1 && dv->isint) {
	    *doff = 0;
	    return c;
	  }
	  else if (type == 0) {
	    *doff = 0;
	    return c;
	  }
	  /* -- else continue searching -- */
	}
      }
    }

    /* -- search dynamic info -- */
    phash_iter_init (si->bnl->cdH, &bi);
    while ((b = phash_iter_next (si->bnl->cdH, &bi))) {
      act_dynamic_var_t *dv = (act_dynamic_var_t *)b->v;
      act_connection *c = (act_connection *)b->key;
      phash_bucket_t *xb = phash_lookup (si->map, c);
      Assert (xb, "What?");
      if (xb->i < off && off < xb->i + dv->a->size()) {
	if (type == 1 && dv->isint) {
	  *doff = off - xb->i;
	  return c;
	}
	else if (type == 0) {
	  *doff = off - xb->i;
	  return c;
	}
      }
    }
    return NULL;
  }  
}

/*------------------------------------------------------------------------
 *
 *  Check if the connection exists
 *
 *------------------------------------------------------------------------
 */
bool ActStatePass::connExists (stateinfo_t *si, act_connection *c)
{
  phash_bucket_t *b;
  
  b = phash_lookup (si->bnl->cdH, c);

  if (!b) {
    b = phash_lookup (si->bnl->cH, c);
  }
  
  if (b) {
    return true;
  }
  else {
    return false;
  }
}


ActStatePass::~ActStatePass()
{
  /* free stuff */
}



int ActStatePass::globalBoolOffset (ActId *id)
{
  Process *top_level = _root_si->bnl->p;
  
  /* -- partition processes -- */
  Process *loc;
  ActId *rest = id->nonProcSuffix (top_level, &loc);
  Assert (loc, "Hmm");

  /* -- "rest" may actually be available at a higher level of the
     hierarchy since it may be connected to ports or globals.
     -- */
  stateinfo_t *si;
  act_connection *conn = rest->Canonical (loc->CurScope());
  phash_bucket_t *ib;
  Assert (conn, "Hmm");
  
  if (conn->isglobal()) {
    /* global ID: done! */
    si = _root_si;
    Assert (si, "Hmm");
    ib = phash_lookup (si->map, conn);
    Assert (ib, "Hmm");
    loc = top_level;
  }
  else {
    /* is it a port? */
    si = getStateInfo (loc);
    Assert (si, "Hmm");
    ib = phash_lookup (si->map, conn);
    Assert (ib, "Hmmmmm");
    while (loc != top_level && (ib->i < 0)) {
      /* port in a subprocess! */
      ActId *rootid;
      InstType *it;
      Process *cproc = top_level;

      /* find parent id */
      rootid = id;
      while (rootid->Rest() != rest) {
	it = cproc->CurScope()->FullLookup (rootid->getName());
	Assert (TypeFactory::isProcessType (it), "What?");
	cproc = dynamic_cast<Process *> (it->BaseType());
	rootid = rootid->Rest();
      }

      /* segment: tmp.port */
      ActId *tmp = new ActId (rootid->getName(), rootid->arrayInfo());
      tmp->Append (conn->toid());
      conn = tmp->Canonical (cproc->CurScope());
      delete tmp;

      /* repeat this! */
      rest = rootid;
      loc = cproc;
      si = getStateInfo (loc);
      Assert (si, "Hmm");
      ib = phash_lookup (si->map, conn);
      Assert (ib, "What?");
    }
  }

  int local_si_id = ib->i;

  /* 
     If negative offset, this HAS to be the top-level since it is a
     global signal or a top-level port.
  */
  Assert ((ib->i >= 0) || ((top_level == loc) && rest == id), "What?");

  /* -- conn : canonical connection
     -- loc  : root process
     -- rest : the ID segment corresponding to it.
  */
  state_counts soff;

  Scope *sc = top_level->CurScope();
  ValueIdx *vx = sc->FullLookupVal (id->getName());
  Assert (vx, "Hmm");

  /* -- check all the array indices! -- */
  ValueIdx *ux = vx;
  ActId *rid = id;
  
  
  ux = vx;
  rid = id;
  sc = top_level->CurScope();
  si = _root_si;

  while (rid != rest) {
    int aoff;
    if (ux->t->arrayInfo()) {
      if (!rid->arrayInfo() || !rid->arrayInfo()->isDeref()) {
	fatal_error ("Array not basic id: should have been caught earlier!");
      }
      aoff = ux->t->arrayInfo()->Offset (rid->arrayInfo());
    }
    else {
      if (rid->arrayInfo()) {
	fatal_error ("Array reference for non-arrayed type: should have been caught earlier!");
      }
      aoff = 0;
    }
    if (aoff == -1) {
      fatal_error ("Array index out of range!");
    }

    ib = phash_lookup (si->inst, ux);
    Assert (ib, "Hmm");
    soff.addVar (*((state_counts *)ib->v));

    Process *user = dynamic_cast<Process *> (ux->t->BaseType());
    Assert (user, "Partitioning failed?");

    si = getStateInfo (user);
    Assert (si, "Hmm...");

    if (aoff > 0) {
      soff.addVar (si->all, aoff);
    }

    rid = rid->Rest();
    sc = user->CurScope();
    Assert (rid, "What?");
    ux = sc->LookupVal (rid->getName());
  }

  return soff.numBools() + local_si_id;
}


int ActStatePass::checkIdExists (ActId *id)
{
  Process *top_level = _root_si->bnl->p;
  
  /* -- partition processes -- */
  Process *loc;
  ActId *rest = id->nonProcSuffix (top_level, &loc);

  if (!loc) return 0;
  
  Assert (loc, "Hmm");

  /* -- "rest" may actually be available at a higher level of the
     hierarchy since it may be connected to ports or globals.
     -- */
  stateinfo_t *si;
  act_connection *conn = rest->Canonical (loc->CurScope());
  phash_bucket_t *ib;

  if (!conn) return 0;
  
  Assert (conn, "Hmm");
  
  if (conn->isglobal()) {
    /* global ID: done! */
    si = _root_si;
    Assert (si, "Hmm");
    ib = phash_lookup (si->map, conn);
    if (!ib) return 0;
    Assert (ib, "Hmm");
    loc = top_level;
  }
  else {
    /* is it a port? */
    si = getStateInfo (loc);
    Assert (si, "Hmm");
    ib = phash_lookup (si->map, conn);
    if (!ib) return 0;
    Assert (ib, "Hmmmmm");
    while (loc != top_level && (ib->i < 0)) {
      /* port in a subprocess! */
      ActId *rootid;
      InstType *it;
      Process *cproc = top_level;

      /* find parent id */
      rootid = id;
      while (rootid->Rest() != rest) {
	it = cproc->CurScope()->FullLookup (rootid->getName());
	if (!it || !TypeFactory::isProcessType (it)) return 0;
	Assert (TypeFactory::isProcessType (it), "What?");
	cproc = dynamic_cast<Process *> (it->BaseType());
	rootid = rootid->Rest();
      }

      /* segment: tmp.port */
      ActId *tmp = new ActId (rootid->getName(), rootid->arrayInfo());
      tmp->Append (conn->toid());
      conn = tmp->Canonical (cproc->CurScope());
      delete tmp;

      /* repeat this! */
      rest = rootid;
      loc = cproc;
      si = getStateInfo (loc);
      Assert (si, "Hmm");
      ib = phash_lookup (si->map, conn);
      if (!ib) {
	return 0;
      }
      Assert (ib, "What?");
    }
  }

  
  if (!((ib->i >= 0) || ((top_level == loc) && rest == id))) {
    return 0;
  }

  /* -- conn : canonical connection
     -- loc  : root process
     -- rest : the ID segment corresponding to it.
  */
  state_counts soff;

  Scope *sc = top_level->CurScope();
  ValueIdx *vx = sc->FullLookupVal (id->getName());
  if (!vx) return 0;
  Assert (vx, "Hmm");

  /* -- check all the array indices! -- */
  ValueIdx *ux = vx;
  ActId *rid = id;
  
  
  ux = vx;
  rid = id;
  sc = top_level->CurScope();
  si = _root_si;

  while (rid != rest) {
    int aoff;
    if (ux->t->arrayInfo()) {
      if (!rid->arrayInfo() || !rid->arrayInfo()->isDeref()) {
	return 0;
      }
      aoff = ux->t->arrayInfo()->Offset (rid->arrayInfo());
    }
    else {
      if (rid->arrayInfo()) {
	return 0;
      }
      aoff = 0;
    }
    if (aoff == -1) {
      return 0;
    }

    ib = phash_lookup (si->inst, ux);
    if (!ib) {
      return 0;
    }
    soff.addVar (*((state_counts *)ib->v));

    Process *user = dynamic_cast<Process *> (ux->t->BaseType());
    if (!user) {
      return 0;
    }
    Assert (user, "Partitioning failed?");

    si = getStateInfo (user);
    if (!si) {
      return 0;
    }
    Assert (si, "Hmm...");

    if (aoff > 0) {
      soff.addVar (si->all, aoff);
    }

    rid = rid->Rest();
    sc = user->CurScope();
    if (!rid) {
      return 0;
    }
    Assert (rid, "What?");
    ux = sc->LookupVal (rid->getName());
  }
  return 1;
}
