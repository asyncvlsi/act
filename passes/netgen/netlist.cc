/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2015, 2017, 2018-2019 Rajit Manohar
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
#include <math.h>
#include <string.h>
#include "netlist.h"
#include <common/hash.h>
#include <common/qops.h>
#include <common/bitset.h>
#include <common/config.h>
#include <act/iter.h>
#include <act/passes/sizing.h>

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* for maximum acyclic path algorithm */
typedef struct {
  bitset_t *b; 			/* list of nodes visited
				   (excluding power supply) */
  double reff;			/* reff through this path */
} path_t;

const char *ActNetlistPass::global_vdd = NULL;
const char *ActNetlistPass::global_gnd = NULL;
const char *ActNetlistPass::local_vdd = NULL;
const char *ActNetlistPass::local_gnd = NULL;
Act *ActNetlistPass::current_act = NULL;
ActDynamicPass *ActNetlistPass::current_annotate = NULL;
int ActNetlistPass::grids_per_lambda = 0;


#define VINF(x) ((struct act_nl_varinfo *)((x)->extra))

/*-- compute the maximum acyclic path from power supply to each node --*/
/* (either p-type or n-type) */
static void compute_max_reff (netlist_t *N, int type)
{
  list_t *l;
  listitem_t *li, *mi;

  l = list_new ();

  if (type == EDGE_NFET) {
    li = list_first (N->gnd_list);
  }
  else {
    li = list_first (N->vdd_list);
  }
  while (li) {
    list_append (l, list_value (li));
    li = list_next (li);
  }

  /* seed power supply with reff = 0 */
  path_t *p;

  for (li = list_first (l); li; li = list_next (li)) {
    node_t *tmp;
    NEW (p, path_t);
    p->b = NULL;
    p->reff = 0;
    tmp = (node_t *)list_value (li);
    list_append (tmp->wl, p);
  }

  /* worklist algorithm */
  while (!list_isempty (l)) {
    node_t *n = (node_t *)list_delete_tail (l);

    /* look at all edges of the correct type: at this point, there
       are no keepers! */
    for (li = list_first (n->e); li; li = list_next (li)) {
      edge_t *e;
      node_t *m;

      e = (edge_t *)list_value (li);
      
      if (e->type != type) continue;
      if (e->pchg) continue;
      if (e->raw) continue;
      
      if (e->a == n) {
	m = e->b;
      }
      else {
	m = e->a;
      }
      Assert (m != n, "Why is there an edge to myself?!");

      if (m->supply) continue;

      double r;
      int changed = 0;

      r = (double)e->l/(double)e->w;

      /* propagate paths from n to m */
      for (mi = list_first (n->wl); mi; mi = list_next (mi)) {
	path_t *pnew;

	p = (path_t *)list_value (mi);
	
	if (p->b && bitset_tst (p->b, m->i)) {
	  /* would create a cycle */
	  continue;
	}

	NEW (pnew, path_t);
	if (!p->b) {
	  pnew->b = bitset_new (N->idnum);
	}
	else {
	  pnew->b = bitset_copy (p->b);
	}
	bitset_set (pnew->b, m->i);
	pnew->reff = r + p->reff;

	/* merge this path into m's worklist */
	listitem_t *wli;
	for (wli = list_first (m->wl); wli; wli = list_next (wli)) {
	  path_t *chk;
	  chk = (path_t *)list_value (wli);
	  if (!chk->b) continue;
	  if (bitset_equal (chk->b, pnew->b)) {
	    if (pnew->reff > chk->reff) {
	      chk->reff = pnew->reff;
	      changed = 1;
	    }
	    else {
	      /* pruned! */
	      bitset_free (pnew->b);
	      FREE (pnew);
	      pnew = NULL;
	      break;
	    }
	  }
	}
	if (!wli) {
	  list_append (m->wl, pnew);
	  changed = 1;
	}
      }
      if (changed) {
	list_append (l, m);
      }
    }
  }
  list_free (l);

  /* for each variable v, compute reff as the max of all paths */
  ihash_bucket_t *b;
  ihash_iter_t iter;
  ihash_iter_init (N->bN->cH, &iter);
  while ((b = ihash_iter_next (N->bN->cH, &iter))) {
    act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;

    /* some variables are only there for connecting to sub-circuits */
    if (!VINF(v)) continue;
      
    if (VINF(v)->n->reff_set[type]) {
      /* over-ridden by attributes */
      continue;
    }
    VINF(v)->n->reff[type] = -1;
    for (li = list_first (VINF(v)->n->wl); li; li = list_next (li)) {
      p = (path_t *) list_value (li);
      VINF(v)->n->reff[type] = MAX(VINF(v)->n->reff[type], p->reff);
    }
  }

  /* release storage */
  node_t *n;
  for (n = N->hd; n; n = n->next) {
    while (!list_isempty (n->wl)) {
      p = (path_t *)list_delete_tail (n->wl);
      if (p->b) {
	bitset_free (p->b);
      }
      FREE (p);
    }
  }
}


/*-- at hash table in netlist_t returns one of these --*/
struct at_lookup {
  node_t *n;
  act_prs_expr_t *e;
  int max_depth;
};

static at_lookup *at_alloc (void)
{
  at_lookup *a;
  NEW (a, at_lookup);
  a->n = NULL;
  a->e = NULL;
  a->max_depth = 0;
  return a;
}

static node_t *node_alloc (netlist_t *n, struct act_nl_varinfo *vi)
{
  node_t *x;

  NEW (x, node_t);
  x->i = n->idnum++;
  x->v = vi;

  x->e = list_new ();
  x->b = bool_false (n->B);

  x->contact = 0;
  x->supply = 0;
  x->inv = 0;
  x->visited = 0;

  x->reff[0] = -1;
  x->reff[1] = -1;

  x->wl = list_new ();

  x->reff_set[0] = 0;
  x->reff_set[1] = 0;
  x->cap = 0;
  x->resis = 0;
  x->next = NULL;

  q_ins (n->hd, n->tl, x);

  return x;
}

static void *varinfo_alloc (netlist_t *n, act_booleanized_var_t *v)
{
  struct act_nl_varinfo *vi;
  
  NEW (vi, struct act_nl_varinfo);
  vi->e_up = NULL;
  vi->e_dn = NULL;
  vi->b = bool_newvar (n->B);
  vi->up = bool_false (n->B);
  vi->dn = bool_false (n->B);

  vi->n = node_alloc (n, vi);

  vi->vdd = NULL;
  vi->gnd = NULL;
  
  vi->spec_keeper = 0;
  vi->unstaticized = 0;
  vi->stateholding = 0;
  vi->usecf = 2;
  vi->manualkeeper = 0;
  vi->inv = NULL;
  vi->extra = NULL;

  vi->v = v;

  return vi;
}

static act_booleanized_var_t *var_lookup (netlist_t *n, act_connection *c)
{
  phash_bucket_t *b;
  act_booleanized_var_t *v;

  if (!c) return NULL;

  c = c->primary();

  b = phash_lookup (n->bN->cH, c);
  Assert (b, "What?!");
  v = (act_booleanized_var_t *) b->v;
  if (!v->extra) {
    v->extra = varinfo_alloc (n, v);
  }
  return v;
}

static act_booleanized_var_t *var_lookup (netlist_t *n, ActId *id)
{
  act_connection *c;

  c = id->Canonical (n->bN->cur);
  return var_lookup (n, c);
}


static node_t *node_lookup (netlist_t *n, act_connection *c)
{
  phash_bucket_t *b;
  
  if (!c) return NULL;
  c = c->primary();
  b = phash_lookup (n->bN->cH, c);
  if (b) {
    return VINF(((act_booleanized_var_t *)b->v))->n;
  }
  else {
    return NULL;
  }
}

static edge_t *edge_alloc (netlist_t *n, node_t *gate,
			   node_t *a, node_t *b, node_t *bulk)
{
  edge_t *e;

  NEW (e, edge_t);
  e->g = gate;
  e->a = a;
  e->b = b;
  e->bulk = bulk;

  if (e->g->v) {
    Assert (e->g->v->v->used == 1, "What?");
  }
  if (e->a->v) {
    Assert (e->a->v->v->used == 1, "What?");
  }
  if (e->b->v) {
    Assert (e->b->v->v->used == 1, "What?");
  }
  if (e->bulk->v) {
    e->bulk->v->v->used = 1;
  }

  e->w = -1;
  e->l = -1;
  e->flavor = 0; /* default fet type */

  e->type = 0;
  
  e->pchg = 0;
  e->keeper = 0;
  e->combf = 0;
  e->raw = 0;

  e->nfolds = 1;  /* default, 1 fold */
  e->nlen = 1;
  e->visited = 0;
  e->pruned = 0;
  e->tree = 0;

  list_append (a->e, e);
  list_append (b->e, e);

  return e;
}
  
static edge_t *edge_alloc (netlist_t *n, ActId *id, node_t *a, node_t *b,
			   node_t *bulk)
{
  edge_t *e;
  act_connection *c;
  act_booleanized_var_t *v;

  c = id->Canonical (n->bN->cur);
  Assert (c, "This is weird");
  Assert (c == c->primary(), "This is weird");

  v = var_lookup (n, c);

  return edge_alloc (n, VINF(v)->n, a, b, bulk);
}
  
  

static void append_node_to_list (list_t *x, node_t *n)
{
  listitem_t *li;

  for (li = list_first (x); li; li = list_next (li)) {
    if (list_value (li) == n) return;
  }
  list_append (x, n);
}

static node_t *search_supply_list_for_null (list_t *x)
{
  listitem_t *li;
  node_t *n;

  for (li = list_first (x); li; li = list_next (li)) {
    n = (node_t *) list_value (li);
    if (!n->v) {
      return n;
    }
  }
  return NULL;
}


#define EDGE_TYPE(t) ((t) & (0x1 << 0))
#define EDGE_SIZE(t) ((t) & (0x7 << 1))
#define EDGE_MODE(t) ((t) & (0xf << 4))

/* these three size options are exclusive */
#define EDGE_NORMAL   (0x01 << 1)
#define EDGE_FEEDBACK (0x02 << 1)
#define EDGE_STATINV  (0x03 << 1)
#define EDGE_HALFNORM (0x04 << 1)
#define EDGE_HALFNORMREV (0x05 << 1)

/* flags */
#define EDGE_TREE     (0x01 << 4) // tree edges -- mark specially
#define EDGE_PCHG     (0x02 << 4) // pchg edges: can use n to vdd, p to gnd
#define EDGE_INVERT   (0x04 << 4) // strip pchg
#define EDGE_CELEM    (0x08 << 4) // strip pchg
#define EDGE_KEEPER   (0x10 << 4) // keeper edge (force)
#define EDGE_CKEEPER  (0x20 << 4) // ckeeper edge (force)

int ActNetlistPass::find_length_fit (int len)
{
  for (int i=0; i < discrete_fet_length_sz/2; i++) {
    if (discrete_fet_length[2*i]*getGridsPerLambda() <= len &&
	len <= discrete_fet_length[2*i+1]*getGridsPerLambda()) {
      /* we are fine */
      return len;
    }
    if (i != (discrete_fet_length_sz/2-1)) {
      if (len < discrete_fet_length[2*i+2]*getGridsPerLambda()) {
	/* length is between current window and next window */
	return discrete_fet_length[2*i+1]*getGridsPerLambda();
      }
    }
  }
  return discrete_fet_length[discrete_fet_length_sz-1]*getGridsPerLambda();
}

