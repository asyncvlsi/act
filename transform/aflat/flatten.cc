/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include "aflat.h"
#include "config.h"
#include <act/iter.h>

/*-- a pass to walk through all connection pairs --*/


/* hash table for labels */
static struct Hashtable *labels;

/* prefix tracking */
static list_t *prefixes = NULL;
static list_t *prefix_array = NULL;

static list_t *suffixes = NULL;
static list_t *suffix_array = NULL;

static void (*g_apply_fn) (void *, ActId *, ActId *) = NULL;
static void (*g_apply_procfn) (void *, ActId *, Process *) = NULL;
static void *g_cookie = NULL;

static void push_namespace_name (const char *s)
{
  char *n;
  MALLOC (n, char, strlen (s)+3);
  sprintf (n, "%s::", s);
  list_append (prefixes, n);
  list_append (prefix_array, NULL);
}

static void push_name (const char *s)
{
  list_append (prefixes, s);
  list_append (prefix_array, NULL);
}

static void push_name_suffix (const char *s)
{
  list_append (suffixes, s);
  list_append (suffix_array, NULL);
}

static void push_name_array_suffix (const char *s, Array *t)
{
  list_append (suffixes, s);
  list_append (suffix_array, t);
}

static void pop_name_suffix ()
{
  list_delete_tail (suffixes);
  Array *a = (Array *) list_delete_tail (suffix_array);
  if (a) {
    delete a;
  }
}

static ActId *suffix_to_id ()
{
  listitem_t *li, *mi;
  Array *a;
  char *s;
  ActId *id, *tail;

  id = NULL;
  tail = NULL;
  if (suffixes) {
    mi = list_first (suffix_array);
    for (li = list_first (suffixes); li; li = list_next (li)) {
      a = (Array *) list_value (mi);
      s = (char *) list_value (li);

      if (!id) {
	id = new ActId (s);
	tail = id;
      }
      else {
	tail->Append (new ActId (s));
	tail = tail->Rest();
      }
      if (a) {
	tail->setArray (a);
      }
      mi = list_next (mi);
    }
  }
  return id;
}  

static void push_name_array (const char *s, Array *t)
{
  list_append (prefixes, s);
  list_append (prefix_array, t);
}

static void pop_name ()
{
  char *s = (char *) list_delete_tail (prefixes);
  if (s[strlen(s)-1] == ':') {
    FREE (s);
  }
  Array *a = (Array *) list_delete_tail (prefix_array);
  if (a) {
    delete a;
  }
}

static ActId *tailid (ActId *id)
{
  if (!id) return NULL;
  while (id->Rest()) {
    id = id->Rest();
  }
  return id;
}

static void prefix_print ();

static ActId *prefix_to_id (ActId **tailp)
{
  listitem_t *li, *mi;
  Array *a;
  char *s;
  ActId *id, *tail;

  id = NULL;
  tail = NULL;
  if (prefixes) {
    mi = list_first (prefix_array);
    for (li = list_first (prefixes); li; li = list_next (li)) {
      a = (Array *) list_value (mi);
      s = (char *) list_value (li);

      if (!id) {
	id = new ActId (s);
	tail = id;
      }
      else {
	tail->Append (new ActId (s));
	tail = tail->Rest();
      }
      if (a) {
	tail->setArray (a);
      }
      mi = list_next (mi);
    }
  }
  if (tailp) {
    *tailp = tail;
  }
  return id;
}

static void nullify_arrays (ActId *id)
{
  while (id) {
    id->setArray (NULL);
    id = id->Rest();
  }
}

static void suffix_print ()
{
  listitem_t *li, *mi;
  Array *a;
  
  printf (".");
  if (suffixes) {
    mi = list_first (suffix_array);
    for (li = list_first (suffixes); li; li = list_next (li)) {
      a = (Array *) list_value (mi);
      printf ("%s", (char *)list_value (li));
      if (a) {
	a->Print (stdout);
      }
      printf (".");
      mi = list_next (mi);
    }
  }
}

static void _flat_connections_bool (ValueIdx *vx)
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

      ActId *id1, *id2;
      ActId *tail1, *tail2;

      id1 = c->toid();
      id2 = tmp->toid();

      tail1 = tailid (id1);
      tail2 = tailid (id2);

      while (!s1->isend()) {
	Array *a1, *a2;
	Assert (!s2->isend(), "What?");

	a1 = s1->toArray ();
	a2 = s2->toArray ();

	tail1->setArray (a1);
	tail2->setArray (a2);

	(*g_apply_fn) (g_cookie, id1, id2);

	delete a1;
	delete a2;
	
	s1->step();
	s2->step();
      }

      Assert (s2->isend(), "Hmm...");
      delete s1;
      delete s2;

      tail1->setArray (NULL);
      tail2->setArray (NULL);

      delete id1;
      delete id2;
      
    }
    else {
      ActId *id1, *id2;
      id1 = c->toid();
      id2 = tmp->toid();
      (*g_apply_fn) (g_cookie, id1, id2);
      delete id1;
      delete id2;
    }
  }

  /* subconnections in case of an array */
  if (c && vx->t->arrayInfo() && c->hasSubconnections()) {
    for (int i=0; i < c->numSubconnections(); i++) {
      act_connection *d = c->a[i];
      if (!d) continue;

      ActConniter iter2(d);
      ActId *id1, *id2;

      id1 = d->toid();

      for (iter2 = iter2.begin(); iter2 != iter2.end(); iter2++) {

	tmp = *iter2;

	if (tmp == d) continue;

	ig = tmp->isglobal();
	if (ig != is_global) continue;

	id2 = tmp->toid();
	(*g_apply_fn) (g_cookie, id1, id2);

	delete id2;
      }
      delete id1;
    }
  }
}

