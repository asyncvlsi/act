/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <iostream>
#include <act/namespaces.h>
#include <act/types.h>
#include <act/inst.h>
#include <act/iter.h>
#include <act/act.h>
#include <string.h>
#include "misc.h"
#include "array.h"

ActNamespace *ActNamespace::global = NULL;
int ActNamespace::creating_global = 0;
Act *ActNamespace::act = NULL;

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
    global->I->setNamespace (global);
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
  if (creating_global) {
    I = new Scope  (NULL);
    I->setNamespace (this);
  }
  else {
    I = new Scope(ns->CurScope());
    I->setNamespace (this);
  }
  B = NULL;
  exported = 0;
  lang.chp = NULL;
  lang.hse = NULL;
  lang.prs = NULL;
  lang.spec = NULL;
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

int ActNamespace::EditType (const char *s, UserDef *u)
{
  hash_bucket_t *b;
  b = hash_lookup (T, s);
  if (!b) {
    return 0;
  }
  b->v = u;
  u->setName (b->key);
  return 1;
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
  ns = NULL;
  up = parent;

  A_INIT (vpint);
  A_INIT (vpints);
  A_INIT (vpreal);
  A_INIT (vptype);
  vpbool = NULL;

  vpint_set = NULL;
  vpints_set = NULL;
  vpreal_set = NULL;
  vptype_set = NULL;
  vpbool_set = NULL;
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

ValueIdx *Scope::LookupVal (const char *s)
{
  hash_bucket_t *b;

  if (!expanded) {
    return NULL;
  }

  b = hash_lookup (H, s);
  if (!b) {
    return NULL;
  }
  return (ValueIdx *)b->v;
}

ValueIdx *Scope::FullLookupVal (const char *s)
{
  ValueIdx *vx;

  vx = LookupVal (s);
  if (vx) {
    return vx;
  }
  if (up) {
    return up->FullLookupVal (s);
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
  /* free items from the hash table */
  hash_bucket_t *b;
  int i;

  if (expanded) {
    for (i=0; i < H->size; i++) {
      for (b = H->head[i]; b; b = b->next) {
	ValueIdx *vx = (ValueIdx *)b->v;
	/* delete connections too */
	delete vx;
      }
    }
  }
  hash_free (H);
  H = NULL;

  A_FREE (vpint);
  A_FREE (vpints);
  A_FREE (vpreal);
  A_FREE (vptype);
  if (vpbool) { bitset_free (vpbool); }
  if (vpint_set) { bitset_free (vpint_set); }
  if (vpints_set) { bitset_free (vpints_set); }
  if (vpreal_set) { bitset_free (vpreal_set); }
  if (vptype_set) { bitset_free (vptype_set); }
  if (vpbool_set) { bitset_free (vpbool_set); }
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
    v->a = NULL;
    v->array_spec = NULL;
    v->t = it;
    v->init = 0;
    if (getUserDef() == NULL) {
      v->global = getNamespace();
      v->immutable = 1;
    }
    else {
      v->global = NULL;
      v->immutable = 0;
    }
    b->v = v;
    if (!TypeFactory::isParamType (it->BaseType())) {
      v->u.obj.name = b->key;
      v->u.obj.c = NULL;
    }
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

    Assert (v->t, "Huh");
    
    if (!TypeFactory::isParamType (v->t->BaseType())) {
      if (v->u.obj.c) {
	warning ("Del() called, but object has a connection!");
      }
    }
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
	Assert (v->t, "Huh");
	if (!TypeFactory::isParamType (v->t->BaseType())) {
	  if (v->u.obj.c) {
	    warning ("FlushExpand() called, but object has a connection!");
	  }
	}
	delete v;
      }
    }
    A_FREE (vpint);
    A_FREE (vpints);
    A_FREE (vpreal);
    A_FREE (vptype);
    if (vpbool) { bitset_free (vpbool); }
    if (vpint_set) { bitset_free (vpint_set); }
    if (vpints_set) { bitset_free (vpints_set); }
    if (vpreal_set) { bitset_free (vpreal_set); }
    if (vptype_set) { bitset_free (vptype_set); }
    if (vpbool_set) { bitset_free (vpbool_set); }
  }
  hash_clear (H);
  expanded = 1;

  /* value storage */
  A_INIT (vpint);
  A_INIT (vpints);
  A_INIT (vpreal);
  A_INIT (vptype);
  vpbool = NULL;

  vpint_set = NULL;
  vpints_set = NULL;
  vpreal_set = NULL;
  vptype_set = NULL;
  vpbool_set = NULL;
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

  if (this == global) {
    act_error_push ("::<Global>", NULL, 0);
  }
  else {
    act_error_push (getName(), NULL, 0);
  }

  Assert (I, "No scope?");

  /* flush the scope, and re-create it! */
  I->FlushExpand ();

  /* 
     YYY: fixme

     This has to be expanded in the order it was encountered in the
     file!

     import globals;
     import foo;
     import bar;

     ... 
     defns

     more stuff
  */

  /* Expand any sub-namespaces */
  for (i=0; i < N->size; i++) {
    for (bkt = N->head[i]; bkt; bkt = bkt->next) {
      ns = (ActNamespace *)bkt->v;
      ns->Expand();
    }
  }

  /* Expand all meta parameters at the top level of the namespace. */
  if (B) {
    B->Expandlist (this, I);
  }

  act_error_pop ();
}