int ActNetlistPass::find_length_window (edge_t *e)
{
  int i;
  int last = -1;
  for (i=0; i < discrete_fet_length_sz/2; i++) {
    if (discrete_fet_length[2*i]*getGridsPerLambda() <= e->l &&
	e->l <= discrete_fet_length[2*i+1]*getGridsPerLambda()) {
      /* we are fine */
      return -1;
    }
    if (i != (discrete_fet_length_sz/2-1)) {
      if (e->l < discrete_fet_length[2*i+2]*getGridsPerLambda()) {
	/* length is between current window and next window */
	return 2*i;
	break;
      }
    }
  }
  return discrete_fet_length_sz - 2;
}

void ActNetlistPass::fold_transistors (netlist_t *N)
{
  node_t *n;
  listitem_t *li;
  edge_t *e;
  int fold;

  if (n_fold == 0 && p_fold == 0 && discrete_len == 0 &&
      discrete_fet_length_sz == 0) return;

  for (n = N->hd; n; n = n->next) {
    for (li = list_first (n->e); li; li = list_next (li)) {
      e = (edge_t *) list_value (li);
      if (e->visited > 0) continue;
      e->visited = 1;
      if (e->type == EDGE_NFET) {
	fold = n_fold*getGridsPerLambda();
      }
      else {
	fold = p_fold*getGridsPerLambda();
      }
      if (fold > 0 && (e->w/e->nfolds) > fold) {
	e->nfolds = e->w/fold;
	if ((e->w % fold) >= min_w_in_lambda) {
	  e->nfolds++;
	}
      }

      if (discrete_len > 0) {
	e->nlen = (e->l + getGridsPerLambda()*discrete_len - 1)/(getGridsPerLambda()*discrete_len);
      }
      else if (discrete_fet_length_sz > 0) {
	int last = find_length_window (e);
	if (last != -1) {
	  /* closest smaller window is [last, last+1]; we assume the
	     residual width can be handled with one transistor */
	  e->nlen = (e->l + discrete_fet_length[last+1]*getGridsPerLambda() - 1)/(discrete_fet_length[last+1]*getGridsPerLambda());
	  Assert (e->nlen > 1, "How is this possible?");
	}
      }
    }
  }

  /* clear visited flag */
  for (n = N->hd; n; n = n->next) {
    for (li = list_first (n->e); li; li = list_next (li)) {
      e = (edge_t *) list_value (li);
      e->visited = 0;
    }
  }
}


static void _alloc_weak_vdd (netlist_t *N, node_t *w, int min_w, int len)
{
  edge_t *e;

  Assert (w, "What?");
	  
  e = edge_alloc (N, N->GND, N->Vdd, w, N->nsc);
  e->type = EDGE_PFET;
  e->w = min_w*ActNetlistPass::getGridsPerLambda();
  e->l = len*ActNetlistPass::getGridsPerLambda();
  e->keeper = 1;
}

static void _alloc_weak_gnd (netlist_t *N, node_t *w, int min_w, int len)
{
  edge_t *e;

  Assert (w, "What?");
	  
  e = edge_alloc (N, N->Vdd, N->GND, w, N->psc);
  e->type = EDGE_NFET;
  e->w = min_w*ActNetlistPass::getGridsPerLambda();
  e->l = len*ActNetlistPass::getGridsPerLambda();
  e->keeper = 1;
}


void ActNetlistPass::generate_staticizers (netlist_t *N,
					   int num_vdd_share,
					   int num_gnd_share,
					   int vdd_len,
					   int gnd_len,
					   node_t *weak_vdd,
					   node_t *weak_gnd)
{
  node_t *n;
  
  for (n = N->hd; n; n = n->next) {
    if (!n->v) continue;
    if (n->supply) continue;

    if (n->v->up == bool_false (N->B) && n->v->dn == bool_false (N->B)) {
      n->v->v->input = 1;
      continue;
    }

    if (n->v->unstaticized == 2) {
      continue;
    }

    bool_t *v = bool_or (N->B, n->v->up, n->v->dn);
    if (v == bool_true (N->B)) {
      /* combinational */
      if (n->v->up == bool_var (N->B, bool_topvar (n->v->up)) ||
	  n->v->dn == bool_var (N->B, bool_topvar (n->v->up))) {
	/*- this is an inverter -*/

	if (n->v->e_up && n->v->e_dn) {
	  if (n->v->e_up->type == ACT_PRS_EXPR_VAR) {
	    act_booleanized_var_t *inv = var_lookup (N, n->v->e_up->u.v.id);
	    if (bool_var (N->B, bool_topvar (n->v->up)) == VINF(inv)->b) {
	      VINF(inv)->inv = n;
	    }
	    else {
	      act_error_ctxt (stderr);
	      fatal_error ("Complex inverter?");
	    }
	  }
	  else if (n->v->e_dn->type == ACT_PRS_EXPR_VAR) {
	    act_booleanized_var_t *inv = var_lookup (N, n->v->e_dn->u.v.id);
	    if (bool_var (N->B, bool_topvar (n->v->up)) == VINF(inv)->b) {
	      VINF(inv)->inv = n;
	    }
	    else {
	      act_error_ctxt (stderr);
	      fatal_error ("Complex inverter?");
	    }
	  }
	}
      }
      continue;
    }
    else {
      n->v->stateholding = 1;
    }
  }

  if (config_get_int ("net.disable_keepers") == 1) return;

  int cf_keepers = 0;
  if (config_exists ("net.comb_feedback")) {
    cf_keepers = config_get_int ("net.comb_feedback");
  }

  for (n = N->hd; n; n = n->next) {
    if (!n->v) continue;
    if (n->v->stateholding && !n->v->unstaticized) {
      /* state-holding node */
      if (!n->v->inv) {
	node_t *iout; /* inverter output */
	iout = node_alloc (N, NULL);
	iout->inv = 1;
	Assert (n->v->v->used == 1, "Hmm");

	edge_t *e_inv;

	e_inv = edge_alloc (N, n, N->Vdd, iout, N->nsc);
	e_inv->type = EDGE_PFET;
	set_fet_params (N, e_inv, EDGE_STATINV|EDGE_PFET, NULL);

	e_inv = edge_alloc (N, n, N->GND, iout, N->psc);
	e_inv->type = EDGE_NFET;
	set_fet_params (N, e_inv, EDGE_STATINV|EDGE_NFET, NULL);
	n->v->inv = iout;
      }

      /*-- node has forward inverter --*/

      if (n->v->spec_keeper) {
	/* special H-tree structure */
	node_t *node[2][2];
	int cnt[2];
	listitem_t *li;
	edge_t *e;
	node[0][0] = NULL;
	node[0][1] = NULL;
	node[1][0] = NULL;
	node[1][1] = NULL;
	cnt[0] = 0;
	cnt[1] = 0;

	for (li = list_first (n->e); li; li = list_next (li)) {
	  e = (edge_t *) list_value (li);
	  if (cnt[e->type] == 2) {
	    fatal_error ("Error: invalid node used for h-type keeper!");
	  }
	  if (e->a == n) {
	    node[e->type][cnt[e->type]] = e->b;
	  }
	  else {
	    node[e->type][cnt[e->type]] = e->a;
	  }
	  cnt[e->type]++;
	}
	if (cnt[0] != 2 || cnt[1] != 2) {
	  fatal_error ("Error: invalid node used for h-type keeper!");
	}
	e = edge_alloc (N, n->v->inv, node[EDGE_NFET][0], node[EDGE_NFET][1],
			N->psc);
	e->type = EDGE_NFET;
	e->keeper = 1;
	e->w = min_w_in_lambda*getGridsPerLambda();
	e->l = min_l_in_lambda*getGridsPerLambda();
	
	e = edge_alloc (N, n->v->inv, node[EDGE_PFET][0], node[EDGE_PFET][1],
			N->nsc);
	e->type = EDGE_PFET;
	e->keeper = 1;
	e->w = min_w_in_lambda*getGridsPerLambda();
	e->l = min_l_in_lambda*getGridsPerLambda();
      }
      else {
	if ((n->v->usecf == 1) || (cf_keepers && (n->v->usecf == 2))) {
	  /* combinational feedback */
	  node_t *tmp;
	  edge_t *e;

	  /* pfets */
	  tmp = node_alloc (N, NULL);
	  e = edge_alloc (N, n->v->inv, tmp, n, N->nsc);
	  e->type = EDGE_PFET;
	  e->w = min_w_in_lambda*getGridsPerLambda();
	  e->l = min_l_in_lambda*getGridsPerLambda();
	  e->keeper = 1;
	  e->combf = 1;
	  create_expr_edges (N, EDGE_PFET|EDGE_FEEDBACK|EDGE_INVERT,
			     N->Vdd, n->v->e_dn, tmp, 1 /* invert */);

	  /* nfets */
	  tmp = node_alloc (N, NULL);
	  e = edge_alloc (N, n->v->inv, tmp, n, N->psc);
	  e->type = EDGE_NFET;
	  e->w = min_w_in_lambda*getGridsPerLambda();
	  e->l = min_l_in_lambda*getGridsPerLambda();
	  e->keeper = 1;
	  e->combf = 1;
	  create_expr_edges (N, EDGE_NFET|EDGE_FEEDBACK|EDGE_INVERT,
			     N->GND, n->v->e_up, tmp, 1);
	}
	else {
	  double r;
	  double rleft;
	  int len;
	  edge_t *e;
	  /* weak inverter */

	  /*-- p stack --*/
	  r = n->reff[EDGE_NFET]; // strength of n-stack
	  r /= weak_to_strong_ratio;
	  r /= p_n_ratio;

	  rleft = r - (double)min_l_in_lambda/(double)min_w_in_lambda;
	  if (rleft < 0) {
	    len = 0;
	  }
	  else {
	    len = (int) (0.5 + rleft*min_w_in_lambda);
	  }

	  /* two options:
	     - minimum size is weak enough, or 
	     series resistance is less than min length in which
	     case use slightly longer inverter
	      
	     - min size + series resistance
	  */
	  if ((double)min_l_in_lambda/(double)min_w_in_lambda >= r ||
	      (len < min_l_in_lambda)) {
	    e = edge_alloc (N, n->v->inv, N->Vdd, n, N->nsc);
	    e->type = EDGE_PFET;
	    e->w = min_w_in_lambda*getGridsPerLambda();
	    e->l = (min_l_in_lambda + len)*getGridsPerLambda();
	    e->keeper = 1;
	  }
	  else {
	    /* two edges */
	    /* residual length conforming to rules */

	    if (num_vdd_share == weak_share_max) {
	      _alloc_weak_vdd (N, weak_vdd, min_w_in_lambda, vdd_len);
	      weak_vdd = NULL;
	      num_vdd_share = 0;
	    }
	    if (!weak_vdd) {
	      weak_vdd = node_alloc (N, NULL);
	      num_vdd_share = 0;
	      vdd_len = 0;
	    }

	    num_vdd_share++;
	    vdd_len = MAX(vdd_len, len);

#if 0
	    /* lazy  emission of this resistor */
	    node_t *tmp;
	    tmp = node_alloc (N, NULL); // tmp node

	    /* resistor */
	    e = edge_alloc (N, N->GND, N->Vdd, tmp, N->nsc);
	    e->type = EDGE_PFET;
	    e->w = min_w_in_lambda*getGridsPerLambda();
	    e->l = len*getGridsPerLambda();
	    e->keeper = 1;
#endif	  

	    /* inv */
	    e = edge_alloc (N, n->v->inv, weak_vdd, n, N->nsc);
	    e->type = EDGE_PFET;
	    e->w = min_w_in_lambda*getGridsPerLambda();
	    e->l = min_l_in_lambda*getGridsPerLambda();
	    e->keeper = 1;
	  }

	  /*-- n stack --*/
	  r = n->reff[EDGE_PFET];
	  r /= weak_to_strong_ratio;
	  r *= p_n_ratio;
	  rleft = r - (double)min_l_in_lambda/(double)min_w_in_lambda;
	  if (rleft < 0) {
	    len = 0;
	  }
	  else {
	    len = (int) (0.5 + rleft*min_w_in_lambda);
	  }
	
	  if ((double)min_l_in_lambda/(double)min_w_in_lambda >= r ||
	      (len < min_l_in_lambda)) {
	    e = edge_alloc (N, n->v->inv, N->GND, n, N->psc);
	    e->type = EDGE_NFET;
	    e->w = min_w_in_lambda*getGridsPerLambda();
	    e->l = (min_l_in_lambda + len)*getGridsPerLambda();
	    e->keeper = 1;
	  }
	  else {
	    /* two edges */
	    /* residual length conforming to rules */

	    if (num_gnd_share == weak_share_max) {
	      _alloc_weak_gnd (N, weak_gnd, min_w_in_lambda, gnd_len);
	      weak_gnd = NULL;
	      num_gnd_share = 0;
	    }
	    if (!weak_gnd) {
	      weak_gnd = node_alloc (N, NULL);
	      gnd_len = 0;
	      num_gnd_share = 0;
	    }

	    num_gnd_share++;
	    gnd_len = MAX(gnd_len, len);

#if 0
	    node_t *tmp;

	    tmp = node_alloc (N, NULL); // tmp node

	    /* resistor */
	    e = edge_alloc (N, N->Vdd, N->GND, tmp, N->psc);
	    e->type = EDGE_NFET;
	    e->w = min_w_in_lambda*getGridsPerLambda();
	    e->l = len*getGridsPerLambda();
	    e->keeper = 1;
#endif	  

	    /* inv */
	    e = edge_alloc (N, n->v->inv, weak_gnd, n, N->psc);
	    e->type = EDGE_NFET;
	    e->w = min_w_in_lambda*getGridsPerLambda();
	    e->l = min_l_in_lambda*getGridsPerLambda();
	    e->keeper = 1;
	  }
	}
      }
    }
  }

  if (weak_vdd) {
    if (N->bN->p == _root) {
      /* flush it! */
      _alloc_weak_vdd (N, weak_vdd, min_w_in_lambda, vdd_len);
      N->weak_supply_vdd = 0;
    }
    else {
      if (num_vdd_share >= weak_share_min) {
	/* flush it! */
	_alloc_weak_vdd (N, weak_vdd, min_w_in_lambda, vdd_len);
	N->weak_supply_vdd = 0;
      }
      else {
	N->weak_supply_vdd = num_vdd_share;
	if (num_vdd_share > 0) {
	  N->vdd_len = vdd_len;
	  N->nid_wvdd = weak_vdd->i;
	}
      }
    }
  }
  if (weak_gnd) {
    if (N->bN->p == _root) {
      /* flush it! */
      _alloc_weak_gnd (N, weak_gnd, min_w_in_lambda, gnd_len);
      N->weak_supply_gnd = 0;
    }
    else {
      if (num_gnd_share >= weak_share_min) {
	/* flush it! */
	_alloc_weak_gnd (N, weak_gnd, min_w_in_lambda, gnd_len);
	N->weak_supply_gnd = 0;
      }
      else {
	N->weak_supply_gnd = num_gnd_share;
	if (num_gnd_share > 0) {
	  N->gnd_len = gnd_len;
	  N->nid_wgnd = weak_gnd->i;
	}
      }
    }
  }
}



