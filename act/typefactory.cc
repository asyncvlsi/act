/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
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
#include <act/types.h>
#include <common/int.h>

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
struct cHashtable *TypeFactory::exprH = NULL;
struct cHashtable *TypeFactory::idH = NULL;


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
  InstType *i = new InstType (s, (sig == 0 ? _iu : (sig == 1 ? _is : _ie)));
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
int TypeFactory::isfunc (const InstType *it)		\
{						\
  return TypeFactory::isfunc (it->BaseType());	\
}

#define XINSTMACRO(isfunc)			\
int TypeFactory::isfunc (const InstType *it)		\
{						\
 if (!it->isExpanded()) return -1;		\
 return TypeFactory::isfunc (it->BaseType());	\
}


int TypeFactory::isUserType (const Type *t)
{
  const UserDef *tmp_u = dynamic_cast<const UserDef *>(t);
  if (tmp_u) {
    return 1;
  }
  return 0;
}

INSTMACRO(isUserType)

int TypeFactory::isDataType (const Type *t)
{
  const Data *tmp_d = dynamic_cast<const Data *>(t);
  if (tmp_d) {
    if (TypeFactory::isStructure (t)) {
      return 0;
    }
    return 1;
  }
  const Int *tmp_i = dynamic_cast<const Int *>(t);
  if (tmp_i) {
    return 1;
  }
  const Bool *tmp_b = dynamic_cast<const Bool *>(t);
  if (tmp_b) {
    return 1;
  }
  return 0;
}
INSTMACRO(isDataType)

int TypeFactory::isStructure (const Type *t)
{
  const Data *tmp_d = dynamic_cast<const Data *>(t);
  if (!tmp_d) {
    return 0;
  }
  if (tmp_d->getParent()) {
    return TypeFactory::isStructure (tmp_d->getParent());
  }
  else {
    /* no parent, so structure! */
    return 1;
  }
}
INSTMACRO(isStructure)


int TypeFactory::isUserEnum (const Type *t)
{
  const Data *tmp_d = dynamic_cast<const Data *>(t);
  if (tmp_d) {
    if (tmp_d->isEnum()) {
      return 1;
    }
    else {
      return 0;
    }
  }
  return 0;
}
INSTMACRO(isUserEnum)

int TypeFactory::isUserPureEnum (const Type *t)
{
  const Data *tmp_d = dynamic_cast<const Data *>(t);
  if (tmp_d) {
    if (tmp_d->isPureEnum()) {
      return 1;
    }
    else {
      return 0;
    }
  }
  return 0;
}

INSTMACRO(isUserPureEnum)


int TypeFactory::isEnum (const Type *t)
{
  const Data *tmp_d = dynamic_cast<const Data *>(t);
  if (tmp_d) {
    if (tmp_d->isEnum()) {
      return 1;
    }
    else {
      return 0;
    }
  }
  const Int *tmp_i = dynamic_cast<const Int *> (t);
  if (tmp_i) {
    if (tmp_i->kind == 2) {
      return 1;
    }
  }
  return 0;
}
INSTMACRO(isEnum)


  
int TypeFactory::isIntType (const Type *t)
{
  const Int *tmp_i = dynamic_cast<const Int *>(t);
  if (tmp_i) {
    return 1;
  }
  return 0;
}
INSTMACRO(isIntType)

int TypeFactory::isPIntType (const Type *t)
{
  if (t == TypeFactory::pint->BaseType ()) {
    return 1;
  }
  return 0;
}
INSTMACRO(isPIntType)

int TypeFactory::isPIntsType (const Type *t)
{
  if (t == TypeFactory::pints->BaseType ()) {
    return 1;
  }
  return 0;
}
INSTMACRO(isPIntsType)

int TypeFactory::isBoolType (const Type *t)
{
  const Bool *tmp_b = dynamic_cast<const Bool *>(t);
  if (tmp_b) {
    return 1;
  }
  return 0;
}
INSTMACRO(isBoolType)

int TypeFactory::isPBoolType (const Type *t)
{
  if (t == TypeFactory::pbool->BaseType ()) {
    return 1;
  }
  return 0;
}
INSTMACRO(isPBoolType)

int TypeFactory::isPRealType (const Type *t)
{
  if (t == TypeFactory::preal->BaseType ()) {
    return 1;
  }
  return 0;
}
INSTMACRO(isPRealType)