/* if nm == NULL, there are no suffixes! */
static void _flat_single_connection (ActId *one, Array *oa,
				      ActId *two, Array *ta,
				      const char *nm, Arraystep *na,
				      ActNamespace *isoneglobal)
{
  ActId *id1, *id2;
  ActId *tail1, *tail2;
  ActId *tmp1, *tmp2;
  ActId *suf1, *suf2;
  Array *na_arr;

  if (na) {
    na_arr = na->toArray();
  }
  else {
    na_arr = NULL;
  }

  /*-- ok, construct two ids here and call the apply function! --*/
  if (isoneglobal && isoneglobal != ActNamespace::Global()) {
    char buf[10240];
    sprintf (buf, "%s::", isoneglobal->Name());
    id1 = new ActId (buf);
    tail1 = id1;
  }
  else {
    id1 = prefix_to_id (&tail1);
  }
  tail1->Append (one);
  tmp1 = tailid (one);

  if (nm) {
    suf1 = suffix_to_id ();
    tmp1->Append (suf1);
    if (suf1) {
      suf1 = tailid (suf1);
    }
    else {
      suf1 = tmp1;
    }
    suf1->Append (new ActId (nm, na_arr));
  }
  else {
    suf1 = tmp1;
  }

  id2 = prefix_to_id (&tail2);
  tail2->Append (two);
  tmp2 = tailid (two);
  if (nm) {
    suf2 = suffix_to_id ();
    tmp2->Append (suf2);
    if (suf2) {
      suf2 = tailid (suf2);
    }
    else {
      suf2 = tmp2;
    }
    suf2->Append (new ActId (nm, na_arr));
  }
  else {
    suf2 = tmp2;
  }
    
  if (oa) {
    Array *a1, *a2;
    Arraystep *s1, *s2;

    s1 = oa->stepper();
    s2 = ta->stepper();

    while (!s1->isend()) {
      a1 = s1->toArray();
      a2 = s2->toArray();

      tmp1->setArray (a1);
      tmp2->setArray (a2);

      (*g_apply_fn) (g_cookie, id1, id2);

      delete a1;
      delete a2;
      
      s1->step();
      s2->step();
    }
    tmp1->setArray (NULL);
    tmp2->setArray (NULL);
    delete s1;
    delete s2;
  }
  else {
    (*g_apply_fn) (g_cookie, id1, id2);
  }
    
  /* dealloc prefix and suffix */
  tail1->prune();
  nullify_arrays (id1);
  delete id1;
  if (tmp1->Rest()) {
    nullify_arrays (tmp1->Rest());
    delete tmp1->Rest();
    tmp1->prune();
  }

  tail2->prune();
  nullify_arrays (id2);
  delete id2;
  if (tmp2->Rest()) {
    nullify_arrays (tmp2->Rest());
    delete tmp2->Rest();
    tmp2->prune();
  }
  if (na_arr) {
    delete na_arr;
  }
}


static void _flat_rec_bool_conns (ActId *one, ActId *two, UserDef *ux,
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
      
    if (!vx->isPrimary()) continue;
    
    if (TypeFactory::isUserType (vx->t)) {
      UserDef *rux = dynamic_cast<UserDef *>(vx->t->BaseType());
      
      if (vx->t->arrayInfo()) {
	Arraystep *p = vx->t->arrayInfo()->stepper ();
	while (!p->isend()) {
	  push_name_array_suffix (vx->getName (), p->toArray());
	  _flat_rec_bool_conns (one, two, rux, oa, ta, isoneglobal);
	  pop_name_suffix ();
	  p->step();
	}
      }
      else {
	push_name_suffix (vx->getName());
	_flat_rec_bool_conns (one, two, rux, oa, ta, isoneglobal);
	pop_name_suffix ();
      }
    }
    else if (TypeFactory::isBoolType (vx->t)) {
      /* print all booleans here */
      if (vx->t->arrayInfo()) {
	Arraystep *p = vx->t->arrayInfo()->stepper();
	while (!p->isend()) {
	  _flat_single_connection (one, oa, two, ta, vx->getName (), p,
				   isoneglobal);
	  p->step();
	}
      }
      else {
	_flat_single_connection (one, oa, two, ta, vx->getName(), NULL,
				  isoneglobal);
      }
    }
  }
}

