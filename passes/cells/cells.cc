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
#include <act/passes/sizing.h>
#include <act/passes/booleanize.h>
#include <act/passes/netlist.h>
#include <common/config.h>
#include <math.h>

struct act_varinfo {
  int nup, ndn;			// # of times in up and down guards
  int *depths;			// depth of each instance (nup + ndn)
  int cup, cdn;
				// size), 0..nup-1 followed by ndn
  
  unsigned char tree;		// 0 = not in tree, 1 dn in tree, 2 up
				// in tree, 3 both in tree
};


class act_prsinfo {
 private:
  int leak_adjust;
  int tval;			 /* for tree<>; -1 = none, otherwise tree  */

 public:
  Process *cell;		/* the cell; NULL until it is
				   created. */

  int nvars;			/* # of variables. includes @-labels */

  int nout;			/* # of outputs 
				   Variable convention:
				   0, 1, ..., nout-1 are outputs.
				   nout, ... nouts + nat - 1 are @ labels.
				   nout + nat, ... nvars-1 are inputs
				*/

  act_attr_t **nattr;		/* 2 per output: pull-up and pull-down */
  
  int nat;                     /* # of labels */

  int *at_perm; 		/* permutation of ATs */

  /* variable attributes */
  int *attr_map;

  
  struct act_varinfo *attrib;

  A_DECL (act_prs_expr_t *, up); /* pull-up */
  A_DECL (act_prs_expr_t *, dn); /* pull-down */
      /* NOTE: all actid pointers are actually just simple integers */
      /* The # of these will be nout + any internal labels */

  int *match_perm;		// used to report match!


  act_prsinfo(int _leak_flag) {
    cell = NULL;
    nvars = 0;
    nout = 0;
    nat = 0;
    tval = -1;
    match_perm = NULL;
    nattr = NULL;
    at_perm = NULL;
    leak_adjust = _leak_flag;

    attrib = NULL;
    A_INIT (up);
    A_INIT (dn);
  }

  /* leakage adjust flag for cell */
  void set_leak_flag (int flag) { leak_adjust = flag; }
  int get_leak_flag () { return leak_adjust; }

  /* tree depth if set, 0 if not set */
  void set_tree_info (int flag) { tval = flag; }
  int get_tree_info () { return tval; }
  
  /*
    Each unique variable right hand side of a production rule in a
    prs block is assigned a slot. Slots are needed for both output
    variables as well as labels (@-variables).
  */
  void add_new_slot () {
    A_NEW (up, act_prs_expr_t *);
    A_NEW (dn, act_prs_expr_t *);
    A_NEXT (up) = NULL;
    A_NEXT (dn) = NULL;
    A_INC (up);
    A_INC (dn);
  }

  /*
    Set the number of outputs, and initialize the attribute lists for
    the output variables. Note that @-variables do not have
    attributes, so this is only needed for each output variable.
  */
  void set_num_outputs(int n) {
    nout = n;
    nvars = n;
    MALLOC (nattr, act_attr_t *, n*2);
    for (int i=0; i < n*2; i++) {
      nattr[i] = NULL;
    }
  }

  /*
    Set total variables, and allocate attributes etc.
  */
  void set_tot_vars (int n) {
    nvars = n;
    Assert (nvars >= nout, "What?");
    Assert (nout > 0, "No outputs?");
    
    MALLOC (attrib, struct act_varinfo, nvars);
    for (int i=0; i < nvars; i++) {
      attrib[i].nup = 0;
      attrib[i].ndn = 0;
      attrib[i].depths = NULL;
      attrib[i].cup = 0;
      attrib[i].cdn = 0;
      attrib[i].tree = 0;
    }
  }

  int numvars() { return nvars; }


  /*-- compute hash of this prsinfo block --*/
  unsigned int hashfn (int sz) {
    unsigned int h;
    int i;

    h = hash_function_continue (sz, (unsigned char *)&nvars,
				sizeof (int), 0, 0);
    h = hash_function_continue (sz, (unsigned char *)&nout,
				sizeof (int), h, 1);
    h = hash_function_continue (sz, (unsigned char *)&nat,
				sizeof (int), h, 1);
    h = hash_function_continue (sz, (unsigned char *)&tval,
				sizeof (int), h, 1);

    for (i=0; i < numvars(); i++) {
      h = hash_function_continue (sz, (unsigned char *)&attrib[i].nup,
				  sizeof (int), h, 1);
      h = hash_function_continue (sz, (unsigned char *)&attrib[i].ndn,
				  sizeof (int), h, 1);
      h = hash_function_continue (sz, (unsigned char *)&attrib[i].tree,
				  sizeof (char), h, 1);
    }
  
    return h;
  }
  
};


static char *_remap_cell_names (char *s)
{
  static char **user_name_map;
  static int name_count = -1;

  if (name_count == -1) {
    if (config_exists ("net.cell_namemap")) {
      name_count = config_get_table_size ("net.cell_namemap");
      if (name_count % 2) {
	warning ("net.cell_namemap should be even!");
	name_count = 0;
      }
      else {
	user_name_map = config_get_table_string ("net.cell_namemap");
      }
    }
    else {
      name_count = 0;
    }
  }

  for (int i=0; i < name_count/2; i++) {
    if (strcmp (user_name_map[2*i], s) == 0) {
      FREE (s);
      return Strdup (user_name_map[2*i+1]);
    }
  }
  return s;
}

static char *_get_basename (struct act_prsinfo *pi)
{
  if (pi->nout != 1 && pi->nat != 0) {
    return Strdup ("");
  }
  list_t *l;
  l = list_new ();
  for (int i=0; i < pi->nvars - 1; i++) {
    list_append (l, (void *)(unsigned long)(i+1));
  }
  char *up, *dn;
  
  if (pi->up[0]) {
    up = act_prs_expr_to_string (l, pi->up[0]);
  }
  else {
    up = NULL;
  }
  if (pi->dn[0]) {
    dn = act_prs_expr_to_string (l, pi->dn[0]);
  }
  else {
    dn = NULL;
  }

  char *ret;
  MALLOC (ret, char, (up ? strlen (up) : 0) +
	  (dn ? strlen (dn) : 0) + 2);

  strcpy (ret, up ? up : "");
  strcat (ret, "_");
  if (dn) {
    strcat (ret, dn);
  }
  list_free (l);
  if (up) {
    FREE (up);
  }
  if (dn) {
    FREE (dn);
  }
  return _remap_cell_names (ret);
}


static void _add_used_cell (list_t *l, Process *p)
{
  for (listitem_t *li = list_first (l); li; li = list_next (li)) {
    if (p == (Process *) list_value (li)) {
      return;
    }
  }
  list_append (l, p);
}


static void _dump_prsinfo (struct act_prsinfo *p, const char *_in,
			   const char *_out);
static act_prs_expr_t *_convert_prsexpr_to_act (act_prs_expr_t *e,
						const char *_inport_name,
						const char *_outport_name,
						struct act_prsinfo *pi);


static int cell_hashfn (int sz, void *key)
{
  act_prsinfo *ckey = (act_prsinfo *)key;
  return ckey->hashfn (sz);
}

static Expr *_id_to_expr (ActId *id)
{
  Expr *ret;
  
  NEW (ret, Expr);
  ret->type = E_VAR;
  ret->u.e.l = (Expr *)id;
  return ret;
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

static int _equal_expr_helper (act_prs_expr_t *a, act_prs_expr_t *b,
			       int *perm, int paritya, int parityb,
			       int chk_width)
{
  int bid;
  
  if (!a && !b) {
    return 1;
  }
  if (!a || !b) {
    return 0;
  }

  if (a->type == ACT_PRS_EXPR_NOT && b->type == ACT_PRS_EXPR_NOT) {
    return _equal_expr_helper (a->u.e.l, b->u.e.l, perm, 1-paritya, 1-parityb,
			       chk_width);
  }
  while (a && (a->type == ACT_PRS_EXPR_NOT)) {
    paritya = 1 - paritya;
    a = a->u.e.l;
  }
  while (b && (b->type == ACT_PRS_EXPR_NOT)) {
    parityb = 1 - parityb;
    b = b->u.e.l;
  }
  Assert (a && b, "Hmm");

  switch (a->type) {
  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    if (b->type == ACT_PRS_EXPR_TRUE || b->type == ACT_PRS_EXPR_FALSE) {
      int bval, aval;
      bval = (b->type == ACT_PRS_EXPR_TRUE ? 1 : 0);
      aval = (a->type == ACT_PRS_EXPR_TRUE ? 1 : 0);
      bval = bval^parityb;
      aval = aval^paritya;
      return (aval == bval);
    }
    else {
      return 0;
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    if (b->type != ACT_PRS_EXPR_LABEL) {
      return 0;
    }
    if (paritya != parityb) {
      return 0;
    }
    bid = (unsigned long)b->u.l.label;
    if (perm) {
      bid = perm[bid];
    }
    if ((unsigned long)a->u.l.label != bid) {
      return 0;
    }
    else {
      return 1;
    }
    break;

  case ACT_PRS_EXPR_VAR:
    if (b->type != ACT_PRS_EXPR_VAR) {
      return 0;
    }
    if (paritya != parityb) {
      return 0;
    }
    bid = (unsigned long long)b->u.v.id;
    if (perm) {
      bid = perm[bid];
    }
    if ((unsigned long long)a->u.v.id != bid) {
      return 0;
    }
    if (chk_width) {
      if (a->u.v.sz && !b->u.v.sz) return 0;
      if (!a->u.v.sz && b->u.v.sz) return 0;
      if (a->u.v.sz && b->u.v.sz) {
	if (a->u.v.sz->flavor != b->u.v.sz->flavor) return 0;
	if (!expr_equal (a->u.v.sz->w, b->u.v.sz->w)) {
	  double d1, d2;
	  if (act_expr_getconst_real (a->u.v.sz->w, &d1) &&
	      act_expr_getconst_real (b->u.v.sz->w, &d2)) {
	    if (fabs (d1-d2) < 1e-6) {
	      // match!
	    }
	    else {
	      return 0;
	    }
	  }
	  else {
	    return 0;
	  }
	}
	if (!expr_equal (a->u.v.sz->l, b->u.v.sz->l)) {
	  double d1, d2;
	  if (act_expr_getconst_real (a->u.v.sz->w, &d1) &&
	      act_expr_getconst_real (b->u.v.sz->w, &d2)) {
	    if (fabs (d1-d2) < 1e-6) {
	      // match!
	    }
	    else {
	      return 0;
	    }
	  }
	  else {
	    return 0;
	  }
	}
	if (!expr_equal (a->u.v.sz->folds, b->u.v.sz->folds)) return 0;
      }
    }
    return 1;
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("loops in expanded prs?");
    return 0;
    break;

  case ACT_PRS_EXPR_NOT:
    fatal_error ("What happened to the while loop earlier?!");
    return 0;
    break;

#define IS_AND(x,y) (((x)->type == ACT_PRS_EXPR_AND && ((y) == 0)) || \
		     ((x)->type == ACT_PRS_EXPR_OR && ((y) == 1)))

#define IS_OR(x,y) (((x)->type == ACT_PRS_EXPR_OR && ((y) == 0)) || \
		     ((x)->type == ACT_PRS_EXPR_AND && ((y) == 1)))

  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    if (IS_AND (a, paritya)) {
      if (IS_AND (b, parityb)) {
	return _equal_expr_helper (a->u.e.l, b->u.e.l, perm, paritya, parityb, chk_width) &&
	  _equal_expr_helper (a->u.e.r, b->u.e.r, perm, paritya, parityb, chk_width) &&
	  _equal_expr_helper (a->u.e.pchg, b->u.e.pchg, perm, 0, 0, chk_width) &&
	  (a->u.e.pchg ? (a->u.e.pchg_type == b->u.e.pchg_type) : 1);
      }
      else {
	return 0;
      }
    }
    else {
      if (IS_OR (a, paritya)) {
	if (IS_OR (b, parityb)) {
	  return ((_equal_expr_helper (a->u.e.l, b->u.e.l, perm, paritya, parityb, chk_width) &&
		   _equal_expr_helper (a->u.e.r, b->u.e.r, perm, paritya, parityb, chk_width)) ||
		  (_equal_expr_helper (a->u.e.l, b->u.e.r, perm, paritya, parityb, chk_width) &&
		   _equal_expr_helper (a->u.e.r, b->u.e.l, perm, paritya, parityb, chk_width)));
	}
	else {
	  return 0;
	}
      }
      else {
	return 0;
      }
    }    
    break;
#undef IS_AND
#undef IS_OR

  default:
    fatal_error ("What?");
    return 0;
  }
}

