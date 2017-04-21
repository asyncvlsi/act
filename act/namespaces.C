/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <iostream>
#include <act/namespaces.h>
#include <act/types.h>
#include <act/inst.h>
#include <string.h>
#include "misc.h"

ActNamespace *ActNamespace::global = NULL;
int ActNamespace::creating_global = 0;

/**
 * Initialize namespaces. This function creates the ``Global''
 * namespace, and initializes static member function global
 */
void ActNamespace::Init ()
{
  if (global) {
    /* this function has already been called */
    return;
  }
  else {
    creating_global = 1;
    global = new ActNamespace ("Global");
    creating_global = 0;
  }
}
      
void ActNamespace::_init (ActNamespace *ns, const char *s)
{
  if (creating_global) {
    /* skip the checks */
  }
  else {
    ActNamespace::Init();
    if (!ns) {
      ns = global;
    }
    if (ns->findNS (s)) {
      fatal_error ("Cannot create duplicate namespace `%s'\n", s);
    }
  }
  N = hash_new (2);
  if (!creating_global) {
    Assert (ns, "This should not happen");
    Link (ns, s);
  }
  else {
    parent = ns;
  }
  T = hash_new (4);
  xT = hash_new (4);
  if (creating_global) {
    I = new Scope  (NULL);
  }
  else {
    I = new Scope(ns->CurScope());
  }
  B = NULL;
  exported = 0;
}

void ActNamespace::AppendBody (ActBody *b)
{
  if (B) { 
    B->Append (b); 
  } else { 
    B = b;
  }
}


/**
 * Create a new namespace within the specified namespace
 */
ActNamespace::ActNamespace (ActNamespace *ns, const char *s)
{
  _init (ns, s);
}

/**
 *  Create a new namespace within the global namespace
 */
ActNamespace::ActNamespace (const char *s)
{
  _init (NULL, s);
}


/**
 * Find a namespace in the current context
 */
ActNamespace *ActNamespace::findNS (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (N, s);
  if (b) {
    return (ActNamespace *)b->v;
  }
  else {
    return NULL;
  }
}


/**
 * Unlink namespace
 */
void ActNamespace::Unlink ()
{
  ActNamespace *ns = parent;

  Assert (ns, "No parent to this namespace?");
  hash_delete (ns->N, self_bucket->key);
  self_bucket = NULL;
  parent = NULL;
}


/**
 * Link namespace to parent
 */
void ActNamespace::Link (ActNamespace *up, const char *name)
{
  self_bucket = hash_add (up->N, name);
  self_bucket->v = this;
  parent = up;
}


/**
 * String name
 */
char *ActNamespace::Name ()
{
  int sz = 1, len;
  ActNamespace *ns;
  char *ret;

  ns = this;

  if (ns == global) {
    return Strdup ("::");
  }
  
  while (ns->parent) {
    sz += strlen (ns->self_bucket->key);
    sz += 2;
    ns = ns->parent;
  }

  MALLOC (ret, char, sz);
  ns = this;
  ret[sz-1] = '\0';
  sz--;
  while (ns->parent) {
    len = strlen (ns->self_bucket->key);
    strncpy (&ret[sz]-len, ns->self_bucket->key, len);
    sz -= len;
    ret[--sz] = ':';
    ret[--sz] = ':';
    ns = ns->parent;
  }
  Assert (sz == 0, "Eh?");
  return ret;
}

/**
 * Functions for class ActOpen
 *
 */

ActOpen::ActOpen ()
{
  search_path = list_new ();
}


ActOpen::~ActOpen ()
{
  list_free (search_path);
}


int ActOpen::Open(ActNamespace *ns, const char *newname)
{
  listitem_t *li;

  if (newname) {
    if (ActNamespace::Global()->findNS (newname)) {
      return 0;
    }
    ns->Unlink ();
    ns->Link (ActNamespace::Global(), newname);
  }
  else {
    /* add to the search path */
    for (li = list_first (search_path); li; li = list_next (li)) {
      if (list_value (li) == (void *)ns)
	return 1;
    }
    stack_push (search_path, ns);
  }
  return 1;
}


/**
 * Find namespace in the current context, given the context of opens
 *
 */
ActNamespace *ActOpen::find (ActNamespace *cur, const char *s)
{
  listitem_t *li;
  ActNamespace *ns;

  /* Search in the current namespace */
  if (cur) {
    ns = cur->findNS (s);
    if (ns) {
      return ns;
    }
  }

  /* Search in the path */
  Assert (search_path, "Constructor didn't get called?");
  for (li = list_first (search_path); li; li = list_next (li)) {
    ns = (ActNamespace *) list_value (li);
    Assert (ns, "What?");
    ns = ns->findNS (s);
    if (ns) {
      return ns;
    }
  }

  /* Look in the global namespace */
  ns = ActNamespace::Global()->findNS (s);

  return ns;
}

/**
 * Find namespace containing the type in the current context, given
 * the context of opens
 *
 */
ActNamespace *ActOpen::findType (ActNamespace *cur, const char *s)
{
  listitem_t *li;
  ActNamespace *ns;

  /* Search in the current namespace */
  while (cur) {
    if (cur->findType (s)) {
      return cur;
    }
    cur = cur->Parent ();
  }

  /* Search in the path */
  Assert (search_path, "Constructor didn't get called?");
  for (li = list_first (search_path); li; li = list_next (li)) {
    ns = (ActNamespace *) list_value (li);
    if (ns->findType (s)) {
      return ns;
    }
  }

  /* Look in the global namespace */
  if (ActNamespace::Global()->findType (s)) {
    return ActNamespace::Global();
  }
  
  return NULL;
}