/*------------------------------------------------------------------------
 *
 *   Functions to handle parameter values in a scope
 *
 *------------------------------------------------------------------------
 */

/**----- pint -----**/

unsigned long Scope::AllocPInt(int count)
{
  unsigned long ret;
  if (count <= 0) {
    fatal_error ("Scope::AllocPInt(): count must be >0!");
  }
  A_NEWP (vpint, unsigned long, count);
  ret = A_LEN (vpint);
  if (!vpint_set) { vpint_set = bitset_new (count); }
  else {
    bitset_expand (vpint_set, ret + count);
  }
  A_LEN (vpint) += count;
  return ret;
}

void Scope::DeallocPInt (unsigned long idx, int count)
{
  if (count <= 0) {
    fatal_error ("Scope::DeallocPInt(): count must be >0!");
  }
  if ((idx+count) > A_LEN (vpint)) {
    fatal_error ("Scope::DeallocPInt(): out of range");
  }
  if (idx+count == A_LEN (vpint)) {
    /* we can actually deallocate this! */
    for (unsigned long i = idx; i < idx + count; i++) {
      bitset_clr (vpint_set, i);
    }
    A_LEN (vpint) -= count;
    
    return;
  }
  /* otherwise, ignore it. too bad! */
}

void Scope::setPInt(unsigned long id, unsigned long val)
{
#if 0
  fprintf (stderr, "[%x] set %d to %d\n", this, id, val);
#endif
  if (id >= A_LEN (vpint)) {
    fatal_error ("Scope::setPInt(): invalid identifier!");
  }
  vpint[id] = val;
  bitset_set (vpint_set, id);
}

int Scope::issetPInt(unsigned long id)
{
#if 0  
  fprintf (stderr, "[%x] check %d\n", this, id);
#endif  
  if (id >= A_LEN (vpint)) {
    fatal_error ("Scope::setPInt(): invalid identifier!");
  }
  return bitset_tst (vpint_set, id);
}

unsigned long Scope::getPInt(unsigned long id)
{
  if (id >= A_LEN (vpint)) {
    fatal_error ("Scope::setPInt(): invalid identifier!");
  }
  return vpint[id];
}

/**----- pints -----**/

unsigned long Scope::AllocPInts(int count)
{
  unsigned long ret;
  if (count <= 0) {
    fatal_error ("Scope::AllocPInts(): count must be >0!");
  }
  A_NEWP (vpints, long, count);
  ret = A_LEN (vpints);
  if (!vpints_set) { vpints_set = bitset_new (count); }
  else {
    bitset_expand (vpints_set, A_LEN (vpints)+count);
  }
  A_LEN (vpints) += count;
  return ret;
}

void Scope::DeallocPInts (unsigned long idx, int count)
{
  if (count <= 0) {
    fatal_error ("Scope::DeallocPInts(): count must be >0!");
  }
  if ((idx+count) > A_LEN (vpints)) {
    fatal_error ("Scope::DeallocPInts(): out of range");
  }
  if (idx+count == A_LEN (vpints)) {
    /* we can actually deallocate this! */
    for (unsigned long i = idx; i < idx + count; i++) {
      bitset_clr (vpints_set, i);
    }
    A_LEN (vpints) -= count;
    
    return;
  }
  /* otherwise, ignore it. too bad! */
}