/*
  non-zero if equal, 0 otherwise

  flip = 0, compare a to b
  flip = 1, compare a to NOT b

  chk_width = 1 : check widths of gates
*/
static int _equal_expr (act_prs_expr_t *a, act_prs_expr_t *b,
			int *perm, int chk_width)
{
  return _equal_expr_helper (a, b, perm, 0, 0, chk_width);
}

static int basic_match (struct act_prsinfo *k1, struct act_prsinfo *k2)
{
  if (k1->nvars != k2->nvars) return 0;
  if (k1->nout != k2->nout) return 0;
  if (k1->get_tree_info() != k2->get_tree_info()) return 0;
  if (k1->nat != k2->nat) return 0;

  if (A_LEN (k1->up) != A_LEN (k2->up)) return 0;
  if (A_LEN (k1->dn) != A_LEN (k2->dn)) return 0;

  for (int i=0; i < k1->nout*2; i++) {
    act_attr_t *a1, *a2;
    a1 = k1->nattr[i];
    a2 = k2->nattr[i];
    
    if (a1) {
      if (a2) {
	while (a1 && a2) {
	  if (strcmp (a1->attr, a2->attr) != 0) return 0;
	  if (!expr_equal (a1->e, a2->e)) return 0;
	  a1 = a1->next;
	  a2 = a2->next;
	}
	if (a1 || a2) return 0;
      }
      else {
	return 0;
      }
    }
    else if (a2) {
      return 0;
    }
  }
  return 1;
}

/*
  ASSUMES: basic_match (k1,k2) is true.
*/
static int match_prsinfo (struct act_prsinfo *k1,
			  struct act_prsinfo *k2,
			  int chk_width)
{
  int i;
  static int *equal_choices = NULL;
  static int sz = 0;

  if (k1->get_leak_flag() != k2->get_leak_flag()) {
    return 0;
  }

  if (k1->numvars() > sz) {
    sz = k1->numvars();
    if (equal_choices) {
      REALLOC (equal_choices, int, sz);
    }
    else {
      MALLOC (equal_choices, int, sz);
    }
  }

  equal_choices[0] = 0;
  for (i=0; i < k1->numvars(); i++) {
    if (k1->attrib[i].tree != k2->attrib[i].tree) return 0;
    if (k1->attrib[i].nup != k2->attrib[i].nup) return 0;
    if (k1->attrib[i].ndn != k2->attrib[i].ndn) return 0;
    for (int j=0; j < k1->attrib[i].nup + k1->attrib[i].ndn; j++) {
      if (k1->attrib[i].depths[j] != k2->attrib[i].depths[j]) return 0;
    }
    if (i > 0) {
      equal_choices[i] = 0;
      if ((k1->attrib[i].tree == k1->attrib[i-1].tree) &&
	  (k1->attrib[i].nup == k1->attrib[i-1].nup) &&
	  (k1->attrib[i].ndn == k1->attrib[i-1].ndn)) {
	equal_choices[i] = 1;
	for (int j=0; j < k1->attrib[i].nup + k1->attrib[i].ndn; j++) {
	  if (k1->attrib[i].depths[j] != k1->attrib[i-1].depths[j]) {
	    equal_choices[i] = 0;
	    break;
	  }
	}
      }
    }
  }

#if 0
  printf ("comparing:\n");
  _dump_prsinfo (k1, "in", "out");
  _dump_prsinfo (k2, "in", "out");
#endif  

  int *perm;
  MALLOC (perm, int, k1->nvars);

  for (i=0; i < k1->nvars; i++) {
    perm[k2->attr_map[i]] = k1->attr_map[i];
  }

#if 0
  printf ("perm is: ");
  for (i=0; i < k1->nvars; i++) {
    printf (" %d", perm[i]);
  }
  printf ("  ===\n");
#endif  
  
  /* 
     perm is now the default map from k1 to k2 

     We also need to sub-divide the range to contiguous sections where
     the attribute values match; we need to try all permutations of
     let's say i.
  */
  for (i=0; i < A_LEN (k1->up); i++) {
    if (!_equal_expr (k1->up[i], k2->up[perm[i]], perm, chk_width)) {
#if 0
      printf ("up[%d] doesn't match\n", i);
#endif      
      FREE (perm);
      return 0;
    }
  }
  for (i=0; i < A_LEN (k1->dn); i++) {
    if (!_equal_expr (k1->dn[i], k2->dn[perm[i]], perm, chk_width)) {
#if 0
      printf ("dn[%d] doesn't match\n", i);
#endif      
      FREE (perm);
      return 0;
    }
  }

#if 0
  printf ("match!\n");
#endif  
  k1->match_perm = perm;
  return 1;
}



static int cmp_fn_varinfo (const  void *a, const void *b)
{
  struct act_varinfo *v1, *v2;
  v1 = (struct act_varinfo *)a;
  v2 = (struct act_varinfo *)b;
  
  if (v1->nup < v2->nup) return -1;
  if (v1->nup > v2->nup) return 1;
  if (v1->ndn < v2->ndn) return -1;
  if (v1->ndn > v2->ndn) return 1;
  if (v1->tree < v2->tree) return -1;
  if (v1->tree > v2->tree) return 1;
  int k;
  for (k=0; k < v1->nup + v1->ndn; k++) {
    if (v1->depths[k] < v2->depths[k]) return -1;
    if (v1->depths[k] > v2->depths[k]) return 1;
  }
  return 0;
}

			       

static int cell_matchfn (void *key1, void *key2)
{
  struct act_prsinfo *k1, *k2;
  
  k1 = (struct act_prsinfo *)key1;
  k2 = (struct act_prsinfo *)key2;

#if 0
  printf ("level0... comparing:\n");
  _dump_prsinfo (k1, "in", "out");
  _dump_prsinfo (k2, "in", "out");
  printf ("end level0\n");
#endif
  if (!basic_match (k1, k2)) return 0;

#if 0
  printf (" --> made it past basic match!\n");
#endif
  return match_prsinfo (k1, k2, 1);
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


static int _uses_a_label (act_prs_expr_t *e)
{
  if (!e) return 0;

  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    return _uses_a_label (e->u.e.l) || _uses_a_label (e->u.e.r);
   break;

  case ACT_PRS_EXPR_NOT:
    return _uses_a_label (e->u.e.l);
    break;

  case ACT_PRS_EXPR_VAR:
    return 0;
    break;

  case ACT_PRS_EXPR_LABEL:
    return 1;
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    return 0;
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    return 0;
    break;

  default:
    fatal_error ("What?");
    return 0;
    break;
  }
}

