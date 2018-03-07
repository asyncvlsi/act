/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <act/types.h>
#include <act/inst.h>
#include <string.h>
#include "misc.h"
#include "hash.h"

/**
 * Create all the static members
 */
TypeFactory *TypeFactory::tf = NULL;

InstType *TypeFactory::pint = NULL;
InstType *TypeFactory::pints = NULL;
InstType *TypeFactory::pbool = NULL;
InstType *TypeFactory::preal = NULL;

InstType *TypeFactory::bools[5] = { NULL, NULL, NULL, NULL, NULL };

struct cHashtable *TypeFactory::inthash = NULL;
struct cHashtable *TypeFactory::chanhash = NULL;
struct cHashtable *TypeFactory::enumhash = NULL;
struct cHashtable *TypeFactory::ptypehash = NULL;

/*------------------------------------------------------------------------
 *
 *  Parameter types (preal, pbool, pint, ptype)
 *
 *------------------------------------------------------------------------
 */
void Type::Init ()
{
  TypeFactory::Init();
}

/* nothing else needed here! */


/*------------------------------------------------------------------------
 *
 * Type factory: this class handles the caching of all types so that
 * exactly one copy of a unique type is allocated.
 *
 *------------------------------------------------------------------------
 */
TypeFactory::TypeFactory()
{
  /* allocate hash tables */
}



TypeFactory::~TypeFactory()
{
  /* nothing */
}

void *TypeFactory::operator new (size_t s)
{
  void *space;

  if (TypeFactory::tf) {
    return tf;
  }
  
  space = malloc (s);
  if (!space) {
    fatal_error ("Could not allocate space for TypeFactory!");
  }
  return space;
}

void TypeFactory::operator delete (void *s)
{
  /* nothing: if you really want to clean up this space, use
     refcounts 
  */
}

void TypeFactory::Init ()
{
  Scope *gs = ActNamespace::Global ()->CurScope ();

  /* initialize pint, etc */
  pint = new InstType (gs, new PInt (), 0);
  pints = new InstType (gs, new PInts (), 0);
  pbool = new InstType (gs, new PBool (), 0);
  preal = new InstType (gs, new PReal (), 0);

  tf = new TypeFactory();

  /* create hash table for int<> types */
  TypeFactory::inthash = chash_new (4);
  TypeFactory::inthash->hash = TypeFactory::inthashfn;
  TypeFactory::inthash->match = TypeFactory::intmatchfn;
  TypeFactory::inthash->dup = TypeFactory::intdupfn;
  TypeFactory::inthash->free = TypeFactory::intfreefn;

  /* create hash table for enum<> types */
  TypeFactory::enumhash = chash_new (4);
  TypeFactory::enumhash->hash = TypeFactory::inthashfn;
  TypeFactory::enumhash->match = TypeFactory::intmatchfn;
  TypeFactory::enumhash->dup = TypeFactory::intdupfn;
  TypeFactory::enumhash->free = TypeFactory::intfreefn;

  /* create hash table for chan<> types */
  TypeFactory::chanhash = chash_new (4);
  TypeFactory::chanhash->hash = TypeFactory::chanhashfn;
  TypeFactory::chanhash->match = TypeFactory::chanmatchfn;
  TypeFactory::chanhash->dup = TypeFactory::chandupfn;
  TypeFactory::chanhash->free = TypeFactory::chanfreefn;

  /* create hash table for ptype<> types */
  TypeFactory::ptypehash = chash_new (4);
  TypeFactory::ptypehash->hash = TypeFactory::chanhashfn;
  TypeFactory::ptypehash->match = TypeFactory::chanmatchfn;
  TypeFactory::ptypehash->dup = TypeFactory::chandupfn;
  TypeFactory::ptypehash->free = TypeFactory::chanfreefn;
}

InstType *TypeFactory::NewBool (Type::direction dir)
{
  Scope *gs = ActNamespace::Global ()->CurScope ();

  int i = (int)dir;

  if (!bools[i]) {
    bools[i] = new InstType (gs, new Bool(), 0);
    bools[i]->SetDir (dir);
  }
  return bools[i];
}

struct inthashkey {
  Scope *s;
  Type::direction d;
  int sig;			// signed or not
  Expr *w;
};

/*
  sig = is signed?
*/
InstType *TypeFactory::NewInt (Scope *s, Type::direction dir, int sig, Expr *w)
{
  chash_bucket_t *b;
  struct inthashkey k;
  static Int *_i = NULL;

  if (_i == NULL) {
    _i = new Int();
    _i->expanded = 0;
    _i->is_signed = sig;
  }
  
  k.s = s;
  k.d = dir;
  k.w = w;
  k.sig = sig;

  /* the key should contain the direction and width expression.
     For int<w>, we need to know the *context* of w.. is it the same w
     as another int<w>, or not.
  */
  b = chash_lookup (inthash, &k);
  if (!b) {
    InstType *i = new InstType (s, _i, 0);
    i->setNumParams (1);
    i->setParam (0, w);
    i->SetDir (dir);

    b = chash_add (inthash, &k);
    b->v = i;
  }
  return (InstType *)b->v;
}


InstType *TypeFactory::NewEnum (Scope *s, Type::direction dir, Expr *w)
{
  chash_bucket_t *b;
  struct inthashkey k;
  static Enum *_e = NULL;

  if (!_e) {
    _e = new Enum();
  }
  
  k.s = s;
  k.d = dir;
  k.w = w;
  k.sig = 0;

  /* the key should contain the direction and width expression.
     For int<w>, we need to know the *context* of w.. is it the same w
     as another int<w>, or not.
  */
  b = chash_lookup (enumhash, &k);
  if (!b) {
    InstType *i = new InstType (s, _e, 0);
    i->setNumParams (1);
    i->setParam (0, w);
    i->SetDir (dir);

    b = chash_add (enumhash, &k);
    b->v = i;
  }
  return (InstType *)b->v;
}


int TypeFactory::isUserType (Type *t)
{
  UserDef *tmp_u = dynamic_cast<UserDef *>(t);
  if (tmp_u) {
    return 1;
  }
  return 0;
}

int TypeFactory::isDataType (Type *t)
{
  Data *tmp_d = dynamic_cast<Data *>(t);
  if (tmp_d) {
    return 1;
  }
  Int *tmp_i = dynamic_cast<Int *>(t);
  if (tmp_i) {
    return 1;
  }
  Enum *tmp_e = dynamic_cast<Enum *>(t);
  if (tmp_e) {
    return 1;
  }
  Bool *tmp_b = dynamic_cast<Bool *>(t);
  if (tmp_b) {
    return 1;
  }
  return 0;
}

