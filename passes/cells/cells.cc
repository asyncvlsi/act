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
#include <stdio.h>
#include <string.h>
#include <set>
#include <map>
#include <utility>
#include <act/passes/cells.h>

static struct cHashtable *cell_table = NULL;
static std::set<Process *> *visited_procs = NULL;
static ActNamespace *cell_ns = NULL;


L_A_DECL (act_prs_expr_t *, pending);

static int cell_hashfn (int sz, void *key)
{
  struct act_prsinfo *ckey = (struct act_prsinfo *)key;
  unsigned int h;
  int i;

  h = hash_function_continue (sz, (unsigned char *)&ckey->nvars,
			      sizeof (int), 0, 0);
  h = hash_function_continue (sz, (unsigned char *)&ckey->nout,
			      sizeof (int), h, 1);
  h = hash_function_continue (sz, (unsigned char *)&ckey->nat,
			      sizeof (int), h, 1);
  h = hash_function_continue (sz, (unsigned char *)&ckey->tval,
			      sizeof (int), h, 1);

  for (i=0; i < A_LEN (ckey->attrib); i++) {
    h = hash_function_continue (sz, (unsigned char *)&ckey->attrib[i].nup,
				sizeof (int), h, 1);
    h = hash_function_continue (sz, (unsigned char *)&ckey->attrib[i].ndn,
				sizeof (int), h, 1);
    h = hash_function_continue (sz, (unsigned char *)&ckey->attrib[i].tree,
				sizeof (char), h, 1);
  }
  
  return h;
}

static void _propagate_sz (act_prs_expr_t *e, act_size_spec_t **sz)
{
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    _propagate_sz (e->u.e.l, sz);
    _propagate_sz (e->u.e.r, sz);
    break;

  case ACT_PRS_EXPR_NOT:
    _propagate_sz (e->u.e.l, sz);
    break;

  case ACT_PRS_EXPR_LABEL:
    break;
    
  case ACT_PRS_EXPR_VAR:
    if (e->u.v.sz) {
      *sz = e->u.v.sz;
    }
    else {
      e->u.v.sz = *sz;
    }
    break;
    
  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    Assert (0, "Loops?!");
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;
  }
}


/*
  non-zero if equal, 0 otherwise

  flip = 0, compare a to b
  flip = 1, compare a to NOT b
*/
static int _equal_expr (act_prs_expr_t *a, act_prs_expr_t *b, int *perm, int flip)
{
  int bid;
  int btype;
  if (!a && !b) return 1;
  if (!a || !b) return 0;
  btype = b->type;
  if (flip) {
    if (b->type == ACT_PRS_EXPR_NOT) {
      flip = 0;
      b = b->u.e.l;
    }
    else if (a->type == ACT_PRS_EXPR_NOT) {
      flip = 0;
      a = a->u.e.l;
    }
    else if (b->type == ACT_PRS_EXPR_AND) {
      btype = ACT_PRS_EXPR_OR;
    }
    else if (b->type == ACT_PRS_EXPR_OR) {
      btype = ACT_PRS_EXPR_AND;
    }
  }
  if ((a->type != b->type) && (flip == 0)) {
    if (a->type == ACT_PRS_EXPR_NOT) {
      return _equal_expr (a->u.e.l, b, perm, 1);
    }
    else if (b->type == ACT_PRS_EXPR_NOT) {
      return _equal_expr (a, b->u.e.l, perm, 1);
    }
    return 0;
  }
  if (flip && (a->type != btype)) return 0;
  
  switch (a->type) {
  case ACT_PRS_EXPR_AND:
    return _equal_expr (a->u.e.l, b->u.e.l, perm, flip) &&
      _equal_expr (a->u.e.r, b->u.e.r, perm, flip) &&
      _equal_expr (a->u.e.pchg, b->u.e.pchg, perm, 0) &&
      (a->u.e.pchg ? (a->u.e.pchg_type == b->u.e.pchg_type) : 1);
    break;

  case ACT_PRS_EXPR_OR:
    return ((_equal_expr (a->u.e.l, b->u.e.l, perm, flip) &&
	    _equal_expr (a->u.e.r, b->u.e.r, perm, flip)) ||
	    (_equal_expr (a->u.e.l, b->u.e.r, perm, flip) &&
	     _equal_expr (a->u.e.r, b->u.e.l, perm, flip))) &&
      _equal_expr (a->u.e.pchg, b->u.e.pchg, perm, 0) &&
      (a->u.e.pchg ? (a->u.e.pchg_type == b->u.e.pchg_type) : 1);
    break;
    
  case ACT_PRS_EXPR_NOT:
    return _equal_expr (a->u.e.l, b->u.e.l, perm, flip);
    break;

  case ACT_PRS_EXPR_VAR:
    bid = (unsigned long long)b->u.v.id;
    if (perm) bid = perm[bid];
    //if (!a->u.v.id->isEqual (b->u.v.id)) return 0;
    if ((unsigned long long)a->u.v.id != bid) return 0;
    if (a->u.v.sz && !b->u.v.sz) return 0;
    if (!a->u.v.sz && b->u.v.sz) return 0;
    if (a->u.v.sz && b->u.v.sz) {
      if (a->u.v.sz->flavor != b->u.v.sz->flavor) return 0;
      if (!expr_equal (a->u.v.sz->w, b->u.v.sz->w)) return 0;
      if (!expr_equal (a->u.v.sz->l, b->u.v.sz->l)) return 0;
    }
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("loops in prs?");
    break;

  case ACT_PRS_EXPR_LABEL:
    bid = (unsigned long)b->u.l.label;
    if (perm) bid = perm[bid];
    if ((unsigned long)a->u.l.label != bid) return 0;
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    return 1;
    break;
  }
  return 1;
}