void ActCellPass::_mark_at_used (act_prs_expr_t *e, bitset_t *b,
				 int *idx_array, int idx_len)
{
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    _mark_at_used (e->u.e.l, b, idx_array, idx_len);
    _mark_at_used (e->u.e.r, b, idx_array, idx_len);
    break;

  case ACT_PRS_EXPR_NOT:
    _mark_at_used (e->u.e.l, b, idx_array, idx_len);
    break;

  case ACT_PRS_EXPR_VAR:
    return;
    break;

  case ACT_PRS_EXPR_LABEL:
    for (int i=0; i < idx_len; i++) {
      if (strcmp (e->u.l.label,
		  (char *)_pending_prs[idx_array[i]]->u.one.id) == 0) {
	bitset_set (b, i);
	return;
      }
    }
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    return;
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

void ActCellPass::_mark_at_used2 (act_prs_expr_t *e, bitset_t *b,
				  act_prs_lang_t **rules, int len)
{
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    _mark_at_used2 (e->u.e.l, b, rules, len);
    _mark_at_used2 (e->u.e.r, b, rules, len);
    break;

  case ACT_PRS_EXPR_NOT:
    _mark_at_used2 (e->u.e.l, b, rules, len);
    break;

  case ACT_PRS_EXPR_VAR:
    return;
    break;

  case ACT_PRS_EXPR_LABEL:
    for (int i=0; i < len; i++) {
      if (strcmp (e->u.l.label,(char *)rules[i]->u.one.id) == 0) {
	bitset_set (b, i);
	return;
      }
    }
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    return;
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

static int _at_inv_lookup (int idx, int *at_idx, int len)
{
  for (int i=0; i < len; i++) {
    if (at_idx[i] == idx) {
      return i;
    }
  }
  return -1;
}

void ActCellPass::flush_group (Scope *sc, pending_group *g)
{
  act_prs_lang_t *group;
  int sz = 1 + g->_pending.size();
  MALLOC (group, act_prs_lang_t, sz);
  group[0] = *(g->tree);
  for (int i=1; i < sz; i++) {
    group[i-1].next = &group[i];
    group[i] = *(g->_pending[i-1]);
  }
  group[sz-1].next = NULL;
  _create_new_cell (sc, &group[0]);
  FREE (group);
  g->mark_processed ();
}

void ActCellPass::flush_pending (Scope *sc)
{
  int pending_count = 0;
  int pending_sz;

  if (_pending_prs.size() == 0) {
    return;
  }

  pending_sz = _pending_prs.size();

  /* -- handle shared gates -- */
  int *at_idx;
  MALLOC (at_idx, int, pending_sz);
  int at_len = 0;

  for (int i=0; i < pending_sz; i++) {
    if (_pending_prs[i]->u.one.label) {
      at_idx[at_len] = i;
      at_len++;
    }
  }

  /*
    at_idx[.] maps the pendingprs index to a number from 0 to at_len-1
     (at_len = # of at-variables on the RHS of all pending rules).
  */

  bitset_t **at_use = NULL;
  int *grouped = NULL;

  if (at_len > 0) {
    /* there are some labels */

    /* figure out which pending rules use them */
    MALLOC (at_use, bitset_t *, pending_sz);
    MALLOC (grouped, int, pending_sz);

#if 0
    printf ("--\n");
#endif    
    for (int i=0; i < pending_sz; i++) {
      grouped[i] = 0;
      at_use[i] = bitset_new (at_len);
      _mark_at_used (_pending_prs[i]->u.one.e, at_use[i], at_idx, at_len);
#if 0
      printf ("%d : ", i);
      bitset_print (at_use[i]);
      printf ("\n");
#endif
    }
#if 0
    printf ("--\n");
#endif    
  }
  else {
    FREE (at_idx);
    at_len = 0;
    at_idx = NULL;
  }

  /*
    at_use[i] is a bit-vector that indicates which at rules are used
    by pendingprs[i]
  */

  A_DECL (act_prs_lang_t, groupprs);
  A_INIT (groupprs);

  bitset_t *at_group = NULL;
  bitset_t *at_tmp = NULL;
  if (at_len > 0) {
    at_group = bitset_new (at_len);
    at_tmp = bitset_new (at_len);
  }

  /*
    grouped[i] : 0 if it has not been used, 1 if it is being
    considered for the current group, 2 if it has been processed.
  */
  
  for (int i=0; i < pending_sz; i++) {
    struct act_prsinfo *pi;
    chash_bucket_t *b;

    if (at_len > 0 && (grouped[i] == 2)) {
      /* skip, since it has been grouped already */
      continue;
    }

    A_NEWM (groupprs, act_prs_lang_t);
    A_NEXT (groupprs) = *(_pending_prs[i]);
    A_NEXT (groupprs).next = NULL;
    A_INC (groupprs);

    if (at_len == 0 ||
	(bitset_isclear (at_use[i]) && !_pending_prs[i]->u.one.label)) {
      /* if there are no at-rules
	 OR
	   this rule is not an @-rule and
	   this rule doesn't use any @ labels,
	 then
	   nothing to do!
      */
    }
    else {
      /* Some @ rule is used, or we are at an @ rule */
      grouped[i] = 1;

      // the at-group for this rule includes all the labels it uses
      bitset_or (at_group, at_use[i]);

      // if this is a label, the at-group includes this label
      if (_pending_prs[i]->u.one.label) {
	/* find the index of the at-variable for this label */
	int k = _at_inv_lookup (i, at_idx, at_len);
	Assert (k != -1, "What?");
	bitset_set (at_group, k);
      }

      do {
	bitset_clear (at_tmp);
	bitset_or (at_tmp, at_group);

	for (int j=i+1; j < pending_sz; j++) {
	  if (!bitset_andclear (at_group, at_use[j])) {
	    /* there is an AT in common! */
	    bitset_or (at_group, at_use[j]);
	    grouped[j] = 1;
	    if (_pending_prs[j]->u.one.label) {
	      int k = _at_inv_lookup (j, at_idx, at_len);
	      bitset_set (at_group, k);
	    }
	  }
	  else if (_pending_prs[j]->u.one.label) {
	    /* check if this label is already in the at group */
	    int k = _at_inv_lookup (j, at_idx, at_len);
	    Assert (k != -1, "What?");
	    if (bitset_tst (at_group, k)) {
	      grouped[j] = 1;
	      bitset_or (at_group, at_use[j]);
	    }
	  }
	}
	/* repeat while the at group changed */
      } while (!bitset_equal (at_tmp, at_group));

      // now we have all the @s we need, and they are flagged with the
      // grouped bit

#if 0
      printf ("Grouped: ");
#endif
      
      for (int j=i; j < pending_sz; j++) {
#if 0
	if (grouped[j] == 1) {
	  printf (" %d", j);
	}
#endif	
	if (grouped[j] == 1 && !_pending_prs[j]->u.one.label) {
	  for (int k=j+1; k < pending_sz; k++) {
	    if ((_pending_prs[j]->u.one.id == _pending_prs[k]->u.one.id) ||
		_pending_prs[j]->u.one.id->isEqual (_pending_prs[k]->u.one.id)) {
	      grouped[k] = 1;
	      break;
	    }
	  }
	}
      }
#if 0
      printf ("\n");
#endif      

      for (int j=i+1; j < pending_sz; j++) {
	if (grouped[j] == 1) {
	  A_NEWM (groupprs, act_prs_lang_t);
	  A_NEXT (groupprs) = *(_pending_prs[j]);
	  A_NEXT (groupprs).next = NULL;
	  A_INC (groupprs);
	  grouped[j] = 2;
	}
      }
      for (int j=0; j < A_LEN (groupprs)-1; j++) {
	groupprs[j].next = &groupprs[j+1];
      }
      bitset_clear (at_group);
    }

    pi = _gen_prs_attributes (&groupprs[0]);
#if 0
    _dump_prsinfo (pi);
#endif

    if (cell_table) {
      b = chash_lookup (cell_table, pi);
      if (b) {
	pi = (struct act_prsinfo *)b->v;
      }
      else {
#if 0
	_dump_prsinfo (pi);
#endif
	add_new_cell (pi);
	b = chash_add (cell_table, pi);
	b->v = pi;
      }

      char buf[100];
      do {
	snprintf (buf, 100, "cpx%d", pending_count++);
      } while (sc->Lookup (buf));

      Assert (sc->isExpanded(), "Hmm");

      _add_used_cell (_used_cells, pi->cell);

      InstType *it = new InstType (sc, pi->cell, 0);
      it = it->Expand (NULL, sc);

      Assert (it->isExpanded(), "Hmm");
    
      Assert (sc->Add (buf, it), "What?");
      /*--- now make connections --*/
    
      ActBody_Conn *ac;

      //printf (" --- [%s] \n", p->getName());
      ac = _build_connections (buf, pi);
      //ac->Print (stdout);
      //ac->Next()->Print (stdout);
      //printf (" --- \n");

      if (ac) {
	int oval = Act::double_expand;
	Act::double_expand = 0;
	ac->Expandlist (NULL, sc);
	Act::double_expand = oval;
      }
      //printf ("---\n");
    }
    A_FREE (groupprs);
    A_INIT (groupprs);
  }
  if (at_len > 0) {
    FREE (at_idx);
    at_len = 0;
    at_idx = NULL;
    bitset_free (at_group);
    bitset_free (at_tmp);
    for (int i=0; i < pending_sz; i++) {
      bitset_free (at_use[i]);
    }
    FREE (at_use);
    FREE (grouped);
  }
  _pending_prs.clear ();
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
    v = 0;
    fatal_error ("and/or loop?!");
    break;

  default:
    v = 0;
    fatal_error ("What?");
    break;
  }
  return v;
}



void ActCellPass::add_new_cell (struct act_prsinfo *pi)
{
  int i;
  
  Assert (!pi->cell, "Hmm...");

  /* add the unexpanded process to the cell namespace, and then
     expand it! */
  Process *proc;

  /*-- create a new process in the namespace --*/
  Assert (cell_ns, "What?");
  UserDef *u = new UserDef (cell_ns);
  proc = new Process (u);
  delete u;
  proc->MkCell ();
  proc->MkExported ();

  /*-- add ports --*/
  InstType *it = TypeFactory::Factory()->NewBool (Type::IN);
  Expr *arr = const_expr (pi->nvars - pi->nout - pi->nat);
  Array *ta;
  it = new InstType (it);
  ta = new Array (arr);
  ta->mkArray ();
  it->MkArray (ta);

  Assert (proc->AddPort (it, _inport_name) == 1, "Error adding in port?");

  it = TypeFactory::Factory()->NewBool (Type::OUT);
  if (pi->nout > 1) {
    arr = const_expr (pi->nout);
    it = new InstType (it);
    ta = new Array (arr);
    ta->mkArray ();
    it->MkArray (ta);
  }
  Assert (proc->AddPort (it, _outport_name) == 1, "Error adding out port?");

  /*-- prs body --*/
  act_prs *prs_body;
  NEW (prs_body, act_prs);
  prs_body->vdd = NULL;
  prs_body->gnd = NULL;
  prs_body->psc = NULL;
  prs_body->nsc = NULL;
  prs_body->next = NULL;
  prs_body->leak_adjust = pi->get_leak_flag();

  act_prs_lang_t *rules = NULL;

  /*-- now do the rules: @-labels go first --*/
  for (int j = 0; j < A_LEN (pi->up); j++) {
    if (pi->up[j]) {
      act_prs_lang_t *tmp;
      NEW (tmp, act_prs_lang_t);
      tmp->next = rules;
      rules = tmp;

      rules->type = ACT_PRS_RULE;
      rules->u.one.attr = (j < pi->nout ? pi->nattr[2*j+1] : NULL);
      rules->u.one.arrow_type = 0;
      rules->u.one.label = (j < pi->nout ? 0 : 1);
      rules->u.one.e =
	_convert_prsexpr_to_act (pi->up[j], _inport_name, _outport_name, pi);
      if (rules->u.one.label) {
	char buf[12];
	snprintf (buf, 12, "x%d", j-pi->nout);
	rules->u.one.id = (ActId *) Strdup (buf);
      }
      else {
	if (pi->nout == 1) {
	  rules->u.one.id = new ActId (_outport_name);
	}
	else {
	  rules->u.one.id = new ActId (_outport_name, new Array (const_expr (j)));
	}
      }
      rules->u.one.dir = 1; /* up */
    }
    if (pi->dn[j]) {
      act_prs_lang_t *tmp;
      NEW (tmp, act_prs_lang_t);
      tmp->next = rules;
      rules = tmp;

      rules->type = ACT_PRS_RULE;
      rules->u.one.attr = (j < pi->nout ? pi->nattr[2*j] : NULL);
      rules->u.one.arrow_type = 0;
      rules->u.one.label = (j < pi->nout ? 0 : 1);
      rules->u.one.e =
	_convert_prsexpr_to_act (pi->dn[j], _inport_name, _outport_name, pi);
      if (rules->u.one.label) {
	char buf[12];
	snprintf (buf, 12, "x%d", j-pi->nout);
	rules->u.one.id = (ActId *) Strdup (buf);
      }
      else {
	if (pi->nout == 1) {
	  rules->u.one.id = new ActId (_outport_name);
	}
	else {
	  rules->u.one.id = new ActId (_outport_name, new Array (const_expr (j)));
	}
      }
      rules->u.one.dir = 0; /* dn */
    }
  }

  /*--- now reorder rules if necessary ---*/
  if (pi->nat > 1) {
    act_prs_lang_t *t;
    act_prs_lang_t **atrules;
    bitset_t **atused;
    int *finished;

    MALLOC (pi->at_perm, int, pi->nat);

    MALLOC (atrules, act_prs_lang_t *, pi->nat);
    MALLOC (atused, bitset_t *, pi->nat);
    MALLOC (finished, int, pi->nat);
    t = rules;
    for (int i=0; i < pi->nat; i++) {
      finished[i] = 0;
      atused[i] = bitset_new (pi->nat);
      atrules[i] = t;
      t = t->next;
    }
    for (int i=0; i < pi->nat; i++) {
      _mark_at_used2 (atrules[i]->u.one.e, atused[i], atrules, pi->nat);
    }

    bitset_t *done = bitset_new (pi->nat);
    int count = 0;
    act_prs_lang_t *prev;

    prev = NULL;
    while (count != pi->nat) {
      for (int i=0; i < pi->nat; i++) {
	if (finished[i]) continue;
	if (bitset_subset (atused[i], done)) {
	  pi->at_perm[count] = (pi->nat-1-i);
	  bitset_set (done, i);
	  finished[i] = 1;
	  if (count == 0) {
	    rules = atrules[i];
	    atrules[i]->next = NULL;
	    prev = atrules[i];
	  }
	  else {
	    prev->next = atrules[i];
	    prev = atrules[i];
	  }
	  count++;
	}
      }
    }
    prev->next = t;

    for (int i=0; i < pi->nat; i++) {
      bitset_free (atused[i]);
    }
    FREE (atused);
    FREE (atrules);
    FREE (finished);
  }

  
  prs_body->p = rules;
  proc->AppendBody (new ActBody_Lang  (-1, prs_body));
  proc->MkDefined ();
  pi->cell = proc;
  //proc->Expand (cell_ns, cell_ns->CurScope(), 0, NULL);
  list_append (_new_cells, proc);
  _add_used_cell (_used_cells, proc);

  char *base_name = _get_basename (pi);
  char *buf;
  int idx = 0;
  MALLOC (buf, char, strlen (base_name) + 50);
  do {
    snprintf (buf, strlen (base_name) + 50, "g%sx%d", base_name, idx++);
  } while (cell_ns->findName (buf) != 0);
  cell_ns->CreateType (buf, proc);
  FREE (base_name);
  FREE (buf);
#if 0
  /* now check the actual name */
  list_t *l = list_new ();
  for (int i=0; i < pi->nvars - pi->nout - pi->nat; i++) {
    ActId *tmp;
    Expr *arr = const_expr (i);
    tmp = new ActId (_inport_name, new Array (arr));
    list_append (l, tmp);
  }
  while (rules) {
    if (rules->type == ACT_PRS_RULE) {
      printf ("Rule: ");
      act_print_one_prs (stdout, rules);
      char *nm = act_prs_expr_to_string (l, rules->u.one.e);
      printf ("  > %s\n", nm);
      FREE (nm);
    }
    rules = rules->next;
  }
#endif  
}




static act_prs_expr_t *_convert_prsexpr_to_act (act_prs_expr_t *e,
						const char *_inport_name,
						const char *_outport_name,
						struct act_prsinfo *pi)
{
  int v;
  act_prs_expr_t *ret;
  ActId *id;

  if (!e) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = e->type;
  
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l =
      _convert_prsexpr_to_act (e->u.e.l, _inport_name, _outport_name, pi);
    ret->u.e.r =
      _convert_prsexpr_to_act (e->u.e.r, _inport_name, _outport_name, pi);
    ret->u.e.pchg_type = e->u.e.pchg_type;
    ret->u.e.pchg =
      _convert_prsexpr_to_act (e->u.e.pchg, _inport_name, _outport_name, pi);
    break;
    
  case ACT_PRS_EXPR_NOT:
    ret->u.e.l =
      _convert_prsexpr_to_act (e->u.e.l, _inport_name, _outport_name, pi);
    break;

  case ACT_PRS_EXPR_VAR:
    v = (unsigned long)e->u.v.id;
    if (v < pi->nout) {
      if (pi->nout == 1) {
	ret->u.v.id = new ActId (_outport_name);
      }
      else {
	Expr *tmp = const_expr (v);
	ret->u.v.id = new ActId (_outport_name, new Array (tmp));
      }
    }
    else if (v >= (pi->nout + pi->nat)) {
      Expr *tmp = const_expr (v-pi->nout-pi->nat);
      ret->u.v.id = new ActId (_inport_name, new Array (tmp));
    }
    else {
      /* XXX should not be here */
      fatal_error ("Should not be here?");
      /*fprintf (fp, "@x%d", v-pi->nout);*/
    }
    ret->u.v.sz = e->u.v.sz;
    break;

  case ACT_PRS_EXPR_LABEL:
    v = (long)e->u.l.label;
    {
      char buf[12];
      snprintf (buf, 12, "x%d", v-pi->nout);
      ret->u.l.label = Strdup (buf);
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
  return ret;
}

static void _dump_expr (FILE *fp,
			const char *_inport_name,
			const char *_outport_name,
			act_prs_expr_t *e, struct act_prsinfo *pi, int prec = 0)
{
  int v;

  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
    if (prec > 2) {
      fprintf (fp, "(");
    }
    _dump_expr (fp, _inport_name, _outport_name, e->u.e.l, pi, 2);
    fprintf (fp, " & ");
    _dump_expr (fp, _inport_name, _outport_name, e->u.e.r, pi, 2);
    if (prec > 2) {
      fprintf (fp, ")");
    }
    /* XXX: precharges? */
    break;
    
  case ACT_PRS_EXPR_OR:
    if (prec > 1) {
      fprintf (fp, "(");
    }
    _dump_expr (fp, _inport_name, _outport_name, e->u.e.l, pi, 1);
    fprintf (fp, " | ");
    _dump_expr (fp, _inport_name, _outport_name, e->u.e.r, pi, 1);
    if (prec > 1) {
      fprintf (fp, ")");
    }
    break;

  case ACT_PRS_EXPR_NOT:
    fprintf (fp, "~");
    _dump_expr (fp, _inport_name, _outport_name, e->u.e.l, pi, 3);
    break;

  case ACT_PRS_EXPR_VAR:
    v = (unsigned long)e->u.v.id;
    if (v < pi->nout) {
      if (pi->nout == 1) {
	fprintf (fp, "%s", _outport_name);
      }
      else {
	fprintf (fp, "%s[%d]", _outport_name, v);
      }
    }
    else if (v >= (pi->nout + pi->nat)) {
      fprintf (fp, "%s[%d]", _inport_name, v-pi->nout-pi->nat);
    }
    else {
      fprintf (fp, "@x%d", v-pi->nout);
    }
    if (e->u.v.sz) {
      act_print_size (fp, e->u.v.sz);
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    v = (long)e->u.l.label;
    fprintf (fp, "@x%d", v-pi->nout);
    break;

  case ACT_PRS_EXPR_TRUE:
    fprintf (fp, "true");
    break;
    
  case ACT_PRS_EXPR_FALSE:
    fprintf (fp, "false");
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
    ret->u.v.id = (ActId *)(long)i->find_or_alloc (e->u.v.id);
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l.label = (char *)(long)i->find_or_alloc ((ActId *)e->u.l.label, 1);
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

/*
 * If the rule doesn't exist, set the expression pointer to the
 * rule. Otherwise, OR in the new expression with the existing one.
 */
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

/*-- convert prs block into attriburtes used for isomorphism checking --*/
struct act_prsinfo *ActCellPass::_gen_prs_attributes (act_prs_lang_t *prs,
						      int ninp, int noutp)
{
  struct act_prsinfo *ret;
  act_prs_lang_t *l, *lpush;
  struct idmap imap;
  int i;
  int in_tree;

  current_idmap.clear();

  ret = new act_prsinfo (_leak_flag);

  /* create a slot for each output from this prs, if the number of
     outputs is known. */
  for (int i=0; i < noutp; i++) {
    ActId *tmp = new ActId (_outport_name);
    if (noutp > 1) {
      tmp->setArray (new Array (const_expr (i)));
    }
    tmp = tmp->Expand (NULL, NULL);
    imap.alloc_new_id (tmp);
    ret->add_new_slot ();
  }

  /* walk through the production rules and extract all the outputs */
  l = prs;
  lpush = NULL;
  in_tree = 0;
  int tree_count = 0;
  while (l) {
    if (l->type == ACT_PRS_TREE) {
      tree_count++;
      if (tree_count != 1) {
	fatal_error ("More than one tree!");
      }
      if (l->u.l.lo) {
	ret->set_tree_info (l->u.l.lo->u.ival.v);
      }
      else {
	ret->set_tree_info (0);
      }
      lpush = l->next;
      l = l->u.l.p;
      in_tree = 1;
    }

    if (l->type == ACT_PRS_SUBCKT) {
      act_error_ctxt (stderr);
      fatal_error ("prs attribute computation called on a subcircuit; nested subckt in design?");
    }

    Assert (l->type == ACT_PRS_RULE, "gen_prs_attributes context error");

    /* look for this variable */
    if (!l->u.one.label) {
      /* if I haven't seen it before, add it! */
      i = imap.find_idx (l->u.one.id);
      if (i == -1) {
	i = imap.alloc_new_id (l->u.one.id);
	ret->add_new_slot ();
      }
    }
    l = l->next;
    if (!l) {
      l = lpush;
      in_tree = 0;
    }
  }

  /*-- set nout field, now that all outputs have been found and
     collected. --*/
  imap.finalize_outs ();
  ret->set_num_outputs (imap.num_outputs());

  /*-- collect all labels --*/
  l = prs;
  while (l) {
    if (l->u.one.label) {
      ret->nat++;
      if (imap.find_idx (l->u.one.id) == -1) {
	imap.alloc_new_atid (l->u.one.id);
	ret->add_new_slot ();
      }
    }
    l = l->next;
  }

  /*-- collect specified inputs --*/
  if (ninp > 0) {
    for (int i=0; i < ninp; i++) {
      ActId *tmp = new ActId (_inport_name, new Array (const_expr (i)));
      tmp = tmp->Expand (NULL, NULL);
      imap.find_or_alloc (tmp);
    }
  }

  /*-- add any new inputs found in the prs, scrubbing rules as we go --*/
  l = prs;
  lpush = NULL;
  in_tree = 0;
  while (l) {
    if (l->type == ACT_PRS_TREE) {
      lpush = l->next;
      l = l->u.l.p;
      in_tree = 1;
    }

    /* collect all the inputs, scrub rules */
    i = imap.find_or_alloc (l->u.one.id, l->u.one.label);
    
    /* now process the production rule, create a clean copy, replace
       all variables with ids! */
    act_prs_expr_t *x = _scrub_rule (&imap, l->u.one.e);
    act_prs_expr_t *xx = NULL;

    if (l->u.one.attr) {
      if (i >= ret->nout) {
	fatal_error ("Attributes on labels? This is not supported");
      }
      ret->nattr[2*i+(l->u.one.dir ? 1 : 0)] = l->u.one.attr;
    }
    
    if (l->u.one.arrow_type == 1) {
      xx = act_prs_complement_rule (x);
    }
    else if (l->u.one.arrow_type == 2) {
      xx = act_prs_celement_rule (x);
    }

    /* dn first, then up */
    if (!l->u.one.dir) {
      /* down rule */
      _add_rule (&ret->dn[i], x);
      if (l->u.one.arrow_type != 0) {
	_add_rule (&ret->up[i], xx);
      }
    }
    else {
      /* up rule */
      if (l->u.one.arrow_type != 0) {
	_add_rule (&ret->dn[i], xx);
      }
      _add_rule (&ret->up[i], x);
    }
    l = l->next;
    if (!l) {
      l = lpush;
      in_tree = 0;
    }
  }
  
  /* scrubbed rules are now ready: now we can create the varinfo space */
  ret->set_tot_vars (imap.num_ids ());
  Assert (A_LEN (ret->up) == A_LEN (ret->dn), "hmm");
  Assert (A_LEN (ret->up) >= ret->nout, "what happened?");

  /* mark tree flag */
  l = prs;
  lpush = NULL;
  in_tree = 0;
  while (l) {
    if (l->type == ACT_PRS_TREE) {
      lpush = l->next;
      l = l->u.l.p;
      in_tree = 1;
    }
    if (!l->u.one.label && in_tree) {
      i = imap.find_idx (l->u.one.id);
      Assert (i != -1, "What?");
      if (l->u.one.dir == 0) {
	ret->attrib[i].tree |= 1;
      }
      else {
	ret->attrib[i].tree |= 2;
      }
    }
    l = l->next;
    if (!l) {
      l = lpush;
      in_tree = 0;
    }
  }

  /* 1. Gather counts */
  for (i=0; i < A_LEN (ret->up); i++) {
    _count_occurrences (ret, ret->up[i], 1);
    _count_occurrences (ret, ret->dn[i], 0);
  }
  
  /* 2. Gather depths */
  for (i=0; i < ret->numvars(); i++) {
    if ((ret->attrib[i].nup + ret->attrib[i].ndn) > 0) {
      MALLOC (ret->attrib[i].depths, int,
	      ret->attrib[i].nup + ret->attrib[i].ndn);
      for (int j=0; j < (ret->attrib[i].nup + ret->attrib[i].ndn); j++) {
	ret->attrib[i].depths[j] = 0;
      }
      ret->attrib[i].cup = 0;

      // counts for pull down start after pull-up
      ret->attrib[i].cdn = ret->attrib[i].nup;
    }
  }
  for (i=0; i < A_LEN (ret->up); i++) {
    _collect_depths (ret, ret->up[i], 1);
    _collect_depths (ret, ret->dn[i], 0);
  }

  /* Sort depths to canonicalize */
  for (i=0; i < ret->numvars(); i++) {
    if (ret->attrib[i].depths) {
      myintmergesort (ret->attrib[i].depths, ret->attrib[i].nup);
      myintmergesort (ret->attrib[i].depths + ret->attrib[i].nup,
		      ret->attrib[i].ndn);
    }
  }

  /*-- now sort attributes! --*/
  struct act_varinfo **_core_array;
  MALLOC (_core_array, struct act_varinfo *, ret->nvars);
  MALLOC (ret->attr_map, int, ret->nvars);
  for (i=0; i < ret->nvars; i++) {
    _core_array[i] = &ret->attrib[i];
#if 0
    printf ("%lu ", (unsigned long)(_core_array[i]- &ret->attrib[0]));
#endif    
  }

  mymergesort ((const void **)_core_array, ret->nvars, cmp_fn_varinfo);

#if 0
  printf ("GEN-PRS-ATTRIB:\n");
  printf("map: ");
#endif  
  for (i=0; i < ret->nvars; i++) {
    ret->attr_map[i] = (_core_array[i] - &ret->attrib[0]);
#if 0
    printf (" %d", ret->attr_map[i]);
#endif    
  }
#if 0
  printf("\n");
#endif  
  FREE (_core_array);

  /* permute the attribute array so it is sorted */
  struct act_varinfo *tmp;
  MALLOC (tmp, struct act_varinfo, ret->nvars);
  for (i=0; i < ret->nvars; i++) {
    tmp[i] = ret->attrib[i];
  }
  for (i=0; i < ret->nvars; i++) {
    ret->attrib[i] = tmp[ret->attr_map[i]];
  }
  FREE (tmp);

  current_idmap = imap;
  imap.moved ();

  return ret;
}


static void _dump_prsinfo (struct act_prsinfo *p, const char *_inport_name,
			   const char *_outport_name)
{
  printf ("-------\n");
  if (p->cell) {
    printf (" cell: %s\n", p->cell->getName());
  }
  else {
    printf (" cell: -no-name-\n");
  }
  printf ("  nvars=%d, nout=%d, nat=%d, tval=%d\n",
	  p->nvars, p->nout, p->nat, p->get_tree_info());

  for (int i=0; i < A_LEN (p->up); i++) {
    printf (" %d: up = ", i);
    if (!p->up[i]) { printf ("n/a"); }
    else { _dump_expr (stdout, _inport_name, _outport_name, p->up[i], p); }
    printf (" ; dn = ");
    if (!p->dn[i]) { printf ("n/a"); }
    else { _dump_expr (stdout, _inport_name, _outport_name, p->dn[i], p); }
    printf ("\n");
  }
  for (int i=0; i < p->numvars(); i++) {
    printf (" var %d: ", i);
    printf ("nup %d, ndn %d, tree %d; ", p->attrib[i].nup, p->attrib[i].ndn,
	    p->attrib[i].tree);
    printf (" depths: ");
    for (int k=0; k < p->attrib[i].nup + p->attrib[i].ndn; k++) {
      printf ("%d ", p->attrib[i].depths[k]);
    }
    printf ("\n");
  }
  printf ("perm: ");
  for (int i=0; i < p->numvars(); i++) {
    printf (" %d", p->attr_map[i]);
  }
  printf ("\n");
  if (p->at_perm) {
    printf ("at-perm:");
    for (int i=0; i < p->nat; i++) {
      printf (" %d", p->at_perm[i]);
    }
    printf ("\n");
  }
  printf ("-------\n");
}

static void _dump_prs_cell (FILE *fp,
			    const char *_inport_name,
			    const char *_outport_name,
			    struct act_prsinfo *p, const char *name)
{
  const char *s = name;
  fprintf (fp, "export defcell ");
  while (*s && *s != '<') {
    fputc (*s, fp);
    s++;
  }
  fprintf (fp, " (");
  fprintf (fp, "bool? %s[%d]; bool! %s", _inport_name,
	   p->nvars - p->nout - p->nat, _outport_name);
  if (p->nout > 1) {
    fprintf (fp, "[%d]", p->nout);
  }
  fprintf (fp, ")\n{\n   prs %s{\n", p->get_leak_flag() ? "* " : "");

  int has_tree = p->get_tree_info();
  /*
    Go through this once or twice depending on if this has a tree { }
     directive or not
  */
  for (int k=0; k < 1 + (has_tree != -1 ? 1 : 0); k++) {
    int idx = p->nout-1;

    if (has_tree != -1 && k == 1) {
      fprintf (fp, "   tree");
      if (has_tree != 0) {
	fprintf (fp, "<%d>", has_tree);
      }
      fprintf (fp, " {\n");
    }
    
    for (int ii=0; ii < A_LEN (p->up); ii++) {
      int i;
      idx = (idx + 1) % A_LEN (p->up);
      if ((idx >= p->nout) && (p->at_perm)) {
	i = p->nout + p->at_perm[idx-p->nout];
      }
      else {
	i = idx;
      }

      if (k == 0 && (p->attrib[i].tree & 1) == 0 ||
	  k == 1 && (p->attrib[i].tree & 1) == 1) {
	
	if (p->dn[i]) {
	  fprintf (fp, "   ");

	  if ((i < p->nout) && p->nattr[2*i]) {
	    act_attr_t *a;
	    fprintf (fp, "[");
	    for (a = p->nattr[2*i]; a; a = a->next) {
	      fprintf (fp, "%s=", a->attr);
	      print_expr (fp, a->e);
	      if (a->next) {
		fprintf (fp, "; ");
	      }
	    }
	    fprintf (fp, "] ");
	  }

	  _dump_expr (fp, _inport_name, _outport_name, p->dn[i], p);
	  fprintf (fp, " -> ");
	  if (i < p->nout) {
	    if (p->nout == 1) {
	      fprintf (fp, "%s-", _outport_name);
	    }
	    else {
	      fprintf (fp, "%s[%d]-", _outport_name, i);
	    }
	  }
	  else {
	    fprintf (fp, "@x%d-", i-p->nout);
	  }
	  fprintf (fp, "\n");
	}
      }

      if (k == 0 && (p->attrib[i].tree & 2) == 0 ||
	  k == 1 && (p->attrib[i].tree & 2) == 2) {
	if (p->up[i]) {
	  fprintf (fp, "   ");

	  if ((i < p->nout) && p->nattr[2*i+1]) {
	    act_attr_t *a;
	    fprintf (fp, "[");
	    for (a = p->nattr[2*i+1]; a; a = a->next) {
	      fprintf (fp, "%s=", a->attr);
	      print_expr (fp, a->e);
	      if (a->next) {
		fprintf (fp, "; ");
	      }
	    }
	    fprintf (fp, "] ");
	  }
      
	  _dump_expr (fp, _inport_name, _outport_name, p->up[i], p);
	  fprintf (fp, " -> ");
	  if (i < p->nout) {
	    if (p->nout == 1) {
	      fprintf (fp, "%s+", _outport_name);
	    }
	    else {
	      fprintf (fp, "%s[%d]+", _outport_name, i);
	    }
	  }
	  else {
	    fprintf (fp, "@x%d+", i-p->nout);
	  }
	  fprintf (fp, "\n");
	}
      }
    }
    if (has_tree != -1 && k == 1) {
      fprintf (fp, "   }\n");
    }
  }
  
#if 0  
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
#endif
  fprintf (fp, "   }\n}\n\n");
}


struct cell_name {
  struct act_prsinfo *p;
  const char *name;
};

static int _cmp_cells (const void *a, const void *b)
{
  const struct cell_name *xa, *xb;
  xa = (struct cell_name *)a;
  xb = (struct cell_name *)b;
  return strcmp (xa->name, xb->name);
}


void ActCellPass::dump_celldb (FILE *fp)
{
  int i;
  chash_bucket_t *b;
  chash_iter_t iter;
  struct act_prsinfo *pi;

  A_DECL (struct cell_name *, cells);
  A_INIT (cells);

  if (!cell_table) return;

  chash_iter_init (cell_table, &iter);
  if (cell_table->n > 0) {
    while ((b = chash_iter_next (cell_table, &iter))) {
      pi = (struct act_prsinfo *)b->v;
      A_NEW (cells, struct cell_name *);
      NEW (A_NEXT (cells), struct cell_name);
      A_NEXT (cells)->p = pi;
      if (pi->cell) {
	A_NEXT (cells)->name = pi->cell->getName();
      }
      else {
	A_NEXT (cells)->name = _get_basename (pi);
      }
      A_INC (cells);
    }
  }

  /* lets sort the cell names */
  mymergesort ((const void **)cells, A_LEN (cells), _cmp_cells);

  fprintf (fp, "namespace %s {\n\n", config_get_string ("net.cell_namespace"));
  
  /* print */
  for (i=0; i < A_LEN (cells); i++) {
    pi = cells[i]->p;
    _dump_prs_cell (fp, _inport_name, _outport_name, pi, cells[i]->name);
    if (!pi->cell) {
      FREE ((void*)cells[i]->name);
    }
    FREE (cells[i]);
  }
  A_FREE (cells);

  fprintf (fp, "export template<pint w,l> defcell p0(bool? %s[2]; bool! %s) {\n", _inport_name, _outport_name);
  fprintf (fp, "  prs { passp<w,l> (%s[0],%s[1],%s) }\n}\n\n",
	   _inport_name, _inport_name, _outport_name);

  fprintf (fp, "export template<pint w,l> defcell n0(bool? %s[2]; bool! %s) {\n", _inport_name, _outport_name);
  fprintf (fp, "  prs { passn<w,l> (%s[0],%s[1],%s) }\n}\n\n",
	   _inport_name, _inport_name, _outport_name);

  fprintf (fp, "export template<pint w,l> defcell t0(bool? %s[3]; bool! %s) {\n", _inport_name, _outport_name);
  fprintf (fp, "  prs { transgate<w,l> (%s[0],%s[1],%s[2],%s) }\n}\n\n",
	   _inport_name, _inport_name, _inport_name, _outport_name);

  fprintf (fp, "export defcell p1(bool? %s[2]; bool! %s) {\n",
	   _inport_name, _outport_name);
  fprintf (fp, "  prs { passp (%s[0],%s[1],%s) }\n}\n\n",
	   _inport_name, _inport_name, _outport_name);

  fprintf (fp, "export defcell n1(bool? %s[2]; bool! %s) {\n",
	   _inport_name, _outport_name);
  fprintf (fp, "  prs { passn (%s[0],%s[1],%s) }\n}\n\n",
	   _inport_name, _inport_name, _outport_name);

  fprintf (fp, "export defcell t1(bool? %s[3]; bool! %s) {\n",
	   _inport_name, _outport_name);
  fprintf (fp, "  prs { transgate (%s[0],%s[1],%s[2],%s) }\n}\n\n",
	   _inport_name, _inport_name, _inport_name, _outport_name);

  int max;
  char **table;
  max = config_get_table_size ("act.prs_device");
  table = config_get_table_string ("act.prs_device");
  for (int i=0; i < max; i++) {
    fprintf (fp, "export template<pint w,l> defcell c%d(bool %s; bool %s) {\n",
	     2*i, _inport_name, _outport_name);
    fprintf (fp, "  prs { %s<w,l> (%s,%s) }\n}\n\n",
	     table[i], _inport_name, _outport_name);
    fprintf (fp, "export defcell c%d(bool %s; bool %s) {\n",
	     2*i+1, _inport_name, _outport_name);
    fprintf (fp, "  prs { %s (%s,%s) }\n}\n\n",
	     table[i], _inport_name, _outport_name);
  }
  fprintf (fp, "\n\n}\n");
}


Expr *ActCellPass::_idexpr (int idx, struct act_prsinfo *pi)
{
  if (pi->match_perm) {
    return _id_to_expr (current_idmap.getId (pi->match_perm[idx]));
  }
  else {
    return _id_to_expr (current_idmap.getId (idx));
  }
}

ActBody_Conn *ActCellPass::_build_connections (const char *name,
					       struct act_prsinfo *pi)
{
  int i;
  ActId *instname;

  /*--- variable order:  outputs at-vars inputs ---*/

  /*-- inputs --*/
  AExpr *a, *ret;
  Expr *idexpr;

  i = pi->nout + pi->nat;

  if (i == pi->nvars) {
    // no inputs, so nothing to connect to
    return NULL;
  }
  Assert (i < pi->nvars, "No inputs?");

  idexpr = _idexpr (i, pi);
  
  ret = new AExpr (AExpr::COMMA, new AExpr (idexpr), NULL);
  a = ret;
  i++;
  for (; i < pi->nvars; i++) {
    idexpr = _idexpr (i, pi);
    a->SetRight (new AExpr (AExpr::COMMA,
			    new AExpr (idexpr), NULL));
    a = a->GetRight ();
  }

  instname = new ActId (name);
  instname->Append (new ActId (_inport_name));
  
  ActBody_Conn *ac = new ActBody_Conn (-1, instname, ret);

  instname = new ActId (name);
  instname->Append (new ActId (_outport_name));

  idexpr = _idexpr (0, pi);
  if (pi->nout == 1) {
    ac->Append (new ActBody_Conn (-1, instname, new AExpr (idexpr)));
  }
  else {
    ret = new AExpr (AExpr::COMMA, new AExpr (idexpr), NULL);
    a = ret;
    i = 1;
    for (; i < pi->nout; i++) {
      idexpr = _idexpr (i, pi);
      a->SetRight (new AExpr (AExpr::COMMA,
			      new AExpr (idexpr), NULL));
      a = a->GetRight ();
    }
    ac->Append (new ActBody_Conn (-1, instname, ret));
  }
  return ac;
}

ActBody_Conn *ActCellPass::_build_connections (const char *name,
					       act_prs_lang_t *gate)
{
  ActId *instname;
  
  /*--- variable order:  outputs at-vars inputs ---*/

  /*-- inputs --*/
  AExpr *a, *ret;
  Expr *idexpr;

  ret = NULL;
  if (gate->u.p.g && gate->u.p._g) {
    ret = new AExpr (AExpr::COMMA, new AExpr (_id_to_expr(gate->u.p.g)), NULL);
    a = ret;
    a->SetRight (new AExpr (AExpr::COMMA,
			    new AExpr (_id_to_expr (gate->u.p._g)), NULL));
    a = a->GetRight ();
    a->SetRight (new AExpr (AExpr::COMMA,
			    new AExpr (_id_to_expr (gate->u.p.s)), NULL));
  }
  else if (gate->u.p.g) {
    ret = new AExpr (AExpr::COMMA, new AExpr (_id_to_expr (gate->u.p.g)), NULL);
    a = ret;
    a->SetRight (new AExpr (AExpr::COMMA,
			    new AExpr (_id_to_expr (gate->u.p.s)), NULL));
  }
  else if (gate->u.p._g) {
    ret = new AExpr (AExpr::COMMA, new AExpr (_id_to_expr (gate->u.p._g)), NULL);
    a = ret;
    a->SetRight (new AExpr (AExpr::COMMA,
			    new AExpr (_id_to_expr (gate->u.p.s)), NULL));
  }
  else {
    /* cap */
    ret = new AExpr (_id_to_expr (gate->u.p.s));
  }

  instname = new ActId (name);
  instname->Append (new ActId (_inport_name));
  
  ActBody_Conn *ac = new ActBody_Conn (-1, instname, ret);

  instname = new ActId (name);
  instname->Append (new ActId (_outport_name));

  ac->Append (new ActBody_Conn (-1, instname, new AExpr (_id_to_expr (gate->u.p.d))));

  return ac;
}


/*
 * Is the rule I have here in a member of the group?
 */
static int _in_subgroup (act_prs_lang_t *prs,
			 act_prs_lang_t *prsgroup)
{
  Assert (prsgroup->type == ACT_PRS_TREE, "Need a group as 2nd arg");

  act_prs_lang_t *p;

  for (p = prsgroup->u.l.p; p; p = p->next) {
    if (prs->u.one.id == p->u.one.id ||
	prs->u.one.id->isEqual (p->u.one.id)) {
      return 1;
    }
  }
  return 0;
}

void ActCellPass::_create_new_cell (Scope *sc, act_prs_lang_t *prslist)
{
  chash_bucket_t *b;
  act_prsinfo *pi;
  
  /* see if we can match this production rule list against a cell */
  pi = _gen_prs_attributes (prslist);

  Assert (cell_table, "No cell table?!");

  b = chash_lookup (cell_table, pi);
  if (b) {
    /* found match! */
  }
  else {
#if 0
    printf (">> NEW CELL NEEDED!\n");
    _dump_prsinfo (pi, "in", "out");
#endif
    add_new_cell (pi);
    b = chash_add (cell_table, pi);
    b->v = pi;
  }
  pi = (struct act_prsinfo *)b->key;
    
  char buf[100];
  do {
    snprintf (buf, 100, "cx%d", proc_inst_count++);
  } while (sc->Lookup (buf));

  Assert (sc->isExpanded(), "Hmm");

  _add_used_cell (_used_cells, pi->cell);
    
  InstType *it = new InstType (sc, pi->cell, 0);
  it = it->Expand (NULL, sc);

  Assert (it->isExpanded(), "Hmm");
    
  Assert (sc->Add (buf, it), "What?");
  /*--- now make connections --*/
    
  ActBody_Conn *ac;

  //printf (" --- [%s] \n", p->getName());
  ac = _build_connections (buf, pi);
  //ac->Print (stdout);
  //ac->Next()->Print (stdout);
  //printf (" --- \n");

  if (ac) {
    int oval = Act::double_expand;
    Act::double_expand = 0;
    ac->Expandlist (NULL, sc);
    Act::double_expand = oval;
  }

  if (pi->match_perm) {
    FREE (pi->match_perm);
    pi->match_perm = NULL;
  }
}

bool ActCellPass::_collect_one_prs (Scope *sc, act_prs_lang_t *prs)
{
  int i;
  act_prs_lang_t newprs, newprs2;
  
  // add this prs to the list, if it is paired then we can find a gate
  Assert (prs->type == ACT_PRS_RULE, "Hmm.");

  if (prs->u.one.arrow_type != 0) {
    /* we're good, let's do this! */
    newprs = *prs;
    newprs.next = NULL;

    /*-- newprs is used to generate attributes, so we're all set --*/
    if (_uses_a_label (prs->u.one.e)) {
      fatal_error ("@-expressions can only be used with -> production rules");
    }
  }
  else {
    /* labels can only occur once, so we are done */
    if (prs->u.one.label) {
      _pending_prs.push_back (prs);
      return true;
    }

    /* check if this matches a pending tree group; if so, fuse and quit */
    for (int i=0; i < _pending_group.size(); i++) {
      auto &g = _pending_group[i];
      int pos = g.has_var (prs->u.one.id);
      if (pos != -1) {
	if ((g.get_dir (pos) & (prs->u.one.dir ? 2 : 1)) != 0) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "ERROR: Production rule is both within and outside tree {} block, or\nhas multiple rules outside tree block.\n");
	  fprintf (stderr, "    ID: ");
	  prs->u.one.id->Print (stderr);
	  fprintf (stderr, "\n");
	  exit (1);
	}
	g.add_dir (pos, prs->u.one.dir ? 2 : 1);
	g._pending.push_back (prs);

	if (g.complete()) {
	  flush_group (sc, &g);
	}

	return true;
      }
    }

    /* check if this matches an existing pending production rule */
    for (i=0; i < _pending_prs.size(); i++) {
      if (prs->u.one.id == _pending_prs[i]->u.one.id ||
	  prs->u.one.id->isEqual (_pending_prs[i]->u.one.id)) {
	break;
      }
    }
    if (i == _pending_prs.size()) {
      /* this is a new RHS that is pending, so add it and return */
      _pending_prs.push_back (prs);
      return true;
    }

    newprs = *(_pending_prs[i]);
    
    if (prs->u.one.dir == newprs.u.one.dir) {
      /* Multiple production rules with the same RHS; OR-combine it */
      
      act_prs_expr_t *eor;  /* XXX: NEED TO FREE THESE ALLOCATIONS */
      act_prs_lang_t *tmp;
      
      NEW (eor, act_prs_expr_t);
      eor->type = ACT_PRS_EXPR_OR;
      eor->u.e.l = prs->u.one.e;
      eor->u.e.r = newprs.u.one.e;
      eor->u.e.pchg_type = -1;
      eor->u.e.pchg = NULL;
      newprs.u.one.e = eor;
      if (prs->u.one.attr && !newprs.u.one.attr) {
	newprs.u.one.attr = prs->u.one.attr;
      }
      NEW (tmp, act_prs_lang_t);
      *tmp = newprs;
      _pending_prs[i] = tmp;
      return true;
    }

    /* 
       We now have a pull-up and pull-down; check if it needs labels!
    */
    if (_uses_a_label (prs->u.one.e) || _uses_a_label (newprs.u.one.e)) {
      /* if this rule uses a label, or the matching production rule uses a
	 label, then we just add it to the pending list to be handled
	 later.
      */
      _pending_prs.push_back (prs);
      return true;
    }

    /* newprs/newprs2 are used to generate attributes */
    newprs.next = &newprs2;
    newprs2 = *prs;
    newprs2.next = NULL;
    
    /* compact pending prs array */
    i++;
    for (; i < _pending_prs.size(); i++) {
      _pending_prs[i-1] = _pending_prs[i];
    }
    _pending_prs.pop_back();
  }

  _create_new_cell (sc, &newprs);

  return true;
}

/*
 * Collect pass transistor or transmission gate, using the default
 * templated/non-templated cells.
 */
void ActCellPass::_collect_one_passgate (Scope *sc, act_prs_lang_t *prs)
{
  int i;
  
  // add this prs to the list, if it is paired then we can find a gate
  Assert (prs->type == ACT_PRS_GATE, "Hmm.");

  /* XXX: ignoring attributes! */

  char buf[100];
  do {
    snprintf (buf, 100, "cx%d", proc_inst_count++);
  } while (sc->Lookup (buf));

  Assert (sc->isExpanded(), "Hmm");

  Process *cell;
  Assert (cell_ns, "No cell namespace!");

  if (prs->u.p.g && prs->u.p._g) {
    if (prs->u.p.sz) {
      cell = dynamic_cast<Process *>(cell_ns->findType ("t0"));
    }
    else {
      cell = dynamic_cast<Process *>(cell_ns->findType ("t1"));
    }
  }
  else if (prs->u.p.g) {
    if (prs->u.p.sz) {
      cell = dynamic_cast<Process *>(cell_ns->findType ("n0"));
    }
    else {
      cell = dynamic_cast<Process *>(cell_ns->findType ("n1"));
    }
  }
  else {
    if (prs->u.p.sz) {
      cell = dynamic_cast<Process *>(cell_ns->findType ("p0"));
    }
    else {
      cell = dynamic_cast<Process *>(cell_ns->findType ("p1"));
    }
  }

  Assert (cell, "No transmission gates?");

  InstType *it = new InstType (sc, cell, 0);
  int w, l;
  if (prs->u.p.sz) {
    if (prs->u.p.sz->w) {
      if (prs->u.p.sz->w->type == E_INT) {
	w = prs->u.p.sz->w->u.ival.v;
      }
      else {
	w = prs->u.p.sz->w->u.f;
      }
    }
    else {
      w = config_get_int ("net.std_p_width");
    }
    if (prs->u.p.sz->l) {
      if (prs->u.p.sz->l->type == E_INT) {
	l = prs->u.p.sz->l->u.ival.v;
      }
      else {
	l = prs->u.p.sz->l->u.f;
      }
    }
    else {
      l = config_get_int ("net.std_p_length");
    }
    it->setNumParams (2);
    it->setParam (0, new AExpr (const_expr (w)));
    it->setParam (1, new AExpr (const_expr (l)));
  }
  else {
    /* nothing to do */
  }
  it = it->Expand (NULL, sc);

  Assert (it->isExpanded(), "Hmm");
  
  Assert (sc->Add (buf, it), "What?");
    /*--- now make connections --*/
    
  ActBody_Conn *ac;

    //printf (" --- [%s] \n", p->getName());
  ac = _build_connections (buf, prs);
    //ac->Print (stdout);
    //ac->Next()->Print (stdout);
    //printf (" --- \n");

  if (ac) {
    int oval = Act::double_expand;
    Act::double_expand = 0;
    ac->Expandlist (NULL, sc);
    Act::double_expand = oval;
  }
}

/*
 * Replace a two-terminal device with a c# instance (like a cap) from
 * the default templated/non-templated cells.
 */
void ActCellPass::_collect_one_cap (Scope *sc, act_prs_lang_t *prs)
{
  int i;
  
  // add this prs to the list, if it is paired then we can find a gate
  Assert (ACT_PRS_LANG_TYPE (prs->type) == ACT_PRS_DEVICE, "Hmm.");

  /* XXX: ignoring attributes! */

  char buf[100];
  do {
    snprintf (buf, 100, "cx%d", proc_inst_count++);
  } while (sc->Lookup (buf));

  Assert (sc->isExpanded(), "Hmm");

  Process *cell;
  Assert (cell_ns, "No cell namespace!");

  char cname[10];


  if (prs->u.p.sz) {
    snprintf (cname, 10, "c%d", 2*(prs->type - ACT_PRS_DEVICE));
  }
  else {
    snprintf (cname, 10, "c%d", 2*(prs->type - ACT_PRS_DEVICE)+1);
  }
  cell = dynamic_cast<Process *>(cell_ns->findType (cname));

  Assert (cell, "No devices?");

  InstType *it = new InstType (sc, cell, 0);
  int w, l;
  if (prs->u.p.sz) {
    if (prs->u.p.sz->w) {
      if (prs->u.p.sz->w->type == E_INT) {
	w = prs->u.p.sz->w->u.ival.v;
      }
      else {
	w = prs->u.p.sz->w->u.f;
      }
    }
    else {
      w = 1;
    }
    if (prs->u.p.sz->l) {
      if (prs->u.p.sz->l->type == E_INT) {
	l = prs->u.p.sz->l->u.ival.v;
      }
      else {
	l = prs->u.p.sz->l->u.f;
      }
    }
    else {
      l = 1;
    }
    it->setNumParams (2);
    it->setParam (0, new AExpr (const_expr (w)));
    it->setParam (1, new AExpr (const_expr (l)));
  }
  else {
    /* nothing to do */
  }
  it = it->Expand (NULL, sc);

  Assert (it->isExpanded(), "Hmm");
  
  Assert (sc->Add (buf, it), "What?");
    /*--- now make connections --*/
    
  ActBody_Conn *ac;

    //printf (" --- [%s] \n", p->getName());
  ac = _build_connections (buf, prs);
    //ac->Print (stdout);
    //ac->Next()->Print (stdout);
    //printf (" --- \n");

  if (ac) {
    int oval = Act::double_expand;
    Act::double_expand = 0;
    ac->Expandlist (NULL, sc);
    Act::double_expand = oval;
  }
}

void ActCellPass::_collect_treegroup (Scope *sc, act_prs_lang_t *prs)
{
  int tval;

  Assert (prs->type == ACT_PRS_TREE, "Why am I here?");

  if (prs->u.l.lo) {
    tval = prs->u.l.lo->u.ival.v;
  }
  else {
    tval = 1;
  }
  Assert (tval > 0, "tree<> directive with <= 0 value?");

  /*
   * This is a group.
   *
   * 1. There should be no @-labels either used or generated.
   *
   * 2. Make sure there isn't another pending group with an
   * intersection.
   *
   * 3. Add it to the group list.
   */

  act_prs_lang_t *l;
  l = prs->u.l.p;
  while (l) {
    if (l->type != ACT_PRS_RULE) {
      act_error_ctxt (stderr);
      fatal_error ("tree { } can only contain prs rules");
    }
    if (l->u.one.label || _uses_a_label (l->u.one.e)) {
      act_error_ctxt (stderr);
      fatal_error ("tree { } block cannot use @-labels");
    }
    if (l->u.one.arrow_type != 0) {
      act_error_ctxt (stderr);
      fatal_error ("tree { } block cannot use => or #> arrows");
    }
    l = l->next;
  }

  l = prs->u.l.p;
  while (l) {
    for (int i=0; i < _pending_group.size(); i++) {
      auto &g = _pending_group[i];
      // check if my variable appears in the pending group
      if (g.has_var (l->u.one.id) != -1) {
	act_error_ctxt (stderr);
	fprintf (stderr, "ERROR: Multiple tree { } blocks have the same variable");
	fprintf (stderr, "      ID: ");
	l->u.one.id->Print (stderr);
	fprintf (stderr, "\n");
	exit (1);
      }
    }
    l = l->next;
  }

  pending_group g;
  g.tree = prs;
  l = prs->u.l.p;
  while (l) {
    int pos = g.has_var (l->u.one.id);
    if (pos != -1) {
      // dir = 0 (-), flag = 1
      // dir = 1 (+), flag = 2
      g.add_var (l->u.one.id, l->u.one.dir ? 2 : 1);
    }
    l = l->next;
  }

  /* now check the pending production rules to see if we have a match */
  bool flag = false;
  for (int i=0; i < _pending_prs.size(); i++) {
    auto *x = _pending_prs[i];
    Assert (x->type == ACT_PRS_RULE, "What?");
    if (x->u.one.label) {
      continue;
    }
    int pos = g.has_var (x->u.one.id);
    if (pos != -1) {
      if ((g.get_dir (pos) & (x->u.one.dir ? 2 : 1)) != 0) {
	act_error_ctxt (stderr);
	fprintf (stderr, "ERROR: Production rule is both within and outside tree { } block, or\nhas multiple rules outside tree block.\n");
	fprintf (stderr,"     ID: ");
	x->u.one.id->Print (stderr);
	fprintf (stderr, "\n");
	exit (1);
      }
      /* merge into the group! */
      g.add_dir (pos, x->u.one.dir ? 2 : 1);
      g._pending.push_back (x);
      _pending_prs[i] = NULL;
      flag = true;
    }
  }
  if (flag) {
    /* strip out the pending ones now! */
    int i, j;
    i = 0;
    j = 0;
    for (i = 0; i < _pending_prs.size(); i++) {
      if (_pending_prs[i] && j < i) {
	_pending_prs[j] = _pending_prs[i];
	j++;
      }
    }
    while (j != _pending_prs.size()) {
      _pending_prs.pop_back ();
    }
  }

  if (g.complete()) {
    flush_group (sc, &g);
  }
  else {
    _pending_group.push_back (g);
  }
}


/*
 * For associative operations &, | in the production rule, we should
 * make sure that the expression tree is in a canonical form when
 * comparing the two. Otherwise (a & b) & c won't match a & (b & c).
 *
 */
static  act_prs_expr_t *_left_canonicalize (act_prs_expr_t *p)
{
  if (!p) {
    return p;
  }
  if (p->type == ACT_PRS_EXPR_NOT) {
    p->u.e.l = _left_canonicalize (p->u.e.l);
    return p;
  }
  if (p->type == ACT_PRS_EXPR_AND || p->type == ACT_PRS_EXPR_OR) {
    p->u.e.l = _left_canonicalize (p->u.e.l);
    if (p->type == p->u.e.r->type) {
      act_prs_expr_t *x, *y;
      x = p->u.e.r;
      p->u.e.r = x->u.e.l;
      x->u.e.l = p;
      p = x;
      return _left_canonicalize (p);
    }
    else {
      p->u.e.r = _left_canonicalize (p->u.e.r);
      return p;
    }
  }
  return p;
}

/*
  Helper function for walking through the production rule block
*/
void ActCellPass::collect_gates (Scope *sc, act_prs_lang_t **pprs)
{
#define SNIP_RULE  
  act_prs_lang_t *prs = *pprs;
  act_prs_lang_t *prev = NULL;
  while (prs) {
    switch (ACT_PRS_LANG_TYPE (prs->type)) {
    case ACT_PRS_RULE:
      prs->u.one.e = _left_canonicalize (prs->u.one.e);
#ifdef SNIP_RULE
      /* snip rule */
      if (prev) {
	prev->next = prs->next;
      }
#endif      
      _collect_one_prs (sc, prs);
      break;
    case ACT_PRS_GATE:
#ifdef SNIP_RULE
      if (prev) {
	prev->next = prs->next;
      }
      _collect_one_passgate (sc, prs);
#else
      /* preserve pass gates */
      if (!prev) {
	*pprs = prs;
      }
      prev = prs;
#endif      
      break;

    case ACT_PRS_DEVICE:
#ifdef SNIP_RULE
      if (prev) {
	prev->next = prs->next;
      }
      _collect_one_cap (sc, prs);
#else
      /* preserve caps */
      if (!prev) {
	*pprs = prs;
      }
      prev = prs;
#endif      
      break;

      
    case ACT_PRS_TREE:
#ifdef SNIP_RULE
      if (prev) {
	prev->next = prs->next;
      }
#endif      
      /* collect all the gates as a single gate */
      if (prs->u.l.lo) {
	Assert (expr_is_a_const (prs->u.l.lo), "Hmm...");
	Assert (prs->u.l.lo->type == E_INT, "Hmmmm");
      }
      _collect_treegroup (sc, prs);
      break;
    case ACT_PRS_SUBCKT:
      // collect all prs in this subcircuit as a single group
      _create_new_cell (sc, prs->u.l.p);
      break;
    case ACT_PRS_LOOP:
      fatal_error ("Should have been expanded");
      break;
    }
    prs = prs->next;
  }
#ifdef SNIP_RULE
  if (!prev) {
    *pprs = NULL;
  }
#endif  
}



/*
  add_cells: -1 if not to be added, otherwise starting id of newcells 
*/
void ActCellPass::prs_to_cells (Process *p)
{
  Scope *sc;
  act_languages *lang;

  sc = p ? p->CurScope() : ActNamespace::Global()->CurScope();
  if (p) {
    if (p->isCell() && p->isLeaf()) {
      _add_used_cell (_used_cells, p);
      /* nothing to do here */
      return;
    }
  }

  proc_inst_count = 0;
  _pending_prs.clear ();
  _pending_group.clear ();

  lang = (p ? p->getlang() : ActNamespace::Global()->getlang());
  Assert (lang, "What?");
  act_prs *prs = lang->getprs();

  _leak_flag = 0;
  while (prs) {
    if (prs->leak_adjust) {
      _leak_flag = 1;
    }
    prs = prs->next;
  }

  prs = lang->getprs();
  
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
    collect_gates (sc, &prs->p);
    prs = prs->next;
  }
  flush_pending (sc);
  for (int i=0; i < _pending_group.size(); i++) {
    if (_pending_group[i].pending()) {
      flush_group (sc, &_pending_group[i]);
    }
  }

  prs = lang->getprs();
  act_prs *prevprs = NULL;

  /* remove empty prs blocks */
  while (prs) {
    if (prs->p) {
      prevprs = prs;
    }
    else {
      if (!prevprs) {
	lang->setprs (prs->next);
      }
      else {
	prevprs->next = prs->next;
      }
    }
    prs = prs->next;
  }
}


/*
 * collect all cells into the cell table, and return the max cell number
 */
int ActCellPass::_collect_cells (ActNamespace *cells)
{
  ActTypeiter it(cell_ns);
  chash_bucket_t *b;
  struct act_prsinfo *pi;
  int num_cells = 0;

  /*
    If cells are not expanded, then expand them!
  */
  A_DECL (const char *, xcell);
  A_INIT (xcell);

  /* -- collect potential cells -- */
  for (it = it.begin(); it != it.end(); it++) {
    Type *u = (*it);
    Process *p = dynamic_cast<Process *>(u);
    if (!p) continue;

    if (!p->isCell()) {
      continue;
    }
    if ((!p->isExpanded()) && (p->getNumParams() == 0)) {
      A_NEW (xcell, const char *);
      A_NEXT (xcell) = p->getName();
      A_INC (xcell);
    }
  }

  /* -- expand potential cells if they don't exist -- */
  for (int i=0; i < A_LEN (xcell); i++) {
    char buf[10240];
    snprintf (buf, 10240, "%s<>", xcell[i]);
    Type *u = cell_ns->findType (xcell[i]);
    Assert (u, "What?");
    Process *p = dynamic_cast<Process *>(u);
    Assert (p, "What?");
    if (!cells->findType (buf)) {
      /* create expanded version */
      p->Expand (cells, cells->CurScope(), 0, NULL);
    }
  }
  A_FREE (xcell);


  /* 
     Collect production rules from expanded cells 
  */
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
       5. pass gate cells are t0,t1,n0,n1,p0,p1
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
      if (p->getNumPorts() != 2 && p->getNumPorts() != 1) {
	fatal_error ("Cell `%s::%s': More than two ports",
		     cell_ns->getName(), p->getName());
      }
      if (p->getNumPorts() == 2) {
	if ((strcmp (p->getPortName (0), _inport_name) != 0) ||
	    (strcmp (p->getPortName (1), _outport_name) != 0)) {
	  fatal_error ("Cell `%s::%s': Ports should be in/out",
		       cell_ns->getName(), p->getName());
	}
      }
      else if (p->getNumPorts() == 1) {
	if (strcmp (p->getPortName (0), _outport_name) != 0) {
	  fatal_error ("Cell `%s::%s': Ports should be in/out",
		       cell_ns->getName(), p->getName());
	}
      }
      InstType *in_t, *out_t;

      if (p->getNumPorts() == 2) {
	in_t = p->getPortType (0);
	out_t = p->getPortType (1);
      }
      else {
	in_t = NULL;
	out_t = p->getPortType (0);
      }

#if 0      
      int id, version;
      if (sscanf (p->getName()+1, "%dx%d", &id, &version) == 2) {
	/* for cells numbered g <num> x <num>, we track cellmax */
	cellmax = (cellmax > id) ? cellmax : id;
      }
#endif
      num_cells++;
      
      /* in_t must be a bool array or bool
	 out_t must be a bool array or bool
      */
      if ((in_t && !TypeFactory::isBoolType (in_t)) || !TypeFactory::isBoolType (out_t)) {
	fatal_error ("Cell `%s::%s': Port base types must `bool'",
		     cell_ns->getName(), p->getName());
      }

      /* sanity check prs */
      act_prs *prs = p->getprs();
      if (prs && prs->next) {
	fatal_error ("Cell `%s::%s': More than one prs body",
		     cell_ns->getName(), p->getName());
      }
      act_prs_lang_t *l;
      l = NULL;
      if (prs) {
	int is_tree = 0;
	act_prs_lang_t *lpush = NULL;
	l = prs->p;
	while (l) {
	  Assert (l->type != ACT_PRS_LOOP, "Expanded?!");
	  if (l->type == ACT_PRS_GATE || l->type == ACT_PRS_SUBCKT ||
	      ACT_PRS_LANG_TYPE (l->type) == ACT_PRS_DEVICE) {
	    fatal_error ("Cell `%s::%s': no fets/devs/subckts allowed",
			 cell_ns->getName(), p->getName());
	  }
	  if (l->type == ACT_PRS_TREE && is_tree) {
	    fatal_error ("Cell `%s::%s': only one tree allowed",
			 cell_ns->getName(), p->getName());
	  }
	  if (l->type == ACT_PRS_TREE) {
	    is_tree = 1;
	    lpush = l->next;
	    l = l->u.l.p;
	  }
	  l = l->next;
	  if (!l) {
	    l = lpush;
	    lpush = NULL;
	  }
	}
	l = prs->p;
      }
      
      
      /* fine. now dump into celldb */
      pi = _gen_prs_attributes (l,
				in_t ?
				(in_t->arrayInfo() ?
				 in_t->arrayInfo()->size() : 1) : 0,
				out_t->arrayInfo() ?
				out_t->arrayInfo()->size() : 1);
      pi->set_leak_flag (prs->leak_adjust);

#if 0
      printf ("CELL: %s\n", p->getName());
      _dump_prsinfo (pi, "in", "out");
#endif
      {
	char *buf;
	int cut;
        buf = Strdup (p->getName());
	cut = strlen (buf) - 2;
	buf[cut] = '\0';
	UserDef *u = cells->findType (buf);
	Assert (u, "Hmm");
	pi->cell = dynamic_cast<Process *>(u);
	Assert (pi->cell, "Hmm...");
        FREE (buf);
      }
      
      if (out_t->arrayInfo()) {
         if (out_t->arrayInfo()->size() != pi->nout) {
            fatal_error ("Cell `%s::%s': inconsistent outputs",
			 cell_ns->getName(), p->getName());
         }
      }
      else {
         if (pi->nout != 1) {
            fatal_error ("Cell `%s::%s': inconsistent outputs",
			 cell_ns->getName(), p->getName());
         }
      }
#if 0
      if (is_tree) {
	if (treeval) {
	  Assert (treeval->type == E_INT, "Hmm");
	  pi->set_tree_info (treeval->u.ival.v);
	  Assert (pi->get_tree_info() > 0, "tree<> parameter has to be > 0!");
	}
	else {
	  pi->set_tree_info (0);
	}
      }
#endif      

      if (A_LEN (pi->up) == 0 && A_LEN (pi->dn) == 0) {
	act_error_ctxt (stderr);
	warning("Cell `%s' has no production rules?", p->getName());
      }

      // XXX: HERE!!!
      b = chash_lookup (cell_table, pi);
      if (b) {
	act_error_ctxt (stderr);
	warning("Cell `%s' is a duplicate?", p->getName());
#if 0	
	printf ("-- cur cell --\n");
	_dump_prsinfo (pi, "in", "out");
	
	pi = (struct act_prsinfo *)b->v;
	warning ("Previous cell: `%s'", pi->cell->getName());
	
	printf ("-- old cell --\n");
	_dump_prsinfo (pi, "in", "out");
#endif	
      }
      else {
	b = chash_add (cell_table, pi);
#if 0	
	_dump_prsinfo (pi);
#endif	
	b->v = pi;
      }
      if (pi->match_perm) {
	FREE (pi->match_perm);
	pi->match_perm = NULL;
      }
    }
  }
  return num_cells;
}


