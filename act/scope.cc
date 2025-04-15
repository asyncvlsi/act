/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2019 Rajit Manohar
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
#include <act/act.h>
#include <act/iter.h>
#include <iterator>
#include <set>
#include <string.h>
#include <vector>

int _act_is_reserved_id (const char *s);

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
  A_INIT (vpstruct);

  vpint_set = NULL;
  vpints_set = NULL;
  vpreal_set = NULL;
  vptype_set = NULL;
  vpbool_set = NULL;

  is_function = 0;
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

ValueIdx *Scope::FullLookupValSc (const char *s, Scope **sc)
{
  ValueIdx *vx;

  vx = LookupVal (s);
  if (vx) {
    *sc = this;
    return vx;
  }
  if (up) {
    return up->FullLookupValSc (s, sc);
  }
  else {
    *sc = NULL;
    return NULL;
  }
}


int Scope::isGlobal (const char *s)
{
  hash_bucket_t *b;
  Scope *cur = this;

  do {
    b = hash_lookup (cur->H, s);
    if (b) {
      if (cur->getUserDef()) {
	return 0;
      }
      return 1;
    }
    cur = cur->up;
  } while (cur);
  return 0;
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
  hash_iter_t iter;

  if (expanded) {
    hash_iter_init (H, &iter);
    while ((b = hash_iter_next (H, &iter))) {
      ValueIdx *vx = (ValueIdx *)b->v;
      /* delete connections too */
      delete vx;
    }
  }
  hash_free (H);
  H = NULL;

  A_FREE (vpint);
  A_FREE (vpints);
  A_FREE (vpreal);
  A_FREE (vptype);
  A_FREE (vpstruct);
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

  if (id->isNamespace()) {
    s = id->getNamespace()->CurScope();
    id = id->Rest();
  }
  else {
    s = this;
  }
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

#if 0
  fprintf (stderr, "Add ");
  if (u) { fprintf (stderr, "%s ", u->getName()); }
  fprintf (stderr, "[scope=%x ex:%d] %s ", this, expanded, b->key);
  fprintf (stderr, " <> ");
  it->Print (stderr);
  fprintf (stderr, "\n");
#endif  
  
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
	if (v->hasAnyConnection()) {
  	   warning ("Del() called, but object has a connection!");
	}
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
    hash_bucket_t *b;
    hash_iter_t iter;
    ValueIdx *v;

    hash_iter_init (H, &iter);
    while ((b = hash_iter_next (H, &iter))) {
      v = (ValueIdx *)b->v;
      Assert (v->t, "Huh");
      if (!TypeFactory::isParamType (v->t->BaseType())) {
	if (v->u.obj.c) {
	  warning ("FlushExpand() called, but object has a connection!");
	}
      }
      delete v;
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
  
  hash_bucket_t *b, *tmp;
  hash_iter_t iter;

  hash_iter_init (s->H, &iter);
  while ((b = hash_iter_next (s->H, &iter))) {
    if (hash_lookup (H, b->key)) {
      fatal_error ("Scope::Merge(): id `%s' already exists!", b->key);
    }
    tmp = hash_add (H, b->key);
    tmp->v = b->v;
  }
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
  A_LEN_RAW (vpint) += count;
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
    A_LEN_RAW (vpint) -= count;
    
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
  A_LEN_RAW (vpints) += count;
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
    A_LEN_RAW (vpints) -= count;
    
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
  A_LEN_RAW (vpreal) += count;
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
    A_LEN_RAW (vpreal) -= count;
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
  A_LEN_RAW (vptype) += count;
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
    A_LEN_RAW (vptype) -= count;
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
  return bitset_tst (vpbool, id) ? 1 : 0;
}

/**----- pstruct -----**/

unsigned long Scope::AllocPStruct(PStruct *ps, int count)
{
  unsigned long ret;
  if (count <= 0) {
    fatal_error ("Scope::AllocPStruct(): count must be >0!");
  }
  A_NEWP (vpstruct, Scope::pstruct, count);
  ret = A_LEN (vpstruct);

  // now we need to also allocate all the aux fields
  int pb, pi, pr, pt;
  Assert (ps, "Scope::AllocPStruct() called without a pstruct??");
  ps->getCounts (&pb, &pi, &pr, &pt);
  unsigned long curpb, curpi, curpr, curpt;

  /* allocate the actual parameters in this scope */
  if (pb > 0) {
    curpb = AllocPBool (pb*count);
  }
  else {
    curpb = 0;
  }
  if (pi > 0) {
    curpi = AllocPInt (pi*count);
  }
  else {
    curpi = 0;
  }
  if (pr > 0) {
    curpr = AllocPReal (pr*count);
  }
  else {
    curpr = 0;
  }
  if (pt > 0) {
    curpt = AllocPType (pt*count);
  }
  else {
    curpt = 0;
  }

  /* populate the pstruct offsets */
  for (unsigned long i=0; i < count; i++) {
    if (pb > 0) {
      vpstruct[ret+i].b_off = curpb;
      curpb += pb;
    }
    else {
      vpstruct[ret+i].b_off = 0;
    }
    if (pi > 0) {
      vpstruct[ret+i].i_off = curpi;
      curpi += pi;
    }
    else {
      vpstruct[ret+i].i_off = 0;
    }
    if (pr > 0) {
      vpstruct[ret+i].r_off = curpr;
      curpr += pr;
    }
    else {
      vpstruct[ret+i].r_off = 0;
    }
    if (pt > 0) {
      vpstruct[ret+i].t_off = curpt;
      curpt += pt;
    }
    else {
      vpstruct[ret+i].t_off = 0;
    }
  }
  A_LEN_RAW (vpstruct) += count;
  return ret;
}

Scope::pstruct Scope::getPStruct(unsigned long id)
{
  if (id >= A_LEN (vpstruct)) {
    fatal_error ("Scope::getPStruct(): invalid identifier!");
  }
  return vpstruct[id];
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
    if (id->isNamespace()) {
      Scope *sc = id->getNamespace()->CurScope();
      sc->BindParam (id->Rest(), tt);
      return;
    }
    ValueIdx *vx = LookupVal (id->getName());
    Assert (vx, "Should have been caught earlier?");
    if (!TypeFactory::isPStructType(vx->t)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Binding to a parameter that is in a different user-defined type");
      fprintf (stderr," ID: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
    PStruct *ps = dynamic_cast<PStruct *> (vx->t->BaseType());
    Assert (ps, "Hmm");
    int pb, pi, pr, pt;
    if (!ps->getOffset (id->Rest(), &pb, &pi, &pr, &pt)) {
      Assert (0, "Should have been caught earlier!");
    }
    if (pt < 0 || (pb >= 0 || pi >= 0 || pr >= 0)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Binding a type to a non-type field");
      fprintf (stderr, " ID: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
    if (!vx->init) {
      vx->init = 1;
      int count;
      if (vx->t->arrayInfo()) {
	count = vx->t->arrayInfo()->size();
      }
      else {
	count = 1;
      }
      vx->u.idx = AllocPStruct (ps, count);
    }
    int aoff;
    if (id->arrayInfo()) {
      aoff = vx->t->arrayInfo()->Offset (id->arrayInfo());
    }
    else {
      aoff = 0;
    }
    if (vx->immutable && issetPType (vpstruct[vx->u.idx + aoff].t_off + pt)) {
      act_error_ctxt (stderr);
      fprintf (stderr, " Id: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Setting immutable parameter that has already been set");
    }
    setPType (vpstruct[vx->u.idx + aoff].t_off + pt, tt);
    Assert (getPType (vpstruct[vx->u.idx + aoff].t_off + pt) == tt, "What?");
    tt->MkCached ();
  }
}

void Scope::BindParam (ActId *id, AExprstep *aes, int idx)
{
  if (id->Rest() != NULL) {
    if (id->isNamespace()) {
      Scope *sc = id->getNamespace()->CurScope();
      sc->BindParam (id->Rest(), aes, idx);
      return;
    }
    ValueIdx *vx = LookupVal (id->getName());
    Assert (vx, "Should have been caught earlier?");
    if (!TypeFactory::isPStructType(vx->t)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Binding to a parameter that is in a different user-defined type");
      fprintf (stderr," ID: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
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
    else if (TypeFactory::isPStructType (vx->t->BaseType())) {
      vx->u.idx = AllocPStruct (dynamic_cast<PStruct *>(vx->t->BaseType()), len);
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
    if (idx == -1) {
      offset = 0;
    }
    else {
      offset = idx;
    }
  }

#if 0
  fprintf (stderr, " id: ");
  id->Print (stderr);
  fprintf (stderr, "\n type: ");
  vx->t->Print (stderr);
  fprintf (stderr, "\n");

  printf ("check: id=%d, offset=%d, idx=%d\n", vx->u.idx, offset, idx);
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
  else if (TypeFactory::isPStructType (vx->t->BaseType())) {
    Scope::pstruct val = getPStruct (vx->u.idx + offset);

    /* we've taken care of the top-level array already */
    
    PStruct *ps = dynamic_cast<PStruct *> (vx->t->BaseType());
    int nb, ni, nr, nt;
    if (!ps->getOffset (id->Rest(), &nb, &ni, &nr, &nt)) {
      act_error_ctxt (stderr);
      fprintf (stderr, " Id: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Could not find non-array field in pstruct definition");
    }
    InstType *it = NULL;
    Assert (act_type_var (this, id, &it) != T_ERR, "Typecheck err?");
    if (vx->immutable &&
	((TypeFactory::isPIntType (it) && issetPInt (val.i_off + ni)) ||
	 (TypeFactory::isPBoolType (it) && issetPBool (val.b_off + nb)) ||
	 (TypeFactory::isPRealType (it) && issetPReal (val.r_off + nr)) ||
	 (TypeFactory::isPTypeType (it) && issetPType (val.t_off + nt)))) {
      act_error_ctxt (stderr);
      fprintf (stderr, " Id: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Setting immutable parameter that has already been set");
    }
    if (TypeFactory::isPIntType (it)) {
      setPInt (val.i_off + ni, aes->getPInt());
    }
    else if (TypeFactory::isPBoolType (it)) {
      setPBool (val.b_off + nb, aes->getPBool());
    }
    else if (TypeFactory::isPRealType (it)) {
      setPReal (val.r_off + nr, aes->getPReal());
    }
    else if (TypeFactory::isPTypeType (it)) {
      setPType (val.i_off + nt, aes->getPType());
    }
    else if (TypeFactory::isPStructType (it)) {
      struct expr_pstruct *v = aes->getPStruct();
      
      // we are assigning to the following pstruct
      PStruct *pps = dynamic_cast<PStruct *> (it->BaseType());
      Assert (pps, "Hmm");
      int cb, ci, cr, ct;
      pps->getCounts (&cb, &ci, &cr, &ct);
      for (int i=0; i < cb; i++) {
	if (vx->immutable && issetPBool (val.b_off + nb + i)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " Id: ");
	  id->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Setting immutable pbool parameter @ %d that has already been set", i);
	}
	setPBool (val.b_off + nb + i, v->pbool[i]);
      }
      for (int i=0; i < ci; i++) {
	if (vx->immutable && issetPInt (val.i_off + ni + i)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " Id: ");
	  id->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Setting immutable pint parameter @ %d that has already been set", i);
	}
	setPInt (val.i_off + ni + i, v->pint[i]);
      }
      for (int i=0; i < cr; i++) {
	if (vx->immutable && issetPReal (val.r_off + nr + i)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " Id: ");
	  id->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Setting immutable preal parameter @ %d that has already been set", i);
	}
	setPReal (val.r_off + nr + i, v->preal[i]);
      }
      for (int i=0; i < ct; i++) {
	if (vx->immutable && issetPType (val.t_off + nt + i)) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " Id: ");
	  id->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Setting immutable ptype parameter @ %d that has already been set", i);
	}
	setPType (val.t_off + nt + i, (InstType *)v->ptype[i]);
      }
    }
    else {
      Assert (0, "FIXME!");
    }
  }
  else {
    Assert (0, "Should not be here");
  }
}


void Scope::BindParamFull (ActId *id, AExprstep *aes, int idx)
{
  if (id->Rest() != NULL) {
    if (id->isNamespace()) {
      Scope *sc = id->getNamespace()->CurScope();
      sc->BindParamFull (id->Rest(), aes, idx);
      return;
    }
    ValueIdx *vx = FullLookupVal (id->getName());
    if (vx && !TypeFactory::isPStructType(vx->t)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Binding to a parameter that is in a different user-defined type");
      fprintf (stderr," ID: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  Scope *nsc;
  ValueIdx *vx = FullLookupValSc (id->getName(), &nsc);
  Assert (vx, "should have checked this before");

  nsc->BindParam (id, aes, idx);
}

void Scope::BindParamFull (ActId *id, AExpr *ae)
{
  if (id->Rest() != NULL) {
    if (id->isNamespace()) {
      Scope *sc = id->getNamespace()->CurScope();
      sc->BindParamFull (id->Rest(), ae);
      return;
    }
    ValueIdx *vx = FullLookupVal (id->getName());
    if (vx && !TypeFactory::isPStructType(vx->t)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Binding to a parameter that is in a different user-defined type");
      fprintf (stderr," ID: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  Scope *nsc;
  ValueIdx *vx = FullLookupValSc (id->getName(), &nsc);
  Assert (vx, "should have checked this before");

  nsc->BindParam (id, ae);
}

void Scope::BindParamFull (ActId *id, InstType *tt)
{
  if (id->Rest() != NULL) {
    if (id->isNamespace()) {
      Scope *sc = id->getNamespace()->CurScope();
      sc->BindParamFull (id->Rest(), tt);
      return;
    }
    ValueIdx *vx = FullLookupVal (id->getName());
    if (vx && !TypeFactory::isPStructType(vx->t)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Binding to a parameter that is in a different user-defined type");
      fprintf (stderr," ID: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  Scope *nsc;
  ValueIdx *vx = FullLookupValSc (id->getName(), &nsc);
  Assert (vx, "should have checked this before");

  nsc->BindParam (id, tt);
}


/*
  ae has to be expanded
*/
void Scope::BindParam (ActId *id, AExpr *ae)
{
#if 0
  fprintf (stderr, "Bind ");
  if (u) { fprintf (stderr, "%s ", u->getName()); }
  fprintf (stderr, "[scope=%x] ", this);
  id->Print (stderr);
  fprintf (stderr, " = ");
  ae->Print (stderr);
  fprintf (stderr, "\n");
#endif  
  
  /* get the ValueIdx for the parameter */
  if (id->Rest() != NULL) {
    if (id->isNamespace()) {
      id->getNamespace()->CurScope()->BindParam (id->Rest(), ae);
      return;
    }
    ValueIdx *vx = FullLookupVal (id->getName());
    if (vx && !TypeFactory::isPStructType(vx->t)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Binding to a parameter that is in a different user-defined type");
      fprintf (stderr," ID: ");
      id->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
  }

  /* nothing to do here */
  if (!ae) return;
  
  int need_alloc = 0;
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
  AExprstep *aes = NULL;

  if (subrange_info) {
    actual = act_actual_insttype (this, id, NULL);
  }
  else {
    actual = vx->t;
  }
  
  if (ae) {
    xrhs = ae->getInstType (this, NULL, 1 /* expanded */);
    if (type_connectivity_check (actual, xrhs) != 1) {
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
  else if (TypeFactory::isPStructType (vx->t->BaseType())) {

    if (need_alloc) {
      vx->u.idx = AllocPStruct (dynamic_cast<PStruct *>(vx->t->BaseType()), len);
    }

    // XXX: pstruct fixme
    Assert (0, "FIXME!");

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


void Scope::printConnections (FILE *fp, act_connection *cx, bool force)
{
  ActConniter ci(cx);
  ActId *id;
  int first = 1;
  int global;

  global = cx->isglobal();
  if (cx->isPrimary() || force) {
    if (ci.begin() != ci.end() && (++ci.begin() != ci.end())) {
      ActId *idfirst = NULL;
      for (ci = ci.begin(); ci != ci.end(); ci++) {
	act_connection *c = *ci;

	if (!global || c->isglobal()) {
	  id = c->toid();
	  if (first) {
	    idfirst = id;
	    first = 0;
	  }
	  else {
	    idfirst->Print (fp);
	    fprintf (fp, "=");
	    id->Print (fp);
	    fprintf (fp, ";");
	    delete id;
	  }
	}
      }
      if (idfirst) {
	delete idfirst;
	fprintf (fp, "\n");
      }
    }
  }
  else {
    if (!global && cx->primary()->isglobal()) {
	id = cx->toid();
	id->Print (fp);
        delete id;
        id = cx->primary()->toid();
        fprintf (fp, "=");
	id->Print (fp);
        delete id;
        fprintf (fp, ";\n");
    }
  }
  if (cx->hasSubconnections()) {
    for (int i=0; i < cx->numSubconnections(); i++) {
      if (cx->hasSubconnections(i)) {
	printConnections (fp, cx->getsubconn (i, cx->numSubconnections()), force);
      }
    }
  }
}


void Scope::Print (FILE *fp, bool all_inst)
{
  char buf[10240];

  // we can only print expanded scopes
  if (!expanded)
    return;
  
  fprintf (fp, "\n/* instances */\n");
  struct Hashtable *H = NULL;
  UserDef *u = getUserDef ();

  // get parent symbols
  if (!all_inst && u && u->getParent()) {
    if (TypeFactory::isUserType (u->getParent())) {
      UserDef *up = dynamic_cast<UserDef *> (u->getParent()->BaseType());
      H = hash_new (8);
      ActInstiter iparent(up->CurScope());
      for (iparent = iparent.begin(); iparent != iparent.end(); iparent++) {
        ValueIdx *vx = *iparent;
        if (TypeFactory::isParamType (vx->t)) continue;
        hash_add (H, vx->getName());
      }
    }
  }
  
  ActInstiter inst(this);
  
  // go through all contents of this scope
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;
    Array *a;

    // skip parameters
    if (TypeFactory::isParamType (vx->t)) continue;

    // skip internally generated names
    if (_act_is_reserved_id (vx->getName())) continue;
    
    // skip instances from parent
    if (H && hash_lookup (H, vx->getName())) continue;
    
    // skip ports
    if (u && (u->FindPort (vx->getName()) != 0)) continue;

    Array *ta;
    char *ns_name;
    struct act_attr *aa;

    ns_name = NULL;

    // if the current element has array info, buffer it and remove for printing
    a = vx->t->arrayInfo();
    if (a) {
      vx->t->clrArray();
    }
    ta = a;

    // get the list of attributes for this element
    aa = vx->getAttr();

    // if the element is in a namespace other than global, save that to ns_name
    if (TypeFactory::isUserType (vx->t)) {
      UserDef *u = dynamic_cast<UserDef *> (vx->t->BaseType());
      ActNamespace *ns;
      Assert(u, "What?");
      ns = u->getns();
      Assert (ns, "Hmm");
      if (ns && ns != ActNamespace::Global() && ns != getNamespace()) {
        ns_name = ns->Name();
      }
    }

    // we loop over all array elements
    // if there is no array, we still need to run this at least once
    do {
      //print the namespace if applicable
      if (ns_name) {
        fprintf (fp, "%s::", ns_name);
      }

      // if the type of the element is fully expanded
      if (vx->t->isExpanded()) {
        // if the type is primitive, print the info
        if (!TypeFactory::isUserType (vx->t)) {
          vx->t->sPrint (buf, 10240, 0);
          fprintf (fp, "%s", buf);
        }
        // otherwise print the user type name
        else {
          UserDef *tmpu = dynamic_cast<UserDef *>(vx->t->BaseType());
          Assert (tmpu, "Hmm");
          ActNamespace::Act()->mfprintfproc (fp, tmpu, 1);
        }

        // print the instance name
        fprintf (fp, " %s", vx->getName());
      }
      // if the scope of the element is not expanded
      else {
        // print type and instance name
        vx->t->Print (fp, 1);
        fprintf (fp, " %s", vx->getName());
      }

      // if there is indeed array data attached to this element, print the index
      // and advance in the array
      if (ta) {
        ta->PrintOne (fp);
        ta = ta->Next();
      }

      // if there is still data left in the array and there are attributes, print them now
      if (ta) {
        if (aa) {
          fprintf (fp, " @ ");
          act_print_attributes (fp, aa);
        }
        fprintf (fp, ";\n");
      }

    } while (ta);

    // finally, print the attributes (necessary for non-array elements)
    if (aa) {
      fprintf (fp, " @ ");
      act_print_attributes (fp, aa);
    }

    fprintf (fp, ";\n");
    
    // if there is array info, it was removed before. reinstate it.
    if (a) {
      vx->t->MkArray (a);
    }

    // if there was a namespace name, free that memory
    if (ns_name) {
      FREE (ns_name);
    }

    // if there are array specific attributes, print them now
    if (vx->haveAttrIdx()) {
      for (int i=0; i < vx->numAttrIdx(); i++) {
        aa = vx->getAttrIdx (i);
        if (aa) {
          Array *tmp;
          Assert (a, "Hmm");
          fprintf (fp, " %s", vx->getName());
          tmp = a->unOffset (i);
          tmp->Print (fp);
          delete tmp;
          fprintf (fp, " @ ");
          act_print_attributes (fp, aa);
          fprintf (fp, ";\n");
        }
      }
    }
  }

  // free the parent signal list
  if (H) {
    hash_free (H);
    H = NULL;
  }
  
  fprintf (fp, "\n/* connections */\n");
  
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;

    if (TypeFactory::isParamType (vx->t))
      continue;
    
    /* fix this.
     * connections should be reported only once
     * subconnections should be reported 
     */
    if (!TypeFactory::isParamType (vx->t) && vx->hasConnection()) {
      Scope::printConnections (fp, vx->connection());
    }
  }

}

void Scope::playBody (ActBody *b)
{
  for (; b; b = b->Next()) {
    ActBody_Inst *inst = dynamic_cast<ActBody_Inst *> (b);
    if (inst) {
      if (!Lookup (inst->getName())) {
	Add (inst->getName(), inst->getType());
      }
    }
    else {
      if (dynamic_cast<ActBody_Loop *>(b)) {
	ActBody_Loop *l = dynamic_cast<ActBody_Loop *>(b);
	playBody (l->getBody());
      }
      else {
	ActBody_Select_gc *sel;
	if (dynamic_cast<ActBody_Select *>(b)) {
	  sel = dynamic_cast<ActBody_Select *>(b)->getGC();
	}
	else if (dynamic_cast<ActBody_Genloop *>(b)) {
	  sel = dynamic_cast<ActBody_Genloop *>(b)->getGC();
	}
	else {
	  sel = NULL;
	}
	while (sel) {
	  playBody (sel->getBody());
	  sel = sel->getNext();
	}
      }
    }
  }
}


void Scope::refineBaseType (const char *s, InstType *u)
{
  hash_bucket_t *b;

  b = hash_lookup (H, s);
  if (!b) return;
  if (!expanded) {
    InstType *x = (InstType *)b->v;
    b->v = x->refineBaseType (u);
  }
  else {
    //Assert (0, "Should not be here!");
    ValueIdx *vx = (ValueIdx *)b->v;
    vx->t = vx->t->refineBaseType (u);
  }
}

const char *Scope::getName ()
{
  if (u) {
    return u->getName();
  }
  if (ns) {
    return (ns == ActNamespace::Global()) ? "-global-" : ns->getName();
  }
  return "-unknown-";
}

const std::set<const char*> Scope::getParentSymbols ()
{
  UserDef *u_def = getUserDef ();
  std::set<const char*> parent_visible;

  // if the current instance has a parent
  // we need this info to filter out signals that are also visible in the parent
  if (
    u_def && 
    u_def->getParent() && 
    TypeFactory::isUserType (u_def->getParent())
  ) {
    // get the base type of the parent
    UserDef *parent = dynamic_cast<UserDef *> (u_def->getParent()->BaseType());
    ActInstiter iparent(parent->CurScope());

    // remember all to the parent visible items that are not parameters
    for (iparent = iparent.begin(); iparent != iparent.end(); iparent++) {
      ValueIdx *element = *iparent;

      // skip parameters
      if (TypeFactory::isParamType (element->t)) continue;
      
      parent_visible.insert(element->getName());
    }
  }

  return parent_visible;
}

const std::vector<ValueIdx*> Scope::getPorts ()
{
  // get symbols visible to parent
  auto& parent_visible = this->getParentSymbols();
  
  ActInstiter elem_iter(this);
  std::vector<ValueIdx*> ports;
  UserDef *u_def = getUserDef ();
  
  // go through all contents of this scope
  for (elem_iter = elem_iter.begin(); elem_iter != elem_iter.end(); elem_iter++) {
    ValueIdx *element = *elem_iter;

    // skip parameters
    if (TypeFactory::isParamType (element->t)) continue;
    
    // skip internally generated names
    if (_act_is_reserved_id (element->getName())) continue;

    // skip instances from the parent
    auto search = parent_visible.find(element->getName());
    if (search != parent_visible.end()) continue;

    // skip non-ports
    if (u_def && (u_def->FindPort (element->getName()) == 0)) continue;

    ports.emplace_back(element);
  }

  return ports;
}

const std::vector<ValueIdx*> Scope::getUserDefInst ()
{
  // get symbols visible to parent
  auto& parent_visible = this->getParentSymbols();
  
  ActInstiter elem_iter(this);
  std::vector<ValueIdx*> userDefs;
  UserDef *u_def = getUserDef ();
  
  // go through all contents of this scope
  for (elem_iter = elem_iter.begin(); elem_iter != elem_iter.end(); elem_iter++) {
    ValueIdx *element = *elem_iter;

    // skip parameters
    if (TypeFactory::isParamType (element->t)) continue;
    
    // skip internally generated names
    if (_act_is_reserved_id (element->getName())) continue;

    // skip instances from the parent
    auto search = parent_visible.find(element->getName());
    if (search != parent_visible.end()) continue;

    // skip ports
    if (u_def && (u_def->FindPort (element->getName()) != 0)) continue;

    // skip primitive types
    if (!TypeFactory::isUserType(element->t)) continue;

    userDefs.emplace_back(element);

  }

  return userDefs;
}

const std::vector<ValueIdx*> Scope::getPrimitiveInst ()
{
  // get symbols visible to parent
  auto& parent_visible = this->getParentSymbols();
  
  ActInstiter elem_iter(this);
  std::vector<ValueIdx*> primInst;
  UserDef *u_def = getUserDef ();
  
  // go through all contents of this scope
  for (elem_iter = elem_iter.begin(); elem_iter != elem_iter.end(); elem_iter++) {
    ValueIdx *element = *elem_iter;

    // skip parameters
    if (TypeFactory::isParamType (element->t)) continue;
    
    // skip internally generated names
    if (_act_is_reserved_id (element->getName())) continue;

    // skip instances from the parent
    auto search = parent_visible.find(element->getName());
    if (search != parent_visible.end()) continue;

    // skip ports
    if (u_def && (u_def->FindPort (element->getName()) != 0)) continue;

    // skip primitive types
    if (TypeFactory::isUserType(element->t)) continue;

    primInst.emplace_back(element);

  }

  return primInst;
}

InstType *Scope::FullLookup (ActId *id, Array **aref)
{
  ValueIdx *vx;
  InstType *it;
  
  if (!id) return NULL;

  if (expanded) {
    if (id->isNamespace()) {
      vx = id->getNamespace()->CurScope()->FullLookupVal (id->Rest()->getName());
      id = id->Rest();
    }
    else {
      vx = FullLookupVal (id->getName());
    }
    if (!vx) return NULL;
    it = vx->t;
  }
  else {
    if (id->isNamespace()) {
      it = id->getNamespace()->CurScope()->FullLookup (id->Rest()->getName());
      id = id->Rest();
    }
    else {
      it = FullLookup (id->getName());
    }
  }
  
  while (id->Rest()) {
    UserDef *u = dynamic_cast<UserDef *> (it->BaseType());
    if (!u) return NULL;
    id = id->Rest();
    it = u->Lookup (id);
    if (!it) return NULL;
  }
  if (aref) {
    *aref = id->arrayInfo();
  }
  return it;
}


InstType *Scope::localLookup (ActId *id, Array **aref)
{
  ValueIdx *vx;
  InstType *it;
  
  if (!id) return NULL;

  if (id->isNamespace()) {
    vx = id->getNamespace()->CurScope()->LookupVal (id->Rest()->getName());
    id = id->Rest();
  }
  else {
    vx = LookupVal (id->getName());
  }
  
  if (!vx) return NULL;

  it = vx->t;
  while (id->Rest()) {
    UserDef *u = dynamic_cast<UserDef *> (it->BaseType());
    if (!u) return NULL;
    id = id->Rest();
    it = u->Lookup (id);
    if (!it) return NULL;
  }
  if (aref) {
    *aref = id->arrayInfo();
  }
  return it;
}


void Scope::findFresh (const char *prefix, int *num)
{
  char *buf;
  int len = strlen (prefix) + 40;
  MALLOC (buf, char, len);
  do {
    snprintf (buf, len, "%s%d", prefix, *num);
    *num = *num + 1;
  } while (Lookup (buf));
  *num = *num - 1;
  FREE (buf);
}


Scope *Scope::localClone ()
{
  if (expanded) {
    return NULL;
  }
  Scope *ret = new Scope (up);
  ret->ns = ns;
  ret->u = u;
  ret->is_function = is_function;

  hash_iter_t it;
  hash_bucket_t *b;
  hash_iter_init (H, &it);
  while ((b = hash_iter_next (H, &it))) {
    hash_bucket_t *nb = hash_add (ret->H, b->key);
    nb->v = b->v;
  }
  return ret;
}