int TypeFactory::isChanType (const Type *t)
{
  const Chan *tmp_c = dynamic_cast<const Chan *>(t);
  if (tmp_c) {
    return 1;
  }
  const Channel *tmp_uc = dynamic_cast<const Channel *>(t);
  if (tmp_uc) {
    return 1;
  }
  return 0;
}
INSTMACRO(isChanType)

int TypeFactory::isExactChanType (const Type *t)
{
  const Chan *tmp_c = dynamic_cast<const Chan *>(t);
  if (tmp_c) {
    return 1;
  }
  return 0;
}
INSTMACRO(isExactChanType)

int TypeFactory::isPureStruct (const Type *t)
{
  if (!isStructure (t)) {
    return 0;
  }
  const Data *x = dynamic_cast<const Data *>(t);
  
  /* -- now check that this is a structure with only data fields! -- */
  for (int i=0; i < x->getNumPorts(); i++) {
    InstType *it = x->getPortType (i);
    if (isStructure (it)) {
      if (!isPureStruct (x->getPortType(i)->BaseType())) {
	return 0;
      }
    }
    else if (!(TypeFactory::isIntType (it) ||
	       TypeFactory::isBoolType (it) ||
	       TypeFactory::isEnum (it))) {
      return 0;
    }
  }
  return 1;
}
INSTMACRO(isPureStruct)
  

int TypeFactory::isValidChannelDataType (const Type *t)
{
  const Data *x = dynamic_cast<const Data *>(t);
  if (!x) {
    /* is an int/enum/bool */
    return 1;
  }
  if (!isStructure (t)) {
    return 0;
  }

  /* -- now check that this is a structure with only data fields! -- */
  for (int i=0; i < x->getNumPorts(); i++) {
    if (!isValidChannelDataType (x->getPortType(i)->BaseType())) {
      return 0;
    }
#if 0
    if (x->getPortType(i)->arrayInfo()) {
      return 0;
    }
#endif
  }

  if (x->getParent()) {
    if (!isValidChannelDataType (x->getParent())) {
      return 0;
    }
  }
  return 1;
}
INSTMACRO(isValidChannelDataType);


int TypeFactory::isProcessType (const Type *t)
{
  const Process *tmp_p = dynamic_cast<const Process *>(t);
  if (tmp_p) {
    return 1;
  }
  return 0;
}
INSTMACRO(isProcessType)

int TypeFactory::isFuncType (const Type *t)
{
  const Function *tmp_f = dynamic_cast<const Function *>(t);
  if (tmp_f) {
    return 1;
  }
  return 0;
}
INSTMACRO(isFuncType)

int TypeFactory::isInterfaceType (const Type *t)
{
  const Interface *tmp_i = dynamic_cast<const Interface *>(t);
  if (tmp_i) {
    return 1;
  }
  return 0;
}
INSTMACRO(isInterfaceType)


int TypeFactory::isPTypeType (const Type *t)
{
  const PType *tmp_t = dynamic_cast<const PType *>(t);
  if (tmp_t) {
    return 1;
  }
  else {
    return 0;
  }
}
INSTMACRO(isPTypeType)


int TypeFactory::isParamType (const Type *t)
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


#if 0
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
    if (w->u.e.r->u.e.l) {
      prev = expr_hash (sz, w->u.e.r->u.e.l, prev);
    }
    else {
      prev = expr_hash (sz, w->u.e.r->u.e.r, prev);
    }
    prev = expr_hash (sz, w->u.e.r->u.e.r, prev);
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
      (sz, (const unsigned char *) &w->u.ival.v, sizeof (unsigned int), prev, 1);
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
#endif

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
  InstType *ack;
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
  v = hash_function_continue
    (sz, (const unsigned char *) &ch->ack, sizeof (InstType *), v, 1);
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
  if (c1->ack != c2->ack) return 0;
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
  cret->ack = c->ack;
  return cret;
}

void TypeFactory::chanfreefn (void *key)
{
  struct chanhashkey *c = (struct chanhashkey *)key;
  free (c);
}