int TypeFactory::isIntType (Type *t)
{
  Int *tmp_i = dynamic_cast<Int *>(t);
  if (tmp_i) {
    return 1;
  }
  Enum *tmp_e = dynamic_cast<Enum *>(t);
  if (tmp_e) {
    return 1;
  }
  return 0;
}

int TypeFactory::isPIntType (Type *t)
{
  if (t == TypeFactory::pint->BaseType ()) {
    return 1;
  }
  return 0;
}

int TypeFactory::isPIntsType (Type *t)
{
  if (t == TypeFactory::pints->BaseType ()) {
    return 1;
  }
  return 0;
}

int TypeFactory::isBoolType (Type *t)
{
  Bool *tmp_b = dynamic_cast<Bool *>(t);
  if (tmp_b) {
    return 1;
  }
  return 0;
}

int TypeFactory::isPBoolType (Type *t)
{
  if (t == TypeFactory::pbool->BaseType ()) {
    return 1;
  }
  return 0;
}

int TypeFactory::isPRealType (Type *t)
{
  if (t == TypeFactory::preal->BaseType ()) {
    return 1;
  }
  return 0;
}

int TypeFactory::isChanType (Type *t)
{
  Chan *tmp_c = dynamic_cast<Chan *>(t);
  if (tmp_c) {
    return 1;
  }
  Channel *tmp_uc = dynamic_cast<Channel *>(t);
  if (tmp_uc) {
    return 1;
  }
  return 0;
}

int TypeFactory::isProcessType (Type *t)
{
  Process *tmp_p = dynamic_cast<Process *>(t);
  if (tmp_p) {
    return 1;
  }
  return 0;
}

int TypeFactory::isFuncType (Type *t)
{
  Function *tmp_f = dynamic_cast<Function *>(t);
  if (tmp_f) {
    return 1;
  }
  return 0;
}

int TypeFactory::isPtypeType (Type *t)
{
  PType *tmp_t = dynamic_cast<PType *>(t);
  if (tmp_t) {
    return 1;
  }
  else {
    return 0;
  }
}

int TypeFactory::isParamType (Type *t)
{
  if (isPtypeType (t) ||
      isPIntType (t) ||
      isPIntsType (t) ||
      isPBoolType (t) ||
      isPRealType (t)) {
    return 1;
  }
  return 0;
}



/**
 * Compute a hash for the expression, using "prev" as the starting
 * value (previous hash)
 *
 * \param sz is the size of the hash table
 * \param w  is the expression
 * \param prev is the value of the hash so far
 * \return the new hash value incorporating w into prev
 */
static int expr_hash (int sz, Expr *w, int prev)
{
  char t;

  if (w == NULL) return prev;
  
  t = w->type;

  prev = hash_function_continue (sz, (const unsigned char *)&t, sizeof (char), prev, 1);

  switch (t) {
    /* binary */
  case E_AND:
  case E_OR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV:
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
    prev = expr_hash (sz, w->u.e.l, prev);
    prev = expr_hash (sz, w->u.e.r, prev);
    break;
    
    /* unary */
  case E_UMINUS:
  case E_NOT:
  case E_COMPLEMENT:
    prev = expr_hash (sz, w->u.e.l, prev);
    break;

    /* special */
  case E_QUERY:
    prev = expr_hash (sz, w->u.e.l, prev);
    prev = expr_hash (sz, w->u.e.r->u.e.l, prev);
    prev = expr_hash (sz, w->u.e.r->u.e.r, prev);
    break;

  case E_CONCAT:
    fatal_error ("Should not be here");
    while (w && w->type == E_CONCAT) {
      prev = expr_hash (sz, w->u.e.l, prev);
      w = w->u.e.r;
    }
    break;

  case E_BITFIELD:
    prev = hash_function_continue 
      (sz, (const unsigned char *)&w->u.e.l, sizeof (Expr *), prev, 1);
    prev = hash_function_continue 
      (sz, (const unsigned char *)&w->u.e.r->u.e.l, sizeof (Expr *), prev, 1);
    prev = hash_function_continue 
      (sz, (const unsigned char *)&w->u.e.r->u.e.r, sizeof (Expr *), prev, 1);
    break;

  case E_FUNCTION:
    fatal_error ("Fix this please");
    break;

    /* leaf */
  case E_INT:
    prev = hash_function_continue 
      (sz, (const unsigned char *) &w->u.v, sizeof (unsigned int), prev, 1);
    break;

  case E_REAL:
    prev = hash_function_continue 
      (sz, (const unsigned char *) &w->u.f, sizeof (double), prev, 1);
    break;

  case E_VAR:
  case E_PROBE:
    /* hash the id */
    prev = hash_function_continue 
      (sz, (const unsigned char *) &w->u.e.l, sizeof (Expr *), prev, 1);
    break;

  case E_TRUE:
  case E_FALSE:
    break;

    
  default:
    fatal_error ("Unknown expression type?");
    break;
  }
  return prev;
}

/*
  hash ids!
*/
#define _id_equal(a,b) ((a) == (b))

/**
 *  Compare two expressions structurally for equality
 *
 *  \param a First expression to be compared
 *  \param b Second expression to be compared
 *  \return 1 if the two are structurally identical, 0 otherwise
 */
static int expr_equal (Expr *a, Expr *b)
{
  int ret;
  if (a == b) { 
    return 1;
  }
  if (a == NULL || b == NULL) {
    /* this means a != b given the previous clause */
    return 0;
  }
  if (a->type != b->type) {
    return 0;
  }
  switch (a->type) {
    /* binary */
  case E_AND:
  case E_OR:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV:
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
    if (expr_equal (a->u.e.l, b->u.e.l) &&
	expr_equal (a->u.e.r, b->u.e.r)) {
      return 1;
    }
    else {
      return 0;
    }
    break;
    
    /* unary */
  case E_UMINUS:
  case E_NOT:
  case E_COMPLEMENT:
    return expr_equal (a->u.e.l, b->u.e.l);
    break;

    /* special */
  case E_QUERY:
    if (expr_equal (a->u.e.l, b->u.e.l) &&
	expr_equal (a->u.e.r->u.e.l, b->u.e.r->u.e.l) &&
	expr_equal (a->u.e.r->u.e.r, b->u.e.r->u.e.r)) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_CONCAT:
    fatal_error ("Should not be here");
    ret = 1;
    while (a && ret && a->type == E_CONCAT) {
      if (!b || b->type != E_CONCAT) return 0;
      ret = expr_equal (a->u.e.l, b->u.e.l);
      if (!ret) return 0;
      a = a->u.e.r;
      b = b->u.e.r;
    }
    return 1;
    break;

  case E_BITFIELD:
    if (_id_equal (a->u.e.l, b->u.e.l) &&
	(a->u.e.r->u.e.l == b->u.e.r->u.e.l) &&
	(a->u.e.r->u.e.r == b->u.e.r->u.e.r)) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_FUNCTION:
    fatal_error ("Fix this please");
    break;

    /* leaf */
  case E_INT:
    if (a->u.v == b->u.v) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_REAL:
    if (a->u.f == b->u.f) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_VAR:
  case E_PROBE:
    /* hash the id */
    if (_id_equal (a->u.e.l, b->u.e.l)) {
      return 1;
    }
    else {
      return 0;
    }
    break;

  case E_TRUE:
  case E_FALSE:
    return 1;
    break;

    
  default:
    fatal_error ("Unknown expression type?");
    return 0;
    break;
  }
  return 0;
}

