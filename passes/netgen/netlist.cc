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
#include <string.h>
#include "netlist.h"
#include "hash.h"
#include "qops.h"
#include "bitset.h"
#include "config.h"
#include <act/iter.h>

/* minimum transistor size */
static int min_w_in_lambda;
static int min_l_in_lambda;

/* strength ratios */
static double p_n_ratio;
static double weak_to_strong_ratio;

/* load cap */
static double default_load_cap;

/* local and global Vdd/GND */
const char *local_vdd, *local_gnd, *global_vdd, *global_gnd;

static std::map<Process *, netlist_t *> *netmap = NULL;
static std::map<Process *, act_boolean_netlist_t *> *boolmap = NULL;

#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* for maximum acyclic path algorithm */
typedef struct {
  bitset_t *b; 			/* list of nodes visited
				   (excluding power supply) */
  double reff;			/* reff through this path */
} path_t;

static void set_fet_params (netlist_t *n, edge_t *f, unsigned int type,
			    act_size_spec_t *sz);
static void create_expr_edges (netlist_t *N, int type, node_t *left,
			       act_prs_expr_t *e, node_t *right, int sense = 0);


#define VINF(x) ((struct act_varinfo *)((x)->extra))

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
  for (int i=0; i < N->bN->cH->size; i++)
    for (b = N->bN->cH->head[i]; b; b = b->next) {
      act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;
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
};

static at_lookup *at_alloc (void)
{
  at_lookup *a;
  NEW (a, at_lookup);
  a->n = NULL;
  a->e = NULL;
  return a;
}

/*-- variables --*/
static void emit_node (netlist_t *N, FILE *fp, node_t *n);

static node_t *node_alloc (netlist_t *n, struct act_varinfo *vi)
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
  struct act_varinfo *vi;
  
  NEW (vi, struct act_varinfo);
  vi->e_up = NULL;
  vi->e_dn = NULL;
  vi->b = bool_newvar (n->B);
  vi->up = bool_false (n->B);
  vi->dn = bool_false (n->B);

  vi->n = node_alloc (n, vi);

  vi->vdd = NULL;
  vi->gnd = NULL;

  vi->unstaticized = 0;
  vi->stateholding = 0;
  vi->usecf = 0;
  vi->inv = NULL;

  vi->v = v;

  return vi;
}

