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
#include "aflat.h"
#include "config.h"
#include <act/iter.h>

/* hash table for labels */
static struct Hashtable *labels;

/* prefix tracking */
static list_t *prefixes = NULL;

static list_t *suffixes = NULL;

static output_formats export_format;

#define ARRAY_STYLE (export_format == LVS_FMT ? 1 : 0)
#define EXTRA_ARGS  NULL, (export_format == LVS_FMT ? 1 : 0)

static void print_connect()
{
  if (export_format == LVS_FMT) {
    printf ("connect ");
  }
  else {
    printf ("= ");
  }
}

static void push_namespace_name (const char *s)
{
  char *n;
  MALLOC (n, char, strlen (s)+3);
  sprintf (n, "%s::", s);
  list_append (prefixes, n);
}

static void push_name (const char *s)
{
  char *n;
  MALLOC (n, char, strlen (s) + 2);
  sprintf (n, "%s.", s);
  list_append (prefixes, n);
}

static void push_name_suffix (const char *s)
{
  char *n;
  MALLOC (n, char, strlen (s) + 2);
  sprintf (n, "%s.", s);
  list_append (suffixes, n);
}

static void push_name_array_suffix (const char *s, const char *t)
{
  char *n;
  MALLOC (n, char, strlen (s) + strlen (t) + 2);
  sprintf (n, "%s%s.", s, t);
  list_append (suffixes, n);
}

static void pop_name_suffix ()
{
  char *s = (char *) list_delete_tail (suffixes);
  FREE (s);
}

static void push_name_array (const char *s, const char *t)
{
  char *n;
  MALLOC (n, char, strlen (s) + strlen (t) + 2);
  sprintf (n, "%s%s.", s, t);
  list_append (prefixes, n);
}

static void pop_name ()
{
  char *s = (char *) list_delete_tail (prefixes);
  FREE (s);
}

static void prefix_print ()
{
  listitem_t *li;
  if (prefixes) {
    for (li = list_first (prefixes); li; li = list_next (li)) {
      printf ("%s", (char *)list_value (li));
    }
  }
}

static void suffix_print ()
{
  listitem_t *li;
  printf (".");
  if (suffixes) {
    for (li = list_first (suffixes); li; li = list_next (li)) {
      printf ("%s", (char *)list_value (li));
    }
  }
}

static void prefix_id_print (Scope *s, ActId *id, const char *str = "")
{
  printf ("\"");

  if (s->Lookup (id, 0)) {
    prefix_print ();
  }
  else {
    ValueIdx *vx;
    /* must be a global */
    vx = s->FullLookupVal (id->getName());
    Assert (vx, "Hmm.");
    Assert (vx->global, "Hmm");
    if (vx->global == ActNamespace::Global()) {
      /* nothing to do */
    }
    else {
      char *tmp = vx->global->Name ();
      printf ("%s::", tmp);
      FREE (tmp);
    }
  }
  id->Print (stdout, EXTRA_ARGS);
  printf ("%s\"", str);
}

static void prefix_connid_print (act_connection *c, const char *s = "")
{
  printf ("\"");
  prefix_print ();
  ActId *tid = c->toid();
  tid->Print (stdout, EXTRA_ARGS);
  delete tid;
  printf ("%s\"", s);
}


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
    _print_prs_expr (s, e->u.e.l, (myprec), flip);	\
    printf ("%s", (sym));			\
    _print_prs_expr (s, e->u.e.r, (myprec), flip);	\
    PREC_END (myprec);				\
  } while (0)

#define EMIT_UNOP(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    printf ("%s", sym);				\
    _print_prs_expr (s, e->u.e.l, (myprec), flip);	\
    PREC_END (myprec);				\
  } while (0)