/*------------------------------------------------------------------------
 *
 * Standard pass API functions
 *
 *------------------------------------------------------------------------
 */

void *ActCellPass::local_op (Process *p, int mode)
{
  prs_to_cells (p);
  return NULL;
}


void ActCellPass::free_local (void *v)
{
  return;
}


int ActCellPass::run (Process *p)
{
  int ret = ActPass::run (p);

  /* 
     We've changed the netlist: all passes need to be re-computed!
  */
  ActPass::refreshAll (a, p);

  return ret;
}


void ActCellPass::Print (FILE *fp)
{
  if (!completed()) {
    warning ("ActCellPass::Print() called without pass being run");
    return;
  }
  if (!cell_table) {
    return;
  }
  dump_celldb (fp);
}



/*
 * Create cell pass, hash tables for cell matching, and the cell
 * namespace if it doesn't already exist.
 *
 * Sizing must be called before this pass, since cells should
 * correspond to finalized sizes for layout purposes.
 *
 */
ActCellPass::ActCellPass (Act *a) : ActPass (a, "prs2cells")
{
  cell_table = NULL;
  cell_ns = NULL;
  proc_inst_count = 0;
  cell_count = 0;
  _new_cells = list_new ();
  _used_cells = list_new ();

  /*-- disables propagation of updates --*/
  disableUpdate ();

  if (!a->pass_find ("sizing")) {
    ActSizingPass *sp = new ActSizingPass (a);
  }
  AddDependency ("sizing");

  /*-- create hash table for cell matching --*/
  cell_table = chash_new (32);
  cell_table->hash = cell_hashfn;
  cell_table->match = cell_matchfn;
  cell_table->dup = cell_dupfn;
  cell_table->free = cell_freefn;
  cell_table->print = NULL;

  /*-- initialize map table for ids --*/
  current_idmap.clear ();

  config_set_default_string ("net.cell_namespace", "cell");
  config_set_default_string ("net.cell_inport", "in");
  config_set_default_string ("net.cell_outport", "out");
    
  cell_ns = a->findNamespace (config_get_string ("net.cell_namespace"));
  _inport_name = config_get_string ("net.cell_inport");
  _outport_name = config_get_string ("net.cell_outport");
  
  if (!cell_ns) {
    /*-- create empty namespace --*/
    cell_ns = new ActNamespace (ActNamespace::Global(),
				config_get_string ("net.cell_namespace"));
    cell_ns->Expand ();
    cell_count = 0;
  }
  else {
    /*-- put existing cells into the hash table for matching --*/
    cell_count =  _collect_cells (cell_ns);
  }

  /*-- add the pass gates and cap templated values into the cell space --*/
  add_passgates_cap ();
}


