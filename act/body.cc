/*************************************************************************
 *
 *  Copyright (c) 2011-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <act/namespaces.h>
#include <act/types.h>
#include <act/inst.h>
#include <act/lang.h>
#include <act/act.h>
#include <act/body.h>
#include <string.h>
#include "misc.h"
#include "config.h"

/* XXX: free actbody */

void ActBody::Append (ActBody *b)
{
  ActBody *t, *u;
  if (!next) {
    next = b;
  }
  else {
    t = next;
    u = next->next;
    while (u) {
      t = u;
      u = u->next;
    }
    t->next = b;
  }
}

ActBody *ActBody::Tail ()
{
  ActBody *b = this;

  while (b->next) {
    b = b->next;
  }
  return b;
}

ActBody_Inst::ActBody_Inst (InstType *it, const char *_id)
{
  id = _id;
  t = it;
}

Type *ActBody_Inst::BaseType ()
{
  Assert (t, "NULL deref on ActBody_Inst's insttype");
  return t->BaseType();
}

void ActBody_Inst::Print (FILE *fp)
{
  char buf[10240];
  Array *a = NULL;
  if (t->isExpanded()) {
    t->sPrint (buf, 10240);
    ActNamespace::Act()->mfprintf (fp, "%s", buf);
  }
  else {
    a = t->arrayInfo();
    t->clrArray ();
    t->Print (fp);
  }
  fprintf (fp, " %s", id);
  if (a) {
    a->Print (fp);
    t->MkArray (a);
  }
  fprintf (fp, ";\n");
}

/*------------------------------------------------------------------------*/

/*
 * ns = the current namespace. Namespaces get expanded in place, since
 * there's no notion of a parameterized namespace.
 * s = the *fresh*, new scope for expansion
 */
void ActBody_Inst::Expand (ActNamespace *ns, Scope *s)
{
  InstType *x, *it;

  /* typechecking should all pass, so there shouldn't be an issue
     beyond creating the actual object. For arrays, there may be
     duplicate dereference issues
  */

  /* 
     expand instance type!
  */
  it = t->Expand (ns, s);
  x = s->Lookup (id);

  if (x) {
    ValueIdx *vx;
    Array *old;

    vx = s->LookupVal (id);
    Assert (vx && vx->t == x, "What?");
    old = x->arrayInfo()->Clone();

#if 0
    fprintf (stderr, "Original: ");
    vx->t->Print (stderr);
    fprintf (stderr, "\n");
#endif

    /* sparse array */
    Array *ta, *tb;

    ta = it->arrayInfo();
    it->clrArray ();

    tb = vx->t->arrayInfo();
    vx->t->clrArray();

    if (!it->isEqual (vx->t)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Sparse array type error on %s\n", id);
      fprintf (stderr, "\tOrig type: ");
      vx->t->Print (stderr);
      fprintf (stderr, "\n\tNew type: ");
      it->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
    it->MkArray (ta);
    vx->t->MkArray (tb);

    x->arrayInfo()->Merge (it->arrayInfo());

#if 0
    fprintf (stderr, "New: ");
    vx->t->Print (stderr);
    fprintf (stderr, "\n");
#endif
    
    if (vx->init) {
      /* it's been allocated; needs reallocation! */
      if (TypeFactory::isParamType (vx->t)) {
	bitset_t *b = bitset_new (old->size());
	/* realloc and copy */

	if (TypeFactory::isPIntType (vx->t->BaseType())) {
	  int *vals;
	  Arraystep *as;
	  
	  MALLOC (vals, int, old->size());
	  for (int i=0; i < old->size(); i++) {
	    if (s->issetPInt (vx->u.idx + i)) {
	      bitset_set (b, i);
	      vals[i] = s->getPInt (vx->u.idx + i);
	    }
	  }
	  s->DeallocPInt (vx->u.idx, old->size());
	  vx->u.idx = s->AllocPInt (x->arrayInfo()->size());

	  as = old->stepper();
	  while (!as->isend()) {
	    if (bitset_tst (b, as->index())) {
	      s->setPInt (vx->u.idx + as->index (x->arrayInfo()),
			  vals[as->index()]);
	    }
	    as->step();
	  }
	  delete as;
	  FREE (vals);
	}
	else if (TypeFactory::isPIntsType (vx->t->BaseType())) {
	  int *vals;
	  Arraystep *as;
	  
	  MALLOC (vals, int, old->size());
	  for (int i=0; i < old->size(); i++) {
	    if (s->issetPInts (vx->u.idx + i)) {
	      bitset_set (b, i);
	      vals[i] = s->getPInts (vx->u.idx + i);
	    }
	  }
	  s->DeallocPInts (vx->u.idx, old->size());
	  vx->u.idx = s->AllocPInts (x->arrayInfo()->size());

	  as = old->stepper();
	  while (!as->isend()) {
	    if (bitset_tst (b, as->index())) {
	      s->setPInts (vx->u.idx + as->index (x->arrayInfo()),
			  vals[as->index()]);
	    }
	    as->step();
	  }
	  delete as;
	  FREE (vals);
	}
	else if (TypeFactory::isPRealType (vx->t->BaseType())) {
	  double *vals;
	  Arraystep *as;
	  
	  MALLOC (vals, double, old->size());
	  for (int i=0; i < old->size(); i++) {
	    if (s->issetPReal (vx->u.idx + i)) {
	      bitset_set (b, i);
	      vals[i] = s->getPReal (vx->u.idx + i);
	    }
	  }
	  s->DeallocPReal (vx->u.idx, old->size());
	  vx->u.idx = s->AllocPReal (x->arrayInfo()->size());

	  as = old->stepper();
	  while (!as->isend()) {
	    if (bitset_tst (b, as->index())) {
	      s->setPReal (vx->u.idx + as->index (x->arrayInfo()),
			   vals[as->index()]);
	    }
	    as->step();
	  }
	  delete as;
	  FREE (vals);
	}
	else if (TypeFactory::isPBoolType (vx->t->BaseType())) {
	  int *vals;
	  Arraystep *as;
	  
	  MALLOC (vals, int, old->size());
	  for (int i=0; i < old->size(); i++) {
	    if (s->issetPBool (vx->u.idx + i)) {
	      bitset_set (b, i);
	      vals[i] = s->getPBool (vx->u.idx + i);
	    }
	  }
	  s->DeallocPBool (vx->u.idx, old->size());
	  vx->u.idx = s->AllocPBool (x->arrayInfo()->size());

	  as = old->stepper();
	  while (!as->isend()) {
	    if (bitset_tst (b, as->index())) {
	      s->setPBool (vx->u.idx + as->index (x->arrayInfo()),
			   vals[as->index()]);
	    }
	    as->step();
	  }
	  delete as;
	  FREE (vals);
	}
	else {
	  Assert (0, "no ptype arrays");
	}
        bitset_free (b);
      }
      else {
	/* XXX: HERE: WORK ON THIS ONCE CONNECTIONS ARE DONE */
	warning ("Sparse array: Fix this please");
      }
    }
    else {
      /* nothing needed here, since this was not allocated at all */
    }

    delete old;
#if 0
    act_error_ctxt (stderr);
    warning ("Sparse array--FIXME, skipping right now!\n");
#endif    
  }
  else {
    Assert (s->Add (id, it), "Should succeed; what happened?!");
  }
}


void ActBody_Assertion::Expand (ActNamespace *ns, Scope *s)
{
  Expr *ex;

  ex = expr_expand (e, ns, s);
  if (ex->type != E_TRUE && ex->type != E_FALSE) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Expression: ");
    print_expr (stderr, e);
    fprintf (stderr, "\n");
    fatal_error ("Not a Boolean constant!");
  }
  if (ex->type == E_FALSE) {
    act_error_ctxt (stderr);
    fprintf (stderr, "*** Assertion failed ***\n");
    fprintf (stderr, " assertion: ");
    print_expr (stderr, e);
    fprintf (stderr, "\n");
    if (msg) {
      char *s = Strdup (msg+1);
      s[strlen(s)-1] = '\0';
      fprintf (stderr, "   message: %s\n", s);
    }
    fatal_error ("Aborted execution on failed assertion");
  }
}

