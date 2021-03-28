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


void ActNetlistPass::emit_netlist (Process *p, FILE *fp)
{
  Assert (p->isExpanded(), "Process must be expanded!");

  netlist_t *n = getNL (p);
  if (!n) {
    fprintf (stderr, "Could not find process `%s'", p->getName());
    fprintf (stderr, " in created netlists; inconstency!\n");
    fatal_error ("Internal inconsistency or error in pass ordering!");
  }

  if (n->bN->visited) return;
  n->bN->visited = 1;

  if (n->bN->p->isBlackBox() && black_box_mode) return;

  ActInstiter i(p->CurScope());
  
  if (!top_level_only) {
    /* emit sub-processes */

    /* handle all processes instantiated by this one */
    for (i = i.begin(); i != i.end(); i++) {
      ValueIdx *vx = *i;
      if (TypeFactory::isProcessType (vx->t)) {
	emit_netlist (dynamic_cast<Process *>(vx->t->BaseType()), fp);
      }
    }
  }

  fprintf (fp, "*\n");
  if (p->getns() && p->getns() != ActNamespace::Global()) {
    char *tmp = p->getns()->Name();
    fprintf (fp, "*---- act defproc: %s::%s -----\n", tmp, p->getName());
    FREE (tmp);
  }
  else {
    fprintf (fp, "*---- act defproc: %s -----\n", p->getName());
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
  fprintf (fp, ".subckt ");
  /* special mangling for processes that only end in <> */
  a->mfprintfproc (fp, p);
  int out = 0;
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
      emit_node (n, fp, x, 1);
    }
    fprintf (fp, "\n");
  }

  if (!list_isempty (n->gnd_list)) {
    fprintf (fp, "*.POWER GND");
    for (vi = list_first (n->gnd_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      emit_node (n, fp, x, 1);
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
      emit_node (n, fp, x, 1);
    }
    fprintf (fp, "\n");
  }

  if (!list_isempty (n->nsc_list)) {
    fprintf (fp, "*.POWER PSUB");
    for (vi = list_first (n->nsc_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      emit_node (n, fp, x, 1);
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
    if (x->v->stateholding) {
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

  for (x = n->hd; x; x = x->next) {
    listitem_t *li;

    /* emit node cap, if any */
    if ((x->cap > 0) && !ignore_loadcap) {
      fprintf (fp, "C_per_node_%d ", ncaps++);
      emit_node (n, fp, x, 1);
      fprintf (fp, " ");
      emit_node (n, fp, n->GND, 1);
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
	l = discrete_len;
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

	  if (width_repeat > 1) {
	    w = EDGE_WIDTH (e, iw);
	  }
	  else {
	    w = e->w;
	  }
	
	  if (use_subckt_models) {
	    fprintf (fp, "x");
	  }

	  fprintf (fp, "M%d", fets);
	  if (len_repeat > 1) {
	    fprintf (fp, "_%d", il);
	  }
	  if (width_repeat > 1) {
	    fprintf (fp, "_%d", iw);
	  }

	  /* name of the instance includes how the fet was generated in it */
	  if (e->pchg) {
	    fprintf (fp, "_pchg ");
	  }
	  else if (e->combf) {
	    fprintf (fp, "_ckeeper ");
	  }
	  else if (e->keeper) {
	    fprintf (fp, "_keeper ");
	  }
	  else if (e->raw) {
	    fprintf (fp, "_pass ");
	  }
	  else {
	    fprintf (fp, "_ ");
	  }

	  /* if length repeat, source/drain changes */
	  if (il == 0) {
	    emit_node (n, fp, src, 1);
	  }
	  else {
	    fprintf (fp, "#l%d", repnodes);
	  }
	  fprintf (fp, " ");
	  emit_node (n, fp, e->g, 1);
	  fprintf (fp, " ");

	  if (il == len_repeat-1) {
	    emit_node (n, fp, drain, 1);
	  }
	  else {
	    fprintf (fp, "#l%d", repnodes+1);
	  }
	  if (len_repeat > 1 && il != len_repeat-1) {
	    repnodes++;
	  }

	  fprintf (fp, " ");
	  emit_node (n, fp, e->bulk, 1);

	  sprintf (devname, "net.%cfet_%s", (e->type == EDGE_NFET ? 'n' : 'p'),
		   act_dev_value_to_string (e->flavor));
	  if (!config_exists (devname)) {
	    act_error_ctxt (stderr);
	    fatal_error ("Device mapping for `%s' not defined in technology file.", devname);
	  }
	  fprintf (fp, " %s", config_get_string (devname));
	  fprintf (fp, " W=%gU L=%gU", w*manufacturing_grid*1e6,
		   (l*manufacturing_grid + leak*leak_adjust)*1e6);

	  /* print extra fet string */
	  if (extra_fet_string && strcmp (extra_fet_string, "") != 0) {
	    fprintf (fp, " %s\n", extra_fet_string);
	  }
	  else {
	    fprintf (fp, "\n");
	  }
      
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
	    fprintf (fp, "+ AS=%gP PS=%gU",
		     (e->w/getGridsPerLambda()*gap)*(lambda*lambda)*1e12,
		     2*(e->w/getGridsPerLambda() + gap)*lambda*1e6);

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
	    fprintf (fp, " AD=%gP PD=%gU\n",
		     (e->w/getGridsPerLambda()*gap)*(lambda*lambda)*1e12,
		     2*(e->w/getGridsPerLambda() + gap)*lambda*1e6);
	  }

	  fflush (fp);

	  if (e->type == EDGE_NFET) {
	    if (max_n_w_in_lambda != 0 && e->w > max_n_w_in_lambda*getGridsPerLambda()) {
	      act_error_ctxt (stderr);
	      fatal_error ("Device #%d: pfet width (%d) exceeds maximum limit (%d)\n", fets-1, e->w/getGridsPerLambda(), max_n_w_in_lambda);
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

  /* clear visited flag */
  for (x = n->hd; x; x = x->next) {
    listitem_t *li;
    for (li = list_first (x->e); li; li = list_next (li)) {
      edge_t *e = (edge_t *)list_value (li);
      e->visited = 0;
    }
  }
  
  /*-- emit instances --*/
  int iport = 0;
  int iweak = 0;
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      netlist_t *sub;
      Process *instproc = dynamic_cast<Process *>(vx->t->BaseType ());
      int ports_exist;
      sub = netmap->find (instproc)->second;
      
      ports_exist = 0;
      for (int i=0; i < A_LEN (sub->bN->ports); i++) {
	if (sub->bN->ports[i].omit == 0) {
	  ports_exist = 1;
	  break;
	}
      }

      if (ports_exist) {
	if (vx->t->arrayInfo()) {
	  Arraystep *as = vx->t->arrayInfo()->stepper();
	  while (!as->isend()) {
	    char *str = as->string();
	    a->mfprintf (fp, "x%s%s", vx->getName(), str);
	    FREE (str);
	    for (int i=0; i < A_LEN (sub->bN->ports); i++) {
	      ActId *id;
	      char buf[10240];
	      if (sub->bN->ports[i].omit) continue;

	      Assert (iport < A_LEN (n->bN->instports), "Hmm");
	      id = n->bN->instports[iport]->toid();
	      fprintf (fp, " ");
	      id->sPrint (buf, 10240);
	      a->mfprintf (fp, "%s", buf);
	      delete id;
	      iport++;
	    }

	    if (sub->weak_supply_vdd > 0) {
	      Assert (iweak < A_LEN (n->instport_weak), "What?");
	      fprintf (fp, " #%d", n->instport_weak[iweak++]);
	    }
	    if (sub->weak_supply_gnd > 0) {
	      Assert (iweak < A_LEN (n->instport_weak), "What?");
	      fprintf (fp, " #%d", n->instport_weak[iweak++]);
	    }
	    
	    a->mfprintf (fp, " ");
	    a->mfprintfproc (fp, instproc);
	    a->mfprintf (fp, "\n");
	    as->step();
	  }
	  delete as;
	}
	else {
	  a->mfprintf (fp, "x%s", vx->getName ());
	  for (int i =0; i < A_LEN (sub->bN->ports); i++) {
	    ActId *id;
	    char buf[10240];
	    if (sub->bN->ports[i].omit) continue;
	  
	    Assert (iport < A_LEN (n->bN->instports), "Hmm");
	    id = n->bN->instports[iport]->toid();
	    fprintf (fp, " ");
	    id->sPrint (buf, 10240);
	    a->mfprintf (fp, "%s", buf);
	    delete id;
	    iport++;
	  }

	  if (sub->weak_supply_vdd > 0) {
	    Assert (iweak < A_LEN (n->instport_weak), "What?");
	    fprintf (fp, " #%d", n->instport_weak[iweak++]);
	  }
	  if (sub->weak_supply_gnd > 0) {
	    Assert (iweak < A_LEN (n->instport_weak), "What?");
	    fprintf (fp, " #%d", n->instport_weak[iweak++]);
	  }

	  a->mfprintf (fp, " ");
	  a->mfprintfproc (fp, instproc);
	  a->mfprintf (fp, "\n");
	}
      }
    }
  }
  Assert (iport == A_LEN (n->bN->instports), "Hmm...");
  Assert (iweak == A_LEN (n->instport_weak), "Hmm...");
  
  fprintf (fp, ".ends\n");
  fprintf (fp, "*---- end of process: %s -----\n", p->getName());

  return;
}


void ActNetlistPass::Print (FILE *fp, Process *p)
{
  Assert (p, "act_emit_netlist() requires a non-NULL process!");
  Assert (p->isExpanded (), "Process must be expanded!");

  if (!completed()) {
    fatal_error ("ActNetlistPass::Print() called before pass is run!");
  }
  
  emit_netlist (p, fp);

  /*--- clear visited flag ---*/
  std::map<Process *, netlist_t *>::iterator it;
  for (it = netmap->begin(); it != netmap->end(); it++) {
    netlist_t *n = it->second;
    Assert (n->bN, "Hmm...");
    n->bN->visited = 0;
  }
}
