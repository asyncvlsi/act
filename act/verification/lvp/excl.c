/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#include <stdio.h>
#include "var.h"
#include "lvs.h"
#include "act/common/misc.h"
#include "act/common/bool.h"

static struct excllists *EXTRA_EXCLHI = NULL, *EXTRA_EXCLLO = NULL;


/*
 * add node to an excl list
 */
struct excl *add_to_excl_list (struct excl *l, var_t *v)
{
  struct excl *m;

  MALLOC (m, struct excl, 1);
  m->v = v;
  m->next = l;

#ifdef VERBOSE
  printf (" --> add %s to excl list\n", var_string (v));
#endif 

  return m ;
}

void add_exclhi_list (struct excl *l)
{
  struct excllists *m;

#ifdef VERBOSE
  printf (" ### EXCLHI list\n\n");
#endif

  MALLOC (m, struct excllists, 1);
  m->l = l;
  m->next = EXTRA_EXCLHI;
  EXTRA_EXCLHI = m;
}

void add_excllo_list (struct excl *l)
{
  struct excllists *m;

#if VERBOSE
  printf (" ### EXCLLO list\n\n");
#endif

  MALLOC (m, struct excllists, 1);
  m->l = l;
  m->next = EXTRA_EXCLLO;
  EXTRA_EXCLLO = m;
}


int extra_exclhi (var_t *a, var_t *b)
{
  struct excllists *l;
  struct excl *ll;
  var_t *t;
  int found;

  a = canonical_name (a);
  b = canonical_name (b);

  if (a == b) return 0;

  for (l=EXTRA_EXCLHI; l; l = l->next) {
    found = 0;
    for (ll = l->l; ll; ll = ll->next) {
      t = canonical_name (ll->v);
      if (t == a) {
	found++;
	if (found == 2) return 1;
      }
      if (t == b) {
	found++;
	if (found == 2) return 1;
      }
    }
  }
  return 0;
}

int extra_excllo (var_t *a, var_t *b)
{
  struct excllists *l;
  struct excl *ll;
  var_t *t;
  int found;

  a = canonical_name (a);
  b = canonical_name (b);

  if (a == b) return 0;

  for (l=EXTRA_EXCLLO; l; l = l->next) {
    found = 0;
    for (ll = l->l; ll; ll = ll->next) {
      t = canonical_name (ll->v);
      if (t == a) {
	found++;
	if (found == 2) return 1;
      }
      if (t == b) {
	found++;
	if (found == 2) return 1;
      }
    }
  }
  return 0;
}


bool_t *compute_additional_local_n_invariant (BOOL_T *B, var_t *v)
{
  struct excllists *l;
  struct excl *ll, *mm;
  var_t *t;
  bool_t *invtmp, *t2, *t3, *t1, *t4;

  v = canonical_name (v);
  invtmp = bool_true (B);

  for (l = EXTRA_EXCLHI; l; l = l->next) {
    for (ll = l->l; ll; ll = ll->next) {
      t = canonical_name (ll->v);
      if (t == v) {

	t1 = bool_false (B); /* gather disjuncts */

	for (ll = l->l; ll; ll = ll->next) {

	  t2 = bool_true (B); /* make conjunct */

	  for (mm = l->l; mm; mm = mm->next) {
	    if (mm != ll) {

	      t = canonical_name (mm->v);

	      Assert (t->flags & VAR_HAVEVAR, "Huh???");
	      t3 = bool_and (B, t2, t4 = bool_not (B, bool_var (B,
								t->v)));
	      bool_free (B, t2);
	      bool_free (B, t4);
	      t2 = t3;
	    }
	  }
	  t3 = bool_or (B, t1, t2);
	  bool_free (B, t1);
	  bool_free (B, t2);
	  t1 = t3;
	}
	invtmp = bool_and (B, t2 = invtmp, t1);
	bool_free (B, t2);
	break;
      }
    }
  }
  return invtmp;
}


bool_t *compute_additional_local_p_invariant (BOOL_T *B, var_t *v)
{
  struct excllists *l;
  struct excl *ll, *mm;
  var_t *t;
  bool_t *invtmp, *t2, *t3, *t1, *t4;

  v = canonical_name (v);
  invtmp = bool_true (B);

  for (l = EXTRA_EXCLLO; l; l = l->next) {
    for (ll = l->l; ll; ll = ll->next) {
      t = canonical_name (ll->v);
      if (t == v) {
	t1 = bool_false (B);
	for (ll = l->l; ll; ll = ll->next) {
	  t2 = bool_true (B);
	  for (mm = l->l; mm; mm = mm->next) {
	    if (mm != ll) {
	      t = canonical_name (mm->v);
	      Assert (t->flags & VAR_HAVEVAR, "Huh???");
	      t3 = bool_and (B, t2, bool_var (B, t->v));
	      bool_free (B, t2);
	      t2 = t3;
	    }
	  }
	  t3 = bool_or (B, t1, t2);
	  bool_free (B, t1);
	  bool_free (B, t2);
	  t1 = t3;
	}
	invtmp = bool_and (B, t2 = invtmp, t1);
	bool_free (B, t2);
	break;
      }
    }
  }
  return invtmp;
}


