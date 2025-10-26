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

void ActNetlistPass::getSharedStatName (char *buf, int sz,
					int w, int pl, int nl)
{
  int pos, len;
  pos = 0;

  /** note the three colons; this is not a valid ACT process name **/
  snprintf (buf+pos, sz, "::cell:::weak_");
  len = strlen (buf+pos);
  pos += len;
  sz -= len;

  if (pl != 0 && nl != 0) {
    snprintf (buf+pos, sz, "supply<%d,%d,%d>", w, nl, pl);
  }
  else if (pl != 0) {
    snprintf (buf+pos, sz, "_up<%d,%d>", w, pl);
  }
  else {
    snprintf (buf+pos, sz, "_dn<%d,%d>", w, nl);
  }
}

void ActNetlistPass::emitWeakSupplies ()
{
  FILE *fp = _outfp;
  listitem_t *li;
  char buf[64];

  if (top_level_only) return;

  fprintf (fp, "*\n*--- weak supply instances ---*\n*\n");
  for (li = list_first (shared_stat_list); li; li = list_next (li)) {
    int fet = 0;
    int repcount = 0;
    shared_stat *s = (shared_stat *) list_value (li);
    fprintf (fp, ".subckt ");
    buf[0] = '\0';
    getSharedStatName (buf, 64, s->en ? s->en->w : s->ep->w,
		       s->ep ? s->ep->l : 0, s->en ? s->en->l : 0);
    a->mfprintf (fp, "%s", buf);
    if (s->en && s->ep) {
      fprintf (fp, " lvdd lgnd wvdd wgnd\n");
      _emit_one_fet (fp, NULL, s->en, fet, repcount);
      _emit_one_fet (fp, NULL, s->ep, fet, repcount);
    }
    else if (s->ep) {
      fprintf (fp, " lvdd lgnd wvdd\n");
      _emit_one_fet (fp, NULL, s->ep, fet, repcount);
    }
    else {
      fprintf (fp, " lvdd lgnd wgnd\n");
      _emit_one_fet (fp, NULL, s->en, fet, repcount);
    }
    fprintf (fp, ".ends\n");
  }
  fprintf (fp, "*\n*--- end weak supply instances ---*\n*\n");
}