int TypeFactory::inthashfn (int sz, void *key)
{
  struct inthashkey *k = (struct inthashkey *)key;
  int v;
  unsigned char sig;

  /* scope pointers are unique */
  v = hash_function_continue 
    (sz, (const unsigned char *) &k->s, sizeof (Scope *), 0, 0);

  /* hash the direction */
  v = hash_function_continue 
    (sz, (const unsigned char *) &k->d, sizeof (Type::direction), v,1);

  /* hash the sign */
  sig = k->sig;
  v = hash_function_continue (sz, (const unsigned char *) &sig, 1, v, 1);

  /* now walk the expression to compute its hash */
  v = expr_hash (sz, k->w, v);

  return v;
}

int TypeFactory::intmatchfn (void *key1, void *key2)
{
  struct inthashkey *k1, *k2;
  k1 = (struct inthashkey *)key1;
  k2 = (struct inthashkey *)key2;
  if (k1->s != k2->s) return 0;
  if (k1->d != k2->d) return 0;
  if (k1->sig != k2->sig) return 0;
  return expr_equal (k1->w, k2->w);
}

void *TypeFactory::intdupfn (void *key)
{
  struct inthashkey *k, *kret;
  k = (struct inthashkey *) key;
  kret = new inthashkey;
  kret->s = k->s;
  kret->d = k->d;
  kret->w = k->w;
  kret->sig = k->sig;
  return kret;
}

void TypeFactory::intfreefn (void *key)
{
  struct inthashkey *k = (struct inthashkey *)key;
  delete k;
}

struct chanhashkey {
  Scope *s;
  Type::direction d;
  int ntypes;			// number of types
  InstType **t;			// array of types
};


int TypeFactory::chanhashfn (int sz, void *key)
{
  int v;
  struct chanhashkey *ch = (struct chanhashkey *)key;
  int i;

  v = hash_function_continue
    (sz, (const unsigned char *) &ch->s, sizeof (Scope *), 0, 0);
  v = hash_function_continue
    (sz, (const unsigned char *) &ch->d, sizeof (Type::direction), v, 1);
  v = hash_function_continue
    (sz, (const unsigned char *) &ch->ntypes, sizeof (int), v, 1);
  for (i=0; i < ch->ntypes; i++) {
    v = hash_function_continue
      (sz, (const unsigned char *) ch->t[i], sizeof (Type *), v, 1);
  }
  return v;
}

int TypeFactory::chanmatchfn (void *key1, void *key2)
{
  struct chanhashkey *c1, *c2;
  c1 = (struct chanhashkey *)key1;
  c2 = (struct chanhashkey *)key2;

  if (c1->s != c2->s) return 0;
  if (c1->d != c2->d) return 0;
  if (c1->ntypes != c2->ntypes) return 0;
  for (int i=0; i < c1->ntypes; i++) {
    if (c1->t[i] != c2->t[i]) return 0;
  }
  return 1;
}

void *TypeFactory::chandupfn (void *key)
{
  struct chanhashkey *c, *cret;
  c = (struct chanhashkey *)key;
  cret = new chanhashkey;

  cret->s = c->s;
  cret->d = c->d;
  cret->ntypes = c->ntypes;
  cret->t = new InstType *[cret->ntypes];
  for (int i=0; i < cret->ntypes; i++) {
    cret->t[i] = c->t[i];
  }
  return cret;
}

void TypeFactory::chanfreefn (void *key)
{
  struct chanhashkey *c = (struct chanhashkey *)key;
  delete c->t;
  delete c;
}


InstType *TypeFactory::NewChan (Scope *s, Type::direction dir, int n, InstType **l)
{
  struct chanhashkey c;
  chash_bucket_t *b;
  static Chan *_c = NULL;

  if (!_c) {
    _c = new Chan();
    _c->expanded = 0;
  }

  c.s = s;
  c.d = dir;
  c.ntypes = n;
  c.t = l;

  b = chash_lookup (chanhash, &c);
  if (!b) {
    InstType *ch = new InstType (s, _c, 0);

    ch->setNumParams (n);
    for (int i=0; i < n; i++) {
      ch->setParam (i, l[i]);
    }
    ch->SetDir (dir);

    b = chash_add (chanhash, &c);
    b->v = ch;
  }
  return (InstType *)b->v;
}

InstType *TypeFactory::NewUserDef (Scope *s, InstType *it)
{
  /* FIX THIS TO CACHE THIS TYPE PROPERLY */
  it->MkCached ();
  return it;
}

InstType *TypeFactory::NewPType (Scope *s, InstType *t)
{
  static PType *_t = NULL;
  chash_bucket_t *b;
  struct chanhashkey c;
  InstType *it[1];

  if (_t == NULL) {
    _t = new PType();
  }

  c.s = s;
  c.d = Type::NONE;
  c.ntypes = 1;
  it[0] = t;
  c.t = it;

  b = chash_lookup (ptypehash, &c);

  if (!b) {
    InstType *i = new InstType (s, _t, 0);

    i->setScope (s);
    i->setNumParams (1);
    i->setParam (0, t);

    b = chash_add (ptypehash, &c);
    b->v = i;
  }
  return (InstType *)b->v;
}



const char *UserDef::getName()
{
  return name;
}



Array::Array (Expr *e, Expr *f)
{
  dims = 1;
  NEW (r, struct range);

  if (f == NULL) {
    r->u.ue.lo = NULL;
    r->u.ue.hi = e;
    deref = 1;
  }
  else {
    r->u.ue.lo = e;
    r->u.ue.hi = f;
    deref = 0;
  }
  expanded = 0;
  next = NULL;
}

Array::~Array ()
{
  if (next) {
    delete next;
  }
  FREE (r);
}

