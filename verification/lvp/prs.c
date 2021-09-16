/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

/*
 *
 * Generate production rules from transistor graph
 *
 */

#include <stdio.h>
#include "lvs.h"
#include "parse.h"
#include <common/misc.h>
#include "hier.h"

static int missing_layout_connects;
static int missing_prs_connects;
static int missing_layout;
static int missing_prs = 0;
static int missing_staticizers = 0;
static int extra_staticizers = 0;
static int bad_staticizers = 0;
static int weak_drivers = 0;
static int prs_differences = 0;
static int sneak_paths = 0;
static int naming_violations = 0;
static int io_violations = 0;
static int missing_global_connects = 0;

static int chargesharing_errs = 0;

static int known_missing_layout = 0;
static int known_missing_staticizers = 0;

static
int is_a_global (char *s)
{
  while (*s && *s != '!') s++;
  if (*s == '!') return 1;
  else return 0;
}

static
void mark_globals (VAR_T *V)
{
  var_t *u, *v;

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    u = canonical_name (v);
    if (is_a_global (v->s) || v->name_convert.globalnode)
      u->flags |= VAR_GLOBALNODE;
  }
}

void inc_sneak_paths (void)
{
  sneak_paths++;
  exit_status = 1;
}

void inc_chargesharing_errs (void)
{
  chargesharing_errs++;
  exit_status = 1;
}

void inc_naming_violations (void)
{
  naming_violations ++;
  exit_status = 1;
}

static int pchg_errors = 0;

void inc_pchg_errors (void)
{
  pchg_errors ++;
  exit_status = 1;
}

static bool_t *InvariantHi, *InvariantLo;

var_t *ResetVar, *_ResetVar;

void find_feedback_inverter (VAR_T *V, BOOL_T *B,
			     var_t *v, var_t **vp, var_t *out, int type);

/*------------------------------------------------------------------------
 *
 *   Canonicalize a name
 *
 *------------------------------------------------------------------------
 */
static
var_t *prs_canonical_name (var_t *v)
{
  var_t *o = v, *root;

  while (v->alias_prs)
    v = v->alias_prs;
  root = v;
  if (root != o) {
    while (o->alias_prs) {
      v = o->alias_prs;
      o->alias_prs = root;
      o = v;
    }
  }
  return root;
}


static
int list_member (struct excl *hd, var_t *v)
{
  while (hd) {
    if (canonical_name(v) == canonical_name(hd->v)) return 1;
    hd = hd->next;
  }
  return 0;
}

/*------------------------------------------------------------------------
 *
 *  Find Reset, Reset_ variables in the circuit
 *
 *------------------------------------------------------------------------
 */
static
void find_reset (VAR_T *V, var_t **reset, var_t **reset_)
{
  if (!(*reset_ = var_locate (V, "Reset_")) &&
      !(*reset_ = var_locate (V, "Reset_!")) &&
      !(*reset_ = var_locate (V, "_Reset")) &&
      !(*reset_ = var_locate (V, "_Reset!")))
    ;
  if (!(*reset = var_locate (V, "Reset")) &&
      !(*reset = var_locate (V, "Reset!")))
    ;
}

/*------------------------------------------------------------------------
 * 
 *  is_strong --
 *
 *    Returns a "1" (true) or "0" (false) if a transistor is considered
 *    strong or weak.
 *
 *------------------------------------------------------------------------
 *
 */
static
int is_strong (int width, int length)
{
  double ratio = (double)width/(double)length;

  if ((strip_by_width == 0 && ratio >= strip_threshold) ||
      (strip_by_width == 1 && width >= width_threshold))
    return 1;
  else
    return 0;
}


/*------------------------------------------------------------------------
 *
 * Mark output variables. A variable is of type output if it is connected
 * to an N-type transistor and a P-type transistor source/drain.
 *
 *------------------------------------------------------------------------
 */
static void
mark_prs_output (VAR_T *V)
{
  var_t *v, *u, *w;
  edgelist_t *e;
  int type;
  int flag;

  if (debug_level > 40) {
    pp_puts (PPout, "VARIABLE FLAGS:"); pp_forced (PPout,0);
  }
  for (v = var_step_first (V); v; v = var_step_next (V)) {
    u = canonical_name (v);
    if (u != v) continue;
    if (outputs_by_name) {
      /* a node is an output if it has a name */
      w = u;
      do {
	if (var_string(w)[strlen(var_string(w))-1] != '#' &&
	    var_string(w)[strlen(var_string(w))-1] != '&') {
	  u->flags |= VAR_OUTPUT;
	  break;
	}
	w = w->alias_ring;
      } while (u != w);
    }
    e = u->edges; type = -1; flag = 0;
    /* a node is an output if it is connected to both a pull-up
       and a pull-down */
    while (e) {
      if (e->type != type)
	flag++;
      type = e->type;
      e = e->next;
    }
    if (flag > 1) {
      if (!outputs_by_name)
	u->flags |= VAR_OUTPUT|VAR_DRIVENOUTPUT;
      else
	u->flags |= VAR_DRIVENOUTPUT;
    }
    if (and_hack) {
#ifdef OLD_AND_HACK
      w = u;
      do {
	if (var_string (w)[strlen(var_string(w))-1] == '&') {
	  u->flags &= ~VAR_OUTPUT;
	  break;
	}
	w = w->alias_ring;
      } while (w != u);
#else
      if (u->flags2 & VAR_PCHG_ALL)
	u->flags &= ~VAR_OUTPUT;
#endif
    }
    if (debug_level > 40 && (u->flags & VAR_OUTPUT)) {
      pp_printf (PPout, "%s: output", var_name(v));
      pp_forced (PPout, 0);
    }
  }
}

static
void clearflags (VAR_T *V)
{
  var_t *v;

  for (v=var_step_first (V); v; v = var_step_next (V)) {
    v->flags &= ~VAR_VISITED;
  }
}


/*------------------------------------------------------------------------
 *
 *  Search thru transistor graph, generating production rules
 *
 *------------------------------------------------------------------------
 */
static
void compute_prs (VAR_T *V, BOOL_T *B,
		  var_t *supply, var_t *othersupply, int type)
{
  var_t *hd = NULL, *tl = NULL;
  edgelist_t *e;
  bool_t *x, *y, *node, *gate;
  var_t *chan, *vtmp;
  int which;
  bool_t *t1, *t2, *t3;

  clearflags (V);
  hd = supply;
  tl = supply;
  do {
    /* walk through everything in the edgelist; if node changes, add to
       the edgelist
    */
    e = hd->edges;
    /* TRAVERSE EDGES; if the node is an output node, snip */
    if (!(hd->flags & VAR_OUTPUT) || pass_gates) {
      while (e) {
	if (e->type == type) {
	  if (is_strong (e->width, e->length))
	    which = STRONG;		/* strong */
	  else
	    which = WEAK;		/* weak */

#if 0
	  /* pass only thru strong transistors */
	  if ((hd->flags & VAR_OUTPUT) && which == 2) {
	    e = e->next;
	    continue;
	  }
#endif

	  if (type == P_TYPE) {
	    if (!hd->up[which])
	      hd->up[which] = bool_false (B);
	    node = bool_copy (B, (bool_t*)hd->up[which]);
	  }
	  else {
	    if (!hd->dn[which])
	      hd->dn[which] = bool_false (B);
	    node = bool_copy (B, (bool_t*)hd->dn[which]);
	  }

	  if (type == P_TYPE)
	    x = (bool_t*)e->t1->up[which];
	  else
	    x = (bool_t*)e->t1->dn[which];

	  if (debug_level > 50) {
	    pp_printf (PPout, "Examining: [%c] %s -> %s, gate=%s, w=%d, l=%d, pass=%d",
		    type == P_TYPE ? 'p' : 'n',
		    var_name (hd), var_name (e->t1), var_name (e->gate),
		    e->width, e->length,
		    (hd->flags & VAR_OUTPUT) ? 1 : 0);
	    pp_forced (PPout, 0);
	  }

	  /* if the node is weak or the edge is weak, make the other node
	     weak too . . .
	   */
	  if ((type == N_TYPE && (hd->flags & VAR_WEAKDN)) ||
	      (type == P_TYPE && (hd->flags & VAR_WEAKUP)) ||
	      e->isweak) {
	    e->t1->flags |= (type == N_TYPE) ? VAR_WEAKDN : VAR_WEAKUP;
	  }

	  /* check if gate is part of a channel, then look at the
	     rest of the things in the channel to see if the node
	     expression already has a series transistor which corresponds
	     to a channel variable */

	  y = bool_false (B);
	  if (type == N_TYPE) {
	    for (chan=e->gate->exclhi; chan != e->gate; chan = chan->exclhi) {
	      vtmp = canonical_name (chan);
	      if (!(vtmp->flags & VAR_HAVEVAR)) {
		vtmp->flags |= VAR_HAVEVAR;
		vtmp->v = bool_topvar (t1 = bool_newvar (B));
		bool_free (B, t1);
	      }
	      y = bool_or (B, t1 = y, t2 = bool_var (B, vtmp->v));
	      bool_free (B, t1);
	      bool_free (B, t2);
	    }
	    y = bool_or (B, t1 = y, t2 = bool_not (B, node));
	    bool_free (B, t1);
	    bool_free (B, t2);
	  }
	  else {
	    for (chan=e->gate->excllo; chan != e->gate; chan = chan->excllo) {
	      vtmp = canonical_name (chan);
	      if (!(vtmp->flags & VAR_HAVEVAR)) {
		vtmp->flags |= VAR_HAVEVAR;
		vtmp->v = bool_topvar (t1 = bool_newvar (B));
		bool_free (B, t1);
	      }
	      y = bool_or (B, t1=y, t2=bool_not (B, t3=bool_var (B, vtmp->v)));
	      bool_free (B, t1);
	      bool_free (B, t2);
	      bool_free (B, t3);
	    }
	    y = bool_or (B, t1 = y, t2 = bool_not (B, node));
	    bool_free (B, t1); 
	    bool_free (B, t2);
	  }

	  if (y != bool_true (B)) {
	    /* create disjunct node & gate  */
	    if (!(e->gate->flags & VAR_HAVEVAR)) {
	      e->gate->flags |= VAR_HAVEVAR;
	      e->gate->v = bool_topvar (t1 = bool_newvar (B));
	      bool_free (B, t1);
	    }
	    if (type == P_TYPE) {
	      gate = bool_not (B, t1 = bool_var (B, e->gate->v));
	      bool_free (B, t1);
	    }
	    else
	      gate = bool_var (B, e->gate->v);
	    if (e->gate != othersupply) {
	      y = bool_and (B, t1 = gate, node);
	      bool_free (B, t1);
	      bool_free (B, node);
	    }
	    else {
	      bool_free (B, gate);
	      y = node;
	    }
	    if (x != NULL) {
	      y = bool_or (B, x, t1 = y);
	      bool_free (B, t1);
	    }
	    if (y != x) {
	      if (e->t1 != othersupply) {
		/* do something about it */
		if (type == P_TYPE) {
		  if (e->t1->up[which])
		    bool_free (B, e->t1->up[which]);
		  e->t1->up[which] = (void*)y;
		}
		else {
		  if (e->t1->dn[which])
		    bool_free (B, e->t1->dn[which]);
		  e->t1->dn[which] = (void*)y;
		}
		/* add "t1" to the list of things to work on */
		if (!(e->t1->flags & VAR_VISITED)) {
		  tl->worklist = e->t1;
		  tl = e->t1;
		  tl->worklist = NULL;
		  tl->flags |= VAR_VISITED;
		}
	      }
	    }
	    else {
	      bool_free (B, y);
	    }
	  }
	}
	e = e->next;
      }
    }
    hd->flags &= ~VAR_VISITED;
    chan = hd;
    hd = hd->worklist;
    chan->worklist = NULL;
    bool_gc (B);
  } while (hd);
}


static
void canonicalize_expr (expr_t *e)
{
  if (!e) return;
  switch (e->type) {
  case E_AND:
  case E_OR:
    canonicalize_expr (e->l); canonicalize_expr (e->r);
    break;
  case E_NOT:
    canonicalize_expr (e->l);
    break;
  case E_TRUE:
  case E_FALSE:
    break;
  case E_ID:
    e->l = (expr_t*)canonical_name ((var_t*)e->l);
    break;
  }
}

/*------------------------------------------------------------------------
 *
 *  elim_const_nodes --
 *
 *    Eliminate power supply nodes from an excl list
 *
 *------------------------------------------------------------------------
 */
static void elim_const_nodes (var_t *v, var_t *vdd, var_t *gnd)
{
  var_t *t, *prev;
  int count;

  if (vdd && v->excllo != v) {
    count = 1;
    for (prev = v; prev->excllo != v; prev = prev->excllo) count++;
    t = v;
    while (count >= 0) {
      if (canonical_name (t) == canonical_name (vdd)) {
	/* snip "t" from the excl list */
	prev->excllo = t->excllo;
	t->excllo = t;
	t = prev->excllo;
      }
      else {
	prev = t;
	t = t->excllo;
      }
      count--;
    }
  }

  if (gnd && v->exclhi != v) {
    count = 1;
    for (prev = v; prev->exclhi != v; prev = prev->exclhi) count++;
    t = v;
    while (count >= 0) {
      if (canonical_name (t) == canonical_name (gnd)) {
	/* snip "t" from the excl list */
	prev->exclhi = t->exclhi;
	t->exclhi = t;
	t = prev->exclhi;
      }
      else {
	prev = t;
	t = t->exclhi;
      }
      count--;
    }
  }
}

/*------------------------------------------------------------------------
 *
 *  1 if the two nodes have compatible exclusive-hi/lo lists. The nodes 
 *  are compatible any lists they have in common are identical.
 *
 *------------------------------------------------------------------------
 */