void ActNetlistPass::_emit_one_fet (FILE *fp, netlist_t *n, edge_t *e,
				    int &fets, int &repnodes)
{
  char devname[1024];
  node_t *src, *drain;
  double lambda2 = lambda*lambda;
  double oscale2 = output_scale_factor * output_scale_factor;

  int len_repeat, width_repeat;
  int width_last;
  int il, iw;
  int w, l;
  int fold;
  int leak;

  if (e->visited || e->pruned) return;
  e->visited = 1;

  w = e->w;
  l = e->l;
  leak = 0;

  if (e->l == min_l_in_lambda*getGridsPerLambda() && (!n || n->leak_correct)) {
    leak = 1;
  }

  /* discretize lengths */
  len_repeat = e->nlen;
  if (discreteLength()) {
    l = discrete_len*getGridsPerLambda();
  }
  else if (len_repeat > 1) {
    int len_val = find_closest_length_upperbound (e);
    Assert (len_val > 0, "Hmm");
    l = len_val*getGridsPerLambda();
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
	if (!n) {
	  if (e->type == EDGE_PFET) {
	    fprintf (fp, "lvdd");
	  }
	  else {
	    fprintf (fp, "lgnd");
	  }
	}
	else {
	  emit_node (n, fp, src, dev_name, "S", 2);
	}
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
      if (!n) {
	if (e->type == EDGE_PFET) {
	  fprintf (fp, "lgnd");
	}
	else {
	  fprintf (fp, "lvdd");
	}
      }
      else {
	emit_node (n, fp, e->g, dev_name, "G", 2);
      }
      fprintf (fp, " ");

      if (il == len_repeat-1) {
	if (!n) {
	  if (e->type == EDGE_PFET) {
	    fprintf (fp, "wvdd");
	  }
	  else {
	    fprintf (fp, "wgnd");
	  }
	}
	else {
	  emit_node (n, fp, drain, dev_name, "D", 2);
	}
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
      if (!n) {
	if (e->type == EDGE_PFET) {
	  fprintf (fp, "lvdd");
	}
	else {
	  fprintf (fp, "lgnd");
	}
      }
      else {
	emit_node (n, fp, e->bulk, NULL, NULL, 1);
      }

      snprintf (devname, 1024, "net.%cfet_%s", (e->type == EDGE_NFET ? 'n' : 'p'),
		act_dev_value_to_string (e->flavor));
      if (!config_exists (devname)) {
	act_error_ctxt (stderr);
	fatal_error ("Device mapping for `%s' not defined in technology file.", devname);
      }
      fprintf (fp, " %s", config_get_string (devname));
      fprintf (fp, " %s=", param_names.w);
      print_number (fp, w*manufacturing_grid*output_scale_factor);
      fprintf (fp, " %s=", param_names.l);

      if (len_repeat > 1 && !discreteLength() && il == len_repeat-1) {
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
	if (!param_names.fin) {
	  fatal_error ("fin_width specified without fin name in fet parameters!");
	}
	fprintf (fp, " %s=%d", param_names.fin, w/_fin_width);
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
	fprintf (fp, "+ %s=", param_names.as);
	print_number (fp, (e->w/getGridsPerLambda()*gap)*(lambda2*oscale2));
	fprintf (fp, " %s=", param_names.ps);
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
	fprintf (fp, " %s=", param_names.ad);
	print_number (fp, (e->w/getGridsPerLambda()*gap)*(lambda2*oscale2));
	fprintf (fp, " %s=", param_names.pd);
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
    fets++;
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
  int repnodes = 0;

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
      _emit_one_fet (fp, n, e, fets, repnodes);
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

  /*-- emit weak instances --*/
  phash_bucket_t *pb = phash_lookup (shared_inst, p);
  if (pb) {
    list_t *l = (list_t *) pb->v;
    int wi_cnt = 0;
    Scope *sc = p ? p->CurScope () : ActNamespace::Global()->CurScope();
    fprintf (fp, "*--- weak supplies\n");
    for (listitem_t *li = list_first (l); li; li = list_next (li)) {
      shared_stat_inst *si = (shared_stat_inst *) list_value (li);
      char buf[64];
      do {
	snprintf (buf, 64, "wk_stat_%d", wi_cnt++);
      } while (sc->Lookup (buf));
      fprintf (fp, "x%s ", buf);
      emit_node (n, fp, n->Vdd,  NULL, NULL, 1);
      fprintf (fp, " ");
      emit_node (n, fp, n->GND,  NULL, NULL, 1);
      if (si->weak_vdd) {
	fprintf (fp, " ");
	emit_node (n, fp, si->weak_vdd, NULL, NULL, 1);
      }
      if (si->weak_gnd) {
	fprintf (fp, " ");
	emit_node (n, fp, si->weak_gnd, NULL, NULL, 1);
      }
      fprintf (fp, " ");

      getSharedStatName (buf, 64, si->w, si->pl, si->nl);
      a->mfprintf (fp, "%s", buf);
      fprintf (fp, "\n");
    }
  }

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
  _printflat (NULL, NULL, stk, _root, false);
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
				       ActId *inst, Process *p,
				       bool act_mode)
{
  char buf[10240];
  list_t *l;

  if (act_mode == false) {
    fprintf (_outfp, "x");
  }
  else {
    if (p->getns() != ActNamespace::Global()) {
      char *tmp = p->getns()->Name (true);
      fprintf (_outfp, " %s", tmp);
      FREE (tmp);
    }
    else {
      fprintf (_outfp, " ");
    }
    a->mfprintfproc (_outfp, p, true);
    fprintf (_outfp, " ");
  }
  inst->sPrint (buf, 10240);
  a->mfprintf (_outfp, "%s ", buf);

  ActId *orig_inst = inst;

  l = list_new ();
  int orig_level;
  list_append_head (l, inst);
  orig_level = 0;
  while (inst->Rest()) {
    inst = inst->Rest();
    list_append_head (l, inst);
    orig_level++;
  }

  Assert (list_length (l) == list_length (stk), "What?");

  /* print ports! */

  if (act_mode) {
    fprintf (_outfp, "(");
  }

  int k = -1;
  int sz = 0;
  int was_array = 0;
  int need_comma = 0;

  for (int i=0; i < A_LEN (bnl->ports); i++) {
    int count = 0;
    bool found = false;
    act_local_net_t *thenet = NULL;
    act_boolean_netlist_t *bN;
    struct cHashtable *cH;

    if ((mangled_ports_actflat || act_mode == false) && bnl->ports[i].omit) continue;

    if (act_mode == true) {
      if (!mangled_ports_actflat) {
	if (sz == 0) {
	  if (was_array) {
	    fprintf (_outfp, "}");
	    need_comma = 1;
	  }
	  k++;
	  InstType *pt = p->getPortType (k);
	  Assert (TypeFactory::isBoolType (pt), "Cells must have only Boolean ports");
	  if (pt->arrayInfo()) {
	    sz = pt->arrayInfo()->size();
	    if (need_comma) {
	      fprintf (_outfp, ",");
	    }
	    fprintf (_outfp, "{");
	    need_comma = 0;
	    was_array = 1;
	  }
	  else {
	    sz = 1;
	    was_array = 0;
	  }
	}
      }
      if (need_comma) {
	fprintf (_outfp, ",");
      }
    }
    
    ActId *look = NULL;
    listitem_t *bnl_li, *id_li;
    bnl_li = list_first (stk);
    id_li = list_first (l);
    int found_level = -1;
    int level = orig_level;
    while (!found && bnl_li) {
      phash_bucket_t *pb;

      bN = (act_boolean_netlist_t *) list_value (bnl_li);

      pb = phash_lookup (_invNetH, bN->p);
      cH = (struct cHashtable *) pb->v;
      Assert (cH, "What?");
      
      look = (ActId *) list_value (id_li);

      act_local_pin_t tst;
      chash_bucket_t *cb;
      tst.inst = look;
      tst.pin = bnl->ports[i].c;
      cb = chash_lookup (cH, &tst);

      /* replacement */
      if (cb) {
	found = true;
	thenet = (act_local_net_t *)cb->v;
#if 0
	printf ("\n --- found: ");
	look->Print (stdout);
	printf (" ; pin: ");
	bnl->ports[i].c->Print (stdout);
	printf (" ; net: ");
	thenet->net->Print (stdout);
	printf (" / level = %d\n", level);
#endif
	found_level = level;
      }
      bnl_li = list_next (bnl_li);
      id_li = list_next (id_li);
      if (found && bnl_li) {
	if (thenet->port) {
	  level--;
#if 0
	  printf (" == look higher!\n");
#endif
	  // this has a higher-level net specified
	  found = false;
	  thenet = NULL;
	}
      }
    }
    if (found) {
      ActId *stop;
      listitem_t *tmpli;
#if 0
      printf ("[lev: %d]", found_level);
      printf ("[orig: ");
      orig_inst->Print (stdout);
      printf ("][net: ");
      thenet->net->Print (stdout);
      printf ("]");
#endif
      stop = orig_inst;
      while (stop && found_level > 0) {
	stop = stop->Rest();
	found_level--;
      }
      if (stop != orig_inst) {
	orig_inst->sPrint (buf, 10240, stop);
      }
      else {
	buf[0] = '\0';
      }
      ActId *tmp = thenet->net->toid();
      if (buf[0] != '\0') {
	int tl = strlen (buf);
	if (tl < 10240) {
	  buf[tl] = '.';
	  tl++;
	  buf[tl] = '\0';
	}
	tmp->sPrint (buf + tl, 10240-tl);
      }
      else {
	tmp->sPrint (buf, 10240);
      }
      char buf2[10240];

      /* XXX:
	 if !mangled_ports and buf is actually the very top level
	 port, then we need to print it, not mangle print it
      */
      if (!mangled_ports_actflat &&
	  found_level == 0 && _root->FindPort (tmp->getName())) {
	snprintf (buf2, 10240, "%s", buf);
      }
      else {
	a->msnprintf (buf2, 10240, "%s", buf);
      }
      if (split_net (buf2)) {
	// inst.pin
	inst->sPrint (buf, 10240);
	a->mfprintf (_outfp, "%s.", buf);
	delete tmp;
	tmp = bnl->ports[i].c->toid();
	tmp->sPrint (buf, 10240);
	a->mfprintf (_outfp, "%s", buf);
      }
      else {
	fprintf (_outfp, "%s", buf2);
      }
      delete tmp;
    }
    else {
      fprintf (_outfp, "-?-");
    }
    if (act_mode == false) {
      fprintf (_outfp, " ");
    }
    else {
      need_comma = 1;
      if (!mangled_ports_actflat) {
	sz--;
      }
    }
  }

  list_free (l);

  if (act_mode == false) {
    a->mfprintfproc (_outfp, p);
  }
  else {
    if (!mangled_ports_actflat) {
      if (was_array) {
	fprintf (_outfp, "}");
      }
    }
    fprintf (_outfp, ");");
  }
  
  fprintf (_outfp, "\n");
}

void ActNetlistPass::_printflat (ActId *prefix, ActId *tl,
				 list_t *stk, Process *p,
				 bool act_mode)
{
  ActUniqProcInstiter inst(p->CurScope());
  list_append_head (stk, bools->getBNL (p));

  if (act_mode == true) {
    // we need to dump a list of declarations for each net

    fprintf (_outfp, " /* nets for ");
    if (prefix) {
      prefix->Print (_outfp);
    }
    else {
      fprintf (_outfp, " -toplevel-");
    }
    fprintf (_outfp, "*/\n");
    
    act_boolean_netlist_t *bnl = bools->getBNL (p);
    for (int i=0; i < A_LEN (bnl->nets); i++) {
      char buf[10240];
      if (bnl->nets[i].port) continue;
      if (bnl->nets[i].skip) continue;
      if (A_LEN (bnl->nets[i].pins) == 0) continue;

      fprintf (_outfp, " bool ");
      if (prefix) {
	prefix->sPrint (buf, 10240);
	a->mfprintf (_outfp, "%s.", buf);
      }
      ActId *tmp = bnl->nets[i].net->toid();
      tmp->sPrint (buf, 10240);
      a->mfprintf (_outfp, "%s", buf);
      fprintf (_outfp, ";\n");
      delete tmp;
    }
  }

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
	    _print_flat_cell (bnl, stk, prefix, iproc, act_mode);
	  }
	  else {
	    _printflat (prefix, ntl, stk, iproc, act_mode);
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
	_print_flat_cell (bnl, stk, prefix, iproc, act_mode);
      }
      else {
	_printflat (prefix, ntl, stk, iproc, act_mode);
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

void ActNetlistPass::_updateInvHash (Process *p)
{
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


void ActNetlistPass::flatHelper (Process *p)
{
  if (!p) return;
  if (p->isCell()) {
    emitNetlist (p);
  }
  else {
    _updateInvHash (p);
  }
}

static void _printnsact (FILE *fp, ActNamespace *ns, int *count)
{
  list_t *l;

  if (ns == ActNamespace::Global()) {
    *count = 0;
    return;
  }
  
  l = list_new ();
  while (ns && ns != ActNamespace::Global()) {
    stack_push (l, ns);
    ns = ns->Parent ();
  }
  *count = 0;
  while (!stack_isempty (l)) {
    ns = (ActNamespace *) stack_pop (l);
    if (*count > 0) {
      fprintf (fp, "export ");
    }
    fprintf (fp, "namespace %s { ", ns->getName());
    *count = *count + 1;
  }
  fprintf (fp, "\n");
  list_free (l);
}

void ActNetlistPass::flatActHelper (Process *p)
{
  if (!p) return;
  if (p->isCell()) {
    char buf[10240];
    if (p->getns() != _curflatns) {
      if (_curflatns) {
	for (int i=0; i < _curnsnest; i++) {
	  fprintf (_outfp, "}");
	}
	fprintf (_outfp, "\n\n");
      }
      _printnsact (_outfp, p->getns(), &_curnsnest);
    }
    _curflatns = p->getns();

    // we have to print the header differently
    if (p->IsExported()) {
      fprintf (_outfp, "export ");
    }
    fprintf (_outfp, "defcell ");
    a->mfprintfproc (_outfp, p, true /* omit namespace */);
    fprintf (_outfp, " (");
    if (!mangled_ports_actflat) {
      for (int i=0; i < p->getNumPorts (); i++) {
	InstType *it = p->getPortType (i);
	Array *a = it->arrayInfo();
	it->clrArray();
	it->Print (_outfp);
	fprintf (_outfp, " %s", p->getPortName (i));
	if (a) {
	  a->Print (_outfp);
	}
	it->MkArray (a);
	if (i != p->getNumPorts()-1) {
	  fprintf (_outfp, "; ");
	}
      }
      fprintf (_outfp, ")\n{\n");
      p->CurScope()->Print (_outfp, true /* print all instances */);
      if (p->getlang()) {
	p->getlang()->Print (_outfp);
      }
      fprintf (_outfp, "}\n\n");
    }
    else {
      act_boolean_netlist_t *bnl = bools->getBNL (p);
      Assert (bnl, "What?");
      int need_comma = 0;
      for (int i=0; i < A_LEN (bnl->ports); i++) {
	if (bnl->ports[i].omit) continue;
	if (need_comma) {
	  fprintf (_outfp, ";");
	}
	fprintf (_outfp, "bool ");
	ActId *id = bnl->ports[i].c->toid();
	id->sPrint (buf, 10240);
	a->mfprintf (_outfp, "%s", buf);
	delete id;
	need_comma = 1;
      }
      fprintf (_outfp, ")\n{\n");
      // now connect this up
      for (int i=0; i < p->getNumPorts(); i++) {
	InstType *it = p->getPortType (i);
	Array *a = it->arrayInfo();
	if (TypeFactory::isBoolType (it) && !a) continue;
	it->clrArray ();
	fprintf (_outfp, " ");
	it->Print (_outfp, 1);
	fprintf (_outfp, " %s", p->getPortName (i));
	if (a) {
	  a->Print (_outfp);
	  it->MkArray (a);
	}
	fprintf (_outfp, ";\n");
      }
      for (int i=0; i < A_LEN (bnl->ports); i++) {
	if (bnl->ports[i].omit) continue;
	fprintf (_outfp, " ");
	ActId *id = bnl->ports[i].c->toid();
	id->sPrint (buf, 10240);
	fprintf (_outfp, "%s=", buf);
	a->mfprintf (_outfp, "%s", buf);
	fprintf (_outfp, ";\n");
      }
      p->CurScope()->Print (_outfp);
      if (p->getlang()) {
	p->getlang()->Print (_outfp);
      }
      fprintf (_outfp, "}\n");
    }
  }
  else {
    _updateInvHash (p);
  }
}


void ActNetlistPass::printActFlat (FILE *fp)
{
  if (!completed()) {
    fatal_error ("ActNetlistPass::printActFlat() called before pass is run!");
  }
  if (!_root) {
    fatal_error ("ActNetlistPass::printActFlat() requires a top-level process!");
  }
  if (_root->isCell()) {
    fatal_error ("ActNetlistPass::printActFlat() must be called on a process, not a cell!");
  }
  
  bools->createNets (_root);
  _outfp = fp;
  _invNetH = phash_new (4);
  _curflatns = ActNamespace::Global ();
  _curnsnest = 0;

  // Run a new pass that only prints cells and
  // computes the inverse hash that we need to accelerate net
  // identification.
  run_recursive (_root, 3);

  if (_curflatns) {
    for (int i=0; i < _curnsnest; i++) {
      fprintf (_outfp, "}");
    }
    fprintf (_outfp, "\n\n");
  }

  act_boolean_netlist_t *bnl = bools->getBNL (_root);
  fprintf (_outfp, "defproc ");
  a->mfprintfproc (_outfp, _root);
  fprintf (_outfp, " (");

  if (!mangled_ports_actflat) {
    for (int k=0; k < _root->getNumPorts(); k++) {
      if (k > 0) {
	fprintf (_outfp, "; ");
      }
      _root->getPortType (k)->Print (_outfp);
      fprintf (_outfp, " %s", _root->getPortName (k));
    }
  }
  else {
    Assert (bnl, "What?");
    int need_comma = 0;
    char buf[10240];
    for (int i=0; i < A_LEN (bnl->ports); i++) {
      if (bnl->ports[i].omit) continue;
      if (need_comma) {
	fprintf (_outfp, "; ");
      }
      fprintf (_outfp, "bool ");
      ActId *id = bnl->ports[i].c->toid();
      id->sPrint (buf, 10240);
      a->mfprintf (_outfp, "%s", buf);
      delete id;
      need_comma = 1;
    }
  }
  fprintf (_outfp, ")\n{\n");

  /* XXX: internal port list connections */
  if (!mangled_ports_actflat) {
    A_DECL (act_connection *, emitted);
    A_INIT (emitted);
    for (int i=0; i < A_LEN (bnl->ports); i++) {
      if (bnl->ports[i].omit) {
	bool found = false;
	for (int j=0; !found && j < A_LEN (emitted); j++) {
	  if (bnl->ports[i].c == emitted[j]) {
	    found = true;
	  }
	}
	if (!found) {
	  A_NEW (emitted, act_connection *);
	  A_NEXT (emitted) = bnl->ports[i].c;
	  A_INC (emitted);
	  // print connections for c!
	  if (A_LEN (emitted) == 1) {
	    fprintf (_outfp, " /* port internal connections */\n");
	  }
	  fprintf (_outfp, " ");
	  ActConniter ci(bnl->ports[i].c);
	  ActId *idfirst;
	  int first = 1;
	  for (ci = ci.begin(); ci != ci.end(); ci++) {
	    act_connection *c = *ci;
	    ActId *tmpid = c->toid();
	    if (first) {
	      idfirst = tmpid;
	      first = 0;
	    }
	    else {
	      if (_root->FindPort (tmpid->getName())) {
		idfirst->Print (_outfp);
		fprintf (_outfp, "=");
		tmpid->Print (_outfp);
		fprintf (_outfp, "; ");
	      }
	      delete tmpid;
	    }
	  }
	  if (idfirst) {
	    delete idfirst;
	  }
	  fprintf (_outfp, "\n");
	}
      }
    }
    A_FREE (emitted);
  }

  /* XXX: we need to declare all the nets used! */
  

  list_t *stk = list_new ();
  _printflat (NULL, NULL, stk, _root, true);
  list_free (stk);

  fprintf (_outfp, "}\n");

  _outfp = NULL;

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