void Scope::setPInts(unsigned long id, long val)
{
  if (id >= A_LEN (vpints)) {
    fatal_error ("Scope::setPInts(): invalid identifier!");
  }
  vpints[id] = val;
  bitset_set (vpints_set, id);
}

int Scope::issetPInts(unsigned long id)
{
  if (id >= A_LEN (vpints)) {
    fatal_error ("Scope::setPInts(): invalid identifier!");
  }
  return bitset_tst (vpints_set, id);
}

long Scope::getPInts(unsigned long id)
{
  if (id >= A_LEN (vpints)) {
    fatal_error ("Scope::setPInts(): invalid identifier!");
  }
  return vpints[id];
}

/**----- preal -----**/

unsigned long Scope::AllocPReal(int count)
{
  unsigned long ret;
  if (count <= 0) {
    fatal_error ("Scope::AllocPReal(): count must be >0!");
  }
  A_NEWP (vpreal, double, count);
  ret = A_LEN (vpreal);
  if (!vpreal_set) { vpreal_set = bitset_new (count); }
  else {
    bitset_expand (vpreal_set, A_LEN (vpreal)+count);
  }
  A_LEN (vpreal) += count;
  return ret;
}

void Scope::DeallocPReal (unsigned long idx, int count)
{
  if (count <= 0) {
    fatal_error ("Scope::DeallocPReal(): count must be >0!");
  }
  if ((idx+count) > A_LEN (vpreal)) {
    fatal_error ("Scope::DeallocPReal(): out of range");
  }
  if (idx+count == A_LEN (vpreal)) {
    /* we can actually deallocate this! */
    for (unsigned long i = idx; i < idx + count; i++) {
      bitset_clr (vpreal_set, i);
    }
    A_LEN (vpreal) -= count;
    return;
  }
  /* otherwise, ignore it. too bad! */
}


void Scope::setPReal(unsigned long id, double val)
{
  if (id >= A_LEN (vpreal)) {
    fatal_error ("Scope::setPPReal(): invalid identifier!");
  }
  vpreal[id] = val;
  bitset_set (vpreal_set, id);
}

int Scope::issetPReal(unsigned long id)
{
  if (id >= A_LEN (vpreal)) {
    fatal_error ("Scope::setPReal(): invalid identifier!");
  }
  return bitset_tst (vpreal_set, id);
}

double Scope::getPReal(unsigned long id)
{
  if (id >= A_LEN (vpreal)) {
    fatal_error ("Scope::setPReal(): invalid identifier!");
  }
  return vpreal[id];
}



/**----- ptype -----**/

unsigned long Scope::AllocPType(int count)
{
  unsigned long ret;
  if (count <= 0) {
    fatal_error ("Scope::AllocPType(): count must be >0!");
  }
  A_NEWP (vptype, InstType *, count);
  ret = A_LEN (vptype);
  if (!vptype_set) { vptype_set = bitset_new (count); }
  else {
    bitset_expand (vptype_set, A_LEN (vptype)+count);
  }
  A_LEN (vptype) += count;
  return ret;
}

void Scope::DeallocPType (unsigned long idx, int count)
{
  if (count <= 0) {
    fatal_error ("Scope::DeallocPType(): count must be >0!");
  }
  if ((idx+count) > A_LEN (vptype)) {
    fatal_error ("Scope::DeallocPReal(): out of range");
  }
  if (idx+count == A_LEN (vptype)) {
    /* we can actually deallocate this! */
    for (unsigned long i = idx; i < idx + count; i++) {
      bitset_clr (vptype_set, i);
    }
    A_LEN (vptype) -= count;
    return;
  }
  /* otherwise, ignore it. too bad! */
}

void Scope::setPType(unsigned long id, InstType *val)
{
  if (id >= A_LEN (vptype)) {
    fatal_error ("Scope::setPPType(): invalid identifier!");
  }
  vptype[id] = val;
  bitset_set (vptype_set, id);
}

int Scope::issetPType(unsigned long id)
{
  if (id >= A_LEN (vptype)) {
    fatal_error ("Scope::setPType(): invalid identifier!");
  }
  return bitset_tst (vptype_set, id);
}