/*
  3 = ~
  2 = &
  1 = |
*/
static void _print_prs_expr (Scope *s, act_prs_expr_t *e, int prec, int flip)
{
  hash_bucket_t *b;
  act_prs_lang_t *pl;
  
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
    prefix_id_print (s, e->u.v.id);
    break;
    
  case ACT_PRS_EXPR_NOT:
    EMIT_UNOP(3, "~");
    break;
    
  case ACT_PRS_EXPR_LABEL:
    if (!labels) {
      fatal_error ("No labels defined!");
    }
    b = hash_lookup (labels, e->u.l.label);
    if (!b) {
      fatal_error ("Missing label `%s'", e->u.l.label);
    }
    pl = (act_prs_lang_t *) b->v;
    if (pl->u.one.dir == 0) {
      printf ("~");
    }
    printf ("(");
    _print_prs_expr (s, pl->u.one.e, 0, flip);
    printf (")");
    break;

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

static void print_attr_prefix (act_attr_t *attr)
{
  /* examine attributes:
     after
     weak
     unstab
  */
  int weak = 0;
  int unstab = 0;
  int after = 0;
  int have_after = 0;
  act_attr_t *x;

  if (export_format == LVS_FMT) return;

  for (x = attr; x; x = x->next) {
    if (strcmp (x->attr, "weak") == 0) {
      weak = 1;
    }
    else if (strcmp (x->attr, "unstab") == 0) {
      unstab = 1;
    }
    else if (strcmp (x->attr, "after") == 0) {
      after = x->e->u.v;
      have_after = 1;
    }
  }
  if (weak) {
    printf ("weak ");
  }
  if (unstab) {
    printf ("unstab ");
  }
  if (have_after) {
    printf ("after %d ", after);
  }
}

static void aflat_print_spec (Scope *s, act_spec *spec)
{
  const char *tmp;
  ActId *id;
  while (spec) {
    tmp = act_spec_string (spec->type);
    Assert (tmp, "Hmm");
    if ((export_format == PRSIM_FMT &&
	 (strcmp (tmp, "mk_exclhi") == 0 || strcmp (tmp, "mk_excllo") == 0)) ||
	(export_format == LVS_FMT &&
	 (strcmp (tmp, "exclhi") == 0 || strcmp (tmp, "excllo") == 0))) {
      if (spec->count > 1) {
	int comma = 0;
	printf ("%s(", tmp);
	for (int i=0; i < spec->count; i++) {
	  id = spec->ids[i];
	  ValueIdx *vx = s->FullLookupVal (id->getName());
	  InstType *it = vx->t;
	  Assert (vx, "What");
	  while (id->Rest()) {
	    UserDef *u = dynamic_cast<UserDef *> (it->BaseType ());
	    Assert (u, "Hm");
	    id = id->Rest();
	    it = u->Lookup (id);
	    Assert (it, "Hmm");
	  }
	  /* spec->ids[i] might be a bool, or a bool array! fix bool
	     array case */
	  if (it->arrayInfo()) {
	    Array *a = it->arrayInfo();
	    if (id->arrayInfo()) {
	      a = id->arrayInfo();
	    }
	    Arraystep *astep = a->stepper();
	    int restore = 0;
	    if (id->arrayInfo()) {
	      id->setArray (NULL);
	      restore = 1;
	    }
	    while (!astep->isend()) {
	      char *tmp = astep->string();
	      if (comma != 0) {
		printf (",");
	      }
	      prefix_id_print (s, spec->ids[i], tmp);
	      comma = 1;
	      FREE (tmp);
	      astep->step();
	    }
	    delete astep;
	    if (restore) {
	      id->setArray (a);
	    }
	  }
	  else {
	    if (comma != 0) {
	      printf (",");
	    }
	    prefix_id_print (s, spec->ids[i]);
	    comma = 1;
	  }
	}
	printf (")\n");
      }
    }
    spec = spec->next;
  }
}

static void aflat_print_prs (Scope *s, act_prs_lang_t *p)
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
	print_attr_prefix (p->u.one.attr);
	_print_prs_expr (s, p->u.one.e, 0, 0);
	printf ("->");
	prefix_id_print (s, p->u.one.id);
	if (p->u.one.dir) {
	  printf ("+\n");
	}
	else {
	  printf ("-\n");
	}
	if (p->u.one.arrow_type == 1) {
	  print_attr_prefix (p->u.one.attr);
	  printf ("~(");
	  _print_prs_expr (s, p->u.one.e, 0, 0);
	  printf (")");
	  printf ("->");
	  prefix_id_print (s, p->u.one.id);
	  if (p->u.one.dir) {
	    printf ("-\n");
	  }
	  else {
	    printf ("+\n");
	  }
	}
	else if (p->u.one.arrow_type == 2) {
	  print_attr_prefix (p->u.one.attr);
	  _print_prs_expr (s, p->u.one.e, 0, 1);
	  printf ("->");
	  prefix_id_print (s, p->u.one.id);
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
      aflat_print_prs (s, p->u.l.p);
      break;
    case ACT_PRS_SUBCKT:
      /* this is fine */
      aflat_print_prs (s, p->u.l.p);
      break;
    default:
      fatal_error ("loops should have been expanded by now!");
      break;
    }
    p = p->next;
  }
}