ActCellPass::~ActCellPass ()
{
  if (cell_table) {
    chash_free (cell_table);
  }
  list_free (_new_cells);
  list_free (_used_cells);
}


/*
 * Add a number of standard default cells into the cell space
 *   t0 = transmission gate with template w,l parameters
 *   t1 = transmission gate
 *   p0 = pass p-fet with w,l parms
 *   p1 = pass p-fet
 *   n0 = pass n-fet with w,l parms
 *   n1 = pass n-fet
 *   c0 = cap with template w,l parameters
 *   c1 = cap
 *   ... if there are more devices, they would be numbered c2, c3, etc.
 *
 */
void ActCellPass::add_passgates_cap ()
{
  int i;
  int max;
  const char *g[] = { "t0", "t1", "n0", "n1", "p0", "p1" };

  Assert (cell_ns, "What?");

  if (cell_ns->findType (g[0])) {
    return;
  }

  max = 6 + config_get_table_size ("act.prs_device")*2;

  char gendev[12];

  for (i=0; i < max; i++) {
    /* add the unexpanded process to the cell namespace, and then
       expand it! */
    Process *proc;

    /*-- create a new process in the namespace --*/
    UserDef *u = new UserDef (cell_ns);

    // the even ones have parameters
    if ((i % 2) == 0) {
      InstType *xit;
      xit = TypeFactory::Factory()->NewPInt();
      u->AddMetaParam (xit, "w", NULL);
      u->AddMetaParam (xit, "l", NULL);
    }
    
    proc = new Process (u);
    delete u;
    proc->MkCell ();
    proc->MkExported ();

    if (i < 6) {
      snprintf (gendev, 12, "%s", g[i]);
    }
    else {
      snprintf (gendev, 12, "c%d", i - 6);
    }

    Assert (cell_ns->findName (gendev) == 0, "Name conflict?");
    cell_ns->CreateType (gendev, proc);

    /*-- add ports --*/
    InstType *it = TypeFactory::Factory()->NewBool ((i >= 6) ? 
						    Type::NONE : Type::IN);

    Expr *arr;
    if (gendev[0] == 't') {
      arr = const_expr (3);
    }
    else if (gendev[0] == 'p' || gendev[0] == 'n') {
      arr = const_expr (2);
    }
    else {
      arr = NULL;
    }
    if (arr) {
      Array *ta;
      it = new InstType (it);
      ta = new Array (arr);
      ta->mkArray ();
      it->MkArray (ta);
    }

    Assert (proc->AddPort (it, _inport_name) == 1, "Error adding in port?");

    it = TypeFactory::Factory()->NewBool (gendev[0] == 'c' ? Type::NONE :
					  Type::OUT);
    Assert (proc->AddPort (it, _outport_name) == 1, "Error adding out port?");

    /*-- prs body --*/
    act_prs *prs_body;
    NEW (prs_body, act_prs);
    prs_body->vdd = NULL;
    prs_body->gnd = NULL;
    prs_body->psc = NULL;
    prs_body->nsc = NULL;
    prs_body->next = NULL;
    prs_body->leak_adjust = 0;

    act_prs_lang_t *rules = NULL;

    NEW (rules, act_prs_lang_t);
    rules->next = NULL;
    if (gendev[0] != 'c') {
      rules->type = ACT_PRS_GATE;
    }
    else {
      rules->type = ACT_PRS_DEVICE + (i - 6)/2;
    }

    rules->u.p.sz = NULL;
    rules->u.p.g = NULL;
    rules->u.p._g = NULL;
    rules->u.p.s = NULL;
    rules->u.p.d = NULL;
    rules->u.p.attr = NULL;

    int j = 0;

    if (gendev[0] != 'c') {
      if (gendev[0] != 'p') {
	rules->u.p.g = new ActId (_inport_name, new Array (const_expr (j)));
	j++;
      }
      else {
	rules->u.p.g = NULL;
      }
      if (gendev[0] != 'n') {
	rules->u.p._g = new ActId (_inport_name, new Array (const_expr (j)));
	j++;
      }
      else {
	rules->u.p._g = NULL;
      }
    }
    else {
      rules->u.p._g = NULL;
      rules->u.p.g = NULL;
    }
    if (gendev[0] != 'c') {
      rules->u.p.s = new ActId (_inport_name, new Array (const_expr (j)));
      j++;
    }
    else {
      rules->u.p.s = new ActId (_inport_name);
    }
    rules->u.p.d = new ActId (_outport_name);

    if ((i % 2) == 0) {
      act_size_spec_t *sz;
      NEW (sz, act_size_spec_t);
      sz->flavor = 0;
      sz->folds = NULL;
      sz->w = _id_to_expr (new ActId ("w"));
      sz->l = _id_to_expr (new ActId ("l"));
      rules->u.p.sz = sz;
    }
    prs_body->p = rules;
    proc->AppendBody (new ActBody_Lang  (-1, prs_body));
    proc->MkDefined ();
  }
}