static void check_supply (netlist_t *N,  ActId *id, int type, node_t *n)
{
  act_booleanized_var_t *v = var_lookup (N, id);
  if (type == EDGE_PFET) {
    if (!VINF(v)->vdd) {
      VINF(v)->vdd = n;
      return;
    }
    else if (VINF(v)->vdd == n) return;
  }
  else {
    if (!VINF(v)->gnd) {
      VINF(v)->gnd = n;
      return;
    }
    else if (VINF(v)->gnd == n) return;
  }
  fprintf (stderr, "Inconsistent power supply for signal `");
  id->Print (stderr);
  fprintf (stderr, "'\n");
  exit (1);
  return;
}

void ActNetlistPass::set_fet_params (netlist_t *n, edge_t *f, unsigned int type,
				     act_size_spec_t *sz)
{
  /* set edge flags */
  if (type & EDGE_PCHG) {
    f->pchg = 1;
  }
  if (EDGE_SIZE (type) == EDGE_FEEDBACK) {
    f->keeper = 1;
  }
  if ((EDGE_SIZE (type) == EDGE_FEEDBACK && (type & EDGE_INVERT)) || (type & EDGE_CKEEPER)) {
    f->combf = 1;
  }
  if (type & EDGE_TREE) {
    f->tree = 1;
  }
  if (EDGE_SIZE (type) == EDGE_NORMAL ||
      EDGE_SIZE (type) == EDGE_HALFNORMREV ||
      EDGE_SIZE (type) == EDGE_HALFNORM) {
    /* if type & EDGE_INVERT, could be => opp rule 
       if type & EDGE_CELEM, could be #> rule
    */
    if (sz) {
      f->flavor = sz->flavor;
      n->sz[f->type].flavor = f->flavor;

      if (sz->w) {
	f->w = (sz->w->type == E_INT ? sz->w->u.ival.v*getGridsPerLambda() :
		sz->w->u.f*getGridsPerLambda());
	n->sz[f->type].w = f->w;
      }
      else {
	f->w = n->sz[f->type].w;
      }
      if (sz->l) {
	f->l = (sz->l->type == E_INT ? sz->l->u.ival.v*getGridsPerLambda() :
		sz->l->u.f*getGridsPerLambda());
	n->sz[f->type].l = f->l;
      }
      else {
	f->l = n->sz[f->type].l;
      }
      if (sz->folds) {
	Assert (sz->folds->type == E_INT, "What?");
	f->nfolds = sz->folds->u.ival.v;
	if (f->nfolds < 1) {
	  f->nfolds = 1;
	}
	n->sz[f->type].nf = f->nfolds;
      }
    }
    else {
      f->w = n->sz[f->type].w;
      f->l = n->sz[f->type].l;
      f->nfolds = n->sz[f->type].nf;
      f->flavor = n->sz[f->type].flavor;
    }
    if (EDGE_SIZE (type) != EDGE_NORMAL) {
      f->w = (f->w + 1)/2;
    }
    f->w = MAX(f->w, min_w_in_lambda*getGridsPerLambda());
    f->l = MAX(f->l, min_l_in_lambda*getGridsPerLambda());
  }
  else if (EDGE_SIZE (type) == EDGE_STATINV
	   || EDGE_SIZE(type) == EDGE_FEEDBACK) {
    /* use standard staticizer size; ignore sz field */
    f->w = n->sz[f->type].sw;
    f->l = n->sz[f->type].sl;
    f->flavor = 0;		/* standard fet */
  }
  else {
    /*
       This case is used for internal precharges; so don't override
       any default sizing. The size selected is the min size,
       overridden by sz directive if it exists.
    */
    if (sz) {
      f->flavor = sz->flavor;

      if (sz->w) {
	f->w = (sz->w->type == E_INT ? sz->w->u.ival.v*getGridsPerLambda() :
		sz->w->u.f*getGridsPerLambda());
      }
      else {
	f->w = min_w_in_lambda*getGridsPerLambda();
      }
      if (sz->l) {
	f->l = (sz->l->type == E_INT ? sz->l->u.ival.v*getGridsPerLambda() :
		sz->l->u.f*getGridsPerLambda());
      }
      else {
	f->l = min_l_in_lambda*getGridsPerLambda();
      }
      if (sz->folds) {
	Assert (sz->folds->type == E_INT, "What?");
	f->nfolds = sz->folds->u.ival.v;
	if (f->nfolds < 1) {
	  f->nfolds = 1;
	}
      }
    }
    else {
      f->w = min_w_in_lambda*getGridsPerLambda();
      f->l = min_l_in_lambda*getGridsPerLambda();
    }
  }
  if (f->w/f->nfolds < min_w_in_lambda*getGridsPerLambda()) {
    f->nfolds = f->w/(min_w_in_lambda*getGridsPerLambda());
  }

  if (_fin_width > 0) {
    f->w = (f->w + _fin_width - 1)/_fin_width;
    f->w *= _fin_width;
  }
}

/*
  type == n/p  with optional EDGE_CELEM
*/
static bool_t *compute_bool (netlist_t *N, act_prs_expr_t *e, int type, int sense = 0)
{
  bool_t *l, *r, *b;
  act_booleanized_var_t *v;
  hash_bucket_t *at;
  
  if (!e) return bool_false (N->B);
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    l = compute_bool (N, e->u.e.l, type, sense);
    r = compute_bool (N, e->u.e.r, type, sense);
    if ((e->type == ACT_PRS_EXPR_AND && sense == 0) ||
	(e->type == ACT_PRS_EXPR_OR && sense == 1)) {
      /* and */
      b = bool_and (N->B, l, r);
    }
    else {
      b = bool_or (N->B, l, r);
    }
    bool_free (N->B, l);
    bool_free (N->B, r);
    return b;
    break;

  case ACT_PRS_EXPR_NOT:
    return compute_bool (N, e->u.e.l, type, 1 - sense);
    break;
      
  case ACT_PRS_EXPR_VAR:
    v = var_lookup (N, e->u.v.id);
    if ((sense == 0 && ((type & EDGE_CELEM) == 0)) ||
	(sense == 1 && ((type & EDGE_CELEM) != 0))) {
      b = bool_copy (N->B, VINF(v)->b);
    }
    else {
      b = bool_not (N->B, VINF(v)->b);
    }
    return b;
    break;

  case ACT_PRS_EXPR_LABEL:
    /* only occurs in a simple prs */
    at = hash_lookup (N->atH[EDGE_TYPE(type)], e->u.l.label);
    if (!at) {
      act_error_ctxt (stderr);
      fatal_error ("@-expression with unknown label `%s'", e->u.l.label);
    }
    return compute_bool (N, ((at_lookup *)at->v)->e, type, sense);
    break;

  case ACT_PRS_EXPR_TRUE:
    return bool_true (N->B);
    break;
    
  case ACT_PRS_EXPR_FALSE:
    return bool_false (N->B);
    break;
    
  default:
    fatal_error ("Unknown type");
    break;
  }
  return NULL;
}

static int is_two_inp_c_elem (act_prs_expr_t *e)
{
  if (!e) {
    return 0;
  }
  if (e->type != ACT_PRS_EXPR_AND) {
    return 0;
  }
  
#define IS_VAR_OR_NOTVAR(x) ((x)->type == ACT_PRS_EXPR_VAR || (x)->type == ACT_PRS_EXPR_NOT && (x)->u.e.l->type == ACT_PRS_EXPR_VAR)
  
  if (!IS_VAR_OR_NOTVAR (e->u.e.l) || !IS_VAR_OR_NOTVAR (e->u.e.r)) {
    return 0;
  }
  
  return 1;
}


/*
 * create edges from left to right, corresponding to expression "e"
 *
 *  sense = 0 : normal
 *  sense = 1 : complemented 
 *
 *  type has two parts: the EDGE_TYPE has EDGE_PFET or EDGE_NFET
 *                      the EDGE_SIZE piece (normal, feedback, small
 *                      inv)
 *
 *  Returns max depth
 */
