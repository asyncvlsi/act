/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2017-2019 Rajit Manohar
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
#include <act/act.h>
#include <act/types.h>
#include <act/inst.h>
#include <act/lang.h>
#include <act/body.h>
#include <act/value.h>
#include <act/iter.h>
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

Expr *TypeFactory::expr_true = NULL;
Expr *TypeFactory::expr_false = NULL;
struct iHashtable *TypeFactory::expr_int = NULL;

Expr *const_expr (int val);

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


  /* const expr hash */
  NEW (TypeFactory::expr_true, Expr);
  TypeFactory::expr_true->type = E_TRUE;

  NEW (TypeFactory::expr_false, Expr);
  TypeFactory::expr_false->type = E_FALSE;
  
  TypeFactory::expr_int = ihash_new (32);
}

InstType *TypeFactory::NewBool (Type::direction dir)
{
  Scope *gs = ActNamespace::Global ()->CurScope ();
  Bool *b;

  int i = (int)dir;

  b = NULL;
  for (int j=0; j < sizeof (bools)/sizeof (bools[0]); j++) {
    if (bools[j]) {
      b = dynamic_cast<Bool*> (bools[j]->t);
      break;
    }
  }

  if (!bools[i]) {
    if (!b) {
      b = new Bool();
    }
    bools[i] = new InstType (gs, b, 0);
    bools[i]->SetDir (dir);
  }
  return bools[i];
}

struct inthashkey {
  int sig;			// signed or not
  int w;
};

/*
  sig = is signed?
*/
Int *TypeFactory::NewInt (int sig, int w)
{
  struct inthashkey k;
  chash_bucket_t *b;

  k.sig = sig;
  k.w = w;
  
  b = chash_lookup (inthash, &k);
  if (!b) {
    Int *i = new Int();
    i->kind = sig;
    i->w = w;
    i->name = NULL;
    
    b = chash_add (inthash, &k);
    b->v = i;
  }
  return (Int *)b->v;
}
 
InstType *TypeFactory::NewInt (Scope *s, Type::direction dir, int sig, Expr *w)
{
  static Int *_iu = NULL, *_is = NULL;
  static Int *_ie = NULL;

  if (_iu == NULL) {
    _iu = new Int();
    _iu->kind = 0;
    _iu->name = NULL;
    _iu->w = -1;

    _is = new Int();
    _is->kind = 1;
    _is->name = NULL;
    _is->w = -1;

    _ie = new Int();
    _ie->name = NULL;
    _ie->w = -1;
    _ie->kind = 2;
  }
  
  /* the key should contain the direction and width expression.
     For int<w>, we need to know the *context* of w.. is it the same w
     as another int<w>, or not.
  */
  InstType *i = new InstType (s, (sig == 0 ? _iu : (sig == 1 ? _is : _ie)), 0);
  i->setNumParams (1);
  i->setParam (0, w);
  i->SetDir (dir);

  return i;
}


InstType *TypeFactory::NewEnum (Scope *s, Type::direction dir, Expr *w)
{
  /* enums are just a special type of int. */
  return NewInt (s, dir, 2, w);
}


#define INSTMACRO(isfunc)			\
int TypeFactory::isfunc (InstType *it)		\
{						\
  return TypeFactory::isfunc (it->BaseType());	\
}

#define XINSTMACRO(isfunc)			\
int TypeFactory::isfunc (InstType *it)		\
{						\
 if (!it->isExpanded()) return -1;		\
 return TypeFactory::isfunc (it->BaseType());	\
}


int TypeFactory::isUserType (Type *t)
{
  UserDef *tmp_u = dynamic_cast<UserDef *>(t);
  if (tmp_u) {
    return 1;
  }
  return 0;
}

INSTMACRO(isUserType)

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
  Bool *tmp_b = dynamic_cast<Bool *>(t);
  if (tmp_b) {
    return 1;
  }
  return 0;
}
INSTMACRO(isDataType)

int TypeFactory::isIntType (Type *t)
{
  Int *tmp_i = dynamic_cast<Int *>(t);
  if (tmp_i) {
    return 1;
  }
  return 0;
}
INSTMACRO(isIntType)

int TypeFactory::isPIntType (Type *t)
{
  if (t == TypeFactory::pint->BaseType ()) {
    return 1;
  }
  return 0;
}
INSTMACRO(isPIntType)

int TypeFactory::isPIntsType (Type *t)
{
  if (t == TypeFactory::pints->BaseType ()) {
    return 1;
  }
  return 0;
}
INSTMACRO(isPIntsType)

int TypeFactory::isBoolType (Type *t)
{
  Bool *tmp_b = dynamic_cast<Bool *>(t);
  if (tmp_b) {
    return 1;
  }
  return 0;
}
INSTMACRO(isBoolType)

int TypeFactory::isPBoolType (Type *t)
{
  if (t == TypeFactory::pbool->BaseType ()) {
    return 1;
  }
  return 0;
}
INSTMACRO(isPBoolType)

int TypeFactory::isPRealType (Type *t)
{
  if (t == TypeFactory::preal->BaseType ()) {
    return 1;
  }
  return 0;
}
INSTMACRO(isPRealType)

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
INSTMACRO(isChanType)

int TypeFactory::isExactChanType (Type *t)
{
  Chan *tmp_c = dynamic_cast<Chan *>(t);
  if (tmp_c) {
    return 1;
  }
  return 0;
}
INSTMACRO(isExactChanType)

int TypeFactory::isProcessType (Type *t)
{
  Process *tmp_p = dynamic_cast<Process *>(t);
  if (tmp_p) {
    return 1;
  }
  return 0;
}
INSTMACRO(isProcessType)

int TypeFactory::isFuncType (Type *t)
{
  Function *tmp_f = dynamic_cast<Function *>(t);
  if (tmp_f) {
    return 1;
  }
  return 0;
}
INSTMACRO(isFuncType)