static void _any_global_conns (act_connection *c)
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
	  suffix_array = list_new ();
	  _flat_rec_bool_conns (one, two, rux,
				 it->arrayInfo(), xit->arrayInfo(),
				 root->getvx()->global);
	  list_free (suffixes);
	  list_free (suffix_array);
	  suffixes = NULL;
	  suffix_array = NULL;
	}
	else if (TypeFactory::isBoolType (xit)) {
	  _flat_single_connection (one, it->arrayInfo(),
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


static void _flat_scope (Scope *s)
{
  ActInstiter inst(s);

  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx;
    UserDef *ux;
    Process *px;
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
      if (g_apply_fn) {
	_any_global_conns (vx->connection());
      }
      continue;
    }

    it = vx->t;
    ux = dynamic_cast<UserDef *>(it->BaseType());
    
    if (ux) {
      px = dynamic_cast <Process *>(ux);
      /* set scope here */
      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	int idx = 0;

	while (!step->isend()) {
	  if (vx->isPrimary(idx)) {
	    push_name_array (vx->getName(), step->toArray());

	    /*-- process me --*/
	    if (px && g_apply_procfn) {
	      ActId *proc_inst = prefix_to_id (NULL);
	      (*g_apply_procfn) (g_cookie, proc_inst, px);
	      nullify_arrays (proc_inst);
	      delete proc_inst;
	    }
	    
	    _flat_scope (ux->CurScope());
	    pop_name ();
	  }
	  idx++;
	  step->step();
	}
	delete step;
      }
      else {
	push_name (vx->getName());

	/*-- process me --*/
	if (px && g_apply_procfn) {
	  ActId *proc_inst = prefix_to_id (NULL);
	  (*g_apply_procfn) (g_cookie, proc_inst, px);
	  nullify_arrays (proc_inst);
	  delete proc_inst;
	}
	
	_flat_scope (ux->CurScope());
	pop_name ();
      }
    }

    if (!g_apply_fn) continue;

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
	    if (ig != is_global_conn) continue; // only print global
	    // to global or
	    // non-global to non-global
	      
	    two = (*ci)->toid();
	    suffixes = list_new ();
	    suffix_array = list_new ();
	    _flat_rec_bool_conns (one, two, rux, it->arrayInfo(),
				  ((*ci)->vx ?
				   (*ci)->vx->t->arrayInfo() : NULL),
				  NULL);
	    list_free (suffixes);
	    list_free (suffix_array);
	    suffixes = NULL;
	    suffix_array = NULL;
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
	      if (c->hasDirectconnections (i) && c->isPrimary (i)) {
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
		  if (*ci == c->a[i]) continue;

		  ig = (*ci)->isglobal();
		  if (ig != is_global_conn) continue; 

		  two = (*ci)->toid();
		  if (TypeFactory::isUserType (xit)) {
		    suffixes = list_new ();
		    suffix_array = list_new ();
		    if (type == 1) {
		      _flat_rec_bool_conns (one, two, rux, NULL, NULL, NULL);
		    }
		    else {
		      _flat_rec_bool_conns (one, two, rux, xit->arrayInfo(),
					     (*ci)->getvx()->t->arrayInfo(),
					     NULL);
		    }
		    list_free (suffixes);
		    list_free (suffix_array);
		    suffixes = NULL;
		    suffix_array = NULL;
		  }
		  else if (TypeFactory::isBoolType (xit)) {
		    if (type == 1) {
		      _flat_single_connection (one, NULL,
						two, NULL,
						NULL, NULL, NULL);
		    }
		    else {
		      _flat_single_connection (one, xit->arrayInfo(),
						two,
						(*ci)->getvx()->t->arrayInfo(),
						NULL, NULL, NULL);
		    }
		  }
		  delete two;
		}
		delete one;
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
	_flat_connections_bool (vx);
      }
    }
  }
}


static void _flat_ns (ActNamespace *ns)
{
  /* sub-namespaces */
  ActNamespaceiter iter(ns);

  for (iter = iter.begin(); iter != iter.end(); iter++) {
    ActNamespace *t = *iter;

    push_namespace_name (t->getName());
    _flat_ns (t);
    pop_name ();
  }

  /* connections! */
  _flat_scope (ns->CurScope());
}


void act_flat_apply_conn_pairs (Act *a, void *cookie,
				void (*f)(void *, ActId *, ActId *))
{
  g_cookie = cookie;
  g_apply_fn = f;
  if (!f) return;
  prefixes = list_new ();
  prefix_array = list_new ();
  _flat_ns (a->Global());
  list_free (prefixes);
  list_free (prefix_array);
  g_cookie = NULL;
  g_apply_fn = NULL;
}

void act_flat_apply_processes (Act *a, void *cookie,
			       void (*f)(void *, ActId *, Process *p))
{
  g_cookie = cookie;
  g_apply_procfn = f;
  if (!f) return;
  prefixes = list_new ();
  prefix_array = list_new ();
  _flat_ns (a->Global());
  list_free (prefixes);
  list_free (prefix_array);
  g_cookie = NULL;
  g_apply_procfn = NULL;
}
