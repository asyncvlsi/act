/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#include <stdio.h>
#include "lvs.h"
#include "parse.h"
#include "act/common/misc.h"

extern var_t *ResetVar, *_ResetVar;
static var_t *vdd, *gnd;

static var_t **output_scc_nodes;
static int noutputs;
    /* used to store list of output nodes when checking sneak paths */

static var_t **edge_list;
static int edges = 0;
static int nedges;
    /* used to store list of edges so far in the search tree */


/*------------------------------------------------------------------------
 *
 *  1 if (v1,v2) are exclusive of the type specified.
 *  type == N_TYPE: exclhi
 *  type == P_TYPE: excllo
 *
 *------------------------------------------------------------------------
 */
static
int exclusive_nodes (var_t *v1, var_t *v2, int type)
{
  var_t *t;

  v1 = canonical_name (v1);
  v2 = canonical_name (v2);
  if (v1 == v2) return 0;
  if (type == N_TYPE) {
    if (v1->exclhi == v1 || v2->exclhi == v2) return extra_exclhi (v1, v2);
    for (t = v1->exclhi; t != v1; t = t->exclhi)
      if (canonical_name(t) == v2) return 1;
    return extra_exclhi (v1, v2);
  }
  else {
    if (v1->excllo == v1 || v2->excllo == v2) return extra_excllo (v1, v2);
    for (t = v1->excllo; t != v1; t = t->excllo)
      if (canonical_name(t) == v2) return 1;
    return extra_excllo (v1, v2);
  }
  /*NOTREACHED*/
  return 0;
}

/*------------------------------------------------------------------------
 *
 *  prune_paths --
 *
 *   Pruning heuristic: prune from vdd/gnd where one can show there
 *   cannot be any sneak paths present; in the case of nand, nor, gen.
 *   c-element style pull-up/pull-downs, the check is vacuous.
 *
 *------------------------------------------------------------------------
 */
static void prune_paths (var_t *supply, int type)
{
  var_t *hd, *tl;
  edgelist_t *e;
  var_t *t;
  int nout;

  tl = hd = supply;
  supply->flags |= VAR_VISITED;
  supply->flags |= VAR_PRUNE(type);

  /* visited == on the worklist */

  while (hd) {
    nout = 0;
    if (hd != supply) {
      t = NULL;
      for (e = hd->edges; e; e = e->next) {
	if (e->isweak) continue;
	if (e->type != type) continue;
	if (e->t1->flags & VAR_PRUNE(type)) continue;
	if (t != e->t1) {
	  nout++;
	  t = e->t1;
	}
      }
    }
    /* delete hd from the worklist */
    if (nout < 2) {
      /* no branching */
      hd->flags |= VAR_PRUNE(type);
      /* put its neighbors on the check-pruned list iff non-output nodes */
      for (e = hd->edges; e ; e = e->next) {
	if (e->isweak) continue;
	if (e->type != type) continue;
	if (e->t1->flags & (VAR_OUTPUT|VAR_PRUNE(type)|VAR_VISITED)) continue;
	e->t1->flags |= VAR_VISITED;
	tl->worklist = e->t1;
	tl = e->t1;
	tl->worklist = NULL;
      }
    }
    t = hd;
    hd = hd->worklist;
    t->worklist = NULL;
    t->flags &= ~VAR_VISITED;
  }
}

/*------------------------------------------------------------------------
 *
 *  dfs to check for sneak paths
 *
 *------------------------------------------------------------------------
 */
