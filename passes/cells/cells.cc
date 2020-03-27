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
#include <config.h>

struct act_varinfo {
  int nup, ndn;			// # of times in up and down guards
  int *depths;			// depth of each instance (nup + ndn)
  int cup, cdn;
				// size), 0..nup-1 followed by ndn
  unsigned char tree;		// 0 = not in tree, 1 in tree
};

struct act_prsinfo {
  Process *cell;		/* the cell; NULL until it is
				   created. */

  int nvars;			/* # of variables. includes @-labels */

  int nout;			/* # of outputs 
				   Variable convention:
				   0, 1, ..., nout-1 are outputs.
				   nout, ... nvars-1 are inputs
				*/

  act_attr_t **nattr;		/* 2 per output: pull-up and pull-down */
  
  int nat;                     /* # of labels */

  int *at_perm; 		/* permutation of ATs */

  /* variable attributes */
  int *attr_map;
  A_DECL (struct act_varinfo, attrib);

  int tval;			 /* for tree<>; -1 = none, 0 = mgn,
				    otherwise tree  */

  /* XXX: need attributes from the production rules */
  
  A_DECL (act_prs_expr_t *, up); /* pull-up */
  A_DECL (act_prs_expr_t *, dn); /* pull-down */
      /* NOTE: all actid pointers are actually just simple integers */
      /* The # of these will be nout + any internal labels */

  int *match_perm;		// used to report match!
  
};

static void _dump_prsinfo (struct act_prsinfo *p);
static act_prs_expr_t *_convert_prsexpr_to_act (act_prs_expr_t *e,
						struct act_prsinfo *pi);


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