InstType *Scope::getPType(unsigned long id)
{
  if (id >= A_LEN (vptype)) {
    fatal_error ("Scope::setPType(): invalid identifier!");
  }
  return vptype[id];
}


/**----- pbool -----**/

unsigned long Scope::AllocPBool(int count)
{
  if (count <= 0) {
    fatal_error ("Scope::AllocPBool(): count must be >0!");
  }
  if (!vpbool) {
    vpbool = bitset_new (count);
    vpbool_set = bitset_new (count);
    vpbool_len = count;
    return 0;
  }
  else {
    bitset_expand (vpbool, vpbool_len + count);
    bitset_expand (vpbool_set, vpbool_len + count);
    vpbool_len += count;
    return vpbool_len - count;
  }
}

void Scope::DeallocPBool (unsigned long idx, int count)
{
  if (count <= 0) {
    fatal_error ("Scope::DeallocPBool(): count must be >0!");
  }
  if ((idx+count) > vpbool_len) {
    fatal_error ("Scope::DeallocPBool(): out of range");
  }
  if (idx+count == vpbool_len) {
    /* we can actually deallocate this! */
    for (unsigned long i = idx; i < idx + count; i++) {
      bitset_clr (vpbool_set, i);
    }
    vpbool_len -= count;
    return;
  }
  /* otherwise, ignore it. too bad! */
}

void Scope::setPBool(unsigned long id, int val)
{
  if (id >= vpbool_len) {
    fatal_error ("Scope::setPBool(): invalid identifier!");
  }
  if (val) {
    bitset_set (vpbool, id);
  }
  else {
    bitset_clr (vpbool, id);
  }
  bitset_set (vpbool_set, id);
}

int Scope::issetPBool(unsigned long id)
{
  if (id >= vpbool_len) {
    fatal_error ("Scope::setPBool(): invalid identifier!");
  }
  return bitset_tst (vpbool_set, id);
}

int Scope::getPBool(unsigned long id)
{
  if (id >= vpbool_len) {
    fatal_error ("Scope::setPBool(): invalid identifier!");
  }
  return bitset_tst (vpbool, id);
}


/*
  tt has to be expanded
*/
void Scope::BindParam (const char *s, InstType *tt)
{
  /* get the ValueIdx for the parameter */
  int need_alloc = 0;
  ValueIdx *vx = LookupVal (s);

  Assert (vx, "should have checked this before");
  Assert (vx->t, "what?");

  if (!vx->init) {
    need_alloc = 1;
  }
  vx->init = 1;


  /* allocate space and bind it to a value */
  Assert (TypeFactory::isPTypeType (vx->t->BaseType()), "BindParam called with a Type, but needs a value");

  /* recall: no ptype arrays */
  Assert (!vx->t->arrayInfo(), "No ptype arrays?");

  if (need_alloc) {
    /* alloc */
    vx->u.idx = AllocPType();
  }
  if (tt) {
    if (vx->immutable && issetPType (vx->u.idx)) {
      act_error_ctxt (stderr);
      fatal_error ("Setting immutable parameter `%s' that has already been set", s);
    }
    /* assign */
    setPType (vx->u.idx, tt);
    Assert (getPType (vx->u.idx) == tt, "Huh?!");
    tt->MkCached ();
  }
}

void Scope::BindParam (ActId *id, InstType *tt)
{
  if (id->Rest() == NULL) {
    Assert (id->arrayInfo() == NULL, "No array ptypes please");
    BindParam (id->getName(), tt);
  }
  else {
    act_error_ctxt (stderr);
    fprintf (stderr, "Binding to a parameter that is in a different user-defined type");
    fprintf (stderr," ID: ");
    id->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }
}

