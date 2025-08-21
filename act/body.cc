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
#include <common/misc.h>
#include <common/config.h>
#include <common/log.h>

ActBody::ActBody(int line)
{
  _line = line;
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

void ActBody::insertNext (ActBody *b)
{
  ActBody *t = next;

  if (!b) return;

  next = b;
  b = b->Tail();
  b->next = t;
}

ActBody_Inst::ActBody_Inst (int line, InstType *it, const char *_id)
: ActBody (line)
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

  a = t->arrayInfo();
  t->clrArray ();
  if (t->isExpanded() && TypeFactory::isUserType (t)) {
    t->sPrint (buf, 10240);
    ActNamespace::Act()->mfprintf (fp, "%s", buf);
  }
  else {
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
  act_error_setline (getLine());

  if (it->arrayInfo() && it->arrayInfo()->size() == 0) {
    return;
  }

  if (it->arrayInfo() && it->arrayInfo()->size() == 0) {
    act_error_ctxt (stderr);
    fatal_error ("Instance `%s': zero-length array creation not permitted", id);
  }
  
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
    
    if (vx->haveAttrIdx()) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Array being extended after attributes specified on elements.\n");
      fprintf (stderr, "\tType: ");
      vx->t->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
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
	  Assert (0, "no ptype arrays or pstruct dynamic arrays");
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

	    /*-- now extend connection imports --*/
	    if (TypeFactory::isUserType (vx->t)) {
	      UserDef *ux = dynamic_cast<UserDef *> (vx->t->BaseType());
	      Arraystep *newelems = x->arrayInfo()->stepper (it->arrayInfo());
	      while (!newelems->isend()) {
		_act_int_import_connections (vx->connection(), ux, vx->t->arrayInfo(), newelems->index());
		newelems->step();
	      }
	      delete newelems;
	    }

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
      x->MkCached ();
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
    ActId *x1, *x2;
    act_connection *c1, *c2;

    x1 = u.t1.id1->Expand (ns, s);
    x2 = u.t1.id2->Expand (ns, s);

    c1 = x1->Canonical (s);
    c2 = x2->Canonical (s);

    if (((u.t1.op == 0) && (c1 != c2)) || ((u.t1.op == 1) && (c1 == c2))) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Identifiers `");
      x1->Print (stderr);
      fprintf (stderr, "' and `");
      x2->Print (stderr);
      fprintf (stderr, "' are %sconnected.\n", u.t1.op == 0 ? "not " : "");

      if (u.t1.msg) {
	char *s = Strdup (u.t1.msg+1);
	s[strlen(s)-1] = '\0';
	fprintf (stderr, "   message: %s\n", s);
      }
      fatal_error ("Aborted execution on failed assertion");
      exit (1);
    }
  }
}

