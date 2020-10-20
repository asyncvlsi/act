/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011, 2018-2019 Rajit Manohar
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
#include <act/namespaces.h>
#include <act/types.h>
#include <act/inst.h>
#include <act/lang.h>
#include <act/act.h>
#include <act/body.h>
#include <act/value.h>
#include <string.h>
#include "misc.h"
#include "config.h"
#include "log.h"

ActBody::ActBody()
{
  next = NULL;
}

/* XXX: free actbody */
ActBody::~ActBody()
{
  if (next) {
    delete next;
  }
}

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
    if (x->arrayInfo()) {
      old = x->arrayInfo()->Clone();
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("Instance `%s' being created multiple times!", id);
    }

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
	/* vx->init means some connection was processed. We have a
	   problem! */
	if (vx->hasConnection()) {
	  if (vx->connection()->hasDirectconnections()) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Array being extended after it has participated in a connection.\n");
	    fprintf (stderr, "\tType: ");
	    vx->t->Print (stderr);
	    fprintf (stderr, "\n");
	    exit (1);
	  }
	  else {
	    /* XXX: this has to be fixed properly. 
	       need to re-adjust connection structure based on old vs new
	       If we are connected at the base type (i.e. x = y), then
	       this is an error
	    */
	    act_connection **ca;

	    Assert (vx->connection()->hasSubconnections(), "What?");
#if 0	    
	    fatal_error ("Sparse array connected and then extended. Needs to be fixed.");
#endif
	    MALLOC (ca, act_connection *, x->arrayInfo()->size());
	    for (int i=0; i < x->arrayInfo()->size(); i++) {
	      ca[i] = NULL;
	    }

	    Arraystep *newstep = x->arrayInfo()->stepper (old);
	    Arraystep *oldstep = old->stepper();
	    int idx = 0;

	    while (!oldstep->isend()) {
	      Assert (idx == oldstep->index(), "Hmm");
	      if (vx->connection()->a[idx]) {
		ca[newstep->index()] = vx->connection()->a[idx];
	      }
	      idx++;
	      oldstep->step();
	      newstep->step();
	    }
	    Assert (newstep->isend(), "Hmm...");
	    delete oldstep;
	    delete newstep;
	    FREE (vx->connection()->a);
	    vx->connection()->a = ca;
	  }
	}
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
    if (TypeFactory::isInterfaceType (it)) {
      /* this means that this was a ptype... we need to do more work */
      const char *pt = it->getPTypeID();
      Assert (pt, "PType ID missing!");
      ValueIdx *vx = s->LookupVal (pt);
      Assert (vx, "PType ID not found?");
      if (!vx->init || !s->issetPType (vx->u.idx)) {
	act_error_ctxt (stderr);
	fatal_error ("Ptype `%s' used to instantiate `%s' is not set", pt, id);
      }

      InstType *x = new InstType (s->getPType (vx->u.idx));
      x->setIfaceType (it);
      if (it->arrayInfo()) {
	x->MkArray (it->arrayInfo());
      }
      Assert (s->Add (id, x), "Should succeed!");
    }
    else {
      Assert (s->Add (id, it), "Should succeed; what happened?!");
    }
  }
}