void Scope::BindParam (ActId *id, AExprstep *aes, int idx)
{
  if (id->Rest() != NULL) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Binding to a parameter that is in a different user-defined type");
    fprintf (stderr," ID: ");
    id->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }
  
  ValueIdx *vx = LookupVal (id->getName());

  Assert (vx, "should have checked this before");
  Assert (vx->t, "what?");

  int need_alloc = 0;
  
  if (!vx->init) {
    need_alloc = 1;
  }
  vx->init = 1;

  Array *xa;

  /* compute the array, if any */
  unsigned int len;
  xa = vx->t->arrayInfo();
  if (xa) {
    len = xa->size();
  }
  else {
    len = 1;
  }
  
  if (need_alloc) {
    /* allocate */
    if (TypeFactory::isPIntType (vx->t->BaseType())) {
      vx->u.idx = AllocPInt(len);
    }
    else if (TypeFactory::isPIntsType (vx->t->BaseType())) {
      vx->u.idx = AllocPInts(len);
    }
    else if (TypeFactory::isPRealType (vx->t->BaseType())) {
      vx->u.idx = AllocPReal (len);
    }
    else if (TypeFactory::isPBoolType (vx->t->BaseType())) {
      vx->u.idx = AllocPBool (len);
    }
    else {
      Assert (0, "Should not be here!");
    }
  }

  int offset;

  if (id->arrayInfo()) {
    if (idx == -1) {
      if (!vx->t->arrayInfo()->Validate (id->arrayInfo())) {
	act_error_ctxt (stderr);
	fprintf (stderr, " id: ");
	id->Print (stderr);
	fprintf (stderr, "\n type: ");
	vx->t->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Dereference out of range");
      }

      offset = vx->t->arrayInfo()->Offset (id->arrayInfo());
    }
    else {
      offset = idx;
    }
  }
  else {
    offset = 0;
  }

#if 0
  fprintf (stderr, " id: ");
  id->Print (stderr);
  fprintf (stderr, "\n type: ");
  vx->t->Print (stderr);
  fprintf (stderr, "\n");

  printf ("check: id=%d, offset=%d\n", vx->u.idx, offset);
#endif  

  if (vx->immutable &&
      ((TypeFactory::isPIntType (vx->t->BaseType()) && issetPInt (vx->u.idx + offset))
       || (TypeFactory::isPIntsType (vx->t->BaseType()) && issetPInts (vx->u.idx + offset))
       || (TypeFactory::isPRealType (vx->t->BaseType()) && issetPReal (vx->u.idx + offset))
       || (TypeFactory::isPBoolType (vx->t->BaseType()) && issetPBool (vx->u.idx + offset)))
      ) {
    act_error_ctxt (stderr);
    fprintf (stderr, " Id: %s", id->getName());
    fprintf (stderr, "\n");
    fatal_error ("Setting immutable parameter that has already been set");
  }
  if (TypeFactory::isPIntType (vx->t->BaseType())) {
    setPInt (vx->u.idx + offset, aes->getPInt());
  }
  else if (TypeFactory::isPIntsType (vx->t->BaseType())) {
    setPInts (vx->u.idx + offset, aes->getPInts());
  }
  else if (TypeFactory::isPRealType (vx->t->BaseType())) {
    setPReal (vx->u.idx + offset, aes->getPReal());
  }
  else if (TypeFactory::isPBoolType (vx->t->BaseType())) {
    setPBool (vx->u.idx + offset, aes->getPBool());
  }
  else {
    Assert (0, "Should not be here");
  }
}