static act_booleanized_var_t *var_lookup (netlist_t *n, act_connection *c)
{
  ihash_bucket_t *b;
  act_booleanized_var_t *v;

  if (!c) return NULL;

  c = c->primary();

  b = ihash_lookup (n->bN->cH, (long)c);
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
  ihash_bucket_t *b;
  
  if (!c) return NULL;
  c = c->primary();
  b = ihash_lookup (n->bN->cH, (long)c);
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
#define EDGE_SIZE(t) ((t) & (0x3 << 1))
#define EDGE_MODE(t) ((t) & (0xf << 3))

/* these three size options are exclusive */
#define EDGE_NORMAL   (0x01 << 1)
#define EDGE_FEEDBACK (0x02 << 1)
#define EDGE_STATINV  (0x03 << 1)

/* flags */
#define EDGE_TREE     (0x01 << 3) // tree edges -- mark specially
#define EDGE_PCHG     (0x02 << 3) // pchg edges: can use n to vdd, p to gnd
#define EDGE_INVERT   (0x04 << 3) // strip pchg
#define EDGE_CELEM    (0x08 << 3) // strip pchg
#define EDGE_KEEPER   (0x10 << 3) // keeper edge (force)
#define EDGE_CKEEPER  (0x20 << 3) // ckeeper edge (force)


static void fold_transistors (netlist_t *N)
{
  node_t *n;
  listitem_t *li;
  edge_t *e;
  int n_fold = config_get_int ("net.fold_nfet_width");
  int p_fold = config_get_int ("net.fold_pfet_width");
  int discrete_len = config_get_int ("net.discrete_length");
  int fold;

  if (n_fold == 0 && p_fold == 0 && discrete_len == 0) return;

  for (n = N->hd; n; n = n->next) {
    for (li = list_first (n->e); li; li = list_next (li)) {
      e = (edge_t *) list_value (li);
      if (e->visited > 0) continue;
      e->visited = 1;
      if (e->type == EDGE_NFET) {
	fold = n_fold;
      }
      else {
	fold = p_fold;
      }
      if (fold > 0 && e->w > fold) {
	e->nfolds = e->w/fold;
	if ((e->w % fold) >= min_w_in_lambda) {
	  e->nfolds++;
	}
      }
      if (discrete_len > 0) {
	e->nlen = (e->l + discrete_len - 1)/discrete_len;
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


static void generate_staticizers (netlist_t *N)
{
  node_t *n;

  for (n = N->hd; n; n = n->next) {
    if (!n->v) continue;
    if (n->supply) continue;

    if (n->v->up == bool_false (N->B) && n->v->dn == bool_false (N->B)) {
      n->v->v->input = 1;
      continue;
    }

    if (n->v->unstaticized) {
      continue;
    }

    bool_t *v = bool_or (N->B, n->v->up, n->v->dn);
    if (v == bool_true (N->B)) {
      /* combinational */
      if (n->v->up == bool_var (N->B, bool_topvar (n->v->up)) ||
	  n->v->dn == bool_var (N->B, bool_topvar (n->v->up))) {
	/*- this is an inverter -*/

	Assert (n->v->e_up && n->v->e_dn, "What?");
	if (n->v->e_up->type == ACT_PRS_EXPR_VAR) {
	  act_booleanized_var_t *inv = var_lookup (N, n->v->e_up->u.v.id);
	  if (bool_var (N->B, bool_topvar (n->v->up)) == VINF(inv)->b) {
	    VINF(inv)->inv = n;
	  }
	  else {
	    fatal_error ("Complex inverter?");
	  }
	}
	else if (n->v->e_dn->type == ACT_PRS_EXPR_VAR) {
	  act_booleanized_var_t *inv = var_lookup (N, n->v->e_dn->u.v.id);
	  if (bool_var (N->B, bool_topvar (n->v->up)) == VINF(inv)->b) {
	    VINF(inv)->inv = n;
	  }
	  else {
	    fatal_error ("Complex inverter?");
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

  for (n = N->hd; n; n = n->next) {
    if (!n->v) continue;
    if (n->v->stateholding) {
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
      if (n->v->usecf) {
	/* combinational feedback */
	node_t *tmp;
	edge_t *e;

	/* pfets */
	tmp = node_alloc (N, NULL);
	e = edge_alloc (N, n->v->inv, tmp, n, N->nsc);
	e->type = EDGE_PFET;
	e->w = min_w_in_lambda;
	e->l = min_l_in_lambda;
	e->keeper = 1;
	e->combf = 1;
	create_expr_edges (N, EDGE_PFET|EDGE_FEEDBACK|EDGE_INVERT,
			   N->Vdd, n->v->e_dn, tmp, 1 /* invert */);

	/* nfets */
	tmp = node_alloc (N, NULL);
	e = edge_alloc (N, n->v->inv, tmp, n, N->psc);
	e->type = EDGE_NFET;
	e->w = min_w_in_lambda;
	e->l = min_l_in_lambda;
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
	  e->w = min_w_in_lambda;
	  e->l = min_l_in_lambda + len;
	  e->keeper = 1;
	}
	else {
	  /* two edges */
	  /* residual length conforming to rules */
	  node_t *tmp;

	  tmp = node_alloc (N, NULL); // tmp node

	  /* resistor */
	  e = edge_alloc (N, N->GND, N->Vdd, tmp, N->nsc);
	  e->type = EDGE_PFET;
	  e->w = min_w_in_lambda;
	  e->l = len;
	  e->keeper = 1;

	  /* inv */
	  e = edge_alloc (N, n->v->inv, tmp, n, N->nsc);
	  e->type = EDGE_PFET;
	  e->w = min_w_in_lambda;
	  e->l = min_l_in_lambda;
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
	  e->w = min_w_in_lambda;
	  e->l = min_l_in_lambda + len;
	  e->keeper = 1;
	}
	else {
	  /* two edges */
	  /* residual length conforming to rules */
	  node_t *tmp;

	  tmp = node_alloc (N, NULL); // tmp node

	  /* resistor */
	  e = edge_alloc (N, N->Vdd, N->GND, tmp, N->psc);
	  e->type = EDGE_NFET;
	  e->w = min_w_in_lambda;
	  e->l = len;
	  e->keeper = 1;

	  /* inv */
	  e = edge_alloc (N, n->v->inv, tmp, n, N->psc);
	  e->type = EDGE_NFET;
	  e->w = min_w_in_lambda;
	  e->l = min_l_in_lambda;
	  e->keeper = 1;
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

static void set_fet_params (netlist_t *n, edge_t *f, unsigned int type,
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
  if (EDGE_SIZE (type) == EDGE_NORMAL) {
    /* if type & EDGE_INVERT, could be => opp rule 
       if type & EDGE_CELEM, could be #> rule
    */
    if (sz) {
      f->flavor = sz->flavor;

      if (sz->w) {
	f->w = (sz->w->type == E_INT ? sz->w->u.v : sz->w->u.f);
	n->sz[f->type].w = f->w;
      }
      else {
	f->w = n->sz[f->type].w;
      }
      if (sz->l) {
	f->l = (sz->l->type == E_INT ? sz->l->u.v : sz->l->u.f);
	n->sz[f->type].l = f->l;
      }
      else {
	f->l = n->sz[f->type].l;
      }
    }
    else {
      f->w = n->sz[f->type].w;
      f->l = n->sz[f->type].l;
    }
    f->w = MAX(f->w, min_w_in_lambda);
    f->l = MAX(f->l, min_l_in_lambda);
  }
  else if (EDGE_SIZE (type) == EDGE_STATINV
	   || EDGE_SIZE(type) == EDGE_FEEDBACK) {
    /* use standard staticizer size; ignore sz field */
    f->w = n->sz[f->type].sw;
    f->l = n->sz[f->type].sl;
    f->flavor = 0;		/* standard fet */
  }
  else {
    /* min size, overridden by sz directive */
    if (sz) {
      f->flavor = sz->flavor;

      if (sz->w) {
	f->w = (sz->w->type == E_INT ? sz->w->u.v : sz->w->u.f);
      }
      else {
	f->w = min_w_in_lambda;
      }
      if (sz->l) {
	f->l = (sz->l->type == E_INT ? sz->l->u.v : sz->l->u.f);
      }
      else {
	f->l = min_l_in_lambda;
      }
    }
    else {
      f->w = min_w_in_lambda;
      f->l = min_l_in_lambda;
    }
  }
}

/*
  type == 0 or EDGE_CELEM
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
    at = hash_lookup (N->atH[EDGE_TYPE(type)], e->u.l.label);
    if (!at) {
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

/*
 * create edges from left to right, corresponding to expression "e"
 *
 *  sense = 0 : normal
 *  sense = 1 : complemented 
 *
 *  type has two parts: the EDGE_TYPE has EDGE_PFET or EDGE_NFET
 *                      the EDGE_SIZE piece (normal, feedback, small inv)
 */
static void create_expr_edges (netlist_t *N, int type, node_t *left,
			       act_prs_expr_t *e, node_t *right, int sense)
{
  node_t *mid;
  node_t *at_node;
  if (!e) return;

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
	    fatal_error ("@-expression with unknown label `%s'", e->u.e.l->u.l.label);
	  }
	  at_node = ((at_lookup *)b->v)->n;
	}
	else if (e->u.e.l->type == ACT_PRS_EXPR_NOT
		 && EDGE_TYPE (type) == EDGE_NFET
		 && e->u.e.l->u.e.l->type == ACT_PRS_EXPR_LABEL) {
	  b = hash_lookup (N->atH[EDGE_NFET], e->u.e.l->u.e.l->u.l.label);
	  if (!b) {
	    fatal_error ("@-expression with unknown label `%s'", e->u.e.l->u.e.l->u.l.label);
	  }
	  at_node = ((at_lookup *)b->v)->n;
	}
      }
      /* create edges recursively */
      if (!at_node) {
	mid = node_alloc (N, NULL);
	create_expr_edges (N, type, left, e->u.e.l, mid, sense);
	create_expr_edges (N, type, mid, e->u.e.r, right, sense);
      }
      else {
	mid = at_node;
	create_expr_edges (N, type, mid, e->u.e.r, right, sense);
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
    }
    else {
      /* it is an OR */
      create_expr_edges (N, type, left, e->u.e.l, right, sense);
      create_expr_edges (N, type, left, e->u.e.r, right, sense);
    }
    break;
    
  case ACT_PRS_EXPR_NOT:
    create_expr_edges (N, type, left, e->u.e.l, right, 1-sense);
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
    break;
      
  case ACT_PRS_EXPR_LABEL:
    fatal_error ("Label `%s' in a context where it was not recognized.",
		 e->u.l.label);
    break;
    
  case ACT_PRS_EXPR_TRUE:
    if (sense == 1) {
      /* done, disconnected */
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
    }
    break;
  case ACT_PRS_EXPR_FALSE:
    if (sense == 0) {
      /* left and right are disconnected; done */
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
    }
    break;
  default:
    fatal_error ("Unknown type");
    break;
  }
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

static void emit_node (netlist_t *N, FILE *fp, node_t *n)
{
  if (n->v) {
    ActId *id = n->v->v->id->toid();
    id->Print (fp);
    delete id;
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
	  c = vx->connection()->primary();
	  ActId *id  = c->toid();
	  id->Print (fp);
	  delete id;
	}
	else {
	  fprintf (fp, "%s", global_vdd);
	}
      }
      else {
	fprintf (fp, "%s", global_vdd);
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
	  c = vx->connection()->primary();
	  ActId *id  = c->toid();
	  id->Print (fp);
	  delete id;
	}
	else {
	  fprintf (fp, "%s", global_gnd);
	}
      }
      else {
	fprintf (fp, "%s", global_gnd);
      }
    }
    else {
      if (n->inv) {
	fprintf (fp, "#fb%d#", n->i);
      }
      else {
	fprintf (fp, "#%d", n->i);
      }
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
    b1 = compute_bool (N, e, 0, 1);
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
    b1 = compute_bool (N, e, 0, 0);
  }
  
  if (((EDGE_TYPE (type) == EDGE_NFET) && ((type & (EDGE_INVERT|EDGE_CELEM)) == 0))
      ||
      (EDGE_TYPE (type) == EDGE_PFET && ((type & (EDGE_INVERT|EDGE_CELEM)) != 0))) {
    /* pull-down, n-type */
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
    b2 = VINF(v)->dn;
    VINF(v)->dn = bool_or (N->B, b2, b1);
    bool_free (N->B, b1);
    bool_free (N->B, b2);
  }
  else {
    /* pull-up, p-type */
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
    b2 = VINF(v)->up;
    VINF(v)->up = bool_or (N->B, b2, b1);
    bool_free (N->B, b1);
    bool_free (N->B, b2);
  }
}

static void generate_prs_graph (netlist_t *N, act_prs_lang_t *p, int istree = 0)
{
  int d;
  if (!p) return;
  act_attr_t *attr;

  switch (p->type) {
  case ACT_PRS_RULE:
    d = (p->u.one.dir == 0 ? EDGE_NFET : EDGE_PFET);

    /*-- reset default sizes per production rule --*/
    N->sz[EDGE_NFET].w = config_get_int ("net.std_n_width");
    N->sz[EDGE_NFET].l = config_get_int ("net.std_n_length");
    N->sz[EDGE_PFET].w = config_get_int ("net.std_p_width");
    N->sz[EDGE_PFET].l = config_get_int ("net.std_p_length");

    if (p->u.one.label) {
      if (istree) {
	fatal_error ("@ variables cannot be within a tree { } block");
      }
      hash_bucket_t *b;
      b = hash_lookup (N->atH[d], (char *)p->u.one.id);
      if (b || hash_lookup (N->atH[1-d], (char *)p->u.one.id)) {
	fatal_error ("Duplicate label `%s'", (char *)p->u.one.id);
      }
      b = hash_add  (N->atH[d], (char *)p->u.one.id);
      at_lookup *a = at_alloc ();
      b->v = a;
      a->n = node_alloc (N, NULL);
      a->e = p->u.one.e;
      create_expr_edges (N, d | EDGE_NORMAL,
			 (d == EDGE_NFET ? N->GND : N->Vdd),
			 p->u.one.e, ((at_lookup *)b->v)->n, 0);

      b = hash_lookup (N->atH[d], (char *)p->u.one.id);
    }
    else {
      act_booleanized_var_t *v;
      double cap = default_load_cap;
      unsigned int attr_type = 0;

      v = var_lookup (N, p->u.one.id);
      Assert (v->output == 1, "eh?");

      for (attr = p->u.one.attr; attr; attr = attr->next) {
	/* look for keeper, iskeeper, isckeeper, loadcap, oresis,
	   output,  N_reff, P_reff, autokeeper, comb 
	*/
	if (strcmp (attr->attr, "keeper") == 0) {
	  if (attr->e->u.v == 0) {
	    VINF(v)->unstaticized = 1;
	  }
	  else {
	    VINF(v)->unstaticized = 0;
	  }
	}
	else if (strcmp (attr->attr, "iskeeper") == 0) {
	  if (attr->e->u.v) {
	    attr_type |= EDGE_KEEPER;
	  }
	}
	else if (strcmp (attr->attr, "isckeeper") == 0) {
	  if (attr->e->u.v) {
	    attr_type |= EDGE_CKEEPER;
	  }
	}
	else if (strcmp (attr->attr, "loadcap") == 0) {
	  cap = attr->e->u.f;
	}
	else if (strcmp (attr->attr, "oresis") == 0) {
	  /* do something here */
	}
	else if (strcmp (attr->attr, "N_reff") == 0) {
	  VINF(v)->n->reff[EDGE_NFET] = attr->e->u.f;
	  VINF(v)->n->reff_set[EDGE_NFET] = 1;
	}
	else if (strcmp (attr->attr, "P_reff") == 0) {
	  VINF(v)->n->reff[EDGE_PFET] = attr->e->u.f;
	  VINF(v)->n->reff_set[EDGE_PFET] = 1;
	}
	else if (strcmp (attr->attr, "comb") == 0) {
	  if (attr->e->u.v) {
	    VINF(v)->usecf = 1;
	  }
	  else {
	    VINF(v)->usecf = 0;
	  }
	}
      }
      
      VINF(v)->n->cap = cap;

      if (p->u.one.arrow_type == 0) {
	/* -> */
	create_expr_edges (N, d | attr_type | EDGE_NORMAL | (istree ? EDGE_TREE : 0),
			   (d == EDGE_NFET ? N->GND : N->Vdd),
			   p->u.one.e, VINF(v)->n, 0);
	update_bdds_exprs (N, v, p->u.one.e, d|EDGE_NORMAL);
	check_supply (N, p->u.one.id, d, (d == EDGE_NFET ? N->GND : N->Vdd));
      }
      else if (p->u.one.arrow_type == 1) {
	if (istree) {
	  fatal_error ("tree { } blocks can only contain `->' production rules");
	}
	/* => */
	create_expr_edges (N, d | attr_type | EDGE_NORMAL,
			   (d == EDGE_NFET ? N->GND : N->Vdd),
			   p->u.one.e, VINF(v)->n, 0);
	update_bdds_exprs (N, v, p->u.one.e, d|EDGE_NORMAL);
	create_expr_edges (N, (1-d) | attr_type | EDGE_INVERT | EDGE_NORMAL,
			   (d == EDGE_NFET ? N->Vdd : N->GND),
			   p->u.one.e, VINF(v)->n, 1);
	update_bdds_exprs (N, v, p->u.one.e, d|EDGE_NORMAL|EDGE_INVERT);
	check_supply (N, p->u.one.id, EDGE_NFET, N->GND);
	check_supply (N, p->u.one.id, EDGE_PFET, N->Vdd);
      }
      else {
	Assert (p->u.one.arrow_type == 2, "???");
	if (istree) {
	  fatal_error ("tree { } blocks can only contain `->' production rules");
	}
	/* #> */
	create_expr_edges (N, d | attr_type | EDGE_NORMAL,
			   (d == EDGE_NFET ? N->GND : N->Vdd),
			   p->u.one.e, VINF(v)->n, 0);
	update_bdds_exprs (N, v, p->u.one.e, d | EDGE_NORMAL);
	create_expr_edges (N, (1-d) | attr_type | EDGE_CELEM | EDGE_NORMAL,
			   (d == EDGE_NFET ? N->Vdd : N->GND),
			   p->u.one.e, VINF(v)->n, 0);
	update_bdds_exprs (N, v, p->u.one.e, d | EDGE_CELEM | EDGE_NORMAL);
	check_supply (N, p->u.one.id, EDGE_NFET, N->GND);
	check_supply (N, p->u.one.id, EDGE_PFET, N->Vdd);
      }
    }
    break;

  case ACT_PRS_GATE:
    /*-- reset default sizes per gate --*/
    N->sz[EDGE_NFET].w = config_get_int ("net.std_n_width");
    N->sz[EDGE_NFET].l = config_get_int ("net.std_n_length");
    N->sz[EDGE_PFET].w = config_get_int ("net.std_p_width");
    N->sz[EDGE_PFET].l = config_get_int ("net.std_p_length");
    for (attr = p->u.p.attr; attr; attr = attr->next) {
      if (strcmp (attr->attr, "output") == 0) {
	unsigned int v = attr->e->u.v;
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
			  p->u.l.lo ? p->u.l.lo->u.v : 4);
    tree_compute_sharing (N, N->Vdd, EDGE_PFET,
			  p->u.l.lo ? p->u.l.lo->u.v : 4);
    break;
    
  case ACT_PRS_SUBCKT:
    /* handle elsewhere */
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
  for (i=0; i < H->size; i++) {
    for (b = H->head[i]; b; b = b->next) {
      if (b->v) {
	FREE (b->v);
      }
      b->v = NULL;
    }
  }
}

static netlist_t *generate_netgraph (Act *a, Process *proc)
{
  act_prs *p; 
  netlist_t *N;
  Scope *cur;

  if (proc) {
    p = proc->getprs();
    cur = proc->CurScope();
  }
  else {
    /* global namespace */
    p = ActNamespace::Global()->getprs();
    cur = ActNamespace::Global()->CurScope();
  }

  NEW (N, netlist_t);

  N->bN = boolmap->find(proc)->second;

  N->B = bool_init ();

  Assert (proc == N->bN->p, "Hmm");
  Assert (cur == N->bN->cur, "Hmm");

  N->bN->visited = 0;

  N->hd = NULL;
  N->tl = NULL;
  N->idnum = 0;
  
  N->atH[EDGE_NFET] = hash_new (2);
  N->atH[EDGE_PFET] = hash_new (2);

  N->vdd_list = list_new ();
  N->gnd_list = list_new ();
  N->psc_list = list_new ();
  N->nsc_list = list_new ();

  if (!p) {
    return N;
  }

  /* walk through each PRS block */
  while (p) {
    /* add vdd and gnd */
    act_connection *cvdd, *cgnd, *cpsc, *cnsc;
    act_prs_lang_t *prs;

    cvdd = NULL;
    cgnd = NULL;
    cpsc = NULL;
    cnsc = NULL;

    if (p->vdd) cvdd = p->vdd->Canonical(cur);
    if (p->gnd) cgnd = p->gnd->Canonical(cur);
    if (p->psc) cpsc = p->psc->Canonical(cur);
    if (p->nsc) cnsc = p->nsc->Canonical(cur);

    N->sz[EDGE_NFET].w = config_get_int ("net.std_n_width");
    N->sz[EDGE_NFET].l = config_get_int ("net.std_n_length");
    N->sz[EDGE_NFET].sw = config_get_int ("net.stat_n_width");
    N->sz[EDGE_NFET].sl = config_get_int ("net.stat_n_length");

    N->sz[EDGE_PFET].w = config_get_int ("net.std_p_width");
    N->sz[EDGE_PFET].l = config_get_int ("net.std_p_length");
    N->sz[EDGE_PFET].sw = config_get_int ("net.stat_p_width");
    N->sz[EDGE_PFET].sl = config_get_int ("net.stat_p_length");

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

    for (prs = p->p; prs; prs = prs->next) {
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
  generate_staticizers (N);

  /*-- fold transistors --*/
  fold_transistors (N);

#if 0 
  release_atalloc (N->atH[EDGE_NFET]);
  release_atalloc (N->atH[EDGE_PFET]);
  hash_clear (N->atH[EDGE_PFET]);
  hash_clear (N->atH[EDGE_NFET]);
#endif
  
  return N;
}

static void generate_netlist (Act *a, Process *p)
{
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
      generate_netlist (a, dynamic_cast<Process *>(vx->t->BaseType()));
    }
  }
  netlist_t *n = generate_netgraph (a, p);
  (*netmap)[p] = n;
  return;
}


void act_prs_to_netlist (Act *a, Process *p)
{
  std::map<Process *, netlist_t *> *tmp;

  boolmap = (std::map<Process *, act_boolean_netlist_t *> *) a->aux_find ("booleanize");
  
  if (!boolmap) {
    fatal_error ("act_prs_to_netlist called without booleanize pass");
  }

  tmp = (std::map<Process *, netlist_t *> *) a->aux_find ("prs2net");
  if (tmp) {
    return;
    delete tmp;
  }
  netmap = new std::map<Process *, netlist_t *>();

  default_load_cap = config_get_real ("net.default_load_cap");
  p_n_ratio = config_get_real ("net.p_n_ratio");
  weak_to_strong_ratio = config_get_real ("net.weak_to_strong_ratio");
  min_w_in_lambda = config_get_int ("net.min_width");
  min_l_in_lambda = config_get_int ("net.min_length");

  local_vdd = config_get_string ("net.local_vdd");
  local_gnd = config_get_string ("net.local_gnd");

  global_vdd = config_get_string ("net.global_vdd");
  global_gnd = config_get_string ("net.global_gnd");

  if (!p) {
    ActNamespace *g = ActNamespace::Global();
    ActInstiter i(g->CurScope());

    for (i = i.begin(); i != i.end(); i++) {
      ValueIdx *vx = *i;
      if (TypeFactory::isProcessType (vx->t)) {
	Process *x = dynamic_cast<Process *>(vx->t->BaseType());
	if (x->isExpanded()) {
	  generate_netlist (a, x);
	}
      }
    }
    
    /*-- generate netlist for any prs in the global scope --*/
    netlist_t *N = generate_netgraph (a, NULL);
    (*netmap)[NULL] = N;
  }
  else {
    generate_netlist (a, p);
  }

  a->aux_add ("prs2net", netmap);

  netmap = NULL;
  boolmap = NULL;
}