static int compatible_excl_lists (var_t *v1, var_t *v2, var_t *vdd, var_t *gnd)
{
  int c1, c2, flag;
  var_t *t1, *t2;
  var_t *i1, *i2;
  int retval = 1;

  if (v1->excllo != v1 && v2->excllo != v2) {
    c1 = c2 = 0;
    t1 = v1;
    do {
      if (!vdd || canonical_name (t1) != canonical_name (vdd))
	c1++;
      t1 = t1->excllo;
    } while (t1 != v1);
    t2 = v2;
    do {
      if (!vdd || canonical_name (t2) != canonical_name (vdd))
	c2++;
      t2 = t2->excllo;
    } while (t2 != v2);
    if (c2 < c1) {
      retval |= 2;
      t1 = v1;
      v1 = v2;
      v2 = t1;
    }
    t1 = v1;
    do {
      /* look for t1 in v2 */
      if (!vdd || canonical_name (t1) != canonical_name (vdd)) {
	flag = 0;
	t2 = v2;
	do {
	  if (canonical_name(t2) == canonical_name(t1)) flag = 1;
	  t2 = t2->excllo;
	} while (flag == 0 && t2 != v2);
	if (flag == 0) 
	  return 0;
      }
      t1 = t1->excllo;
    } while (t1 != v1);
  }
  if (v1->exclhi != v1 && v2->exclhi != v2) {
    c1 = c2 = 0;
    t1 = v1;
    do {
      if (!gnd || canonical_name (t1) != canonical_name (gnd))
	c1++;
      t1 = t1->exclhi;
    } while (t1 != v1);
    t2 = v2;
    do {
      if (!gnd || canonical_name (t2) != canonical_name (gnd))
	c2++;
      t2 = t2->exclhi;
    } while (t2 != v2);
    if (c2 < c1) {
      t1 = v1;
      v1 = v2;
      v2 = t1;
      if (!(retval & 2))
	retval |= 4;
    }
    else {
      if (retval & 2)
	retval |= 4;
    }
    t1 = v1;
    do {
      /* look for t1 in v2 */
      if (!gnd || canonical_name (t1) != canonical_name (gnd)) {
	flag = 0;
	t2 = v2;
	do {
	  if (canonical_name(t2) == canonical_name(t1)) flag = 1;
	  t2 = t2->exclhi;
	} while (flag == 0 && t2 != v2);
	if (flag == 0)
	  return 0;
      }
      t1 = t1->exclhi;
    } while (t1 != v1);
  }
  return retval;
}

/*------------------------------------------------------------------------
 *
 *  Adjust aliases:
 *
 *     1. propagate all flags to the root of the alias list;
 *         *** erase intermediate variable flags as you do this.
 *
 *     2. canonicalize all edges.
 *
 *     3. check that exactly one of the vars has a production rule in it.
 *
 *     4. move prs to the root.
 *
 *     5. canonicalize excl lists.
 *
 *------------------------------------------------------------------------
 */
static
void fix_aliases (VAR_T *V)
{
  var_t *v, *t, *root, *tmp;
  var_t *err[2];
  int flags, flags2;
  edgelist_t *edge, *prevedge;
  int prs_found;
  expr_t *e;
  var_t *vdd, *gnd;
  int exclret;
  int nchecked = 0;

  vdd = var_locate (V, Vddnode);
  gnd = var_locate (V, GNDnode);

  for (v = var_step_first (V); v ; v = var_step_next (V)) {
    if (debug_level > 0) {
      nchecked++;
      if ((nchecked % 10000) == 0)
	pp_printf_raw (PPout, "Done %dK alias fixup\n", nchecked/1000);
    }

    /* if multiple rules that are aliased have a production rule in them,
     * complain.
     */
    flags = 0;
    flags2 = 0;
    prs_found = 0;
    t = v;
    while (t->alias) {
      if (t->up[PRSFILE] || t->dn[PRSFILE]) {
	err[!!prs_found] = t;
	prs_found++;
      }
      flags |= t->flags;
      flags2 |= t->flags2;
      t->flags = 0;		/* clear flags */
      t->flags2 = 0;
      t = t->alias;
    }
    if (t->up[PRSFILE] || t->dn[PRSFILE]) {
      err[!!prs_found] = t;
      prs_found++;
    }
    root = t;
    root->flags |= flags;	/* set flags */
    root->flags2 |= flags2;

    if (prs_found > 1) {
#if 0
      if (warnings)
	exit_status = 1;
#endif
      pp_printf (PPout, "WARNING: multiple aliased nodes have a prs rule");
      pp_forced (PPout, 5);
      t = v;
      while (t) {
	if (t->up[PRSFILE] || t->dn[PRSFILE])
	  pp_printf (PPout, "%s ", t->s);
	t = t->alias;
      }
      pp_printf (PPout, " . . . taking OR-combination");
      pp_forced (PPout, 0);
      t = v;
      while (t) {
	if (t->up[PRSFILE] || t->dn[PRSFILE]) {
	  if (t != err[0]) {
	    /* or-combination */
	    if (t->up[PRSFILE]) {
	      if (err[0]->up[PRSFILE]) {
		MALLOC (e, expr_t, 1);
		e->type = E_OR;
		e->l = err[0]->up[PRSFILE];
		e->r = t->up[PRSFILE];
		err[0]->up[PRSFILE] = e;
	      }
	      else
		err[0]->up[PRSFILE] = t->up[PRSFILE];
	    }
	    if (t->dn[PRSFILE]) {
	      if (err[0]->dn[PRSFILE]) {
		MALLOC (e, expr_t, 1);
		e->type = E_OR;
		e->l = err[0]->dn[PRSFILE];
		e->r = t->dn[PRSFILE];
		err[0]->dn[PRSFILE] = e;
	      }
	      else
		err[0]->dn[PRSFILE] = t->dn[PRSFILE];
	    }
	  }
	}
	t = t->alias;
      }
    }
    if (prs_found > 0) {
      /* canonicalize expression, move prs to the root */
      canonicalize_expr (err[0]->up[PRSFILE]);
      canonicalize_expr (err[0]->dn[PRSFILE]);
      root->up[PRSFILE] = err[0]->up[PRSFILE];
      root->dn[PRSFILE] = err[0]->dn[PRSFILE];
    }
    /* flatten tree */
    t = v;
    while (t != root) {
      tmp = t;
      t = t->alias;
      tmp->alias = root;
      tmp->up[PRSFILE] = NULL;
      tmp->dn[PRSFILE] = NULL;
    }
    /* canonicalize edges */
    prevedge  = NULL;
    for (edge = v->edges; edge; edge = edge->next) {
      edge->gate = canonical_name (edge->gate);
      edge->t1 = canonical_name (edge->t1);
      prevedge = edge;
    }
    /* move fixed-up edge list to the root */
    if (root != v && prevedge) {
      prevedge->next = root->edges;
      root->edges = v->edges;
      v->edges = NULL;
    }
    /* move excl lists up to the root */
    if (v != root) {
      elim_const_nodes (v, vdd, gnd);
      elim_const_nodes (root, vdd, gnd);
      exclret = compatible_excl_lists (v, root, vdd, gnd);
      if (!exclret) {
#if 0
	pp_printf (PPout, "WARNING: incompatible exclusive lists for "
		   "nodes %s and %s!", var_string (v), var_string(root));
	pp_forced (PPout, 0);
	if (warnings)
	  exit_status = 1;
#endif
	/* create extra excl list for "v" node */
	struct excl *ll = NULL;
	if (v->exclhi != v) {
	  root = v;
	  ll = add_to_excl_list (NULL, root);
	  for (root=root->exclhi; v != root; root = root->exclhi)
	    ll = add_to_excl_list (ll, root);
	  add_exclhi_list (ll);
	}
	if (v->excllo != v) {
	  root = v;
	  ll = add_to_excl_list (NULL, root);
	  for (root = root->excllo; v != root; root = root->excllo) 
	    ll = add_to_excl_list (ll, root);
	  add_excllo_list (ll);
	}
      }
      else {
	if (v->excllo != v && root->excllo == root) {
	  root->excllo = v->excllo;
	  for (tmp = v->excllo; tmp->excllo != v; tmp = tmp->excllo) ;
	  tmp->excllo = root;
	  v->excllo = v;
	}
	else {
	  if (exclret & 2) {
	    for (tmp = v->excllo; tmp->excllo != v; tmp = tmp->excllo) ;
	    for (t = root->excllo; t->excllo != root; t = t->excllo) ;
	    t->excllo = v;
	    tmp->excllo = root;
	    t = v->excllo;
	    v->excllo = root->excllo;
	    root->excllo = t;
	  }
	}
	if (v->exclhi != v && root->exclhi == root) {
	  root->exclhi = v->exclhi;
	  for (tmp = v->exclhi; tmp->exclhi != v; tmp = tmp->exclhi) ;
	  tmp->exclhi = root;
	  v->exclhi = v;
	}
	else {
	  if (exclret & 4) {
	    for (tmp = v->exclhi; tmp->exclhi != v; tmp = tmp->exclhi) ;
	    for (t = root->exclhi; t->exclhi != root; t = t->exclhi) ;
	    t->exclhi = v;
	    tmp->exclhi = root;
	    t = v->exclhi;
	    v->exclhi = root->exclhi;
	    root->exclhi = t;
	  }
	}
      }
    }
  }
  if (vdd)
    vdd = canonical_name (vdd);
  if (gnd)
    gnd = canonical_name (gnd);
  find_reset (V, &ResetVar, &_ResetVar);
  for (v = var_step_first (V); v; v = var_step_next (V)) {
    for (edge = v->edges; edge; edge = edge->next) {
      edge->gate = canonical_name (edge->gate);
      edge->t1 = canonical_name (edge->t1);
      if ((edge->type == P_TYPE && edge->gate == ResetVar && edge->t1 == vdd)
	  ||
	  (edge->type == N_TYPE && edge->gate == _ResetVar && edge->t1 == gnd)
	  )
	v->flags |= VAR_RESET_SUPPLY;
    }
  }
}


/*------------------------------------------------------------------------
 *
 *  calculate distance of edge "n1" to supply
 *
 *------------------------------------------------------------------------
 */
static
int max_supply_distance (edgelist_t *e, var_t *supply, var_t *other)
{
  int sub, d = -1;
  edgelist_t *f;

  Assert (!(e->t1->flags & (VAR_VISITED|VAR_OUTPUT)), "Huh?");
  e->t1->flags |= VAR_VISITED;
  if (e->t1 != supply && e->t1 != other) {
    for (f = e->t1->edges; f; f = f->next) {
      if (f->type != e->type) continue;
      if (f->t1->flags & (VAR_VISITED|VAR_OUTPUT)) continue;
      sub = max_supply_distance (f, supply, other);
      if (d < sub) d = sub;
    }
  }
  else
    d = 0;
  d++;
  e->t1->flags &= ~VAR_VISITED;
  return d;
}

static
int min_supply_distance (edgelist_t *e, var_t *supply, var_t *other)
{
  int sub, d = -1;
  edgelist_t *f;

  Assert (!(e->t1->flags & (VAR_VISITED|VAR_OUTPUT)), "Huh?");
  e->t1->flags |= VAR_VISITED;
  if (e->t1 != supply && e->t1 != other) {
    for (f = e->t1->edges; f; f = f->next) {
      if (f->type != e->type) continue;
      if (f->t1->flags & (VAR_VISITED|VAR_OUTPUT)) continue;
      sub = min_supply_distance (f, supply, other);
      if (sub != 0)
	if (d == -1 || d > sub) d = sub;
    }
  }
  else
    d = 0;
  d++;
  e->t1->flags &= ~VAR_VISITED;
  return d;
}


/*------------------------------------------------------------------------
 *
 *  Check strength ratios of strong pull-up to strong pull-down 
 *
 *  IF type == P_TYPE: (p is ^weak)
 *            calculate MAX p to MIN n ratio
 *
 *  If type == N_TYPE: (n is ^weak)
 *            calculate MAX n to MIN p ratio
 *
 *------------------------------------------------------------------------
 */
double strong_ratio (var_t *node, int type, var_t *vdd, var_t *gnd)
{
  edgelist_t *e, *f;
  double t_max_ratio, t_min_ratio;
  double r;
  int depth;

  t_max_ratio = t_min_ratio = -1;

  node->flags |= VAR_VISITED;
  for (e = node->edges; e; e = e->next) {
    if (!is_strong (e->width, e->length)) continue;
    if (e->gate == ResetVar || e->gate == _ResetVar) /* reset switches once */
      continue;
    if (e->t1->flags & VAR_OUTPUT)
      continue;
    r = (double)e->width/(double)e->length;
    /* calculate depth to supply: FIXME! */
    if (e->type == type) {
      depth = min_supply_distance (e, e->type == P_TYPE ? vdd : gnd,
				   e->type == P_TYPE ? gnd : vdd);
      if (depth == 0) {
	pp_printf (PPout, "Warning: no %c path to supply for output `%s', gate `%s'", e->type == P_TYPE ? 'p' : 'n', var_name (node), var_name (e->gate));
	pp_forced (PPout, 0);
	continue;
      }
      r /= depth;
      if (t_max_ratio < r) 
	t_max_ratio = r;
    }
    else {
      depth = max_supply_distance (e, e->type == P_TYPE ? vdd : gnd,
				   e->type == P_TYPE ? gnd : vdd);
      if (depth == 0) {
	pp_printf (PPout, "Warning: no %c path to supply for output `%s', gate `%s'", e->type == P_TYPE ? 'p' : 'n', var_name (node), var_name (e->gate));
	pp_forced (PPout, 0);
	continue;
      }
      r /= depth;
      if (t_min_ratio == -1 || t_min_ratio > r)
	t_min_ratio = r;
    }
  }
  node->flags &= ~VAR_VISITED;
  if (t_max_ratio == -1 || t_min_ratio == -1) return -1; /* bad stuff */
  return t_max_ratio / t_min_ratio;
}


double operator_strength (var_t *node, int type, var_t *vdd, var_t *gnd)
{
  edgelist_t *e, *f;
  double t_max_ratio, t_min_ratio;
  double r;
  int depth;

  t_max_ratio = t_min_ratio = -1;

  node->flags |= VAR_VISITED;
  for (e = node->edges; e; e = e->next) {
    if (!is_strong (e->width, e->length)) {
      if (e->type != type) {
	r = (double)e->width/(double)e->length;
	if (e->type == P_TYPE) {
	  if (e->t1 != vdd) {
	    for (f = e->t1->edges; f; f = f->next)
	      if (f->type == P_TYPE && f->t1 == vdd)
		break;
	    if (f)
	      r = (double)e->width/((double)e->length+f->length);
	  }
	}
	else {
	  if (e->t1 != gnd) {
	    for (f = e->t1->edges; f; f = f->next)
	      if (f->type == N_TYPE && f->t1 == gnd)
		break;
	    if (f)
	      r = (double)e->width/((double)e->length+f->length);
	  }
	}
	if (t_min_ratio == -1 || 
	    t_min_ratio > r)
	  t_min_ratio = r;
      }
      continue;
    }
    if (e->gate == ResetVar || e->gate == _ResetVar) /* reset switches once */
      continue;
    if (e->t1->flags & VAR_OUTPUT)
      continue;
    r = (double)e->width/(double)e->length;
    /* calculate depth to supply: FIXME! */
    if (e->type == type) {
      depth = min_supply_distance (e, e->type == P_TYPE ? vdd : gnd,
				   e->type == P_TYPE ? gnd : vdd);
      if (depth == 0) {
	pp_printf (PPout, "Warning: no %c path to supply for output `%s', gate `%s'", e->type == P_TYPE ? 'p' : 'n', var_name (node), var_name (e->gate));
	pp_forced (PPout, 0);
	continue;
      }
      r /= depth;
      if (t_max_ratio < r) 
	t_max_ratio = r;
    }
  }
  node->flags &= ~VAR_VISITED;
  if (t_max_ratio == -1 || t_min_ratio == -1) return -1; /* bad stuff */
  return t_max_ratio / t_min_ratio;
}