int TypeFactory::isInterfaceType (Type *t)
{
  Interface *tmp_i = dynamic_cast<Interface *>(t);
  if (tmp_i) {
    return 1;
  }
  return 0;
}
INSTMACRO(isInterfaceType)


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
INSTMACRO(isPTypeType)


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
INSTMACRO(isParamType)


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
    prev = hash_function_continue
      (sz, (const unsigned char *)&w->u.fn.s, sizeof (UserDef *), prev, 1);
    {
      Expr *x = w->u.fn.r;
      while (x) {
	prev = expr_hash (sz, x->u.e.l, prev);
	x = x->u.e.r;
      }
    }
    break;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    prev = expr_hash (sz, w->u.e.l, prev);
    prev = expr_hash (sz, w->u.e.r, prev);
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
  sig = k->sig;
  v = hash_function_continue 
    (sz, (const unsigned char *) &sig, 1, 0, 0);

  v = hash_function_continue (sz, (const unsigned char *) &k->w, sizeof (int), v, 1);

  return v;
}

int TypeFactory::intmatchfn (void *key1, void *key2)
{
  struct inthashkey *k1, *k2;
  k1 = (struct inthashkey *)key1;
  k2 = (struct inthashkey *)key2;
  if (k1->sig != k2->sig) return 0;
  if (k1->w != k2->w) return 0;
  return 1;
}

void *TypeFactory::intdupfn (void *key)
{
  struct inthashkey *k, *kret;
  k = (struct inthashkey *) key;
  kret = new inthashkey;
  kret->w = k->w;
  kret->sig = k->sig;
  return kret;
}

void TypeFactory::intfreefn (void *key)
{
  struct inthashkey *k = (struct inthashkey *)key;
  free (k);
}

struct chanhashkey {
  Scope *s;
  Type::direction d;
  InstType *t;			// type
};


int TypeFactory::chanhashfn (int sz, void *key)
{
  int v;
  struct chanhashkey *ch = (struct chanhashkey *)key;

  v = hash_function_continue
    (sz, (const unsigned char *) &ch->s, sizeof (Scope *), 0, 0);
  v = hash_function_continue
    (sz, (const unsigned char *) &ch->d, sizeof (Type::direction), v, 1);
  v = hash_function_continue
    (sz, (const unsigned char *) &ch->t, sizeof (InstType *), v, 1);
  return v;
}

int TypeFactory::chanmatchfn (void *key1, void *key2)
{
  struct chanhashkey *c1, *c2;
  c1 = (struct chanhashkey *)key1;
  c2 = (struct chanhashkey *)key2;

  if (c1->s != c2->s) return 0;
  if (c1->d != c2->d) return 0;
  if (c1->t != c2->t) return 0;
  return 1;
}

void *TypeFactory::chandupfn (void *key)
{
  struct chanhashkey *c, *cret;
  c = (struct chanhashkey *)key;
  cret = new chanhashkey;

  cret->s = c->s;
  cret->d = c->d;
  cret->t = c->t;
  return cret;
}

void TypeFactory::chanfreefn (void *key)
{
  struct chanhashkey *c = (struct chanhashkey *)key;
  free (c);
}



Chan *TypeFactory::NewChan (InstType *l)
{
  struct chanhashkey c;
  chash_bucket_t *b;
  
  c.s = NULL;
  c.d = Type::NONE;
  c.t = l;
  b = chash_lookup (chanhash, &c);
  if (!b) {
    Chan *cx;
    
    b = chash_add (chanhash, &c);
    cx = new Chan();
    cx->name = NULL;
    cx->p = l;
    l->MkCached();

    b->v = cx;
  }
  return (Chan *)b->v;
}