/*
  ae has to be expanded
*/
void Scope::BindParam (ActId *id, AExpr *ae)
{
#if 0
  fprintf (stderr, "Bind [scope=%x] ", this);
  id->Print (stderr);
  fprintf (stderr, " = ");
  ae->Print (stderr);
  fprintf (stderr, "\n");
#endif  
  
  /* get the ValueIdx for the parameter */
  if (id->Rest() != NULL) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Binding to a parameter that is in a different user-defined type");
    fprintf (stderr," ID: ");
    id->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }

  /* nothing to do here */
  if (!ae) return;
  
  int need_alloc = 0;
  int subrange_offset = 0;
  Array *subrange_info = id->arrayInfo();
  ValueIdx *vx = LookupVal (id->getName());

  Assert (vx, "should have checked this before");
  Assert (vx->t, "what?");

  if (!vx->init) {
    need_alloc = 1;
  }
  vx->init = 1;

  Assert (!TypeFactory::isPTypeType (vx->t->BaseType()), "Should not be a type!");

  InstType *xrhs;
  Array *xa;

  /* compute the array, if any */
  unsigned int len;
  xa = vx->t->arrayInfo();	
  if (xa) {
    len = xa->size();
  }
  else {
    len = 1;
  }

  /* x = expanded type of port parameter
     xa = expanded array field of x
     len = number of items
  */
  InstType *actual;
  
  AExpr *rhsval;
  AExprstep *aes;

  if (subrange_info) {
    actual = actual_insttype (this, id);
  }
  else {
    actual = vx->t;
  }
  
  if (ae) {
    xrhs = ae->getInstType (this, 1 /* expanded */);
    if (!type_connectivity_check (actual, xrhs)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Typechecking failed, ");
      actual->Print (stderr);
      fprintf (stderr, "  v/s ");
      xrhs->Print (stderr);
      fprintf (stderr, "\n\t%s\n", act_type_errmsg());
      exit (1);
    }
    rhsval = ae;
    aes = rhsval->stepper();
  }
  if (subrange_info) {
    delete actual;
    actual = NULL;
  }

  if (TypeFactory::isPIntType (vx->t->BaseType())) {
    unsigned long v;

    if (need_alloc) {
      vx->u.idx = AllocPInt(len); /* allocate */
    }

    if (ae) {
      if (xa) {
	/* identifier is of an array type */
	int idx;
	Arraystep *as;

	as = xa->stepper(subrange_info);

	while (!as->isend()) {
	  Assert (!aes->isend(), "This should have been caught earlier");
	  
	  idx = as->index();
	  v = aes->getPInt();

	  if (vx->immutable && issetPInt (vx->u.idx + idx)) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, " Id: %s", id->getName());
	    as->Print (stderr);
	    fprintf (stderr, "\n");
	    
	    fatal_error ("Setting immutable parameter that has already been set");
	  }
	  setPInt (vx->u.idx + idx, v);
	    
	  as->step();
	  aes->step();
	}
	Assert (aes->isend(), "What on earth?");
	delete as;
      }
      else {
	aes = rhsval->stepper();
	if (vx->immutable && issetPInt (vx->u.idx)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " Id: %s\n", id->getName());
	  fatal_error ("Setting immutable parameter that has already been set");
	}
	setPInt (vx->u.idx, aes->getPInt());
	aes->step();
	Assert (aes->isend(), "This should have been caught earlier");
      }
    }
  }
  else if (TypeFactory::isPIntsType (vx->t->BaseType())) {
    long v;

    if (need_alloc) {
      vx->u.idx = AllocPInts(len); /* allocate */
    }

    if (ae) {
      if (xa) {		/* array assignment */
	int idx;
	Arraystep *as;
	  
	as = xa->stepper(subrange_info);

	while (!as->isend()) {
	  Assert (!aes->isend(), "This should have been caught earlier");

	  idx = as->index();
	  v = aes->getPInts();

	  if (vx->immutable && issetPInts (vx->u.idx + idx)) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, " Id: %s", id->getName());
	    as->Print (stderr);
	    fprintf (stderr, "\n");
	    
	    fatal_error ("Setting immutable parameter that has already been set");
	  }
	  
	  setPInts (vx->u.idx + idx, v);
	    
	  as->step();
	  aes->step();
	}
	Assert (aes->isend(), "What on earth?");
	delete as;
      }
      else {
	aes = rhsval->stepper();
	if (vx->immutable && issetPInts (vx->u.idx)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " Id: %s\n", id->getName());
	  fatal_error ("Setting immutable parameter that has already been set");
	}
	setPInts (vx->u.idx, aes->getPInts());
	aes->step();
	Assert (aes->isend(), "This should have been caught earlier");
      }
    }
  }
  else if (TypeFactory::isPRealType (vx->t->BaseType())) {
    double v;

    if (need_alloc) {
      vx->u.idx = AllocPReal(len); /* allocate */
    }

    if (ae) {
      if (xa) {		/* array assignment */
	int idx;
	Arraystep *as;
	  
	as = xa->stepper(subrange_info);

	while (!as->isend()) {
	  Assert (!aes->isend(), "This should have been caught earlier");
	  
	  idx = as->index();
	  v = aes->getPReal();

	  if (vx->immutable && issetPReal (vx->u.idx + idx)) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, " Id: %s", id->getName());
	    as->Print (stderr);
	    fprintf (stderr, "\n");
	    
	    fatal_error ("Setting immutable parameter that has already been set");
	  }
	  
	  setPReal (vx->u.idx + idx, v);
	    
	  as->step();
	  aes->step();
	}
	Assert (aes->isend(), "What on earth?");
	delete as;
      }
      else {
	aes = rhsval->stepper();

	if (vx->immutable && issetPReal (vx->u.idx)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " Id: %s\n", id->getName());
	  fatal_error ("Setting immutable parameter that has already been set");
	}
	
	setPReal (vx->u.idx, aes->getPReal());
	aes->step();
	Assert (aes->isend(), "This should have been caught earlier");
      }
    }
  }
  else if (TypeFactory::isPBoolType (vx->t->BaseType())) {
    int v;

    if (need_alloc) {
      vx->u.idx = AllocPBool(len); /* allocate */
    }

    if (ae) {
      if (xa) {		/* array assignment */
	int idx;
	Arraystep *as;
	  
	as = xa->stepper(subrange_info);

	while (!as->isend()) {
	  Assert (!aes->isend(), "This should have been caught earlier");

	  idx = as->index();
	  v = aes->getPBool();

	  if (vx->immutable && issetPBool (vx->u.idx + idx)) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, " Id: %s", id->getName());
	    as->Print (stderr);
	    fprintf (stderr, "\n");
	    
	    fatal_error ("Setting immutable parameter that has already been set");
	  }
	  
	  setPBool (vx->u.idx + idx, v);
	    
	  as->step();
	  aes->step();
	}
	Assert (aes->isend(), "What on earth?");
	delete as;
      }
      else {
	aes = rhsval->stepper();

	if (vx->immutable && issetPBool (vx->u.idx)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " Id: %s\n", id->getName());
	  fatal_error ("Setting immutable parameter that has already been set");
	}
	
	setPBool (vx->u.idx, aes->getPBool());
	aes->step();
	Assert (aes->isend(), "This should have been caught earlier");
      }
    }
  }
  else {
    fatal_error ("Should not be here: meta params only");
  }
  if (ae) {
    delete aes;
    delete xrhs;
  }
}