static int offset (act_connection **a, act_connection *c)
{
  int i;
  i = 0;
  while (1) {
    if (a[i] == c) return i;
    i++;
    if (i > 10000) return -1;
  }
  return -1;
}

bool act_connection::isglobal()
{
  act_connection *c;
  ValueIdx *vx;

  c = this;
  vx = NULL;
  
  while (c) {
    if (c->vx) {
      vx = c->vx;
      c = c->parent;
    }
    else if (c->parent->vx) {
      vx = c->parent->vx;
      c = c->parent->parent;
    }
    else {
      Assert (c->parent->parent->vx, "What?");
      vx = c->parent->parent->vx;
      c = c->parent->parent->parent;
    }
  }
  Assert (vx, "What?");
  return vx->global ? 1 : 0;
}


ActId *act_connection::toid()
{
  list_t *stk = list_new ();
  ValueIdx *vx;
  act_connection *c = this;

  while (c) {
    stack_push (stk, c);
    if (c->vx) {
      c = c->parent;
    }
    else if (c->parent->vx) {
      c = c->parent->parent;
    }
    else {
      Assert (c->parent->parent->vx, "What?");
      c = c->parent->parent->parent;
    }
    
  }

  ActId *ret = NULL;
  ActId *cur = NULL;

  while (!stack_isempty (stk)) {
    ActId *t;
    
    c = (act_connection *) stack_pop (stk);
    
    if (c->vx) {
      vx = c->vx;
      t = new ActId (vx->u.obj.name);
    }
    else if (c->parent->vx) {
      vx = c->parent->vx;
      if (vx->t->arrayInfo()) {
	Array *tmp;
	tmp = vx->t->arrayInfo()->unOffset (offset (c->parent->a, c));
	t = new ActId (vx->u.obj.name, tmp);
      }
      else {
	UserDef *ux;
	ux = dynamic_cast<UserDef *> (vx->t->BaseType());

	t = new ActId (vx->u.obj.name);
	t->Append (new ActId (ux->getPortName (offset (c->parent->a, c))));
      }
    }
    else {
      vx = c->parent->parent->vx;
      Assert (vx, "What?");
      
      Array *tmp;
      tmp = vx->t->arrayInfo()->unOffset (offset (c->parent->parent->a, c->parent));
      UserDef *ux;
      ux = dynamic_cast<UserDef *> (vx->t->BaseType());
      Assert (ux, "what?");

      t = new ActId (vx->u.obj.name, tmp);
      t->Append (new ActId (ux->getPortName (offset (c->parent->a, c))));
    }

    if (!ret) {
      ret = t;
      cur = ret;
    }
    else {
      cur->Append (t);
      cur = t;
    }
    while (cur->Rest()) {
      cur = cur->Rest();
    }
  }
  list_free (stk);
  return ret;
}

int act_connection::numSubconnections()
{
  ValueIdx *_vx;
  int type;

  if (!a) { return 0; }

  _vx = getvx();
  type = getctype();

  Assert (type == 0 || type == 1, "Hmm");

  if (type == 0 && _vx->t->arrayInfo()) {
    /* it is an array! */
    return _vx->t->arrayInfo()->size();
  }
  else {
    UserDef *ux = dynamic_cast<UserDef *>(_vx->t->BaseType());
#if 0
    if  (!ux) {
      printf ("Hmm... id is: ");
      toid()->Print (stdout);
      printf ("\n");
      printf ("  +++ VX t is: ");
      _vx->t->Print (stdout);
      printf ("\n");
      return 0;
    }
#endif      
    Assert (ux, "hmm...");
    return ux->getNumPorts ();
  }

    
}

ValueIdx *act_connection::getvx()
{
  if (vx) {
    return vx;
  }
  else if (parent->vx) {
    return parent->vx;
  }
  else if (parent->parent->vx) {
    return parent->parent->vx;
  }
  Assert (0, "What");
  return vx;
}

unsigned int act_connection::getctype()
{
  if (vx) {
    return 0;
  }
  else if (parent->vx) {
    return 1;
  }
  else if (parent->parent->vx) {
    return 2;
  }
  return 3;
}

static void print_id (act_connection *c)
{
  if (!c) return;
  
  ActId *id = c->toid();

  id->Print (stdout);
  delete id;
}

#if 0
static void print_id (act_connection *c)
{
  list_t *stk = list_new ();
  ValueIdx *vx;

  while (c) {
    stack_push (stk, c);
    if (c->vx) {
      c = c->parent;
    }
    else if (c->parent->vx) {
      c = c->parent->parent;
    }
    else {
      Assert (c->parent->parent->vx, "What?");
      c = c->parent->parent->parent;
    }
  }
  
  while (!stack_isempty (stk)) {
    c = (act_connection *) stack_pop (stk);
    if (c->vx) {
      vx = c->vx;
      printf ("%s", vx->u.obj.name);
    }
    else if (c->parent->vx) {
      vx = c->parent->vx;
      if (vx->t->arrayInfo()) {
	Array *tmp;
	tmp = vx->t->arrayInfo()->unOffset (offset (c->parent->a, c));
	printf ("%s", vx->u.obj.name);
	tmp->Print (stdout);
	delete tmp;
      }
      else {
	UserDef *ux;
	ux = dynamic_cast<UserDef *> (vx->t->BaseType());
	Assert (ux, "what?");
	printf ("%s.%s", vx->u.obj.name, ux->getPortName (offset (c->parent->a, c)));
      }
    }
    else {
      vx = c->parent->parent->vx;
      Assert (vx, "What?");
      
      Array *tmp;
      tmp = vx->t->arrayInfo()->unOffset (offset (c->parent->parent->a, c->parent));
      UserDef *ux;
      ux = dynamic_cast<UserDef *> (vx->t->BaseType());
      Assert (ux, "what?");

      printf ("%s", vx->u.obj.name);
      tmp->Print (stdout);
      printf (".%s", ux->getPortName (offset (c->parent->a, c)));

      delete tmp;
    }
    if (vx->global) {
      printf ("(g)");
    }
    if (!stack_isempty (stk)) {
      printf (".");
    }
  }
  list_free (stk);
}
#endif

