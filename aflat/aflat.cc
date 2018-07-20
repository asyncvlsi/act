/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include "aflat.h"
#include <act/iter.h>
#include <act/inst.h>
#include <act/lang.h>

/* prefix tracking */

static list_t *prefixes = NULL;

void push_namespace_name (const char *s)
{
  char *n;
  MALLOC (n, char, strlen (s)+3);
  sprintf (n, "%s::", s);
  list_append (prefixes, n);
}

void push_name (const char *s)
{
  char *n;
  MALLOC (n, char, strlen (s) + 2);
  sprintf (n, "%s.", s);
  list_append (prefixes, n);
}

void push_name_array (const char *s, const char *t)
{
  char *n;
  MALLOC (n, char, strlen (s) + strlen (t) + 2);
  sprintf (n, "%s%s.", s, t);
  list_append (prefixes, n);
}

void pop_name ()
{
  char *s = (char *) list_delete_tail (prefixes);
  FREE (s);
}

void prefix_id_print (ActId *id)
{
  listitem_t *li;
  printf ("\"");
  if (prefixes) {
    for (li = list_first (prefixes); li; li = list_next (li)) {
      printf ("%s", (char *)list_value (li));
    }
  }
  id->Print (stdout);
  printf ("\"");
}

/* hash table for labels */
static struct Hashtable *labels;

#define PREC_BEGIN(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      printf ("(");				\
    }						\
  } while (0)

#define PREC_END(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      printf (")");				\
    }						\
  } while (0)

#define EMIT_BIN(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    _print_prs_expr (e->u.e.l, (myprec), flip);	\
    printf ("%s", (sym));			\
    _print_prs_expr (e->u.e.r, (myprec), flip);	\
    PREC_END (myprec);				\
  } while (0)

#define EMIT_UNOP(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    printf ("%s", sym);				\
    _print_prs_expr (e->u.e.l, (myprec), flip);	\
    PREC_END (myprec);				\
  } while (0)

/*
  3 = ~
  2 = &
  1 = |
*/
static void _print_prs_expr (act_prs_expr_t *e, int prec, int flip = 0)
{
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
    EMIT_BIN(2, "&");
    break;
    
  case ACT_PRS_EXPR_OR:
    EMIT_BIN(1, "|");
    break;
    
  case ACT_PRS_EXPR_VAR:
    if (flip) {
      printf ("~");
    }
    prefix_id_print (e->u.v.id);
    break;
    
  case ACT_PRS_EXPR_NOT:
    EMIT_UNOP(3, "~");
    break;
    
  case ACT_PRS_EXPR_LABEL:

  case ACT_PRS_EXPR_TRUE:
    printf ("true");
    break;
    
  case ACT_PRS_EXPR_FALSE:
    printf ("false");
    break;

  default:
    fatal_error ("What?");
    break;
  }
}


void aflat_print_prs (act_prs_lang_t *p)
{
  if (!p) return;
  
  while (p) {
    switch (p->type) {
    case ACT_PRS_RULE:
      /* attributes */
      if (p->u.one.label) {
	hash_bucket_t *b;
	if (hash_lookup (labels, (char *)p->u.one.id)) {
	  fatal_error ("Duplicate label `%s'", (char *)p->u.one.id);
	}
	b = hash_add (labels, (char *)p->u.one.id);
	b->v = p;
      }
      else {
	/* examine attributes */
	_print_prs_expr (p->u.one.e, 0);
	printf ("->");
	prefix_id_print (p->u.one.id);
	if (p->u.one.dir) {
	  printf ("+\n");
	}
	else {
	  printf ("-\n");
	}
	if (p->u.one.arrow_type == 1) {
	  printf ("~(");
	  _print_prs_expr (p->u.one.e, 0);
	  printf (")");
	  printf ("->");
	  prefix_id_print (p->u.one.id);
	  if (p->u.one.dir) {
	    printf ("-\n");
	  }
	  else {
	    printf ("+\n");
	  }
	}
	else if (p->u.one.arrow_type == 2) {
	  _print_prs_expr (p->u.one.e, 1);
	  printf ("->");
	  prefix_id_print (p->u.one.id);
	  if (p->u.one.dir) {
	    printf ("-\n");
	  }
	  else {
	    printf ("+\n");
	  }
	}
	else if (p->u.one.arrow_type != 0) {
	  fatal_error ("Eh?");
	}
      }
      break;
    case ACT_PRS_GATE:
      /* not printing pass fets */
      break;
    case ACT_PRS_TREE:
      /* this is fine */
      aflat_print_prs (p->u.l.p);
      break;
    case ACT_PRS_SUBCKT:
      /* this is fine */
      aflat_print_prs (p->u.l.p);
      break;
    default:
      fatal_error ("loops should have been expanded by now!");
      break;
    }
    p = p->next;
  }
}


void aflat_prs_scope (Scope *s)
{
  ActInstiter inst(s);

  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx;
    UserDef *ux;
    InstType *it;
    int count;

    vx = *inst;

    it = vx->t;

    ux = dynamic_cast<UserDef *>(it->BaseType());
    
    if (ux) {
      if (!vx->isPrimary()) {
	continue;
      }

      act_prs *p = ux->getprs();

      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	act_connection *c = vx->connection();
	int idx = 0;

	while (!step->isend()) {
	  if (!c || !c->a || !c->a[idx] || (c->a[idx]->up == NULL)) {
	    char *tmp =  step->string();
	    push_name_array (vx->getName(), tmp);
	    FREE (tmp);
	    if (p) {
	      labels = hash_new (2);
	      aflat_print_prs (p->p);
	      hash_free (labels);
	      labels = NULL;
	    }
	    aflat_prs_scope (ux->CurScope());
	    pop_name ();
	  }
	  idx++;
	  step->step();
	}
	delete step;
      }
      else {
	push_name (vx->getName());
	if (p) {
	  labels = hash_new (2);
	  aflat_print_prs (p->p);
	  hash_free (labels);
	  labels = NULL;
	}
	aflat_prs_scope (ux->CurScope());
	pop_name ();
      }
    }
    else {
      /* for bools, print connections as well */
    }
  }
}


void aflat_prs_ns (ActNamespace *ns)
{
  act_prs *p;

  /* my top-level prs */
  p = ns->getprs();
  if (p) {
    aflat_print_prs (p->p);
  }

  /* sub-namespaces */
  ActNamespaceiter iter(ns);

  for (iter = iter.begin(); iter != iter.end(); iter++) {
    ActNamespace *t = *iter;

    push_namespace_name (t->getName());
    aflat_prs_ns (t);
    pop_name ();
  }

  /* instances */
  aflat_prs_scope (ns->CurScope());
  
}


void aflat_prs (Act *a)
{
  prefixes = list_new ();
  aflat_prs_ns (a->Global());
  list_free (prefixes);
}