InstType *TypeFactory::NewChan (Scope *s, Type::direction dir, InstType *l)
{
  struct chanhashkey c;
  chash_bucket_t *b;
  static Chan *_c = NULL;

  if (!_c) {
    _c = new Chan();
    _c->p = NULL;
    _c->name = NULL;
  }

  c.s = s;
  c.d = dir;
  c.t = l;

  b = chash_lookup (chanhash, &c);
  if (!b) {
    InstType *ch = new InstType (s, _c, 0);

    ch->setNumParams (1);
    ch->setParam (0, l);
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

  if (_t == NULL) {
    _t = new PType();
  }

  c.s = s;
  c.d = Type::NONE;
  c.t = t;

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

PType *TypeFactory::NewPType (InstType *t)
{
  chash_bucket_t *b;
  struct chanhashkey c;
  
  c.s = NULL;
  c.d = Type::NONE;
  c.t = t;

  b = chash_lookup (ptypehash, &c);

  if (!b) {
    PType *i = new PType();
    i->i = t;

    b = chash_add (ptypehash, &c);
    b->v = i;
  }
  return (PType *)b->v;
}



const char *UserDef::getName()
{
  return name;
}


int UserDef::AddMetaParam (InstType *t, const char *id)
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
  if (expanded) {
    ValueIdx *vx = I->LookupVal (id);
    Assert (vx, "What?");
    /* port parameters are immutable */
    vx->immutable = 1;
  }

  nt++;
  
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

void UserDef::refinePortType (int pos, InstType *u)
{
  Assert (pos >= 0, "Can't refine parameter types");
  Assert (pos < getNumPorts(), "Invalid pos!");
  port_t[pos] = port_t[pos]->refineBaseType (u);
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
  level = ACT_MODEL_PRS;	/* default */
  parent = NULL;

  defined = 0;
  expanded = 0;
  pending = 0;

  nt = 0;
  pt = NULL;
  pn = NULL;
  exported = 0;

  name = NULL;
  
  nports = 0;
  port_t = NULL;
  port_n = NULL;
  unexpanded = NULL;

  b = NULL;

  lang = new act_languages ();

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
  defined  = u->defined; u->defined = 0;
  expanded = u->expanded; u->expanded = 0;
  pending = u->pending; u->pending = 0;

  nt = u->nt; u->nt = 0;
  
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
  _ns = u->_ns; u->_ns = NULL;

  lang = u->lang;

  u->lang = new act_languages ();
}


Process::Process (UserDef *u) : UserDef (*u)
{
  is_cell = 0;
  b = NULL;
  ifaces = NULL;
  has_refinement = 0;

  /* copy over userdef */
  MkCopy (u);
}

Process::~Process ()
{
  if (b) {
    delete b;
  }
  if (ifaces) {
    list_free (ifaces);
    ifaces = NULL;
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


Interface::Interface (UserDef *u) : UserDef (*u)
{
  /* copy over userdef */
  MkCopy (u);
}

Interface::~Interface() { }


void UserDef::SetParent (InstType *t)
{
  parent = t;
}


int UserDef::isStrictPort (const  char *s)
{
  int i;

  /*printf ("mt = %d, nt = %d [%x]\n", mt, nt, this);*/

  for (i=0; i < nt; i++) {
    if (strcmp (s, pn[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int UserDef::isPort (const  char *s)
{
  int i;

  /*printf ("mt = %d, nt = %d [%x]\n", mt, nt, this);*/

  for (i=0; i < nports; i++) {
    if (strcmp (s, port_n[i]) == 0) {
      return 1;
    }
  }
  for (i=0; i < nt; i++) {
    if (strcmp (s, pn[i]) == 0) {
      return 1;
    }
  }
  return 0;
}


Data::Data (UserDef *u) : UserDef (*u)
{
  int i;
  /* copy over userdef */
  is_enum = 0;
  b = NULL;

  for (i=0; i < 2; i++) {
    methods[i] = NULL;
  }

  MkCopy (u);
}

Data::~Data()
{
  
}

Channel::Channel (UserDef *u) : UserDef (*u)
{
  int i;
  /* copy over userdef */
  b = NULL;

  for (i=0; i < 4; i++) {
    methods[i] = NULL;
  }
  for (i=0; i < 2; i++) {
    emethods[i] = NULL;
  }
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
  if (nt != u->nt) return 0;
  if (nports != u->nports) return 0;

  if (exported != u->exported) return 0;
  if (expanded != u->expanded) return 0;

  for (i=0; i < nt; i++) {
    if (pn[i] != u->pn[i]) return 0;
    if (!pt[i]->isEqual (u->pt[i])) return 0;
  }

  for (i=0; i < nports; i++) {
    if (port_n[i] != u->port_n[i]) return 0;
    if (!port_t[i]->isEqualDir (u->port_t[i])) return 0;
  }
  
  return 1;
}



  

static int recursion_depth = 0;

/*------------------------------------------------------------------------
 *
 *   Expand user-defined type! 
 *
 *------------------------------------------------------------------------
 */
UserDef *UserDef::Expand (ActNamespace *ns, Scope *s, int spec_nt, inst_param *u, int *cache_hit)
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
  printf ("Hello, expand userdef %s\n", getName());
  printf ("Expanding userdef, nt=%d, spec_nt=%d\n", nt, spec_nt);
  for (int i=0; i < spec_nt; i++) {
    printf (" param %d: ", i);
    u[i].u.tp->Print (stdout);
    printf (" [u+i=%x]\n", u+i);
  }
#endif

  /* create a new userdef type */
  ux = new UserDef (ns);
  ux->unexpanded = this;

  if (defined) {
    ux->MkDefined();
  }

  /* set its scope to "expanded" mode */
  ux->I->FlushExpand();
  /* set to pending */
  ux->pending = 1;
  ux->expanded = 1;

  InstType *instparent;
  UserDef *uparent;

  instparent = parent;
  if (instparent) {
    uparent = dynamic_cast <UserDef *> (instparent->BaseType());
  }
  else {
    uparent = NULL;
  }

  /* create bindings for type parameters */
  int i, ii;

  ii = 0;
  for (i=0; i < nt; i++) {
#if 0
    printf ("i=%d, ii=%d\n", i, ii);
#endif    
    p = getPortType (-(i+1));
    if (!p)  {
      Assert (0, "Enumeration?");
      /* this is an enumeration type, there is nothing to be done here */
      ux->AddMetaParam (NULL, pn[i]);
    }
    else {
      x = p->Expand (ns, ux->I); // this is the real type of the parameter

      /* add parameter to the scope */
      ux->AddMetaParam (x, pn[i]);

      while (i < nt && uparent && uparent->isPort (getPortName (-(i+1)))) {
#if 0
	printf ("uparent: i=%d, ii=%d\n", i, ii);
#endif
	/* walk through instparent and continue bindings */
	if (instparent->getNumParams() > 0) {
	  /* walk through this list, incrementing i and repeating the
	     param stuff */
	  for (int j=0; j < instparent->getNumParams(); j++) {
#if 0
	    printf ("[j=%d] Bind: %s\n", j, pn[i]);
#endif
	    if (TypeFactory::isPTypeType (p->BaseType())) {
	      x = instparent->getTypeParam (j);
	      x = x->Expand (ns, ux->I);
	      ux->I->BindParam (pn[i], x);
	    }
	    else {
	      InstType *rhstype;
	      AExpr *rhsval = instparent->getAExprParam (j);
	      rhsval = rhsval->Expand (ns, ux->I);
	      rhstype = rhsval->getInstType (ux->I, NULL, 1);
	      if (type_connectivity_check (x, rhstype) != 1) {
		act_error_ctxt (stderr);
		fprintf (stderr, "Typechecking failed, ");
		x->Print (stderr);
		fprintf (stderr, "  v/s ");
		rhstype->Print (stderr);
		fprintf (stderr, "\n\t%s\n", act_type_errmsg());
		exit (1);
	      }
	      ux->I->BindParam (pn[i], rhsval);
	      delete rhstype;
	      delete rhsval;
	    }
	    i++;
	    if (i < nt) {
	      p = getPortType (-(i+1));
	      Assert (p, "Hmm");
	      x = p->Expand (ns, ux->I);
	      ux->AddMetaParam (x, pn[i]);
	    }
	    else {
	      p = NULL;
	    }
	  }
	}
	instparent = uparent->getParent();
	if (instparent) {
	  uparent = dynamic_cast<UserDef *>(instparent->BaseType());
	}
	else {
	  uparent = NULL;
	}
      }
      
#if 0
      printf ("Bind: [%d] %s (ii=%d, spec_nt=%d)\n", i, pn[i], ii, spec_nt);
#endif
      if (i < nt) {
	if (TypeFactory::isPTypeType (p->BaseType())) {
	  if (ii < spec_nt && u[ii].u.tt) {
	    if (u[ii].isatype) {
	      x = u[ii].u.tt /*->Expand (ns, ux->I)*/;
	    }
	    else {
	      x = u[ii].u.tp->isType();
	      if (!x) {
		act_error_ctxt (stderr);
		fprintf (stderr, "Typechecking failed, ");
		u[ii].u.tp->Print (stderr);
		fprintf (stderr, "  v/s ");
		p->Print (stderr);
		exit (1);
	      }
	    }
	    ux->I->BindParam (pn[i], x);
	    ii++;
	  }
	}
	else {
	  Assert (TypeFactory::isParamType (x), "What?");
	  if (ii < spec_nt && u[ii].u.tp) {
	    InstType *rhstype;
	    AExpr *rhsval = u[ii].u.tp /*->Expand (ns, ux->I)*/;
	    rhstype = rhsval->getInstType (s, NULL, 1);
	    if (type_connectivity_check (x, rhstype) != 1) {
	      act_error_ctxt (stderr);
	      fprintf (stderr, "Typechecking failed, ");
	      x->Print (stderr);
	      fprintf (stderr, "  v/s ");
	      rhstype->Print (stderr);
	      fprintf (stderr, "\n\t%s\n", act_type_errmsg());
	      exit (1);
	    }
	    ux->I->BindParam (pn[i], rhsval);
	    ii++;
	    delete rhstype;
	  }
	}
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

    x = ux->getPortType (-(i+1));
    xa = x->arrayInfo();
    if (TypeFactory::isPTypeType (x->BaseType())) {
      if (xa) {
	act_error_ctxt (stderr);
	fatal_error ("ptype array parameters not supported");
      }

      if (i < spec_nt) {
	if (u[i].isatype) {
	  x = u[i].u.tt;
	}
	else {
	  x = u[i].u.tp->isType();
	  if (!x) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Typechecking failed for param #%d ", i);
	    u[i].u.tp->Print (stderr);
	    exit (1);
	  }
	}
      }
      else {
	x = NULL;
      }
      /* x is now the value of the parameter */
      if (x) {
	sz += strlen (x->BaseType()->getName()) + 2;
      }
      /* might have directions, upto 2 characters worth */
    }
    else {
      /* check array info */
      if (i < spec_nt && u[i].u.tp) {
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
    x = ux->getPortType (-(i+1));
    vx = ux->I->LookupVal (pn[i]);
    xa = x->arrayInfo();
    if (TypeFactory::isPTypeType (x->BaseType())) {
      if (i < spec_nt) {
	if (u[i].isatype) {
	  x = u[i].u.tt;
	}
	else {
	  x = u[i].u.tp->isType();
	}	  
      }
      else {
	x = NULL;
      }
      if (x) {
	snprintf (buf+k, sz, "%s%s", x->BaseType()->getName(),
		  Type::dirstring (x->getDir()));
	len = strlen (buf+k); k += len; sz -= len;
      }
    }
    else {
      if (i < spec_nt && u[i].u.tp) {
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
    *cache_hit = 1;
    return uy;
  }
  *cache_hit = 0;

  Assert (ns->CreateType (buf, ux), "Huh");
  FREE (buf);

  if (parent) {
    uparent = dynamic_cast <UserDef *> (parent->BaseType());
    if (uparent) {
      for (i=0; i < spec_nt; i++) {
	if (uparent->isPort (pn[i])) {
	  break;
	}
      }
      if (i < spec_nt) {
	InstType *parent2 = new InstType (parent);
	parent2->appendParams (spec_nt - i, u+i);
	ux->SetParent (parent2->Expand (ns, ux->CurScope()));
	delete parent2;
      }
      else {
	ux->SetParent (parent->Expand (ns, ux->CurScope()));
      }
    }
    else {
      ux->SetParent (parent->Expand (ns, ux->CurScope()));
    }
  }
  if (ux->parent) {
    ux->parent->MkCached ();
  }
  ux->exported = exported;

  /*-- create ports --*/
  for (int i=0; i < nports; i++) {
    Assert (ux->AddPort (getPortType(i)->Expand (ns, ux->I), getPortName (i)), "What?");
    ActId *tmp = new ActId (getPortName (i));
    tmp->Canonical (ux->CurScope());
    delete tmp;
  }

  /*-- XXX: create this, if necessary --*/
  if (dynamic_cast<Channel *>(this)) {
    InstType *x;
    Assert (parent, "Hmm...");
    x = ux->root();

    Chan *ch = dynamic_cast <Chan *>(x->BaseType());
    Assert (ch, "Hmm?!");
    Assert (ch->datatype(), "What?");
    ux->CurScope()->Add ("self", ch->datatype());
  }
  else if (dynamic_cast<Data *>(this)) {
    InstType *x;
    Assert (parent, "Hmm...");
    x = ux->root();
    ux->CurScope()->Add ("self", x);
  }
  else if (dynamic_cast<Function *>(this)) {
    InstType *x = dynamic_cast<Function *>(this)->getRetType();
    x = x->Expand (ns, ux->I);
    ux->CurScope()->Add ("self", x);
  }
  else {
    /* process, no this pointer */
  }

  /*-- expand body --*/
  if (b) {
    b->Expandlist (ns, ux->I);
  }

  ux->pending = 0;
  recursion_depth--;
  return ux;
}


Process *Process::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Process *xp;
  UserDef *ux;
  int cache_hit;

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Process *> (ux);
  }

  xp = new Process (ux);
  delete ux;

  Assert (ns->EditType (xp->name, xp) == 1, "What?");
  xp->is_cell = is_cell;

  if (ifaces) {
    listitem_t *li;
    xp->ifaces = list_new ();
    for (li = list_first (ifaces); li; li = list_next (li)) {
      InstType *iface = (InstType *)list_value (li);
      Assert (list_next (li), "What?");
      list_t *lmap = (list_t *)list_value (list_next (li));
      li = list_next (li);
      list_append (xp->ifaces, iface->Expand (ns, s));
      list_append (xp->ifaces, lmap);
    }
  }
  else {
    xp->ifaces = NULL;
  }  
  return xp;
}

int Process::isBlackBox ()
{
  if (isExpanded()) {
    Assert (unexpanded, "What?");
    return unexpanded->isDefined() && (unexpanded->getBody() == NULL);
  }
  else {
    return isDefined () && (getBody() == NULL);
  }
}

Data *Data::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Data *xd;
  UserDef *ux;
  int cache_hit;
  int i;

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Data *>(ux);
  }

  xd = new Data (ux);
  delete ux;

  Assert (ns->EditType (xd->name, xd) == 1, "What?");
  xd->is_enum = is_enum;

  for (i=0; i < 2; i++) {
    xd->methods[i] = chp_expand (methods[i], ns, xd->CurScope());
  }
  
  return xd;
}


Channel *Channel::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Channel *xc;
  UserDef *ux;
  int cache_hit;
  int i;

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Channel *>(ux);
  }

  xc = new Channel (ux);
  delete ux;

  Assert (ns->EditType (xc->name, xc) == 1, "What?");

  for (i=0; i < 4; i++) {
    xc->methods[i] = chp_expand (methods[i], ns, xc->CurScope());
  }
  for (i=0; i < 2; i++) {
    xc->emethods[i] = expr_expand (emethods[i], ns, xc->CurScope(), 0);
  }

  return xc;
}


Function *Function::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Function *xd;
  UserDef *ux;
  int cache_hit;
  int i;

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Function *>(ux);
  }

  xd = new Function (ux);
  delete ux;

  Assert (ns->EditType (xd->name, xd) == 1, "What?");

  xd->setRetType (ret_type->Expand (ns, s));
  
  return xd;
}

Interface *Interface::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Interface *xd;
  UserDef *ux;
  int cache_hit;
  int i;

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Interface *>(ux);
  }

  xd = new Interface (ux);
  delete ux;

  Assert (ns->EditType (xd->name, xd) == 1, "What?");
  return xd;
}

PType *PType::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Assert (nt == 1, "What?");

  return TypeFactory::Factory()->NewPType (u[0].u.tt->Expand (ns, s));
}

const char *PType::getName ()
{
  if (!i) {
    return "ptype";
  }
  else if (!name) {
    char buf[10240];
    sprintf (buf, "ptype(");
    i->sPrint (buf+strlen (buf), 10240-strlen (buf));
    sprintf (buf+strlen(buf), ")");
    name = Strdup (buf);
  }
  return name;
}



Int *Int::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Int *ix;
  AExpr *ae;
  InstType *it;
  
  Assert (nt == 1, "What?");

  if (u[0].u.tp) {
    ae = u[0].u.tp->Expand (ns, s);
  }
  else {
    ae = new AExpr (const_expr (32));
  }
  it = ae->getInstType (s, NULL);
  if (!TypeFactory::isPIntType (it->BaseType()) ||
      it->arrayInfo()) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Expanding integer type; parameter is not an integer\n");
    fprintf (stderr, " parameter: ");
    ae->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }
  delete it;

  AExprstep *step = ae->stepper();

  ix = TypeFactory::Factory()->NewInt (kind, step->getPInt());

  step->step();
  Assert (step->isend(), "Hmm?");

  return ix;
}