#if 0
static void print_id (act_connection *c)
{
  printf ("<rev: ");

  while (c) {
    if (c->vx) {
      printf (".");
      if (c->vx->global) {
	char *s = c->vx->t->getNamespace()->Name();
	printf ("%s%s(g)", s, c->vx->u.obj.name);
	FREE (s);
      }
      else {
	UserDef *ux;
	printf ("{t:");
	ux = c->vx->t->getUserDef();
	if (ux) {
	  printf ("#%s#", ux->getName());
	}
	c->vx->t->Print (stdout);
	printf ("}%s", c->vx->u.obj.name);
      }
    }
    else {
      InstType *it;
      if (c->parent->vx) {
	/* one level: either x[] or x.y */
	it = c->parent->vx->t;
	if (it->arrayInfo()) {
	  printf ("[i:%d]", offset (c->parent->a, c));
	}
	else {
	  UserDef *ux = dynamic_cast<UserDef *>(it->BaseType());
	  Assert (ux, "What?");
	  printf (".%s", ux->getPortName (offset (c->parent->a, c)));
	}
      }
      else if (c->parent->parent->vx) {
	UserDef *ux;
	it = c->parent->parent->vx->t;
	/* x[].y */
	Assert (it->arrayInfo(), "What?");
	ux = dynamic_cast<UserDef *>(it->BaseType());
	Assert (ux, "What?");
	printf ("[i:%d]", offset (c->parent->parent->a, c->parent));
	printf (".%s", ux->getPortName(offset (c->parent->a, c)));
      }
      else {
	Assert (0, "What?");
      }
    }
    c = c->parent;
  }
  printf (">");
}
#endif

static void dump_conn (act_connection *c)
{
  act_connection *tmp, *root;

  root = c;
  while (root->up) root = root->up;

  tmp = c;

  printf ("conn: ");
  do {
    print_id (tmp);
    if (tmp == root) {
      printf ("*");
    }
    printf (" , ");
    tmp = tmp->next;
  } while (tmp != c);
  printf("\n");
}

static void mk_raw_connection (act_connection *c1, act_connection *c2)
{
  /* c1 is the root, not c2 */
  while (c2->up) {
    c2 = c2->up;
  }
  c2->up = c1;

  act_connection *t1, *t2;

  /* merge c1, c2 connection ring */
  t1 = c1->next;
  t2 = c2->next;
  c1->next = t2;
  c2->next = t1;
}

static void mk_raw_skip_connection (act_connection *c1, act_connection *c2)
{
  act_connection *tmp = c2;
  /* c1 is the root, not c2 */

  if (c2->next == c2) {
    /* nothing to do */
  }
  else {
    act_connection *t1, *t2;
    int in_ring = 0;

    t1 = c1->primary();
    t2 = c2->primary();

    if (t1 == t2) {
      in_ring = 1;
    }
    else {
      in_ring = 0;
    }

    if (!in_ring) {
      while (c2->up) {
	c2 = c2->up;
      }
      c2->up = c1;
    }

    t1 = tmp->next;
    while (t1 != tmp) {
      if (t1->up == c2) {
	t1->up = c2->up;
      }
      t1 = t1->next;
    }

    c2 = tmp;

    if (!in_ring) {
      /* merge c1, c2 connection ring, and drop c2 itself */
      t1 = c1->next;
      t2 = c2->next->next;
      c1->next = t2;
      c2->next->next = t1;
    }
    else {
      /* the rings are already merged. we need to delete c2 from it */
      t1 = c2;
      while (t1->next != c2) {
	t1 = t1->next;
      }
      t1->next = t1->next->next;
    }
  }
  /* now we need to merge any further subconnections between the array
     elements of c1->a and c2->a, if any */

  if (!c1->a && !c2->a) {
    FREE (c2);
    return;
  }
  
  ValueIdx *vx;

  if (c1->vx) {
    /* direct */
    FREE (c2);
    return;
  }
  else if (c1->parent->vx) {
    int sz;
    /* just an array or just a subport */
    vx = c1->parent->vx;

    if (vx->t->arrayInfo()) {
      sz = vx->t->arrayInfo()->size();
    }
    else {
      UserDef *ux;
      ux = dynamic_cast <UserDef *> (vx->t->BaseType());
      sz = ux->getNumPorts();
    }
    if (!c1->a) {
      c1->a = c2->a;
      for (int i=0; i < sz; i++) {
	c1->a[i]->parent = c1;
      }
    }
    else if (c2->a) {
      for (int i=0; i < sz; i++) {
	if (!c1->a[i]) {
	  c1->a[i] = c2->a[i];
	  if (c1->a[i]) {
	    c1->a[i]->parent = c1->a[i];
	  }
	}
	else if (c2->a[i]) {
	  mk_raw_skip_connection (c1->a[i], c2->a[i]);
	}
      }
      FREE (c2->a);
    }
  }
  else {
    int sz1, sz2;
    /* array and sub-port */
    vx = c1->parent->parent->vx;
    Assert (vx, "What?");
    
    sz1 = vx->t->arrayInfo()->size();

    UserDef *ux;
    ux = dynamic_cast <UserDef *> (vx->t->BaseType());

    sz2 = ux->getNumPorts();

    if (!c1->a) {
      c1->a = c2->a;
      if (c1->a) {
	for (int i=0; i < sz1; i++) {
	  c1->a[i]->parent = c1;
	  if (c1->a[i]->a) {
	    for (int j=0; j < sz2; j++) {
	      if (c1->a[i]->a[j]) {
		c1->a[i]->a[j]->parent = c1->a[i];
	      }
	    }
	  }
	}
      }
    }
    else if (c2->a) {
      for (int i=0; i < sz1; i++) {
	if (c1->a[i]) {
	  if (c2->a[i]) {
	    for (int j=0; j < sz2; j++) {
	      if (c1->a[i]->a[j]) {
		if (c2->a[i]->a[j]) {
		  mk_raw_skip_connection (c1->a[i]->a[j], c2->a[i]->a[j]);
		}
	      }
	      else if (c2->a[i]->a[j]) {
		c1->a[i]->a[j] = c2->a[i]->a[j];
		c1->a[i]->a[j]->parent = c1->a[i];
	      }
	    }
	  }
	}
	else {
	  c1->a[i] = c2->a[i];
	  if (c2->a[i]) {
	    for (int j=0; j < sz2; j++) {
	      c1->a[i]->a[j]->parent = c1->a[i];
	    }
	  }
	}
      }
      FREE (c2->a);
    }
  }
}