static void _print_connections_bool (ValueIdx *vx)
{
  act_connection *c = vx->connection();
  ActConniter iter(c);
  act_connection *tmp;
  ValueIdx *vx2;
  int is_global, ig;

  is_global = vx->connection()->isglobal();

  for (iter = iter.begin(); iter != iter.end(); iter++) {
    tmp = *iter;

    /* don't print connections to yourself */
    if (tmp == c) continue;

    ig = tmp->isglobal();
    //if (!(!is_global || ig == is_global)) continue;
    if (ig != is_global) continue;
    
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

	print_connect();
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
      print_connect();
      prefix_connid_print (c);
      printf (" ");
      prefix_connid_print (tmp);
      printf ("\n");
    }
  }

  /* subconnections in case of an array */
  if (c && vx->t->arrayInfo() && c->hasSubconnections()) {
    for (int i=0; i < c->numSubconnections(); i++) {
      act_connection *d = c->a[i];
      if (!d) continue;
      if (!d->isPrimary()) continue;

      ActConniter iter2(d);

      for (iter2 = iter2.begin(); iter2 != iter2.end(); iter2++) {

	tmp = *iter2;

	if (tmp == d) continue;

	ig = tmp->isglobal();
	//if (!(!ig || ig == is_global)) continue;
	if (ig != is_global) continue;
	
	/* cannot be an array */
	print_connect();
	prefix_connid_print (d);
	printf(" ");
	prefix_connid_print (tmp);
	printf ("\n");
      }
    }
  }
}

/* if nm == NULL, there are no suffixes! */
static void _print_single_connection (ActId *one, Array *oa,
				      ActId *two, Array *ta,
				      const char *nm, Arraystep *na,
				      ActNamespace *isoneglobal)
{
  if (oa) {
    Arraystep *s1, *s2;
    s1 = oa->stepper();
    s2 = ta->stepper();
    while (!s1->isend()) {
      print_connect();
      printf ("\"");
      if (isoneglobal) {
	if (isoneglobal == ActNamespace::Global()) {
	  /* nothing */
	}
	else {
	  char *tmp = isoneglobal->Name();
	  printf ("%s::", tmp);
	  FREE (tmp);
	}
      }
      else {
	prefix_print ();
      }
      one->Print (stdout, EXTRA_ARGS);
      s1->Print (stdout, ARRAY_STYLE);
      if (nm) {
	suffix_print ();
	printf ("%s", nm);
      }
      if (na) {
	na->Print (stdout);
      }
      printf ("\" \"");
      prefix_print ();
      two->Print (stdout);
      s2->Print (stdout);
      if (nm) {
	suffix_print ();
	printf ("%s", nm);
      }
      if (na) {
	na->Print (stdout, ARRAY_STYLE);
      }
      printf ("\"\n");
      s1->step();
      s2->step();
    }
    delete s1;
    delete s2;
  }
  else {
    print_connect();
    printf ("\"");
    if (isoneglobal) {
      if (isoneglobal == ActNamespace::Global()) {
	/* nothing */
      }
      else {
	char *tmp = isoneglobal->Name();
	printf ("%s::", tmp);
	FREE (tmp);
      }
    }
    else {
      prefix_print ();
    }
    one->Print (stdout, EXTRA_ARGS);
    if (nm) {
      suffix_print ();
      printf ("%s", nm);
    }
    if (na) {
      na->Print (stdout, ARRAY_STYLE);
    }
    printf ("\" \"");
    prefix_print ();
    two->Print (stdout, EXTRA_ARGS);
    if (nm) {
      suffix_print ();
      printf ("%s", nm);
    }
    if (na) {
      na->Print (stdout, ARRAY_STYLE);
    }
    printf ("\"\n");
  }
}


