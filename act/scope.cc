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
#include <string.h>


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
  return bitset_tst (vpbool, id) ? 1 : 0;
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


static void _print_connections (FILE *fp, act_connection *cx)
{
  ActConniter ci(cx);
  ActId *id;
  int first = 1;

  if (cx->isPrimary()) {
    if (ci.begin() != ci.end() && (++ci.begin() != ci.end())) {
      for (ci = ci.begin(); ci != ci.end(); ci++) {
	act_connection *c = *ci;
	if (!first) {
	  fprintf (fp, "=");
	}
	id = c->toid();
	first = 0;
	id->Print (fp);
	delete id;
      }
      fprintf (fp, ";\n");
    }
  }
  if (cx->hasSubconnections()) {
    for (int i=0; i < cx->numSubconnections(); i++) {
      if (cx->hasSubconnections(i)) {
	_print_connections (fp, cx->getsubconn (i, cx->numSubconnections()));
      }
    }
  }
}

void Scope::Print (FILE *fp)
{
  char buf[10240];
  UserDef *u = getUserDef ();

  if (!expanded)
    return;
  
  fprintf (fp, "\n/* instances */\n");
  
  ActInstiter inst(this);
  
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;
    Array *a;

    if (!TypeFactory::isParamType (vx->t)) {
      if (strcmp (vx->getName(), "self") == 0) continue;
      if (!u || (u->FindPort (vx->getName()) == 0)) {
	a = vx->t->arrayInfo();
	if (a) {
	  vx->t->clrArray();
	}
	if (vx->t->isExpanded()) {
	  vx->t->sPrint (buf, 10240);
	  ActNamespace::Act()->mfprintf (fp, "%s", buf);
	}
	else {
	  vx->t->Print (fp);
	}
	fprintf (fp, " %s", vx->getName());
	if (a) {
	  a->Print (fp);
	}
	fprintf (fp, ";\n");
	if (a) {
	  vx->t->MkArray (a);
	}
      }
    }
    /* fix this.
     * connections should be reported only once
     * subconnections should be reported 
     */
    if (vx->hasConnection()) {
      _print_connections (fp, vx->connection());
#if 0      
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
	act_connection *cx = vx->connection();
	for (int i=0; i < cx->numSubconnections(); i++) {
	  
	  /* do something! */
	}
      }
#endif      
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
  }
}


void Scope::refineBaseType (const char *s, InstType *u)
{
  hash_bucket_t *b;
  InstType *ni;

  b = hash_lookup (H, s);
  if (!b) return;
  if (!expanded) {
    InstType *x = (InstType *)b->v;
    ni = new InstType (x, 1);
    if (x->arrayInfo()) {
      ni->MkArray (x->arrayInfo()->Clone());
      ni->refineBaseType (u);
    }
    b->v = ni;
  }
  else {
    ValueIdx *vx = (ValueIdx *)b->v;
    ni = new InstType (vx->t, 1);
    if (vx->t->arrayInfo()) {
      ni->MkArray (vx->t->arrayInfo()->Clone());
      ni->refineBaseType (u);
    }
    vx->t = ni;
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