int ActNetlistPass::create_expr_edges (netlist_t *N, int type, node_t *left,
					act_prs_expr_t *e, node_t *right, int sense)
{
  node_t *mid;
  node_t *at_node;
  int ldepth, rdepth;
  if (!e) return 0;

  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    if (((e->type == ACT_PRS_EXPR_AND) && (sense == 0)) ||
	((e->type == ACT_PRS_EXPR_OR) && (sense == 1))) {
      /* it is an AND */

      /* special case check:  @x & ... -> v+ OR ~@x & ... -> v- */
      at_node = NULL;
      if (e->type == ACT_PRS_EXPR_AND) {
	hash_bucket_t *b;
	if (e->u.e.l->type == ACT_PRS_EXPR_LABEL
	    && EDGE_TYPE (type) == EDGE_PFET) {
	  b = hash_lookup (N->atH[EDGE_PFET], e->u.e.l->u.l.label);
	  if (!b) {
	    act_error_ctxt (stderr);
	    fatal_error ("@-expression with unknown label `%s'", e->u.e.l->u.l.label);
	  }
	  at_node = ((at_lookup *)b->v)->n;
	  ldepth = ((at_lookup *)b->v)->max_depth;
	}
	else if (e->u.e.l->type == ACT_PRS_EXPR_NOT
		 && EDGE_TYPE (type) == EDGE_NFET
		 && e->u.e.l->u.e.l->type == ACT_PRS_EXPR_LABEL) {
	  b = hash_lookup (N->atH[EDGE_NFET], e->u.e.l->u.e.l->u.l.label);
	  if (!b) {
	    act_error_ctxt (stderr);
	    fatal_error ("@-expression with unknown label `%s'", e->u.e.l->u.e.l->u.l.label);
	  }
	  at_node = ((at_lookup *)b->v)->n;
	  ldepth = ((at_lookup *)b->v)->max_depth;
	}
      }
      /* create edges recursively */
      if (!at_node) {
	mid = node_alloc (N, NULL);
	if ((type & EDGE_HALFNORMREV) == EDGE_HALFNORMREV) {
	  ldepth = create_expr_edges (N, type, left, e->u.e.r, mid, sense);
	  rdepth = create_expr_edges (N, type, mid, e->u.e.l, right, sense);
	}
	else {
	  ldepth = create_expr_edges (N, type, left, e->u.e.l, mid, sense);
	  rdepth = create_expr_edges (N, type, mid, e->u.e.r, right, sense);
	}
      }
      else {
	mid = at_node;
	rdepth = create_expr_edges (N, type, mid, e->u.e.r, right, sense);
      }

      /* internal precharge */
      if (e->u.e.pchg_type != -1 && (EDGE_SIZE(type) == EDGE_NORMAL) &&
	  ((EDGE_MODE (type) & (EDGE_INVERT|EDGE_CELEM)) == 0)) {
	if (e->u.e.pchg_type == 0 /* to GND */) {
	  create_expr_edges (N, EDGE_PCHG, mid, e->u.e.pchg, N->GND, 0);
	}
	else {
	  create_expr_edges (N, EDGE_PCHG, mid, e->u.e.pchg, N->Vdd, 0);
	}
      }
      /*-- end of AND --*/
      ldepth = ldepth + rdepth;
    }
    else {
      /* it is an OR */
      ldepth = create_expr_edges (N, type, left, e->u.e.l, right, sense);
      rdepth = create_expr_edges (N, type, left, e->u.e.r, right, sense);
      if (rdepth > ldepth) {
	ldepth = rdepth;
      }
    }
    break;
    
  case ACT_PRS_EXPR_NOT:
    ldepth = create_expr_edges (N, type, left, e->u.e.l, right, 1-sense);
    break;
    
  case ACT_PRS_EXPR_VAR:
    if ((EDGE_TYPE (type) == EDGE_NFET && sense == 0) ||
	(EDGE_TYPE (type) == EDGE_PFET && sense == 1) ||
	(EDGE_MODE (type) == EDGE_PCHG)) {
      edge_t *f;

      f = edge_alloc (N, e->u.v.id, left, right,
		      (sense == 0 /* uninverted var, n-fet */
		       ? N->psc : N->nsc));

      /* set w, l, flavor, subflavor */
      f->type = (sense == 0 ? EDGE_NFET : EDGE_PFET);
      set_fet_params (N, f, type, e->u.v.sz);
    }
    else if (EDGE_MODE (type) == EDGE_CELEM &&
	     ((EDGE_TYPE (type) == EDGE_PFET && sense == 0) ||
	      (EDGE_TYPE (type) == EDGE_NFET && sense == 1))) {
      edge_t *f;

      f = edge_alloc (N, e->u.v.id, left, right,
		      (sense == 1 /* inverted var, n-fet */
		       ? N->psc : N->nsc));
      f->type = (sense == 0 ? EDGE_PFET : EDGE_NFET);
      set_fet_params (N, f, type, e->u.v.sz);
    }
    else {
      fprintf (stderr, "Production rule is not CMOS-implementable.\n");
      fprintf (stderr, " variable: ");
      e->u.v.id->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
    ldepth = 1;
    break;
      
  case ACT_PRS_EXPR_LABEL:
    act_error_ctxt (stderr);
    fatal_error ("Label `%s' in a context where it was not recognized.",
		 e->u.l.label);
    break;
    
  case ACT_PRS_EXPR_TRUE:
    if (sense == 1) {
      /* done, disconnected */
      ldepth = 0;
    }
    else {
      edge_t *f;
      node_t *supply, *sub;
      /* depending on n-type or p-type, use Vdd or GND */
      if (EDGE_TYPE (type) == EDGE_NFET) {
	supply = N->Vdd;
	sub = N->psc;
      }
      else {
	supply = N->GND;
	sub = N->nsc;
      }
      f = edge_alloc (N, supply, left, right, sub);
      f->type = EDGE_TYPE (type);
      set_fet_params (N, f, type, NULL);
      ldepth = 1;
    }
    break;
  case ACT_PRS_EXPR_FALSE:
    if (sense == 0) {
      /* left and right are disconnected; done */
      ldepth = 0;
    }
    else {
      edge_t *f;
      node_t *supply, *sub;
      /* depending on n-type or p-type, use Vdd or GND */
      if (EDGE_TYPE (type) == EDGE_NFET) {
	supply = N->Vdd;
	sub = N->psc;
      }
      else {
	supply = N->GND;
	sub = N->nsc;
      }
      f = edge_alloc (N, supply, left, right, sub);
      f->type = EDGE_TYPE (type);
      set_fet_params (N, f, type, NULL);
      ldepth = 1;
    }
    break;
  default:
    fatal_error ("Unknown type");
    break;
  }
  return ldepth;
}

static act_prs_expr_t *synthesize_celem (act_prs_expr_t *e)
{
  act_prs_expr_t *ret;
  act_prs_expr_t *tmp;
  
  if (!e) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = e->type;
  
  switch (ret->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l = synthesize_celem (e->u.e.l);
    ret->u.e.r = synthesize_celem (e->u.e.r);
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;
    
  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = synthesize_celem (e->u.e.l);
    ret->u.e.r = NULL;
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_LABEL:
    act_error_ctxt (stderr);
    fatal_error ("@-expressions cannot be used in prs with a  #>");
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v.id = e->u.v.id;
    ret->u.v.sz = e->u.v.sz;	/* keep sizing */
    NEW (tmp, act_prs_expr_t);
    tmp->type = ACT_PRS_EXPR_NOT;
    tmp->u.e.pchg = NULL;
    tmp->u.e.pchg_type = -1;
    tmp->u.e.r = NULL;
    tmp->u.e.l = ret;
    ret = tmp;
    break;

  default:
    fatal_error ("Unknown expression type in synthesize_celem()");
    break;
  }
  return ret;
}

node_t *ActNetlistPass::connection_to_node (netlist_t *N, act_connection *c)
{
  node_t *n;
  
  n = N->hd;
  while (n) {
    if (n->v) {
      Assert (n->v->v, "What?");
      if (n->v->v->id == c) {
	return n;
      }
    }
    n = n->next;
  }
  return NULL;
}

node_t *ActNetlistPass::string_to_node (netlist_t *N, char *s)
{
  int k;
  node_t *n = NULL;
  if (!s) return NULL;
  if (!*s) return NULL;

  Process *p = N->bN->p;
  
  if (*s == '#') {
    if (*(s+1) == 'f') {
      k = atoi (s+3);
    }
    else {
      k = atoi (s+1);
    }
    /* k = node# */
    n = N->hd;
    while (n) {
      if (n->i == k) {
	break;
      }
      n = n->next;
    }
  }
  else {
    /* not an internal node */
    char *t = s;
    char dot;
    while (*t && *t != '.' && *t != '[') {
      t++;
    }
    Assert (N->bN->cur, "What?");
    dot = *t;
    *t = '\0';
    if (!N->bN->cur->FullLookup (s)) {
      *t = dot;
      /* has to be localvdd, localgnd, globalvdd, globalgnd */
    }
    else {
      *t = dot;
      ActId *id = ActId::parseId (s); /* was: act_string_to_id (s);*/
      if (!id) {
	fatal_error ("Could not convert string `%s' to an ActId", s);
      }
      if (!id->validateDeref (N->bN->cur)) {
	// could not validate/find signal
	return NULL;
      }
      act_connection *c = id->Canonical (N->bN->cur);
      n = ActNetlistPass::connection_to_node (N, c);
    }
    if (!n) {
      if ((strcmp (s, local_vdd) == 0) ||
	  (strcmp (s, global_vdd) == 0)) {
	n = N->Vdd;
      }
      else if ((strcmp (s, local_gnd) == 0) ||
	       (strcmp (s, global_gnd) == 0)) {
	n = N->GND;
      }
    }
  }
  if (!n) {
    Assert (p, "What?");
    fatal_error ("Looking for signal `%s', not found in `%s'", s,
		 p->getName());
  }
  return n;
}

void ActNetlistPass::sprint_conn (char *buf, int sz, act_connection *c)
{
  ActId *id = c->toid();
  if (c->isglobal()) {
    ActNamespace *ns = c->getnsifglobal ();
    if (ns && ns != ActNamespace::Global()) {
      char *tmp = ns->Name(true);
      int len;
      snprintf (buf, sz, "%s", tmp);
      FREE (tmp);
      len = strlen (buf);
      buf += len;
      sz -= len;
    }
  }
  id->sPrint (buf, sz);
  delete id;
  return;
}