/*
  ae has to be expanded
*/
void Scope::BindParam (const char *s, AExpr *ae)
{
  /* get the ValueIdx for the parameter */
  ActId *tmpid = new ActId (s, NULL);
  BindParam (tmpid, ae);
  delete tmpid;
}


void Scope::Print (FILE *fp)
{
  char buf[10240];
  UserDef *u = getUserDef ();
  fprintf (fp, "\n/* instances */\n");
  
  ActInstiter inst(this);
  
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;

    if (!u || (u->FindPort (vx->getName()) == 0)) {
      if (!TypeFactory::isParamType (vx->t)) {
	if (vx->t->isExpanded()) {
	  vx->t->sPrint (buf, 10240);
	  ActNamespace::Act()->mfprintf (fp, "%s", buf);
	}
	else {
	  vx->t->Print (fp);
	}
	fprintf (fp, " %s;\n", vx->getName());
      }
    }
    /* fix this.
     * connections should be reported only once
     * subconnections should be reported 
     */
    if (vx->hasConnection()) {
      ActConniter ci(vx->connection());
      ActId *id;

      if (ci.begin() != ci.end() && (++ci.begin() != ci.end())) {

      fprintf (fp, "%s", vx->getName());
      for (ci = ci.begin(); ci != ci.end(); ci++) {
	act_connection *c = *ci;
	if (c == vx->connection()) continue;
	id = c->toid();
	fprintf (fp, "=");
	id->Print (fp);
	delete id;
      }
      fprintf (fp, ";\n");
      }

      if (vx->hasSubconnections()) {
	/* do something! */
      }
    }
  }
}




/*
  Print namespace
*/
void ActNamespace::Print (FILE *fp)
{
  if (this != ActNamespace::Global()) {
    fprintf (fp, "%s {\n", getName());
  }

  /* print subnamespaces */
  ActNamespaceiter i(this);
  for (i = i.begin(); i != i.end(); i++) {
    ActNamespace *n = *i;
    n->Print (fp);
  }

  /* print types */
  ActTypeiter it(this);
  for (it = it.begin(); it != it.end(); it++) {
    Type *t = *it;
    UserDef *u = dynamic_cast<UserDef *>(t);
    Assert (u, "Hmm...");
    /* print type! */
    u->Print (fp);
  }

  /* print instances */
  CurScope()->Print (fp);
  
  if (this != ActNamespace::Global()) {
    fprintf (fp, "}\n");
  }
}