/*
  perm: permutation of k2; var i from k1 -> perm[i] in k2
*/
static int match_prsinfo (struct act_prsinfo *k1,
			   struct act_prsinfo *k2,
			   int *perm)
{
  int i;

  if (k1->nvars != k2->nvars) return 0;
  if (k1->nout != k2->nout) return 0;
  if (k1->tval != k2->tval) return 0;
  if (k1->nat != k2->nat) return 0;
  
  if (A_LEN (k1->up) != A_LEN (k2->up)) return 0;
  if (A_LEN (k1->dn) != A_LEN (k2->dn)) return 0;
  if (A_LEN (k1->attrib) != A_LEN (k2->attrib)) return 0;

  for (i=0; i < A_LEN (k1->attrib); i++) {
    if (perm) {
      if (k1->attrib[i].tree != k2->attrib[perm[i]].tree) return 0;
      if (k1->attrib[i].nup != k2->attrib[perm[i]].nup) return 0;
      if (k1->attrib[i].ndn != k2->attrib[perm[i]].ndn) return 0;
      for (int j=0; j < k1->attrib[i].nup + k1->attrib[i].ndn; j++) {
	if (k1->attrib[i].depths[j] != k2->attrib[perm[i]].depths[j]) return 0;
      }
    }
    else {
      if (k1->attrib[i].tree != k2->attrib[i].tree) return 0;
      if (k1->attrib[i].nup != k2->attrib[i].nup) return 0;
      if (k1->attrib[i].ndn != k2->attrib[i].ndn) return 0;
      for (int j=0; j < k1->attrib[i].nup + k1->attrib[i].ndn; j++) {
	if (k1->attrib[i].depths[j] != k2->attrib[i].depths[j]) return 0;
      }
    }
  }

  if (perm) {
    for (i=0; i < A_LEN (k1->up); i++) {
      if (!_equal_expr (k1->up[i], k2->up[perm[i]], perm, 0)) return 0;
    }
    for (i=0; i < A_LEN (k1->dn); i++) {
      if (!_equal_expr (k1->dn[i], k2->dn[perm[i]], perm, 0)) return 0;
    }
  }
  else {
    for (i=0; i < A_LEN (k1->up); i++) {
      if (!_equal_expr (k1->up[i], k2->up[i], perm, 0)) return 0;
    }
    for (i=0; i < A_LEN (k1->dn); i++) {
      if (!_equal_expr (k1->dn[i], k2->dn[i], perm, 0)) return 0;
    }
  }
  return 1;
}


static int cell_matchfn (void *key1, void *key2)
{
  struct act_prsinfo *k1, *k2;
  
  k1 = (struct act_prsinfo *)key1;
  k2 = (struct act_prsinfo *)key2;

  return match_prsinfo (k1, k2, NULL);
}

static void *cell_dupfn (void *key)
{
  struct act_prsinfo *ch = (struct act_prsinfo *)key;
  struct act_prsinfo *d;

  NEW (d, struct act_prsinfo);
  *d = *ch;  // don't duplicate the arrays...
  return d;
}