#if 0
static void print_id (act_connection *c)
{
  if (!c) return;
  
  ActId *id = c->toid();

  id->Print (stdout);
  delete id;
}
#endif

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
	tmp = vx->t->arrayInfo()->unOffset (c->myoffset());
	printf ("%s", vx->u.obj.name);
	tmp->Print (stdout);
	delete tmp;
      }
      else {
	UserDef *ux;
	ux = dynamic_cast<UserDef *> (vx->t->BaseType());
	Assert (ux, "what?");
	printf ("%s.%s", vx->u.obj.name, ux->getPortName (c->myoffset()));
      }
    }
    else {
      vx = c->parent->parent->vx;
      Assert (vx, "What?");
      
      Array *tmp;
      tmp = vx->t->arrayInfo()->unOffset (c->myoffset());
      UserDef *ux;
      ux = dynamic_cast<UserDef *> (vx->t->BaseType());
      Assert (ux, "what?");

      printf ("%s", vx->u.obj.name);
      tmp->Print (stdout);
      printf (".%s", ux->getPortName (c->myoffset()));

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

#if 0
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
#endif

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

	s->BindParamFull ((ActId *)e->u.e.l, astep->getPType());
	
	astep->step();
	Assert (astep->isend(), "What?");
	delete astep;
      }
      else {
	/* any other parameter assignment */
	s->BindParamFull ((ActId *)e->u.e.l, arhs);
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
	    act_mk_connection (s->getUserDef(), id, lcx, rid, rcx);
	  }
	  else {
	    //Assert (trhs->arrayInfo(), "What?");
	    //Assert (rsize == trhs->arrayInfo()->size(), "What?");
	    rcx = rcx->getsubconn (ridx, rsize)->primary();

	    act_mk_connection (s->getUserDef(), id, lcx, rid, rcx);
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

	  act_mk_connection (s->getUserDef(), lid, lx, rid, rx);
	  
	  lhsstep->step();
	  rhsstep->step();
	}
	Assert (rhsstep->isend(), "What?");
	delete lhsstep;
      }
      delete rhsstep;
    }

    if (e) { FREE (e); }
    delete ex;
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
	s->BindParamFull (alhs->toid(), astep->getPType());
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
	    s->BindParamFull (lhsid, bes);
	  }
	  else {
	    s->BindParamFull (lhsid, bes, lhsidx);
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

	act_mk_connection (s->getUserDef(), lid, lx, rid, rx);
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

	  act_mk_connection (s->getUserDef(), lid, lx, rid, rx);
	  
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
  fprintf (fp, "%s", id);
  fprintf (fp, ":");
  if (hi) {
    print_expr (fp, lo);
    fprintf (fp, " .. ");
    print_expr (fp, hi);
  }
  else {
    print_expr (fp, lo);
  }
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
  *ilo = ix->u.ival.v;
  if (hi) {
    ix = expr_expand (hi, ns, s);
    if (!expr_is_a_const (ix)) {
      act_error_ctxt (stderr);
      print_expr (stderr, hi);
      fprintf (stderr, "\n");
      fatal_error ("Isn't a constant expression");
    }
    Assert (ix->type == E_INT, "Should have been caught earlier");
    *ihi = ix->u.ival.v;
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

    act_error_setline (getLine());
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
      ilo = e->u.ival.v;

      if (igc->hi) {
	e = expr_expand (igc->hi, ns, s);
	if (!expr_is_a_const (e)) {
	  act_error_ctxt (stderr);
	  print_expr (stderr, igc->hi);
	  fprintf (stderr, "\n");
	  fatal_error ("Isn't a constant expression");
	}
	Assert (e->type == E_INT, "Hmm");
	ihi = e->u.ival.v;
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
      act_error_setline (getLine());
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
  if (Act::empty_select) {
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
      act_error_setline (getLine());
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
  case ActBody_Lang::LANG_EXTERN:
    lang_extern_print (fp, nm, lang);
    break;
  }
}

void ActBody_Lang::Expand (ActNamespace *ns, Scope *s)
{
  UserDef *ux;
  act_prs *p;
  act_chp *c;
  act_spec *spec;
  act_sizing *sz;
  act_initialize *init;
  act_dataflow *dflow;
  act_languages *all_lang;
  void *ext;
  int in_refinement = 0;

  ux = s->getUserDef();
  if (!ux) {
    /* better be the global namespace! */
    all_lang = ActNamespace::Global()->getlang();
  }
  else {
    all_lang = ux->getlang();
  }

  if (ux) {
    /* check if there are nested refines that will override language
       bodies, OR if there are pending refines that do the same */
    if ((ux->getRefineList() != NULL &&
	 ActNamespace::Act()->getRefSteps() >=
	 list_ivalue (list_first (ux->getRefineList()))) ||
	ux->moreRefinesExist (ActNamespace::Act()->getRefSteps())) {
      in_refinement = 1;
    }
  }

#if 0
  printf ("in ux: %s\n", ux ? ux->getName() : "-none-");
  printf ("  in-ref: %d\n", in_refinement);
  printf ("  cursteps: %d\n", ActNamespace::Act()->getRefSteps());
#endif

  switch (t) {
  case ActBody_Lang::LANG_PRS:
    if (!in_refinement) {
      act_prs *old;
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
    break;

  case ActBody_Lang::LANG_CHP:
    if (!in_refinement) {
      c = chp_expand ((act_chp *)lang, ns, s);
      chp_check_channels (c->c, s);
      if (all_lang->getchp()) {
	act_error_ctxt (stderr);
	fatal_error ("Only one chp body permitted");
      }
      all_lang->setchp (c);
    }
    break;
    
  case ActBody_Lang::LANG_HSE:
    if (!in_refinement) {
      c = chp_expand ((act_chp *)lang, ns, s);
      if (all_lang->gethse()) {
	act_error_ctxt (stderr);
	fatal_error ("Only one hse body permitted");
      }
      if (c->c && c->c->type == ACT_HSE_FRAGMENTS) {
	// validate fragments
	act_chp_lang_t *x;
	struct Hashtable *lH = hash_new (4);
	x = c->c;
	while (x) {
	  if (x->label) {
	    if (hash_lookup (lH, x->label)) {
	      act_error_ctxt (stderr);
	      fatal_error ("Duplicate fragment label `%s'!", x->label);
	    }
	    hash_add (lH, x->label);
	  }
	  else {
	    if (x != c->c) {
	      act_error_ctxt (stderr);
	      fatal_error ("HSE fragment missing a label!");
	    }
	  }
	  x = x->u.frag.next;
	}
	x = c->c;
	while (x) {
	  if (x->u.frag.nextlabel) {
	    if (!hash_lookup (lH, x->u.frag.nextlabel)) {
	      act_error_ctxt (stderr);
	      fatal_error ("Missing target fragment labelled `%s'!", x->u.frag.nextlabel);
	    }
	  }
	  else {
	    listitem_t *li;
	    for (li = list_first (x->u.frag.exit_conds); li; li = list_next (li)) {
	      li = list_next (li);
	      if (!hash_lookup (lH, (const char *)list_value (li))) {
		act_error_ctxt (stderr);
		fatal_error ("Missing target fragment labelled `%s'!",
			     (char *) list_value (li));
	      }
	    }
	  }
	  x = x->u.frag.next;
	}
	hash_free (lH);
      }
      all_lang->sethse (c);
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
      act_refine *r = (act_refine *)lang;
      Assert (r, "What?");

      if (ux->acceptRefine (ActNamespace::Act()->getRefSteps(), r->nsteps)) {
	list_t *old = ux->getRefineList ();

        ux->pushRefine (r->nsteps, old);
	ux->setRefineList (r->refsublist);
	ActNamespace::Act()->decRefSteps(r->nsteps);

	if (r->b) {
	  r->b->Expandlist (ns, s);
	}
	ux->setRefineList (old);
        ux->popRefine (); 
	ActNamespace::Act()->incRefSteps(r->nsteps);
      }
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
    break;

  case ActBody_Lang::LANG_INIT:
    if (!in_refinement) {
      act_initialize *old;
      init = initialize_expand ((act_initialize *)lang, ns, s);
      if ((old = all_lang->getinit())) {
	while (old->next) {
	  old = old->next;
	}
	old->next = init;
      }
      else {
	all_lang->setinit (init);
      }
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
    break;

  case ActBody_Lang::LANG_EXTERN:
    if (!in_refinement) {
      ext = lang_extern_expand (nm, lang, ns, s);
      if (all_lang->getextern (nm)) {
	act_error_ctxt (stderr);
	warning ("Duplicate external language `%s'; ignoring previous one",
		 nm);
      }
      all_lang->setextern (nm, ext);
    }
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
    _arr = arr->Expand (_ns, s, 1);
  }
  else {
    _arr = NULL;
  }

  if (!_arr) {
    act_merge_attributes (vx, vx, &vx->a, _a);
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
    act_merge_attributes (vx, vx, &vx->array_spec[i], _a);
  }
  if (_arr) {
     delete _arr;
  }
}


void ActBody::Expandlist (ActNamespace *ns, Scope *s)
{
  ActBody *b;

  for (b = this; b; b = b->Next()) {
    if (dynamic_cast<ActBody_Namespace *> (b)) {
      ns = (dynamic_cast<ActBody_Namespace *>(b)->getNS());
      s = ns->CurScope ();
    }
    else {
      act_error_setline (b->getLine());
      b->Expand (ns, s);
    }
  }
  act_error_setline (-1);
}

UserDef *_act_userdef_replace (ActNamespace *replace, ActNamespace *newns,
			       UserDef *orig)
{
  if (!replace) {
    return orig;
  }
  list_t *l = replace->findNSPath (orig);
  if (l) {
    listitem_t *li;
    ActNamespace *tns = newns;
    for (li = list_first (l); li; li = list_next (li)) {
      tns = tns->findNS ((char *) list_value (li));
      Assert (tns, "Cloning failed, ns not found!");
    }
    UserDef *u = tns->findType (orig->getName());
    Assert (u, "What?");
    list_free (l);
    return u;
  }
  return orig;
}

Function *_act_fn_replace (ActNamespace *replace, ActNamespace *newns,
			   Function *orig)
{
  UserDef *ux = _act_userdef_replace (replace, newns, orig);
  Function *fn = dynamic_cast <Function *> (ux);
  Assert (fn, "What?!");
  return fn;
}

ActBody *ActBody_Conn::Clone (ActNamespace *replace, ActNamespace *newns)
{
  ActBody_Conn *ret;

  if (type == 0) {
    ret = new ActBody_Conn (_line, u.basic.lhs->Clone(replace, newns),
			    u.basic.rhs->Clone (replace, newns));
  }
  else {
    Assert (type == 1, "Hmm");
    ret = new ActBody_Conn (_line, u.general.lhs->Clone (replace, newns),
			    u.general.rhs->Clone (replace, newns));
  }
  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}

InstType *_act_clone_replace (ActNamespace *replace, ActNamespace *newns,
			      InstType *orig)
{
  if (!replace) {
    return orig;
  }
  if (TypeFactory::isUserType (orig)) {
    UserDef *u = dynamic_cast<UserDef *> (orig->BaseType());
    Assert (u, "what?");
    UserDef *unew = _act_userdef_replace (replace, newns, u);
  
    if (u != unew) {
      InstType *ret = new InstType (orig);
      ret->refineBaseType (unew);
      ret->MkCached ();
      return ret;
    }
    return orig;
  }
  return orig;
}


ActBody *ActBody_Inst::Clone (ActNamespace *replace, ActNamespace *newns)
{
  InstType *nit;

  nit = _act_clone_replace (replace, newns, t);
  
  ActBody_Inst *ret = new ActBody_Inst(_line, nit, id);
  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}

ActBody *ActBody_Lang::Clone (ActNamespace *replace, ActNamespace *newns)
{
  ActBody_Lang *ret;
  void *newlang;

  // XXX: within a language, we need to fix expressions and function
  // calls
  switch (t) {
  case LANG_CHP:
  case LANG_HSE:
    newlang = chp_dup ((act_chp *)lang, replace, newns);
    break;

  case LANG_REFINE:
    newlang = refine_dup ((act_refine *)lang, replace, newns);
    break;

  case LANG_DFLOW:
    newlang = dflow_dup ((act_dataflow *)lang, replace, newns);
    break;

  case LANG_PRS:
    newlang = prs_dup ((act_prs *)lang, replace, newns);
    break;
    
  case LANG_SPEC:
    newlang = spec_dup ((act_spec *)lang, replace, newns);
    break;
    
  case LANG_SIZE:
    newlang = sizing_dup ((act_sizing *)lang, replace, newns);
    break;
    
  case LANG_INIT:
    newlang = lang;
    break;
    
  case LANG_EXTERN:
    newlang = lang_extern_clone (nm, lang, replace, newns);
    break;
  }

  if (t == LANG_EXTERN) {
    ret = new ActBody_Lang (_line, t, nm, newlang);
  }
  else {
    ret = new ActBody_Lang (_line, t, NULL, newlang);
  }

  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}

ActBody *ActBody_Loop::Clone (ActNamespace *replace, ActNamespace *newns)
{
  ActBody_Loop *ret;
  Expr *newlo, *newhi;

  newlo = expr_update (expr_predup (lo), replace, newns);
  newhi = expr_update (expr_predup (hi), replace, newns);

  ret = new ActBody_Loop (_line, id, newlo, newhi, b->Clone(replace, newns));
  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}


ActBody *ActBody_Select::Clone (ActNamespace *replace, ActNamespace *newns)
{
  ActBody_Select *ret;

  ret = new ActBody_Select (_line, gc->Clone(replace, newns));

  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}

ActBody *ActBody_Genloop::Clone (ActNamespace *replace, ActNamespace *newns)
{
  ActBody_Genloop *ret;

  ret = new ActBody_Genloop (_line, gc->Clone(replace, newns));

  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}

static act_attr *_clone_attrib (act_attr *a,
				ActNamespace *orig, ActNamespace *newns)
{
  act_attr *ret, *cur;
  ret = NULL;
  while (a) {
    if (!ret) {
      NEW (ret, act_attr);
      cur = ret;
    }
    else {
      NEW (cur->next, act_attr);
      cur = cur->next;
    }
    cur->next = NULL;
    cur->attr = a->attr;
    cur->e = expr_predup (a->e);
    cur->e = expr_update (cur->e, orig, newns);
    a = a->next;
  }
  return ret;
}


ActBody *ActBody_Attribute::Clone(ActNamespace *replace, ActNamespace *newns)
{
  ActBody_Attribute *ret;
  Array *narr;
  if (arr) {
    narr = arr->Clone(replace, newns);
  }
  else {
    narr = arr;
  }
  ret = new ActBody_Attribute (_line, inst, _clone_attrib (a, replace, newns), narr);
  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}

ActBody *ActBody_Namespace::Clone(ActNamespace *replace, ActNamespace *newns)
{
  ActBody_Namespace *ret;
  list_t *l;

  ret = NULL;
  if (replace) {
    l = replace->findNSPath (ns);
    if (l) {
      listitem_t *li;
      ActNamespace *tmp = newns;
      for (li = list_first (l); li; li = list_next (li)) {
	tmp = tmp->findNS ((char *) list_value (li));
	Assert (tmp, "Cloning failed");
      }
      list_free (l);
      ret = new ActBody_Namespace (tmp);
    }
  }
  if (!ret) {
    ret = new ActBody_Namespace (ns);
  }

  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}

ActBody *ActBody_Assertion::Clone(ActNamespace *replace, ActNamespace *newns)
{
  ActBody_Assertion *ret;

  // XXX: expressions
  if (type == 0) {
    Expr *tmp = expr_predup (u.t0.e);
    tmp = expr_update (tmp, replace, newns);
    ret = new ActBody_Assertion (_line, tmp, u.t0.msg);
  }
  else if (type == 1) {
    ret = new ActBody_Assertion (_line, u.t1.id1->Clone (replace, newns), 
				 u.t1.id2->Clone (replace, newns), 
				 u.t1.op, u.t1.msg);
  }
  else {
    Assert (0, "Should not be here");
  }
  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
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

ActBody *ActBody_Print::Clone(ActNamespace *replace, ActNamespace *newns)
{
  list_t *newl;
  listitem_t *li;
  act_func_arguments_t *fn, *fn2;
  newl = list_dup (l);
  for (li = list_first (newl); li; li = list_next (li)) {
    fn = (act_func_arguments_t *) list_value (li);
    NEW (fn2, act_func_arguments_t);
    fn2->isstring = fn->isstring;
    if (fn->isstring) {
      fn2->u.s = fn->u.s;
    }
    else {
      fn2->u.e = expr_predup (fn->u.e);
      expr_update (fn2->u.e, replace, newns);
    }
    list_value (li) = fn2;
  }
  ActBody_Print *ret = new ActBody_Print(_line, newl);

  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}


void ActBody::updateInstType (list_t *namelist, InstType *it, bool handle_subref)
{
  ActBody *b = this;
  listitem_t *li;

  for (b = this; b; b = b->Next()) {
    if (dynamic_cast<ActBody_Inst *> (b)) {
      ActBody_Inst *bi = dynamic_cast<ActBody_Inst *> (b);
      for (li = list_first (namelist); li; li = list_next (li)) {
	if (strcmp ((char *)list_value (li), bi->getName()) == 0) {
	  break;
	}
      }
      if (li) {
	ActBody *override_asserts =
	  new ActBody_OverrideAssertion (bi->getLine(),
					 bi->getName(),
					 bi->getType(),
					 it);

	bi->updateInstType (it);
	bi->insertNext (override_asserts);
      }
    }
    else if (dynamic_cast<ActBody_Loop *> (b)) {
      ActBody_Loop *bl = dynamic_cast<ActBody_Loop *> (b);
      if (bl->getBody()) {
	bl->getBody()->updateInstType (namelist, it, handle_subref);
      }
    }
    else {
      ActBody_Select_gc *sel;
      if (dynamic_cast<ActBody_Select *> (b)) {
	sel = dynamic_cast<ActBody_Select *> (b)->getGC();
      }
      else if (dynamic_cast<ActBody_Genloop *> (b)) {
	sel = dynamic_cast<ActBody_Genloop *> (b)->getGC();
      }
      else if (dynamic_cast<ActBody_Lang *> (b) && handle_subref) {
	ActBody_Lang *l = dynamic_cast<ActBody_Lang *> (b);
	if (l->gettype() == ActBody_Lang::LANG_REFINE) {
	  act_refine *r = (act_refine *) l->getlang();
	  if (r->b && r->nsteps <= ActNamespace::Act()->getRefSteps ()) {
	    ActNamespace::Act()->decRefSteps (r->nsteps);
	    r->b->updateInstType (namelist, it, true);
	    ActNamespace::Act()->incRefSteps (r->nsteps);
	  }
	}
	sel = NULL;
      }
      else {
	sel = NULL;
      }
      while (sel) {
	if (sel->getBody()) {
	  sel->getBody()->updateInstType (namelist, it, handle_subref);
	}
	sel = sel->getNext();
      }
    }
  }
}


ActBody_Select_gc *ActBody_Select_gc::Clone (ActNamespace *replace, ActNamespace *newns)
{
  Expr *newlo, *newhi, *newg;
  newlo = expr_update (expr_predup (lo), replace, newns);
  newhi = expr_update (expr_predup (hi), replace, newns);
  newg = expr_update (expr_predup (g), replace, newns);
  
  ActBody_Select_gc *ret =
    new ActBody_Select_gc (id, newlo, newhi, newg, s->Clone(replace, newns));
  if (next) {
    ret->next = next->Clone(replace, newns);
  }
  return ret;
}

ActBody *ActBody_OverrideAssertion::Clone(ActNamespace *replace, ActNamespace *newns)
{
  InstType *x1, *x2;
  x1 = _act_clone_replace (replace, newns, _orig_type);
  x2 = _act_clone_replace (replace, newns, _new_type);
  
  ActBody_OverrideAssertion *ret =
    new ActBody_OverrideAssertion (_line, _name_check, x1, x2);

  if (Next()) {
    ret->Append (Next()->Clone(replace, newns));
  }
  return ret;
}


void ActBody_OverrideAssertion::Expand (ActNamespace *ns, Scope *s)
{
  if (s->Lookup (_name_check) == NULL) {
    // this is conditionally created; if it is not created, skip the check!
    return;
  }
  
  Array *tmpa = _orig_type->arrayInfo();
  _orig_type->clrArray();
  InstType *orig = _orig_type->Expand (ns, s);
  InstType *chk =  _new_type->Expand (ns, s);
  InstType *ochk = chk;

  if (chk->isEqual (orig)) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Override for: %s\n", _name_check);
    fprintf (stderr, "   Orig: "); orig->Print (stderr);
    fprintf (stderr, "\n    New: "); chk->Print (stderr);
    fprintf (stderr, "\n");
    warning ("Override not required when the types are the same");
    return;
  }
  while (chk) {
    if (chk->isEqual (orig)) {
      break;
    }
    UserDef *ux = dynamic_cast <UserDef *> (chk->BaseType());
    if (!ux) {
      chk = NULL;
    }
    else {
      chk = ux->getParent ();
      if (chk) {
	Assert (chk->arrayInfo() == NULL, "What?");
      }
    }
  }
  if (!chk) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Override for: %s\n", _name_check);
    fprintf (stderr, "   Orig: "); orig->Print (stderr);
    fprintf (stderr, "\n    New: "); ochk->Print (stderr);
    fprintf (stderr, "\n");
    fatal_error ("Illegal override; the new type doesn't implement the original.");
  }
  _orig_type->MkArray (tmpa);
}


ActBody *ActBody::Clone(ActNamespace */*replace*/,
			ActNamespace */*newns*/)
{
  return NULL;
}