void ActNetlistPass::sprint_node (char *buf, int sz, netlist_t *N, node_t *n)
{
  if (!ActNetlistPass::local_vdd) {
    ActNetlistPass::local_vdd = config_get_string ("net.local_vdd");
    ActNetlistPass::local_gnd = config_get_string ("net.local_gnd");

    ActNetlistPass::global_vdd = config_get_string ("net.global_vdd");
    ActNetlistPass::global_gnd = config_get_string ("net.global_gnd");
  }

  if (n->v) {
    sprint_conn (buf, sz, n->v->v->id);
    return;
  }
  else {
    if (n == N->Vdd) {
      if (N->bN->cur) {
	ValueIdx *vx;
	act_connection *c;
	vx = N->bN->cur->FullLookupVal (local_vdd);
	if (!vx) {
	  vx = N->bN->cur->FullLookupVal (global_vdd);
	}
	if (vx) {
	  if (vx->hasConnection()) {
	    c = vx->connection()->primary();
	    sprint_conn (buf, sz, c);
	  }
	  else {
	    char *tmp = NULL;
	    if (vx->global) {
	      if (vx->global != ActNamespace::Global()) {
		tmp = vx->global->Name (true);
	      }
	    }
	    snprintf (buf, sz, "%s%s", tmp ? tmp : "", vx->getName());
	    if (tmp) {
	      FREE (tmp);
	    }
	  }
	}
	else {
	  snprintf (buf, sz, "%s", global_vdd);
	}
      }
      else {
	snprintf (buf, sz, "%s", global_vdd);
      }	
    }
    else if (n == N->GND) {
      if (N->bN->cur) {
	ValueIdx *vx;
	act_connection *c;
	vx = N->bN->cur->FullLookupVal (local_gnd);
	if (!vx) {
	  vx = N->bN->cur->FullLookupVal (global_gnd);
	}
	if (vx) {
	  if (vx->hasConnection()) {
	    c = vx->connection()->primary();
	    sprint_conn (buf, sz, c);
	  }
	  else {
	    snprintf (buf, sz, "%s", vx->getName());
	  }
	}
	else {
	  snprintf (buf, sz, "%s", global_gnd);
	}
      }
      else {
	snprintf (buf, sz, "%s", global_gnd);
      }
    }
    else {
      if (n->inv) {
	snprintf (buf, sz, "#fb%d#", n->i);
      }
      else {
	snprintf (buf, sz, "#%d", n->i);
      }
    }
  }
}

void ActNetlistPass::emit_node (netlist_t *N, FILE *fp, node_t *n,
				// alternate output
				const char *inst_name,
				const char *pin,
				int mangle)
{
  char buf[10240];

  sprint_node (buf, 10240, N, n);
  if (split_net (buf) && inst_name) {
    if (mangle == 1) {
      ActNetlistPass::current_act->mfprintf (fp, "%s", inst_name);
    }
    else {
      // mangle = 0 or 2
      fprintf (fp, "%s", inst_name);
    }
    if (mangle) {
      // mangle = 1 or 2
      ActNetlistPass::current_act->mfprintf (fp, ".%s", pin);
    }
    else {
      fprintf (fp, ".%s", pin);
    }
  }
  else {
    if (mangle) {
      ActNetlistPass::current_act->mfprintf (fp, "%s", buf);
    }
    else {
      fprintf (fp, "%s", buf);
    }
  }
}

static void tree_compute_sharing (netlist_t *N, node_t *power,
				  int type, int count)
{
  list_t *wavefront;
  listitem_t *li, *mi;
  node_t *a;
  edge_t *e, *f;

  list_t *next;

  wavefront = list_new ();
  next = list_new ();

  list_append (wavefront, power);
  while (count > 0) {
#if 0
    printf ("count = %d\n", count);
    printf ("size of wavefront = %d\n", list_length (wavefront));
#endif    
    count--;
  
    while (!list_isempty (wavefront)) {
      
      a = (node_t *)list_delete_tail (wavefront);
      /* a is a surviving node */
      a->visited = 1;

#if 0 
      printf ("node ");
      emit_node (N, stdout, a);
      printf ("\n");
#endif      

      /* now check if there are any tree edges from a */
      li = list_first (a->e);
      while (li) {
	e = (edge_t *)list_value (li);

	if (e->type != type || e->pruned  || !e->tree) {
	  li = list_next (li);
	  continue;
	}

	if ((e->a == a && (e->b->visited || e->b->v)) ||
	    (e->b == a && (e->a->visited || e->a->v))) {
	  li = list_next (li);
	  continue;
	}

#if 0
	printf (" -- edge to ");
	if (a == e->a) {
	  emit_node (N, stdout, e->b);
	}
	else {
	  emit_node (N, stdout, e->a);
	}
	printf (" [flags:");
	if (e->pruned) {
	  printf (" pruned");
	}
	if (e->tree) {
	  printf (" tree");
	}
	printf ("]\n");
#endif

	if (e->a == a) {
	  if (!e->b->visited) {
	    /* add e->b to  the next list */
	    listitem_t *xi;
	    for (xi = list_first (next); xi; xi = list_next (xi)) {
	      if ((node_t *)list_value (xi) == e->b)
		break;
	    }
	    if (!xi) {
	      list_append (next, e->b);
	    }
	  }
	}
	else {
	  if (!e->a->visited) {
	    /* add e->a to the next list */
	    listitem_t *xi;
	    for (xi = list_first (next); xi; xi = list_next (xi)) {
	      if ((node_t *)list_value (xi) == e->a)
		break;
	    }
	    if (!xi) {
	      list_append (next, e->a);
	    }
	  }
	}

	/* e is a surviving tree edge */
	mi = list_next (li);
	while (mi) {
	  f = (edge_t *)list_value (mi);
	  if (f->type != e->type || !f->tree || f->pruned) {
	    mi = list_next (mi);
	    continue;
	  }

	  if (f->a == a && f->b->v) {
	    mi = list_next (li);
	    continue;
	  }
	  if (f->b == a && f->a->v) {
	    mi = list_next (li);
	    continue;
	  }
	  
	  if (f->g == e->g) {
	    node_t *merge;
	    node_t *src;
	    /* ok we can prune f! */
	    f->pruned = 1;

	    if (f->a == a) {
	      merge = f->b;
	    }
	    else {
	      merge = f->a;
	    }

	    /* merge "merge" with e */
	    if (e->a == a) {
	      src = e->b;
	    }
	    else {
	      src = e->a;
	    }

	    /* merge with src */
	    for (listitem_t *xi = list_first (merge->e);
		 xi; xi = list_next (xi)) {
	      edge_t *tmp = (edge_t *) list_value (xi);
	      if (tmp->a == merge) {
		tmp->a = src;
	      }
	      else {
		Assert (tmp->b == merge, "Hmm");
		tmp->b = src;
	      }
	    }
	    list_concat (src->e, merge->e);
	  }
	  mi = list_next (mi);
	}
	li = list_next (li);
      }
    }
    list_t *xtmp = wavefront;
    wavefront = next;
    next = xtmp;
  }
  
  list_free (next);
  list_free (wavefront);

  for (a = N->hd; a; a = a->next) {
    a->visited = 0;
  }
}

static void update_bdds_exprs (netlist_t *N,
			       act_booleanized_var_t *v, act_prs_expr_t *e,
			       int type)
{
  bool_t *b1, *b2;
  act_prs_expr_t *tmp;

  if (type & EDGE_INVERT) {
    b1 = compute_bool (N, e, type, 1);
    NEW (tmp, act_prs_expr_t);
    tmp->type = ACT_PRS_EXPR_NOT;
    tmp->u.e.l = e;
    tmp->u.e.r = NULL;
    tmp->u.e.pchg = NULL;
    tmp->u.e.pchg_type = -1;
    e = tmp;
  }
  else if (type & EDGE_CELEM) {
    b1 = compute_bool (N, e, type, 0);
    e = synthesize_celem (e);
  }
  else {
    /* normal */
    b1 = compute_bool (N, e, type, 0);
  }
  
  if (((EDGE_TYPE (type) == EDGE_NFET) && ((type & (EDGE_INVERT|EDGE_CELEM)) == 0))
      ||
      (EDGE_TYPE (type) == EDGE_PFET && ((type & (EDGE_INVERT|EDGE_CELEM)) != 0))) {
    /* pull-down, n-type */
    if (!(type & (EDGE_KEEPER|EDGE_CKEEPER))) {
      if (!VINF(v)->e_dn) {
	VINF(v)->e_dn = e;
      }
      else {
	NEW (tmp, act_prs_expr_t);
	tmp->type = ACT_PRS_EXPR_OR;
	tmp->u.e.l = VINF(v)->e_dn;
	tmp->u.e.r = e;
	tmp->u.e.pchg = NULL;
	tmp->u.e.pchg_type = -1;
	VINF(v)->e_dn = tmp;
      }
    }
    b2 = VINF(v)->dn;
    VINF(v)->dn = bool_or (N->B, b2, b1);
    bool_free (N->B, b1);
    bool_free (N->B, b2);
  }
  else {
    /* pull-up, p-type */
    if (!(type & (EDGE_KEEPER | EDGE_CKEEPER))) {
      if (!VINF(v)->e_up) {
	VINF(v)->e_up = e;
      }
      else {
	NEW (tmp, act_prs_expr_t);
	tmp->type = ACT_PRS_EXPR_OR;
	tmp->u.e.l = VINF(v)->e_up;
	tmp->u.e.r = e;
	tmp->u.e.pchg = NULL;
	tmp->u.e.pchg_type = -1;
	VINF(v)->e_up = tmp;
      }
    }
    b2 = VINF(v)->up;
    VINF(v)->up = bool_or (N->B, b2, b1);
    bool_free (N->B, b1);
    bool_free (N->B, b2);
  }
}