static var_t *run_dfs (var_t *v, int type)
{
  var_t *tmp;
  edgelist_t *e;
  int i;
  
  v->flags |= VAR_VISITED;
  for (e = v->edges; e ; e = e->next) {
    if (e->isweak || (e->type != type) || (e->t1->flags & VAR_PRUNE(type))
	/* no need to visit this edge */
	|| (e->t1->flags & VAR_VISITED) 
    	   /* back edge */
	|| (e->t1 == vdd || e->t1 == gnd || (e->t1->flags & VAR_RESET_SUPPLY))
	   /* to power supply */
	|| ((e->gate == vdd || e->gate == _ResetVar) && type == P_TYPE) 
	   /* thru cut-off transistor */
	|| ((e->gate == gnd || e->gate == ResetVar) && type == N_TYPE)
   	   /* to power supply, or thru cut-off transistor */
	)
      continue;
    for (i=0; i < edges; i++)
      if (exclusive_nodes (e->gate, edge_list[i],type)) break;
    if (i != edges) continue;
    /* traverse this edge */
    if (edges == nedges) {
      nedges *= 2;
      REALLOC (edge_list, var_t *, nedges);
    }
    edge_list[edges++] = e->gate;
    if ((e->t1->flags & VAR_OUTPUT) && 
	!(e->t1->flags & (type == P_TYPE ? VAR_SKIPSNEAKP : VAR_SKIPSNEAKN))) {
      /* short to another output node */
      v->flags &= ~VAR_VISITED;
      return e->t1;
    }
    if ((tmp = run_dfs (e->t1,type))) {
      v->flags &= ~VAR_VISITED;
      return tmp;
    }
    edges--;
  }
  v->flags &= ~VAR_VISITED;
  return NULL;
}


static void check_sneak_scc_p (var_t *v, var_t *gnd, var_t *vdd)
{
  var_t *hd, *tl, *t;
  edgelist_t *e;
  int outputs = 0;
  int i, j;

  hd = v;
  tl = v;
  hd->flags |= VAR_SNEAKEDP;

  /*
   * mark nodes in scc with VAR_SNEAKEDN, and clear their dn fields.
   */
  while (hd) {
    if (hd->flags & VAR_OUTPUT) {
      if (outputs == noutputs) {
	noutputs *= 2;
	REALLOC (output_scc_nodes, var_t *, noutputs);
      }
      output_scc_nodes[outputs++] = hd;
    }
    for (e = hd->edges; e; e = e->next) {
      if (e->isweak || e->type == N_TYPE || e->t1 == gnd) continue;
      if (e->t1->flags & VAR_PRUNEP) continue;
      if (e->t1->flags & VAR_SNEAKEDP) continue;
      e->t1->flags |= VAR_SNEAKEDP;
      tl->worklist = e->t1;
      tl = e->t1;
      tl->worklist = NULL;
    }
    t = hd;
    hd = hd->worklist;
    t->worklist = NULL;
  }

  if (outputs == 1) return;

  for (i=0; i < outputs; i++) {
    if (output_scc_nodes[i] == NULL) continue;
    if (output_scc_nodes[i]->flags & VAR_SKIPSNEAKP) continue;
    edges = 0;
    if ((hd = run_dfs (output_scc_nodes[i],P_TYPE))) {
      for (j=i+1; j < outputs; j++)
	if (output_scc_nodes[j] == hd) output_scc_nodes[j] = NULL;
      pp_printf (PPout, "Sneak path P: %s <-> %s", 
		 var_name(output_scc_nodes[i]), var_name(hd));
      pp_forced (PPout, 0);
      inc_sneak_paths ();
      if (verbose) {
	pp_puts (PPout, "     ");
	pp_setb (PPout);
	for (j=0; j < edges; j++) {
	  pp_puts (PPout, var_name (edge_list[j]));
	  if (j != (edges-1)) {
	    pp_puts (PPout, ", ");
	    pp_lazy (PPout, 3);
	  }
	}
	pp_endb (PPout);
	pp_forced (PPout, 0);
      }
    }
  }

}

/*------------------------------------------------------------------------
 *
 *  check_sneak_scc_n --
 *
 *    Look for sneak paths in a strongly connected component of the graph.
 *
 *------------------------------------------------------------------------
 */