/**
 * Find a type in the current namespace
 */
UserDef *ActNamespace::findType (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (T, s);
  if (b) {
    return (UserDef *)b->v;
  }
  else {
    return NULL;
  }
}


/**
 * Find an instance in the current namespace 
 */
InstType *ActNamespace::findInstance (const char *s)
{
  return I->Lookup (s);
}


/**
 * Create a new type
 */
int ActNamespace::CreateType (const char *s, UserDef *u)
{
  hash_bucket_t *b;

  b = hash_lookup (T, s);
  if (b) {
    /* failure: name already exists */
    return 0;
  }
  else {
    b = hash_add (T, s);
    b->v = u;
    u->setName (b->key);

    return 1;
  }
}


/**
 *  Check if a name is free
 */
int ActNamespace::findName (const char *s)
{
  if (findNS (s)) {
    return 2;
  }
  else if (findType (s)) {
    return 1;
  }
  else if (findInstance (s)) {
    return 3;
  }
  return 0;
}


Scope::Scope (Scope *parent, int is_expanded)
{
  expanded = is_expanded;
  H = hash_new (2);
  u = NULL;
  up = parent;
}

InstType *Scope::Lookup (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (H, s);
  if (b) {
    if (!expanded) {
      return (InstType *)b->v;
    }
    else {
      return ((ValueIdx *)b->v)->t;
    }
  }
  else {
    return NULL;
  }
}

InstType *Scope::FullLookup (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (H, s);
  if (b) {
    if (!expanded) {
      return (InstType *)b->v;
    }
    else {
      return ((ValueIdx *)b->v)->t;
    }
  }
  else {
    if (up) {
      return up->FullLookup (s);
    }
    return NULL;
  }
}


Scope::~Scope ()
{
  hash_free (H);
  H = NULL;
}

InstType *Scope::Lookup (ActId *id, int err)
{
  InstType *it;
  Scope *s;

  s = this;
  it = s->Lookup (id->getName ());
  if (!it) { 
    return NULL; 
  }
  if (err) {
    if (id->Rest()) {
      fatal_error ("Illegal call to Scope::Lookup() with dotted identifier");
    }
  }
  return it;
}

/**
 *  Add a new identifier to the scope.
 *
 *  @param s is a string corresponding to the identifier being added
 *  to the scope
 *  @param it is the instantiation type 
 *
 *  @return 0 on a failure, 1 on success.
 */
int Scope::Add (const char *s, InstType *it)
{
  hash_bucket_t *b;
  
  if (Lookup (s)) {
    /* failure */
    return 0;
  }

  b = hash_add (H, s);

  if (expanded == 0) {
    b->v = it;
  }
  else {
    ValueIdx *v = new ValueIdx;

    if (it->isExpanded() == 0) {
      fatal_error ("Scope::Add(): Scope is expanded, but instance type is not!");
    }
    
    v->t = it;
    v->init = 0;
    b->v = v;
  }
  return 1;
}

void Scope::Del (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (H, s);
  if (!b) {
    fatal_error ("Del() called, failed!");
  }
  if (expanded) {
    ValueIdx *v = (ValueIdx *)b->v;
    delete v;
  }
  hash_delete (H, s);
}

/**
 *  Flush the table
 */
void Scope::FlushExpand ()
{
  if (expanded) {
    int i;
    hash_bucket_t *b;
    ValueIdx *v;
    
    for (i=0; i < H->size; i++) {
      for (b = H->head[i]; b; b = b->next) {
	v = (ValueIdx *)b->v;
	delete v;
      }
    }
  }
  hash_clear (H);
  expanded = 1;
}

/**
 * Merge in instances from another scope
 */
void Scope::Merge (Scope *s)
{
  if (expanded && !s->expanded) {
    fatal_error ("Scope::Merge(): can't merge expanded scope into unexpanded one");
  }
  if (!expanded && s->expanded) {
    fatal_error ("Scope::Merge(): can't merge unexpanded scope into expanded one");
  }
  
  int i;
  hash_bucket_t *b, *tmp;

  for (i=0; i < s->H->size; i++) {
    for (b = s->H->head[i]; b; b = b->next) {
      if (hash_lookup (H, b->key)) {
	fatal_error ("Scope::Merge(): id `%s' already exists!", b->key);
      }
      tmp = hash_add (H, b->key);
      tmp->v = b->v;
    }
  }
}



/*------------------------------------------------------------------------
 *
 * Expand a namespace
 *
 *------------------------------------------------------------------------
 */
void ActNamespace::Expand ()
{
  ActBody *b;
  int i;
  ActNamespace *ns;
  hash_bucket_t *bkt;

  Assert (I, "No scope?");

  /* flush the scope, and re-create it! */
  I->FlushExpand ();

  /* Expand all namespaces that are nested within me */
  for (i=0; i < N->size; i++) {
    for (bkt = N->head[i]; bkt; bkt = bkt->next) {
      ns = (ActNamespace *) bkt->v;
      ns->Expand ();
    }
  }

  /* Expand body */
  for (b = B; b; b = b->Next ()) {
    b->Expand (this, I);
  }
}
