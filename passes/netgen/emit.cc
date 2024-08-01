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
#include <map>
#include <string.h>
#include "netlist.h"
#include <common/config.h>
#include <act/iter.h>

#define VINF(x) ((struct act_varinfo *)((x)->extra))

static void print_number (FILE *fp, double x)
{
  if (x > 1e3) {
    fprintf (fp, "%gK", x*1e-3);
  }
  if (x > 1e-3) {
    fprintf (fp, "%g", x);
  }
  else if (x > 1e-9) {
    fprintf (fp, "%gU", x*1e6);
  }
  else {
    fprintf (fp, "%gP", x*1e12);
  }
}

netlist_t *ActNetlistPass::emitNetlist (Process *p)
{
  FILE *fp = _outfp;
  netlist_t *n = getNL (p);
  char *tmp_str;

  if (!n) {
    fprintf (stderr, "Could not find process `%s'", p ? p->getName() : "-none");
    fprintf (stderr, " in created netlists; inconstency!\n");
    fatal_error ("Internal inconsistency or error in pass ordering!");
  }

  if (n->bN->p && black_box_mode &&
      (n->bN->p->isBlackBox() || n->bN->p->isLowLevelBlackBox())) {
    if (n->bN->macro && n->bN->macro->isValid()) {
      const char *tmp = n->bN->macro->getSPICEFile();
      if (tmp) {
	FILE *sfp = fopen (tmp, "r");
	if (!sfp) {
	  warning ("Black box %s: could not open spice file `%s'",
		   p->getName(), tmp);
	}
	else {
	  char buf[1024];
	  int sz;
	  while ((sz = fread (buf, 1, 1024, sfp)) > 0) {
	    fwrite (buf, 1, sz, fp);
	  }
	  fclose (sfp);
	}
      }
      else {
	fprintf (fp, "* black box: %s; missing SPICE file path\n", p->getName());
      }
    }
    return n;
  }
  
  if (top_level_only && p != _root) return n;

  fprintf (fp, "*\n");
  if (p) {
    tmp_str = p->getFullName();
  }
  else {
    tmp_str = NULL;
  }
  fprintf (fp, "*---- act defproc: %s -----\n", tmp_str ? tmp_str : "-none-");
  if (tmp_str) {
    FREE (tmp_str);
  }
  if (a->mangle_active()) {
    fprintf (fp, "* raw ports: ");
    for (int k=0; k < A_LEN (n->bN->ports); k++) {
      if (n->bN->ports[k].omit) continue;
      fprintf (fp, " ");
      ActId *id = n->bN->ports[k].c->toid();
      id->Print (fp);
      delete id;
    }
    fprintf (fp, "\n*\n");
  }
  int out = 0;
  if (p) {
    if (n->bN->isempty) {
      fprintf (fp, "* empty subckt\n");
    }
    else {
      fprintf (fp, ".subckt ");
      /* special mangling for processes that only end in <> */
      a->mfprintfproc (fp, p);
      for (int k=0; k < A_LEN (n->bN->ports); k++) {
	if (n->bN->ports[k].omit) continue;
	ActId *id = n->bN->ports[k].c->toid();
	char buf[10240];
	id->sPrint (buf, 10240);
	a->mfprintf (fp, " %s", buf);
	delete id;
	out = 1;
      }

      if (n->weak_supply_vdd > 0) {
	fprintf (fp, " #%d", n->nid_wvdd);
      }

      if (n->weak_supply_gnd > 0) {
	fprintf (fp, " #%d", n->nid_wgnd);
      }
      fprintf (fp, "\n");
    }
  }

  /* print pininfo */
  if (out) {
    fprintf (fp, "*.PININFO");
    for (int k=0; k < A_LEN (n->bN->ports); k++) {
      if (n->bN->ports[k].omit) continue;
      ActId *id = n->bN->ports[k].c->toid();
      char buf[10240];
      id->sPrint (buf, 10240);
      a->mfprintf (fp, " %s", buf);
      fprintf (fp, ":%c", n->bN->ports[k].input ? 'I' : 'O');
      delete id;
    }
    if (n->weak_supply_vdd > 0) {
      fprintf (fp, " #%d:I", n->nid_wvdd);
    }
    if (n->weak_supply_gnd > 0) {
      fprintf (fp, " #%d:I", n->nid_wgnd);
    }
    fprintf (fp, "\n");
  }

  listitem_t *vi;

  if (!list_isempty (n->vdd_list)) {
    fprintf (fp, "*.POWER VDD");
    for (vi = list_first (n->vdd_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      emit_node (n, fp, x, NULL, NULL, 1);
    }
    fprintf (fp, "\n");
  }

  if (!list_isempty (n->gnd_list)) {
    fprintf (fp, "*.POWER GND");
    for (vi = list_first (n->gnd_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      emit_node (n, fp, x, NULL, NULL, 1);
    }
    fprintf (fp, "\n");
  }

  if (!list_isempty (n->psc_list)) {
    /* this is the power supply for the subtrate of n-type
       transistors. It's a P-type substrate! 
    */
    fprintf (fp, "*.POWER NSUB");
    for (vi = list_first (n->psc_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      emit_node (n, fp, x, NULL, NULL, 1);
    }
    fprintf (fp, "\n");
  }

  if (!list_isempty (n->nsc_list)) {
    fprintf (fp, "*.POWER PSUB");
    for (vi = list_first (n->nsc_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      emit_node (n, fp, x, NULL, NULL, 1);
    }
    fprintf (fp, "\n");
  }

  out = 0;
  node_t *x;
  for (x = n->hd; x; x = x->next) {
    if (!x->v) continue;
    if (!x->v->v->output) continue;
    ActId *id = x->v->v->id->toid();
    char buf[10240];
    id->sPrint (buf, 10240);
    delete id;
    if (!out) {
      fprintf (fp, "*\n* --- node flags ---\n*\n");
      out = 1;
    }
    a->mfprintf (fp, "* %s ", buf);
    if (x->v->stateholding && !x->v->unstaticized) {
      fprintf (fp, "(state-holding): pup_reff=%g; pdn_reff=%g\n",
	       x->reff[EDGE_PFET], x->reff[EDGE_NFET]);
    }
    else {
      fprintf (fp, "(combinational)\n");
    }
  }
  if (out) {
    fprintf (fp, "*\n* --- end node flags ---\n*\n");
  }
 
  
  /*-- emit local netlist --*/
  int fets = 0;
  int ncaps = 0;
  char devname[1024];
  int repnodes = 0;

  double oscale2 = output_scale_factor * output_scale_factor;
  double lambda2 = lambda*lambda;

  for (x = n->hd; x; x = x->next) {
    listitem_t *li;

    /* emit node cap, if any */
    if ((x->cap > 0) && !ignore_loadcap) {
      fprintf (fp, "C_per_node_%d ", ncaps++);
      emit_node (n, fp, x, NULL, NULL, 1);
      fprintf (fp, " ");
      emit_node (n, fp, n->GND, NULL, NULL, 1);
      fprintf (fp, " %g\n", x->cap*1e-15);
    }
    
    for (li = list_first (x->e); li; li = list_next (li)) {
      edge_t *e = (edge_t *)list_value (li);
      node_t *src, *drain;

      int len_repeat, width_repeat;
      int width_last;
      int il, iw;
      int w, l;
      int fold;
      int leak;
      int len_idx;
      
      if (e->visited || e->pruned) continue;
      e->visited = 1;

      w = e->w;
      l = e->l;
      leak = 0;

      if (e->l == min_l_in_lambda*getGridsPerLambda() && n->leak_correct) {
	leak = 1;
      }

      /* discretize lengths */
      len_repeat = e->nlen;
      if (discrete_len > 0) {
	l = discrete_len*getGridsPerLambda();
      }
      else if (len_repeat > 1) {
	len_idx = find_length_window (e);
	Assert (len_idx != -1, "Hmm");
	l = discrete_fet_length[len_idx+1]*getGridsPerLambda();
      }

      if (e->type == EDGE_NFET) {
	fold = n_fold;
      }
      else {
	Assert (e->type == EDGE_PFET, "Hmm");
	fold = p_fold;
      }

      width_repeat = e->nfolds;
      
      if (swap_source_drain) {
	src = e->b;
	drain = e->a;
      }
      else {
	src = e->a;
	drain = e->b;
      }
	
      for (il = 0; il < len_repeat; il++) {
	for (iw = 0; iw < width_repeat; iw++) {
	  char dev_name[256];
	  int sz = 256;
	  int len = 0;
	  dev_name[0] = '\0';

#define UPDATE_SZ_LEN						\
	  do {							\
	    int tmp = strlen (dev_name + len);			\
	    len += tmp;						\
	    sz -= len;						\
	    Assert (sz > 0, "Increase device name buffer!");	\
	  } while (0)

	  if (width_repeat > 1) {
	    w = EDGE_WIDTH (e, iw);
	  }
	  else {
	    w = e->w;
	  }
	
	  if (use_subckt_models) {
	    fprintf (fp, "x");
	  }

	  snprintf (dev_name + len, sz, "M%d", fets);
	  UPDATE_SZ_LEN;

	  if (len_repeat > 1) {
	    snprintf (dev_name + len, sz, "_%d", il);
	    UPDATE_SZ_LEN;
	  }
	  if (width_repeat > 1) {
	    snprintf (dev_name + len, sz, "_%d", iw);
	    UPDATE_SZ_LEN;
	  }

	  /* name of the instance includes how the fet was generated in it */
	  if (e->pchg) {
	    snprintf (dev_name + len, sz, "_pchg");
	  }
	  else if (e->combf) {
	    snprintf (dev_name + len, sz, "_ckeeper");
	  }
	  else if (e->keeper) {
	    snprintf (dev_name + len, sz, "_keeper");
	  }
	  else if (e->raw) {
	    snprintf (dev_name + len, sz, "_pass");
	  }
	  else {
	    snprintf (dev_name + len, sz, "_");
	  }
	  UPDATE_SZ_LEN;

	  fprintf (fp, "%s ", dev_name);

	  /* if length repeat, source/drain changes */
	  if (il == 0) {
	    emit_node (n, fp, src, dev_name, "S", 2);
	  }
	  else {
	    char buf[32];
	    snprintf (buf, 32, "#l%d", repnodes);
	    if (split_net (buf)) {
	      fprintf (fp, "%s", dev_name);
	      a->mfprintf (fp, ".S");
	    }
	    else {
	      fprintf (fp, "%s", buf);
	    }
	  }
	  fprintf (fp, " ");
	  emit_node (n, fp, e->g, dev_name, "G", 2);
	  fprintf (fp, " ");

	  if (il == len_repeat-1) {
	    emit_node (n, fp, drain, dev_name, "D", 2);
	  }
	  else {
	    char buf[32];
	    snprintf (buf, 32, "#l%d", repnodes+1);
	    if (split_net (buf)) {
	      fprintf (fp, "%s", dev_name);
	      a->mfprintf (fp, ".D");
	    }
	    else {
	      fprintf (fp, "%s", buf);
	    }
	  }
	  if (len_repeat > 1 && il != len_repeat-1) {
	    repnodes++;
	  }

	  fprintf (fp, " ");
	  /* Do we need spef for bulk? */
	  emit_node (n, fp, e->bulk, NULL, NULL, 1);

	  snprintf (devname, 1024, "net.%cfet_%s", (e->type == EDGE_NFET ? 'n' : 'p'),
		   act_dev_value_to_string (e->flavor));
	  if (!config_exists (devname)) {
	    act_error_ctxt (stderr);
	    fatal_error ("Device mapping for `%s' not defined in technology file.", devname);
	  }
	  fprintf (fp, " %s", config_get_string (devname));
	  fprintf (fp, " W=");
	  print_number (fp, w*manufacturing_grid*output_scale_factor);
	  fprintf (fp, " L=");

	  if (len_repeat > 1 && discrete_len == 0 && il == len_repeat-1) {
	    print_number (fp, (find_length_fit (e->l - (e->nlen-1)*l)*
			  manufacturing_grid + leak*leak_adjust)
			  *output_scale_factor);
	  }
	  else {
	    print_number (fp, (l*manufacturing_grid + leak*leak_adjust)
			  *output_scale_factor);
	  }

	  if (_fin_width > 0) {
	    Assert ((w % _fin_width) == 0, "Internal inconsistency in fin width value");
	    fprintf (fp, " NFIN=%d", w/_fin_width);
	  }

	  /* print extra fet string */
	  if (extra_fet_string && strcmp (extra_fet_string, "") != 0) {
	    fprintf (fp, " %s\n", extra_fet_string);
	  }
	  else {
	    fprintf (fp, "\n");
	  }

#undef UPDATE_SZ_LEN	  

	  /* area/perim for source/drain */
	  if (emit_parasitics) {
	    int gap;

	    if (il == 0) {
	      if (src->v) {
		gap = fet_diff_overhang;
	      }
	      else {
		if (list_length (src->e) > 2 || width_repeat > 1) {
		  gap = fet_spacing_diffcontact;
		}
		else {
		  gap = fet_spacing_diffonly;
		}
	      }
	    }
	    else {
	      gap = fet_spacing_diffonly;
	    }
	    fprintf (fp, "+ AS=");
	    print_number (fp, (e->w/getGridsPerLambda()*gap)*(lambda2*oscale2));
	    fprintf (fp, " PS=");
	    print_number (fp, 2*(e->w/getGridsPerLambda() + gap)*
			  lambda*output_scale_factor);

	    if (il == len_repeat-1) {
	      if (drain->v) {
		gap = fet_diff_overhang;
	      }
	      else {
		if (list_length (drain->e) > 2 || width_repeat > 1) {
		  gap = fet_spacing_diffcontact;
		}
		else {
		  gap = fet_spacing_diffonly;
		}
	      }
	    }
	    else {
	      gap = fet_spacing_diffonly;
	    }
	    fprintf (fp, " AD=");
	    print_number (fp, (e->w/getGridsPerLambda()*gap)*(lambda2*oscale2));
	    fprintf (fp, " PD=");
	    print_number (fp, 2*(e->w/getGridsPerLambda() + gap)*
			  lambda*output_scale_factor);
	    fprintf (fp, "\n");
	  }

	  fflush (fp);

	  if (e->type == EDGE_NFET) {
	    if (max_n_w_in_lambda != 0 && e->w > max_n_w_in_lambda*getGridsPerLambda()) {
	      act_error_ctxt (stderr);
	      fatal_error ("Device #%d: nfet width (%d) exceeds maximum limit (%d)\n", fets-1, e->w/getGridsPerLambda(), max_n_w_in_lambda);
	    }
	  }
	  else {
	    if (max_p_w_in_lambda != 0 && e->w > max_p_w_in_lambda*getGridsPerLambda()) {
	      act_error_ctxt (stderr);
	      fatal_error ("Device #%d: pfet width (%d) exceeds maximum limit (%d)\n", fets-1, e->w/getGridsPerLambda(), max_p_w_in_lambda);
	    }
	  }
	}
      }
      fets++;
    }
  }

  if (n->devs) {
    int ncap = 0;
    char **table = config_get_table_string ("net.device");
    for (listitem_t *li = list_first (n->devs); li; li = list_next (li)) {
      netlist_device *c = (netlist_device *) list_value (li);
      fprintf (fp, "%s%d_%d ", table[c->idx], fets++, ncap++);
      /* Need to know pin names for these devices! */
      emit_node (n, fp, c->n1, NULL, NULL, /* FIXME */ 1);
      fprintf (fp, " ");
      emit_node (n, fp, c->n2, NULL, NULL, /* FIXME */ 1);
      fprintf (fp, " ");
      fprintf (fp, "%g\n", c->wval*c->lval);
    }
  }

  /* clear visited flag */
  for (x = n->hd; x; x = x->next) {
    listitem_t *li;
    for (li = list_first (x->e); li; li = list_next (li)) {
      edge_t *e = (edge_t *)list_value (li);
      e->visited = 0;
    }
  }

  ActUniqProcInstiter i(p ? p->CurScope() : ActNamespace::Global()->CurScope());

  /*-- emit instances --*/
  int iport = 0;
  int iweak = 0;
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    netlist_t *sub;
    Process *instproc = dynamic_cast<Process *>(vx->t->BaseType ());
    sub = getNL (instproc);
    int ports_exist;
      
    ports_exist = 0;
    for (int i=0; i < A_LEN (sub->bN->ports); i++) {
      if (sub->bN->ports[i].omit == 0) {
	ports_exist = 1;
	break;
      }
    }

    if (ports_exist) {
      Arraystep *as;
      if (vx->t->arrayInfo()) {
	as = vx->t->arrayInfo()->stepper();
      }
      else {
	as = NULL;
      }
      do {
	if (!as || (!as->isend() && vx->isPrimary (as->index()))) {
	  char *str = NULL;
	  if (as) {
	    str = as->string();
	  }
	  if (sub->bN->isempty) {
	    fprintf (fp, "* ");
	  }
	  a->mfprintf (fp, "x%s%s", vx->getName(), str ? str : "");
	  for (int i=0; i < A_LEN (sub->bN->ports); i++) {
	    ActId *id;
	    char buf[10240];
	    if (sub->bN->ports[i].omit) continue;

	    Assert (iport < A_LEN (n->bN->instports), "Hmm");
	    fprintf (fp, " ");
	    sprint_conn (buf, 10240, n->bN->instports[iport]);
	    if (split_net (buf)) {
	      a->mfprintf (fp, "%s%s", vx->getName(), str ? str : "");
	      a->mfprintf (fp, ".");
	      ActId *idtmp = sub->bN->ports[i].c->toid();
	      idtmp->sPrint (buf, 10240);
	      delete idtmp;
	      a->mfprintf (fp, "%s", buf);
	    }
	    else {
	      a->mfprintf (fp, "%s", buf);
	    }
	    iport++;
	  }
	  if (str) {
	    FREE (str);
	  }

	  if (sub->weak_supply_vdd > 0) {
	    Assert (iweak < A_LEN (n->instport_weak), "What?");
	    /* XXX: what do I do here? */
	    fprintf (fp, " #%d", n->instport_weak[iweak++]);
	  }
	  if (sub->weak_supply_gnd > 0) {
	    Assert (iweak < A_LEN (n->instport_weak), "What?");
	    /* XXX: what do I do here? */
	    fprintf (fp, " #%d", n->instport_weak[iweak++]);
	  }
	    
	  a->mfprintf (fp, " ");
	  a->mfprintfproc (fp, instproc);
	  a->mfprintf (fp, "\n");
	}
	if (as) {
	  as->step ();
	}
      } while (as && !as->isend());
      if (as) {
	delete as;
      }
    }
  }
  Assert (iport == A_LEN (n->bN->instports), "Hmm...");
  Assert (iweak == A_LEN (n->instport_weak), "Hmm...");

  if (_annotate) {
    _annotate->runcmd ("dump");
  }

  if (p) {
    if (n->bN->isempty) {
      fprintf (fp, "* end empty subckt\n");
    }
    else {
      fprintf (fp, ".ends\n");
    }
  }
  fprintf (fp, "*---- end of process: %s -----\n", p ? p->getName() : "-none-");

  return n;
}

void ActNetlistPass::Print (FILE *fp, Process *p)
{
  Assert (!p || p->isExpanded (), "Process must be expanded!");

  if (!completed()) {
    fatal_error ("ActNetlistPass::Print() called before pass is run!");
  }
  if (!_annotate) {
    ActPass *ap = a->pass_find ("annotate");
    if (ap) {
       _annotate = dynamic_cast <ActDynamicPass *> (ap);
       Assert (_annotate, "WHat?");
       if (!_annotate->completed()) {
          _annotate->run (p);
       }
       current_annotate = _annotate;
    }
  }
  if (_annotate) {
    _annotate->setParam ("outfp", (void *)fp);
  }

  _outfp = fp;
  run_recursive (p, 1);
  _outfp = NULL;

  if (_annotate) {
    _annotate->clearParam ("outfp");
    _annotate->clearParam ("proc");
    _annotate->clearParam ("net");
  }
  current_annotate = NULL;
}


void ActNetlistPass::printFlat (FILE *fp)
{
  if (!completed()) {
    fatal_error ("ActNetlistPass::printFlat() called before pass is run!");
  }
  if (!_root) {
    fatal_error ("ActNetlistPass::printFlat() requires a top-level process!");
  }
  bools->createNets (_root);
  _outfp = fp;
  _invNetH = phash_new (4);

  if (!_annotate) {
    ActPass *ap = a->pass_find ("annotate");
    if (ap) {
       _annotate = dynamic_cast <ActDynamicPass *> (ap);
       Assert (_annotate, "WHat?");
       if (!_annotate->completed()) {
	 _annotate->run (_root);
       }
       current_annotate = _annotate;
    }
  }
  if (_annotate) {
    _annotate->setParam ("outfp", (void *)fp);
  }
  
  run_recursive (_root, 2);

  current_annotate = NULL;

  // Run a new pass that only prints cells and
  // computes the inverse hash that we need to accelerate net
  // identification.
  

  act_boolean_netlist_t *bnl = bools->getBNL (_root);
  fprintf (_outfp, ".subckt ");

  a->mfprintfproc (_outfp, _root);
  for (int k=0; k < A_LEN (bnl->ports); k++) {
    if (bnl->ports[k].omit) continue;
    ActId *id = bnl->ports[k].c->toid();
    char buf[10240];
    id->sPrint (buf, 10240);
    a->mfprintf (_outfp, " %s", buf);
    delete id;
  }
  fprintf (_outfp, "\n");

  list_t *stk = list_new ();
  _printflat (NULL, NULL, stk, _root);
  list_free (stk);

  fprintf (_outfp, ".ends\n");
  _outfp = NULL;

  if (_annotate) {
    _annotate->clearParam ("outfp");
    _annotate->clearParam ("proc");
    _annotate->clearParam ("net");
  }

  // free each hash table entry, and then...
  {
    phash_bucket_t *pb;
    phash_iter_t it;
    phash_iter_init (_invNetH, &it);
    while ((pb = phash_iter_next (_invNetH, &it))) {
      struct cHashtable *cH = (struct cHashtable *) pb->v;
      chash_free (cH);
    }
  }
  phash_free (_invNetH);
  
}


/*
  stk = stack of act_boolean_netlist_t pointers corresponding to the
  call hierarchy.
*/
void ActNetlistPass::_print_flat_cell (act_boolean_netlist_t *bnl,
				       list_t *stk,
				       ActId *inst, Process *p)
{
  char buf[10240];
  list_t *l;
  fprintf (_outfp, "x");
  inst->sPrint (buf, 10240);
  a->mfprintf (_outfp, "%s ", buf);

  l = list_new ();
  list_append_head (l, inst);
  while (inst->Rest()) {
    inst = inst->Rest();
    list_append_head (l, inst);
  }

  Assert (list_length (l) == list_length (stk), "What?");

  /* print ports! */
  for (int i=0; i < A_LEN (bnl->ports); i++) {
    if (bnl->ports[i].omit) continue;
    int count = 0;
    bool found = false;
    act_local_net_t *thenet = NULL;
    act_boolean_netlist_t *bN;
    struct cHashtable *cH;
    
    // search for pin!
#if 0
    printf ("\nsearch for: ");
    a->mfprintfproc (stdout, p);
    printf (" / ");
    bnl->ports[i].c->Print (stdout);
    printf (" / ");
    ((ActId *)stack_peek (l))->Print (stdout);
    printf ("\n");
#endif
    ActId *look;
    listitem_t *bnl_li, *id_li;
    bnl_li = list_first (stk);
    id_li = list_first (l);
    while (!found && bnl_li) {
      phash_bucket_t *pb;

      bN = (act_boolean_netlist_t *) list_value (bnl_li);

      pb = phash_lookup (_invNetH, bN->p);
      cH = (struct cHashtable *) pb->v;
      Assert (cH, "What?");
      
      look = (ActId *) list_value (id_li);
#if 0      
      for (int j=0; !found && j < A_LEN (bN->nets); j++) {
	if (bN->nets[j].skip) continue;
#if 0      
	printf (" net: ");
	bN->nets[j].net->Print (stdout);
	printf ("\n");
#endif      
	for (int k=0; k < A_LEN (bN->nets[j].pins); k++) {
#if 0
	  printf ("   check: ");
	  //a->mfprintfproc (stdout, bN->nets[j].pins[k].cell);
	  printf (" / ");
	  bN->nets[j].pins[k].pin->Print (stdout);
	  printf (" / ");
	  bN->nets[j].pins[k].inst->Print (stdout);
	  printf ("\n");
#endif	
	  if (/*bN->nets[j].pins[k].cell == p &&*/
	      bN->nets[j].pins[k].pin == bnl->ports[i].c &&
	      bN->nets[j].pins[k].inst->isEqual (look)) {
	    thenet = &bN->nets[j];
	    found = true;
	    break;
	  }
	}
      }
#endif
      
      act_local_pin_t tst;
      chash_bucket_t *cb;
      tst.inst = look;
      tst.pin = bnl->ports[i].c;
      cb = chash_lookup (cH, &tst);

      /* replacement */
      if (cb) {
	found = true;
	thenet = (act_local_net_t *)cb->v;
      }

#if 0      
      if (found) {
	if (!cb) {
	  printf ("** (not found in hash) mismatch for ");
	  tst.inst->Print (stdout);
	  printf(" / ");
	  tst.pin->Print (stdout);
	  printf ("\n");
	}
	else {
	  if (cb->v != (void *)thenet) {
	    printf ("** >ptr mismatch for ");
	    tst.inst->Print (stdout);
	    printf(" / ");
	    tst.pin->Print (stdout);
	    printf ("\n");
	  }
	}
      }
      else {
	if (cb) {
	  printf ("** (found in hash) mismatch for ");
	  tst.inst->Print (stdout);
	  printf(" / ");
	  tst.pin->Print (stdout);
	  printf ("\n");
	}
      }
#endif
      bnl_li = list_next (bnl_li);
      id_li = list_next (id_li);
      if (found && bnl_li) {
	if (thenet->port) {
	  // this has a higher-level net specified
	  found = false;
	  thenet = NULL;
	}
      }
      
    }
    if (found) {
      ActId *tmp = thenet->net->toid();
      tmp->sPrint (buf, 10240);
      a->mfprintf (_outfp, "%s", buf);
      delete tmp;
    }
    else {
      fprintf (_outfp, "-?-");
    }
    fprintf (_outfp, " ");
  }

  list_free (l);
	    
  a->mfprintfproc (_outfp, p);
  fprintf (_outfp, "\n");
}

void ActNetlistPass::_printflat (ActId *prefix, ActId *tl,
				 list_t *stk, Process *p)
{
  ActUniqProcInstiter inst(p->CurScope());
  list_append_head (stk, bools->getBNL (p));
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    char buf[10240];
    ValueIdx *vx = (*inst);
    ActId *newid = new ActId (vx->getName());
    ActId *ntl;
    Process *iproc = dynamic_cast<Process *>(vx->t->BaseType());
    act_boolean_netlist_t *bnl;
    Assert (iproc, "What?");
    bnl = bools->getBNL (iproc);

    if (tl) {
      tl->Append (newid);
      ntl = tl->Rest ();
    }
    else {
      prefix = newid;
      tl = newid;
      ntl = tl;
    }

    if (vx->t->arrayInfo()) {
      Arraystep *as = vx->t->arrayInfo()->stepper();
      while (!as->isend()) {
	if (vx->isPrimary (as->index())) {
	  Array *x = as->toArray ();
	  newid->setArray (x);
	  if (iproc->isCell()) {
	    _print_flat_cell (bnl, stk, prefix, iproc);
	  }
	  else {
	    _printflat (prefix, ntl, stk, iproc);
	  }
	  delete x;
	  newid->setArray (NULL);
	}
	as->step();
      }
      delete as;
    }
    else {
      if (iproc->isCell()) {
	_print_flat_cell (bnl, stk, prefix, iproc);
      }
      else {
	_printflat (prefix, ntl, stk, iproc);
      }
    }
    delete newid;
    if (prefix == newid) {
      prefix = NULL;
      tl = NULL;
    }
    else {
      tl->prune ();
    }
  }
  list_delete_head (stk);
}


static int hash_net (int sz, void *key)
{
  act_local_pin_t *p = (act_local_pin_t *)key;
  int ret;
  
  ret = hash_function_continue
    (sz, (const unsigned char *) &p->pin, sizeof (unsigned long), 0, 0);
  ret = p->inst->getHash (ret, sz);
  return ret;
}

static int match_net (void *k1, void *k2)
{
  act_local_pin_t *p1, *p2;
  p1 = (act_local_pin_t *) k1;
  p2 = (act_local_pin_t *) k2;

  if (p1->pin != p2->pin) return 0;
  if (p1->inst->isEqual (p2->inst)) return 1;
  return 0;
}

static void *dup_net (void *key)
{
  return key;
}

static void free_net (void *key)
{
  // nothing
}

static void print_net (FILE *fp, void *key)
{
  // nothing
}


void ActNetlistPass::flatHelper (Process *p)
{
  if (!p) return;
  if (p->isCell()) {
    emitNetlist (p);
  }
  else {
    // compute inverse net hash!
    act_boolean_netlist_t *bnl;
    phash_bucket_t *b = phash_add (_invNetH, p);
    struct cHashtable *cH = chash_new (4);
    cH->hash = hash_net;
    cH->match = match_net;
    cH->dup = dup_net;
    cH->free = free_net;
    cH->print = print_net;
    b->v = cH;

    bnl = bools->getBNL (p);
    for (int i=0; i < A_LEN (bnl->nets); i++) {
      for (int j=0; j < A_LEN (bnl->nets[i].pins); j++) {
	chash_bucket_t *pb;
	pb = chash_add (cH, &bnl->nets[i].pins[j]);
	pb->v = &bnl->nets[i];
      }
    }
  }
}