static void check_sneak_scc_n (var_t *v, var_t *gnd, var_t *vdd)
{
  var_t *hd, *tl, *t;
  edgelist_t *e;
  int outputs = 0;
  int i, j;

  hd = v;
  tl = v;
  hd->flags |= VAR_SNEAKEDN;

  /*
   * mark nodes in scc with VAR_SNEAKEDN, and clear their dn fields.
   */
  while (hd) {
    if (hd->flags & VAR_OUTPUT) {
      if (outputs == noutputs) {
	noutputs *= 2;
	REALLOC (output_scc_nodes, var_t *, noutputs);
      }
      output_scc_nodes[outputs++] = hd;
    }
    for (e = hd->edges; e; e = e->next) {
      if (e->isweak || e->type == P_TYPE || e->t1 == vdd) continue;
      if (e->t1->flags & VAR_PRUNEN) continue;
      if (e->t1->flags & VAR_SNEAKEDN) continue;
      e->t1->flags |= VAR_SNEAKEDN;
      tl->worklist = e->t1;
      tl = e->t1;
      tl->worklist = NULL;
    }
    t = hd;
    hd = hd->worklist;
    t->worklist = NULL;
  }

  if (outputs == 1) return;

  for (i=0; i < outputs; i++) {
    if (output_scc_nodes[i] == NULL) continue;
    if (output_scc_nodes[i]->flags & VAR_SKIPSNEAKN) continue;
    edges = 0;
    if ((hd = run_dfs (output_scc_nodes[i],N_TYPE))) {
      for (j=i+1; j < outputs; j++)
	if (output_scc_nodes[j] == hd) output_scc_nodes[j] = NULL;
      pp_printf (PPout, "Sneak path N: %s <-> %s", 
		 var_name(output_scc_nodes[i]), var_name(hd));
      pp_forced (PPout, 0);
      inc_sneak_paths ();
      if (verbose) {
	pp_puts (PPout, "     ");
	pp_setb (PPout);
	for (j=0; j < edges; j++) {
	  pp_puts (PPout, var_name (edge_list[j]));
	  if (j != (edges-1)) {
	    pp_puts (PPout, ", ");
	    pp_lazy (PPout, 3);
	  }
	}
	pp_endb (PPout);
	pp_forced (PPout, 0);
      }
    }
  }
}


/*------------------------------------------------------------------------
 *
 * check_sneak_paths --
 *
 *   Search graph for sneak paths between different output nodes.
 *  *NOTE* This function changes v->up[STRONG] and v->dn[STRONG].
 *
 *------------------------------------------------------------------------
 */
void check_sneak_paths (VAR_T *V)
{
  var_t *v;
  edgelist_t *e;

  vdd = var_locate (V, Vddnode);
  gnd = var_locate (V, GNDnode);
  
  MALLOC (output_scc_nodes, var_t *, 10);
  noutputs = 10;
  MALLOC (edge_list, var_t *, 100);
  nedges = 100;

  if (vdd) vdd->flags |= VAR_PRUNEN|VAR_PRUNEP;
  if (gnd) gnd->flags |= VAR_PRUNEN|VAR_PRUNEP;

  if (vdd) prune_paths (vdd, P_TYPE);
  if (gnd) prune_paths (gnd, N_TYPE);

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    if (!(v->flags & VAR_OUTPUT)) continue;
    if (!(v->flags & VAR_PRUNEP) && !(v->flags & VAR_SNEAKEDP) 
	&& !(v->flags & VAR_SKIPSNEAKP))
      check_sneak_scc_p (v,gnd,vdd);
    if (!(v->flags & VAR_PRUNEN) && !(v->flags & VAR_SNEAKEDN)
	&& !(v->flags & VAR_SKIPSNEAKN))
      check_sneak_scc_n (v,gnd,vdd);
  }
  FREE (output_scc_nodes);
  FREE (edge_list);
  output_scc_nodes = NULL;
  edge_list = NULL;
}