void ActNetlistPass::generate_prs_graph (netlist_t *N, act_prs_lang_t *p,
					 int istree)
{
  int d;
  if (!p) return;
  act_attr_t *attr;
  int depth;

  switch (ACT_PRS_LANG_TYPE (p->type)) {
  case ACT_PRS_RULE:
    d = (p->u.one.dir == 0 ? EDGE_NFET : EDGE_PFET);

    /*-- reset default sizes per production rule --*/
    N->sz[EDGE_NFET].w = config_get_int ("net.std_n_width")*getGridsPerLambda();
    N->sz[EDGE_NFET].l = config_get_int ("net.std_n_length")*getGridsPerLambda();
    N->sz[EDGE_PFET].w = config_get_int ("net.std_p_width")*getGridsPerLambda();
    N->sz[EDGE_PFET].l = config_get_int ("net.std_p_length")*getGridsPerLambda();
    N->sz[EDGE_NFET].nf = 1;
    N->sz[EDGE_PFET].nf = 1;
    N->sz[EDGE_NFET].flavor = 0;
    N->sz[EDGE_PFET].flavor = 0;

    if (p->u.one.label) {
      if (istree) {
	act_error_ctxt (stderr);
	fatal_error ("@ variables cannot be within a tree { } block");
      }
      hash_bucket_t *b;
      b = hash_lookup (N->atH[d], (char *)p->u.one.id);
      if (b || hash_lookup (N->atH[1-d], (char *)p->u.one.id)) {
	act_error_ctxt (stderr);
	fatal_error ("Duplicate label `%s'", (char *)p->u.one.id);
      }
      b = hash_add  (N->atH[d], (char *)p->u.one.id);
      at_lookup *a = at_alloc ();
      b->v = a;
      a->n = node_alloc (N, NULL);
      a->e = p->u.one.e;
      depth = create_expr_edges (N, d | EDGE_NORMAL,
				 (d == EDGE_NFET ? N->GND : N->Vdd),
				 p->u.one.e, ((at_lookup *)b->v)->n, 0);
      a->max_depth = depth;
      //b = hash_lookup (N->atH[d], (char *)p->u.one.id);
      if (d == EDGE_NFET && series_n_warning) {
	if (depth >= series_n_warning) {
	  act_error_ctxt (stderr);
	  warning ("Label `%s': series nfet chain length %d exceeds threshold (%d)\n", b->key, depth, series_n_warning);
	}
      }
      else if (d == EDGE_PFET && series_p_warning) {
	if (depth >= series_p_warning) {
	  act_error_ctxt (stderr);
	  warning ("Label `%s': series pfet chain length %d exceeds threshold (%d)\n", b->key, depth, series_p_warning);
	}
      }
    }
    else {
      act_booleanized_var_t *v;
      double cap = default_load_cap;
      unsigned int attr_type = 0;
      int is_h_celem = 0;

      v = var_lookup (N, p->u.one.id);
      Assert (v->output == 1, "eh?");

      for (attr = p->u.one.attr; attr; attr = attr->next) {
	/* look for keeper, iskeeper, isckeeper, loadcap, oresis,
	   output,  N_reff, P_reff, autokeeper, comb 
	*/
	if (strcmp (attr->attr, "keeper") == 0) {
	  if (attr->e->u.ival.v == 0) {
	    /* don't generate a keeper */
	    VINF(v)->unstaticized = 1;
	  }
	  else if (attr->e->u.ival.v == 2) {
	    /* don't generate a keeper, but it isn't state-holding */
	    VINF(v)->unstaticized = 2;
	  }
	  else {
	    VINF(v)->unstaticized = 0;
	    if (attr->e->u.ival.v == 3) {
	      is_h_celem = 1;
	    }
	  }
	}
	else if (strcmp (attr->attr, "iskeeper") == 0) {
	  if (attr->e->u.ival.v) {
	    attr_type |= EDGE_KEEPER;
	    VINF(v)->manualkeeper = 1;
	  }
	}
	else if (strcmp (attr->attr, "isckeeper") == 0) {
	  if (attr->e->u.ival.v) {
	    attr_type |= EDGE_CKEEPER;
	    VINF(v)->manualkeeper = 2;
	  }
	}
	else if (strcmp (attr->attr, "loadcap") == 0) {
	  Assert (act_expr_getconst_real (attr->e, &cap), "loadcap error?");
	}
	else if (strcmp (attr->attr, "oresis") == 0) {
	  /* do something here */
	}
	else if (strcmp (attr->attr, "N_reff") == 0) {
	  Assert (act_expr_getconst_real (attr->e, &VINF(v)->n->reff[EDGE_NFET]),
		  "N_reff error?");
	  VINF(v)->n->reff_set[EDGE_NFET] = 1;
	}
	else if (strcmp (attr->attr, "P_reff") == 0) {
	  Assert (act_expr_getconst_real (attr->e, &VINF(v)->n->reff[EDGE_PFET]),
		  "P_reff error?");
	  VINF(v)->n->reff_set[EDGE_PFET] = 1;
	}
	else if (strcmp (attr->attr, "comb") == 0) {
	  if (attr->e->u.ival.v) {
	    VINF(v)->usecf = 1;
	  }
	  else {
	    VINF(v)->usecf = 0;
	  }
	}
      }
      
      VINF(v)->n->cap = cap;

      if (p->u.one.arrow_type == 0) {
	if (is_h_celem) {
	  warning ("H-type c-element specifier ignored");
	}
	/* -> */
	depth = create_expr_edges (N, d | attr_type | EDGE_NORMAL | (istree ? EDGE_TREE : 0),
			   (d == EDGE_NFET ? N->GND : N->Vdd),
			   p->u.one.e, VINF(v)->n, 0);
	update_bdds_exprs (N, v, p->u.one.e, d|attr_type|EDGE_NORMAL);
	_check_emit_warning (d, depth, p->u.one.id);
	
	check_supply (N, p->u.one.id, d, (d == EDGE_NFET ? N->GND : N->Vdd));
      }
      else if (p->u.one.arrow_type == 1) {
	if (is_h_celem) {
	  warning ("H-type c-element specifier ignored");
	}
	if (istree) {
	  act_error_ctxt (stderr);
	  fatal_error ("tree { } blocks can only contain `->' production rules");
	}
	/* => */
	depth = create_expr_edges (N, d | attr_type | EDGE_NORMAL,
				   (d == EDGE_NFET ? N->GND : N->Vdd),
				   p->u.one.e, VINF(v)->n, 0);
	update_bdds_exprs (N, v, p->u.one.e, d|attr_type|EDGE_NORMAL);
	_check_emit_warning (d, depth, p->u.one.id);
	
	depth = create_expr_edges (N, (1-d) | attr_type | EDGE_INVERT | EDGE_NORMAL,
				   (d == EDGE_NFET ? N->Vdd : N->GND),
				   p->u.one.e, VINF(v)->n, 1);
	update_bdds_exprs (N, v, p->u.one.e, d|attr_type|EDGE_NORMAL|EDGE_INVERT);
	_check_emit_warning (1-d, depth, p->u.one.id);

	check_supply (N, p->u.one.id, EDGE_NFET, N->GND);
	check_supply (N, p->u.one.id, EDGE_PFET, N->Vdd);
      }
      else {
	Assert (p->u.one.arrow_type == 2, "???");
	if (istree) {
	  act_error_ctxt (stderr);
	  fatal_error ("tree { } blocks can only contain `->' production rules");
	}
	/* #> */

	if (is_h_celem) {
	  if (!is_two_inp_c_elem (p->u.one.e) || attr_type != 0) {
	    warning ("H-type c-element specifier ignored");
	    is_h_celem = 0;
	  }
	  else {
	    VINF(v)->spec_keeper = 1;
	  }
	}

	if (is_h_celem) {
	  depth = create_expr_edges (N, d | EDGE_HALFNORM,
				     (d == EDGE_NFET ? N->GND : N->Vdd),
				     p->u.one.e, VINF(v)->n, 0);
	  depth = create_expr_edges (N, d | EDGE_HALFNORMREV,
				     (d == EDGE_NFET ? N->GND : N->Vdd),
				     p->u.one.e, VINF(v)->n, 0);
	  _check_emit_warning (d, depth, p->u.one.id);
	  update_bdds_exprs (N, v, p->u.one.e, d | EDGE_NORMAL);

	  depth = create_expr_edges (N, (1-d) | EDGE_HALFNORM | EDGE_CELEM,
				     (d == EDGE_NFET ? N->Vdd : N->GND),
				     p->u.one.e, VINF(v)->n, 0);
	  depth = create_expr_edges (N, (1-d) | EDGE_HALFNORMREV | EDGE_CELEM,
				     (d == EDGE_NFET ? N->Vdd : N->GND),
				     p->u.one.e, VINF(v)->n, 0);
	  update_bdds_exprs (N, v, p->u.one.e, d | attr_type | EDGE_CELEM |
			     EDGE_NORMAL);
	  _check_emit_warning (1-d, depth, p->u.one.id);
	}
	else {
	  depth = create_expr_edges (N, d | attr_type | EDGE_NORMAL,
				     (d == EDGE_NFET ? N->GND : N->Vdd),
				     p->u.one.e, VINF(v)->n, 0);

	  update_bdds_exprs (N, v, p->u.one.e, d | attr_type | EDGE_NORMAL);
	  _check_emit_warning (d, depth, p->u.one.id);
	
	  depth = create_expr_edges (N, (1-d) | attr_type | EDGE_CELEM | EDGE_NORMAL,
				     (d == EDGE_NFET ? N->Vdd : N->GND),
				     p->u.one.e, VINF(v)->n, 0);
	  update_bdds_exprs (N, v, p->u.one.e, d | attr_type | EDGE_CELEM | EDGE_NORMAL);
	  _check_emit_warning (1-d, depth, p->u.one.id);
	}

	check_supply (N, p->u.one.id, EDGE_NFET, N->GND);
	check_supply (N, p->u.one.id, EDGE_PFET, N->Vdd);
      }
    }
    break;

  case ACT_PRS_DEVICE:
    {
      netlist_device *c;
      NEW (c, netlist_device);
      c->idx = p->type - ACT_PRS_DEVICE;
      c->n1 = VINF(var_lookup (N, p->u.p.s))->n;
      c->n2 = VINF(var_lookup (N, p->u.p.d))->n;
      /* XXX: 1e-15 scaling; need to fix this */
      if (p->u.p.sz) {
	// XXX: handle this for all device types, not just for caps!
	c->wval = (p->u.p.sz->w->type == E_INT ?
		   p->u.p.sz->w->u.ival.v : p->u.p.sz->w->u.f);
	c->wval *= unit_dev;
	if (p->u.p.sz->l) {
	  c->lval = (p->u.p.sz->l->type == E_INT ?
		     p->u.p.sz->l->u.ival.v : p->u.p.sz->l->u.f);
	}
	else {
	  c->lval = 1;
	}
      }
      else {
	c->wval = unit_dev;
	c->lval = 1;
      }
      if (!N->devs) {
	N->devs = list_new ();
      }
      list_append (N->devs, c);
    }
    break;

  case ACT_PRS_GATE:
    /*-- reset default sizes per gate --*/
    N->sz[EDGE_NFET].w = config_get_int ("net.std_n_width")*getGridsPerLambda();
    N->sz[EDGE_NFET].l = config_get_int ("net.std_n_length")*getGridsPerLambda();
    N->sz[EDGE_PFET].w = config_get_int ("net.std_p_width")*getGridsPerLambda();
    N->sz[EDGE_PFET].l = config_get_int ("net.std_p_length")*getGridsPerLambda();
    N->sz[EDGE_NFET].nf = 1;
    N->sz[EDGE_PFET].nf = 1;
    for (attr = p->u.p.attr; attr; attr = attr->next) {
      if (strcmp (attr->attr, "output") == 0) {
	unsigned int v = attr->e->u.ival.v;
	if (v & 0x1) {
	  act_booleanized_var_t *x = var_lookup (N, p->u.p.s);
	  Assert (x->output == 1, "What?");
	}
	if (v & 0x2) {
	  act_booleanized_var_t *x = var_lookup (N, p->u.p.d);
	  Assert (x->output == 1, "Hmm");
	}
      }
    }
    if (p->u.p.g) {
      edge_t *f;
      /* add nfet */
      f = edge_alloc (N, p->u.p.g,
		      VINF(var_lookup (N, p->u.p.s))->n,
		      VINF(var_lookup (N, p->u.p.d))->n,
		      N->psc);
      f->type = EDGE_NFET;
      f->raw = 1;
      set_fet_params (N, f, EDGE_NFET|EDGE_NORMAL, p->u.p.sz);
    }
    if (p->u.p._g) {
      edge_t *f;
      /* add pfet */
      f = edge_alloc (N, p->u.p._g,
		      VINF(var_lookup (N, p->u.p.s))->n,
		      VINF(var_lookup (N, p->u.p.d))->n,
		      N->nsc);
      f->type = EDGE_PFET;
      f->raw = 1;
      set_fet_params (N, f, EDGE_PFET|EDGE_NORMAL, p->u.p.sz);
    }
    break;
    
  case ACT_PRS_TREE:
    act_prs_lang_t *x;
    for (x = p->u.l.p; x; x = x->next) {
      generate_prs_graph (N, x, 1);
    }
    tree_compute_sharing (N, N->GND, EDGE_NFET,
			  p->u.l.lo ? p->u.l.lo->u.ival.v : 4);
    tree_compute_sharing (N, N->Vdd, EDGE_PFET,
			  p->u.l.lo ? p->u.l.lo->u.ival.v : 4);
    break;
    
  case ACT_PRS_SUBCKT:
    /* handle elsewhere */
    act_error_ctxt (stderr);
    warning("subckt { } in prs bodies is ignored; use defcell instead");
    for (act_prs_lang_t *x = p->u.l.p; x; x = x->next) {
      generate_prs_graph (N, x);
    }
    break;

  default:
    fatal_error ("Should not be here\n");
    break;
  }
}

static void release_atalloc (struct Hashtable *H)
{
  int i;
  hash_bucket_t *b;
  hash_iter_t iter;
  hash_iter_init (H, &iter);
  while ((b = hash_iter_next (H, &iter))) {
    if (b->v) {
      FREE (b->v);
    }
    b->v = NULL;
  }
}