const char *Int::getName()
{
  const char *k2v;

  if (name) return name;
  
  if (kind == 0) {
    k2v = "int";
  }
  else if (kind == 1) {
    k2v = "ints";
  }
  else {
    k2v = "enum";
  }
  
  if (w == -1) {
    name = k2v;
  }
  else {
    char buf[1024];
    sprintf (buf, "%s<%d>", k2v, w);
    name = Strdup (buf);
  }
  return name;
}

const char *Chan::getName ()
{
  char buf[10240];
  if (name) return name;

  if (!p) {
    sprintf (buf, "chan");
  }
  else {
    sprintf (buf, "chan(");
    p->sPrint (buf+strlen (buf), 10239-strlen(buf));
    strcat (buf, ")");
  }
  name = Strdup (buf);
  return name;
}


Chan *Chan::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Chan *cx;
  InstType *cp;

  Assert (nt == 1, "Hmm");
  cp = u[0].u.tt->Expand (ns, s);
  if (TypeFactory::isParamType (cp)) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Parameter to channel type is not a datatype\n");
    fprintf (stderr, " parameter: ");
    cp->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }
  cx = TypeFactory::Factory()->NewChan (cp);
  if (cx->p != cp) {
    delete cp;
  }
  return cx;
}


Expr *TypeFactory::NewExpr (Expr *x)
{
  if (!x) return NULL;

  if (x->type == E_TRUE) {
    /* find unique ETRUE */
    return TypeFactory::expr_true;
  }
  else if (x->type == E_FALSE) {
    /* find unique E_FALSE */
    return TypeFactory::expr_false;
  }
  else if (x->type == E_INT) {
    ihash_bucket_t *b;

    b = ihash_lookup (TypeFactory::expr_int, x->u.v);
    if (b) {
      return (Expr *)b->v;
    }
    else {
      Expr *t;
      b = ihash_add (TypeFactory::expr_int, x->u.v);
      NEW (t, Expr);
      t->type = E_INT;
      t->u.v = x->u.v;
      b->v = t;
      return t;
    }
  }
  else {
    fatal_error ("TypeFactory::NewExpr() called without a constant int/bool expression!");
  }
  return NULL;
}