static void _print_rec_bool_conns (ActId *one, ActId *two, UserDef *ux,
				   Array *oa, Array *ta,
				   ActNamespace *isoneglobal)

{
  Assert (ux, "What");
  Assert (one, "What");
  Assert (two, "What");
  Assert ((oa && ta) || (!oa && !ta), "What?");

  /* walk through all instances */
  ActInstiter inst(ux->CurScope());
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = (*inst);
    if (TypeFactory::isParamType (vx->t)) continue;
    if (strcmp (vx->u.obj.name, "self") == 0) {
      continue;
    }
      
    //if (!vx->isPrimary()) continue;
    
    if (TypeFactory::isUserType (vx->t)) {
      UserDef *rux = dynamic_cast<UserDef *>(vx->t->BaseType());
      
      if (vx->t->arrayInfo()) {
	Arraystep *p = vx->t->arrayInfo()->stepper ();
	while (!p->isend()) {
	  char *tmp;
	  tmp = p->string(ARRAY_STYLE);
	  push_name_array_suffix (vx->getName (), tmp);
	  FREE (tmp);
	  _print_rec_bool_conns (one, two, rux, oa, ta, isoneglobal);
	  pop_name_suffix ();
	  p->step();
	}
      }
      else {
	push_name_suffix (vx->getName());
	_print_rec_bool_conns (one, two, rux, oa, ta, isoneglobal);
	pop_name_suffix ();
      }
    }
    else if (TypeFactory::isBoolType (vx->t)) {
      /* print all booleans here */
      if (vx->t->arrayInfo()) {
	Arraystep *p = vx->t->arrayInfo()->stepper();
	while (!p->isend()) {
	  _print_single_connection (one, oa, two, ta, vx->getName (), p,
				    isoneglobal);
	  p->step();
	}
      }
      else {
	_print_single_connection (one, oa, two, ta, vx->getName(), NULL,
				  isoneglobal);
      }
    }
  }
}

static void print_any_global_conns (act_connection *c)
{
  act_connection *root;
  list_t *stack;

  stack = list_new ();
  list_append (stack, c);

  while ((c = (act_connection *)list_delete_tail (stack))) {
    root = c->primary();
    if (c->hasDirectconnections()) {
      if (root->isglobal() && !c->isglobal()) {
	InstType *xit, *it;
	ActId *one, *two;
	int type;
	UserDef *rux;
	
	/* print a connection from c to root */
	type = c->getctype ();
	xit = c->getvx()->t;
	it = root->getvx()->t;
	
	rux = dynamic_cast<UserDef *> (xit->BaseType());

	one = root->toid();
	two = c->toid();

	if (TypeFactory::isUserType (xit)) {
	  suffixes = list_new ();
	  _print_rec_bool_conns (one, two, rux,
				 it->arrayInfo(), xit->arrayInfo(),
				 root->getvx()->global);
	  list_free (suffixes);
	  suffixes = NULL;
	}
	else if (TypeFactory::isBoolType (xit)) {
	  _print_single_connection (one, it->arrayInfo(),
				    two, xit->arrayInfo(),
				    NULL, NULL,
				    root->getvx()->global);
	}
	delete one;
	delete two;
      }
    }
    if (c->hasSubconnections()) {
      for (int i=0; i < c->numSubconnections(); i++) {
	if (c->a[i]) {
	  list_append (stack, c->a[i]);
	}
      }
    }
  }
  list_free (stack);
}