static void _set_current_supplies (netlist_t *N, act_prs *p)
{
  /* add vdd and gnd */
  act_connection *cvdd, *cgnd, *cpsc, *cnsc;

  cvdd = NULL;
  cgnd = NULL;
  cpsc = NULL;
  cnsc = NULL;

  if (p && p->vdd) cvdd = p->vdd->Canonical(N->bN->cur);
  if (p && p->gnd) cgnd = p->gnd->Canonical(N->bN->cur);
  if (p && p->psc) cpsc = p->psc->Canonical(N->bN->cur);
  if (p && p->nsc) cnsc = p->nsc->Canonical(N->bN->cur);

  N->sz[EDGE_NFET].w = config_get_int ("net.std_n_width") *
    ActNetlistPass::getGridsPerLambda();
  N->sz[EDGE_NFET].l = config_get_int ("net.std_n_length") *
    ActNetlistPass::getGridsPerLambda();
  N->sz[EDGE_NFET].sw = config_get_int ("net.stat_n_width") *
    ActNetlistPass::getGridsPerLambda();
  N->sz[EDGE_NFET].sl = config_get_int ("net.stat_n_length") *
    ActNetlistPass::getGridsPerLambda();
  
  N->sz[EDGE_NFET].nf = 1;
  N->sz[EDGE_NFET].flavor = 0;

  N->sz[EDGE_PFET].w = config_get_int ("net.std_p_width") *
    ActNetlistPass::getGridsPerLambda();
  N->sz[EDGE_PFET].l = config_get_int ("net.std_p_length") *
    ActNetlistPass::getGridsPerLambda();
  N->sz[EDGE_PFET].sw = config_get_int ("net.stat_p_width") *
    ActNetlistPass::getGridsPerLambda();
  N->sz[EDGE_PFET].sl = config_get_int ("net.stat_p_length") *
    ActNetlistPass::getGridsPerLambda();
  
  N->sz[EDGE_PFET].nf = 1;
  N->sz[EDGE_PFET].flavor = 0;

  /* set the current Vdd/GND/psc/nsc */
  if (cvdd) {
    N->Vdd = VINF(var_lookup (N, cvdd))->n;
  }
  else {
    N->Vdd = search_supply_list_for_null (N->vdd_list);
    if (!N->Vdd) {
      N->Vdd = node_alloc (N, NULL);
    }
  }
  if (cgnd) {
    N->GND = VINF(var_lookup (N, cgnd))->n;
  }
  else {
    /* no GND declared; allocate a new one; this one will be called `GND' */
    N->GND = search_supply_list_for_null (N->gnd_list);
    if (!N->GND) {
      N->GND = node_alloc (N, NULL);
    }
  }
  if (cpsc) {
    N->psc = VINF(var_lookup (N, cpsc))->n;
  }
  else {
    N->psc = N->GND;
  }
  if (cnsc) {
    N->nsc = VINF(var_lookup (N, cnsc))->n;
  }
  else {
    N->nsc = N->Vdd;
  }

  N->Vdd->supply = 1;
  N->GND->supply = 1;
  N->psc->supply = 1;
  N->nsc->supply = 1;

  append_node_to_list (N->vdd_list, N->Vdd);
  append_node_to_list (N->gnd_list, N->GND);
  append_node_to_list (N->psc_list, N->psc);
  append_node_to_list (N->nsc_list, N->nsc);
    
  N->Vdd->reff[EDGE_PFET] = 0;
  N->Vdd->reff_set[EDGE_PFET] = 1;

  N->GND->reff[EDGE_NFET] = 0;
  N->GND->reff_set[EDGE_NFET] = 1;
}

static netlist_t *_initialize_empty_netlist (act_boolean_netlist_t *bN)
{
  netlist_t *N;
  act_prs *p;
  Scope *cur;
  phash_iter_t it;
  phash_bucket_t *b;
  
  NEW (N, netlist_t);

  if (bN->p) {
    p = bN->p->getprs();
    cur = bN->p->CurScope();
  }
  else {
    p = ActNamespace::Global()->getprs();
    cur = ActNamespace::Global()->CurScope();
  }

  N->bN = bN;
  N->B = bool_init ();
  N->bN->visited = 0;
  N->weak_supply_vdd = 0;
  N->weak_supply_gnd = 0;
  N->vdd_len = 0;
  N->gnd_len = 0;
  A_INIT (N->instport_weak);

  N->hd = NULL;
  N->tl = NULL;
  N->devs = NULL;
  N->idnum = 0;
  
  N->atH[EDGE_NFET] = hash_new (2);
  N->atH[EDGE_PFET] = hash_new (2);

  N->vdd_list = list_new ();
  N->gnd_list = list_new ();
  N->psc_list = list_new ();
  N->nsc_list = list_new ();

  N->leak_correct = 0;

  /* clear extra flag! */
  phash_iter_init (bN->cH, &it);
  while ((b = phash_iter_next (bN->cH, &it))) {
    act_booleanized_var_t *v = (act_booleanized_var_t *) b->v;
    v->extra = NULL;
  }

  /* set Vdd/GND to be the first Vdd/GND there is */
  _set_current_supplies (N, p);

  return N;
}


void ActNetlistPass::generate_netgraph (netlist_t *N,
					int num_vdd_sharing,
					int num_gnd_sharing,
					int vdd_len,
					int gnd_len,
					node_t *weak_vdd,
					node_t *weak_gnd)
{
  act_prs *p; 
  Scope *cur;
  Process *proc;

  proc = N->bN->p;
  
  if (proc) {
    p = proc->getprs();
    cur = proc->CurScope();
  }
  else {
    /* global namespace */
    p = ActNamespace::Global()->getprs();
    cur = ActNamespace::Global()->CurScope();
  }

  Assert (cur == N->bN->cur, "Hmm");

  if (!p) {
    if (N->bN->p == _root) {
      /* flush any shared vdd/gnd nodes for subcircuits */
      if (weak_vdd) {
	_alloc_weak_vdd (N, weak_vdd, min_w_in_lambda, vdd_len);
      }
      if (weak_gnd) {
	_alloc_weak_gnd (N, weak_gnd, min_w_in_lambda, gnd_len);
      }
    }
    else {
      if (weak_vdd) {
	N->weak_supply_vdd = num_vdd_sharing;
	if (num_vdd_sharing > 0) {
	  N->vdd_len = vdd_len;
	  N->nid_wvdd = weak_vdd->i;
	}
      }
      if (weak_gnd) {
	N->weak_supply_gnd = num_gnd_sharing;
	if (num_gnd_sharing > 0) {
	  N->gnd_len = gnd_len;
	  N->nid_wgnd = weak_gnd->i;
	}
      } 
    }
    return;
  }

  /* walk through each PRS block */
  while (p) {
    /* add vdd and gnd */
    _set_current_supplies (N, p);
    if (p->leak_adjust) {
      N->leak_correct = 1;
    }

    for (act_prs_lang_t *prs = p->p; prs; prs = prs->next) {
      generate_prs_graph (N, prs);
      Assert (N->bN->isempty == 0, "Hmm");
    }
    p = p->next;
  }

  /* all production rules have been turned into a netlist! */

  /*-- compute r-effective on the netlist graph --*/
  compute_max_reff (N, EDGE_NFET);
  compute_max_reff (N, EDGE_PFET);

  /*-- generate staticizers --*/
  generate_staticizers (N, num_vdd_sharing, num_gnd_sharing,
			vdd_len, gnd_len, weak_vdd, weak_gnd);

  /*-- fold transistors --*/
  fold_transistors (N);

#if 0 
  release_atalloc (N->atH[EDGE_NFET]);
  release_atalloc (N->atH[EDGE_PFET]);
  hash_clear (N->atH[EDGE_PFET]);
  hash_clear (N->atH[EDGE_NFET]);
#endif

  int ports_exist = 0;
  for (int i=0; i < A_LEN (N->bN->ports); i++) {
    if (N->bN->ports[i].omit == 0) {
      ports_exist = 1;
      break;
    }
  }
  if (!ports_exist) {
    N->weak_supply_gnd = 0;
    N->weak_supply_vdd = 0;
  }

  return;
}

netlist_t *ActNetlistPass::genNetlist (Process *p)
{
  int sub_proc_vdd = 0, sub_proc_gnd = 0;
  int vdd_len = 0, gnd_len = 0;

  netlist_t *n = _initialize_empty_netlist (bools->getBNL (p));
  node_t *weak_vdd = NULL, *weak_gnd = NULL;

  /* handle all processes instantiated by this one */
  Scope *sc;
  if (p) {
    sc = p->CurScope();
  }
  else {
    sc = ActNamespace::Global()->CurScope();
  }

  /* Create netlist for all sub-processes */
  ActUniqProcInstiter i(sc);

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    int cnt = 1;
    netlist_t *tn = (netlist_t *)
      getMap (dynamic_cast<Process *>(vx->t->BaseType()));
    Assert (tn, "What?");

    /* count the # of shared weak drivers so far, and track the
       weakness of the weak supply */

    if (vx->t->arrayInfo()) {
      cnt = vx->t->arrayInfo()->size();
    }

    while (cnt > 0) {
      cnt--;

      if (vx->t->arrayInfo()) {
	if (!vx->isPrimary (cnt))
	  continue;
      }
      
      sub_proc_vdd += tn->weak_supply_vdd;
      if (tn->weak_supply_vdd > 0) {
	vdd_len = MAX (vdd_len, tn->vdd_len);
	if (!weak_vdd) {
	  weak_vdd = node_alloc (n, NULL);
	}
	A_NEW (n->instport_weak, int);
	A_NEXT (n->instport_weak) = weak_vdd->i;
	A_INC (n->instport_weak);
      }
      sub_proc_gnd += tn->weak_supply_gnd;
      if (tn->weak_supply_gnd > 0) {
	gnd_len = MAX (gnd_len, tn->gnd_len);
	if (!weak_gnd) {
	  weak_gnd = node_alloc (n, NULL);
	}
	A_NEW (n->instport_weak, int);
	A_NEXT (n->instport_weak) = weak_gnd->i;
	A_INC (n->instport_weak);
      }

      if (sub_proc_vdd > weak_share_max) {
	// emit a weak driver, and populate the port connections for
	// the instances!
	_alloc_weak_vdd (n, weak_vdd, min_w_in_lambda, vdd_len);
	sub_proc_vdd -= weak_share_max;
	vdd_len = tn->vdd_len;
	weak_vdd = NULL;
      }
	
      if (sub_proc_gnd > weak_share_max) {
	// emit a weak driver, and populate the port connections for
	// the instances!
	_alloc_weak_gnd (n, weak_gnd, min_w_in_lambda, gnd_len);
	sub_proc_gnd -= weak_share_max;
	gnd_len = tn->gnd_len;
	weak_gnd = NULL;
      }
    }
  }

  generate_netgraph (n, sub_proc_vdd, sub_proc_gnd,
		     vdd_len, gnd_len,
		     weak_vdd, weak_gnd);

  return n;
}

void *ActNetlistPass::local_op (Process *p, int mode)
{
  if (_annotate) {
    _annotate->setParam ("proc", (void *)p);
  }
  if (mode == 0) {
    return genNetlist (p);
  }
  else if (mode == 1) {
    return emitNetlist (p);
  }
  else if (mode == 2) {
    void *n = getMap (p);
    flatHelper (p);
    return n;
  }
  else if (mode == 3) {
    void *n = getMap (p);
    flatActHelper (p);
    return n;
  }
  return NULL;
}