void UserDef::PrintHeader (FILE *fp, const char *type)
{
  char buf[10240];
  int n;

  if (exported) {
    fprintf (fp, "export ");
  }
  n = getNumParams();
  if (!expanded && n > 0) {
    fprintf (fp, "template <");
    for (int i=0; i < n; i++) {
      InstType *it = getPortType (-(i+1));
      Array *a = it->arrayInfo();
      it->clrArray();
      it->Print (fp);
      fprintf (fp, " %s", getPortName (-(i+1)));
      if (a) {
	a->Print (fp);
      }
      it->MkArray (a);
      if (i != n-1) {
	fprintf (fp, "; ");
      }
    }
    fprintf (fp, ">\n");
  }
  fprintf (fp, "%s ", type);
  if (expanded) {
    ActNamespace::Act()->mfprintfproc (fp, this, 1);
    fprintf (fp, " ");
  }
  else {
    /* ok there is a possibility of a name conflict here but lets not
       mangle more stuff */
    fprintf (fp, "%s ", getName());
  }

  int skip_ports = 0;

  if (parent) {
    fprintf (fp, "<: ");
    if (!expanded) {
      parent->Print (fp);
    }
    else {
      if (!TypeFactory::isUserType (parent)) {
	parent->sPrint (buf, 10240, 1);
	fprintf (fp, "%s", buf);
      }
      else {
	UserDef *u = dynamic_cast<UserDef *>(parent->BaseType());
	Assert (u, "what?");
	ActNamespace::Act()->mfprintfproc (fp, u);
	skip_ports = u->getNumPorts();
      }
    }
    fprintf (fp, " ");
  }

  n = getNumPorts ();
  fprintf (fp, "(");
  if (skip_ports < n) {
    for (int i=skip_ports; i < n; i++) {
      InstType *it = getPortType (i);
      Array *a = it->arrayInfo();
      it->clrArray ();
      if (it->isExpanded()) {
	it->sPrint (buf, 10240, 1);
	ActNamespace::Act()->mfprintf (fp, "%s", buf);
      }
      else {
	it->Print (fp);
      }
      fprintf (fp, " %s", getPortName (i));
      if (a) {
	a->Print (fp);
      }
      it->MkArray (a);
      if (i != n-1) {
	fprintf (fp, "; ");
      }
    }
  }
  fprintf (fp, ")");
}