static void _merge_subtrees (UserDef *ux, act_connection *c1, act_connection *c2)
{
  ValueIdx *vx;

  vx = c1->getvx();

  if (!c1->a) {
    if (c2->a) {
      if (vx->t->arrayInfo()) {
	int sz;
	
	sz = vx->t->arrayInfo()->size();
	c1->a = c2->a;
	c2->a = NULL;
	for (int i=0; i < sz; i++) {
	  if (c1->a[i]) {
	    c1->a[i]->parent = c1;
	  }
	}
      }
      else {
	UserDef *ux;
	int sz;
	
	ux = dynamic_cast <UserDef *> (vx->t->BaseType());

	c1->a = c2->a;
	c2->a = NULL;
	sz = ux->getNumPorts();
	for (int i=0; i < sz; i++) {
	  if (c1->a[i]) {
	    c1->a[i]->parent = c1;
	  }
	}
      }
    }
  }
  else if (c2->a) {
    int i, sz;
    if (vx->t->arrayInfo()) {
      sz = vx->t->arrayInfo()->size();
      for (i=0; i < sz; i++) {
	/* connect c1->a[i] with c2->a[i] */
	if (c1->a[i] && c2->a[i]) {
	  /* you might have the same thing repeated... */
	  mk_raw_skip_connection (c1->a[i], c2->a[i]);
	}
	else if (c2->a[i]) {
	  c1->a[i] = c2->a[i];
	  c2->a[i] = NULL;
	}
	c1->a[i]->parent = c1;
      }
      FREE (c2->a);
    }
    else {
      UserDef *ux;
      int sz;

      ux = dynamic_cast <UserDef *> (vx->t->BaseType());
      
      sz = ux->getNumPorts();
      for (i=0; i < sz; i++) {
	if (c1->a[i] && c2->a[i]) {
	  mk_raw_skip_connection (c1->a[i], c2->a[i]);
	}
	else if (c2->a[i]) {
	  c1->a[i] = c2->a[i];
	  c2->a[i] = NULL;
	}
	if (c1->a[i]) {
	  c1->a[i]->parent = c1;
	}
      }
      FREE (c2->a);
    }
  }
}


static void _merge_attributes (act_attr_t **x, act_attr *a);


static void mk_connection (UserDef *ux, const char *s1, act_connection *c1,
			   const char *s2, act_connection *c2)
{
  int p1, p2;
  act_connection *tmp;
  ValueIdx *vx1, *vx2, *vxtmp;

#if 0
  printf ("connect: %s and %s\n", s1, s2);
  
  dump_conn (c1);
  dump_conn (c2);
#endif
  if (c1 == c2) return;
  if (c1->primary() == c2->primary()) return;

  /* for global flag, find the root value */
  tmp = c1;
  while (tmp->parent) {
    tmp = tmp->parent;
  }
  Assert (tmp->vx, "What?");
  vx1 = tmp->vx;

  tmp = c2;
  while (tmp->parent) {
    tmp = tmp->parent;
  }
  Assert (tmp->vx, "What?!");
  vx2 = tmp->vx;

  if (vx1->global || vx2->global) {
    if (vx2->global && !vx1->global) {
      tmp = c1;
      c1 = c2;
      c2 = tmp;
      p1 = -1;
      p2 = -1;

      vxtmp = vx1;
      vx1 = vx2;
      vx2 = vxtmp;
    }
    else if (vx1->global && !vx2->global) {
      p1 = -1;
      p2 = -1;
    }
    else {
      p1 = 0;
      p2 = 0;
    }
  }
  else {
    if (ux) {
      /* user defined type */
      p1 = ux->FindPort (s1);
      p2 = ux->FindPort (s2);
      if (p1 > 0 || p2 > 0) {
	if (p2 > 0 && (p1 == 0 || (p2 < p1))) {
	  tmp = c1;
	  c1 = c2;
	  c2 = tmp;
	  vxtmp = vx1;
	  vx1 = vx2;
	  vx2 = vxtmp;
	}
      }
    }
  }
  if (!ux || (p1 == 0 && p2 == 0)) {
    p1 = strlen (s1);
    p2 = strlen (s2);
    if (p2 < p1) {
      tmp = c1;
      c1 = c2;
      c2 = tmp;
      vxtmp = vx1;
      vx1 = vx2;
      vx2 = vxtmp;
    }
    else if (p1 == p2) {
      p1 = strcmp (s1, s2);
      if (p1 > 0) {
	tmp = c1;
	c1 = c2;
	c2 = tmp;
	vxtmp = vx1;
	vx1 = vx2;
	vx2 = vxtmp;
      }
    }
  }

  /* NOTE: attributes can only show up on raw identifiers, or raw
     identifiers followed by a deref */

  /* vx1 gets attributes in vx2 */
  if (vx2 == c2->vx) {
    if (!c2->parent) {
      /* c2 is a raw identifier */
      if (vx1 == c1->vx) {
	if (!c1->parent) {
	  /* c1 is a raw identifier */
	  _merge_attributes (&vx1->a, vx2->a);
	  vx2->a = NULL;
	  if (vx2->array_spec) {
	    if (!vx1->array_spec) {
	      vx1->array_spec = vx2->array_spec;
	      vx2->array_spec = NULL;
	    }
	    else {
	      Assert (vx1->t->arrayInfo(), "Hmm");
	      for (int i=0; i < vx1->t->arrayInfo()->size(); i++) {
		_merge_attributes (&vx1->array_spec[i], vx2->array_spec[i]);
		vx2->array_spec[i] = NULL;
	      }
	      FREE (vx2->array_spec);
	      vx2->array_spec = NULL;
	    }
	  }
	}
	else {
	  /* connection to sub-object, drop attributes */
	}
      }
      else {
	if (c1->parent->vx && (c1->parent->vx == vx1) && vx1->t->arrayInfo()) {
	  int i = offset (c1->parent->a, c1);

	  if (!vx1->array_spec) {
	    int sz;

	    sz = vx1->t->arrayInfo()->size();
	    MALLOC (vx1->array_spec, act_attr_t *, sz);
	    for (; sz > 0; sz--) {
	      vx1->array_spec[sz-1] = NULL;
	    }
	  }
	  if (vx2->a) {
	    _merge_attributes (&vx1->array_spec[i], vx2->a);
	    vx2->a = NULL;
	  }
	}
	else {
	  /* do nothing */
	}
      }
    }
    else {
      /* c2 is a sub-object, so it should not have any attributes;
	 do nothing.
       */
    }
  }
  else if (c2->parent->vx && (c2->parent->vx == vx2) && vx2->t->arrayInfo()) {
    /* array reference, look at attributes on vx2->array_info[i] */
    int i = offset (c2->parent->a, c2);

    if (vx1 == c1->vx) {
      if (!c1->parent) {
	/* look at the attributes on vx2 */
	if (vx2->array_spec && vx2->array_spec[i]) {
	  _merge_attributes (&vx1->a, vx2->array_spec[i]);
	  vx2->array_spec[i] = NULL;
	}
      }
      else {
	if (c1->parent->vx && (c1->parent->vx == vx1) && (vx1->t->arrayInfo())) {
	  int j = offset (c1->parent->a, c1);

	  if (vx2->array_spec && vx2->array_spec[i]) {
	    if (!vx1->array_spec) {
	      int sz;

	      Assert (vx1->t->arrayInfo(), "Hmm");

	      sz = vx1->t->arrayInfo()->size();
	      MALLOC (vx1->array_spec, act_attr_t *, sz);
	      for (; sz > 0; sz--) {
		      vx1->array_spec[sz-1] = NULL;
	      }
	    }
	    _merge_attributes (&vx1->array_spec[j], vx2->array_spec[i]);
	    vx2->array_spec[i] = NULL;
	  }
	}
      }
    }
  }

  /* actually connect them! */
  mk_raw_connection (c1, c2);
  
  /* now merge any subtrees */
  _merge_subtrees (ux, c1, c2);
}