static
void strengthen_weak_gates (VAR_T *V, BOOL_T *B)
{
  var_t *v;

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    if ((v->flags & VAR_WEAKUP) && (v->flags & VAR_WEAKDN)) {
      pp_printf (PPout, "WARNING: [%s] has weak pull-up and pull-dn.",
		 var_name (v));
      v->flags &= ~(VAR_WEAKUP|VAR_WEAKDN);
    }
    if ((v->flags & VAR_WEAKUP) && v->up[STRONG] && v->dn[STRONG]) {
      v->up[STRONG] = bool_and (B, v->up[STRONG], bool_not (B, v->dn[STRONG]));
    }
    if ((v->flags & VAR_WEAKDN) && v->up[STRONG] && v->dn[STRONG]) {
      v->dn[STRONG] = bool_and (B, v->dn[STRONG], bool_not (B, v->up[STRONG]));
    }
  }
}


/*------------------------------------------------------------------------
 *
 * Check gates that have been artificially strengthened by ^weak
 *
 *------------------------------------------------------------------------
 */
static double extra_info1, extra_info2;

static
unsigned int check_weak_gates (var_t *v, var_t *vdd, var_t *gnd)
{
  double r;
  unsigned int flag = 0;

  if ((v->flags & VAR_WEAKUP) && v->up[STRONG] && v->dn[STRONG]) {
    r = strong_ratio (v, P_TYPE, vdd, gnd);
    if (r == -1) {
      pp_printf (PPout, "No p-type path to supply from node `%s'", 
		 var_name (v));
      pp_forced (PPout, 0);
    }
    else {
      /* r = MAX p to MIN n ratio; p is ^weak */
      if (1/r < N_P_Ratio)
	flag |= 1;
      extra_info1 = 1/r;
    }
  }
  if ((v->flags & VAR_WEAKDN) && v->up[STRONG] && v->dn[STRONG]) {
    r = strong_ratio (v, N_TYPE, vdd, gnd);
    if (r == -1) {
      pp_printf (PPout, "No n-type path to supply fro node `%s'",
		 var_name (v));
      pp_forced (PPout, 0);
    }
    else {
      /* r = MAX n to MIN p ratio; n is ^weak */
      if (r > N_P_Ratio)
	flag |= 2;
      extra_info2 = r;
    }
  }
  return flag;
}


static
int countdots (char *s)
{
  int dots = 0;
  while (*s)
    if (*s++ == '.') dots++;
  if (*(s-1) == '#') dots=~(1 << (sizeof(int)*8-1));
  return dots;
}


/*------------------------------------------------------------------------
 * 
 *  nice_prs_name --
 *
 *    Come up with var_t structure for a node that is connected to the
 *    specified node in the prs, but is a layout name for it.
 *
 *------------------------------------------------------------------------
 */
var_t *nice_prs_name (var_t *v)
{
  var_t *u, *best;
  int bestlen, len;
  int bestdots, dots;
  var_t *input = v;
  
  bestlen = strlen (v->s);
  best = v;
  bestdots = countdots (v->s);
  u = v = prs_canonical_name (v);
  do {
    if (canonical_name (u) != canonical_name (input)) goto next;
    dots = countdots (u->s);
    len = strlen (u->s);
    if (dots < bestdots || (dots == bestdots && len < bestlen)) {
      bestdots = dots;
      bestlen = len;
      best = u;
    }
next:
    u = u->alias_ring_prs;
  } while (u != v);
  return best;
}


/*------------------------------------------------------------------------
 * 
 *  nice_layout_name --
 *
 *    Come up with var_t structure for a node that is connected to the
 *    specified node in the layout, but is a prs name for it.
 *
 *------------------------------------------------------------------------
 */
var_t *nice_layout_name (var_t *v)
{
  var_t *u, *best;
  int bestlen, len;
  int bestdots, dots;
  var_t *input = v;

  bestlen = strlen (v->s);
  best = v;
  bestdots = countdots (v->s);
  u = v = canonical_name (v);
  do {
    if (prs_canonical_name (u) != prs_canonical_name (input)) goto next;
    dots = countdots (u->s);
    len = strlen (u->s);
    if (dots < bestdots || (dots == bestdots && len < bestlen)) {
      bestdots = dots;
      bestlen = len;
      best = u;
    }
next:
    u = u->alias_ring;
  } while (u != v);
  return best;
}

/*------------------------------------------------------------------------
 *
 *  check_aliases --
 *
 *      Check that the prs alias list and extracted alias list match
 *      properly.
 *
 *------------------------------------------------------------------------
 */
static int inlist (var_t *l, var_t *x)
{
  while (l) {
    if (l == x) return 1;
    l=l->worklist;
  }
  return 0;
}

void check_aliases (VAR_T *V)
{
  var_t *v, *u, *w, *x;
  int flagu, flagd;
  int flag;
  var_t *checklist;
  int nchecked = 0;

  missing_prs_connects = 0;
  missing_layout_connects = 0;

  if (debug_level > 0) 
    pp_printf_raw (PPout, "%d to be checked\n", V->H->n);

  mark_globals (V);

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    if (debug_level > 0) {
      nchecked++;
      if ((nchecked % 10000) == 0)
	pp_printf_raw (PPout, "Done %dK connection check\n", nchecked/1000);
    }
    if (v->inprs && !v->inlayout && !v->checkedprs) {
      v->checkedprs = 1;
      if (only_check_connects) {
	if (v->up[PRSFILE]) flagu = 1; else flagu = 0;
	if (v->dn[PRSFILE]) flagd = 1; else flagd = 0;
	flag = 0;
	for (u = v->alias_ring_prs; u != v; u = u->alias_ring_prs) {
	  if (u->inlayout) flag = 1;
	  else u->checkedprs = 1;
	  if (u->up[PRSFILE]) flagu = 1;
	  if (u->dn[PRSFILE]) flagd = 1;
	}
	if ((flagu || flagd) && !flag) {
	  missing_layout += flagu + flagd;
	  if (verbose) {
	    if (flagu && flagd) {
	      if (hier_notinput_subcell_node (V, var_string (v),
					    use_dot_separator ? '.' : '/'))
		missing_layout -=2;
	      else
		pp_printf_raw (PPout, "%s: rules missing from layout\n", 
			       var_name (v));
	    }
	    else {
	      if (flagu) 
		pp_printf_raw (PPout, "%s: pull-up missing from layout\n", 
			       var_name (v));
	      if (flagd)
		pp_printf_raw (PPout, "%s: pull-dn missing from layout\n", 
			       var_name (v));
	    }
	  }
	}
      }
    }
    if (!v->inprs || !v->inlayout) continue;
    if (!v->checkedprs) {
      v->checkedprs = 1;
      flag = 0;
      checklist = NULL;
      x = canonical_name (v);
      for (u = v->alias_ring_prs; u != v; u = u->alias_ring_prs) {
	u->checkedprs = 1;
	if (!u->inprs || !u->inlayout) continue;
	w = canonical_name (u);
	if (w != x && !inlist (checklist, w)) {
	  w->worklist = checklist;
	  checklist = w;
	  if (verbose)
	    pp_printf_raw (PPout, "%s and %s: connected in prs, not in layout\n",
			   var_string(nice_prs_name(u)),
			   var_string(nice_prs_name(v)));
	  missing_layout_connects++;
	  if (!connect_warn_only)
	    exit_status = 1;
	  flag = 1;
	  if (connect_globals_in_prs && dump_hier_file) {
	    /* Check to see if this is a global name in the prs file.
	       Vddnode, GNDnode, Reset, Reset_
	       */
	    w = v;
	    do {
	      if (strcmp (var_string (w), Vddnode) == 0 ||
		  strcmp (var_string (w), GNDnode) == 0 ||
		  strcmp (var_string (w), "Reset_") == 0 ||
		  strcmp (var_string (w), "Reset") == 0 ||
		  strcmp (var_string (w), "_Reset") == 0 ||
                  w->flags & VAR_GLOBALNODE) {
		missing_global_connects++;
		break;
	      }
	      w = w->alias_ring_prs;
 	    } while (w != v);
	  }
	}
      }
      u = checklist;
      while (u) {
	w = u->worklist;
	u->worklist = NULL;
	u = w;
      }
      if (flag == 1 && verbose > 1) {
	pp_printf (PPout, "PRS ALIASES:");
	pp_forced (PPout, 5);
	pp_setb (PPout);
	pp_printf (PPout, "%s", v->s);
	for (u = v->alias_ring_prs; u != v; u = u->alias_ring_prs) {
	  pp_printf (PPout, ", ");
	  pp_lazy (PPout, 3);
	  pp_printf (PPout, "%s", u->s);
	}
	pp_endb (PPout);
	pp_forced (PPout, 0);
      }
    }
    if (!v->checkedlayout) {
      v->checkedlayout = 1;
      flag = 0;
      checklist = NULL;
      x = prs_canonical_name (v);
      for (u = v->alias_ring; u != v; u = u->alias_ring) {
	u->checkedlayout = 1;
	if (!u->inprs || !u->inlayout) continue;
	w = prs_canonical_name (u);
	if (w != x && !inlist (checklist, w)) {
	  w->worklist = checklist;
	  checklist = w;
	  if (verbose) 
	    pp_printf_raw (PPout, "%s and %s: connected in layout, not in prs\n",
			   var_string(nice_layout_name(u)),
			   var_string(nice_layout_name(v)));
	  missing_prs_connects ++;
	  exit_status = 1;
	  flag = 1;
	}
      }
      u = checklist;
      while (u) {
	w = u->worklist;
	u->worklist = NULL;
	u = w;
      }
      if (flag == 1 && verbose > 1) {
	pp_printf (PPout, "LAYOUT ALIASES:");
	pp_forced (PPout, 5);
	pp_setb (PPout);
	pp_printf (PPout, "%s", v->s);
	for (u = v->alias_ring; u != v; u = u->alias_ring) {
	  pp_printf (PPout, ", ");
	  pp_lazy (PPout, 3);
	  pp_printf (PPout, "%s", u->s);
	}
	pp_endb (PPout);
	pp_forced (PPout, 0);
      }
    }
  }
}


/*------------------------------------------------------------------------
 *
 *  Validate i/o
 *
 *------------------------------------------------------------------------
 */
static
void validate_io (VAR_T *V)
{
  var_t *v;
  var_t *u;
  int inprs;
  var_t *vdd, *gnd;
  hash_bucket_t *hc;
  int nouts;
  var_t *w;

  
  vdd = var_locate (V, Vddnode);
  gnd = var_locate (V, GNDnode);
  
  for (v = var_step_first (V); v; v = var_step_next (V)) {
    if (v->hname) {
      nouts = 0;
      u = canonical_name (v);
      if (!u->checkedio) {
	u->checkedio = 1;
	if (u->flags & VAR_DRIVENOUTPUT)
	  nouts++;
	else {
	  if ((u->flags & VAR_HIER_NODE) && u->edges)
	    nouts++;
	}
	w = u;
	do {
	  if (w->hname) {
	    hc = w->hc;
	    if (hc == ROOT(hc) && !IS_INPUT (hc))
	      nouts++;
	  }
	  w = w->alias_ring;
	} while (w != u);
	if (nouts > 1) {
	  pp_printf (PPout, "ERROR: node `%s' driven at multiple levels of hierarchy",
		     var_string (var_nice_alias (v)));
	  pp_forced (PPout, 0);
	  io_violations++;
	}
	else if (nouts == 1) {
	  u->flags |= VAR_HOUTPUT;
	}
      }
    }
    if (v != canonical_name (v) || v->flags & VAR_OUTPUT 
	|| v == vdd || v == gnd)
      continue;
    inprs = v->inprs;
    for (u = v->alias_ring; inprs == 0 && u != v; u = u->alias_ring)
      inprs = u->inprs;
    if (inprs && v->edges != NULL) {
      io_violations++;
      pp_printf (PPout, "ERROR: transistor connected to input `%s'",
		 var_string (var_nice_alias(v)));
      pp_forced (PPout, 0);
      if (verbose > 1) {
	pp_printf (PPout, "ALIASES:");
	pp_forced (PPout, 5);
	pp_setb (PPout);
	pp_puts (PPout, var_string (v));
	for (u = v->alias_ring; u != v; u = u->alias_ring) {
	  pp_printf (PPout, ", ");
	  pp_lazy (PPout, 3);
	  pp_printf (PPout, "%s", var_string (u));
	}
	pp_endb (PPout);
	pp_forced (PPout, 0);
      }
      exit_status = 1;
    }
  }
}

/*------------------------------------------------------------------------
 *
 * Mark subcell nodes
 *
 *------------------------------------------------------------------------
 */
static void mark_hier_nodes (VAR_T *V)
{
  var_t *v, *u;
  
  for (v = var_step_first (V); v; v = var_step_next (V)) {
    u = canonical_name (v);
    if (u->flags & VAR_HIER_NODE)
      continue;
    if (hier_subcell_node (V, var_string (v), NULL, 
			   use_dot_separator ? '.' : '/'))
      u->flags |= VAR_HIER_NODE;
  }
}

/*------------------------------------------------------------------------
 *
 *   Print summary info about connect errors
 *
 *------------------------------------------------------------------------
 */