void Array::Concat (Array *a)
{
  int i;

  Assert (isSparse() == 0, "Array::Concat() only supported for dense arrays");
  Assert (a->isSparse() == 0, "Array::Concat() only works for dense arrays");
  Assert (a->isExpanded () == expanded, "Array::Concat() must have same expanded state");
  
  dims += a->dims;
  
  REALLOC (r, struct range, dims);
  
  for (i=0; i < a->dims; i++) {
    r[dims - a->dims + i] = a->r[i];
  }

  /* if any part is not a deref, it is a subrange */
  if (a->deref == 0) {
    deref = 0;
  }
}


InstType::InstType (Scope *_s, Type *_t, int is_temp)
{
  expanded = 0;
  t = _t;
  nt = 0;
  a = NULL;
  dir = Type::NONE;
  u = NULL;
  s = _s;
  temp_type = (is_temp ? 1 : 0);
}

/*
  @return 1 if the kth parameter is a type, 0 if it is an AExpr 
*/
int InstType::isParamAType (int k)
{
  Assert (0 <= k && k < nt, "Hmm");
  if (TypeFactory::isChanType (t) || TypeFactory::isPtypeType (t)) {
    /* all are type parameters */
    return 1;
  }
  if (TypeFactory::isIntType (t)) {
    /* all are AExpr parameters */
    return 0;
  }
  UserDef *u;
  InstType *x;
  u = dynamic_cast<UserDef *> (t);
  Assert (u, "What on earth");
  x = u->getPortType (-(k+1));
  Assert (x, "Huh");
  if (TypeFactory::isPtypeType (x->BaseType ())) {
    return 1;
  }
  else {
    return 0;
  }
}

InstType::~InstType ()
{
  int i;

  if (temp_type == 0) return; /* cached */

  /*
  printf ("free'ing [%x]: ", this);
  Print (stdout);
  printf ("\n");
  fflush (stdout);
  */

  if (nt > 0) {
    for (i=0; i < nt; i++) {
      if (isParamAType (i)) {
	delete u[i].tt;
      }
      else {
	delete u[i].tp;
      }
    }
    FREE (u);
  }
  delete a;
}


void InstType::MkArray (Array *_a)
{
  Assert (a == NULL, "Array of an array?");
  a = _a;
}



int UserDef::AddMetaParam (InstType *t, const char *id, int opt)
{
  int i;

  for (i=0; i < nt; i++) {
    if (strcmp (pn[i], id) == 0) {
      return 0;
    }
  }

  /*printf ("ADD: %s to userdef: %x\n", id, this);*/

  if (!I->Add (id, t)) {
    return 0;
  }


  nt++;
  if (opt == 0) {
    mt++;
  }
  
  if (pn) {
    REALLOC (pn, const char *, nt);
  }
  else {
    NEW (pn, const char *);
  }
  pn[nt-1] = id;

  if (pt) {
    REALLOC (pt, InstType *, nt);
  }
  else {
    NEW (pt, InstType *);
  }
  pt[nt-1] = t;

  return 1;
}



const char *UserDef::getPortName (int pos)
{
  if (pos < 0) {
    pos = -pos;
    Assert (1 <= pos && pos < 1+getNumParams(), "Invalid pos!");
    return pn[pos-1];
  }
  else {
    Assert (0 <= pos && pos < getNumPorts(), "Invalid pos!");
    return port_n[pos];
  }
}

InstType *UserDef::getPortType (int pos)
{
  if (pos < 0) {
    pos = -pos;
    Assert (1 <= pos && pos < 1+getNumParams(), "Invalid pos!");
    return pt[pos-1];
  }
  else {
    Assert (0 <= pos && pos < getNumPorts(), "Invalid pos!");
    return port_t[pos];
  }
}
  
  

int UserDef::AddPort (InstType *t, const char *id)
{
  int i;
  
  /*printf ("ADDport: %s to scope %x\n", id, I);*/

  for (i=0; i < nt; i++) {
    if (strcmp (pn[i], id) == 0) {
      return 0;
    }
  }
  for (i=0; i < nports; i++) {
    if (strcmp (port_n[i], id) == 0) {
      return 0;
    }
  }

  if (!I->Add (id, t)) {
    return 0;
  }

  nports++;

  if (!port_n) {
    NEW (port_n, const  char *);
  }
  else {
    REALLOC (port_n, const char *, nports);
  }
  if (!port_t) {
    NEW (port_t, InstType *);
  }
  else {
    REALLOC (port_t, InstType *, nports);
  }

  port_n[nports-1] = id;
  port_t[nports-1] = t;

  return 1;
}

int UserDef::FindPort (const char *id)
{
  int i;
  
  for (i=0; i < nt; i++) {
    if (strcmp (pn[i], id) == 0) {
      return -(i+1);
    }
  }
  for (i=0; i < nports; i++) {
    if (strcmp (port_n[i], id) == 0) {
      return (i+1);
    }
  }
  return 0;
}


UserDef::UserDef (ActNamespace *ns)
{
  parent = NULL;
  parent_eq = 0;

  defined = 0;
  expanded = 0;

  nt = 0;
  mt = 0;
  pt = NULL;
  pn = NULL;
  exported = 0;

  name = NULL;
  
  nports = 0;
  port_t = NULL;
  port_n = NULL;

  b = NULL;

  I = new Scope(ns->CurScope());
  I->setUserDef (this);
  _ns = ns;
}

UserDef::~UserDef()
{
  int i;
  if (pt) {
    for (i=0; i < nt; i++) {
      if (pt[i]) {
	delete pt[i];
      }
    }
    FREE (pt);
  }
  if (pn) {
    FREE (pn);
  }
  if (port_t) {
    for (i=0; i < nports; i++) {
      if (port_t[i]) {
	delete port_t[i];
      }
    }
    FREE (port_t);
  }
  if (port_n) {
    FREE (port_n);
  }
  if (I) {
    delete I;
  }
  if (b) {
    delete b;
  }
}

void UserDef::MkCopy (UserDef *u)
{
  parent = u->parent; u->parent = NULL;
  parent_eq = u->parent_eq; u->parent_eq = 0;
  
  defined  = u->defined; u->defined = 0;
  expanded = u->expanded; u->expanded = 0;

  nt = u->nt; u->nt = 0;
  mt = u->mt; u->mt = 0;
  
  pt = u->pt; u->pt = NULL;
  pn = u->pn; u->pn = NULL;

  exported = u->exported; u->exported = 0;

  nports = u->nports; u->nports = 0;
  port_t = u->port_t; u->port_t = NULL;
  port_n = u->port_n; u->port_n = NULL;

  I = u->I; u->I = NULL;
  I->setUserDef (this);

  name = u->name; u->name = NULL;
  b = u->b; u->b = NULL;
}


