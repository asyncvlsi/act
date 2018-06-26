/*************************************************************************
 *
 *  Copyright (c) 2017-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <act/act.h>
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

int TypeFactory::isPTypeType (Type *t)
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
  if (isPTypeType (t) ||
      isPIntType (t) ||
      isPIntsType (t) ||
      isPBoolType (t) ||
      isPRealType (t)) {
    return 1;
  }
  return 0;
}

int TypeFactory::isParamType (InstType *it)
{
  return TypeFactory::isParamType (it->BaseType ());
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

    //i->setScope (s);
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


InstType::InstType (Scope *_s, Type *_t, int is_temp)
{
  expanded = 0;
  t = _t;
  nt = 0;
  a = NULL;
  dir = Type::NONE;
  u = NULL;
  //s = _s;
  temp_type = (is_temp ? 1 : 0);
}

/*
  @return 1 if the kth parameter is a type, 0 if it is an AExpr 
*/
int InstType::isParamAType (int k)
{
  Assert (0 <= k && k < nt, "Hmm");
  if (TypeFactory::isChanType (t) || TypeFactory::isPTypeType (t)) {
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
  if (TypeFactory::isPTypeType (x->BaseType ())) {
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
 *
 *               0 = check that the arrays are in fact equal and have
 *               the same index ranges (and range expressions).
 *               Used for checking that port parameters are the same
 *               in the definition and declaration.
 *
 *               1 = checks that arrays are compatible in terms of #
 *               of dimensions, rather than equal.
 *               Used to sanity check connections prior to expanding
 *               parameters. 
 *
 *               2 = check that array ranges have the same size; they
 *               could have different index ranges.
 *               Used to check that two types can be connected.
 *
 *               3 = check that array ranges have the same size,
 *               except for the first dimension.
 *               Used to check that two types can be concatenated in
 *               an array expression.
 * 
 *
 *------------------------------------------------------------------------
 */
int InstType::isEqual (InstType *it, int weak)
{
  if (t != it->t) return 0;   /* same base type */

  if (nt != it->nt) return 0; /* same number of template params, if any */

  /* check that the template parameters of the type are the same */
  for (int i=0; i < nt; i++) {
    if (isParamAType (i)) {
      if (!u[i].tt->isEqual (it->u[i].tt)) return 0;
    }
    else {
      if (!u[i].tp->isEqual (it->u[i].tp)) return 0;
    }
  }

  if ((a && !it->a) || (!a && it->a)) return 0; /* both are either
						   arrays or not
						   arrays */

  /* dimensions must be compatible no matter what */
  if (a && !a->isDimCompatible (it->a)) return 0;

  if (!a || (weak == 1)) return 1; /* we're done */

  if (weak == 0) {
    if (!a->isEqual (it->a, 1)) return 0;
  }
  else if (weak == 2) {
    if (!a->isEqual (it->a, 0)) return 0;
  }
  else if (weak == 3) {
    if (!a->isEqual (it->a, -1)) return 0;
  }
  return 1;
}

void InstType::Print (FILE *fp)
{
  char buf[10240];

  sPrint (buf, 10240);
  fprintf (fp, "%s", buf);
  return;
}
  
void InstType::sPrint (char *buf, int sz)
{
  UserDef *ud;
  InstType *x;
  int k = 0;
  int l;

#define PRINT_STEP				\
  do {						\
    l = strlen (buf+k);				\
    k += l;					\
    sz -= l;					\
    if (sz <= 0) return;			\
  } while (0)
    
#if 0
  /* view cached flag */
  if (temp_type == 0) {
    fprintf (fp, "c:");
  }
#endif
  snprintf (buf+k, sz, "%s", t->getName());
  PRINT_STEP;
  
  if (nt > 0) {
    /* templates are used for int, chan, ptype, and userdef */
    snprintf (buf+k, sz, "<");
    PRINT_STEP;
    
    for (int i=0; i < nt; i++) {
      if (isParamAType (i)) {
	u[i].tt->sPrint (buf+k, sz);
	PRINT_STEP;
      }
      else {
	u[i].tp->sPrint (buf+k, sz);
	PRINT_STEP;
      }
      if (i < nt-1) {
	snprintf (buf+k, sz, ",");
	PRINT_STEP;
      }
    }
    snprintf (buf+k, sz, ">");
    PRINT_STEP;
  }
  switch (dir) {
  case Type::NONE:
    break;
  case Type::IN:
    snprintf (buf+k, sz, "?");
    PRINT_STEP;
    break;
  case Type::OUT:
    snprintf (buf+k, sz, "!");
    PRINT_STEP;
    break;
  case Type::INOUT:
    snprintf (buf+k, sz, "?!");
    PRINT_STEP;
    break;
  case Type::OUTIN:
    snprintf (buf+k, sz, "!?");
    PRINT_STEP;
    break;
  }
  if (a) {
    a->sPrint (buf+k, sz);
    PRINT_STEP;
  }
}

/*
  Clone an InstType
*/
InstType::InstType (InstType *i, int skip_array)
{
  t = i->t;
  dir = i->dir;
  //s = i->s;
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


/*
  tt has to be expanded
*/
void UserDef::BindParam (const char *s, InstType *tt)
{
  /* get the ValueIdx for the parameter */
  int need_alloc = 0;
  ValueIdx *vx = I->LookupVal (s);

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
    vx->u.idx = I->AllocPType();
  }
  if (tt) {
    if (vx->immutable) {
      if (I->issetPType (vx->u.idx)) {
	act_error_ctxt (stderr);
	fatal_error ("Setting immutable parameter `%s' that has already been set", s);
      }
    }
    /* assign */
    I->setPType (vx->u.idx, tt);
    Assert (I->getPType (vx->u.idx) == tt, "Huh?!");
  }
}


/*
  ae has to be expanded
*/
void UserDef::BindParam (const char *s, AExpr *ae)
{
  /* get the ValueIdx for the parameter */
  int need_alloc = 0;
  ValueIdx *vx = I->LookupVal (s);

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
  
  AExpr *rhsval;
  AExprstep *aes;

  if (ae) {
    xrhs = ae->getInstType (I, 1 /* expanded */);
    if (!type_connectivity_check (vx->t, xrhs)) {
      fatal_error ("typechecking failed");
    }
	
    rhsval = ae;
    aes = rhsval->stepper();
  }

  if (TypeFactory::isPIntType (vx->t->BaseType())) {
    unsigned long v;

    vx->u.idx = I->AllocPInt(len); /* allocate */

    if (ae) {
      if (xa) {		/* array assignment */
	int idx;
	Arraystep *as;

	as = xa->stepper();

	while (!as->isend()) {
	  Assert (!aes->isend(), "This should have been caught earlier");
	  
	  idx = as->index();
	  v = aes->getPInt();
	  I->setPInt (vx->u.idx + idx, v);
	    
	  as->step();
	  aes->step();
	}
	Assert (aes->isend(), "What on earth?");
	delete as;
      }
      else {
	aes = rhsval->stepper();
	I->setPInt (vx->u.idx, aes->getPInt());
	aes->step();
	Assert (aes->isend(), "This should have been caught earlier");
      }
    }
  }
  else if (TypeFactory::isPIntsType (vx->t->BaseType())) {
    long v;

    vx->u.idx = I->AllocPInts(len); /* allocate */

    if (ae) {
      if (xa) {		/* array assignment */
	int idx;
	Arraystep *as;
	  
	as = xa->stepper();

	while (!as->isend()) {
	  Assert (!aes->isend(), "This should have been caught earlier");

	  idx = as->index();
	  v = aes->getPInts();
	  I->setPInts (vx->u.idx + idx, v);
	    
	  as->step();
	  aes->step();
	}
	Assert (aes->isend(), "What on earth?");
	delete as;
      }
      else {
	aes = rhsval->stepper();
	I->setPInts (vx->u.idx, aes->getPInts());
	aes->step();
	Assert (aes->isend(), "This should have been caught earlier");
      }
    }
  }
  else if (TypeFactory::isPRealType (vx->t->BaseType())) {
    double v;

    vx->u.idx = I->AllocPReal(len); /* allocate */

    if (ae) {
      if (xa) {		/* array assignment */
	int idx;
	Arraystep *as;
	  
	as = xa->stepper();

	while (!as->isend()) {
	  Assert (!aes->isend(), "This should have been caught earlier");
	  
	  idx = as->index();
	  v = aes->getPReal();
	  I->setPReal (vx->u.idx + idx, v);
	    
	  as->step();
	  aes->step();
	}
	Assert (aes->isend(), "What on earth?");
	delete as;
      }
      else {
	aes = rhsval->stepper();
	I->setPReal (vx->u.idx, aes->getPReal());
	aes->step();
	Assert (aes->isend(), "This should have been caught earlier");
      }
    }
  }
  else if (TypeFactory::isPBoolType (vx->t->BaseType())) {
    int v;

    vx->u.idx = I->AllocPBool(len); /* allocate */

    if (ae) {
      if (xa) {		/* array assignment */
	int idx;
	Arraystep *as;
	  
	as = xa->stepper();

	while (!as->isend()) {
	  Assert (!aes->isend(), "This should have been caught earlier");

	  idx = as->index();
	  v = aes->getPBool();
	  I->setPBool (vx->u.idx + idx, v);
	    
	  as->step();
	  aes->step();
	}
	Assert (aes->isend(), "What on earth?");
	delete as;
      }
      else {
	aes = rhsval->stepper();
	I->setPBool (vx->u.idx, aes->getPBool());
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


static int recursion_depth = 0;
/*------------------------------------------------------------------------
 *
 *   Expand user-defined type! 
 *
 *------------------------------------------------------------------------
 */
Type *UserDef::Expand (ActNamespace *ns, Scope *s, int spec_nt, inst_param *u)
{
  UserDef *ux;
  int k, sz, len;
  InstType *x, *p;  
  Array *xa;

  recursion_depth++;

  
  if (recursion_depth >= Act::max_recurse_depth) {
    act_error_ctxt (stderr);
    fatal_error ("Exceeded maximum recursion depth of %d\n", Act::max_recurse_depth);
  }

  /* nt = # of specified parameters
     u = expanded instance paramters
  */
#if 0
  printf ("Hello, expand userdef!\n");
  printf ("Expanding userdef, nt=%d, spec_nt=%d\n", nt, spec_nt);
#endif

  /* create a new userdef type */
  ux = new UserDef (ns);

  /* set its scope to "expanded" mode */
  ux->I->FlushExpand();
  /* set to pending */
  ux->pending = 1;

  /* create bindings for type parameters */
  for (int i=0; i < nt; i++) {
    p = getPortType (-(i+1));

    x = p->Expand (ns, ux->I); // this is the real type of the
				// parameter

    /* add parameter to the scope */
    ux->AddMetaParam (x, pn[i], (i < mt ? 0 : 1));

    /* get the ValueIdx for the parameter */
    ValueIdx *vx = ux->I->LookupVal (pn[i]);
    vx->init = 1;

    /* allocate space and bind it to a value */
    if (TypeFactory::isPTypeType (p->BaseType())) {
      /* recall: no ptype arrays */
      Assert (!x->arrayInfo(), "No ptype arrays?");
      
      /* alloc */
      vx->u.idx = ux->I->AllocPType();

      if (i < spec_nt && u[i].tt) {
	/* compute value */
	x = u[i].tt->Expand (ns, s);
	Assert (ux->I->issetPType (vx->u.idx) == 0, "Huh?");

	/* assign */
	ux->I->setPType (vx->u.idx, x);
	Assert (ux->I->getPType (vx->u.idx) == x, "Huh?!");
      }
      else { /* skipped parameter */ }
    }
    else {
      InstType *xrhs;
      /* other parameterized types: these could be arrays */

      /* compute the array, if any */
      unsigned int len;
      xa = x->arrayInfo();
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

      AExpr *rhsval;
      AExprstep *aes;

      if (i < spec_nt && u[i].tp) {
	xrhs = u[i].tp->getInstType (s, 1 /* expanded */);
	if (!type_connectivity_check (x, xrhs)) {
	  fatal_error ("typechecking failed");
	}
	
	rhsval = u[i].tp->Expand (ns, s);
	aes = rhsval->stepper();
      }
      else {
	/* no rhs */
      }

      if (TypeFactory::isPIntType (x->BaseType())) {
	unsigned long v;

	vx->u.idx = ux->I->AllocPInt(len); /* allocate */

	if (i < spec_nt && u[i].tp) {
	  if (xa) {		/* array assignment */
	    int idx;
	    Arraystep *as;

	    as = xa->stepper();

	    while (!as->isend()) {
	      Assert (!aes->isend(), "This should have been caught earlier");

	      idx = as->index();
	      v = aes->getPInt();
	      ux->I->setPInt (vx->u.idx + idx, v);
	    
	      as->step();
	      aes->step();
	    }
	    Assert (aes->isend(), "What on earth?");
	    delete as;
	  }
	  else {
	    aes = rhsval->stepper();
	    ux->I->setPInt (vx->u.idx, aes->getPInt());
	    aes->step();
	    Assert (aes->isend(), "This should have been caught earlier");
	  }
	}
      }
      else if (TypeFactory::isPIntsType (x->BaseType())) {
	long v;

	vx->u.idx = ux->I->AllocPInts(len); /* allocate */

	if (i < spec_nt && u[i].tp) {
	  if (xa) {		/* array assignment */
	    int idx;
	    Arraystep *as;
	  
	    as = xa->stepper();

	    while (!as->isend()) {
	      Assert (!aes->isend(), "This should have been caught earlier");

	      idx = as->index();
	      v = aes->getPInts();
	      ux->I->setPInts (vx->u.idx + idx, v);
	    
	      as->step();
	      aes->step();
	    }
	    Assert (aes->isend(), "What on earth?");
	    delete as;
	  }
	  else {
	    aes = rhsval->stepper();
	    ux->I->setPInts (vx->u.idx, aes->getPInts());
	    aes->step();
	    Assert (aes->isend(), "This should have been caught earlier");
	  }
	}
      }
      else if (TypeFactory::isPRealType (x->BaseType())) {
	double v;

	vx->u.idx = ux->I->AllocPReal(len); /* allocate */

	if (i < spec_nt && u[i].tp) {
	  if (xa) {		/* array assignment */
	    int idx;
	    Arraystep *as;
	  
	    as = xa->stepper();

	    while (!as->isend()) {
	      Assert (!aes->isend(), "This should have been caught earlier");

	      idx = as->index();
	      v = aes->getPReal();
	      ux->I->setPReal (vx->u.idx + idx, v);
	    
	      as->step();
	      aes->step();
	    }
	    Assert (aes->isend(), "What on earth?");
	    delete as;
	  }
	  else {
	    aes = rhsval->stepper();
	    ux->I->setPReal (vx->u.idx, aes->getPReal());
	    aes->step();
	    Assert (aes->isend(), "This should have been caught earlier");
	  }
	}
      }
      else if (TypeFactory::isPBoolType (x->BaseType())) {
	int v;

	vx->u.idx = ux->I->AllocPBool(len); /* allocate */

	if (i < spec_nt && u[i].tp) {
	  if (xa) {		/* array assignment */
	    int idx;
	    Arraystep *as;
	  
	    as = xa->stepper();

	    while (!as->isend()) {
	      Assert (!aes->isend(), "This should have been caught earlier");

	      idx = as->index();
	      v = aes->getPBool();
	      ux->I->setPBool (vx->u.idx + idx, v);
	    
	      as->step();
	      aes->step();
	    }
	    Assert (aes->isend(), "What on earth?");
	    delete as;
	  }
	  else {
	    aes = rhsval->stepper();
	    ux->I->setPBool (vx->u.idx, aes->getPBool());
	    aes->step();
	    Assert (aes->isend(), "This should have been caught earlier");
	  }
	}
      }
      else {
	fatal_error ("Should not be here: meta params only");
      }
      if (i < spec_nt && u[i].tp) {
	delete aes;
	delete xrhs;
      }
    }
  }
  
  /*
     create a name for it!
     (old name)"<"string-from-types">"

     create a fresh scope

     expand out the body of the type

     insert the new type into the namespace into the xt table
  */

  sz = 0;

  /* type< , , , ... , > + end-of-string */
  /* for arrays, originally we had {a,b,c}... the expanded values.
     instead, we now have scope#idx
  */
  sz = strlen (getName()) + 3 + nt;

  for (int i=0; i < nt; i++) {
    InstType *x;
    Array *xa;

    x = getPortType (-(i+1));
    xa = x->arrayInfo();
    if (TypeFactory::isPTypeType (x->BaseType())) {
      if (xa) {
	act_error_ctxt (stderr);
	fatal_error ("ptype array parameters not supported");
      }

      x = (i < spec_nt ? u[i].tt : NULL);
      /* x is now the value of the parameter */
      if (x) {
	sz += strlen (x->BaseType()->getName()) + 2;
      }
      /* might have directions, upto 2 characters worth */
    }
    else {
      /* check array info */
      if (i < spec_nt && u[i].tp) {
	if (xa) {
	  Assert (xa->isExpanded(), "Array info is not expanded");
	  sz += 16*xa->size()+2;
	}
	else {
	  sz += 16;
	}
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
    ValueIdx *vx;
    if (i != 0) {
      snprintf (buf+k, sz, ",");
      len = strlen (buf+k); k += len; sz -= len;
    }
    Assert (sz > 0, "Check");
    x = getPortType (-(i+1));
    vx = ux->I->LookupVal (pn[i]);
    xa = x->arrayInfo();
    if (TypeFactory::isPTypeType (x->BaseType())) {
      x = (i < spec_nt ? u[i].tt : NULL);
      if (x) {
	snprintf (buf+k, sz, "%s%s", x->BaseType()->getName(),
		  Type::dirstring (x->getDir()));
	len = strlen (buf+k); k += len; sz -= len;
      }
    }
    else {
      if (i < spec_nt && u[i].tp) {
	if (xa) {
	  Arraystep *as;
	  
	  snprintf (buf+k, sz, "{");
	  k++; sz--;

	  as = xa->stepper();
	  while (!as->isend()) {
	    if (TypeFactory::isPIntType (x->BaseType())) {
	      snprintf (buf+k, sz, "%lu",
			ux->I->getPInt (vx->u.idx + as->index()));
	    }
	    else if (TypeFactory::isPIntsType (x->BaseType())) {
	      snprintf (buf+k, sz, "%ld",
			ux->I->getPInts (vx->u.idx + as->index()));
	    }
	    else if (TypeFactory::isPRealType (x->BaseType())) {
	      snprintf (buf+k, sz, "%g",
			ux->I->getPReal (vx->u.idx + as->index()));
	    }
	    else if (TypeFactory::isPBoolType (x->BaseType())) {
	      snprintf (buf+k, sz, "%d",
			ux->I->getPBool (vx->u.idx + as->index()));
	    }
	    else {
	      fatal_error ("What type is this?");
	    }
	    len = strlen (buf+k); k+= len; sz-= len;
	    as->step();
	    if (!as->isend()) {
	      snprintf (buf+k, sz, ",");
	      k++; sz--;
	    }
	  }
	  delete as;
	  snprintf (buf+k, sz, "}");
	  k++; sz--;
	}
	else {
	  if (TypeFactory::isPIntType (x->BaseType())) {
	    snprintf (buf+k, sz, "%lu", ux->I->getPInt (vx->u.idx));
	  }
	  else if (TypeFactory::isPIntsType (x->BaseType())) {
	    snprintf (buf+k, sz, "%ld", ux->I->getPInts (vx->u.idx));
	  }
	    else if (TypeFactory::isPRealType (x->BaseType())) {
	      snprintf (buf+k, sz, "%g", ux->I->getPReal (vx->u.idx));
	    }
	    else if (TypeFactory::isPBoolType (x->BaseType())) {
	      snprintf (buf+k, sz, "%d", ux->I->getPBool (vx->u.idx));
	    }
	    else {
	      fatal_error ("What type is this?");
	    }
	    len = strlen (buf+k); k+= len; sz-= len;
	}
      }
    }
  }
  snprintf (buf+k, sz, ">");
  k++; sz--;

  Assert (sz >= 0, "Hmmmmm");

  /* now we have the string for the type! */
  UserDef *uy;

  uy = ns->findType (buf);

  if (uy) {
    if (uy->pending) {
      act_error_ctxt (stderr);
      fatal_error ("Recursive construction of type `%s'", buf);
    }
    FREE (buf);
    /* we found one! */
    delete ux;
    recursion_depth--;
    return uy;
  }
  FREE (buf);

  Assert (ns->CreateType (buf, ux), "Huh");

  if (parent) {
    ux->SetParent (parent->Expand (ns, s), parent_eq);
  }
  ux->exported = exported;

  /*-- create ports --*/
  for (int i=0; i < nports; i++) {
    Assert (ux->AddPort (getPortType(i)->Expand (ns, ux->I), getPortName (i)), "What?");
  }

  /*-- expand body --*/
  b->Expandlist (ns, ux->I);

  ux->pending = 0;
  recursion_depth--;
  return ux;
}



/*------------------------------------------------------------------------
 *
 *  Expand instance type!
 *
 *------------------------------------------------------------------------
 */
InstType *InstType::Expand (ActNamespace *ns, Scope *s)
{
  InstType *xit = NULL;
  Type *xt = NULL;

  if (expanded) {
    return this;
  }

  /* copy in my scope and parent base type. But it must be an expanded
     scope.
     
     NOTE:: An expanded instance type has a NULL scope for variable
     evaluation, since there aren't any variables to evaluate!
  */
  Assert (t, "Missing parent type?");

  char *tmp;
  MALLOC (tmp, char, 1024);
  sPrint (tmp, 1024);
  act_error_push (tmp, NULL, 0);

  /* expand template parameters, and then expand the type */
  inst_param *xu;
  int i;

  xu = NULL;
  if (nt > 0) {
    MALLOC (xu, inst_param, nt);
    for (i=0; i < nt; i++) {
      if (isParamAType (i)) {
	xu[i].tt = u[i].tt->Expand (ns, s);
      }
      else {
	xu[i].tp = u[i].tp->Expand (ns, s);
      }
    }
  }

  /* Expand the core type using template parameters, if any */
  xt = t->Expand (ns, s, nt, xu);
  
  /* If parent is user-defined, we need to make sure we have the
     expanded version of this in place!
  */
  xit = new InstType (NULL, xt, 0);
  xit->expanded = 1;

  /* array derefs */
  if (a) {
    xit->MkArray (a->Expand (ns, s));
  }

  FREE (tmp);
  act_error_pop ();

  fprintf (stderr, "expand: ");
  this->Print (stderr);
  fprintf (stderr, " -> ");
  xit->Print (stderr);
  fprintf (stderr, "\n");
  
  return xit;
}