static void print_connect_summary (void)
{
  extern unsigned long num_fets;

  if (missing_layout_connects > 0) {
    pp_printf (PPout, "%d connection%s missing in layout.",
	       missing_layout_connects,
	       missing_layout_connects == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
  if (missing_prs_connects > 0) {
    pp_printf (PPout, "%d connection%s missing in prs.",
	       missing_prs_connects,
	       missing_prs_connects == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
  if (naming_violations > 0) {
    pp_printf (PPout, "%d naming violation%s.", naming_violations,
	       naming_violations == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
  if (io_violations > 0) {
    pp_printf (PPout, "%d input violation%s.", io_violations,
	       io_violations == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
  if (only_check_connects == 1) {
    if (missing_layout > 0) {
      exit_status = 1;
      pp_printf (PPout, "%d rule%smissing from layout.", missing_layout,
		 missing_layout == 1 ? " " : "s ");
      pp_forced (PPout,0);
    }
  }
#if 0
#ifdef DIGITAL_ONLY
  if (num_fets > 0) {
    pp_printf (PPout, "%lu fet%s processed", num_fets, 
	       num_fets == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
#endif
#endif
}


/*------------------------------------------------------------------------
 *
 *  print_prs_summary --
 *
 *    Print summary information about prs errors
 *
 *------------------------------------------------------------------------
 */
static void print_prs_summary (void)
{
  if (prs_differences > 0) {
    pp_printf (PPout, "%d production-rule difference%sfound.", 
	       prs_differences, prs_differences == 1 ? " " : "s ");
    pp_forced (PPout,0);
    exit_status = 1;
  }
  if (missing_layout > 0) {
    pp_printf (PPout, "%d rule%smissing from layout.", missing_layout,
	       missing_layout == 1 ? " " : "s ");
    if (known_missing_layout > 0)
      pp_printf (PPout, " [%d ok]", known_missing_layout);
    pp_forced (PPout,0);
    exit_status = 1;
  }
  if (missing_prs > 0) {
    pp_printf (PPout, "%d rule%smissing from prs.", missing_prs,
	       missing_prs == 1 ? " " : "s ");
    pp_forced (PPout,0);
    exit_status = 1;
  }
  if (missing_staticizers > 0) {
    pp_printf (PPout, "%d missing staticizer%s.", missing_staticizers,
	       missing_staticizers == 1 ? "" : "s");
    if (known_missing_staticizers > 0)
      pp_printf (PPout, " [%d ok]", known_missing_staticizers);
    pp_forced (PPout, 0);
  }
  if (extra_staticizers > 0) {
    pp_printf (PPout, "%d extra staticizer%s.", extra_staticizers,
	       extra_staticizers == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
  if (bad_staticizers > 0) {
    pp_printf (PPout, "%d staticizer error%s.",bad_staticizers,
	       bad_staticizers == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
  if (weak_drivers > 0) {
    pp_printf (PPout, "%d weak driver%s.", weak_drivers,
	       weak_drivers == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
  if (sneak_paths > 0) {
    pp_printf (PPout, "%d sneak path%s.", sneak_paths,
	       sneak_paths == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
  if (pchg_errors > 0) {
    pp_printf (PPout, "%d precharge error%s.", pchg_errors,
	       pchg_errors == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
  if (chargesharing_errs > 0) {
    pp_printf (PPout, "%d charge-sharing error%s.", chargesharing_errs,
	       chargesharing_errs == 1 ? "" : "s");
    pp_forced (PPout, 0);
  }
}

static
void merge__Reset (VAR_T *V)
{
  var_t *_r, *r;
  var_t *v;

  find_reset (V, &r, &_r);

  if (_r == NULL) {
    _r = var_enter (V, "_Reset");
    _r->inprs = 1;
  }
  _r = prs_canonical_name (_r);

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    if (v->s[0] == '_' && v->s[1] != '\0') {
      if (strcmp (v->s + 2, "Reset") == 0) {
	/* connect v and _r */
	v = prs_canonical_name (v);
	if (v != _r) {
	  _r->alias_prs = v;
	  r = _r->alias_ring_prs;
	  _r->alias_ring_prs = v->alias_ring_prs;
	  v->alias_ring_prs = r;
	  _r = prs_canonical_name (_r);
	}
      }
    }
  }
}

/*------------------------------------------------------------------------
 *
 *  union_aliases --
 *
 *    Merge alias lists from the prs file and the layout file 
 *
 *------------------------------------------------------------------------
 */
static
void union_aliases (VAR_T *V)
{
  var_t *v, *u, *x, *y, *z;

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    /*
    u = prs_canonical_name(v);
    x = canonical_name (u);
    y = canonical_name (v);
    if (x != y) {
      u = x->alias_ring;
      x->alias_ring = y->alias_ring;
      y->alias_ring = u;
      x->alias = y;
    }
    */
    u = canonical_name (v);
    x = prs_canonical_name (u);
    y = prs_canonical_name (v);
    if (x != y) {
      u = x->alias_ring_prs;
      x->alias_ring_prs = y->alias_ring_prs;
      y->alias_ring_prs = u;
      x->alias_prs = y;
    }
  }

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    u = v->alias;
    v->alias = v->alias_prs;
    v->alias_prs = u;
    
    u = v->alias_ring;
    v->alias_ring = v->alias_ring_prs;
    v->alias_ring_prs = u;
  }
}

/*------------------------------------------------------------------------
 *
 *  compute_local_p_invariant --
 *
 *    Compute invariant that might be interesting for the network passing
 *    p transistors at a particular node
 *
 *------------------------------------------------------------------------
 */
static
bool_t *compute_local_p_invariant (var_t *v, BOOL_T *B, var_t *vdd)
{
  bool_t *inv, *invtmp, *t1, *t2, *t3;
  edgelist_t *e;
  var_t *hd, *tl, *t, *u;

  /*inv = bool_true (B);*/
  inv = bool_copy (B, InvariantLo);
  
  hd = v;
  hd->flags |= VAR_VISITED;
  tl = hd;
  
  while (hd) {
    for (e = hd->edges; e; e = e->next) {
      if (e->isweak || !is_strong (e->width, e->length)) continue;
      if (e->type == N_TYPE) continue;
      if (e->gate->excllo != e->gate && 
	  !(canonical_name(e->gate)->flags & VAR_MARKED)) {
	/* excllo(a1,...,aN) = (exists i :: (forall j : j != i : aj)) */
	invtmp = bool_false (B);
	u = e->gate;
	do {
	  canonical_name(u)->flags |= VAR_MARKED;
	  t2 = bool_true (B);
	  for (t = u->excllo; t != u; t = t->excllo) {
	    Assert (canonical_name(t)->flags & VAR_HAVEVAR, "Huh?");
	    t3 = bool_and (B, t2, t1 = bool_var (B, canonical_name(t)->v));
	    bool_free (B, t2);
	    bool_free (B, t1);
	    t2 = t3;
	  }
	  invtmp = bool_or (B, t1 = invtmp, t2);
	  bool_free (B, t1);
	  u = u->excllo;
	} while (u != e->gate);
	inv = bool_and (B, t1 = inv, invtmp);
	bool_free (B, t1);
	bool_free (B, invtmp);

	invtmp = inv;
	inv = bool_and (B, inv, 
			t1 = compute_additional_local_p_invariant (B, canonical_name (e->gate)));
	bool_free (B, t1);
	bool_free (B, invtmp);

      }
      if (!(e->t1->flags & VAR_VISITED) && e->t1 != vdd) {
	e->t1->flags |= VAR_VISITED;
	tl->worklist = e->t1;
	tl = e->t1;
	e->t1->worklist = NULL;
      }
    }
    t = hd;
    hd = hd->worklist;
    t->worklist = NULL;
  }

  /* clearflags */
  hd = v;
  tl = hd;
  hd->flags &= ~VAR_VISITED;
  while (hd) {
    for (e = hd->edges; e; e = e->next) {
      if (e->gate->excllo != e->gate && 
	  (canonical_name(e->gate)->flags & VAR_MARKED)) {
	t = e->gate;
	do {
	  canonical_name(t)->flags &= ~VAR_MARKED;
	  t = t->excllo;
	} while (t != e->gate);
      }
      if (e->t1->flags & VAR_VISITED) {
	e->t1->flags &= ~VAR_VISITED;
	tl->worklist = e->t1;
	tl = e->t1;
	tl->worklist = NULL;
      }
    }
    t = hd;
    hd = hd->worklist;
    t->worklist = NULL;
  }
  return inv;
}


/*------------------------------------------------------------------------
 *
 *  compute_local_n_invariant --
 *
 *    Compute invariant that might be interesting for the network passing
 *    n transistors at a particular node
 *
 *------------------------------------------------------------------------
 */
static
bool_t *compute_local_n_invariant (var_t *v, BOOL_T *B, var_t *gnd)
{
  bool_t *inv, *invtmp, *t1, *t2, *t3, *t4;
  edgelist_t *e;
  var_t *hd, *tl, *t, *u;

  inv = bool_copy (B, InvariantHi);

  hd = v;
  hd->flags |= VAR_VISITED;
  tl = hd;

  while (hd) {
    for (e = hd->edges; e; e = e->next) {
      if (e->isweak || !is_strong (e->width, e->length)) continue;
      if (e->type == P_TYPE) continue;


      if (e->gate->exclhi != e->gate && 
	  (canonical_name(e->gate)->flags & VAR_MARKED) == 0) {
#if 0
	printf ("exclhi: ");
#endif
	/* exclhi(a1,...,aN) = (exists i :: (forall j : j != i : ~aj)) */
	invtmp = bool_false (B);
	u = e->gate;
	do {
#if 0
	  printf ("%s ", var_name (canonical_name (u)));
#endif	  
	  canonical_name(u)->flags |= VAR_MARKED;
	  t2 = bool_true (B);
	  for (t = u->exclhi; t != u; t = t->exclhi) {
	    Assert (canonical_name(t)->flags & VAR_HAVEVAR, "Huh???");
	    t3 = bool_and (B, 
			   t2,
			   t1 =
			   bool_not (B, 
				     t4 = bool_var (B, canonical_name(t)->v)));
	    bool_free (B, t2);
	    bool_free (B, t1);
	    bool_free (B, t4);
	    t2 = t3;
	  }
	  invtmp = bool_or (B, t1 = invtmp, t2);
	  bool_free (B, t1);
	  u = u->exclhi;
	} while (u != e->gate);
#if 0
	printf ("\n");
#endif
	inv = bool_and (B,  t1 = inv, invtmp);
	bool_free (B, t1);
	bool_free (B, invtmp);

	invtmp = inv;
	inv = bool_and (B, inv, 
			t1 = compute_additional_local_n_invariant (B, canonical_name (e->gate)));
	bool_free (B, t1);
	bool_free (B, invtmp);
      }
      if (!(e->t1->flags & VAR_VISITED) && e->t1 != gnd) {
	e->t1->flags |= VAR_VISITED;
	tl->worklist = e->t1;
	tl = e->t1;
	e->t1->worklist = NULL;
      }
    }
    t = hd;
    hd = hd->worklist;
    t->worklist = NULL;
  }

  /* clearflags */
  hd = v;
  tl = hd;
  hd->flags &= ~VAR_VISITED;
  while (hd) {
    for (e = hd->edges; e; e = e->next) {
      if (e->gate->exclhi != e->gate && 
	  (canonical_name(e->gate)->flags & VAR_MARKED)) {
	t = e->gate;
	do {
	  canonical_name(t)->flags &= ~VAR_MARKED;
	  t = t->exclhi;
	} while (t != e->gate);
      }
      if (e->t1->flags & VAR_VISITED) {
	e->t1->flags &= ~VAR_VISITED;
	tl->worklist = e->t1;
	tl = e->t1;
	tl->worklist = NULL;
      }
    }
    t = hd;
    hd = hd->worklist;
    t->worklist = NULL;
  }
  return inv;
}

/*------------------------------------------------------------------------
 *
 *  Compute base invariant (gnd=false & vdd=true), and then pick a good
 *  variable ordering for the invariant.
 *
 *------------------------------------------------------------------------
 */
static
void compute_invariant (VAR_T *V, BOOL_T *B)
{
  var_t *v, *chan, *chantwo, *u;
  var_t *vdd, *gnd;
  bool_t *b0, *b1;
  bool_t *t1, *t2, *t3;
  extern struct excl *firstlist;
  struct excl *el;


  InvariantHi = bool_true (B);
  InvariantLo = bool_true (B);
  
  vdd = var_locate (V, Vddnode);
  gnd = var_locate (V, GNDnode);

  /* 
   * make vdd true and gnd false in the invariant
   *
   */
  if (vdd) {
    if (!(vdd->flags & VAR_HAVEVAR)) {
      vdd->flags |= VAR_HAVEVAR;
      vdd->v = bool_topvar (t1 = bool_newvar(B));
      bool_free (B, t1);
    }
    InvariantHi = bool_and (B, t3 = InvariantHi, 
			    t2 = bool_xor (B, t1 = bool_var (B, vdd->v),
					   bool_false (B)));
    bool_free (B, t1); bool_free (B, t2); bool_free (B, t3);
    InvariantLo = bool_and (B, t1 = InvariantLo, 
			    t2 = bool_xor (B, t3 = bool_var (B, vdd->v),
					   bool_false (B)));
    bool_free (B, t1); bool_free (B, t2); bool_free (B, t3);
  }
  if (gnd) {
    if (!(gnd->flags & VAR_HAVEVAR)) {
      gnd->flags |= VAR_HAVEVAR;
      gnd->v = bool_topvar (t1 = bool_newvar (B));
      bool_free (B, t1);
    }
    InvariantHi = bool_and (B, t3 = InvariantHi,
			    t1 = bool_xor (B, t2 = bool_var (B, gnd->v),
					   bool_true (B)));
    bool_free (B, t1); bool_free (B, t2); bool_free (B, t3);
    InvariantLo = bool_and (B, t1 = InvariantLo,
			    t2 = bool_xor (B, t3 = bool_var (B, gnd->v),
					   bool_true (B)));
    bool_free (B, t1); bool_free (B, t2); bool_free (B, t3);
  }
  bool_gc (B);

  /* 
   * Pick a good variable ordering for the invariant calculation
   * so that it is linear in the # of variables.
   * Don't actually compute the invariant; it will be computed 
   * locally as necessary.
   *
   */
  for (el = firstlist; el; el = el->next) {
    u = canonical_name (el->v);
    if (!(u->flags & VAR_HAVEVAR)) {
      u->flags |= VAR_HAVEVAR;
      u->v = bool_topvar (t1 = bool_newvar (B));
      bool_free (B, t1);
    }
  }
    
  for (v = var_step_first (V); v; v = var_step_next (V)) {
    if (v->exclhi != v) {
      /* exclusive high rules */
      u = canonical_name (v);
      if (!(u->flags & VAR_HAVEVAR)) {
	u->flags |= VAR_HAVEVAR;
	u->v = bool_topvar (t1 = bool_newvar(B));
	bool_free (B, t1);
      }
      b1 = bool_false (B);
      chantwo = v;
      for (chan = chantwo->exclhi; chan != chantwo; chan = chan->exclhi) {
	u = canonical_name (chan);
	if (!(u->flags & VAR_HAVEVAR)) {
	  u->flags |= VAR_HAVEVAR;
	  u->v = bool_topvar (t1 = bool_newvar (B));
	  bool_free (B, t1);
	}
      }
    }
    /* exclusive low rules */
    if (v->excllo != v) {
      u = canonical_name (v);
      if (!(u->flags & VAR_HAVEVAR)) {
	u->flags |= VAR_HAVEVAR;
	u->v = bool_topvar (t1 = bool_newvar(B));
	bool_free (B, t1);
      }
      chantwo = v;
      for (chan = chantwo->excllo; chan != chantwo; chan = chan->excllo) {
	u = canonical_name (chan);
	if (!(u->flags & VAR_HAVEVAR)) {
	  u->flags |= VAR_HAVEVAR;
	  u->v = bool_topvar (t1 = bool_newvar (B));
	  bool_free (B, t1);
	}
      }
    }
  }
}

/*------------------------------------------------------------------------
 *
 *  Generate production rules.
 *
 *------------------------------------------------------------------------
 */
void gen_prs (VAR_T *V, BOOL_T *B)
{
  var_t *vdd, *gnd, *v;
  expr_t *e;
  
  /* find power supply nodes */
  vdd = var_find (V, Vddnode);
  gnd = var_find (V, GNDnode);
  if (!vdd && !gnd && (only_check_connects == 0))
    fatal_error ("Missing both GND [%s] and Vdd [%s] nodes.", 
		 GNDnode, Vddnode);
  if (!vdd) {
    pp_printf (PPout, "WARNING: missing Vdd node [%s]", Vddnode);
    pp_forced (PPout, 0);
#if 0
    if (warnings)
      exit_status = 1;
#endif
  }
  else
    vdd->inprs = 1;
  if (!gnd) {
    pp_printf (PPout, "WARNING: missing GND node [%s]", GNDnode);
    pp_forced (PPout, 0);
#if 0
    if (warnings)
      exit_status = 1;
#endif
  }
  else 
    gnd->inprs = 1;

  /*
   * check that the prs alias list and extract alias list match up
   */
  missing_layout = 0;

  if (!print_only)
    check_aliases (V);

  /* 
   * Connect _Reset, _xReset, where "x" is any character
   */
  if (prefix_reset) {
    merge__Reset (V);
  }

  /*
   * Take the union of the prs and extract aliases
   */
  union_aliases (V);

  /*
   * propagate all flags up the extracted alias list, and fix all things
   * in the edge-list to refer to the canonical extract name.
   */
  fix_aliases (V);

  /*
   * mark output nodes 
   */
  mark_prs_output (V);

  /*
   * Mark nodes as part of hierarchical subcell
   */
  mark_hier_nodes (V);

  /*
   * Validate i/o:
   *   If node is inprs & ~output, then it should not have any
   *   transistors connected to it.
   */
  validate_io (V);

  if (only_check_connects == 1) {
    print_connect_summary ();
    if (wizard) return;
    exit (exit_status);
  }


  /* compute the excl(...) invariant */
  compute_invariant (V, B);

  /* propagate values from vdd */
  if (vdd) vdd = canonical_name (vdd);
  if (gnd) gnd = canonical_name (gnd);

  if (vdd) {
    vdd->flags &= ~(VAR_OUTPUT|VAR_INPUT);
    vdd->up[STRONG] = bool_true (B);
    vdd->up[WEAK] = bool_true (B);
    vdd->dn[STRONG] = bool_false (B);
    vdd->dn[WEAK] = bool_false (B);
    compute_prs (V, B, vdd, gnd, P_TYPE);
  }

  /* propagate values from gnd */
  if (gnd) {
    gnd->flags &= ~(VAR_OUTPUT|VAR_INPUT);
    gnd->up[STRONG] = bool_false (B);
    gnd->up[WEAK] = bool_false (B);
    gnd->dn[STRONG] = bool_true (B);
    gnd->dn[WEAK] = bool_true (B);
    compute_prs (V, B, gnd, vdd, N_TYPE);
  }

  /* strengthen gates labelled with weak^ */
  strengthen_weak_gates (V, B);
}


/*------------------------------------------------------------------------
 *
 *  Convert bool identifier into var_t structure
 *
 *------------------------------------------------------------------------
 */
static
var_t *id_to_var (VAR_T *V, bool_var_t id)
{
  var_t *v;
  int i;
  hash_bucket_t *e;
  for (i = 0; i < V->H->size; i++)
    for (e = V->H->head[i]; e; e = e->next) {
      v = (var_t*)e->v;
      if ((v->flags & VAR_HAVEVAR) && id == v->v)
	return v;
    }
  return NULL;
}

/*------------------------------------------------------------------------
 *
 *  Check if variable needs to be quoted
 *
 *------------------------------------------------------------------------
 */
static
int quote_name (char *s)
{
  while (*s) {
    if (*s == '/' || *s == '!' || *s == '[' || *s == ']' || *s == '.'
	|| *s == '#' || *s == '&')
      return 1;
    s++;
  }
  return 0;
}


/*------------------------------------------------------------------------
 *
 *  Check inverter
 *
 *     Checks that "up/dn" is a guard which corresponds to an inverter
 *     after Reset goes away; "id" returns the name of the node there.
 *
 *------------------------------------------------------------------------
 */
static
int is_an_inverter (VAR_T *V, BOOL_T *B, bool_t *up, bool_t *dn, bool_var_t *id)
{
  var_t *v, *v_;
  bool_t *b1, *b2, *b3;

  if (!up || !dn) return 0;

  if (up == bool_true (B) || up == bool_false (B)) return 0;
  if (dn == bool_true (B) || dn == bool_false (B)) return 0;

  up = bool_copy (B, up);
  dn = bool_copy (B, dn);

  /* check down guard */
  if (dn != (b1 = bool_var (B, bool_topvar (dn)))) {
    bool_free (B, b1);
    v = ResetVar;
    v_ = _ResetVar;
    if (!v && !v_) {
      bool_free (B, dn);
      bool_free (B, up);
      return 0;
    }
    if (v_ && (v_->flags & VAR_HAVEVAR)) {
      dn = bool_maketrue (B, b1 = dn, b2 = bool_var (B, v_->v));
      bool_free (B, b1);
      bool_free (B, b2);
    }
    if (v  && (v->flags & VAR_HAVEVAR)) {
      dn = bool_makefalse (B, b1 = dn, b2 = bool_var (B, v->v));
      bool_free (B, b1);
      bool_free (B, b2);
    }
    if (dn == bool_true (B) || dn == bool_false (B)) {
      bool_free (B, up);
      bool_free (B, dn);
      return 0;
    }
    if (dn != (b1 = bool_var (B, bool_topvar (dn)))) {
      bool_free (B, b1);
      bool_free (B, dn);
      bool_free (B, up);
      return 0;
    }
    else 
      bool_free (B, b1);
  }
  else 
    bool_free (B, b1);
  if (up != (b1 = bool_not (B, b2 = bool_var (B, bool_topvar (dn))))) {
    bool_free (B, b1);
    bool_free (B, b2);
    v = ResetVar;
    v_ = _ResetVar;
    if (!v && !v_) {
      bool_free (B, up);
      bool_free (B, dn);
      return 0;
    }
    if (v_ && (v_->flags & VAR_HAVEVAR)) {
      up = bool_maketrue (B, b1 = up, b2 = bool_var (B, v_->v));
      bool_free (B, b1);
      bool_free (B, b2);
    }
    if (v  && (v->flags & VAR_HAVEVAR)) {
      up = bool_makefalse (B, b1 = up, b2 = bool_var (B, v->v));
      bool_free (B, b1);
      bool_free (B, b2);
    }
    if (up == bool_true (B) || up == bool_false (B)) {
      bool_free (B, dn);
      bool_free (B, up);
      return 0;
    }
    if (up != (b1 = bool_not (B, b2 = bool_var (B, bool_topvar (up))))) {
      bool_free (B, b1);
      bool_free (B, b2);
      bool_free (B, dn);
      bool_free (B, up);
      return 0;
    }
    *id = bool_topvar (up);
  }
  else {
    bool_free (B, b1);
    bool_free (B, b2);
    *id = bool_topvar (dn);
  }
  bool_free (B, up);
  bool_free (B, dn);
  return 1;
}

static
int is_a_halfinv (VAR_T *V, BOOL_T *B, bool_t *up, bool_t *dn, bool_var_t *id)
{
  bool_t *b1, *b2;

  if (up == NULL || up == bool_false (B)) {
    if (dn == NULL || dn == bool_true (B) || dn == bool_false (B)) return 0;
    *id = bool_topvar (dn);
    if (bool_var (B, bool_topvar (dn)) == dn) {
      bool_free (B, dn);
      return 1;
    }
    return 0;
  }
  if (dn == NULL || dn == bool_false (B)) {
    if (up == NULL || up == bool_true (B) || up == bool_false (B)) return 0;
    *id = bool_topvar (up);
    if (bool_not (B, b1 = bool_var (B, bool_topvar (up))) == up) {
      bool_free (B, up);
      bool_free (B, b1);
      return 1;
    }
    return 0;
  }
  return 0;
}


/*------------------------------------------------------------------------
 *
 *  combinational_feedback --
 *
 *   Return 1 if the node has combinational feedback, 0 otherwise
 *
 *------------------------------------------------------------------------
 */
int combinational_feedback (VAR_T *V, BOOL_T *B, var_t *v)
{
  var_t *vp, *tvp;
  edgelist_t *e;
  int ngates, pgates, diff;
  bool_var_t id;

  if (!v->up[STRONG] || !v->dn[STRONG] 
      || !v->up[WEAK] || !v->dn[WEAK])
    /* there's no hope, give up now */
    return 0;

  /* check for combinational feedback 
     
     prs :
     s0 -> v+
     s1 -> v-

     feedback:

     ~s1 & ~v' -> v+
     ~s0 & v' -> v-

     v -> v'-
     ~v -> v'+
	     
     Assuming that v' is the node closest to the output v...

     so: 
       - # of weak gates closest to v' MUST be 1.
  */

  find_feedback_inverter (V, B, v, &vp, v, P_TYPE);
  if (!vp)
    return 0;
  find_feedback_inverter (V, B, v, &tvp, v, N_TYPE);
  if (vp != tvp)
    return 0;

  /* ok, so we have hope... */

#if 0
  printf ("  === checking %s ===\n", var_name (v));
  printf ("      inv %s\n", var_name (vp));
#endif

  /* 
     part 1: check if we have an inverter from v->vp
  */
  if (!is_an_inverter (V, B, vp->up[STRONG], vp->dn[STRONG], &id)) {
    return 0;
  }

#if 0
  printf ("      found inv!\n");
#endif

  if ((v->flags & VAR_HAVEVAR) && id == v->v) {
    /* ok, so there is a strong inverter from v->vp */
    /* now to check if the feedback is combinational... */
    
    bool_t *newup, *newdn;
    bool_t *bvp;

    if (!(vp->flags & VAR_HAVEVAR)) {
      /* umm. how did we get here exactly? */
      return 0;
    }

    /*
      Pull-up check:
	 ~vp -> v+
	 OR
         ~vp & ~pulldn -> v+

	 First case is easy...

	 Second case:
	    - (~vp & ~pulldn)[vp:=true] = false
	      (~vp & ~pulldn)[vp:=false] = ~pulldn
    */
    bvp = bool_var (B, vp->v);

    newup = bool_not (B, bvp);
    bool_free (B, newup);
    if (newup != v->up[WEAK]) {
      
      newup = bool_maketrue (B, v->up[WEAK], bvp);
      bool_free (B, newup);
      if (newup != bool_false (B)) {
	bool_free (B, bvp);
	return 0;
      }
      newup = bool_makefalse (B, v->up[WEAK], bvp);
      bool_free (B, bvp);
      bvp = bool_not (B, newup);
      bool_free (B, newup);
      bool_free (B, bvp);
      if (bvp != v->dn[STRONG]) {
	/* nope! */
	return 0;
      }
    }

    bvp = bool_var (B, vp->v);

    if (bvp != v->dn[WEAK]) {
      newdn = bool_makefalse (B, v->dn[WEAK], bvp);
      bool_free (B, newdn);
      if (newdn != bool_false (B)) {
	bool_free (B, bvp);
	return 0;
      }
      
      newdn = bool_maketrue (B, v->dn[WEAK], bvp);
      bool_free (B, bvp);
      bvp = bool_not (B, newdn);
      bool_free (B, newdn);
      bool_free (B, bvp);
      if (bvp != v->up[STRONG]) {
	/* nope! */
	return 0;
      }
    }
    v->flags2 |= (VAR_COMBFEEDBACK_N|VAR_COMBFEEDBACK_P);
    /* yay! */
    return 1;
  }
  else {
    /* no inverter from v to vp */
    return 0;
  }

  return 0;
}


/*------------------------------------------------------------------------
 *
 *  find_feedback_inverter --
 *
 *    Locate combinational feedback inverter for an output node
 *
 *     out = output node
 *       v = current node
 *      vp = return value
 *    type = stack type
 *
 *------------------------------------------------------------------------
 */
void find_feedback_inverter (VAR_T *V, BOOL_T *B,
			     var_t *v, var_t **vp, var_t *out, int type)
{
  edgelist_t *e;
  bool_var_t id;

  *vp = NULL;

  v->flags |= VAR_VISITED;
  for (e = v->edges; e; e = e->next) {
    if (e->type != type) continue;
    if (is_an_inverter (V, B, e->gate->up[STRONG], e->gate->dn[STRONG], &id)
	&& (out->flags & VAR_HAVEVAR) && (out->v == id)) {
      if (*vp == NULL) {
	*vp = e->gate;
      }
      else {
	if (*vp != e->gate) {
	  *vp = NULL;
	  v->flags &= ~VAR_VISITED;
	  return;
	}
      }
    }
  }
  for (e = v->edges; !(*vp) && e; e = e->next) {
    if (e->type != type) continue;
    if (e->t1->flags & (VAR_OUTPUT|VAR_VISITED)) continue;
    find_feedback_inverter (V, B, e->t1, vp, out, type);
  }
  v->flags &= ~VAR_VISITED;
}

/*------------------------------------------------------------------------
 *
 *  general_combinational_feedback --
 *
 *   Split pull-up and pull-down into feedback + non-feedback paths
 *   if the node has combinational feedback
 *
 *  1. merge v->up[STRONG] and v->up[WEAK].  
 *  2. find n/pfet 1-away that feeds into an inverter
 *  3. if exactly-one is found, then 
 *         - assume comb feedback
 *         - recompute v->up[STRONG], v->up[WEAK]
 *                     v->dn[STRONG], v->dn[WEAK]
 *------------------------------------------------------------------------
 */
int split_combinational_feedback (VAR_T *V, BOOL_T *B, var_t *v)
{
  static bool_t *RInv = NULL;
  var_t *vpn, *vpp;
  bool_t *nup, *ndn;
  bool_t *t1, *t2, *t3, *t4;
  bool_t *xA, *xB;
  bool_t *wA, *wB;
  bool_t *tmp;

#if 0
  printf (" --- checking %s --- \n", var_name (v));
#endif

  if (!RInv) {
    RInv = bool_true (B);
    if ((vpn = var_find (V, "_SReset")) && (vpp = var_find (V, "_PReset"))) {
      if ((vpn->flags & VAR_HAVEVAR) && (vpp->flags & VAR_HAVEVAR)) {
	RInv = bool_not (B, bool_xor (B, bool_var (B, vpp->v),
				      bool_var (B, vpn->v)));
      }
    }
  }

  if (!v->up[STRONG] || !v->dn[STRONG]) {
    return 0;
  }

  find_feedback_inverter (V, B, v, &vpp, v, P_TYPE);
  if (!vpp)
    return 0;
  find_feedback_inverter (V, B, v, &vpn, v, N_TYPE);
  if (vpp != vpn)
    return 0;

  if (!(vpp->flags & VAR_HAVEVAR))
    return 0;

  /* recompute pull-up */
  if (!v->up[WEAK]) {
    v->up[WEAK] = bool_false (B);
  }
  if (!v->dn[WEAK]) {
    v->dn[WEAK] = bool_false (B);
  }

#if 0
  printf ("     possibly ok\n");
  printf ("     var: %s\n", var_name (vpp));
#endif

  nup = bool_or (B, v->up[STRONG], v->up[WEAK]);
  ndn = bool_or (B, v->dn[STRONG], v->dn[WEAK]);
  
  xA = bool_maketrue (B, nup, t1 = bool_var (B, vpp->v));
  xB = bool_makefalse (B, ndn, t1);

#if 0
  pp_printf (PPout, "xA: ");
  print_slow_special (PPout, B, V, xA, P_TYPE);
  pp_forced (PPout, 0);

  pp_printf (PPout, "xB: ");
  print_slow_special (PPout, B, V, xB, N_TYPE);
  pp_forced (PPout, 0);
#endif

  /**
     (nup) pull-up MUST BE: xA | ~t1 & ~xB
              OR: xA | ~t1

     (ndn) pull-dn MUST BE: xB | t1 & ~xA
              OR: xB | t1
  **/

  /* check nup */
  tmp = bool_or (B, xA, wA = bool_not (B, t1));
  bool_free (B, tmp);
  if (tmp == nup && (v->up[WEAK] != bool_false (B))) {
    /* xA | ~t1 with weak elements... */
    goto nextchk;
  }

  tmp = bool_or (B, xA, wA = bool_and (B, t3 = bool_not (B, t1), 
				       t4 = bool_not (B, xB)));

  bool_free (B, t3);
  bool_free (B, t4);

  if (tmp != nup) {
    /* nope */
    /* Does RInv => (tmp == nup?) */
    t2 = bool_implies (B, RInv, 
		       t3 = bool_not (B, t4 = bool_xor (B, tmp,
							nup)));
    bool_free (B, t3);
    bool_free (B, t4);
    bool_free (B, t2); /* hehe */

    if (t2 != bool_true (B)) {
      bool_free (B, nup);
      bool_free (B, tmp);
      bool_free (B, t1);
      bool_free (B, xA);
      bool_free (B, xB);
      bool_free (B, wA);
      return 0;
    }
  }
  bool_free (B, nup);
  bool_free (B, tmp);
  v->flags2 |= VAR_COMBFEEDBACK_P;

 nextchk:

  /* check ndn */
  tmp = bool_or (B, xB, wB = bool_copy (B, t1));
  bool_free (B, tmp);
  if (tmp == ndn && (v->dn[WEAK] != bool_false (B))) {
    goto donechk;
  }

  tmp = bool_or (B, xB, wB = bool_and (B, t1, t3 = bool_not (B, xA)));
  bool_free (B, t3);
  bool_free (B, t1);

  if (tmp != ndn) {

    t2 = bool_implies (B, RInv, 
		       t3 = bool_not (B, t4 = bool_xor (B, tmp,
							ndn)));
    bool_free (B, t4);
    bool_free (B, t3);
    bool_free (B, t2);

    if (t2 != bool_true (B)) {
      bool_free (B, ndn);
      bool_free (B, tmp);
      bool_free (B, xA);
      bool_free (B, xB);
      bool_free (B, wA);
      bool_free (B, wB);
      return 0;
    }
  }
  bool_free (B, tmp);
  bool_free (B, ndn);
  v->flags2 |= VAR_COMBFEEDBACK_N;

 donechk:

  bool_free (B, v->up[STRONG]);
  bool_free (B, v->dn[STRONG]);
  bool_free (B, v->up[WEAK]);
  bool_free (B, v->dn[WEAK]);
  
  v->up[STRONG] = xA;
  v->up[WEAK] = wA;
  v->dn[STRONG] = xB;
  v->dn[WEAK] = wB;

  return 1;
}



/*------------------------------------------------------------------------
 *
 *  driven --
 *
 *    true if node is driven by the right type of transistor
 *
 *------------------------------------------------------------------------
 */
static
int driven (BOOL_T *B, var_t *node, int type)
{
  if ((type == P_TYPE 
       && (node->up[STRONG] == bool_true (B) || node->up[WEAK] == bool_true (B)))
      || 
      (type == N_TYPE 
       && (node->dn[STRONG] == bool_true (B) || node->dn[WEAK] == bool_true (B)))
      )
    return 1;
  else
    return 0;
}

static int max_vars = 0;
static var_t **nodelist = NULL;
static var_t **curpath = NULL;
static int num_vars = 0;

static
var_t **ensure_one_slot (var_t **list)
{
  int flag;
  
  if (num_vars == max_vars) {
    if (list == nodelist) flag = 1; else flag = 0;
    max_vars *= 2;
    REALLOC (nodelist, var_t *, max_vars);
    REALLOC (curpath, var_t *, max_vars);
    if (flag) list = nodelist; else list = curpath;
  }
  return list;
}

static
int check_linear_path (var_t *node, edgelist_t *e, var_t *vdd,
		       var_t *gnd, var_t **list, int type)
{
  edgelist_t *f;
  var_t *v;
  int i;
  edgelist_t *x[2];

  while (1) {
    v = e->t1;

    list = ensure_one_slot (list);
    list[num_vars++] = e->gate;

    if ((e->t1 == vdd && type == P_TYPE) ||
	(e->t1 == gnd && type == N_TYPE)) return 0;
    if (e->t1 == vdd || e->t1 == gnd) return -1;

    i = 0;
    for (f = v->edges; f;  f = f->next) {
      if (!is_strong (f->width, f->length)) continue;
      if (f->type != type) continue;
      if (i < 2)
	x[i] = f;
      i++;
    }
    if (i != 2) return -1;
    if (x[0]->t1 == node && x[1]->t1 == node) return -1;
    if (x[0]->t1 != node)
      x[1] = x[0];
    node = v;
    e = x[1];
  }
  return -1;
}

static
double check_simple_folds (var_t *node, int type, var_t *vdd, var_t *gnd)
{
  edgelist_t *e, *f;
  int nedges;
  edgelist_t *x[2];
  int n1;
  int i, j;

  if (max_vars == 0) {
    max_vars = 16;
    MALLOC (nodelist, var_t*, max_vars);
    MALLOC (curpath, var_t*, max_vars);
  }

  nedges = 0;
  for (e = node->edges; e; e = e->next) {
    if (e->type != type) continue;
    if (!is_strong (e->width, e->length)) continue;
    if ((e->gate == ResetVar) || (e->gate == _ResetVar)) continue;
    if ((e->gate == vdd && type == P_TYPE) || (e->gate == gnd && type == N_TYPE))
      continue;
    if (nedges < 2)
      x[nedges] = e;
    nedges++;
  }
  if (nedges != 2) {
    return -1;
  }
  /* really simple folds */
  num_vars = 0;
  if (check_linear_path (node,x[0],vdd,gnd,nodelist,type) < 0) {
    return -1;
  }
  n1 = num_vars;
  num_vars = 0;
  if (check_linear_path (node,x[1],vdd,gnd,curpath,type) < 0) {
    return -1;
  }
  if (n1 != num_vars) return -1;
  num_vars = 0;
  for (i=0; i < n1; i++) {
    for (j=i; j < n1; j++)
      if (nodelist[i] == curpath[j]) break;
    if (j == n1)
      return -1;
    node = curpath[j];
    curpath[j] = curpath[i];
    curpath[i] = node;
  }
  return (double)x[0]->width/(double)x[0]->length + 
    (double)x[1]->width/(double)x[1]->length;
}


/*------------------------------------------------------------------------
 *
 *  weak_drive --
 *
 *   bit0 set: n's not strong enough
 *   bit1 set: p's not strong enough
 *
 *------------------------------------------------------------------------
 */
static
int weak_drive (BOOL_T *B, var_t *node, var_t *vdd, var_t *gnd)
{
  edgelist_t *e, *f;
  double p_strong_ratio, p_weak_ratio;
  double n_strong_ratio, n_weak_ratio;
  double r;
  unsigned int flag = 0;

  p_strong_ratio = n_strong_ratio = 1e6; /* should be large enough? :) */
  n_weak_ratio = p_weak_ratio = 0;

  for (e = node->edges; e; e = e->next) {
    r = (double)e->width/(double)e->length;
    if (e->type == P_TYPE) {
      if (is_strong (e->width, e->length)) {
	/* reset only switches once */
	if (e->gate == ResetVar || e->gate == _ResetVar)
	  continue;
	if (e->gate == vdd)  /* off! */
	  continue;
	/*if (r > p_strong_ratio) p_strong_ratio = r;*/
	if (r < p_strong_ratio) p_strong_ratio = r;
      }
      else {
	/* if the other side of the weak driver is not vdd/gnd, this
	   means that we can add on the width of the next operator, since 
	   all weak drives *must* be staticizers, i.e., inverters */
	if (e->t1 != vdd) {
	  for (f = e->t1->edges; f; f = f->next)
	    if (f->type == P_TYPE && f->t1 == vdd)
	      break;
	  if (f)
	    r = (double)e->width/((double)e->length + f->length);
	}
	if (r > p_weak_ratio) p_weak_ratio = r;
      }
    }
    else {
      if (is_strong (e->width, e->length)) {
	if (e->gate == ResetVar || e->gate == _ResetVar)
	  continue;
	if (e->gate == gnd) /* off! */
	  continue;
	/*if (r > n_strong_ratio) n_strong_ratio = r;*/
	if (r < n_strong_ratio) n_strong_ratio = r;
      }
      else {
	if (e->t1 != gnd) {
	  for (f = e->t1->edges; f; f = f->next)
	    if (f->type == N_TYPE && f->t1 == gnd)
	      break;
	  if (f)
	    r = (double)e->width/((double)e->length + f->length);
	}
	if (r > n_weak_ratio) n_weak_ratio = r;
      }
    }
  }
  if (n_strong_ratio != 0 && p_weak_ratio != 0)
    if ((n_strong_ratio < p_weak_ratio*strength_ratio_dn) &&
	!(node->flags2 & VAR_COMBFEEDBACK_N)) {
      if (verbose > 1) {
	pp_printf (PPout, "N-strong: %.2lf, P-weak: %.2lf",
		   n_strong_ratio, p_weak_ratio);
	pp_forced (PPout, 0);
      }
      flag |= 1;
    }
  if (p_strong_ratio != 0 && n_weak_ratio != 0)
    if ((p_strong_ratio < n_weak_ratio*strength_ratio_up) &&
	!(node->flags2 & VAR_COMBFEEDBACK_P)) {
      r = check_simple_folds (node, P_TYPE, vdd, gnd);
      if (r > 0) p_strong_ratio = r;
      if (p_strong_ratio < n_weak_ratio*strength_ratio_up) {
	if (verbose > 1) {
	  pp_printf (PPout, "P-strong: %.2lf, N-weak: %.2lf",
		     p_strong_ratio, n_weak_ratio);
	  pp_forced (PPout, 0);
	}
	flag |= 2;
      }
    }
  return flag;
}

/*------------------------------------------------------------------------
 *
 *  weak_reset --
 *
 *    1 if reset is too weak to overpower staticizer, 0 otherwise
 *
 *------------------------------------------------------------------------
 */
static
int weak_reset (BOOL_T *B, var_t *node, int type, var_t *reset)
{
  edgelist_t *e;
  int rw, rl, w, l;
  unsigned int flag = 0;
  double strength_ratio;

  node = canonical_name (node);

  for (e = node->edges; e; e = e->next) {
    if (!(flag & 1) && e->type == type && e->gate == reset && 
	driven(B, e->t1, e->type) && is_strong (e->width, e->length)) {
      /* find strong drive */
      rw = e->width;
      rl = e->length;
      flag |= 1;
    }
    else if (!(flag & 2) && e->type != type && driven(B, e->t1,e->type)
	     && !is_strong (e->width, e->length)) {
      w = e->width;
      l = e->length;
      flag |= 2;
    }
    if (flag == 3) break;
  }
  if (flag != 3) return 1;
  strength_ratio = (double)l*rw/(double)(rl*w);
  if (strength_ratio >= 
      (type == P_TYPE ? strength_ratio_up : strength_ratio_dn))
    return 0;
  if (verbose > 1) {
    pp_printf (PPout, "%s: %c-type reset transistor: W=%d, L=%d; W/L=%g", 
	       var_name (node), type == P_TYPE ? 'p' : 'n', 
	       rw, rl, (double)rw/(double)rl);
    pp_forced (PPout, 0);
    pp_printf (PPout, "%s: opposing staticizer: W=%d, L=%d; W/L=%g", 
	       var_name (node), w, l, (double)w/(double)l);
    pp_forced (PPout, 0);
  }
  return 1;
}

/*------------------------------------------------------------------------
 *
 * Check boolean expression equivalence
 *
 *------------------------------------------------------------------------
 */
/*-------------------------------------------------------------------------
 * print a bdd
 *-----------------------------------------------------------------------*/
extern void bool_fprint (FILE *fp, bool_t *b)
{
  if (bool_isleaf(b))
    fprintf (fp, "%s", b->id & 0x01 ? "T" : "F");
  else {
    fprintf (fp, "[%lu,", b->id);
    bool_fprint (fp, b->l);
    fprintf (fp, ",");
    bool_fprint (fp, b->r);
    fprintf (fp, "]");
  }
}

void _bool_clearmk (bool_t *b)
{
  b->mark = 0;
  if (bool_isleaf (b))
    return;
  else {
    if (b->l->mark) _bool_clearmk (b->l);
    if (b->r->mark) _bool_clearmk (b->r);
  }
}

extern int _bool_size (bool_t *b)
{
  int l, r;
  b->mark = 1;
  if (bool_isleaf(b))
    l = r = 0;
  else {
    if (!(b->l->mark))
      l = _bool_size (b->l);
    else
      l = 0;
    if (!(b->r->mark))
      r = _bool_size (b->r);
    else
      r = 0;
  }
  return l+r+1;
}

int bool_size (bool_t *b)
{
  int ret;
  ret = _bool_size (b);
  _bool_clearmk (b);
  return ret;
}

void check_prs (VAR_T *V, BOOL_T *B)
{
  var_t *v, *weak, *chan;
  bool_t *b0, *b1, *b2;
  bool_t *c0, *c1;
  bool_t *s0, *s1, *stmp;
  bool_var_t bool_id;
  int uperr, dnerr;
  int staterr;
  int have_staticizer;
  bool_t *t1,*t2,*t3,*t4;
  var_t *vdd, *gnd;
  bool_t *Inv;
  int staterrflag;
  int flag;
  int flago;
  int nchecked = 0;
  int realchecked = 0;
  int combfeedback;

  vdd = var_locate (V, Vddnode);
  gnd = var_locate (V, GNDnode);

  clearflags (V);

  /* Check production rules */
  find_reset (V, &ResetVar, &_ResetVar);

  if (debug_level > 0)
    pp_printf_raw (PPout, "Checking...\n");

  for (v = var_step_first (V); v; v = var_step_next (V)) {

    if (debug_level > 0) {
      nchecked++;
      if ((nchecked % 10000) == 0)
	pp_printf_raw (PPout, "Done %dK prs check\n", nchecked/1000);
    }
    
    /* only check output nodes */
    if (!(v->flags & VAR_OUTPUT))
      continue;

    if (debug_level > 0) {
      realchecked++;
      if ((realchecked % 10000) == 0)
	pp_printf_raw (PPout, "Done %dK prs\n", realchecked/1000);
    }

    /* don't check cell names! */
    if (v->hcell) continue;

    flag = 0;
    flago = 0;
    if (v->flags & VAR_HIER_NODE) {
      /* skip check if node is driven in the subcell. The node is not
	 driven in the subcell only if it appears as an "i" line in the
	 hxt file, or is not present in the hxt file
       */
      chan = v;
      do {
	if (chan->hname) {
	  if (IS_INPUT(chan->hc)) {
	    flag = 1;
	  }
	  else {
	    flago = 1;
	  }
	}
	chan = chan->alias_ring;
      } while (!(flag && flago) && chan != v);
      if (!flag && !flago) {
	/* not in hxt file! skip check */
	flago = 1;
      }
    }
    /*
      skip check if: output
    */
    if (flago) continue;

    /* canonical name is hierarchical; connection error present
    if (v->hname) continue;
    */

    /* check dangling inverters with weak feedback that are missing
       from the production rule file
    */
    if (v->up[PRSFILE] == NULL && v->dn[PRSFILE] == NULL) {
      /* check if it is a staticizer inverter */
      b0 = (bool_t*)v->up[STRONG];
      b1 = (bool_t*)v->dn[STRONG];
      if (is_an_inverter (V, B, b0, b1, &bool_id)) {
	chan = id_to_var (V, bool_id);
	if (chan && is_an_inverter (V, B, chan->up[WEAK], chan->dn[WEAK], &bool_id) &&
	    ((v->flags & VAR_HAVEVAR) &&  bool_id == v->v))
	  continue;
	/* half staticizer */
	if (chan && is_a_halfinv (V, B, chan->up[WEAK], chan->dn[WEAK],
				  &bool_id) &&
	    ((v->flags & VAR_HAVEVAR) && bool_id == v->v))
	  continue;
      }
    }
    uperr = 0;
    dnerr = 0;

    /* check up guard */
    b0 = expr_to_bool (B, v->up[PRSFILE]);
    if (v->up[STRONG])
      b1 = (bool_t*)v->up[STRONG];
    else
      b1 = bool_false (B);
    if (b0 != b1) {
      uperr = 1;
#if 0
      if (b0 != bool_false (B) && b1 != bool_false (B)) {
#endif
	b2 = bool_not (B, t1 = bool_xor (B, b0, b1));
	bool_free (B, t1);
	Inv = compute_local_p_invariant (v,B,vdd);
	if ((t1 = bool_implies (B, Inv, b2)) == bool_true (B)) {
	  uperr = 0;
	}
	bool_free (B, Inv);
	bool_free (B, t1);
	bool_free (B, b2);
#if 0
      }
#endif
    }
    /* check dn guard */
#ifdef DEBUG
    fprintf (stderr, "CHECK DN: %s\n", var_name (v));
    if (v->dn[PRSFILE]) {
      print_expr2 (PPout, v->dn[PRSFILE]);
      pp_forced (PPout, 0);
    }
#endif
    c0 = expr_to_bool (B, v->dn[PRSFILE]);
#ifdef DEBUG
    fprintf (stderr, "CONV DONE. [%d]\n", bool_size (c0));
    fprintf (stderr, "\n");
#endif

    if (v->dn[STRONG])
      c1 = (bool_t*)v->dn[STRONG];
    else
      c1 = bool_false (B);
    if (c0 != c1) {
      dnerr = 1;
#if 0
      if (c0 != bool_false (B)  && c1 != bool_false (B)) {
#endif
	b2 = bool_not (B, t1 = bool_xor (B, c0, c1));
	bool_free (B, t1);
	Inv = compute_local_n_invariant (v,B,gnd);
	if ((t1 = bool_implies (B, Inv, b2)) == bool_true (B)) {
	  dnerr = 0;
	}
	bool_free (B, Inv);
	bool_free (B, t1);
	bool_free (B, b2);
#if 0
      }
#endif
    }

    combfeedback = 0;

    if (uperr == 1 || dnerr == 1) {
      /* 
       *  Only other option: combinational feedback.
       */
      if (split_combinational_feedback (V, B, v)) {
	uperr = 0;
	dnerr = 0;

	b1 = (bool_t*) v->up[STRONG];

	if (b0 != b1) {
	   uperr = 1;
	   b2 = bool_not (B, t1 = bool_xor (B, b0, b1));
	   bool_free (B, t1);
	   Inv = compute_local_p_invariant (v,B,vdd);
	   if ((t1 = bool_implies (B, Inv, b2)) == bool_true (B)) {
	     uperr = 0;
	   }
	   bool_free (B, Inv);
	   bool_free (B, t1);
	   bool_free (B, b2);
	}
	
	c1 = (bool_t *)v->dn[STRONG];

	if (c0 != c1) {
	  dnerr = 1;

	  b2 = bool_not (B, t1 = bool_xor (B, c0, c1));
	  bool_free (B, t1);
	  Inv = compute_local_n_invariant (v,B,gnd);
	  if ((t1 = bool_implies (B, Inv, b2)) == bool_true (B)) {
	    dnerr = 0;
	  }
	  bool_free (B, Inv);
	  bool_free (B, t1);
	  bool_free (B, b2);
	}

	if (!uperr && !dnerr) {
	  /* node *IS* staticized! */
	  combfeedback = 1;
	}
      }
    }
    /* c0,b0 alloc */


    /* Check ^weak nodes */
    if (!uperr && !dnerr) {
      flag = check_weak_gates (v, vdd, gnd);
      if (flag & 1)
	dnerr = 2;
      if (flag & 2)
	uperr = 2;
    }

    /* Check if the node is staticized properly */
    staterr = 0;
    if (check_staticizers && !uperr && !dnerr) {
      if (v->up[STRONG])
	s0 = (bool_t*)v->up[STRONG];
      else
	s0 = bool_false (B);
      if (v->dn[STRONG])
	s1 = (bool_t*)v->dn[STRONG];
      else
	s1 = bool_false (B);
      /* check for staticizers */
      have_staticizer = 0;
      if (!v->up[WEAK] || !v->dn[WEAK])
	staterr = 1;
      else {
	if (!is_an_inverter (V, B, v->up[WEAK], v->dn[WEAK], &bool_id)) {
	  staterr = 2;
	  /* weak feedback not an inverter. combinational? */
	  if (combfeedback || combinational_feedback (V, B, v)) {
	    staterr = 0;
	  }
	}
	else {
	  weak = id_to_var (V, bool_id);
	  if (!is_an_inverter (V, B, weak->up[STRONG], weak->dn[STRONG], &bool_id) &&
	      !is_an_inverter (V, B, weak->up[WEAK], weak->dn[WEAK], &bool_id))
	    staterr = 3;
	  else {
	    if ((v->flags & VAR_HAVEVAR) && bool_id == v->v)
	      have_staticizer = 1;
	    else 
	      staterr = 5;
	  }
	}
      }
      stmp = bool_or (B, s0, s1);
      if (stmp == bool_true (B)) /* combinational node */ {
	v->flags &= ~VAR_STATEHOLDING;
	staterr = have_staticizer ? 4 : 0;
#if 0
	if (v->flags & VAR_UNSTAT) {
	  staterr = 9;
	}
#endif
      }
      else {
	if (stmp != bool_false (B)) {
	  bool_t *ttmp;
	  bool_t *localnInv, *localpInv, *xtmp;
	  /* check with invariant */

	  localnInv = compute_local_n_invariant (v, B, gnd);
	  localpInv = compute_local_p_invariant (v, B, vdd);
	  xtmp = bool_and (B, localnInv, localpInv);
	  bool_free (B, localnInv);
	  bool_free (B, localpInv);

#if 0
	  printf ("Local inv: ");
	  bool_print (xtmp);
	  printf ("\n");
#endif

	  ttmp = bool_implies (B, xtmp, stmp);
	  bool_free (B, xtmp);

#if 0
	  printf ("Ok, we have: ");
	  bool_print (stmp);
	  printf ("\n");

	  printf ("Simplified: ");
	  bool_print (ttmp);
	  printf ("\n\n");
#endif

	  if (ttmp == bool_true (B)) {
	    bool_free (B, stmp);
	    stmp = ttmp;
	    /* combinational */
	    v->flags &= ~VAR_STATEHOLDING;
	    staterr = have_staticizer ? 4 : 0;
	  }
	  else {
	    bool_free (B, ttmp);
	    v->flags |= VAR_STATEHOLDING;
#if 0
	    if (have_staticizer && (v->flags & VAR_UNSTAT)) {
	      staterr = 8;
	    }
#endif
	  }
	}
      }
      if (stmp == bool_false (B)) /* not being driven; don't complain */
	staterr = 0;
      if (stmp != bool_false (B) && stmp != bool_true (B) && staterr == 0) {
	/* state-holding node, s0 = pull-up, s1 = pull-down */
	Assert (v->up[WEAK], "Huh?");
	Assert (v->dn[WEAK], "What?");
	if ((staterrflag = weak_drive (B, v, vdd, gnd)))
	  staterr = 7;
	if (ResetVar) {
	  /* if G | Reset -> z-, check opposing staticizer */
	  if (ResetVar->flags & VAR_HAVEVAR) {
	    bool_free (B, stmp);
	    stmp = bool_maketrue (B, s1, t1 = bool_var (B, ResetVar->v));
	    bool_free (B, t1);
	    if (stmp == bool_true (B)) {
	      /* check staticizer here */
	      stmp = bool_maketrue (B, v->up[WEAK], t1 = bool_var (B, ResetVar->v));
	      bool_free (B, t1);
	      if (stmp != bool_false (B)) {
		/* ratio check */
		if (weak_reset (B, v, N_TYPE, ResetVar))
		  staterr = 6;
	      }
	    }
	  }
	}
	if (_ResetVar) {
	  /* if G | ~Reset_ -> z+, check opposing staticizer */
	  if (_ResetVar->flags & VAR_HAVEVAR) {
	    stmp = bool_makefalse (B, s0, t1 = bool_var (B, _ResetVar->v));
	    bool_free (B, t1);
	    if (stmp == bool_true (B)) {
	      /* check staticizer here */
	      stmp = bool_makefalse (B, v->dn[WEAK], t1 = bool_var (B, _ResetVar->v));
	      bool_free (B, t1);
	      if (stmp != bool_false (B)) {
		/* ratio check */
		if (weak_reset (B, v, P_TYPE, _ResetVar))
		  staterr = 6;
	      }
	    }
	  }
	}
      }
      if (stmp != bool_true (B)) {
	/* state-holding */
	if (!have_staticizer && (v->flags & VAR_UNSTAT))
	  staterr = 0;
      }
      bool_free (B, stmp);
    }

    if (uperr || dnerr || staterr) {
      if (uperr && dnerr) {
	if (b0 == bool_false (B) && c0 == bool_false (B)) {
	  missing_prs+=2;
	  if (verbose) {
	    pp_printf (PPout, "%s: rules missing from production rule set",
		       var_name (v));
	    pp_forced (PPout,0);
	  }
	  uperr = 0;
	  dnerr = 0;
	}
	else if (b1 == bool_false (B) && c1 == bool_false (B)) {
	  missing_layout+=2;
	  if (verbose) {
	    pp_printf (PPout, "%s: rules missing from layout",
		       var_name (v));
	    pp_forced (PPout,0);
	  }
	  uperr = 0;
	  dnerr = 0;
	}
      }
      if (uperr) {
	v->flags |= VAR_SKIPSNEAKP;
	if (verbose)
	  pp_printf (PPout,"%s: pull-up", var_name(v));
	if (b0 == bool_false (B)) {
	  missing_prs++;
	  if (verbose) pp_printf (PPout, " missing from production rule set");
	}
	else if (b1 == bool_false (B)) {
	  extern struct excl *filterlist;
	  missing_layout++;
	  if (list_member (filterlist, v))
	    known_missing_layout++;
	  if (verbose) pp_printf (PPout, " missing from layout");
	}
	else {
	  prs_differences++;
	  if (verbose) {
	    if (uperr == 1) {
	      pp_printf (PPout, " differs,");
	      pp_forced (PPout, 5);
	      pp_printf (PPout, "prs: ");
	      pp_setb (PPout);
	      print_expr (PPout, v->up[PRSFILE]);
	      pp_endb (PPout);
	      pp_forced (PPout, 5);
	      if (extract_file)
		pp_puts (PPout, "ext: ");
	      else
		pp_puts (PPout, "sim: ");
	      print_slow_special (PPout, B, V, b1, P_TYPE);

	      if (print_differences) {
		b2 = bool_and (B, b0, bool_not (B, b1));
		if (b2 != bool_false (B)) {
		  pp_forced (PPout, 0);
		  if (extract_file)
		    pp_puts (PPout, "prs & ~ext: ");
		  else
		    pp_puts (PPout, "prs & ~sim: ");
		  print_slow_special (PPout, B, V, b2, P_TYPE);
		}
		b2 = bool_and (B, b1, bool_not (B, b0));
		if (b2 != bool_false (B)) {
		  pp_forced (PPout, 0);
		  if (extract_file)
		    pp_puts (PPout, "ext & ~prs: ");
		  else
		    pp_puts (PPout, "sim & ~prs: ");
		  print_slow_special (PPout, B, V, b2, P_TYPE);
		}
	      }
	      pp_forced (PPout, 0);
	    }
	    else if (uperr == 2)
	      pp_printf (PPout, " too weak for ^weak pull-down [ratio=%g]",
			 extra_info2);
	  }
	  else {
	    if (uperr == 1)
	      pp_printf (PPout, "%s: pull-up differs", var_name (v));
	    else if (uperr == 2)
	      pp_printf (PPout, "%s: pull-up too weak for ^weak pull-down",
			 var_name (v));
	    pp_forced (PPout, 0);
	  }
	}
	if (verbose) { pp_forced (PPout,0); }
      }
      if (dnerr) {
	v->flags |= VAR_SKIPSNEAKN;
	if (verbose) pp_printf (PPout,"%s: pull-dn", var_name(v));
	if (c0 == bool_false (B)) {
	  missing_prs++;
	  if (verbose) pp_printf (PPout, " missing from production rule set");
	}
	else if (c1 == bool_false (B)) {
	  missing_layout++;
	  if (verbose) pp_printf (PPout, " missing from layout");
	}
	else {
	  prs_differences++;
	  if (verbose) {
	    if (dnerr == 1) {
	      pp_printf (PPout, " differs,");
	      pp_forced (PPout, 5);
	      pp_printf (PPout, "prs: ");
	      pp_setb (PPout);
	      print_expr (PPout, v->dn[PRSFILE]);
	      pp_endb (PPout);
	      pp_forced (PPout, 5);
	      if (extract_file)
		pp_puts (PPout, "ext: ");
	      else
		pp_puts (PPout, "sim: ");
	      print_slow_special (PPout, B, V, c1, N_TYPE);
	    
	      if (print_differences) {
		b2 = bool_and (B, c0, bool_not (B, c1));
		if (b2 != bool_false (B)) {
		  pp_forced (PPout, 0);
		  if (extract_file)
		    pp_puts (PPout, "prs & ~ext: ");
		  else
		    pp_puts (PPout, "prs & ~sim: ");
		  print_slow_special (PPout, B, V, b2, N_TYPE);
		}
		b2 = bool_and (B, c1, bool_not (B, c0));
		if (b2 != bool_false (B)) {
		  pp_forced (PPout, 0);
		  if (extract_file)
		    pp_puts (PPout, "ext & ~prs: ");
		  else
		    pp_puts (PPout, "sim & ~prs: ");
		  print_slow_special (PPout, B, V, b2, N_TYPE);
		}
	      }
	      pp_forced (PPout,0);
	    }
	    else if (dnerr == 2)
	      pp_printf (PPout, " too weak for ^weak pull-up [ratio=%g]",
			 extra_info1);
	  }
	  else {
	    if (dnerr == 1)
	      pp_printf (PPout, "%s: pull-dn differs", var_name (v));
	    else if (dnerr == 2)
	      pp_printf (PPout, "%s: pull-dn too weak for ^weak pull-up",
			 var_name (v));
	    pp_forced (PPout, 0);
	  }
	}
	if (verbose) pp_forced (PPout, 0);
      }
      if (staterr == 1) {
	extern struct excl *crossinvlist;
	if (verbose) {
	  pp_printf (PPout, "%s: missing staticizer", var_name (v));
	  pp_forced (PPout,0);
	}
	missing_staticizers ++;
	if (list_member (crossinvlist, v))
	  known_missing_staticizers++;
	exit_status = 1;
      }
      else if (staterr == 2) {
	pp_printf (PPout, "%s: weak feedback is not an inverter.",
		   var_name (v));
	pp_forced (PPout,0);
	pp_printf (PPout, " pull-up: "); pp_setb (PPout);
	if (v->up[WEAK])
	  print_slow_special (PPout, B, V, v->up[WEAK], P_TYPE);
	else
	  pp_printf (PPout, "false");
	pp_endb (PPout);
	pp_forced (PPout,0);
	
	pp_printf (PPout, " pull-dn: "); pp_setb (PPout);
	if (v->dn[WEAK])
	  print_slow_special (PPout, B, V, v->dn[WEAK], N_TYPE);
	else
	  pp_printf (PPout, "false");
	pp_endb (PPout);
	pp_forced (PPout,0);
	bad_staticizers++;
	exit_status = 1;
      }
      else if (staterr == 3) {
	pp_printf (PPout, "%s: input to weak inverter (%s) is non-standard.",
		   var_name (v), var_name (weak));
	pp_forced (PPout,0);
	bad_staticizers++;
	exit_status = 1;
      }
      else if (staterr == 4) {
	if (verbose) {
	  pp_printf (PPout, "%s: combinational node is staticized.",
		     var_name (v));
	  pp_forced (PPout, 0);
	}
	extra_staticizers ++;
#if 0
	if (warnings)
	  exit_status = 1;
#endif
      }
      else if (staterr == 5) {
	if (verbose) {
	  pp_printf (PPout, "%s: bad staticizer; not a cross-coupled inverter configuration.",
		     var_name (v));
	  pp_forced (PPout, 0);
	}
	missing_staticizers++;
	exit_status = 1;
      }
      else if (staterr == 6) {
	if (verbose) {
	  pp_printf (PPout, "%s: staticizer might overpower reset transistor.",
		     var_name (v));
	  pp_forced (PPout, 0);
	}
	bad_staticizers++;
	exit_status = 1;
      }
      else if (staterr == 7) {
	if (verbose) {
	  pp_printf (PPout, "%s: staticizer might overpower %s network.",
		     var_name (v), 
		     (staterrflag == 3 ? "n/p" :
		      (staterrflag == 2 ? "p" : "n")));
	  pp_forced (PPout, 0);
	}
	weak_drivers++;
	exit_status = 1;
      }
      else if (staterr == 8) {
	if (verbose) {
	  pp_printf (PPout, "%s: staticized node has `unstaticized' attribute.",
		     var_name (v));
	  pp_forced (PPout, 0);
	}
	extra_staticizers++;
#if 0
	if (warnings)
	  exit_status = 1;
#endif
      }
      else if (staterr == 9) {
	if (verbose) {
	  pp_printf (PPout, "%s: combinational node has `unstaticized' attribute.",
		     var_name (v));
	  pp_forced (PPout, 0);
	}
	bad_staticizers++;
#if 0
	if (warnings)
	  exit_status = 1;
#endif
      }
    }
    bool_free (B, b0);
    bool_free (B, c0);
    bool_gc (B);
  }
  if (!no_sneak_path_check)
    check_sneak_paths (V);
  check_precharges (V,vdd,gnd);
#ifndef DIGITAL_ONLY
  if (!digital_only)
    check_cap_ratios (V);
#endif
  print_prs_summary ();
  print_connect_summary ();
}


/*------------------------------------------------------------------------
 *
 *  Pretty-print production rules
 *
 *------------------------------------------------------------------------
 */
static void
print_one_prs (VAR_T *V, BOOL_T *B, var_t *v)
{
  bool_t *b;

  if (!(v->flags & VAR_OUTPUT))
    return;
  b = v->up[STRONG];
  if (b) {
    print_slow_special (PPdump, B, V, b, P_TYPE);
    pp_lazy (PPdump, 10);
    pp_puts (PPdump, " -> ");
    if (quote_name (var_name (v)))
      pp_printf (PPdump, "\"%s\"", var_name(v));
    else
      pp_puts (PPdump, var_name(v));
    pp_puts (PPdump, "+");
    pp_forced (PPdump, 0);
  }
  b = v->dn[STRONG];
  if (b) {
    print_slow_special (PPdump, B, V, b, N_TYPE);
    pp_lazy (PPdump, 10);
    pp_puts (PPdump, " -> ");
    if (quote_name (var_name(v)))
      pp_printf (PPdump, "\"%s\"", var_name (v));
    else
      pp_puts (PPdump, var_name(v));
    pp_puts (PPdump, "-");
    pp_forced (PPdump, 0);
  }
  pp_forced (PPdump, 0);
}

/*------------------------------------------------------------------------
 *
 *  Pretty-print production rules
 *
 *------------------------------------------------------------------------
 */
void
print_prs (VAR_T *V, BOOL_T *B)
{
  var_t *v;
  bool_t *b;

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    print_one_prs (V, B, v);
  }
  pp_flush (PPdump);
}


/*------------------------------------------------------------------------
 *
 *  Pretty-print alias information
 *
 *------------------------------------------------------------------------
 */
void
print_aliases (VAR_T *V)
{
  var_t *v, *u;

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    u = canonical_name (v);
    if (!(u->flags & (VAR_INPUT|VAR_OUTPUT)))
      continue;
    u = var_nice_alias (v);
    if (u != v) {
      pp_puts (PPdump, "= ");
      if (quote_name (var_string(u)))
	pp_printf (PPdump, "\"%s\"", var_string(u));
      else
	pp_puts (PPdump, var_string(u));
      pp_puts (PPdump, " ");
      if (quote_name (var_string(v)))
	pp_printf (PPdump, "\"%s\"", var_string(v));
      else
	pp_puts (PPdump, var_string(v));
      pp_forced (PPdump, 0);
    }
  }
  pp_flush (PPdump);
}


static
int bang_exists (char *s)
{
  while (*s) {
    if (*s == '!') return 1;
    s++;
  }
  return 0;

}

static
void putstring (FILE *fp, var_t *u)
{
  int i;
  char *s;

  if (use_dot_separator) {
    for (s = var_string(u); *s; s++)
      if (*s == '.') *s = '/';
    if (u->name_convert.ndots > 0) {
      for (i=0; i < u->name_convert.ndots; i++)
	var_string (u)[u->name_convert.pos[i]] = '.';
    }
    fprintf (fp, "\"%s", var_string (u));
    for (s = var_string(u); *s; s++)
      if (*s == '/') *s = '.';
  }
  else 
    fprintf (fp, "\"%s", var_string (u));
  if (u->name_convert.globalnode == 1)
    fprintf (fp, "!");
  fprintf (fp, "\" ");
}

static void dump_node (var_t *v, FILE *fp, var_t *vdd, var_t *gnd)
{
  var_t *u, *w, *x;
  int flag;

start:
  u = v;
  flag = (v->inlayout && v->inprs && !(v->flags & VAR_DUMPEDL)) ? 1 : 0;
  while (u->alias_ring != v && !flag) {
    u = u->alias_ring;
    flag = (u->inprs && u->inlayout && !(u->flags & VAR_DUMPEDL)) ? 1 : 0;
  }
  if (flag == 0) return;
  if ((v->flags & (VAR_DRIVENOUTPUT|VAR_HOUTPUT|VAR_PRSOUTPUT))
      && (v != vdd && v != gnd))
    fprintf (fp, "o ");
  else
    fprintf (fp, "i ");
  
  w = u;
  do {
    u->flags |= VAR_DUMPEDL;
    Assert (u->inlayout, "Something strange is going on . . .");
    putstring (fp, u);
    u = u->alias_ring_prs;
  } while (u != w);
  fprintf (fp, "\n");
  goto start;
}


/*------------------------------------------------------------------------
 *
 *  save_io_nodes --
 *
 *    Save a timestamped version of all layout nodes that are only inputs
 *    or only outputs.
 *
 *------------------------------------------------------------------------
 */
void save_io_nodes (VAR_T *V, FILE *fp, unsigned long stamp)
{
  var_t *v, *vdd, *gnd, *u;
  int c;
  extern int exports_found;

  fprintf (fp, "timestamp%c %lu\n", dump_hier_force ? 'F' : ' ', stamp);

  vdd = var_locate (V, Vddnode);
  gnd = var_locate (V, GNDnode);

  mark_globals (V);

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    if (v != canonical_name (v)) continue;
    c = v->flags & (VAR_INPUT|VAR_PRSOUTPUT|VAR_DRIVENOUTPUT|VAR_HOUTPUT);
    if (v->flags & VAR_GLOBALNODE)
      dump_node (v, fp, vdd, gnd);
    else if (c) {
      if (!exports_found || (v->flags & VAR_EXPORT))
	dump_node (v, fp, vdd, gnd);
    }
  }
}

/*------------------------------------------------------------------------
 *
 *  generate_summary_anyway --
 *
 *     Generate summary info even though there are problems
 *
 *------------------------------------------------------------------------
 */
int
generate_summary_anyway (void)
{
  return
    (missing_layout_connects == missing_global_connects)
    && (missing_prs_connects == 0)
    && (missing_layout == known_missing_layout)
    && (missing_prs == 0)
    && (missing_staticizers == known_missing_staticizers)
    && (extra_staticizers == 0)
    && (bad_staticizers == 0)
    && (weak_drivers == 0)
    && (prs_differences == 0)
    && (sneak_paths == 0)
    && (naming_violations == 0)
    && (io_violations == 0);
}