void ActBody_Conn::Expand (ActNamespace *ns, Scope *s)
{
  Expr *e;
  AExpr *alhs, *arhs;
  ActId *ex;
  InstType *tlhs, *trhs;

  switch (type) {
  case 0:
    /*--  basic --*/

#if 0
    fprintf (stderr, "Conn: ");
    u.basic.lhs->Print (stderr);
    fprintf (stderr, " = ");
    u.basic.rhs->Print (stderr);
    fprintf (stderr, "\n");
#endif
    
    /* lhs */
    ex = u.basic.lhs->Expand (ns, s);
    Assert (ex, "What?");
    e = ex->Eval (ns, s, 1 /* it is an lval */);
    Assert (e, "What?");
    Assert (e->type == E_VAR, "Hmm...");
    tlhs = (InstType *)e->u.e.r;

    /* rhs */
    arhs = u.basic.rhs->Expand (ns, s);
    trhs = arhs->getInstType (s, 1);

    if (!type_connectivity_check (tlhs, trhs)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Connection: ");
      ex->Print (stderr);
      fprintf (stderr, " = ");
      arhs->Print (stderr);
      fprintf (stderr, "\n  LHS: ");
      tlhs->Print (stderr);
      fprintf (stderr, "\n  RHS: ");
      trhs->Print (stderr);
      fprintf (stderr, "\n");
      
      fatal_error ("Type-checking failed on connection\n\t%s",
		   act_type_errmsg());
    }

    if (TypeFactory::isParamType (tlhs)) {
      /* a parameter assignment */
      if (TypeFactory::isPTypeType (tlhs->BaseType())) {
	/* ptype assignment */
	AExprstep *astep = arhs->stepper();

	s->BindParam ((ActId *)e->u.e.l, astep->getPType());
	
	astep->step();
	Assert (astep->isend(), "What?");
	delete astep;
      }
      else {
	/* any other parameter assignment */
	s->BindParam ((ActId *)e->u.e.l, arhs);
      }
    }
    else {
      ActId *id = (ActId *)e->u.e.l;
      act_connection *lcx;
      act_connection *rcx;
      AExprstep *rhsstep = arhs->stepper();
      int done_conn;

      /* if id is not an array, or it is a deref */
      ActId *tid;
      tid = id;
      while (tid->Rest()) {
	tid = tid->Rest();
      }

      done_conn = 0;
      if (!tid->arrayInfo() || tid->arrayInfo()->isDeref()) {
	/* this is a direct connection */
	/* check for special case for rhs */
	if (rhsstep->isSimpleID()) {
	  int ridx;
	  ActId *rid;
	  int rsize;

	  done_conn = 1;

	  lcx = id->Canonical (s);
	  rhsstep->getID (&rid, &ridx, &rsize);
	  rcx = rid->Canonical (s);

	  if (ridx == -1) {
	    mk_connection (s->getUserDef(),
			   id->getName(), lcx,
			   rid->getName(), rcx);
	  }
	  else {
	    //Assert (trhs->arrayInfo(), "What?");
	    //Assert (rsize == trhs->arrayInfo()->size(), "What?");
	    if (!rcx->a) {
	      MALLOC (rcx->a, act_connection *, rsize);
	      for (int i=0; i < rsize; i++) {
		rcx->a[i] = NULL;
	      }
	    }
	    Assert (0 <= ridx && ridx < rsize, "What?");
	    if (!rcx->a[ridx]) {
	      NEW (rcx->a[ridx], act_connection);
	      rcx->a[ridx]->vx = NULL;
	      rcx->a[ridx]->parent = rcx;
	      rcx->a[ridx]->up = NULL;
	      rcx->a[ridx]->next = rcx->a[ridx];
	      rcx->a[ridx]->a = NULL;
	    }
	    rcx = rcx->a[ridx]->primary();

	    mk_connection (s->getUserDef(),
			   id->getName(), lcx,
			   rid->getName(), rcx);
	  }
	}
      }
      if (!done_conn) {
	Arraystep *lhsstep = tlhs->arrayInfo()->stepper (id->arrayInfo());
	/* element by element array connection */

	while (!lhsstep->isend()) {
	  int lidx, lsize;
	  ActId *lid;
	  int ridx, rsize;
	  ActId *rid;
	  act_connection *lx, *rx;

	  lid = id;
	  lidx = lhsstep->index();
	  lsize = lhsstep->typesize();

	  rhsstep->getID (&rid, &ridx, &rsize);

	  lx = lid->Canonical (s);
	  rx = rid->Canonical (s);

	  if (lidx != -1) {
	    if (!lx->a) {
	      MALLOC (lx->a, act_connection *, lsize);
	      for (int i=0; i < lsize; i++) {
		lx->a[i] = NULL;
	      }
	    }
	    if (!lx->a[lidx]) {
	      NEW (lx->a[lidx], act_connection);
	      lx->a[lidx]->vx = NULL;
	      lx->a[lidx]->parent = lx;
	      lx->a[lidx]->up = NULL;
	      lx->a[lidx]->next = lx->a[lidx];
	      lx->a[lidx]->a = NULL;
	    }
	    lx = lx->a[lidx];
	  }
	  if (ridx != -1) {
	    if (!rx->a) {
	      MALLOC (rx->a, act_connection *, rsize);
	      for (int i=0; i < rsize; i++) {
		rx->a[i] = NULL;
	      }
	    }
	    if (!rx->a[ridx]) {
	      NEW (rx->a[ridx], act_connection);
	      rx->a[ridx]->vx = NULL;
	      rx->a[ridx]->parent = rx;
	      rx->a[ridx]->up = NULL;
	      rx->a[ridx]->next = rx->a[ridx];
	      rx->a[ridx]->a = NULL;
	    }
	    rx = rx->a[ridx];
	  }

	  mk_connection (s->getUserDef(),
			 lid->getName(), lx,
			 rid->getName(), rx);
	  
	  lhsstep->step();
	  rhsstep->step();
	}
	Assert (rhsstep->isend(), "What?");
	delete lhsstep;
      }
      delete rhsstep;
    }

    if (e) { FREE (e); }
    delete tlhs;
    delete trhs;
    delete arhs;

    break;
  case 1:
    /* aexpr */

#if 0
    fprintf (stderr, "Conn2: ");
    u.general.lhs->Print (stderr);
    fprintf (stderr, " = ");
    u.general.rhs->Print (stderr);
    fprintf (stderr, "\n");
#endif

    /* lhs */
    alhs = u.general.lhs->Expand (ns, s, 1); /* an lval */
    tlhs = alhs->getInstType (s, 1);

    /* rhs */
    arhs = u.basic.rhs->Expand (ns, s);
    trhs = arhs->getInstType (s, 1);

    if (!type_connectivity_check (tlhs, trhs)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Connection: ");
      alhs->Print (stderr);
      fprintf (stderr, " = ");
      arhs->Print (stderr);
      fprintf (stderr, "\n  LHS: ");
      tlhs->Print (stderr);
      fprintf (stderr, "\n  RHS: ");
      trhs->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Type-checking failed on connection\n\t%s", act_type_errmsg());
    }

    if (TypeFactory::isParamType (tlhs)) {
      /* a parameter assignment */

      if (TypeFactory::isPTypeType (tlhs->BaseType())) {
	/* ptype assignment */
	AExprstep *astep = arhs->stepper();

	s->BindParam (alhs->toid(), astep->getPType());
	
	astep->step();
	Assert (astep->isend(), "What?");
	delete astep;
      }
      else {
	ActId *lhsid;
	int lhsidx;
	AExprstep *aes = alhs->stepper();
	AExprstep *bes = arhs->stepper();
	/* any other parameter assignment */
	int ii = 0;
	while (!aes->isend()) {

	  aes->getID (&lhsid, &lhsidx, NULL);
	  if (lhsidx == -1) {
	    /* it's a pure ID */
	    s->BindParam (lhsid, bes);
	  }
	  else {
	    s->BindParam (lhsid, bes, lhsidx);
	  }
	  aes->step();
	  bes->step();
	}
	Assert (bes->isend(), "Should have been caught earlier!");
	delete aes;
	delete bes;
      }
    }
    else {
      AExprstep *aes, *bes;
      
      ActId *lid, *rid;
      int lidx, ridx;
      int lsize, rsize;
      act_connection *lx, *rx;
      
      /* 
	 check for direct connection id = id.
	 otherwise, we step
      */
      aes = alhs->stepper();
      bes = arhs->stepper();

      if (aes->isSimpleID() && bes->isSimpleID()) {
	/* a simple ID is either foo without array specifier,
	   or foo[complete deref] (i.e. not a subrange)
	*/

	aes->getID (&lid, &lidx, &lsize);
	bes->getID (&rid, &ridx, &rsize);

	lx = lid->Canonical (s);
	rx = rid->Canonical (s);

	mk_connection (s->getUserDef(), lid->getName(), lx,
		       rid->getName(), rx);
      }
      else {
	while (!aes->isend()) {
	  aes->getID (&lid, &lidx, &lsize);
	  bes->getID (&rid, &ridx, &rsize);

	  lx = lid->Canonical (s);
	  rx = rid->Canonical (s);

	  if (lidx != -1) {
	    if (!lx->a) {
	      MALLOC (lx->a, act_connection *, lsize);
	      for (int i=0; i < lsize; i++) {
		lx->a[i] = NULL;
	      }
	    }
	    if (!lx->a[lidx]) {
	      NEW (lx->a[lidx], act_connection);
	      lx->a[lidx]->vx = NULL;
	      lx->a[lidx]->parent = lx;
	      lx->a[lidx]->up = NULL;
	      lx->a[lidx]->next = lx->a[lidx];
	      lx->a[lidx]->a = NULL;
	    }
	    lx = lx->a[lidx];
	  }
	  if (ridx != -1) {
	    if (!rx->a) {
	      MALLOC (rx->a, act_connection *, rsize);
	      for (int i=0; i < rsize; i++) {
		rx->a[i] = NULL;
	      }
	    }
	    if (!rx->a[ridx]) {
	      NEW (rx->a[ridx], act_connection);
	      rx->a[ridx]->vx = NULL;
	      rx->a[ridx]->parent = rx;
	      rx->a[ridx]->up = NULL;
	      rx->a[ridx]->next = rx->a[ridx];
	      rx->a[ridx]->a = NULL;
	    }
	    rx = rx->a[ridx];
	  }

	  mk_connection (s->getUserDef(),
			 lid->getName(), lx,
			 rid->getName(), rx);
	  
	  aes->step();
	  bes->step();
	}
	Assert (bes->isend(), "What?");
      }
      delete aes;
      delete bes;
    }

    delete tlhs;
    delete trhs;
    delete arhs;
    delete alhs;
    
    break;
  default:
    fatal_error ("Should not be here");
    break;
  }
}