/*------------------------------------------------------------------------
 *
 *  Returns first built-in type in the parent type hierarchy.
 *
 *     THIS SHOULD ONLY BE CALLED FOR user-defined channels and data
 *     types. 
 *
 *------------------------------------------------------------------------
 */
InstType *UserDef::root ()
{
  if (!parent) {
    return NULL;
  }
  if (TypeFactory::isUserType (parent)) {
    UserDef *ux = dynamic_cast<UserDef *> (parent->BaseType());
    return ux->root();
  }
  return parent;
}


/*------------------------------------------------------------------------
 *  Printing functions
 *------------------------------------------------------------------------
 */
void Process::Print (FILE *fp)
{
  if (isCell()) {
    PrintHeader (fp, "defcell");
  }
  else {
    PrintHeader (fp, "defproc");
  }
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }

  /* print language bodies */
  lang->Print (fp);

  
  fprintf (fp, "}\n\n");
}

void Channel::Print (FILE *fp)
{
  PrintHeader (fp, "defchan");
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }

  lang->Print (fp);

  int firstmeth = 1;

#define EMIT_METHOD_HEADER(id)			\
  do {						\
    if (methods[id]) {				\
      if (firstmeth) {				\
	fprintf (fp, "  methods {\n");		\
      }						\
      firstmeth = 0;				\
    }						\
  } while (0)

#define EMIT_METHOD_EXPRHEADER(id)		\
  do {						\
    if (emethods[id]) {				\
      if (firstmeth) {				\
	fprintf (fp, "  methods {\n");		\
      }						\
      firstmeth = 0;				\
    }						\
  } while (0)

#define EMIT_METHOD(id,name)			\
  do {						\
    EMIT_METHOD_HEADER(id);			\
    if (methods[id]) {				\
      fprintf (fp, "  %s {\n", name);		\
      chp_print (fp, methods[id]);		\
      fprintf (fp, "}\n");			\
    }						\
  } while (0)

#define EMIT_METHODEXPR(id,name)		\
  do {						\
    EMIT_METHOD_EXPRHEADER(id);			\
    if (emethods[id]) {				\
      fprintf (fp, "  %s =", name);		\
      print_expr (fp, emethods[id]);		\
      fprintf (fp, ";\n");			\
    }						\
  } while (0)

  EMIT_METHOD(ACT_METHOD_SET, "set");
  EMIT_METHOD(ACT_METHOD_GET, "get");
  EMIT_METHOD(ACT_METHOD_SEND_REST, "send_rest");
  EMIT_METHOD(ACT_METHOD_RECV_REST, "recv_rest");
  EMIT_METHODEXPR (ACT_METHOD_SEND_PROBE, "send_probe");
  EMIT_METHODEXPR (ACT_METHOD_RECV_PROBE, "recv_probe");
  if (!firstmeth) {
    fprintf (fp, "}\n");
  }
  fprintf (fp, "}\n\n");
}

void Data::Print (FILE *fp)
{
  PrintHeader (fp, "defdata");
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }
  
  lang->Print (fp);

  int firstmeth = 1;

  EMIT_METHOD(ACT_METHOD_SET, "set");
  EMIT_METHOD(ACT_METHOD_GET, "get");

  if (!firstmeth) {
    fprintf (fp, " }\n");
  }
  fprintf (fp, "}\n\n");
}


void UserDef::AppendBody (ActBody *x)
{
  if (b) {
    b->Append (x);
  }
  else {
    b = x;
  }
}


void Data::copyMethods (Data *d)
{
  for (int i=0; i < 2; i++) {
    methods[i] = d->getMethod ((datatype_methods)i);
  }
}

void Channel::copyMethods (Channel *c)
{
  for (int i=0; i < 4; i++) {
    methods[i] = c->getMethod ((datatype_methods)i);
  }
  for (int i=0; i < 2; i++) {
    emethods[i] = c->geteMethod ((datatype_methods)i);
  }
}


int TypeFactory::bitWidth (Type *t)
{
  if (!t) return -1;
  {
    Chan *tmp = dynamic_cast <Chan *>(t);
    if (tmp) {
      /* ok */
      InstType *x = tmp->datatype();
      if (!x->isExpanded()) return -1;
      return TypeFactory::bitWidth (x);
    }
  }
  {
    Channel *tmp = dynamic_cast <Channel *> (t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      return TypeFactory::bitWidth (tmp->getParent());
    }
  }
  { 
    Data *tmp = dynamic_cast<Data *>(t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      return TypeFactory::bitWidth (tmp->getParent());
    }
  }
  {
    Int *tmp = dynamic_cast<Int *>(t);
    if (tmp) {
      return tmp->w;
    }
  }
  {
    Bool *tmp = dynamic_cast<Bool *>(t);
    if (tmp) {
      return 1;
    }
  }
  return -1;
}
XINSTMACRO(bitWidth)