/*
  non-zero if equal, 0 otherwise

  flip = 0, compare a to b
  flip = 1, compare a to NOT b

  chk_width = 1 : check widths of gates
*/
static int _equal_expr (act_prs_expr_t *a, act_prs_expr_t *b,
			int *perm, int flip, int chk_width)
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
      return _equal_expr (a->u.e.l, b, perm, 1, chk_width);
    }
    else if (b->type == ACT_PRS_EXPR_NOT) {
      return _equal_expr (a, b->u.e.l, perm, 1, chk_width);
    }
    return 0;
  }
  if (flip && (a->type != btype)) return 0;
  
  switch (a->type) {
  case ACT_PRS_EXPR_AND:
    return _equal_expr (a->u.e.l, b->u.e.l, perm, flip, chk_width) &&
      _equal_expr (a->u.e.r, b->u.e.r, perm, flip, chk_width) &&
      _equal_expr (a->u.e.pchg, b->u.e.pchg, perm, 0, chk_width) &&
      (a->u.e.pchg ? (a->u.e.pchg_type == b->u.e.pchg_type) : 1);
    break;

  case ACT_PRS_EXPR_OR:
    return ((_equal_expr (a->u.e.l, b->u.e.l, perm, flip, chk_width) &&
	     _equal_expr (a->u.e.r, b->u.e.r, perm, flip, chk_width)) ||
	    (_equal_expr (a->u.e.l, b->u.e.r, perm, flip, chk_width) &&
	     _equal_expr (a->u.e.r, b->u.e.l, perm, flip, chk_width))) &&
      _equal_expr (a->u.e.pchg, b->u.e.pchg, perm, 0, chk_width) &&
      (a->u.e.pchg ? (a->u.e.pchg_type == b->u.e.pchg_type) : 1);
    break;
    
  case ACT_PRS_EXPR_NOT:
    return _equal_expr (a->u.e.l, b->u.e.l, perm, flip, chk_width);
    break;

  case ACT_PRS_EXPR_VAR:
    bid = (unsigned long long)b->u.v.id;
    if (perm) bid = perm[bid];
    //if (!a->u.v.id->isEqual (b->u.v.id)) return 0;
    if ((unsigned long long)a->u.v.id != bid) return 0;
    if (chk_width) {
      if (a->u.v.sz && !b->u.v.sz) return 0;
      if (!a->u.v.sz && b->u.v.sz) return 0;
      if (a->u.v.sz && b->u.v.sz) {
	if (a->u.v.sz->flavor != b->u.v.sz->flavor) return 0;
	if (!expr_equal (a->u.v.sz->w, b->u.v.sz->w)) return 0;
	if (!expr_equal (a->u.v.sz->l, b->u.v.sz->l)) return 0;
	if (!expr_equal (a->u.v.sz->folds, b->u.v.sz->folds)) return 0;
      }
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

static int basic_match (struct act_prsinfo *k1, struct act_prsinfo *k2)
{
  if (k1->nvars != k2->nvars) return 0;
  if (k1->nout != k2->nout) return 0;
  if (k1->tval != k2->tval) return 0;
  if (k1->nat != k2->nat) return 0;

  if (A_LEN (k1->up) != A_LEN (k2->up)) return 0;
  if (A_LEN (k1->dn) != A_LEN (k2->dn)) return 0;
  if (A_LEN (k1->attrib) != A_LEN (k2->attrib)) return 0;

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

  for (i=0; i < A_LEN (k1->attrib); i++) {
    if (k1->attrib[i].tree != k2->attrib[i].tree) return 0;
    if (k1->attrib[i].nup != k2->attrib[i].nup) return 0;
    if (k1->attrib[i].ndn != k2->attrib[i].ndn) return 0;
    for (int j=0; j < k1->attrib[i].nup + k1->attrib[i].ndn; j++) {
      if (k1->attrib[i].depths[j] != k2->attrib[i].depths[j]) return 0;
    }
  }

#if 0
  printf ("comparing:\n");
  _dump_prsinfo (k1);
  _dump_prsinfo (k2);
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
  printf ("===\n");
#endif  
  
  /* 
     perm is now the default map from k1 to k2 

     We also need to sub-divide the range to contiguous sections where
     the attribute values match; we need to try all permutations of
     let's say i.
  */
  for (i=0; i < A_LEN (k1->up); i++) {
    if (!_equal_expr (k1->up[i], k2->up[perm[i]], perm, 0, chk_width)) {
#if 0      
      printf ("up[%d] doesn't match\n", i);
#endif      
      FREE (perm);
      return 0;
    }
  }
  for (i=0; i < A_LEN (k1->dn); i++) {
    if (!_equal_expr (k1->dn[i], k2->dn[perm[i]], perm, 0, chk_width)) {
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

  if (!basic_match (k1, k2)) return 0;
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


L_A_DECL (act_prs_lang_t *, pendingprs);


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

static void _mark_at_used (act_prs_expr_t *e, bitset_t *b,
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
		  (char *)pendingprs[idx_array[i]]->u.one.id) == 0) {
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

static void _mark_at_used2 (act_prs_expr_t *e, bitset_t *b,
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

void ActCellPass::flush_pending (Process *p)
{
  int pending_count = 0;

  if (A_LEN (pendingprs) == 0) {
    A_FREE (pendingprs);
    A_INIT (pendingprs);
    return;
  }

  /* -- handle shared gates -- */
  int *at_idx;
  MALLOC (at_idx, int, A_LEN (pendingprs));
  int at_len = 0;

  for (int i=0; i < A_LEN (pendingprs); i++) {
    if (pendingprs[i]->u.one.label) {
      at_idx[at_len] = i;
      at_len++;
    }
  }

  bitset_t **at_use = NULL;
  int *grouped = NULL;

  if (at_len > 0) {
    /* there are some labels */

    /* figure out which pending rules use them */
    MALLOC (at_use, bitset_t *, A_LEN (pendingprs));
    MALLOC (grouped, int, A_LEN (pendingprs));

#if 0
    printf ("--\n");
#endif    
    for (int i=0; i < A_LEN (pendingprs); i++) {
      grouped[i] = 0;
      at_use[i] = bitset_new (at_len);
      _mark_at_used (pendingprs[i]->u.one.e, at_use[i], at_idx, at_len);
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

  A_DECL (act_prs_lang_t, groupprs);
  A_INIT (groupprs);

  bitset_t *at_group = NULL;
  bitset_t *at_tmp = NULL;
  if (at_len > 0) {
    at_group = bitset_new (at_len);
    at_tmp = bitset_new (at_len);
  }
  
  for (int i=0; i < A_LEN (pendingprs); i++) {
    struct act_prsinfo *pi;
    chash_bucket_t *b;

    if (at_len > 0 && (grouped[i] == 2)) {
      /* skip, since it has been grouped already */
      continue;
    }

    A_NEWM (groupprs, act_prs_lang_t);
    A_NEXT (groupprs) = *(pendingprs[i]);
    A_NEXT (groupprs).next = NULL;
    A_INC (groupprs);

    if (at_len == 0 ||
	(bitset_isclear (at_use[i]) && !pendingprs[i]->u.one.label)) {
      /* nothing to do! */
    }
    else {
      /* Some @ rule is used, or we are at an @ rule */
      grouped[i] = 1;
      bitset_or (at_group, at_use[i]);
      if (pendingprs[i]->u.one.label) {
	int k;
	for (k=0; k < at_len; k++) {
	  if (at_idx[k] == i) {
	    bitset_set (at_group, i);
	    break;
	  }
	}
	Assert (k != at_len, "What?");
      }

      do {
	bitset_clear (at_tmp);
	bitset_or (at_tmp, at_group);
	for (int j=i+1; j < A_LEN (pendingprs); j++) {
	  if (!bitset_andclear (at_group, at_use[j])) {
	    /* there is an AT in common! */
	    bitset_or (at_group, at_use[j]);
	    grouped[j] = 1;
	  }
	  if (pendingprs[j]->u.one.label) {
	    for (int k=0; k < at_len; k++) {
	      if (at_idx[k] == j) {
		grouped[j] = 1;
		bitset_set (at_group, j);
		bitset_or (at_group, at_use[j]);
		break;
	      }
	    }
	  }
	}
      } while (!bitset_equal (at_tmp, at_group));

      // now we have all the @s we need, and they are flagged with the
      // grouped bit

#if 0
      printf ("Grouped: ");
#endif
      
      for (int j=i; j < A_LEN (pendingprs); j++) {
#if 0
	if (grouped[j] == 1) {
	  printf (" %d", j);
	}
#endif	
	if (grouped[j] == 1 && !pendingprs[j]->u.one.label) {
	  for (int k=j+1; k < A_LEN (pendingprs); k++) {
	    if ((pendingprs[j]->u.one.id == pendingprs[k]->u.one.id) ||
		pendingprs[j]->u.one.id->isEqual (pendingprs[k]->u.one.id)) {
	      grouped[k] = 1;
	      break;
	    }
	  }
	}
      }
#if 0
      printf ("\n");
#endif      

      for (int j=i+1; j < A_LEN (pendingprs); j++) {
	if (grouped[j] == 1) {
	  A_NEWM (groupprs, act_prs_lang_t);
	  A_NEXT (groupprs) = *(pendingprs[j]);
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
      } while (p->CurScope()->Lookup (buf));

      Assert (p->CurScope()->isExpanded(), "Hmm");

      InstType *it = new InstType (p->CurScope(), pi->cell, 0);
      it = it->Expand (NULL, p->CurScope());

      Assert (it->isExpanded(), "Hmm");
    
      Assert (p->CurScope()->Add (buf, it), "What?");
      /*--- now make connections --*/
    
      ActBody_Conn *ac;

      //printf (" --- [%s] \n", p->getName());
      ac = _build_connections (buf, pi);
      //ac->Print (stdout);
      //ac->Next()->Print (stdout);
      //printf (" --- \n");

      int oval = Act::warn_double_expand;
      Act::warn_double_expand = 0;
      ac->Expandlist (NULL, p->CurScope ());
      Act::warn_double_expand = oval;
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
    for (int i=0; i < A_LEN (pendingprs); i++) {
      bitset_free (at_use[i]);
    }
    FREE (at_use);
    FREE (grouped);
  }
  A_FREE (pendingprs);
  A_INIT (pendingprs);
}


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
    ret->u.v.sz = NULL;
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

void ActCellPass::add_passgates ()
{
  int i;
  const char *g[] = { "t0", "t1", "n0", "n1", "p0", "p1" };

  Assert (cell_ns, "What?");

  if (cell_ns->findType (g[0])) {
    return;
  }

  for (i=0; i < 6; i++) {
    /* add the unexpanded process to the cell namespace, and then
       expand it! */
    Process *proc;

    /*-- create a new process in the namespace --*/
    UserDef *u = new UserDef (cell_ns);

    if (g[i][1] == '0') {
      InstType *xit;
      xit = TypeFactory::Factory()->NewPInt();
      u->AddMetaParam (xit, "w");
      u->AddMetaParam (xit, "l");
    }
    
    proc = new Process (u);
    delete u;
    proc->MkCell ();
    proc->MkExported ();

    Assert (cell_ns->findName (g[i]) == 0, "Name conflict?");
    cell_ns->CreateType (g[i], proc);

    /*-- add ports --*/
    InstType *it = TypeFactory::Factory()->NewBool (Type::IN);

    Expr *arr;
    if (g[i][0] == 't') {
      arr = const_expr (3);
    }
    else {
      arr = const_expr (2);
    }
    Array *ta;
    it = new InstType (it);
    ta = new Array (arr);
    ta->mkArray ();
    it->MkArray (ta);

    Assert (proc->AddPort (it, "in") == 1, "Error adding in port?");

    it = TypeFactory::Factory()->NewBool (Type::OUT);
    Assert (proc->AddPort (it, "out") == 1, "Error adding out port?");

    /*-- prs body --*/
    act_prs *prs_body;
    NEW (prs_body, act_prs);
    prs_body->vdd = NULL;
    prs_body->gnd = NULL;
    prs_body->psc = NULL;
    prs_body->nsc = NULL;
    prs_body->next = NULL;

    act_prs_lang_t *rules = NULL;

    NEW (rules, act_prs_lang_t);
    rules->next = NULL;
    rules->type = ACT_PRS_GATE;

    rules->u.p.sz = NULL;
    rules->u.p.g = NULL;
    rules->u.p._g = NULL;
    rules->u.p.s = NULL;
    rules->u.p.d = NULL;
    rules->u.p.attr = NULL;

    int j = 0;

    if (g[i][0] != 'p') {
      rules->u.p.g = new ActId ("in", new Array (const_expr (j)));
      j++;
    }
    else {
      rules->u.p.g = NULL;
    }
    if (g[i][0] != 'n') {
      rules->u.p._g = new ActId ("in", new Array (const_expr (j)));
      j++;
    }
    else {
      rules->u.p._g = NULL;
    }
    rules->u.p.s = new ActId ("in", new Array (const_expr (j)));
    j++;
    rules->u.p.d = new ActId ("out");

    if (g[i][1] == '0') {
      act_size_spec_t *sz;
      NEW (sz, act_size_spec_t);
      sz->flavor = 0;
      sz->folds = NULL;
      sz->w = _id_to_expr (new ActId ("w"));
      sz->l = _id_to_expr (new ActId ("l"));
      rules->u.p.sz = sz;
    }
    prs_body->p = rules;
    proc->AppendBody (new ActBody_Lang  (prs_body));
    proc->MkDefined ();
  }
}



void ActCellPass::add_new_cell (struct act_prsinfo *pi)
{
  int i;
  
  char buf[100];
  snprintf (buf, 100, "g%dx0", cell_count++);
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

  Assert (cell_ns->findName (buf) == 0, "Name conflict?");
  cell_ns->CreateType (buf, proc);

  /*-- add ports --*/
  InstType *it = TypeFactory::Factory()->NewBool (Type::IN);
  Expr *arr = const_expr (pi->nvars - pi->nout - pi->nat);
  Array *ta;
  it = new InstType (it);
  ta = new Array (arr);
  ta->mkArray ();
  it->MkArray (ta);

  Assert (proc->AddPort (it, "in") == 1, "Error adding in port?");

  it = TypeFactory::Factory()->NewBool (Type::OUT);
  if (pi->nout > 1) {
    arr = const_expr (pi->nout);
    it = new InstType (it);
    ta = new Array (arr);
    ta->mkArray ();
    it->MkArray (ta);
  }
  Assert (proc->AddPort (it, "out") == 1, "Error adding out port?");

  /*-- prs body --*/
  act_prs *prs_body;
  NEW (prs_body, act_prs);
  prs_body->vdd = NULL;
  prs_body->gnd = NULL;
  prs_body->psc = NULL;
  prs_body->nsc = NULL;
  prs_body->next = NULL;

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
      rules->u.one.eopp = NULL;
      rules->u.one.e = _convert_prsexpr_to_act (pi->up[j], pi);
      if (rules->u.one.label) {
	char buf[10];
	snprintf (buf, 10, "x%d", j-pi->nout);
	rules->u.one.id = (ActId *) Strdup (buf);
      }
      else {
	if (pi->nout == 1) {
	  rules->u.one.id = new ActId ("out");
	}
	else {
	  rules->u.one.id = new ActId ("out", new Array (const_expr (j)));
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
      rules->u.one.eopp = NULL;
      rules->u.one.e = _convert_prsexpr_to_act (pi->dn[j], pi);
      if (rules->u.one.label) {
	char buf[10];
	snprintf (buf, 10, "x%d", j-pi->nout);
	rules->u.one.id = (ActId *) Strdup (buf);
      }
      else {
	if (pi->nout == 1) {
	  rules->u.one.id = new ActId ("out");
	}
	else {
	  rules->u.one.id = new ActId ("out", new Array (const_expr (j)));
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
  proc->AppendBody (new ActBody_Lang  (prs_body));
  proc->MkDefined ();
  pi->cell = proc;
  //proc->Expand (cell_ns, cell_ns->CurScope(), 0, NULL);
}




static act_prs_expr_t *_convert_prsexpr_to_act (act_prs_expr_t *e,
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
    ret->u.e.l = _convert_prsexpr_to_act (e->u.e.l, pi);
    ret->u.e.r = _convert_prsexpr_to_act (e->u.e.r, pi);
    ret->u.e.pchg_type = e->u.e.pchg_type;
    ret->u.e.pchg = _convert_prsexpr_to_act (e->u.e.pchg, pi);
    break;
    
  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = _convert_prsexpr_to_act (e->u.e.l, pi);
    break;

  case ACT_PRS_EXPR_VAR:
    v = (unsigned long)e->u.v.id;
    if (v < pi->nout) {
      if (pi->nout == 1) {
	ret->u.v.id = new ActId ("out");
      }
      else {
	Expr *tmp = const_expr (v);
	ret->u.v.id = new ActId ("out", new Array (tmp));
      }
    }
    else if (v >= (pi->nout + pi->nat)) {
      Expr *tmp = const_expr (v-pi->nout-pi->nat);
      ret->u.v.id = new ActId ("in", new Array (tmp));
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
      char buf[10];
      snprintf (buf, 10, "x%d", v-pi->nout);
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
			act_prs_expr_t *e, struct act_prsinfo *pi, int prec = 0)
{
  int v;

  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
    if (prec > 2) {
      fprintf (fp, "(");
    }
    _dump_expr (fp, e->u.e.l, pi, 2);
    fprintf (fp, " & ");
    _dump_expr (fp, e->u.e.r, pi, 2);
    if (prec > 2) {
      fprintf (fp, ")");
    }
    /* XXX: precharges? */
    break;
    
  case ACT_PRS_EXPR_OR:
    if (prec > 1) {
      fprintf (fp, "(");
    }
    _dump_expr (fp, e->u.e.l, pi, 1);
    fprintf (fp, " | ");
    _dump_expr (fp, e->u.e.r, pi, 1);
    if (prec > 1) {
      fprintf (fp, ")");
    }
    break;

  case ACT_PRS_EXPR_NOT:
    fprintf (fp, "~");
    _dump_expr (fp, e->u.e.l, pi, 3);
    break;

  case ACT_PRS_EXPR_VAR:
    v = (unsigned long)e->u.v.id;
    if (v < pi->nout) {
      if (pi->nout == 1) {
	fprintf (fp, "out");
      }
      else {
	fprintf (fp, "out[%d]", v);
      }
    }
    else if (v >= (pi->nout + pi->nat)) {
      fprintf (fp, "in[%d]", v-pi->nout-pi->nat);
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


/*-- convert prs block into attriburtes used for isomorphism checking --*/
struct act_prsinfo *ActCellPass::_gen_prs_attributes (act_prs_lang_t *prs)
{
  struct act_prsinfo *ret;
  act_prs_lang_t *l, *lpush;
  struct idmap imap;
  int i;
  int in_tree;

  NEW (ret, struct act_prsinfo);
  
  A_FREE (current_idmap.ids);
  A_INIT (current_idmap.ids);

  ret->cell = NULL;
  ret->nvars = 0;
  ret->nout = 0;
  ret->nat = 0;
  ret->tval = -1;
  ret->match_perm = NULL;
  ret->nattr = NULL;
  ret->at_perm = NULL;

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
  MALLOC (ret->nattr, act_attr_t *, ret->nout*2);
  for (i=0; i < ret->nout*2; i++) {
    ret->nattr[i] = NULL;
  }

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

    if (l->u.one.attr) {
      if (i >= ret->nout) {
	fatal_error ("Attributes on labels? This is not supported");
      }
      ret->nattr[2*i+(l->u.one.dir ? 1 : 0)] = l->u.one.attr;
    }
    
    if (l->u.one.arrow_type == 1) {
      xx = _complement_rule (x);
    }
    else if (l->u.one.arrow_type == 2) {
      xx = _celement_rule (x);
    }

    /* dn first, then up */
    if (!l->u.one.dir) {
      _add_rule (&ret->dn[i], x);
      if (l->u.one.arrow_type != 0) {
	_add_rule (&ret->up[i], xx);
      }
    }
    else {
      if (l->u.one.arrow_type != 0) {
	_add_rule (&ret->dn[i], xx);
      }
      _add_rule (&ret->up[i], x);
    }
#if 0
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
#endif
    
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
  printf ("  nvars=%d, nout=%d, nat=%d, tval=%d\n",
	  p->nvars, p->nout, p->nat, p->tval);

  for (int i=0; i < A_LEN (p->up); i++) {
    printf (" %d: up = ", i);
    if (!p->up[i]) { printf ("n/a"); }
    else { _dump_expr (stdout, p->up[i], p); }
    printf (" ; dn = ");
    if (!p->dn[i]) { printf ("n/a"); }
    else { _dump_expr (stdout, p->dn[i], p); }
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
  printf ("perm: ");
  for (int i=0; i < A_LEN (p->attrib); i++) {
    printf (" %d", p->attr_map[i]);
  }
  printf ("\n");
  printf ("-------\n");
}

static void _dump_prs_cell (FILE *fp, struct act_prsinfo *p, const char *name)
{
  const char *s = name;
  fprintf (fp, "export defcell ");
  while (*s && *s != '<') {
    fputc (*s, fp);
    s++;
  }
  fprintf (fp, " (");
  fprintf (fp, "bool? in[%d]; bool! out", p->nvars - p->nout - p->nat);
  if (p->nout > 1) {
    fprintf (fp, "[%d]", p->nout);
  }
  fprintf (fp, ")\n{\n   prs {\n");

  int idx = p->nout-1;
  for (int ii=0; ii < A_LEN (p->up); ii++) {
    int i;
    idx = (idx + 1) % A_LEN (p->up);
    if ((idx >= p->nout) && (p->at_perm)) {
      i = p->nout + p->at_perm[idx-p->nout];
    }
    else {
      i = idx;
    }
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

      _dump_expr (fp, p->dn[i], p);
      fprintf (fp, " -> ");
      if (i < p->nout) {
	if (p->nout == 1) {
	  fprintf (fp, "out-");
	}
	else {
	  fprintf (fp, "out[%d]-", i);
	}
      }
      else {
	fprintf (fp, "@x%d-", i-p->nout);
      }
      fprintf (fp, "\n");
    }

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
      
      _dump_expr (fp, p->up[i], p);
      fprintf (fp, " -> ");
      if (i < p->nout) {
	if (p->nout == 1) {
	  fprintf (fp, "out+");
	}
	else {
	  fprintf (fp, "out[%d]+", i);
	}
      }
      else {
	fprintf (fp, "@x%d+", i-p->nout);
      }
      fprintf (fp, "\n");
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
  struct act_prsinfo *pi;
  int cellmax = 0;
  int id, version;
  A_DECL (struct cell_name *, cells);
  A_INIT (cells);

  if (!cell_table) return;

  for (i=0; i < cell_table->size; i++) {
    for (b = cell_table->head[i]; b; b = b->next) {
      pi = (struct act_prsinfo *)b->v;
      if (pi->cell) {
	sscanf (pi->cell->getName()+1, "%dx%d", &id, &version);
	cellmax = (cellmax > id) ? cellmax : id;
      }
    }
  }
  cellmax++;

  for (i=0; i < cell_table->size; i++) {
    for (b = cell_table->head[i]; b; b = b->next) {
      pi = (struct act_prsinfo *)b->v;
      A_NEW (cells, struct cell_name *);
      NEW (A_NEXT (cells), struct cell_name);
      A_NEXT (cells)->p = pi;
      if (pi->cell) {
	A_NEXT (cells)->name = pi->cell->getName();
      }
      else {
	char buf[100];
	snprintf (buf, 100, "g%dx0", cellmax++);
	A_NEXT (cells)->name = Strdup (buf);
      }
      A_INC (cells);
    }
  }

  /* lets sort the cell names */
  mymergesort ((const void **)cells, A_LEN (cells), _cmp_cells);

  fprintf (fp, "namespace cell {\n\n");
  
  /* print */
  for (i=0; i < A_LEN (cells); i++) {
    pi = cells[i]->p;
    _dump_prs_cell (fp, pi, cells[i]->name);
    if (!pi->cell) {
      FREE ((void*)cells[i]->name);
    }
    FREE (cells[i]);
  }
  A_FREE (cells);

  fprintf (fp, "export template<pint w,l> defcell p0(bool? in[2]; bool! out) {\n");
  fprintf (fp, "  prs { passp<w,l> (in[0],in[1],out) }\n}\n\n");

  fprintf (fp, "export template<pint w,l> defcell n0(bool? in[2]; bool! out) {\n");
  fprintf (fp, "  prs { passn<w,l> (in[0],in[1],out) }\n}\n\n");

  fprintf (fp, "export template<pint w,l> defcell t0(bool? in[3]; bool! out) {\n");
  fprintf (fp, "  prs { transgate<w,l> (in[0],in[1],in[2],out) }\n}\n\n");

  fprintf (fp, "export defcell p1(bool? in[2]; bool! out) {\n");
  fprintf (fp, "  prs { passp (in[0],in[1],out) }\n}\n\n");

  fprintf (fp, "export defcell n1(bool? in[2]; bool! out) {\n");
  fprintf (fp, "  prs { passn (in[0],in[1],out) }\n}\n\n");

  fprintf (fp, "export defcell t1(bool? in[3]; bool! out) {\n");
  fprintf (fp, "  prs { transgate (in[0],in[1],in[2],out) }\n}\n\n");

  fprintf (fp, "\n\n}\n");
}


Expr *ActCellPass::_idexpr (int idx, struct act_prsinfo *pi)
{
  if (pi->match_perm) {
    return _id_to_expr (current_idmap.ids[pi->match_perm[idx]]);
  }
  else {
    return _id_to_expr (current_idmap.ids[idx]);
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
  instname->Append (new ActId ("in"));
  
  ActBody_Conn *ac = new ActBody_Conn (instname, ret);

  instname = new ActId (name);
  instname->Append (new ActId ("out"));

  idexpr = _idexpr (0, pi);
  if (pi->nout == 1) {
    ac->Append (new ActBody_Conn (instname, new AExpr (idexpr)));
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
    ac->Append (new ActBody_Conn (instname, ret));
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
  else {
    ret = new AExpr (AExpr::COMMA, new AExpr (_id_to_expr (gate->u.p._g)), NULL);
    a = ret;
    a->SetRight (new AExpr (AExpr::COMMA,
			    new AExpr (_id_to_expr (gate->u.p.s)), NULL));
  }

  instname = new ActId (name);
  instname->Append (new ActId ("in"));
  
  ActBody_Conn *ac = new ActBody_Conn (instname, ret);

  instname = new ActId (name);
  instname->Append (new ActId ("out"));

  ac->Append (new ActBody_Conn (instname, new AExpr (_id_to_expr (gate->u.p.d))));

  return ac;
}


void ActCellPass::_collect_one_prs (Process *p, act_prs_lang_t *prs)
{
  int i;
  struct act_prsinfo *pi;
  act_prs_lang_t newprs, newprs2;
  chash_bucket_t *b;
  
  // add this prs to the list, if it is paired then we can find a gate
  Assert (prs->type == ACT_PRS_RULE, "Hmm.");

  if (prs->u.one.arrow_type != 0) {
    /* we're good, let's do this! */
    newprs = *prs;
    newprs.next = NULL;

    if (_uses_a_label (prs->u.one.e)) {
      fatal_error ("@-expressions can only be used with -> production rules");
    }
  }
  else {
    if (prs->u.one.label) {
      A_NEW (pendingprs, act_prs_lang_t *);
      A_NEXT (pendingprs) = prs;
      A_INC (pendingprs);
      return;
    }
    for (i=0; i < A_LEN (pendingprs); i++) {
      if (prs->u.one.id == pendingprs[i]->u.one.id ||
	  prs->u.one.id->isEqual (pendingprs[i]->u.one.id)) {
	break;
      }
    }
    if (i == A_LEN (pendingprs)) {
      A_NEW (pendingprs, act_prs_lang_t *);
      A_NEXT (pendingprs) = prs;
      A_INC (pendingprs);
      return;
    }
    newprs = *(pendingprs[i]);
    if (prs->u.one.dir == newprs.u.one.dir) {
      /* do something */
      act_prs_expr_t *eor;  /* XXX: NEED TO FREE THESE ALLOCATIONS */
      act_prs_lang_t *tmp;
      NEW (eor, act_prs_expr_t);
      eor->type = ACT_PRS_EXPR_OR;
      eor->u.e.l = prs->u.one.e;
      eor->u.e.r = newprs.u.one.e;
      eor->u.e.pchg = NULL;
      newprs.u.one.e = eor;
      if (prs->u.one.attr && !newprs.u.one.attr) {
	newprs.u.one.attr = prs->u.one.attr;
      }
      NEW (tmp, act_prs_lang_t);
      *tmp = newprs;
      pendingprs[i] = tmp;
      return;
    }

    if (_uses_a_label (prs->u.one.e) || _uses_a_label (newprs.u.one.e)) {
      A_NEW (pendingprs, act_prs_lang_t *);
      A_NEXT (pendingprs) = prs;
      A_INC (pendingprs);
      return;
    }
    
    newprs.next = &newprs2;
    newprs2 = *prs;
    newprs2.next = NULL;
    i++;

    /* 
       We now have a pull-up and pull-down; check if it needs labels!
    */
    
    for (; i < A_LEN (pendingprs); i++) {
      pendingprs[i-1] = pendingprs[i];
    }
    A_LEN(pendingprs)--;

    
  }
  pi = _gen_prs_attributes (&newprs);
  if (cell_table) {
    b = chash_lookup (cell_table, pi);
    if (b) {
      pi = (struct act_prsinfo *)b->v;
      /* found match! */
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
      snprintf (buf, 100, "cx%d", proc_inst_count++);
    } while (p->CurScope()->Lookup (buf));

    Assert (p->CurScope()->isExpanded(), "Hmm");

    InstType *it = new InstType (p->CurScope(), pi->cell, 0);
    it = it->Expand (NULL, p->CurScope());

    Assert (it->isExpanded(), "Hmm");
    
    Assert (p->CurScope()->Add (buf, it), "What?");
    /*--- now make connections --*/
    
    ActBody_Conn *ac;

    //printf (" --- [%s] \n", p->getName());
    ac = _build_connections (buf, pi);
    //ac->Print (stdout);
    //ac->Next()->Print (stdout);
    //printf (" --- \n");


    int oval = Act::warn_double_expand;
    Act::warn_double_expand = 0;
    ac->Expandlist (NULL, p->CurScope ());
    Act::warn_double_expand = oval;
  }
}

void ActCellPass::_collect_one_passgate (Process *p, act_prs_lang_t *prs)
{
  int i;
  
  // add this prs to the list, if it is paired then we can find a gate
  Assert (prs->type == ACT_PRS_GATE, "Hmm.");

  /* XXX: ignoring attributes! */

  char buf[100];
  do {
    snprintf (buf, 100, "cx%d", proc_inst_count++);
  } while (p->CurScope()->Lookup (buf));

  Assert (p->CurScope()->isExpanded(), "Hmm");

  Process *cell;
  ActNamespace *cellns = ActNamespace::Global()->findNS ("cell");
  Assert (cellns, "No cell namespace!");

  if (prs->u.p.g && prs->u.p._g) {
    if (prs->u.p.sz) {
      cell = dynamic_cast<Process *>(cellns->findType ("t0"));
    }
    else {
      cell = dynamic_cast<Process *>(cellns->findType ("t1"));
    }
  }
  else if (prs->u.p.g) {
    if (prs->u.p.sz) {
      cell = dynamic_cast<Process *>(cellns->findType ("n0"));
    }
    else {
      cell = dynamic_cast<Process *>(cellns->findType ("n1"));
    }
  }
  else {
    if (prs->u.p.sz) {
      cell = dynamic_cast<Process *>(cellns->findType ("p0"));
    }
    else {
      cell = dynamic_cast<Process *>(cellns->findType ("p1"));
    }
  }

  Assert (cell, "No transmission gates?");
    
  InstType *it = new InstType (p->CurScope(), cell, 0);
  int w, l;
  if (prs->u.p.sz) {
    if (prs->u.p.sz->w) {
      w = prs->u.p.sz->w->u.v;
    }
    else {
      w = config_get_int ("net.std_p_width");
    }
    if (prs->u.p.sz->l) {
      l = prs->u.p.sz->l->u.v;
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
  it = it->Expand (NULL, p->CurScope());

  Assert (it->isExpanded(), "Hmm");
  
  Assert (p->CurScope()->Add (buf, it), "What?");
    /*--- now make connections --*/
    
  ActBody_Conn *ac;

    //printf (" --- [%s] \n", p->getName());
  ac = _build_connections (buf, prs);
    //ac->Print (stdout);
    //ac->Next()->Print (stdout);
    //printf (" --- \n");


  int oval = Act::warn_double_expand;
  Act::warn_double_expand = 0;
  ac->Expandlist (NULL, p->CurScope ());
  Act::warn_double_expand = oval;
}






static void _collect_group_prs (Process *p, int tval, act_prs_lang_t *prs)
{
  Assert (tval > 0, "tree<> directive with <= 0 value?");
  // mark these rules as part of a tree: needs to be fixed
}
  


/*
  Helper function for walking through the production rule block
*/
void ActCellPass::collect_gates (Process *p, act_prs_lang_t **pprs)
{
  act_prs_lang_t *prs = *pprs;
  act_prs_lang_t *prev = NULL;
  while (prs) {
    switch (prs->type) {
    case ACT_PRS_RULE:
#if 1
      /* snip rule */
      if (prev) {
	prev->next = prs->next;
      }
#endif      
      _collect_one_prs (p, prs);
      break;
    case ACT_PRS_GATE:
#if 1
      if (prev) {
	prev->next = prs->next;
      }
      _collect_one_passgate (p, prs);
#else
      /* preserve pass gates */
      if (!prev) {
	*pprs = prs;
      }
      prev = prs;
#endif      
      break;
    case ACT_PRS_TREE:
#if 1
      if (prev) {
	prev->next = prs->next;
      }
#endif      
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
#if 1
      if (prs->u.l.p) {
	/* there's something left */
	if (!prev) {
	  *pprs = prs;
	}
	prev = prs;
      }
#endif      
      break;
    case ACT_PRS_LOOP:
      fatal_error ("Should have been expanded");
      break;
    }
    prs = prs->next;
  }
#if 1
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
  Assert (p->isExpanded(), "Only works for expanded processes!");

  if (visited_procs->find(p) != visited_procs->end()) {
    return;
  }
  visited_procs->insert (p);

  if (p->isCell() && p->isLeaf()) {
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
	prs_to_cells (x);
      }
    }
  }

  proc_inst_count = 0;
  A_INIT (pendingprs);
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
  flush_pending (p);

  prs = p->getprs();
  act_prs *prevprs = NULL;

  /* remove empty prs blocks */
  while (prs) {
    if (prs->p) {
      prevprs = prs;
    }
    else {
      if (!prevprs) {
	p->lang->setprs (prs->next);
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
  int cellmax = -1;

  /*
    If cells are not expanded, then expand them!
  */
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

      {
	char buf[100];
	int cut;
	snprintf (buf, 100, "%s", p->getName());
	cut = strlen (buf) - 2;
	buf[cut] = '\0';
	UserDef *u = cells->findType (buf);
	Assert (u, "Hmm");
	pi->cell = dynamic_cast<Process *>(u);
	Assert (pi->cell, "Hmm...");
      }
      
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
#if 0	
	_dump_prsinfo (pi);
#endif	
	b->v = pi;
      }
    }
  }
  return cellmax;
}


int ActCellPass::run (Process *p)
{
  /*-- start the pass --*/
  init ();

  /*-- run dependencies --*/
  if (!rundeps (p)) {
    return 0;
  }

  if (!p) {
    ActNamespace *g = ActNamespace::Global();
    ActInstiter i(g->CurScope());
    
    for (i = i.begin(); i != i.end(); i++) {
      ValueIdx *vx = *i;
      if (TypeFactory::isProcessType (vx->t)) {
	Process *x = dynamic_cast<Process *>(vx->t->BaseType());
	if (x->isExpanded()) {
	  prs_to_cells (x);
	}
      }
    }
  }
  else {
    prs_to_cells (p);
  }

  delete visited_procs;
  visited_procs = NULL;

  /*-- finished --*/
  _finished = 2;
  return 1;
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


ActCellPass::ActCellPass (Act *a) : ActPass (a, "prs2cells")
{
  cell_table = NULL;
  visited_procs = NULL;
  cell_ns = NULL;
  proc_inst_count = 0;
  cell_count = 0;
}

ActCellPass::~ActCellPass ()
{
  if (cell_table) {
    chash_free (cell_table);
  }
}
  
int ActCellPass::init ()
{
  if (cell_table) {
    warning ("ActCellPass::init(): cannot be run more than once!");
    return 0;
  }
  
  cell_table = chash_new (32);
  cell_table->hash = cell_hashfn;
  cell_table->match = cell_matchfn;
  cell_table->dup = cell_dupfn;
  cell_table->free = cell_freefn;
  cell_table->print = NULL;

  A_INIT (current_idmap.ids);
  current_idmap.nout = 0;
  current_idmap.nat = 0;
    
  visited_procs = new std::set<Process *> ();
  
  cell_ns = a->findNamespace ("cell");
  
  if (!cell_ns) {
    cell_ns = new ActNamespace (ActNamespace::Global(), "cell");
    cell_ns->Expand ();
    cell_count = 0;
  }
  else {
    cell_count =  _collect_cells (cell_ns) + 1;
  }    
  add_passgates ();
  
  _finished = 1;
  return 1;
}