Process::Process (UserDef *u) : UserDef (*u)
{
  is_cell = 0;
  b = NULL;

  /* copy over userdef */
  MkCopy (u);
}

Process::~Process ()
{
  if (b) {
    delete b;
  }
}

Function::Function (UserDef *u) : UserDef (*u)
{
  b = NULL;
  ret_type = NULL;

  /* copy over userdef */
  MkCopy (u);
}

Function::~Function ()
{
  if (b) {
    delete b;
  }
}



void UserDef::SetParent (InstType *t, unsigned int eq)
{
  parent = t;
  parent_eq = eq;
}


int UserDef::isStrictPort (const  char *s)
{
  int i;

  /*printf ("mt = %d, nt = %d [%x]\n", mt, nt, this);*/

  for (i=0; i < mt; i++) {
    if (strcmp (s, pn[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

Data::Data (UserDef *u) : UserDef (*u)
{
  /* copy over userdef */
  is_enum = 0;
  b = NULL;
  set = NULL;
  get = NULL;

  MkCopy (u);
}

Data::~Data()
{
  
}

Channel::Channel (UserDef *u) : UserDef (*u)
{
  /* copy over userdef */
  b = NULL;
  send = NULL;
  recv = NULL;
  MkCopy (u);
}

Channel::~Channel()
{
  
}


/*------------------------------------------------------------------------
 *  Compare two user-defined types
 *------------------------------------------------------------------------*/
int UserDef::isEqual (UserDef *u)
{
  int i;

  if (parent != u->parent) {
    if (!parent || !u->parent) return 0;
    if (!parent->isEqual (u->parent)) {
      return 0;
    }
  }
  if (parent_eq != u->parent_eq) return 0;

  if (nt != u->nt) return 0;
  if (mt != u->mt) return 0;
  if (nports != u->nports) return 0;

  if (exported != u->exported) return 0;
  if (expanded != u->expanded) return 0;

  for (i=0; i < nt; i++) {
    if (pn[i] != u->pn[i]) return 0;
    if (!pt[i]->isEqual (u->pt[i])) return 0;
  }

  for (i=0; i < nports; i++) {
    if (port_n[i] != u->port_n[i]) return 0;
    if (!port_t[i]->isEqual (u->port_t[i])) return 0;
  }
  
  return 1;
}


/*------------------------------------------------------------------------
 * Compare two instance types
 *
 * @param weak : this parameter only matters for array types. All
 *               cases check that the # of dimensions match.
 *               0 = check that the arrays are in fact equal and have
 *               the same index ranges (and range expressions)
 *               1 = checks that arrays are compatible in terms of #
 *               of dimensions, rather than equal
 *               2 = check that array ranges have the same size; they
 *               could have different index ranges 
 *
 *------------------------------------------------------------------------
 */
int InstType::isEqual (InstType *it, int weak)
{
  if (t != it->t) return 0;
  if (nt != it->nt) return 0;

  if ((a && !it->a) || (!a && it->a)) return 0;

  if (weak != 1) {
    if (a && !a->isEqual (it->a, weak == 0 ? 1 : 0)) return 0;
  }
  else {
    if (a && !a->isDimCompatible (it->a)) return 0;
  }

  for (int i=0; i < nt; i++) {
    if (isParamAType (i)) {
      if (!u[i].tt->isEqual (it->u[i].tt)) return 0;
    }
    else {
      if (!u[i].tp->isEqual (it->u[i].tp)) return 0;
    }
  }
  return 1;
}

void InstType::Print (FILE *fp)
{
  UserDef *ud;
  InstType *x;

#if 0
  /* view cached flag */
  if (temp_type == 0) {
    fprintf (fp, "c:");
  }
#endif
  fprintf (fp, "%s", t->getName ());
  if (nt > 0) {
    /* templates are used for int, chan, ptype, and userdef */
    fprintf (fp, "<");
    
    for (int i=0; i < nt; i++) {
      if (isParamAType (i)) {
	u[i].tt->Print (fp);
      }
      else {
	u[i].tp->Print (fp);
      }
      if (i < nt-1) {
	fprintf (fp, ",");
      }
    }
    fprintf (fp, ">");
  }
  switch (dir) {
  case Type::NONE:
    break;
  case Type::IN:
    fprintf (fp, "?");
    break;
  case Type::OUT:
    fprintf (fp, "!");
    break;
  case Type::INOUT:
    fprintf (fp, "?!");
    break;
  case Type::OUTIN:
    fprintf (fp, "!?");
    break;
  }
  if (a) {
    a->Print (fp);
  }
}



/*------------------------------------------------------------------------
 * Compare two arrays
 *------------------------------------------------------------------------
 */
int Array::isEqual (Array *a, int strict) 
{
  struct range *r1, *r2;
  int i;

  if (dims != a->dims) return 0;
  if (deref != a->deref) return 0;
  if ((next && !a->next) || (a->next && !next)) return 0;
  
  r1 = r;
  r2 = a->r;

  if (!expanded) {
    /* no distinction between strict and non-strict */
    for (i=0; i < dims; i++) {
      if ((r1[i].u.ue.lo && !r2[i].u.ue.lo) || (!r1[i].u.ue.lo && r2[i].u.ue.lo))
	return 0;
      if (r1[i].u.ue.lo && !expr_equal (r1[i].u.ue.lo, r2[i].u.ue.lo)) return 0;
      if (r1[i].u.ue.hi && !expr_equal (r1[i].u.ue.hi, r2[i].u.ue.hi)) return 0;
    }
  }
  else {
    for (i=0; i < dims; i++) {
      if (strict) {
	if ((r1[i].u.ex.lo != r2[i].u.ex.lo) || (r1[i].u.ex.hi != r2[i].u.ex.hi))
	  return 0;
      }
      else {
	if ((r1[i].u.ex.hi - r1[i].u.ex.lo) != (r2[i].u.ex.hi - r2[i].u.ex.lo))
	  return 0;
      }
    }
  }
  if (next) {
    return next->isEqual (a->next, strict);
  }
  return 1;
}

/*------------------------------------------------------------------------
 * Check if two arrays are compatible
 *------------------------------------------------------------------------
 */
int Array::isDimCompatible (Array *a)
{
  struct range *r1, *r2;
  int i;

  if (dims != a->dims) return 0;
  if (deref != a->deref) return 0;
  if ((next && !a->next) || (a->next && !next)) return 0;
  
  return 1;
}


/*------------------------------------------------------------------------
 * Compare two array expressions
 *------------------------------------------------------------------------
 */
int AExpr::isEqual (AExpr *a)
{
  if (!a) return 0;
  if (t != a->t) return 0;

  if ((l && !a->l) || (!l && a->l)) return 0;
  if ((r && !a->r) || (!r && a->r)) return 0;
  
  if (0/*t == AExpr::SUBRANGE*/) {
    return _id_equal ((ActId *)l, (ActId *)a->l);
  }
  else if (t == AExpr::EXPR) {
    return expr_equal ((Expr *)l, (Expr *)a->l);
  }
  if (l && !l->isEqual (a->l)) return 0;
  if (r && !r->isEqual (a->r)) return 0;

  return 1;
}


/*------------------------------------------------------------------------
 *
 * Array Expressions
 *
 *------------------------------------------------------------------------
 */
#if 0
AExpr::AExpr (ActId *e)
{
  r = NULL;
  l = (AExpr *)e;
  t = AExpr::SUBRANGE;
}
#endif

AExpr::AExpr (Expr *e)
{
  r = NULL;
  l = (AExpr *)e;
  t = AExpr::EXPR;
}


AExpr::AExpr (AExpr::type typ, AExpr *inl, AExpr *inr)
{
  t = typ;
  l = inl;
  r = inr;
}

AExpr::~AExpr ()
{
  if (/*t != AExpr::SUBRANGE &&*/ t != AExpr::EXPR) {
    if (l) {
      delete l;
    }
    if (r) {
      delete r;
    }
  }
#if 0
  else if (t == AExpr::SUBRANGE) {
    delete ((ActId *)l);
  }
#endif
  else if (t == AExpr::EXPR) {
    /* hmm... expression memory management */
  }
}

void AExpr::Print (FILE *fp)
{
  AExpr *a;
  switch (t) {
  case AExpr::EXPR:
    print_expr (fp, (Expr *)l);
    break;

  case AExpr::CONCAT:
    a = this;
    while (a) {
      a->l->Print (fp);
      a = a->GetRight ();
      if (a) {
	fprintf (fp, "#");
      }
    }
    break;

  case AExpr::COMMA:
    fprintf (fp, "{");
    a = this;
    while (a) {
      a->l->Print (fp);
      a = a->GetRight ();
      if (a) {
	fprintf (fp, ",");
      }
    }
    fprintf (fp, "}");
    break;

#if 0
  case AExpr::SUBRANGE:
#endif
  default:
    fatal_error ("Blah, or unimpl %x", this);
    break;
  }
}


/*
  Expand: returns a list of values.
*/
list_t *AExpr::Expand (ActNamespace *ns, Scope *s)
{
  /* this evaluates the array expression: everything must be a
     constant or known parameter value */
  return NULL;
}


/*------------------------------------------------------------------------
 *
 * Identifiers
 *
 *------------------------------------------------------------------------
 */
ActId::ActId (const char *s, Array *_a)
{
  name = string_create (s);
  a = _a;
  next = NULL;
}

ActId::~ActId ()
{
  string_free (name);
  if (a) {
    delete a;
  }
  if (next) {
    delete next;
  }
  next = NULL;
  a = NULL;
  name = NULL;
}
   


ActId *ActId::Clone ()
{
  ActId *ret;
  Array *aclone;

  if (a) {
    aclone = a->Clone();
  }
  else {
    aclone = NULL;
  }

  ret = new ActId (string_char (name), aclone);

  if (next) {
    ret->next = next->Clone ();
  }

  return ret;
}



int ActId::isRange ()
{
  ActId *id;

  id = this;
  while (id) {
    if (id->a && !id->a->isDeref()) {
      return 1;
    }
    id = id->next;
  }
  return 0;
}

void ActId::Append (ActId *id)
{
  Assert (!next, "ActId::Append() called with non-NULL next field");
  next = id;
}


void ActId::Print (FILE *fp, ActId *end)
{
  ActId *start = this;

  while (start && start != end) {
    if (start != this) {
      fprintf (fp, ".");
    }
    fprintf (fp, "%s", string_char(start->name));
    if (start->a) {
      start->a->Print (fp);
    }
    start = start->next;
  }
  fflush (fp);
}

/*
  Clone an InstType
*/
InstType::InstType (InstType *i, int skip_array)
{
  t = i->t;
  dir = i->dir;
  s = i->s;
  expanded = i->expanded;
  if (!skip_array) {
    Assert (i->a == NULL, "Replication in the presence of arrays?");
  }
  a = NULL;
  nt = 0;
  if (i->nt > 0) {
    nt = i->nt;
    MALLOC (u, inst_param, i->nt);
    /* clone this too */
    for (int k = 0; k < nt; k++) {
      if (i->isParamAType (k)) {
	u[k].tt = new InstType (i->u[k].tt);
      }
      else {
	u[k].tp = i->u[k].tp->Clone ();
      }
    }
  }
  temp_type = 1;
}

/*------------------------------------------------------------------------
 *`
 *   InstType::setNumParams --
 *
 *   Create slots for the template parameters for an instance type
 *
 *------------------------------------------------------------------------
 */
void InstType::setNumParams (int n)
{
  Assert (nt == 0, "Modifying the number of template parameters specified in an instance type??");
  nt = n;
  MALLOC (u, inst_param, nt);
  for (int k=0; k < nt; k++) {
    u[k].tp = NULL;
  }
}

void InstType::setParam (int pn, AExpr *a)
{
  Assert (pn < nt && pn >= 0, "setParam() called with an invalid value");
  Assert (u[pn].tp == NULL, "setParam() changing an existing parameter!");
  /*printf ("setting param %d to %x\n", pn, a);*/
  u[pn].tp = a;
}

void InstType::setParam (int pn, Expr *a)
{
  Assert (pn < nt && pn >= 0, "setParam() called with an invalid value");
  Assert (u[pn].tp == NULL, "setParam() changing an existing parameter!");
  u[pn].tp = new AExpr (a);
}

void InstType::setParam (int pn, InstType *t)
{
  Assert (pn < nt && pn >= 0, "setParam() called with an invalid value");
  Assert (u[pn].tp == NULL, "setParam() changing an existing parameter!");
  u[pn].tt = t;
}

void InstType::setScope (Scope *_s)
{
  s = _s;
}


void Array::Print (FILE *fp)
{
  Array *pr;
  if (next) {
    fprintf (fp, "[ ");
  }
  pr = this;
  while (pr) {
    fprintf (fp, "[");
    for (int i=0; i < pr->dims; i++) {
      if (!expanded) {
	if (pr->r[i].u.ue.lo == NULL) {
	  print_expr (fp, pr->r[i].u.ue.hi);
	}
	else {
	  print_expr (fp, pr->r[i].u.ue.lo);
	  fprintf (fp, "..");
	  print_expr (fp, pr->r[i].u.ue.hi);
	}
      }
      else {
	if (pr->r[i].u.ex.lo == 0) {
	  fprintf (fp, "%d", pr->r[i].u.ex.hi+1);
	}
	else {
	  fprintf (fp, "%d..%d", pr->r[i].u.ex.lo, pr->r[i].u.ex.hi);
	}
      }
      if (i < pr->dims-1) {
	fprintf (fp, ",");
      }
    }
    fprintf (fp, "]");
    pr = pr->next;
  }
  if (this->next) {
    fprintf (fp, " ]");
  }
}

int Array::effDims()
{
  int i;
  int count = 0;

  Assert (!expanded, "Not applicable to expanded arrays!");

  if (isSparse()) return nDims ();

  count = 0;
  for (i=0; i < dims; i++) {
    if (r[i].u.ue.lo == NULL) {
      /* skip */
    }
    else {
      count++;
    }
  }
  return count;
}

/*
 * @return number of elements in the array
 */
int Array::size()
{
  int i;
  int count = 1;
  struct Array *a;

  Assert (expanded, "Only applicable to expanded arrays");
  for (i=0; i < dims; i++) {
    if (r[i].u.ex.hi < r[i].u.ex.lo) {
      count = 0;
      break;
    }
    else {
      count = count*(r[i].u.ex.hi-r[i].u.ex.lo+1);
    }
  }
  if (next) {
    count += next->size();
  }
  return count;
}



/*
  Precedence printing

  highest

  10  #, ~, !
   9  * / %
   8  + -
   7  <<, >>, <, >, <=, >=, ==, !=
   6  & 
   5  ^
   4  |
   3  ?

   called with incoming precdence.
   If my precedence is higher, no parens is necessary
   otherwise parenthesize it.

*/
static void _print_expr (FILE *fp, Expr *e, int prec)
{
  char *s;
  if (!e) return;
 
#define PREC_BEGIN(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (fp, "(");			\
    }						\
  } while (0)

#define PREC_END(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (fp, ")");			\
    }						\
  } while (0)

#define EMIT_BIN(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    _print_expr (fp, e->u.e.l, (myprec));	\
    fprintf (fp, "%s", (sym));			\
    _print_expr (fp, e->u.e.r, (myprec));	\
    PREC_END (myprec);				\
  } while (0)

#define EMIT_UNOP(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    fprintf (fp, "%s", sym);			\
    _print_expr (fp, e->u.e.l, (myprec));	\
    PREC_END (myprec);				\
  } while (0)
    
  switch (e->type) {
  case E_PROBE:
    PREC_BEGIN (10);
    fprintf (fp, "#");
    ((ActId *)(e->u.e.l))->Print (fp);
    PREC_END (10);
    break;
    
  case E_NOT: EMIT_UNOP(10, "!"); break;
  case E_COMPLEMENT: EMIT_UNOP(10, "~"); break;
  case E_UMINUS: EMIT_UNOP(10, "-"); break;

  case E_MULT: EMIT_BIN (9, "*"); break;
  case E_DIV:  EMIT_BIN (9, "/"); break;
  case E_MOD:  EMIT_BIN (9, "%"); break;

  case E_PLUS:  EMIT_BIN (8, "+"); break;
  case E_MINUS: EMIT_BIN (8, "-"); break;

  case E_LSL: EMIT_BIN (7, "<<"); break;
  case E_LSR: EMIT_BIN (7, ">>"); break;
  case E_ASR: EMIT_BIN (7, ">>>"); break;
  case E_LT:  EMIT_BIN (7, "<"); break;
  case E_GT:  EMIT_BIN (7, ">"); break;
  case E_LE:  EMIT_BIN (7, "<="); break;
  case E_GE:  EMIT_BIN (7, ">="); break;
  case E_EQ:  EMIT_BIN (7, "=="); break;
  case E_NE:  EMIT_BIN (7, "!="); break;

  case E_AND: EMIT_BIN (6, "&"); break;
    
  case E_XOR: EMIT_BIN (5, "^"); break;

  case E_OR: EMIT_BIN (4, "|"); break;

  case E_QUERY: /* prec = 3 */
    PREC_BEGIN(3);
    _print_expr (fp, e->u.e.l, 3);
    fprintf (fp, " ? ");
    Assert (e->u.e.r->type == E_COLON, "Hmm");
    _print_expr (fp, e->u.e.r->u.e.l, 3);
    fprintf (fp, " : ");
    _print_expr (fp, e->u.e.r->u.e.r, 3);
    PREC_END(3);
    break;

  case E_INT:
    fprintf (fp, "%d", e->u.v);
    break;

  case E_REAL:
    fprintf (fp, "%g", e->u.f);
    break;

  case E_TRUE:
    fprintf (fp, "true");
    break;

  case E_FALSE:
    fprintf (fp, "false");
    break;
    
  case E_VAR:
    ((ActId *)e->u.e.l)->Print (fp);
    break;

  case E_FUNCTION:
  case E_BITFIELD:
  default:
    fatal_error ("Unhandled case!\n");
    break;
  }
}