static void aflat_prs_scope (Scope *s)
{
  ActInstiter inst(s);

  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx;
    Process *px;
    UserDef *ux;
    InstType *it;
    int count;

    vx = *inst;
    if (TypeFactory::isParamType (vx->t)) continue;

    if (!vx->isPrimary()) {
      if (vx->connection()->isglobal()) continue;
      
      /* Check if this or any of its sub-objects is connected to a
	 global signal. If so, just emit that connection and nothing
	 else. 
      */
      print_any_global_conns (vx->connection());
      continue;
    }

    it = vx->t;
    ux = dynamic_cast<UserDef *>(it->BaseType());
    if (ux) {
      px = dynamic_cast<Process *>(ux);
    }
    else {
      px = NULL;
    }
    if (px || ux) {
      act_prs *p; 
      act_spec *spec;

      if (px) {
	p = px->getprs();
	spec = px->getspec();
      }
      else {
	p = NULL;
	spec = NULL;
      }

      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	int idx = 0;

	while (!step->isend()) {
	  if (vx->isPrimary(idx)) {
	    char *tmp =  step->string(ARRAY_STYLE);
	    push_name_array (vx->getName(), tmp);
	    FREE (tmp);
	    if (px) {
	      p = px->getprs();
	    }
	    else {
	      p = NULL;
	    }
	    while (p) {
	      labels = hash_new (2);
	      aflat_print_prs (px->CurScope(), p->p);
	      hash_free (labels);
	      labels = NULL;
	      p = p->next;
	    }
	    if (spec) {
	      aflat_print_spec (px->CurScope(), spec);
	    }
#if 0	    
	    printf ("ux-scope: %s\n", ux->getName());
#endif	    
	    aflat_prs_scope (ux->CurScope());
#if 0	    
	    printf ("end-ux-scope: %s\n", ux->getName());
#endif
	    pop_name ();
	  }
	  idx++;
	  step->step();
	}
	delete step;
      }
      else {
	push_name (vx->getName());
	while (p) {
	  labels = hash_new (2);
	  aflat_print_prs (px->CurScope(), p->p);
	  hash_free (labels);
	  labels = NULL;
	  p = p->next;
	}
	if (spec) {
	  aflat_print_spec (px->CurScope(), spec);
	}
#if 0	
	printf ("ux-scope: %s\n", ux->getName());
#endif	
	aflat_prs_scope (ux->CurScope());
#if 0	
	printf ("end-ux-scope: %s\n", ux->getName());
#endif	
	pop_name ();
      }
    }

    /* not the special case of global to non-global connection; vx
       is the primary ValueIdx */
    if (vx->hasConnection()) {
      int is_global_conn;

      if (vx->connection()->isglobal()) {
	/* only emit connections when the other vx is a global */
	is_global_conn = 1;
      }
      else {
	/* only emit local connections */
	is_global_conn = 0;
      }
#if 0
      printf ("is_global_conn = %d [name: %s]\n", is_global_conn, vx->getName());
#endif
      
      /* ok, now we get to look at this more closely */
      if (TypeFactory::isUserType (it)) {
	/* user-defined---now expand recursively */
	UserDef *rux = dynamic_cast<UserDef *>(it->BaseType());
	act_connection *c;
	c = vx->connection();
	if (c->hasDirectconnections()) {
	  /* ok, we have other user-defined things directly connected,
	     take care of this */
	  ActId *one, *two;
	  ActConniter ci(c);
	  int ig;

	  one = c->toid();
	  for (ci = ci.begin(); ci != ci.end(); ci++) {
	    if (*ci == c) continue; // don't print connections to yourself

	    ig = (*ci)->isglobal();
	    if (!(!ig || ig == is_global_conn)) continue;
	    // only print global to global or non-global to any
	      
	    two = (*ci)->toid();
	    suffixes = list_new ();
	    _print_rec_bool_conns (one, two, rux, it->arrayInfo(),
				   ((*ci)->vx ?
				    (*ci)->vx->t->arrayInfo() : NULL),
				   NULL);
	    list_free (suffixes);
	    suffixes = NULL;
	    delete two;
	  }
	  delete one;
	}
	if (c->hasSubconnections()) {
	  /* we have connections to components of this as well, check! */
	  list_t *sublist = list_new ();
	  list_append (sublist, c);
	  while ((c = (act_connection *)list_delete_tail (sublist))) {
	    Assert (c->hasSubconnections(), "Invariant fail");

	    for (int i=0; i < c->numSubconnections(); i++) {
	      if (c->hasDirectconnections (i)) {
		if (c->isPrimary (i)) {
		  int type;
		  InstType *xit;
		  ActId *one, *two;
		  ActConniter ci(c->a[i]);
		  int ig;

		  type = c->a[i]->getctype();
		  it = c->a[i]->getvx()->t;

		  UserDef *rux = dynamic_cast<UserDef *> (it->BaseType());

		  /* now find the type */
		  if (type == 0 || type == 1) {
		    xit = it;
		  }
		  else {
		    Assert (rux, "what?");
		    xit = rux->getPortType (i);
		  }

		  one = c->a[i]->toid();
		  for (ci = ci.begin(); ci != ci.end(); ci++) {
		    int type2;
		    if (*ci == c->a[i]) continue;

		    ig = (*ci)->isglobal();

		    if (!(!ig || ig == is_global_conn)) continue;

		    type2 = (*ci)->getctype();

		    two = (*ci)->toid();
		    if (TypeFactory::isUserType (xit)) {
		      suffixes = list_new ();
		      if (type == 1 || type2 == 1) {
			_print_rec_bool_conns (one, two, rux, NULL, NULL, NULL);
		      }
		      else {
			_print_rec_bool_conns (one, two, rux, xit->arrayInfo(),
					       (*ci)->getvx()->t->arrayInfo(),
					       NULL);
		      }
		      list_free (suffixes);
		      suffixes = NULL;
		    }
		    else if (TypeFactory::isBoolType (xit)) {
		      if (type == 1 || type2 == 1) {
			_print_single_connection (one, NULL,
						  two, NULL,
						  NULL, NULL, NULL);
		      }
		      else {
			_print_single_connection (one, xit->arrayInfo(),
						  two,
						  (*ci)->getvx()->t->arrayInfo(),
						  NULL, NULL, NULL);
		      }
		    }
		    delete two;
		  }
		  delete one;
		}
		else {
		  if (!c->a[i]->isglobal()) {
		    print_any_global_conns (c->a[i]);
		  }
		}
	      }
	      if (c->a[i] && c->a[i]->hasSubconnections ()) {
		list_append (sublist, c->a[i]);
	      }
	    }
	  }
	  list_free (sublist);
	}
      }
      else if (TypeFactory::isBoolType (it)) {
	/* print connections! */
	_print_connections_bool (vx);
      }
    }
  }
}


static void aflat_prs_ns (ActNamespace *ns)
{
  act_prs *p;

  /* my top-level prs */
  p = ns->getprs();
  while (p) {
    /* set scope here */
    aflat_print_prs (ns->CurScope(), p->p);
    p = p->next;
  }
  if (ns->getspec ()) {
    aflat_print_spec (ns->CurScope(), ns->getspec ());
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
#if 0  
  printf ("scope-ns: %s\n", (ns == ActNamespace::Global() ? "global" : ns->getName()));
#endif  
  aflat_prs_scope (ns->CurScope());
#if 0  
  printf ("end-scope-ns: %s\n", (ns == ActNamespace::Global() ? "global" : ns->getName()));
#endif  
}


void aflat_prs (Act *a, output_formats fmt)
{
  export_format = fmt;
  prefixes = list_new ();
  aflat_prs_ns (a->Global());
  list_free (prefixes);
}

void act_prsflat_prsim (Act *a)
{
  aflat_prs (a, PRSIM_FMT);
}

void act_prsflat_lvs (Act *a)
{
  aflat_prs (a, LVS_FMT);
}

void act_expand (Act *a)
{
  a->Expand ();
}