static void cell_freefn (void *key)
{
  struct act_prsinfo *ch = (struct act_prsinfo *)key;
  // ... so don't free the arrays either
  FREE (ch);
}



static void flush_pending (void)
{
  /* convert pending gates into cells too */
  A_FREE (pending);
}


struct idmap {
  A_DECL (ActId *, ids);
  int nout;
  int nat;
};

static int _find_alloc_id (struct idmap *i, ActId *id, int islabel)
{
  int k;
  
  for (k=0; k < A_LEN (i->ids); k++) {
    if (i->ids[k] == id) return k;
    if (!islabel && (k < i->nout || k >= i->nout + i->nat)) {
      if (id->isEqual (i->ids[k])) return k;
    }
  }
  A_NEW (i->ids, ActId *);
  A_NEXT (i->ids) = id;
  A_INC (i->ids);
  return k;
}

static act_prs_expr_t *_copy_rule (act_prs_expr_t *e)
{
  act_prs_expr_t *ret;
  
  if (!e) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = e->type;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l = _copy_rule (e->u.e.l);
    ret->u.e.r = _copy_rule (e->u.e.r);
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = _copy_rule (e->u.e.l);
    ret->u.e.r = NULL;
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v = e->u.v;
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l = e->u.l;
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
  return ret;
}

static act_prs_expr_t *_complement_rule (act_prs_expr_t *e)
{
  act_prs_expr_t *r;
  NEW (r, act_prs_expr_t);
  r->type = ACT_PRS_EXPR_NOT;
  r->u.e.r = NULL;
  r->u.e.pchg = NULL;
  r->u.e.pchg_type = -1;
  r->u.e.l = _copy_rule (e);
  return r;
}

static void _twiddle_leaves (act_prs_expr_t *e)
{
  act_prs_expr_t *tmp;
  
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    _twiddle_leaves (e->u.e.l);
    _twiddle_leaves (e->u.e.r);
    break;

  case ACT_PRS_EXPR_NOT:
    _twiddle_leaves (e->u.e.l);
    break;

  case ACT_PRS_EXPR_VAR:
    tmp = _copy_rule (e);
    e->type =ACT_PRS_EXPR_NOT;
    e->u.e.l = tmp;
    e->u.e.r = NULL;
    e->u.e.pchg = NULL;
    e->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_LABEL:
    tmp = _copy_rule (e);
    e->type =ACT_PRS_EXPR_NOT;
    e->u.e.l = tmp;
    e->u.e.r = NULL;
    e->u.e.pchg = NULL;
    e->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_TRUE:
    e->type = ACT_PRS_EXPR_FALSE;
    break;
    
  case ACT_PRS_EXPR_FALSE:
    e->type = ACT_PRS_EXPR_TRUE;
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
}


