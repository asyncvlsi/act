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

static int offset (act_connection **a, act_connection *c)
{
  int i;
  i = 0;
  while (1) {
    if (a[i] == c) return i;
    i++;
    if (i > 100000) return -1;
  }
  return -1;
}

void prefix_connid_print (act_connection *c, const char *s = "")
{
  list_t *stk = list_new ();
  ValueIdx *vx;
  listitem_t *li;
  
  printf ("\"");
  if (prefixes) {
    for (li = list_first (prefixes); li; li = list_next (li)) {
      printf ("%s", (char *)list_value (li));
    }
  }
  
  while (c) {
    stack_push (stk, c);
    if (c->vx) {
      c = c->parent;
    }
    else if (c->parent->vx) {
      c = c->parent->parent;
    }
    else {
      Assert (c->parent->parent->vx, "What?");
      c = c->parent->parent->parent;
    }
  }

  while (!stack_isempty (stk)) {
    c = (act_connection *) stack_pop (stk);
    if (c->vx) {
      vx = c->vx;
      printf ("%s", vx->u.obj.name);
    }
    else if (c->parent->vx) {
      vx = c->parent->vx;
      if (vx->t->arrayInfo()) {
	Array *tmp;
	tmp = vx->t->arrayInfo()->unOffset (offset (c->parent->a, c));
	printf ("%s", vx->u.obj.name);
	tmp->Print (stdout);
	delete tmp;
      }
      else {
	UserDef *ux;
	ux = dynamic_cast<UserDef *> (vx->t->BaseType());
	Assert (ux, "What?");
	printf ("%s.%s", vx->u.obj.name, ux->getPortName (offset (c->parent->a, c)));
      }
    }
    else {
      vx = c->parent->parent->vx;

      Array *tmp;
      tmp = vx->t->arrayInfo()->unOffset (offset (c->parent->parent->a, c->parent));
      UserDef *ux;
      ux = dynamic_cast<UserDef *> (vx->t->BaseType());
      Assert (ux, "what?");

      printf ("%s", vx->u.obj.name);
      tmp->Print (stdout);
      printf (".%s", ux->getPortName (offset (c->parent->a, c)));

      delete tmp;
    }
    if (vx->global) {
      /* XXX */
    }
    if (!stack_isempty (stk)) {
      printf (".");
    }
  }
  printf ("%s\"", s);

  list_free (stk);
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

void _print_connections_bool (ValueIdx *vx)
{
  act_connection *c = vx->connection();
  act_connection *tmp;
  ValueIdx *vx2;

  tmp = c->next;
  while (tmp != c) {
    if (vx->t->arrayInfo()) {
      Arraystep *s1 = vx->t->arrayInfo()->stepper();

      /* tmp might have a different array index, so it needs its own stepper */
      Arraystep *s2;


      if (tmp->vx) {
	Assert (tmp->vx->t->arrayInfo(), "huh?");
	s2 = tmp->vx->t->arrayInfo()->stepper();
	vx2 = tmp->vx;
      }
      else if (tmp->parent->vx) {
	Assert (tmp->parent->vx->t->arrayInfo(), "What?");
	s2 = tmp->parent->vx->t->arrayInfo()->stepper();
	vx2 = tmp->parent->vx;
      }
      else {
	Assert (0, "Can't be this case...");
      }

      while (!s1->isend()) {
	char *tmp1, *tmp2;
	Assert (!s2->isend(), "What?");

	tmp1 = s1->string();
	
	printf ("= ");
	prefix_connid_print (c, tmp1);
	FREE (tmp1);
	printf (" ");

	tmp2 = s2->string();
	prefix_connid_print (tmp, tmp2);
	FREE (tmp2);
	printf ("\n");

	s1->step();
	s2->step();
      }
      Assert (s2->isend(), "Hmm...");
      delete s1;
      delete s2;
    }
    else {
      printf ("= ");
      prefix_connid_print (c);
      printf (" ");
      prefix_connid_print (tmp);
      printf ("\n");
    }
    tmp = tmp->next;
  }
}
  
  


void aflat_prs_scope (Scope *s)
{
  ActInstiter inst(s);

  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx;
    Process *px;
    InstType *it;
    int count;

    vx = *inst;

    if (!vx->isPrimary()) {
      continue;
    }
	
    it = vx->t;
    px = dynamic_cast<Process *>(it->BaseType());
    
    if (px) {
      act_prs *p = px->getprs();

      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	int idx = 0;

	while (!step->isend()) {
	  if (vx->isPrimary(idx)) {
	    char *tmp =  step->string();
	    push_name_array (vx->getName(), tmp);
	    FREE (tmp);
	    if (p) {
	      labels = hash_new (2);
	      aflat_print_prs (p->p);
	      hash_free (labels);
	      labels = NULL;
	    }
	    aflat_prs_scope (px->CurScope());
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
	aflat_prs_scope (px->CurScope());
	pop_name ();
      }
    }

    /* now print connections, if any */
    if (vx->hasConnection()) {
      /* ok, now we get to look at this more closely */
      if (TypeFactory::isUserType (it)) {
	/* user-defined---now expand recursively */
	act_connection *c;
	c = vx->connection();
	if (c->next != c) {
	  /* ok, we have other user-defined things directly connected,
	     take care of this */
	}
	if (c->a) {
	  /* we have connections to components of this as well, check! */
	  /* if it is an array.... check c->a[i]->a[j] */
	}
      }
      else if (TypeFactory::isBoolType (it)) {
	/* print connections! */
	_print_connections_bool (vx);
      }
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
