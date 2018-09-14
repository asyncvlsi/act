/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

#include <stdio.h>
#include "lvs.h"
#include "var.h"
#include "misc.h"

static
edgelist_t **Edges;
static int nvars = 0;
static int maxvars = 0;


#define ATTR_PRUNE (VAR_PCHG_ALL|VAR_VOLTAGE_CONV)

static
void addedge (edgelist_t *e)
{
  if (nvars == maxvars) {
    if (maxvars == 0) {
      maxvars = 16;
      MALLOC (Edges, edgelist_t *, maxvars);
    }
    else {
      maxvars *= 2;
      REALLOC (Edges, edgelist_t *, maxvars);
    }
  }
  Edges[nvars++] = e;
}

static
void delvar (edgelist_t *e)
{
  Assert (e == Edges[--nvars], "Hmm");
}


static
void dump_path (void)
{
  int i;
  for (i=0; i < nvars; i++) {
    pp_printf (PPout, " %s", var_name (Edges[i]->gate),
	       var_name (Edges[i]->t1));
    if (i != nvars-1) {
      pp_puts (PPout, ",");
      pp_lazy (PPout, 2);
    }
  }
}

static var_t *revnode;

static
void dump_path_rev (int type)
{
  int i;

  if (type == N_TYPE)
    pp_printf (PPout, "ndn@ on %s [GND path]", var_name (revnode));
  else
    pp_printf (PPout, "pup@ on %s [Vdd path]", var_name (revnode));
  pp_forced (PPout, 0);
  
  for (i = nvars-1; i >= 0; i--)
    pp_printf (PPout, " %s", var_name (Edges[i]->gate));
  pp_forced (PPout, 0);
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

static
void dump_out_path (var_t *v, int type, var_t *supply, var_t *other)
{
  edgelist_t *e;

  Assert (!(v->flags & VAR_VISITED), "Hmm");
  v->flags |= VAR_VISITED;

  for (e = v->edges; e; e = e->next) {
    if (!is_strong (e->width, e->length)) continue;
    if (e->type != type) continue;
    if (e->t1 == other) continue;
    if (e->t1->flags & VAR_VISITED) continue;
    addedge (e);
    if (e->t1 == supply) {
      dump_path_rev (type);
      delvar (e);
      continue;
    }
    dump_out_path (e->t1, type, supply, other);
    delvar (e);
  }
  
  v->flags &= ~VAR_VISITED;
}


static
void echo_path (var_t *v, int type)
{
  int i;

  if (v->flags2 & VAR_VOLTAGE_CONV) return;

  if (type == N_TYPE) 
    v->flags2 |= VAR_CHECK_PDN;
  else
    v->flags2 |= VAR_CHECK_NUP;

  if (!dump_pchg_paths) return;

  if (v->flags2 & VAR_PCHG)
    pp_printf (PPout, "pchg@ on %s", var_name (v));
  else if (v->flags2 & VAR_PCHG_NUP)
    pp_printf (PPout, "nup@ on %s", var_name (v));
  else if (v->flags2 & VAR_PCHG_NDN)
    pp_printf (PPout, "ndn@ on %s", var_name (v));
  else if (v->flags2 & VAR_PCHG_PUP)
    pp_printf (PPout, "pup@ on %s", var_name (v));
  else if (v->flags2 & VAR_PCHG_PDN)
    pp_printf (PPout, "pdn@ on %s", var_name (v));
  if (type == P_TYPE) 
    pp_printf (PPout, " [Vdd path]");
  else
    pp_printf (PPout, " [GND path]");
  pp_forced (PPout, 0);

  for (i=0; i < nvars; i++)
    pp_printf (PPout, " %s", var_name (Edges[i]->gate));
  pp_forced (PPout, 0);
}

static
void _search_path (var_t *vdd, var_t *gnd, var_t *v, int type)
{
  edgelist_t *e;

  Assert (!(v->flags & VAR_VISITED), "Huh?");
  v->flags |= VAR_VISITED;
  
  if (v->flags & VAR_OUTPUT) {
    inc_pchg_errors ();
    pp_printf (PPout, "%c-type path, %s to %s:", 
	       type == P_TYPE ? 'N' : 'P',
	       type == P_TYPE ? "Vdd" : "GND",
	       var_name (v));
    dump_path ();
    pp_forced (PPout, 0);
    v->flags &= ~VAR_VISITED;
    return;
  }

  for (e = v->edges; e; e = e->next) {
    if (e->type == type) continue;
    if (e->t1 == vdd || e->t1 == gnd || (e->t1->flags & VAR_VISITED)) continue;
    if (e->t1->flags2 & ATTR_PRUNE) {
      addedge (e);
      echo_path (e->t1, type);
      delvar (e);
      continue;
    }
    addedge (e);
    _search_path (vdd, gnd, e->t1, type);
    delvar (e);
  }
  v->flags &= ~VAR_VISITED;
}

static
void _clr_path (var_t *vdd, var_t *gnd, var_t *v, int type)
{
  edgelist_t *e;

  Assert ((v->flags & VAR_VISITED), "Huh?");
  v->flags &= ~VAR_VISITED;

  if (v->flags & VAR_OUTPUT) return;

  for (e = v->edges; e; e = e->next) {
    if (e->type == type) continue;
    if (e->t1 == vdd || e->t1 == gnd || !(e->t1->flags & VAR_VISITED)) 
      continue;
    if (e->t1->flags2 & ATTR_PRUNE) continue;
    _clr_path (vdd, gnd, e->t1, type);
  }
}

static
void search_path (var_t *v, var_t *vdd, var_t *gnd, int type)
{
  if (v == vdd || v == gnd) return;
  if (v->flags2 & ATTR_PRUNE) {
    echo_path (v, type);
    return;
  }
  _search_path (vdd, gnd, v, type);
  /*  _clr_path (vdd, gnd, v, type);*/
}
  

void check_precharges (VAR_T *V, var_t *vdd, var_t *gnd)
{
  edgelist_t *e;
  var_t *v;
  int both = 0;

  if (vdd) {
    vdd->flags |= VAR_VISITED;
    for (e = vdd->edges; e; e = e->next) {
      if (e->type != P_TYPE) {
	addedge (e);
	search_path (e->t1,vdd,gnd,P_TYPE);
	delvar (e);
      }
    }
    vdd->flags &= ~VAR_VISITED;
  }

  if (gnd) {
    gnd->flags |= VAR_VISITED;
    for (e = gnd->edges; e; e = e->next) {
      if (e->type != N_TYPE) {
	addedge (e);
	search_path (e->t1,vdd,gnd, N_TYPE);
	delvar (e);
      }
    }
    gnd->flags &= ~VAR_VISITED;
  }

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    if (v->flags & VAR_VISITED) printf ("Uh oh!\n");
    if (v->flags2 & VAR_PCHG_ALL) {
      both = -1;
      for (e = v->edges; e; e = e->next)
	if (both >= 0 && e->type != both) both = -2;
	else if (both == -1)
	  both = e->type;

      if (((v->flags2 & VAR_PCHG_NUP) && !(v->flags2 & VAR_CHECK_NUP))
	  || ((v->flags2 & VAR_PCHG) && both == N_TYPE && 
	      !(v->flags2 & VAR_CHECK_NUP))) {
	pp_printf (PPout, "%s: nup@/pchg@ attribute, no path from Vdd found",
		   var_name (v));
	pp_forced (PPout, 0);
	inc_pchg_errors ();
      }
      if (((v->flags2 & VAR_PCHG_PDN) && !(v->flags2 & VAR_CHECK_PDN))
	  || ((v->flags2 & VAR_PCHG) && both == P_TYPE &&
	      !(v->flags2 & VAR_CHECK_PDN))) {
	pp_printf (PPout, "%s: pdn@/pchg@ attribute, no path from GND found",
		   var_name (v));
	pp_forced (PPout, 0);
	inc_pchg_errors ();
      }
      if ((v->flags2 & VAR_PCHG) && both == -2) {
	pp_printf (PPout, "WARNING: %s: pchg@ attribute, both p and n fets?",
		   var_name (v));
	pp_forced (PPout, 0);
      }
      if (v->flags2 & VAR_PCHG_PUP) {
	if (both != -2) {
	  pp_printf (PPout, "%s: pup@ attribute, only one type of fet",
		     var_name (v));
	  pp_forced (PPout, 0);
	  inc_pchg_errors ();
	}
	else {
	  if (dump_pchg_paths) {
	    revnode = v;
	    dump_out_path (v,P_TYPE, vdd, gnd);
	  }
	}
      }
      if (v->flags2 & VAR_PCHG_NDN) {
	if (both != -2) {
	  pp_printf (PPout, "%s: ndn@ attribute, only one type of fet",
		     var_name (v));
	  pp_forced (PPout, 0);
	  inc_pchg_errors ();
	}
	else {
	  if (dump_pchg_paths) {
	    revnode = v;
	    dump_out_path (v, N_TYPE, gnd, vdd);
	  }
	}
      }
    }
  }
}