void ActBody_Conn::Print (FILE *fp)
{
  if (type == 0) {
    u.basic.lhs->Print (fp);
    fprintf (fp, " = ");
    u.basic.rhs->Print (fp);
  }
  else {
    u.general.lhs->Print (fp);
    fprintf (fp, " = ");
    u.general.rhs->Print (fp);
  }
  fprintf (fp, ";\n");
}

void ActBody_Loop::Print (FILE *fp)
{
  fprintf (fp, "(");
  switch (t) {
  case ActBody_Loop::SEMI:
    fprintf (fp, ";");
    break;
  case ActBody_Loop::COMMA:
    fprintf (fp, ",");
    break;
  case ActBody_Loop::AND:
    fprintf (fp, "&");
    break;
  case ActBody_Loop::OR:
    fprintf (fp, "|");
    break;
  case ActBody_Loop::BAR:
    fprintf (fp, "[]");
    break;
  default:
    fatal_error ("Eh?");
    break;
  }
  fprintf (fp, "%s", id);
  fprintf (fp, ":");
  print_expr (fp, lo);
  if (hi) {
    fprintf (fp, " .. ");
    print_expr (fp, hi);
  }
  fprintf (fp, ": ");

  ActBody *bi;

  for (bi = b; bi; bi = bi->Next()) {
    bi->Print (fp);
  }
  fprintf (fp, ")\n");
}