static void _count_occurrences (struct act_prsinfo *info,
				act_prs_expr_t *e, int isup)
{
  int v;
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    _count_occurrences (info, e->u.e.l, isup);
    _count_occurrences (info, e->u.e.r, isup);
    //_count_occurrences (info, e->u.e.pchg, isup);
    break;

  case ACT_PRS_EXPR_NOT:
    _count_occurrences (info, e->u.e.l, isup);
    break;

  case ACT_PRS_EXPR_VAR:
    v = (unsigned long)e->u.v.id;
    if (isup) {
      info->attrib[v].nup++;
    }
    else {
      info->attrib[v].ndn++;
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    v = (unsigned long)e->u.l.label;
    if (isup) {
      info->attrib[v].nup++;
    }
    else {
      info->attrib[v].ndn++;
    }
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
}

static int _collect_depths (struct act_prsinfo *info,
			    act_prs_expr_t *e, int isup,
			    int flip = 0,
			    int depth = 0)
{
  int v, w;
  if (!e) return 0;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    if ((flip == 0 && e->type == ACT_PRS_EXPR_AND) ||
	(flip == 1 && e->type == ACT_PRS_EXPR_OR)) {
      v = _collect_depths (info, e->u.e.l, isup, flip, depth);
      v = _collect_depths (info, e->u.e.r, isup, flip, v);
    }
    else {
      v = _collect_depths (info, e->u.e.l, isup, flip, depth);
      w = _collect_depths (info, e->u.e.r, isup, flip, depth);
      v = (v > w) ? v : w;
    }
    break;

  case ACT_PRS_EXPR_NOT:
    v = _collect_depths (info, e->u.e.l, isup, 1-flip, depth);
    break;

  case ACT_PRS_EXPR_VAR:
    v = (unsigned long)e->u.v.id;
    if (isup) {
      info->attrib[v].depths[info->attrib[v].cup++] = depth;
    }
    else {
      info->attrib[v].depths[info->attrib[v].cdn++] = depth;
    }
    v = depth + 1;
    break;

  case ACT_PRS_EXPR_LABEL:
    v = (long)e->u.l.label;
    if (isup) {
      info->attrib[v].depths[info->attrib[v].cup++] = depth;
    }
    else {
      info->attrib[v].depths[info->attrib[v].cdn++] = depth;
    }
    v = depth;
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    v = depth;
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
  return v;
}

static void _dump_expr (act_prs_expr_t *e)
{
  int v;
  
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    printf ("(");
    _dump_expr (e->u.e.l);
    if (e->type == ACT_PRS_EXPR_OR) {
      printf (" | ");
    }
    else {
      printf (" & ");
    }
    _dump_expr (e->u.e.r);
    printf (")");
    break;

  case ACT_PRS_EXPR_NOT:
    printf ("~");
    _dump_expr (e->u.e.l);
    break;

  case ACT_PRS_EXPR_VAR:
    v = (unsigned long)e->u.v.id;
    printf ("%d", v);
    break;

  case ACT_PRS_EXPR_LABEL:
    v = (long)e->u.l.label;
    printf ("%d", v);
    break;

  case ACT_PRS_EXPR_TRUE:
    printf ("T");
    break;
    
  case ACT_PRS_EXPR_FALSE:
    printf ("F");
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
  return;
}


static act_prs_expr_t *_celement_rule (act_prs_expr_t *e)
{
  act_prs_expr_t *r = _copy_rule (e);
  _twiddle_leaves (r);
  return r;
}


static act_prs_expr_t *_scrub_rule (struct idmap *i, act_prs_expr_t *e)
{
  act_prs_expr_t *ret;
  
  if (!e) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = e->type;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l = _scrub_rule (i, e->u.e.l);
    ret->u.e.r = _scrub_rule (i, e->u.e.r);
    ret->u.e.pchg = _scrub_rule (i, e->u.e.pchg);
    ret->u.e.pchg_type = e->u.e.pchg_type;
    break;

  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = _scrub_rule (i, e->u.e.l);
    ret->u.e.r = NULL;
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v.sz = e->u.v.sz; // XXX: aliased!
    ret->u.v.id = (ActId *)(long)_find_alloc_id (i, e->u.v.id, 0);
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l.label = (char *)(long)_find_alloc_id (i, (ActId *)e->u.l.label, 1);
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
  return ret;
}

static void _add_rule (act_prs_expr_t **x, act_prs_expr_t *e)
{
  if (*x) {
    act_prs_expr_t *y;
    NEW (y, act_prs_expr_t);
    y->type = ACT_PRS_EXPR_OR;
    y->u.e.pchg = NULL;
    y->u.e.pchg_type = -1;
    y->u.e.l = *x;
    y->u.e.r = e;
    *x = y;
  }
  else {
    *x = e;
  }
}

static int intcmpfn (const void *a, const void *b)
{
  int x1, x2;
  x1 = (long)a;
  x2 = (long)b;
  return (x1 > x2) ? 1 : 0;
}

/*-- convert prs block into attriburtes used for isomorphism checking --*/
static struct act_prsinfo *_gen_prs_attributes (act_prs_lang_t *prs)
{
  struct act_prsinfo *ret;
  act_prs_lang_t *l, *lpush;
  NEW (ret, struct act_prsinfo);
  struct idmap imap;
  int i;
  int in_tree;

  ret->cell = NULL;
  ret->nvars = 0;
  ret->nout = 0;
  ret->nat = 0;
  ret->tval = -1;

  A_INIT (ret->attrib);
  A_INIT (ret->up);
  A_INIT (ret->dn);

  A_INIT (imap.ids);
  imap.nout = 0;
  imap.nat = 0;

  l = prs;
  lpush = NULL;
  in_tree = 0;
  while (l) {
    if (l->type == ACT_PRS_TREE) {
      if (ret->tval != -1) {
	fatal_error ("More than one tree!");
      }
      ret->tval = l->u.l.lo->u.v;
      lpush = l->next;
      l = l->u.l.p;
      in_tree = 1;
    }
    Assert (l->type == ACT_PRS_RULE, "gen_prs_attributes context error");

    for (i=0; i < A_LEN (imap.ids); i++) {
      if (imap.ids[i] == l->u.one.id)
	break;
      if (!l->u.one.label && (imap.ids[i]->isEqual (l->u.one.id)))
	break;
    }
    if (!l->u.one.label) {
      if (i == A_LEN (imap.ids)) {
	A_NEW (imap.ids, ActId *);
	A_NEXT (imap.ids) = l->u.one.id;
	A_INC (imap.ids);

	A_NEW (ret->up, act_prs_expr_t *);
	A_NEW (ret->dn, act_prs_expr_t *);
	A_NEXT (ret->up) = NULL;
	A_NEXT (ret->dn) = NULL;
	A_INC (ret->up);
	A_INC (ret->dn);

	A_NEW (ret->attrib, struct act_varinfo);
	A_NEXT (ret->attrib).nup = 0;
	A_NEXT (ret->attrib).ndn = 0;
	A_NEXT (ret->attrib).depths = NULL;
	A_NEXT (ret->attrib).cup = 0;
	A_NEXT (ret->attrib).cdn = 0;
	A_NEXT (ret->attrib).tree = 0;
	A_INC (ret->attrib);
      }
    }
    if (in_tree) {
      ret->attrib[i].tree = 1;
    }
    l = l->next;
    if (!l) {
      l = lpush;
      in_tree = 0;
    }
  }
  ret->nout = A_LEN (imap.ids);
  ret->nvars = ret->nout;
  imap.nout = ret->nout;

  /* collect all labels */
  l = prs;
  while (l) {
    if (l->u.one.label) {
      ret->nat++;
      imap.nat++;

      A_NEW (imap.ids, ActId *);
      A_NEXT (imap.ids) = l->u.one.id;
      A_INC (imap.ids);
      A_NEW (ret->up, act_prs_expr_t *);
      A_NEW (ret->dn, act_prs_expr_t *);
      A_NEXT (ret->up) = NULL;
      A_NEXT (ret->dn) = NULL;
      A_INC (ret->up);
      A_INC (ret->dn);

      A_NEW (ret->attrib, struct act_varinfo);
      A_NEXT (ret->attrib).nup = 0;
      A_NEXT (ret->attrib).ndn = 0;
      A_NEXT (ret->attrib).depths = NULL;
      A_NEXT (ret->attrib).cup = 0;
      A_NEXT (ret->attrib).cdn = 0;
      A_NEXT (ret->attrib).tree = 0;
      A_INC (ret->attrib);
      
    }
    l = l->next;
  }

  l = prs;
  while (l) {
    /* collect all the inputs, scrub rules */
    i = _find_alloc_id (&imap, l->u.one.id, l->u.one.label);
    
    /* now process the production rule, create a clean copy, replace
       all variables with ids! */
    act_prs_expr_t *x = _scrub_rule (&imap, l->u.one.e);
    act_prs_expr_t *xx = NULL;

    /* XXX: what about l->u.one.attr?! */
    if (l->u.one.arrow_type == 1) {
      xx = _complement_rule (x);
    }
    else if (l->u.one.arrow_type == 2) {
      xx = _celement_rule (x);
    }

    if (l->u.one.dir) {
      /* up */
      _add_rule (&ret->up[i], x);
    }
    else {
      _add_rule (&ret->dn[i], x);
    }
    if (l->u.one.arrow_type != 0) {
      if (!l->u.one.dir) {
	_add_rule (&ret->up[i], xx);
      }
      else {
	_add_rule (&ret->dn[i], xx);
      }	
    }
    l = l->next;
  }
  /* scrubbed rules are now ready: now we can create the varinfo space */
  ret->nvars = A_LEN (imap.ids);
  Assert (ret->nvars >= ret->nout, "Hmmmm.");
  Assert (ret->nout > 0, "What?");

  while (ret->nvars > A_LEN (ret->attrib)) {
    A_NEW (ret->attrib, struct act_varinfo);
    A_NEXT (ret->attrib).nup = 0;
    A_NEXT (ret->attrib).ndn = 0;
    A_NEXT (ret->attrib).depths = NULL;
    A_NEXT (ret->attrib).cup = 0;
    A_NEXT (ret->attrib).cdn = 0;
    A_NEXT (ret->attrib).tree = 0;
    A_INC (ret->attrib);
  }
  Assert (ret->nvars == A_LEN (ret->attrib), "HMm?!");
  Assert (A_LEN (ret->up) == A_LEN (ret->dn), "hmm");
  Assert (A_LEN (ret->up) >= ret->nout, "what happened?");

  /* 1. Gather counts */
  for (i=0; i < A_LEN (ret->up); i++) {
    _count_occurrences (ret, ret->up[i], 1);
    _count_occurrences (ret, ret->dn[i], 0);
  }
  
  /* 2. Gather depths */
  for (i=0; i < A_LEN (ret->attrib); i++) {
    if ((ret->attrib[i].nup + ret->attrib[i].ndn) > 0) {
      MALLOC (ret->attrib[i].depths, int,
	      ret->attrib[i].nup + ret->attrib[i].ndn);
      for (int j=0; j < (ret->attrib[i].nup + ret->attrib[i].ndn); j++) {
	ret->attrib[i].depths[j] = 0;
      }
      ret->attrib[i].cup = 0;
      ret->attrib[i].cdn = ret->attrib[i].nup;
    }
  }
  for (i=0; i < A_LEN (ret->up); i++) {
    _collect_depths (ret, ret->up[i], 1);
    _collect_depths (ret, ret->dn[i], 0);
  }

  /* Sort depths to canonicalize */
  for (i=0; i < A_LEN (ret->attrib); i++) {
    if (ret->attrib[i].depths) {
      mymergesort ((const void**) ret->attrib[i].depths, ret->attrib[i].nup, intcmpfn);
      mymergesort ((const void **) (ret->attrib[i].depths + ret->attrib[i].nup),
		   ret->attrib[i].ndn, intcmpfn);
    }
  }
  
  return ret;
}


static void _dump_prsinfo (struct act_prsinfo *p)
{
  printf ("-------\n");
  if (p->cell) {
    printf (" cell: %s\n", p->cell->getName());
  }
  else {
    printf (" cell: -no-name-\n");
  }
  printf ("  nvars=%d, nout=%d, tval=%d\n",
	  p->nvars, p->nout, p->tval);

  for (int i=0; i < A_LEN (p->up); i++) {
    printf (" %d: up = ", i);
    if (!p->up[i]) { printf ("n/a"); }
    else { _dump_expr (p->up[i]); }
    printf (" ; dn = ");
    if (!p->dn[i]) { printf ("n/a"); }
    else { _dump_expr (p->dn[i]); }
    printf ("\n");
  }
  for (int i=0; i < A_LEN (p->attrib); i++) {
    printf (" var %d: ", i);
    printf ("nup %d, ndn %d; ", p->attrib[i].nup, p->attrib[i].ndn);
    printf (" depths: ");
    for (int k=0; k < p->attrib[i].nup + p->attrib[i].ndn; k++) {
      printf ("%d ", p->attrib[i].depths[k]);
    }
    printf ("\n");
  }
  printf ("-------\n");
}


static void _collect_one_prs (Process *p, act_prs_lang_t *prs)
{

}


static void _collect_group_prs (Process *p, int tval, act_prs_lang_t *prs)
{
  Assert (tval > 0, "tree<> directive with <= 0 value?");
  
}
  


/*
  Helper function for walking through the production rule block
*/
static void collect_gates (Process *p, act_prs_lang_t **pprs)
{
  act_prs_lang_t *prs = *pprs;
  act_prs_lang_t *prev = NULL;
  while (prs) {
    switch (prs->type) {
    case ACT_PRS_RULE:
      /* snip rule */
      if (prev) {
	prev->next = prs->next;
      }
      _collect_one_prs (p, prs);
      break;
    case ACT_PRS_GATE:
      /* preserve pass transistors */ 
      if (!prev) {
	*pprs = prs;
      }
      prev = prs;
      break;
    case ACT_PRS_TREE:
      if (prev) {
	prev->next = prs->next;
      }
      /* collect all the gates as a single gate */
      if (prs->u.l.lo) {
	Assert (expr_is_a_const (prs->u.l.lo), "Hmm...");
	Assert (prs->u.l.lo->type == E_INT, "Hmmmm");
	_collect_group_prs (p, prs->u.l.lo->u.v, prs->u.l.p);
      }
      else {
	_collect_group_prs (p, 1, prs->u.l.p);
      }
      break;
    case ACT_PRS_SUBCKT:
      collect_gates (p, &prs->u.l.p);
      if (prs->u.l.p) {
	/* there's something left */
	if (!prev) {
	  *pprs = prs;
	}
	prev = prs;
      }
      break;
    case ACT_PRS_LOOP:
      fatal_error ("Should have been expanded");
      break;
    }
    prs = prs->next;
  }
  if (!prev) {
    *pprs = NULL;
  }
}



/*
  add_cells: -1 if not to be added, otherwise starting id of newcells 
*/
static void prs_to_cells (Act *a, Process *p, int add_cells)
{
  Assert (p->isExpanded(), "Only works for expanded processes!");

  if (visited_procs->find(p) != visited_procs->end()) {
    return;
  }
  visited_procs->insert (p);

  if (p->isCell()) {
    /* nothing to do here */
    return;
  }

  /* walk instances */
  ActInstiter i(p->CurScope());
  
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      Process *x = dynamic_cast<Process *>(vx->t->BaseType());
      if (x->isExpanded()) {
	prs_to_cells (a, x, add_cells);
      }
    }
  }

  A_INIT (pending);
  act_prs *prs = p->getprs();
  while (prs) {
    /*
      1. group all gates.
           - A gate is a pull-up + pull-down network
	   - If there is a tree, then all the variables on the RHS
	     there are considered part of a single multi-output gate

      2. for each gate, calculate support of pull-up and pull-down

      3. look through hash table to find a match
           -- store permutation of the match

      4. if you can't find a match, then add an entry to the hash
         table with this gate

      5. create an instance with this gate and replace the prs body
         completely

      6. CURRENTLY IGNORING POWER SUPPLY DOMAINS. In reality, cells
         will be parameterized by power supplies. Easy enough.

      7. For all production rules within the prs block, replace them
         with cell instances that use the permutation map to pass in
         act identifers.
    */
    collect_gates (p, &prs->p);
    prs = prs->next;
  }
  flush_pending ();
}