void ActNetlistPass::free_local (void *v)
{
  netlist_t *n = (netlist_t *) v;
  if (!n) return;
  
  //bool_free (n->B); XXX write bdd clear routine
  
  list_free (n->vdd_list);
  list_free (n->gnd_list);
  list_free (n->psc_list);
  list_free (n->nsc_list);

  for (int k=0; k < 2; k++) {
    hash_iter_t iter;
    hash_bucket_t *b;
    hash_iter_init (n->atH[k], &iter);
    while ((b = hash_iter_next (n->atH[k], &iter))) {
      at_lookup *al = (at_lookup *)b->v;
      FREE (al);
    }
    hash_free (n->atH[k]);
  }

  node_t *tmp, *prev;

  for (tmp = n->hd; tmp; tmp = tmp->next) {
    for (listitem_t *li = list_first (tmp->e); li; li = list_next (li)) {
      edge_t *e = (edge_t *) list_value (li);
      e->visited = 0;
    }
  }

  tmp = n->hd;
  while (tmp) {
    prev = tmp;
    tmp = tmp->next;
    
    list_free (prev->wl);
    if (prev->v) {
      FREE (prev->v);
    }
    /* free edge the second time you see it */
    for (listitem_t *li = list_first (prev->e); li; li = list_next (li)) {
      edge_t *e = (edge_t *) list_value (li);
      if (e->visited) {
	FREE (e);
      }
      else {
	e->visited = 1;
      }
    }
    list_free (prev->e);
    FREE (prev);
  }
  FREE (n);
}

void ActNetlistPass::enableSharedStat ()
{
  if (config_exists ("net.weak_sharing")) {
    Assert (config_get_table_size ("net.weak_sharing") == 2, "weak sharing table incorrect");
    int *t = config_get_table_int ("net.weak_sharing");
    weak_share_min = t[0];
    weak_share_max = t[1];

    if (weak_share_max < weak_share_min) {
      fatal_error ("weak sharing table: range is empty!");
    }
  }
}

/*-- create a pass --*/
ActNetlistPass::ActNetlistPass (Act *a) : ActPass (a, "prs2net")
{
  current_act = a;
  /*-- automatically add this pass if it doesn't exist --*/
  if (!a->pass_find ("booleanize")) {
    ActBooleanizePass *bp = new ActBooleanizePass (a);
  }
  if (!a->pass_find ("sizing")) {
    ActSizingPass *sp = new ActSizingPass (a);
  }
  AddDependency ("booleanize");
  AddDependency ("sizing");

  ActPass *pass = a->pass_find ("booleanize");
  Assert (pass, "What?");

  bools = dynamic_cast <ActBooleanizePass *>(pass);
  Assert (bools, "Huh?");

  weak_share_min = 1;
  weak_share_max = 1;

  config_set_default_string ("net.fet_params.width", "W");
  config_set_default_string ("net.fet_params.length", "L");
  config_set_default_string ("net.fet_params.area_src", "AS");
  config_set_default_string ("net.fet_params.perim_src", "PS");
  config_set_default_string ("net.fet_params.area_drain", "AD");
  config_set_default_string ("net.fet_params.perim_drain", "PD");
  config_set_default_string ("net.fet_params.fin", "NFIN");

  default_load_cap = config_get_real ("net.default_load_cap");
  p_n_ratio = config_get_real ("net.p_n_ratio");
  weak_to_strong_ratio = config_get_real ("net.weak_to_strong_ratio");
  min_w_in_lambda = config_get_int ("net.min_width");
  min_l_in_lambda = config_get_int ("net.min_length");

  if (config_exists ("net.leakage_adjust")) {
    leak_adjust = config_get_real ("net.leakage_adjust");
  }
  else {
    leak_adjust = 0;
  }

  if (config_exists ("net.fin_width")) {
    _fin_width = config_get_int ("net.fin_width");
    if (_fin_width <= 0) {
      _fin_width = -1;
    }
  }
  else {
    _fin_width = -1;
  }
  if (_fin_width > 0) {
    /* sanity checking of min and standard w parameters */
    if ((min_w_in_lambda % _fin_width) != 0) {
      warning ("Minimum width (%d) is not a multiple of a fin (%d)?", min_w_in_lambda, _fin_width);
    }
    if ((config_get_int ("net.std_n_width") % _fin_width) != 0) {
      warning ("Standard n-transistor width (%d) is not a multiple of a fin (%d)?", config_get_int ("net.std_n_width"), _fin_width);
    }      
    if ((config_get_int ("net.std_p_width") % _fin_width) != 0) {
      warning ("Standard p-transistor width (%d) is not a multiple of a fin (%d)?", config_get_int ("net.std_p_width"), _fin_width);
    }      
    if ((config_get_int ("net.stat_n_width") % _fin_width) != 0) {
      warning ("Staticizer n-transistor width (%d) is not a multiple of a fin (%d)?", config_get_int ("net.stat_n_width"), _fin_width);
    }      
    if ((config_get_int ("net.stat_p_width") % _fin_width) != 0) {
      warning ("Staticizer p-transistor width (%d) is not a multiple of a fin (%d)?", config_get_int ("net.stat_p_width"), _fin_width);
    }      
  }
  

  local_vdd = config_get_string ("net.local_vdd");
  local_gnd = config_get_string ("net.local_gnd");

  global_vdd = config_get_string ("net.global_vdd");
  global_gnd = config_get_string ("net.global_gnd");

  n_fold = config_get_int ("net.fold_nfet_width");
  p_fold = config_get_int ("net.fold_pfet_width");
  discrete_len = config_get_int ("net.discrete_length");
  if (config_exists ("net.fet_length_ranges")) {
    discrete_fet_length = config_get_table_int ("net.fet_length_ranges");
    discrete_fet_length_sz = config_get_table_size ("net.fet_length_ranges");
    if ((discrete_fet_length_sz % 2) != 0) {
      warning ("Discrete fet length table should be even length; ignoring last value");
      discrete_fet_length_sz--;
    }
    for (int i=0; i < discrete_fet_length_sz; i++) {
      if (i == 0) continue;
      if (discrete_fet_length[i-1] > discrete_fet_length[i]) {
	fatal_error ("Error in netlist fet_length_ranges table: widths must be non-decreasing.");
      }
    }
  }
  else {
    discrete_fet_length_sz = 0;
    discrete_fet_length = NULL;
  }

  lambda = config_get_real ("net.lambda");
  if (config_exists ("lefdef.manufacturing_grid")) {
    manufacturing_grid = config_get_real ("lefdef.manufacturing_grid")*1e-6;
  }
  else {
    warning ("No lefdef.manufacturing_grid; using 0.5nm default.");
    /*-- agreement with layout generation --*/
    manufacturing_grid = 0.0005*1e-6;
  }

  grids_per_lambda = (int) ((lambda+0.1*manufacturing_grid)/manufacturing_grid);

  if (fabs (grids_per_lambda*manufacturing_grid - lambda) > 1e-6) {
    fatal_error ("lambda (%g) must be an integer multiple of the manufacturing grid (%g)\n", lambda, manufacturing_grid);
  }

  _fin_width *= grids_per_lambda;

  ignore_loadcap = config_get_int ("net.ignore_loadcap");
  emit_parasitics = config_get_int ("net.emit_parasitics");
  if (config_exists ("net.output_scale_factor")) {
    output_scale_factor = config_get_real ("net.output_scale_factor");
  }
  else {
    output_scale_factor = 1;
  }
  
  fet_spacing_diffonly = config_get_int ("net.fet_spacing_diffonly");
  fet_spacing_diffcontact = config_get_int ("net.fet_spacing_diffcontact");
  fet_diff_overhang = config_get_int ("net.fet_diff_overhang");
  
  use_subckt_models = config_get_int ("net.use_subckt_models");
  swap_source_drain = config_get_int ("net.swap_source_drain");
  extra_fet_string = config_get_string ("net.extra_fet_string");

  black_box_mode = config_get_int ("net.black_box_mode");
  if (config_exists ("net.top_level_only")) {
    top_level_only = config_get_int ("net.top_level_only");
  }
  else {
    top_level_only = 0;
  }

  max_n_w_in_lambda = config_get_int ("net.max_n_width");
  max_p_w_in_lambda = config_get_int ("net.max_p_width");

  if (config_exists ("net.series_n_warning")) {
    series_n_warning = config_get_int ("net.series_n_warning");
  }
  else {
    series_n_warning = 0;
  }
  
  if (config_exists ("net.series_p_warning")) {
    series_p_warning = config_get_int ("net.series_p_warning");
  }
  else {
    series_p_warning = 0;
  }

  if (config_exists ("net.unit_dev")) {
    unit_dev = config_get_real ("net.unit_dev");
  }
  else {
    unit_dev = 1e-15;
  }
  _annotate = NULL;

  if (config_exists ("net.mangled_ports_actflat")) {
    mangled_ports_actflat = config_get_int ("net.mangled_ports_actflat") ? true : false;
  }
  else {
    mangled_ports_actflat = true;
  }

  param_names.w = config_get_string ("net.fet_params.width");
  param_names.l = config_get_string ("net.fet_params.length");
  param_names.as = config_get_string ("net.fet_params.area_src");
  param_names.ps = config_get_string ("net.fet_params.perim_src");
  param_names.ad = config_get_string ("net.fet_params.area_drain");
  param_names.pd = config_get_string ("net.fet_params.perim_drain");
  param_names.fin = config_get_string ("net.fet_params.fin");
}

ActNetlistPass::~ActNetlistPass()
{
  bools = NULL;
}
  
int ActNetlistPass::run(Process *p)
{
  return ActPass::run (p);
}

netlist_t *ActNetlistPass::getNL (Process *p)
{
  if (!completed ()) {
    fatal_error ("ActNetlistPass::getNL() called without pass being run!");
  }
  return (netlist_t *) getMap (p);
}

void ActNetlistPass::spice_to_act_name (char *s, char *t, int sz, int xconv)
{
  char buf[10240];
  int i = 0;
  int possible_x = xconv;
  char *tmp;
  int countdots = 0;

  tmp = s;
  while (*tmp) {
    if (*tmp == '.') countdots++;
    tmp++;
  }

  while (*s) {
    if (possible_x && *s == 'x') {
      possible_x = 0;
    }
    else {
      buf[i] = *s;
      i++;
      if (i == 10240) fatal_error ("Resize the buffer");
      buf[i] = '\0';
      if (*s == '.') {
	countdots--;
	if (countdots == 0) {
	  possible_x = 0;
	}
	else {
	  possible_x = xconv;
	}
      }
      else {
	possible_x = 0;
      }
    }
    s++;
  }
  ActNamespace::Global()->Act()->unmangle_string (buf, t, sz);
}


void ActNetlistPass::_check_emit_warning (int d, int depth, ActId *id)
{
  if (d == EDGE_NFET && series_n_warning) {
    if (depth >= series_n_warning) {
      act_error_ctxt (stderr);
      warning ("PRS series nfet length %d >= threshold (%d)", depth, series_n_warning);
      fprintf (stderr, "Rule for: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
    }
  }
  else if (d == EDGE_PFET && series_p_warning) {
    if (depth >= series_p_warning) {
      act_error_ctxt (stderr);
      warning ("PRS series pfet length %d >= threshold (%d)", depth, series_p_warning);
      fprintf (stderr, "Rule for: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
    }
  }
}

bool ActNetlistPass::split_net (char *s)
{
  if (!current_annotate) {
    return false;
  }
  current_annotate->setParam ("net", (void *)s);
  if (current_annotate->runcmd ("split-net")) {
    return true;
  }
  return false;
}
  