void ActBody_Loop::Expand (ActNamespace *ns, Scope *s)
{
  int ilo, ihi;
  ValueIdx *vx;
  Expr *e;
  ActBody *bi;
  
  Assert (t == ActBody_Loop::SEMI, "What loop is this?");

  Assert (s->Add (id, TypeFactory::Factory()->NewPInt()), "Should have been caught earlier");

  if (lo) {
    e = expr_expand (lo, ns, s);
    if (!expr_is_a_const (e)) {
      act_error_ctxt (stderr);
      print_expr (stderr, lo);
      fprintf (stderr, "\n");
      fatal_error ("Isn't a constant expression");
    }
    Assert (e->type == E_INT, "Should have been caught earlier");
    ilo = e->u.v;
    //FREE (e);
  }

  e = expr_expand (hi, ns, s);
  if (!expr_is_a_const (e)) {
    act_error_ctxt (stderr);
    print_expr (stderr, hi);
    fprintf (stderr, "\n");
    fatal_error ("Isn't a constant expression");
  }
  Assert (e->type == E_INT, "Should have been caught earlier");
  ihi = e->u.v;
  //FREE (e);

  if (!lo) {
    ilo = 0;
    ihi--;
  }

  vx = s->LookupVal (id);
  vx->init = 1;
  vx->u.idx = s->AllocPInt();

  for (; ilo <= ihi; ilo++) {
    s->setPInt (vx->u.idx, ilo);
    b->Expandlist (ns, s);
  }

  s->DeallocPInt (vx->u.idx, 1);
  s->Del (id);
}

void ActBody_Select::Expand (ActNamespace *ns, Scope *s)
{
  ActBody_Select_gc *igc;
  ActBody *bi;
  Expr *guard;
  
  for (igc = gc; igc; igc = igc->next) {
    if (!igc->g) {
      /* else */
      igc->s->Expandlist (ns, s);
      return;
    }
    guard = expr_expand (igc->g, ns, s);
    if (!expr_is_a_const (guard)) {
      act_error_ctxt (stderr);
      print_expr (stderr, igc->g);
      fprintf (stderr, "\n");
      fatal_error ("Not a constant expression");
    }
    Assert (guard->type == E_TRUE || guard->type == E_FALSE,
	    "Should have been caught earlier");
    if (guard->type == E_TRUE) {
      igc->s->Expandlist (ns, s);
      return;
    }
  }
  /* all guards false, skip it */
  act_error_ctxt (stderr);
  warning ("All guards in selection are false.");
}

void ActBody_Genloop::Expand (ActNamespace *ns, Scope *s)
{
  ActBody_Select_gc *igc;
  ActBody *bi;
  Expr *guard;
  int flag;
  int loopcount = 0;

  flag = 1;
  while (flag) {
    loopcount++;
    if (loopcount > Act::max_loop_iterations) {
      act_error_ctxt (stderr);
      fatal_error ("# of loop iterations exceeded limit (%d)", Act::max_loop_iterations);
    }
    flag = 0;
    for (igc = gc; igc; igc = igc->next) {
      if (!igc->g) {
	fatal_error ("Should not be here!");
      }
      guard = expr_expand (igc->g, ns, s);
      if (!expr_is_a_const (guard)) {
	act_error_ctxt (stderr);
	print_expr (stderr, igc->g);
	fprintf (stderr, "\n");
	fatal_error ("Not a constant expression");
      }
      Assert (guard->type == E_TRUE || guard->type == E_FALSE,
	      "Should have been caught earlier");
      if (guard->type == E_TRUE) {
	igc->s->Expandlist (ns, s);
	flag = 1;
	break;
      }
    }
  }
  /* all guards false, return */
}


void ActBody_Lang::Print (FILE *fp)
{
  switch (t) {
  case ActBody_Lang::LANG_PRS:
    prs_print (fp, (act_prs *)lang);
    break;
  }
}

void ActBody_Lang::Expand (ActNamespace *ns, Scope *s)
{
  UserDef *ux;
  act_prs *p;
  act_chp *c;
  act_prs *old;
  act_spec *spec;

  switch (t) {
  case ActBody_Lang::LANG_PRS:
    p = prs_expand ((act_prs *)lang, ns, s);
    /* this had better be in a userdef */
    ux = s->getUserDef();
    if (!ux) {
      /* better be the global namespace */
      if ((old = ActNamespace::Global()->getprs())) {
	while (old->next) {
	  old = old->next;
	}
	old->next = p;
      }
      else {
	ActNamespace::Global()->setprs (p);
      }
    }
    else {
      if ((old = ux->getprs())) {
	while (old->next) {
	  old = old->next;
	}
	old->next = p;
      }
      else {
	ux->setprs (p);
      }
    }
    break;

  case ActBody_Lang::LANG_CHP:
    c = chp_expand ((act_chp *)lang, ns, s);
    ux = s->getUserDef();
    if (!ux) {
      ActNamespace::Global()->setchp (c);
    }
    else {
      ux->setchp (c);
    }
    break;
    
  case ActBody_Lang::LANG_HSE:
    c = chp_expand ((act_chp *)lang, ns, s);
    ux = s->getUserDef();
    if (!ux) {
      ActNamespace::Global()->sethse (c);
    }
    else {
      ux->sethse (c);
    }
    break;

  case ActBody_Lang::LANG_SPEC:
    spec = spec_expand ((act_spec *)lang, ns, s);
    ux = s->getUserDef ();
    if (!ux) {
      ActNamespace::Global()->setspec (spec);
    }
    else {
      ux->setspec (spec);
    }
    break;
    
  default:
    fatal_error ("Unknown language");
    break;
  }
}

void ActBody_Namespace::Expand (ActNamespace *_ns, Scope *s)
{
  /* expand the top-level of the namespace that was imported */
  //ns->Expand ();
}

