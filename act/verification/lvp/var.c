/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

/*
 *
 * Hashtable implementation of a variable table.
 *
 */

#include <stdio.h>
#include <string.h>
#include "var.h"
#include <common/misc.h>

/*
 *------------------------------------------------------------------------
 *
 *  Initialize table.
 *
 *------------------------------------------------------------------------
 */
VAR_T *var_init (void)
{
  VAR_T *V;

  MALLOC(V,VAR_T,1);
  V->H = hash_new (VAR_INIT_SIZE);
  V->step = -1;
  V->e = NULL;
  return V;
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

var_t *var_nice_alias (var_t *v)
{
  var_t *u, *best;
  int bestlen, len;
  int bestdots, dots;

  while (v->alias) v = v->alias;
  bestlen = strlen (v->s);
  best = v;
  bestdots = countdots (v->s);
  for (u = v->alias_ring; u != v; u = u->alias_ring) {
    dots = countdots (u->s);
    len = strlen (u->s);
    if (dots < bestdots || (dots == bestdots && len < bestlen)) {
      bestdots = dots;
      bestlen = len;
      best = u;
    }
  }
  return best;
}


var_t *var_nice_prs_alias (var_t *v)
{
  var_t *u, *best;
  int bestlen, len;
  int bestdots, dots;

  while (v->alias_prs) v = v->alias_prs;
  bestlen = strlen (v->s);
  best = v;
  bestdots = countdots (v->s);
  for (u = v->alias_ring_prs; u != v; u = u->alias_ring_prs) {
    dots = countdots (u->s);
    len = strlen (u->s);
    if (dots < bestdots || (dots == bestdots && len < bestlen)) {
      bestdots = dots;
      bestlen = len;
      best = u;
    }
  }
  return best;
}


char *var_name (var_t *v)
{
  v = var_nice_alias (v);
  return v->s;
}

char *var_prs_name (var_t *v)
{
  v = var_nice_prs_alias (v);
  return v->s;
}

/*
 *------------------------------------------------------------------------
 *
 * Enter variable in table.
 *
 *------------------------------------------------------------------------
 */
var_t *var_enter (VAR_T *V, char *s)
{
  hash_bucket_t *e; 
  var_t *v;

  e = hash_lookup (V->H, s);
  if (!e) {
    e = hash_add (V->H, s);
    MALLOC (v,var_t,1);
    v->s = e->key;
    v->up[0] = NULL;
    v->dn[0] = NULL;
    v->up[1] = NULL;
    v->dn[1] = NULL;
    v->up[2] = NULL;
    v->dn[2] = NULL;
    v->checkedprs = 0;
    v->checkedlayout = 0;
    v->checkedio = 0;
    v->flags = 0;
    v->flags2 = 0;
    v->edges = NULL;
    v->alias = NULL;
    v->alias_prs = NULL;
    v->alias_ring = v;
    v->alias_ring_prs = v;
    v->worklist = NULL;
    v->exclhi = v;
    v->excllo = v;
    v->inprs = 0;
    v->inlayout = 0;
    v->done = 0;
    v->must_have_wf = 0;
    v->hcell = 0;
    v->hname = 0;
    v->hc = NULL;
#ifndef DIGITAL_ONLY
    v->c.goodcap = 0.0;
    v->c.p_area = v->c.p_perim = 0;
    v->c.n_area = v->c.n_perim = 0;
    v->c.p_gA = v->c.p_gP = 0;
    v->c.n_gA = v->c.n_gP = 0;
    v->c.f[P_TYPE] = v->c.f[N_TYPE] = NULL;
    v->c.cap = NULL;
    v->c.badcap = 0;
    v->pthresh = v->nthresh = -1;
    v->space = NULL;
#endif
    init_dots (&v->name_convert);
    e->v = (void*)v;
  }
  else {
    v = (var_t*)e->v;
  }
  return v;
}


/*
 *------------------------------------------------------------------------
 *
 * Locate variable
 *
 *------------------------------------------------------------------------
 */
var_t *var_locate (VAR_T *V, char *s)
{
  hash_bucket_t *e;
  var_t *v;

  e = hash_lookup (V->H, s);
  if (!e)
    return NULL;
  else {
    v = (var_t*)e->v;
    while (v->alias) v = v->alias;
    return v;
  }
}


/*
 *------------------------------------------------------------------------
 *
 * Locate variable
 *
 *------------------------------------------------------------------------
 */
var_t *var_find (VAR_T *V, char *s)
{
  hash_bucket_t *e;

  e = hash_lookup (V->H, s);
  if (!e)
    return NULL;
  else
    return (var_t*)e->v;
}


/*
 *------------------------------------------------------------------------
 *
 * Return the number of variables in the table.
 *
 *------------------------------------------------------------------------
 */
int var_number (VAR_T *V)
{
  return V->H->n;
}


/*
 *------------------------------------------------------------------------
 *
 * Iterators for table
 *
 *------------------------------------------------------------------------
 */
var_t *var_step_first (VAR_T *V)
{
  for (V->step = 0; V->step < V->H->size; V->step++)
    for (V->e = V->H->head[V->step]; V->e; V->e = V->e->next)
      return (var_t*)V->e->v;
  return NULL;
}


var_t *var_step_next (VAR_T *V)
{
  var_t *v;
  if (V->e && V->e->next) {
    V->e = V->e->next;
    v = (var_t*)V->e->v;
    return v;
  }
  V->step++;
  for (; V->step < V->H->size; V->step++)
    for (V->e = V->H->head[V->step]; V->e; V->e = V->e->next)
      return (var_t*)V->e->v;
  return NULL;
}


/*
 *------------------------------------------------------------------------
 *
 * Free space associated with variable table.
 *
 *------------------------------------------------------------------------
 */
void var_free (VAR_T *V)
{
  hash_free (V->H);
  FREE (V);
}


var_t *canonical_name (var_t *v)
{
  var_t *o = v, *root;

  while (v->alias)
    v = v->alias;
  root = v;
  if (root != o) {
    while (o->alias) {
      v = o->alias;
      o->alias = root;
      o = v;
    }
  }
  return root;
}