/*
 * collect all cells into the cell table
 */
static int _collect_cells (ActNamespace *cells)
{
  ActTypeiter it(cell_ns);
  chash_bucket_t *b;
  struct act_prsinfo *pi;
  int cellmax = -1;

  for (it = it.begin(); it != it.end(); it++) {
    Type *u = (*it);
    Process *p = dynamic_cast<Process *>(u);
    if (!p) continue;

    if (!p->isCell()) {
      continue;
    }
    if ((!p->isExpanded()) && (p->getNumParams() == 0)) {
      char buf[10240];
      sprintf (buf, "%s<>", p->getName());
      if (!cells->findType (buf)) {
	/* create expanded version */
	p->Expand (cells, cells->CurScope(), 0, NULL);
      }
    }
  }

  for (it = it.begin(); it != it.end(); it++) {
    Type *u = *it;
    Process *p = dynamic_cast <Process *>(u);
    if (!p) {
      continue;
    }
    if (!p->isCell()) {
      continue;
    }
    if (!p->isExpanded()) {
      continue;
    }
    /* cells in the "cells" namespace are very special.

       1. no aliases internally
       2. defcell has two ports: in[n], out[m]
       3. only prs for out[m]'s are present

       4. cell names are g<#> (for "gate")
       5. pass gate cells are
             p0_<flavor> <w,l> pass gate n
	     p1_<flavor> <w,l> pass gate p
	     p2_<flavor> <w,l> transgate
    */
    if (p->getName()[0] == 'g') {
      /* ok, gate! */
      if (p->getNumParams() != 0) {
	fatal_error ("Unexpected cell `%s' in cell namespace (has parameters?)",
		     p->getName());
      }
      if (strcmp (p->getName() + strlen (p->getName())-2, "<>") != 0) {
	fatal_error ("Unexpected cell `%s' in cell namespace (has parameters?)",
		     p->getName());
      }
      if (p->getNumPorts() != 2) {
	fatal_error ("Cell `cell::%s': More than two ports");
      }
      if ((strcmp (p->getPortName (0), "in") != 0) ||
	  (strcmp (p->getPortName (1), "out") != 0)) {
	fatal_error ("Cell `cell::%s': Ports should be in/out",
		     p->getName());
      }
      InstType *in_t, *out_t;
      in_t = p->getPortType (0);
      out_t = p->getPortType (1);

      int id, version;
      sscanf (p->getName()+1, "%dx%d", &id, &version);
      cellmax = (cellmax > id) ? cellmax : id;
      
      /* in_t must be a bool array or bool
	 out_t must be a bool array or bool
      */
      if (!TypeFactory::isBoolType (in_t) || !TypeFactory::isBoolType (out_t)) {
	fatal_error ("Cell `cell::%s': Port base types must `bool'",
		     p->getName());
      }

      /* sanity check prs */
      act_prs *prs = p->getprs();
      if (prs && prs->next) {
	fatal_error ("Cell `cell::%s': More than one prs body",
		     p->getName());
      }
      act_prs_lang_t *l;
      Expr *treeval;
      int mgn;
      l = NULL;
      treeval = NULL;
      mgn = 0;
      if (prs) {
	l = prs->p;
	if (l && l->type == ACT_PRS_TREE) {
	  if (l->next) {
	    fatal_error ("Cell `cell::%s': only one tree permitted",
			 p->getName());
	  }
	  l = l->u.l.p;
	}
	while (l) {
	  Assert (l->type != ACT_PRS_LOOP, "Expanded?!");
	  if (l->type == ACT_PRS_GATE || l->type == ACT_PRS_SUBCKT ||
	      l->type == ACT_PRS_TREE) {
	    fatal_error ("Cell `cell::%s': no fets/subckts/dup trees allowed",
			 p->getName());
	  }
	  else if ((l->type == ACT_PRS_RULE) && l->u.one.label) {
	    mgn = 1;
	  }
	  l = l->next;
	}
	l = prs->p;
	if (l && l->type == ACT_PRS_TREE) {
	  treeval = l->u.l.lo;
	  l = l->u.l.p;
	}
      }
      
      /* fine. now dump into celldb */
      pi = _gen_prs_attributes (l);

      pi->cell = p;
      if (out_t->arrayInfo()) {
         if (out_t->arrayInfo()->size() != pi->nout) {
            fatal_error ("Cell `cell::%s': inconsistent outputs",
                      p->getName());
         }
      }
      else {
         if (pi->nout != 1) {
            fatal_error ("Cell `cell::%s': inconsistent outputs",
                      p->getName());
         }
      }

      if (treeval) {
	Assert (treeval->type == E_INT, "Hmm");
	pi->tval = treeval->u.v;
	Assert (pi->tval > 0, "tree<> parameter has to be > 0!");
      }
      else if (mgn) {
	/* shared gate network */
	pi->tval = 0;
      }

      if (A_LEN (pi->up) == 0 && A_LEN (pi->dn) == 0) {
	warning("Cell `%s' has no production rules?", p->getName());
      }

      // XXX: HERE!!!
      b = chash_lookup (cell_table, pi);
      if (b) {
	warning("Cell `%s' is a duplicate?", p->getName());
	//printf ("-- cur cell --\n");
	//_dump_prsinfo (pi);
	
	pi = (struct act_prsinfo *)b->v;
	warning ("Previous cell: `%s'", pi->cell->getName());
	//printf ("-- old cell --\n");
	//_dump_prsinfo (pi);
      }
      else {
	b = chash_add (cell_table, pi);
	b->v = pi;
      }
    }
  }
  return cellmax;
}