void print_expr (FILE *fp, Expr *e)
{
  _print_expr (fp, e, 10);
}


Array::Array()
{
  r = NULL;
  dims = 0;
  next = NULL;
  deref = 0;
  expanded = 0;
}

/*------------------------------------------------------------------------
 *
 *  Array::Clone --
 *
 *   Deep copy of array
 *
 *------------------------------------------------------------------------
 */
Array *Array::Clone ()
{
  Array *ret;

  ret = new Array();

  ret->deref = deref;
  ret->expanded = expanded;
  if (next) {
    ret->next = next->Clone ();
  }
  ret->dims = dims;

  if (dims > 0) {
    MALLOC (ret->r, struct range, dims);
    for (int i= 0; i < dims; i++) {
      ret->r[i] = r[i];
    }
  }
  return ret;
}


AExpr *AExpr::Clone()
{
  AExpr *newl, *newr;

  newl = NULL;
  newr = NULL;
  if (l) {
    if (t != AExpr::EXPR /*&& t != AExpr::SUBRANGE*/) {
      newl = l->Clone ();
    }
    else {
      if (0/*t == AExpr::SUBRANGE*/) {
	newl = (AExpr *) ((ActId *)l)->Clone ();
      }
      else {
	/* AExpr::Expr... mm. */
	newl = l;
      }
    }
  }
  if (r) {
    newr = r->Clone ();
  }
  return new AExpr (t, newl, newr);
}