Chan *TypeFactory::NewChan (InstType *l, InstType *ack)
{
  struct chanhashkey c;
  chash_bucket_t *b;
  
  c.s = NULL;
  c.d = Type::NONE;
  c.t = l;
  c.ack = ack;
  b = chash_lookup (chanhash, &c);
  if (!b) {
    Chan *cx;
    
    b = chash_add (chanhash, &c);
    cx = new Chan();
    cx->name = NULL;
    cx->p = l;
    cx->ack = ack;
    l->MkCached();
    if (ack) {
      ack->MkCached();
    }
    b->v = cx;
  }
  return (Chan *)b->v;
}

InstType *TypeFactory::NewChan (Scope *s, Type::direction dir, InstType *l,
				InstType *ack)
{
  struct chanhashkey c;
  chash_bucket_t *b;
  
  c.s = s;
  c.d = dir;
  c.t = l;
  c.ack = ack;

  b = chash_lookup (chanhash, &c);
  if (!b) {
    Chan *_c;
    
    _c = new Chan();
    _c->p = l;
    _c->ack = ack;
    _c->name = NULL;

    InstType *ch = new InstType (s, _c, 0);

    if (ack) {
      ch->setNumParams (2);
    }
    else {
      ch->setNumParams (1);
    }
    ch->setParam (0, l);
    if (ack) {
      ch->setParam (1, ack);
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

  if (_t == NULL) {
    _t = new PType();
  }

  c.s = s;
  c.d = Type::NONE;
  c.t = t;
  c.ack = NULL;

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
  c.ack = NULL;

  b = chash_lookup (ptypehash, &c);

  if (!b) {
    PType *i = new PType();
    i->i = t;

    b = chash_add (ptypehash, &c);
    b->v = i;
  }
  return (PType *)b->v;
}


static int _ceil_log2 (int w)
{
  int i;
  int addone = 0;

  i = 0;
  while (w > 1) {
    if (w & 1) {
      addone = 1;
    }
    w = w >> 1;
    i = i + 1;
  }
  return i + addone;
}

int TypeFactory::bitWidth (const Type *t)
{
  if (!t) return -1;
  {
    const Chan *tmp = dynamic_cast <const Chan *>(t);
    if (tmp) {
      /* ok */
      InstType *x = tmp->datatype();
      if (!x->isExpanded()) return -1;
      return TypeFactory::bitWidth (x);
    }
  }
  {
    const Channel *tmp = dynamic_cast <const Channel *> (t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      return TypeFactory::bitWidth (tmp->getParent());
    }
  }
  { 
    const Data *tmp = dynamic_cast<const Data *>(t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      if (tmp->getParent ()) {
	return TypeFactory::bitWidth (tmp->getParent());
      }
      else {
	/* bitwidth of a structure */
	return -1;
      }
    }
  }
  {
    const Int *tmp = dynamic_cast<const Int *>(t);
    if (tmp) {
      if (tmp->kind == 2) {
	return _ceil_log2 (tmp->w);
      }
      else {
	return tmp->w;
      }
    }
  }
  {
    const Bool *tmp = dynamic_cast<const Bool *>(t);
    if (tmp) {
      return 1;
    }
  }
  return -1;
}
XINSTMACRO(bitWidth)


static int _count_struct_width (const Type *t)
{
  int w = 0;
  Assert (TypeFactory::isStructure (t), "need a structure!");
  const Data *d = dynamic_cast<const Data *>(t);
  Assert (d, "huh");
  for (int i=0; i < d->getNumPorts(); i++) {
    InstType *it = d->getPortType (i);
    int sz;
    if (it->arrayInfo()) {
      sz = it->arrayInfo()->size();
    }
    else {
      sz = 1;
    }
    w += sz*TypeFactory::totBitWidth (it->BaseType());
  }
  return w;
}


int TypeFactory::totBitWidth (const Type *t)
{
  if (!t) return -1;
  {
    const Chan *tmp = dynamic_cast <const Chan *>(t);
    if (tmp) {
      /* ok */
      InstType *x = tmp->datatype();
      if (!x->isExpanded()) return -1;
      return TypeFactory::totBitWidth (x);
    }
  }
  {
    const Channel *tmp = dynamic_cast <const Channel *> (t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      return TypeFactory::totBitWidth (tmp->getParent());
    }
  }
  {
    const Data *tmp = dynamic_cast<const Data *>(t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      if (tmp->getParent ()) {
	return TypeFactory::totBitWidth (tmp->getParent());
      }
      else {
	/* bitwidth of a structure */
	return _count_struct_width (tmp);
      }
    }
  }
  {
    const Int *tmp = dynamic_cast<const Int *>(t);
    if (tmp) {
      if (tmp->kind == 2) {
	return _ceil_log2 (tmp->w);
      }
      else {
	return tmp->w;
      }
    }
  }
  {
    const Bool *tmp = dynamic_cast<const Bool *>(t);
    if (tmp) {
      return 1;
    }
  }
  return -1;
}
XINSTMACRO(totBitWidth)

static int _count_struct_width_raw (const Type *t)
{
  int w = 0;
  const UserDef *d = dynamic_cast<const UserDef *>(t);
  Assert (d, "huh");
  for (int i=0; i < d->getNumPorts(); i++) {
    InstType *it = d->getPortType (i);
    int sz;
    if (it->arrayInfo()) {
      sz = it->arrayInfo()->size();
    }
    else {
      sz = 1;
    }
    w += sz*TypeFactory::totBitWidthSpecial (it->BaseType());
  }
  return w;
}


int TypeFactory::totBitWidthSpecial (const Type *t)
{
  if (!t) return -1;
  {
    const UserDef *tmp = dynamic_cast <const UserDef *>(t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      if (tmp->getParent ()) {
	return TypeFactory::totBitWidth (tmp->getParent());
      }
      else {
	/* bitwidth of a structure */
	return _count_struct_width_raw (tmp);
      }
    }
  }
  {
    const Int *tmp = dynamic_cast<const Int *>(t);
    if (tmp) {
      if (tmp->kind == 2) {
	return _ceil_log2 (tmp->w);
      }
      else {
	return tmp->w;
      }
    }
  }
  {
    const Bool *tmp = dynamic_cast<const Bool *>(t);
    if (tmp) {
      return 1;
    }
  }
  {
    const Chan *tmp = dynamic_cast<const Chan *> (t);
    if (tmp) {
      return 0;
    }
  }
  fatal_error ("Should not be here!");
  return -1;
}
XINSTMACRO(totBitWidthSpecial)

int TypeFactory::enumNum (const Type *t)
{
  if (!t) return -1;
  {
    const Chan *tmp = dynamic_cast <const Chan *>(t);
    if (tmp) {
      /* ok */
      InstType *x = tmp->datatype();
      if (!x->isExpanded()) return -1;
      return TypeFactory::enumNum (x);
    }
  }
  {
    const Channel *tmp = dynamic_cast <const Channel *> (t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      return TypeFactory::enumNum (tmp->getParent());
    }
  }
  { 
    const Data *tmp = dynamic_cast<const Data *>(t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      if (tmp->getParent ()) {
	return TypeFactory::enumNum (tmp->getParent());
      }
      else {
	/* bitwidth of a structure */
	return -1;
      }
    }
  }
  {
    const Int *tmp = dynamic_cast<const Int *>(t);
    if (tmp) {
      if (tmp->kind == 2) {
	return tmp->w;
      }
      else {
	return -1;
      }
    }
  }
  {
    const Bool *tmp = dynamic_cast<const Bool *>(t);
    if (tmp) {
      return -1;
    }
  }
  return -1;
}
XINSTMACRO(enumNum)
  

int TypeFactory::boolType (const Type *t)
{
  if (!t) return -1;
  {
    const Chan *tmp = dynamic_cast <const Chan *>(t);
    if (tmp) {
      /* ok */
      InstType *x = tmp->datatype();
      //if (!x->isExpanded()) return -1;
      return TypeFactory::boolType (x);
    }
  }
  {
    const Channel *tmp = dynamic_cast <const Channel *> (t);
    if (tmp) {
      //if (!tmp->isExpanded()) return -1;
      return TypeFactory::boolType (tmp->getParent());
    }
  }
  { 
    const Data *tmp = dynamic_cast<const Data *>(t);

    if (tmp) {
      //if (!tmp->isExpanded()) return -1;
      if (tmp->getParent()) {
	return TypeFactory::boolType (tmp->getParent());
      }
      else {
	return -1;
      }
    }
  }
  {
    const Int *tmp = dynamic_cast<const Int *>(t);
    if (tmp) {
      return 0;
    }
  }
  {
    const Bool *tmp = dynamic_cast<const Bool *>(t);
    if (tmp) {
      return 1;
    }
  }
  return -1;
}

int TypeFactory::boolType (const InstType *t)
{
  if (!t) return -1;
  {
    const Chan *tmp = dynamic_cast <const Chan *>(t->BaseType());
    if (tmp) {
      /* ok */
      InstType *x = tmp->datatype();
      //if (!x->isExpanded()) return -1;
      return TypeFactory::boolType (x);
    }
  }
  {
    const Channel *tmp = dynamic_cast <const Channel *> (t->BaseType());
    if (tmp) {
      //if (!tmp->isExpanded()) return -1;
      return TypeFactory::boolType (tmp->getParent());
    }
  }
  { 
    const Data *tmp = dynamic_cast<const Data *>(t->BaseType());

    if (tmp) {
      //if (!tmp->isExpanded()) return -1;
      if (tmp->getParent()) { 
	return TypeFactory::boolType (tmp->getParent());
      }
      else {
	return -1;
      }
    }
  }
  {
    const Int *tmp = dynamic_cast<const Int *>(t->BaseType());
    if (tmp) {
      return 0;
    }
  }
  {
    const Bool *tmp = dynamic_cast<const Bool *>(t->BaseType());
    if (tmp) {
      return 1;
    }
  }
  return -1;
}

int TypeFactory::bitWidthTwo (const Type *t)
{
  if (!t) return -1;
  {
    const Chan *tmp = dynamic_cast <const Chan *>(t);
    if (tmp) {
      /* ok */
      InstType *x = tmp->acktype();
      if (!x) return 0;
      if (!x->isExpanded()) return -1;
      return TypeFactory::bitWidth (x);
    }
  }
  {
    const Channel *tmp = dynamic_cast <const Channel *> (t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      return TypeFactory::bitWidthTwo (tmp->getParent());
    }
  }
  return -1;
}
XINSTMACRO(bitWidthTwo)


int TypeFactory::totBitWidthTwo (const Type *t)
{
  if (!t) return -1;
  {
    const Chan *tmp = dynamic_cast <const Chan *>(t);
    if (tmp) {
      /* ok */
      InstType *x = tmp->acktype();
      if (!x) return 0;
      if (!x->isExpanded()) return -1;
      return TypeFactory::totBitWidthTwo (x);
    }
  }
  {
    const Channel *tmp = dynamic_cast <const Channel *> (t);
    if (tmp) {
      if (!tmp->isExpanded()) return -1;
      return TypeFactory::totBitWidthTwo (tmp->getParent());
    }
  }
  return -1;
}
XINSTMACRO(totBitWidthTwo)
  


int TypeFactory::isBaseBoolType (const Type *t)
{
  const Bool *tmp_b = dynamic_cast<const Bool *>(t);
  if (tmp_b) {
    return 1;
  }
  const Data *tmp_d = dynamic_cast<const Data *>(t);
  if (!tmp_d) {
    return 0;
  }
  if (isStructure (t)) {
    return 0;
  }
  if (isBoolType (tmp_d->root())) {
    return 1;
  }
  return 0;
}
INSTMACRO(isBaseBoolType)
  
int TypeFactory::isBaseIntType (const Type *t)
{
  const Int *tmp_i = dynamic_cast<const Int *>(t);
  if (tmp_i) {
    return 1;
  }
  const Data *tmp_d = dynamic_cast<const Data *>(t);
  if (!tmp_d) {
    return 0;
  }
  if (isStructure (t)) {
    return 0;
  }
  if (isUserPureEnum (t)) {
    return 0;
  }
  if (isIntType (tmp_d->root())) {
    return 1;
  }
  return 0;
}
INSTMACRO(isBaseIntType)
  

InstType *TypeFactory::getChanDataType (const InstType *t)
{
  if (!t) return NULL;
  {
    const Chan *tmp = dynamic_cast <const Chan *>(t->BaseType());
    if (tmp) {
      /* ok */
      return tmp->datatype();
    }
  }
  {
    const Channel *tmp = dynamic_cast <const Channel *> (t->BaseType());
    if (tmp) {
      while (!TypeFactory::isExactChanType (tmp->getParent())) {
	t = tmp->getParent ();
	tmp = dynamic_cast <const Channel *> (t->BaseType());
	Assert (tmp, "What?");
      }
      Assert (tmp->getParent(), "What?");
      const Chan *tmp2 = dynamic_cast <const Chan *> (tmp->getParent()->BaseType());
      Assert (tmp2, "What?");
      return tmp2->datatype();
    }
  }
  return NULL;
}

InstType *TypeFactory::getChanAckType (const InstType *t)
{
  if (!t) return NULL;
  {
    const Chan *tmp = dynamic_cast <const Chan *>(t->BaseType());
    if (tmp) {
      /* ok */
      return tmp->acktype();
    }
  }
  {
    const Channel *tmp = dynamic_cast <const Channel *> (t->BaseType());
    if (tmp) {
      while (!TypeFactory::isExactChanType (tmp->getParent())) {
	t = tmp->getParent ();
	tmp = dynamic_cast <const Channel *> (t->BaseType());
	Assert (tmp, "What?");
      }
      Assert (tmp->getParent(), "What?");
      const Chan *tmp2 = dynamic_cast <const Chan *> (tmp->getParent()->BaseType());
      Assert (tmp2, "What?");
      return tmp2->acktype();
    }
  }
  return NULL;
}

int expr_getHash (int prev, unsigned long sz, Expr *e)
{
  int hval = prev;
  if (!e) return prev;
  
  hval = hash_function_continue (sz, (const unsigned char *) &e->type,
				 sizeof (int), prev, 1);
  
  switch (e->type) {
  case E_INT:
    hval = hash_function_continue (sz, (const unsigned char *) &e->u.ival.v,
				   sizeof (long), hval, 1);
    if (e->u.ival.v_extra) {
      hval = hash_function_continue (sz,
				     (const unsigned char *) &e->u.ival.v_extra,
				     sizeof (long *), hval, 1);
    }
    break;
    
  case E_TRUE:
  case E_FALSE:
    break;
    
  case E_REAL:
    hval = hash_function_continue (sz, (const unsigned char *) &e->u.f,
				   sizeof (double), hval, 1);
    break;

  case E_FUNCTION:
  case E_USERMACRO:
    hval = hash_function_continue (sz, (const unsigned char *) &e->u.fn.s,
				   sizeof (char *), hval, 1);
    hval = hash_function_continue (sz, (const unsigned char *) &e->u.fn.r,
				   sizeof (Expr *), hval, 1);
    break;

  case E_VAR:
  case E_PROBE:
    if (e->u.e.l) {
      hval = hash_function_continue (sz, (const unsigned char *) &e->u.e.l,
				     sizeof (ActId *), hval, 1);
    }
    break;

  case E_BITFIELD:
    if (e->u.e.l) {
      hval = hash_function_continue (sz, (const unsigned char *) &e->u.e.l,
				     sizeof (ActId *), hval, 1);
    }
    hval = hash_function_continue (sz, (const unsigned char *) &e->u.e.r->u.e.l,
				   sizeof (Expr *), hval, 1);
    hval = hash_function_continue (sz, (const unsigned char *) &e->u.e.r->u.e.r,
				   sizeof (Expr *), hval, 1);
    break;

  default:
    if (e->u.e.l) {
      hval = hash_function_continue (sz, (const unsigned char *) &e->u.e.l,
				     sizeof (Expr *), hval, 1);
    }
    if (e->u.e.r) {
      hval = hash_function_continue (sz, (const unsigned char *) &e->u.e.r,
				     sizeof (Expr *), hval, 1);
    }
    break;
  }
  return hval;
}

/** expression caching **/
static int expr_hashfn (int sz, void *key)
{
  Expr *e = (Expr *)key;
  int hval = 0;
  if (!e) return 0;

  return expr_getHash (0, sz, e);
}

static int expr_matchfn (void *key1, void *key2)
{
  Expr *e1 = (Expr *)key1;
  Expr *e2 = (Expr *)key2;
  if (e1 == e2) return 1;
  if (!e1 || !e2) return 0;
  if (e1->type != e2->type) return 0;
  
  switch (e1->type) {
  case E_INT:
    if (e1->u.ival.v != e2->u.ival.v) return 0;
    if (e1->u.ival.v_extra == e2->u.ival.v_extra) return 1;
    if (!e1->u.ival.v_extra || !e2->u.ival.v_extra) return 0;
    if (*((BigInt *)e1->u.ival.v_extra) !=
	*((BigInt *)e2->u.ival.v_extra))
      return 0;
    break;
    
  case E_TRUE:
  case E_FALSE:
    break;
    
  case E_REAL:
    if (e1->u.f != e2->u.f) return 0;
    break;

  case E_FUNCTION:
  case E_USERMACRO:
    if (e1->u.fn.s != e2->u.fn.s) return 0;
    if (e1->u.fn.r != e2->u.fn.r) return 0;
    break;

  case E_VAR:
  case E_PROBE:
    if (e1->u.e.l == e2->u.e.l) return 1;
    if (!e1->u.e.l || !e2->u.e.l) return 0;
    if (e1->u.e.l != e2->u.e.l) {
      ActId *id1 = (ActId *)e1->u.e.l;
      ActId *id2 = (ActId *)e2->u.e.l;
      if (!id1->isEqual (id2)) {
	return 0;
      }
    }
    break;

  case E_BITFIELD:
    if (e1->u.e.l && !e2->u.e.l) return 0;
    if (!e1->u.e.l && e2->u.e.l) return 0;
    if (e1->u.e.l && (e1->u.e.l != e2->u.e.l)) {
      ActId *id1 = (ActId *)e1->u.e.l;
      ActId *id2 = (ActId *)e2->u.e.l;
      if (!id1->isEqual (id2)) return 0;
    }
    if (e1->u.e.r->u.e.l != e2->u.e.r->u.e.l) return 0;
    if (e1->u.e.r->u.e.r != e2->u.e.r->u.e.r) return 0;
    break;

  default:
    if (e1->u.e.l != e2->u.e.l) return 0;
    if (e1->u.e.r != e2->u.e.r) return 0;
    break;
  }
  return 1;
}

static void *expr_dupfn (void *key)
{
  Expr *e = (Expr *)key;
  Expr *dup;
  if (!e) return NULL;
  
  NEW (dup, Expr);
  *dup = *e;
  if (dup->type == E_INT && dup->u.ival.v_extra) {
    BigInt *bi = new BigInt;
    *bi = *((BigInt *)dup->u.ival.v_extra);
    dup->u.ival.v_extra = bi;
  }
  else if (dup->type == E_BITFIELD) {
    NEW (dup->u.e.r, Expr);
    *dup->u.e.r = *e->u.e.r;
  }
  return dup;
}

static void expr_freefn (void *key)
{
  Expr *e = (Expr *)key;
  if (!e) return;

  switch (e->type) {
  case E_INT:
    if (e->u.ival.v_extra) {
      delete ((BigInt *)e->u.ival.v_extra);
    }
  case E_TRUE:
  case E_FALSE:
  case E_REAL:
    break;

  case E_FUNCTION:
  case E_USERMACRO:
    // r is canonical
    break;

  case E_VAR:
  case E_PROBE:
    break;

  case E_BITFIELD:
    FREE (e->u.e.r); // l, r fields are constants
    break;

  default:
    // l and r will be canonical
    break;
  }
  FREE (e);
  return;
}
  
static void expr_printfn (FILE *fp, void *key)
{

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
  else if (x->type == E_INT && x->u.ival.v_extra == NULL) {
    ihash_bucket_t *b;

    b = ihash_lookup (TypeFactory::expr_int, x->u.ival.v);
    if (b) {
      return (Expr *)b->v;
    }
    else {
      Expr *t;
      b = ihash_add (TypeFactory::expr_int, x->u.ival.v);
      NEW (t, Expr);
      t->type = E_INT;
      t->u.ival.v = x->u.ival.v;
      t->u.ival.v_extra = NULL;
      b->v = t;
      return t;
    }
  }
  else if (x->type == E_INT) {
    Expr *t;
    BigInt *tmp = new BigInt();
    NEW (t, Expr);
    t->type = E_INT;
    t->u.ival.v = x->u.ival.v;
    *tmp = *((BigInt *)x->u.ival.v_extra);
    t->u.ival.v_extra = tmp;
    return t;
  }
  else {
    fatal_error ("TypeFactory::NewExpr() called without a constant int/bool expression!");
  }
  return NULL;
}

static int idhash (int sz, void *key)
{
  return 0;
}

static int idmatch (void *k1, void *k2)
{
  return 0;
}

static void *iddup (void *k)
{
  return k;
}

static void idfree (void *k)
{
}

ActId *TypeFactory::NewId (ActId *id)
{
  Assert (0, "To be implemented");
  return id;
}