int TypeFactory::boolType (Type *t)
{
  if (!t) return -1;
  {
    Chan *tmp = dynamic_cast <Chan *>(t);
    if (tmp) {
      /* ok */
      InstType *x = tmp->datatype();
      //if (!x->isExpanded()) return -1;
      return TypeFactory::boolType (x);
    }
  }
  {
    Channel *tmp = dynamic_cast <Channel *> (t);
    if (tmp) {
      //if (!tmp->isExpanded()) return -1;
      return TypeFactory::boolType (tmp->getParent());
    }
  }
  { 
    Data *tmp = dynamic_cast<Data *>(t);
    if (tmp) {
      //if (!tmp->isExpanded()) return -1;
      return TypeFactory::boolType (tmp->getParent());
    }
  }
  {
    Int *tmp = dynamic_cast<Int *>(t);
    if (tmp) {
      return 0;
    }
  }
  {
    Bool *tmp = dynamic_cast<Bool *>(t);
    if (tmp) {
      return 1;
    }
  }
  return -1;
}
XINSTMACRO(boolType)



act_prs *UserDef::getprs ()
{
  return lang->getprs();
}

act_spec *UserDef::getspec ()
{
  return lang->getspec();
}

int UserDef::isEqual (Type *t)
{
  UserDef *u;
  u = dynamic_cast<UserDef *>(t);
  if (!u) return 0;
  if (u == this) return 1;
  return 0;
}

int Int::isEqual (Type *t)
{
  Int *x = dynamic_cast<Int *>(t);
  if (!x) return 0;
  if (x->kind == kind && x->w == w) return 1;
  return 0;
}
  
int Chan::isEqual (Type *t)
{
  Chan *x = dynamic_cast<Chan *>(t);
  if (!x) return 0;
  if (x == this) return 1;
  if (!p && !x->p) return 1;
  if (!p || !x->p) return 0;

  if (p->isExpanded()) {
    if (p->isEqual (x->p)) return 1;
  }
  else {
    if (p->isEqual (x->p, 1)) return 1;
  }
  return 0;
}



int UserDef::isLeaf ()
{
  ActInstiter i(I);

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = (*i);
    if (TypeFactory::isProcessType (vx->t)) {
      return 0;
    }
  }
  return 1;
}

static void _run_chp (Scope *s, act_chp_lang_t *c)
{
  int loop_cnt = 0;
  
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      _run_chp (s, (act_chp_lang_t *) list_value (li));
    }
    break;

  case ACT_CHP_COMMALOOP:
  case ACT_CHP_SEMILOOP:
    {
      int ilo, ihi;
      ValueIdx *vx;
      act_syn_loop_setup (NULL, s, c->u.loop.id, c->u.loop.lo, c->u.loop.hi,
			  &vx, &ilo, &ihi);

      for (int iter=ilo; iter <= ihi; iter++) {
	s->setPInt (vx->u.idx, iter);
	_run_chp (s, c->u.loop.body);
      }
      act_syn_loop_teardown (NULL, s, c->u.loop.id, vx);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    {
      act_chp_gc_t *gc;
      for (gc = c->u.gc; gc; gc = gc->next) {
	Expr *guard;
	if (gc->id) {
	  ValueIdx *vx;
	  int ilo, ihi;

	  act_syn_loop_setup (NULL, s, gc->id, gc->lo, gc->hi,
			      &vx, &ilo, &ihi);
	
	  for (int iter=ilo; iter <= ihi; iter++) {
	    s->setPInt (vx->u.idx, iter);
	    guard = expr_expand (gc->g, NULL, s);
	    if (!guard || guard->type == E_TRUE) {
	      /* guard is true */
	      _run_chp (s, gc->s);
	      act_syn_loop_teardown (NULL, s, gc->id, vx);
	      return;
	    }
	    if (!expr_is_a_const (guard)) {
	      act_error_ctxt (stderr);
	      fatal_error ("Guard is not a constant expression?");
	    }
	  }
	  act_syn_loop_teardown (NULL, s, gc->id, vx);
	}
	else {
	  guard = expr_expand (gc->g, NULL, s);
	  if (!guard || guard->type == E_TRUE) {
	    _run_chp (s, gc->s);
	    return;
	  }
	  if (!expr_is_a_const (guard)) {
	    act_error_ctxt (stderr);
	    fatal_error ("Guard is not a constant expression?");
	  }
	}
      }
      act_error_ctxt (stderr);
      fatal_error ("In a function call: all guards are false!");
    }
    break;

  case ACT_CHP_LOOP:
    while (1) {
      loop_cnt++;
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	Expr *guard;
	if (gc->id) {
	  ValueIdx *vx;
	  int ilo, ihi;
	  
	  act_syn_loop_setup (NULL, s, gc->id, gc->lo, gc->hi,
			      &vx, &ilo, &ihi);
	
	  for (int iter=ilo; iter <= ihi; iter++) {
	    s->setPInt (vx->u.idx, iter);
	    guard = expr_expand (gc->g, NULL, s);
	    if (!guard || guard->type == E_TRUE) {
	      /* guard is true */
	      _run_chp (s, gc->s);
	      act_syn_loop_teardown (NULL, s, gc->id, vx);
	      goto resume;
	    }
	    if (!expr_is_a_const (guard)) {
	      act_error_ctxt (stderr);
	      fatal_error ("Guard is not a constant expression?");
	    }
	  }
	  act_syn_loop_teardown (NULL, s, gc->id, vx);
	}
	else {
	  guard = expr_expand (gc->g, NULL, s);
	  if (!guard || guard->type == E_TRUE) {
	    _run_chp (s, gc->s);
	    goto resume;
	  }
	  if (!expr_is_a_const (guard)) {
	    act_error_ctxt (stderr);
	    fatal_error ("Guard is not a constant expression?");
	  }
	}
      }
      /* all guards false */
      return;
    resume:
      if (loop_cnt > Act::max_loop_iterations) {
	fatal_error ("# of loop iterations exceeded limit (%d)", Act::max_loop_iterations);
      }
      ;
    }
    break;

  case ACT_CHP_DOLOOP:
    {
      Expr *guard;
      Assert (c->u.gc->next == NULL, "What?");
      do {
	_run_chp (s, c->u.gc->s);
	guard = expr_expand (c->u.gc->g, NULL, s);
      } while (!guard || guard->type == E_TRUE);
      if (!expr_is_a_const (guard)) {
	act_error_ctxt (stderr);
	fatal_error ("Guard is not a constant expression?");
      }
    }
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    {
      Expr *e = expr_expand (c->u.assign.e, NULL, s);
      ActId *id = expand_var_write (c->u.assign.id, NULL, s);
      if (!expr_is_a_const (e)) {
	act_error_ctxt (stderr);
	fatal_error ("Assignment: expression is not a constant?");
      }
      AExpr *ae = new AExpr (e);
      s->BindParam (id, ae);
      delete ae;
    }      
    break;
    
  case ACT_CHP_SEND:
    act_error_ctxt (stderr);
    fatal_error ("Channels in a function?");
    break;
    
  case ACT_CHP_RECV:
    act_error_ctxt (stderr);
    fatal_error ("Channels in a function?");
    break;

  case ACT_CHP_FUNC:
    /* built-in functions; skip */
    break;
    
  default:
    break;
  }
}