void ActBody_Assertion::Expand (ActNamespace *ns, Scope *s)
{
  Expr *ex;

  if (type == 0) {
    ex = expr_expand (u.t0.e, ns, s);
    if (ex->type != E_TRUE && ex->type != E_FALSE) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Expression: ");
      print_expr (stderr, u.t0.e);
      fprintf (stderr, "\n");
      fatal_error ("Not a Boolean constant!");
    }
    if (ex->type == E_FALSE) {
      act_error_ctxt (stderr);
      fprintf (stderr, "*** Assertion failed ***\n");
      fprintf (stderr, " assertion: ");
      print_expr (stderr, u.t0.e);
      fprintf (stderr, "\n");
      if (u.t0.msg) {
	char *s = Strdup (u.t0.msg+1);
	s[strlen(s)-1] = '\0';
	fprintf (stderr, "   message: %s\n", s);
      }
      fatal_error ("Aborted execution on failed assertion");
    }
  }
  else if (type == 1) {
    InstType *xo, *xn, *tmp;
    xo = u.t1.old->Expand (ns, s);
    xn = u.t1.nu->Expand (ns, s);
    tmp = xn;
    while (tmp) {
      if (xo->BaseType() == tmp->BaseType()) return;
      UserDef *ux = dynamic_cast <UserDef *> (tmp->BaseType());
      if (!ux) {
	tmp = NULL;
      }
      else {
	tmp = ux->getParent();
      }
    }
    act_error_ctxt (stderr);
    fprintf (stderr, "Illegal override during expansion; new type doesn't implement the original.\n");
    fprintf (stderr, "\tOriginal: ");
    xo->Print (stderr);
    fprintf (stderr, "\n\tNew: ");
    xn->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }
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
    trhs = arhs->getInstType (s, NULL, 1);


#if 0
    fprintf (stderr, "Conn-ex: ");
    ex->Print (stderr);
    fprintf (stderr, " = ");
    arhs->Print (stderr);
    fprintf (stderr, "\n");