void act_prs_to_cells (Act *a, Process *p, int add_cells)
{
  cell_table = (struct cHashtable *) a->aux_find ("prs2cells");
  if (!cell_table) {
    /* initialize all the fields of cell_table */
    cell_table = chash_new (32);
    cell_table->hash = cell_hashfn;
    cell_table->match = cell_matchfn;
    cell_table->dup = cell_dupfn;
    cell_table->free = cell_freefn;
    cell_table->print = NULL;

    a->aux_add ("prs2cells", cell_table);
  }
    
  visited_procs = new std::set<Process *> ();
  
  cell_ns = a->findNamespace ("cell");


  if (!cell_ns) {
    cell_ns = new ActNamespace (ActNamespace::Global(), "cell");
  }
  else {
    int res =  _collect_cells (cell_ns) + 1;
    if (add_cells != -1) {
      add_cells = add_cells > res ? add_cells : res;
    }
  }
  
  if (!p) {
    ActNamespace *g = ActNamespace::Global();
    ActInstiter i(g->CurScope());

    for (i = i.begin(); i != i.end(); i++) {
      ValueIdx *vx = *i;
      if (TypeFactory::isProcessType (vx->t)) {
	Process *x = dynamic_cast<Process *>(vx->t->BaseType());
	if (x->isExpanded()) {
	  prs_to_cells (a, x, add_cells);
	}
      }
    }
  }
  else {
    prs_to_cells (a, p, add_cells);
  }
  delete visited_procs;
  cell_table = NULL;
  cell_ns = NULL;
}