Expr *Function::eval (ActNamespace *ns, int nargs, Expr **args)
{
  Assert (nargs == getNumParams(), "What?");
  
  for (int i=0; i < nargs; i++) {
      Assert (expr_is_a_const (args[i]), "Argument is not a constant?");
  }

  /* 
     now we allocate all the parameters within the function scope
     and bind them to the specified values.
  */
    
  /* evaluate function!
     1. Bind parameters 
  */

  if (pending) {
    fatal_error ("Sorry, recursive functions (`%s') not supported.",
		 getName());
  }

  /* XXX we could cache the allocated state, maybe later...*/
  
  I->FlushExpand ();
  pending = 1;
  expanded = 1;

  ValueIdx *vx;
      
  for (int i=0; i < getNumParams(); i++) {
    InstType *it;
    const char *name;
    it = getPortType (-(i+1));
    name = getPortName (-(i+1));
    
    I->Add (name, it);
    vx = I->LookupVal (name);
    Assert (vx, "Hmm");
    vx->init = 1;
    if (TypeFactory::isPIntType (it)) {
      vx->u.idx = I->AllocPInt();
    }
    else if (TypeFactory::isPBoolType (it)) {
      vx->u.idx = I->AllocPBool();
    }
    else if (TypeFactory::isPRealType (it)) {
      vx->u.idx = I->AllocPReal();
    }
    else {
      fatal_error ("Invalid type in function signature");
    }
    AExpr *ae = new AExpr (args[i]);
    I->BindParam (name, ae);
    delete ae;
  }

  I->Add ("self", getRetType ()->Expand (ns, I));
  vx = I->LookupVal ("self");
  Assert (vx, "Hmm");
  vx->init = 1;
  if (TypeFactory::isPIntType (getRetType())) {
    vx->u.idx = I->AllocPInt();
  }
  else if (TypeFactory::isPBoolType (getRetType())) {
    vx->u.idx = I->AllocPBool();
  }
  else if (TypeFactory::isPRealType (getRetType())) {
    vx->u.idx = I->AllocPReal();
  }
  else {
    fatal_error ("Invalid return type in function signature");
  }
  

  /* now run the chp body */
  act_chp *c = NULL;
  
  if (b) {
    ActBody *btmp;

    for (btmp = b; btmp; btmp = btmp->Next()) {
      ActBody_Lang *l;
      if (!(l = dynamic_cast<ActBody_Lang *>(btmp))) {
	btmp->Expand (ns, I);
      }
      else {
	Assert (l->gettype() == ActBody_Lang::LANG_CHP, "What?");
	c = (act_chp *)l->getlang();
      }
    }
  }
  
  /* run the chp */
  Assert (c, "Isn't this required?!");
  _run_chp (I, c->c);

  pending = 0;

  Expr *ret;

  if (TypeFactory::isPIntType (getRetType())) {
    if (I->issetPInt (vx->u.idx)) {
      ret = const_expr (I->getPInt (vx->u.idx));
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("self is not assigned!");
    }
  }
  else if (TypeFactory::isPBoolType (getRetType())) {
    if (I->issetPBool (vx->u.idx)) {
      ret = const_expr_bool (I->getPBool (vx->u.idx));
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("self is not assigned!");
    }
  }
  else if (TypeFactory::isPRealType (getRetType())) {
    if (I->issetPReal (vx->u.idx)) {
      ret = const_expr_real (I->getPReal (vx->u.idx));
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("self is not assigned!");
      ret = NULL;
    }
  }
  else {
    fatal_error ("Invalid return type in function signature");
    ret = NULL;
  }
  return ret;
}



void Process::addIface (InstType *iface, list_t *lmap)
{
  if (!ifaces) {
    ifaces = list_new ();
  }
  list_append (ifaces, iface);
  list_append (ifaces, lmap);
}

int Process::hasIface (InstType *x, int weak)
{
  listitem_t *li;
  if (!ifaces) return 0;
  for (li = list_first (ifaces); li; li = list_next (li)) {
    InstType *itmp = (InstType *)list_value (li);
    Assert (itmp, "What?");
    Interface *tmp = dynamic_cast <Interface *>(itmp->BaseType());
    Assert (tmp, "What?");
    if (itmp->isEqual (x, weak)) {
      return 1;
    }
    li = list_next (li);
  }
  return 0;
}

list_t *Process::findMap (InstType *x)
{
  listitem_t *li;

  if (!ifaces) return NULL;

  Array *xtmp = x->arrayInfo();
  x->clrArray();
  
  for (li = list_first (ifaces); li; li = list_next (li)) {
    InstType *itmp = (InstType *)list_value (li);
    Assert (itmp, "What?");
    Interface *tmp = dynamic_cast <Interface *>(itmp->BaseType());
    Assert (tmp, "What?");

    if (itmp->isEqual (x)) {
      x->MkArray (xtmp);
      return (list_t *)list_value (list_next (li));
    }
    li = list_next (li);
  }
  x->MkArray (xtmp);
  return NULL;
}