#endif

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
	if (rhsstep->isSimpleID() && arhs->isBase()) {
	  int ridx;
	  ActId *rid;
	  int rsize;
	  
	  done_conn = 1;

	  lcx = id->Canonical (s);
	  rhsstep->getsimpleID (&rid, &ridx, &rsize);
	  rcx = rid->Canonical (s);

	  if (ridx == -1) {
	    act_mk_connection (s->getUserDef(),
			       id->getName(), lcx,
			       rid->getName(), rcx);
	  }
	  else {
	    //Assert (trhs->arrayInfo(), "What?");
	    //Assert (rsize == trhs->arrayInfo()->size(), "What?");
	    rcx = rcx->getsubconn (ridx, rsize)->primary();

	    act_mk_connection (s->getUserDef(),
			       id->getName(), lcx,
			       rid->getName(), rcx);
	  }
	}
      }
      if (!done_conn) {
	Arraystep *lhsstep = tlhs->arrayInfo()->stepper (tid->arrayInfo());
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
	    lx = lx->getsubconn (lidx, lsize);
	  }
	  if (ridx != -1) {
	    rx = rx->getsubconn (ridx, rsize);
	  }

	  act_mk_connection (s->getUserDef(),
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
    tlhs = alhs->getInstType (s, NULL, 1);
    if (tlhs->arrayInfo()) {
      Array *tmp;
      tmp = tlhs->arrayInfo();
      tlhs->clrArray();
      tlhs->MkArray (tmp->Reduce());
      delete tmp;
    }

    /* rhs */
    arhs = u.basic.rhs->Expand (ns, s);
    trhs = arhs->getInstType (s, NULL, 1);
    if (trhs->arrayInfo()) {
      Array *tmp;
      tmp = trhs->arrayInfo();
      trhs->clrArray();
      trhs->MkArray (tmp->Reduce());
      delete tmp;
    }
    
#if 0
    fprintf (stderr, "Conn2x: ");
    alhs->Print (stderr);
    fprintf (stderr, " = ");
    arhs->Print (stderr);
    fprintf (stderr, "\n");
#endif

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
	while (!aes->isend()) {
	  Assert (!bes->isend(), "What?");
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

      if (aes->isSimpleID() && bes->isSimpleID() &&
	  alhs->isBase() && arhs->isBase()) {
	/* a simple ID is either foo without array specifier,
	   or foo[complete deref] (i.e. not a subrange)
	*/

	aes->getID (&lid, &lidx, &lsize);
	bes->getID (&rid, &ridx, &rsize);

#if 0
	fprintf (stderr, "LID: ");
	lid->Print (stderr);
	fprintf (stderr, "; RID: ");
	rid->Print (stderr);
	fprintf (stderr, "\n");
#endif

	lx = lid->Canonical (s);
	rx = rid->Canonical (s);

	act_mk_connection (s->getUserDef(), lid->getName(), lx,
			   rid->getName(), rx);
      }
      else {
	while (!aes->isend()) {
	  aes->getID (&lid, &lidx, &lsize);
	  bes->getID (&rid, &ridx, &rsize);

	  lx = lid->Canonical (s);
	  rx = rid->Canonical (s);

	  if (lidx != -1) {
	    lx = lx->getsubconn (lidx, lsize);
	  }
	  if (ridx != -1) {
	    rx = rx->getsubconn (ridx, rsize);
	  }

	  act_mk_connection (s->getUserDef(),
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
  if (lo) {
    print_expr (fp, lo);
    //if (hi) {
    fprintf (fp, " .. ");
  }
  print_expr (fp, hi);
  fprintf (fp, ": ");

  ActBody *bi;

  for (bi = b; bi; bi = bi->Next()) {
    bi->Print (fp);
  }
  fprintf (fp, ")\n");
}

void act_syn_loop_setup (ActNamespace *ns, Scope *s,
			    const char *id, Expr *lo, Expr *hi,
			    
			    /* outputs */
			    ValueIdx **vx, int *ilo, int *ihi)
{
  Expr *ix;
  
  Assert (s->Add (id, TypeFactory::Factory()->NewPInt()),"What?");
  
  *vx = s->LookupVal (id);
  (*vx)->init = 1;
  (*vx)->u.idx = s->AllocPInt();
  ix = expr_expand (lo, ns, s);
  if (!expr_is_a_const (ix)) {
    act_error_ctxt (stderr);
    print_expr (stderr, lo);
    fprintf (stderr, "\n");
    fatal_error ("Isn't a constant expression");
    Assert (ix->type == E_INT, "Should have been caught earlier");
  }
  *ilo = ix->u.v;
  if (hi) {
    ix = expr_expand (hi, ns, s);
    if (!expr_is_a_const (ix)) {
      act_error_ctxt (stderr);
      print_expr (stderr, hi);
      fprintf (stderr, "\n");
      fatal_error ("Isn't a constant expression");
    }
    Assert (ix->type == E_INT, "Should have been caught earlier");
    *ihi = ix->u.v;
  }
  else {
    *ihi = *ilo-1;
    *ilo = 0;
  }
}

void act_syn_loop_teardown (ActNamespace *ns, Scope *s,
			    const char *id, ValueIdx *vx)
{
  s->DeallocPInt (vx->u.idx, 1);
  s->Del (id);
}


void ActBody_Loop::Expand (ActNamespace *ns, Scope *s)
{
  int ilo, ihi;
  ValueIdx *vx;
  
  Assert (t == ActBody_Loop::SEMI, "What loop is this?");

  act_syn_loop_setup (ns, s, id, lo, hi, &vx, &ilo, &ihi);

  for (; ilo <= ihi; ilo++) {
    s->setPInt (vx->u.idx, ilo);
    b->Expandlist (ns, s);
  }

  act_syn_loop_teardown (ns, s, id, vx);
}

void ActBody_Select::Expand (ActNamespace *ns, Scope *s)
{
  ActBody_Select_gc *igc;
  Expr *guard;
  int ilo, ihi;
  ValueIdx *vx;
  
  for (igc = gc; igc; igc = igc->next) {
    if (!igc->g) {
      /* else */
      igc->s->Expandlist (ns, s);
      return;
    }

    ilo = 0; ihi = 0;
    if (igc->id) {
      Assert (s->Add (igc->id,
		      TypeFactory::Factory()->NewPInt()),
	      "Should have been caught earlier");
      vx = s->LookupVal (igc->id);
      vx->init = 1;
      vx->u.idx = s->AllocPInt();
      
      Expr *e = expr_expand (igc->lo, ns, s);
      if (!expr_is_a_const (e)) {
	act_error_ctxt (stderr);
	print_expr (stderr, igc->lo);
	fprintf (stderr, "\n");
	fatal_error ("Isn't a constant expression");
      }
      Assert (e->type == E_INT, "What?");
      ilo = e->u.v;

      if (igc->hi) {
	e = expr_expand (igc->hi, ns, s);
	if (!expr_is_a_const (e)) {
	  act_error_ctxt (stderr);
	  print_expr (stderr, igc->hi);
	  fprintf (stderr, "\n");
	  fatal_error ("Isn't a constant expression");
	}
	Assert (e->type == E_INT, "Hmm");
	ihi = e->u.v;
      }
      else {
	ihi = ilo-1;
	ilo = 0;
      }
    }
    for (int iter=ilo; iter <= ihi; iter++) {
      if (igc->id) {
	s->setPInt (vx->u.idx, iter);
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
	if (igc->id) {
	  s->DeallocPInt (vx->u.idx, 1);
	  s->Del (igc->id);
	}
	return;
      }
    }
    if (igc->id) {
      s->DeallocPInt (vx->u.idx, 1);
      s->Del (igc->id);
    }
  }
  /* all guards false, skip it */
  if (Act::warn_emptyselect) {
    act_error_ctxt (stderr);
    warning ("All guards in selection are false.");
  }
}

void ActBody_Genloop::Expand (ActNamespace *ns, Scope *s)
{
  ActBody_Select_gc *igc;
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
  case ActBody_Lang::LANG_HSE:
    hse_print (fp, (act_chp *)lang);
    break;
  case ActBody_Lang::LANG_CHP:
    chp_print (fp, (act_chp *)lang);
    break;
  case ActBody_Lang::LANG_SPEC:
    spec_print (fp, (act_spec *)lang);
    break;
  case ActBody_Lang::LANG_REFINE:
    refine_print (fp, (act_refine *)lang);
    break;
  case ActBody_Lang::LANG_SIZE:
    sizing_print (fp, (act_sizing *)lang);
    break;
  case ActBody_Lang::LANG_INIT:
    initialize_print (fp, (act_initialize *)lang);
    break;
  case ActBody_Lang::LANG_DFLOW:
    dflow_print (fp, (act_dataflow *)lang);
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
  act_sizing *sz;
  act_initialize *init;
  act_dataflow *dflow;
  act_languages *all_lang;
  int in_refinement = 0;

  ux = s->getUserDef();
  if (!ux) {
    /* better be the global namespace! */
    all_lang = ActNamespace::Global()->getlang();
  }
  else {
    all_lang = ux->getlang();
  }

  Process *proc = dynamic_cast<Process *>(ux);
  if (proc && proc->hasRefinment() && ActNamespace::Act()->getRefSteps() > 0) {
    in_refinement = 1;
  }

  switch (t) {
  case ActBody_Lang::LANG_PRS:
    if (!in_refinement) {
      p = prs_expand ((act_prs *)lang, ns, s);
      if ((old = all_lang->getprs())) {
	while (old->next) {
	  old = old->next;
	}
	old->next = p;
      }
      else {
	all_lang->setprs (p);
      }
    }
    else {
      all_lang->setprs (NULL);
    }
    break;

  case ActBody_Lang::LANG_CHP:
    if (!in_refinement) {
      c = chp_expand ((act_chp *)lang, ns, s);
      chp_check_channels (c->c, s);
      if (all_lang->getchp()) {
	act_error_ctxt (stderr);
	fatal_error ("Only one chp body permitted");
      }
      //c->next = all_lang->getchp();
      all_lang->setchp (c);
    }
    else {
      all_lang->setchp (NULL);
    }
    break;
    
  case ActBody_Lang::LANG_HSE:
    if (!in_refinement) {
      c = chp_expand ((act_chp *)lang, ns, s);
      if (all_lang->gethse()) {
	act_error_ctxt (stderr);
	fatal_error ("Only one hse body permitted");
      }
      //c->next = all_lang->gethse();
      all_lang->sethse (c);
    }
    else {
      all_lang->sethse (NULL);
    }
    break;

  case ActBody_Lang::LANG_SPEC:
    spec = spec_expand ((act_spec *)lang, ns, s);
    if (all_lang->getspec()) {
      act_spec *tmp  = all_lang->getspec();
      while (tmp->next) {
	tmp = tmp->next;
      }
      tmp->next = spec;
    }
    else {
      all_lang->setspec (spec);
    }
    break;

  case ActBody_Lang::LANG_REFINE:
    if (in_refinement) {
      ActNamespace::Act()->decRefSteps();
      if (((act_refine *)lang)->b) {
	((act_refine *)lang)->b->Expandlist (ns, s);
      }
      ActNamespace::Act()->incRefSteps();
    }
    break;

  case ActBody_Lang::LANG_SIZE:
    if (!in_refinement) {
      sz = sizing_expand ((act_sizing *)lang, ns, s);
      if (all_lang->getsizing()) {
	act_sizing *tmp = all_lang->getsizing();
	while (tmp->next) {
	  tmp = tmp->next;
	}
	tmp->next = sz;
      }
      else {
	all_lang->setsizing (sz);
      }
    }
    else {
      all_lang->setsizing (NULL);
    }
    break;

  case ActBody_Lang::LANG_INIT:
    if (!in_refinement) {
      init = initialize_expand ((act_initialize *)lang, ns, s);
      if (all_lang->getinit()) {
	act_error_ctxt (stderr);
	fatal_error ("Multiple Initialize { } blocks are not permitted.");
      }
      all_lang->setinit (init);
    }
    else {
      all_lang->setinit (NULL);
    }
    break;

  case ActBody_Lang::LANG_DFLOW:
    if (!in_refinement) {
      dflow = dflow_expand ((act_dataflow *)lang, ns, s);
      if (all_lang->getdflow()) {
	act_dataflow *tmp = all_lang->getdflow();
	list_concat (tmp->dflow, dflow->dflow);
	list_free (dflow->dflow);
	FREE (dflow);
      }
      else {
	all_lang->setdflow (dflow);
      }
    }
    else {
      all_lang->setdflow (NULL);
    }
    break;
  }
}

void ActBody_Namespace::Expand (ActNamespace *_ns, Scope *s)
{
  /* expand the top-level of the namespace that was imported */
  //ns->Expand ();
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
    act_merge_attributes (&vx->a, _a);
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
    act_merge_attributes (&vx->array_spec[i], _a);
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

  if (type == 0) {
    ret = new ActBody_Assertion (u.t0.e, u.t0.msg);
  }
  else {
    ret = new ActBody_Assertion (u.t1.nu, u.t1.old);
  }
  if (Next()) {
    ret->Append (Next()->Clone());
  }
  return ret;
}


void ActBody_Inst::updateInstType (InstType *u)
{
  t = t->refineBaseType (u);
}


void ActBody_Print::Expand (ActNamespace *ns, Scope *s)
{
  listitem_t *li;
  const char *tmp;

  tmp = act_error_top ();
  if (tmp) {
    Act::generic_msg ("[");
    Act::generic_msg (tmp);
    Act::generic_msg ("] ");
  }
  else {
    Act::generic_msg ("msg: ");
  }

  for (li = list_first (l); li; li = list_next (li)) {
    act_func_arguments_t *args = (act_func_arguments_t *) list_value (li);
    if (args->isstring) {
      Act::generic_msg (string_char (args->u.s));
    }
    else {
      char buf[10240];
      Expr *e = expr_expand (args->u.e, ns, s);
      sprint_expr (buf, 10240, e);
      Act::generic_msg (buf);
    }
  }
  Act::generic_msg ("\n");
}

ActBody *ActBody_Print::Clone()
{
  return new ActBody_Print(l);
}