static void _merge_attributes (act_attr_t **x, act_attr *a)
{
  act_attr_t *t, *prev;
  
  if (*x) {
    prev = NULL;
    while (a) {
      for (t = *x; t; t = t->next) {
	if (strcmp (t->attr, a->attr) == 0) {
	  /* merge the two */
	  char **z = config_get_table_string ("act.instance_attr");
	  int i;
	  for (i = 0; i < config_get_table_size ("act.instance_attr"); i++) {
	    if (strcmp (z[i]+4, t->attr) == 0) {
	      break;
	    }
	  }
	  Assert (i != config_get_table_size ("act.instance_attr"), "What");
	  if (z[i][2] == 's') {
	    /* strict! */
	    if (!expr_equal (t->e, a->e)) {
	      act_error_ctxt (stderr);
	      fatal_error ("Attribute `%s': inconsistent values",
			   t->attr);
	    }
	  }
	  else if (z[i][2] == '+') {
	    /* sum */
	    if (t->e->type == E_REAL) {
	      t->e->u.f += a->e->u.f;
	    }
	    else if (t->e->type == E_INT) {
	      t->e->u.v += a->e->u.v;
	    }
	    else {
	      act_error_ctxt (stderr);
	      fatal_error ("Attribute `%s': merge rule SUM only for reals and ints", t->attr);
	    }
	  }
	  else if (z[i][2] == 'M') {
	    /* max */
	    if (t->e->type == E_REAL) {
	      if (t->e->u.f < a->e->u.f) {
		t->e->u.f = a->e->u.f;
	      }
	    }
	    else if (t->e->type == E_INT) {
	      if (t->e->u.v < a->e->u.v) {
		t->e->u.v = a->e->u.v;
	      }
	    }
	    else {
	      act_error_ctxt (stderr);
	      fatal_error ("Attribute `%s': merge rule MAX only for reals and ints", t->attr);
	    }
	  }
	  else if (z[i][2] == 'm') {
	    /* min */
	    if (t->e->type == E_REAL) {
	      if (t->e->u.f > a->e->u.f) {
		t->e->u.f = a->e->u.f;
	      }
	    }
	    else if (t->e->type == E_INT) {
	      if (t->e->u.v > a->e->u.v) {
		t->e->u.v = a->e->u.v;
	      }
	    }
	    else {
	      act_error_ctxt (stderr);
	      fatal_error ("Attribute `%s': merge rule MIN only for reals and ints", t->attr);
	    }
	  }
	  else {
	    fatal_error ("Unrecognized attribute format `%s'", z[i]);
	  }
	  break;
	}
      }
      if (!t) {
	/* take this piece and merge it into vx */
	t = a->next;
	if (prev) {
	  prev->next = a->next;
	}
	a->next = *x;
	*x = a;
	a = t;
      }
      else {
	/* ok, a is merged, so delete it */
	if (prev) {
	  prev->next = a->next;
	}
	t = a;
	a = a->next;
	FREE (t->e);
	FREE (t);
      }
    }
  }
  else {
    *x = a;
  }
}


void ActBody_Attribute::Expand (ActNamespace *_ns, Scope *s)
{
  act_attr_t *_a;
  Array *_arr;
  ValueIdx *vx;
  act_connection *c;
    
  vx = s->LookupVal (inst);
  if (!vx) {
    act_error_ctxt (stderr);
    fatal_error ("Instance `%s' with attribute not from current scope!",
		 inst);
  }
  if (TypeFactory::isParamType (vx->t)) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Invalid attribute on instance `%s'\n", inst);
    fatal_error ("Attributes can only be associated with non-parameter types");
  }

  /* now we need to find the canonical ValueIdx */
  if (vx->init) {
    c = vx->u.obj.c->primary();
  }
  else {
    c = NULL;
  }

  if (c) {
    if (!c->parent) {
      /* primary */
      vx = c->vx;
      Assert (vx, "What");
    }
    else if (c->parent->vx && c->parent->vx->t->arrayInfo()) {
      /* array */
      vx = c->parent->vx;
    }
    else {
      act_error_ctxt (stderr);
      fprintf (stderr, "Invalid attribute on instance `%s'\n", inst);
      fatal_error ("Instance is connected to a port of another instance");
    }
  }

  _a = inst_attr_expand (a, _ns, s);
  if (arr) {
    _arr = arr->Expand (_ns, s);
  }
  else {
    _arr = NULL;
  }

  if (!_arr) {
    _merge_attributes (&vx->a, _a);
  }
  else {
    /* attribute on an element of an array */
    if (!vx->array_spec) {
      Assert (vx->t->arrayInfo(), "Hmm");
      int sz = vx->t->arrayInfo()->size();
      MALLOC (vx->array_spec, act_attr_t *, sz);
      for (; sz > 0; sz--) {
	vx->array_spec[sz-1] = NULL;
      }
    }
    int i = vx->t->arrayInfo()->Offset (_arr);
    _merge_attributes (&vx->array_spec[i], _a);
  }
}


void ActBody::Expandlist (ActNamespace *ns, Scope *s)
{
  ActBody *b;

  for (b = this; b; b = b->Next()) {
    b->Expand (ns, s);
  }
}


ActBody *ActBody_Conn::Clone ()
{
  ActBody_Conn *ret;

  if (type == 0) {
    ret = new ActBody_Conn (u.basic.lhs, u.basic.rhs);
  }
  else {
    ret = new ActBody_Conn (u.general.lhs, u.general.rhs);
  }
  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}

ActBody *ActBody_Inst::Clone ()
{
  ActBody_Inst *ret = new ActBody_Inst(t,id);
  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}

ActBody *ActBody_Lang::Clone ()
{
  ActBody_Lang *ret;

  ret = new ActBody_Lang (t, lang);

  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}

ActBody *ActBody_Loop::Clone ()
{
  ActBody_Loop *ret;

  ret = new ActBody_Loop (t, id, lo, hi, b->Clone());
  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}


ActBody *ActBody_Select::Clone ()
{
  ActBody_Select *ret;

  ret = new ActBody_Select (gc);  // XXX: clone gc here if you free
				  // body
  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}

ActBody *ActBody_Genloop::Clone ()
{
  ActBody_Genloop *ret;

  ret = new ActBody_Genloop (gc);  // XXX: clone gc here if you free
				  // body
  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}



ActBody *ActBody_Attribute::Clone()
{
  ActBody_Attribute *ret;
  ret = new ActBody_Attribute (inst, a, arr);
  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}

ActBody *ActBody_Namespace::Clone()
{
  ActBody_Namespace *ret;

  ret = new ActBody_Namespace (ns);

  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}

ActBody *ActBody_Assertion::Clone()
{
  ActBody_Assertion *ret;

  ret = new ActBody_Assertion (e, msg);

  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}