/*------------------------------------------------------------------------
 *
 *   Expand type! 
 *
 *------------------------------------------------------------------------
 */
Type *UserDef::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  UserDef *ux;
  int k, sz, len;
  InstType *x, *p;  
  Array *xa;

  printf ("Hello, expand userdef!\n");
  /* nt = # of specified parameters
     u = expanded instance paramters
  */
  printf ("Expanding userdef, nt=%d\n", nt);

  /* create a new userdef type */
  ux = new UserDef (ns);

  /* set its scope to "expanded" mode */
  ux->I->FlushExpand();
  
#if 0
  if (nt > 0) {
    MALLOC (xu, inst_param, nt);
    for (int i=0; i < nt; i++) {
      xu[i].tp = NULL;
    }
  }
  for (int i=0; i < nt; i++) {
    if (isParamAType (i)) {
      xu[i].xt = u[i].tt->Expand (ns, s);
    }
    else {
      /* XXX: typecheck here */
      xu[i].xi = u[i].tp->Expand (ns, s);
    }
  }
#endif

  /* create bindings for type parameters */
  for (int i=0; i < nt; i++) {
    p = getPortType (-(i+1));

    x = p->Expand (_ns, ux->I); // this is the real type of the
				// parameter

    /* add parameter to the scope */
    ux->AddMetaParam (x, pn[i], (i < mt ? 0 : 1));

    /* get the ValueIdx for the parameter */
    ValueIdx *vx = ux->I->LookupVal (pn[i]);
    vx->init = 1;

    /* allocate space and bind it to a value */
    if (TypeFactory::isPtypeType (p->BaseType())) {
      /* recall: no ptype arrays */
      Assert (!x->arrayInfo(), "No ptype arrays?");
      
      /* alloc */
      vx->idx = ux->I->AllocPType();
      x = u[i].tt->Expand (ns, s);
      Assert (ux->I->issetPType (vx->idx) == 0, "Huh?");
      ux->I->setPType (vx->idx, x);
      Assert (ux->I->getPType (vx->idx) == x, "Huh?!");
    }
    else {
      unsigned int len;
      xa = x->arrayInfo();
      if (xa) {
	len = xa->size();
      }
      else {
	len = 1;
      }

      /* x = expanded type of port parameter
	 xa = expanded array field
      */
      if (TypeFactory::isPIntType (x->BaseType())) {
	/* expand the array expression */

	/*unsigned long val = u[i].tp->Expand (_ns, ux->I);*/
	/* get the value */
	/* bind */
      }
      else if (TypeFactory::isPIntsType (x->BaseType())) {
	vx->idx = ux->I->AllocPInts(len);
	/* get the value */
	/* bind */
      }
      else if (TypeFactory::isPRealType (x->BaseType())) {
	vx->idx = ux->I->AllocPReal(len);
	/* get the value */
	/* bind */
      }
      else if (TypeFactory::isPBoolType (x->BaseType())) {
	vx->idx = ux->I->AllocPBool(len);
	/* get the value */
	/* bind */
      }
      else {
	fatal_error ("Should not be here: meta params only");
      }
	
    }

  }
  

  /*
    Bind template parameters to the ones passed in
  */
  

  /*
     create a name for it!
     (old name)"<"string-from-types">"

     create a fresh scope

     expand out the body of the type

     insert the new type into the namespace into the xt table
  */
  act_error_ctxt (stderr);
  warning ("Need to actually expand the type");

  sz = 0;

  /* type< , , , ... , > + end-of-string */
  sz = strlen (getName()) + 3 + nt;

  for (int i=0; i < nt; i++) {
    InstType *x;
    Array *xa;

    x = getPortType (-(i+1));
    xa = x->arrayInfo();
    if (TypeFactory::isPtypeType (x->BaseType())) {
      if (xa) {
	act_error_ctxt (stderr);
	fatal_error ("ptype array parameters not supported");
      }

      x = u[i].tt;
      /* x is now the value of the parameter */
      sz += strlen (x->BaseType()->getName())  + 2;
      /* might have directions, upto 2 characters worth */
    }
    else {
      /* check array info */
      if (xa) {
	Assert (xa->isExpanded(), "Array info is not expanded");
	sz += 16*xa->size()+2;
      }
      else {
	sz += 16;
      }
    }
  }
  
  char *buf;
  MALLOC (buf, char, sz);
  k = 0;
  buf[k] = '\0';
  snprintf (buf+k, sz, "%s<", getName());
  len = strlen (buf+k);
  k += len;
  sz -= len;

  for (int i=0; i < nt; i++) {
    if (i != 0) {
      snprintf (buf+k, sz, ",");
      len = strlen (buf+k); k += len; sz -= len;
    }
    Assert (sz > 0, "Check");
    x = getPortType (-(i+1));
    xa = x->arrayInfo();
    if (TypeFactory::isPtypeType (x->BaseType())) {
      x = u[i].tt;
      snprintf (buf+k, sz, "%s%s", x->BaseType()->getName(),
		Type::dirstring (x->getDir()));
      len = strlen (buf+k); k += len; sz -= len;
    }
    else {
      if (xa) {
	snprintf (buf+k, sz, "{");
	k++; sz--;
	/* add code here */
	snprintf (buf+k, sz, "}");
	k++; sz--;
	fatal_error ("XXX: fix this array");
      }
      else {
	/* XXX: ok, need to set up value tables */
	if (TypeFactory::isPIntsType (x->BaseType())) {
	  
	}
	else if (TypeFactory::isPIntType (x->BaseType())) {

	}
	else if (TypeFactory::isPBoolType (x->BaseType())) {
	  
	}
	else if (TypeFactory::isPRealType (x->BaseType())) {
	  
	}
      }
    }
    
  }


  return NULL;
}



InstType *InstType::Expand (ActNamespace *ns, Scope *s)
{
  InstType *xit = NULL;
  Type *xt = NULL;

  /* copy in my scope and parent base type. But it must be an expanded
     scope.
     
     NOTE:: An expanded instance type has a NULL scope for variable
     evaluation, since there aren't any variables to evaluate!
  */
  Assert (t, "Missing parent type?");

  act_error_push (t->getName(), NULL, 0);

  /* Expand the core type using template parameters, if any */
  xt = t->Expand (ns, s, nt, u);
  
  /* If parent is user-defined, we need to make sure we have the
     expanded version of this in place!
  */
  xit = new InstType (NULL, xt, 0);
  xit->expanded = 1;

  act_error_pop ();
  return xit;
}
