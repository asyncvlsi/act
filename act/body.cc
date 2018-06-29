/*************************************************************************
 *
 *  Copyright (c) 2011-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <act/types.h>
#include <act/inst.h>
#include <string.h>
#include "misc.h"

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
      fatal_error ("Type-checking failed on connection");
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
      
      /* YYY: a real connection */
    }

    delete e;
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

#if 0
    /* lhs */
    alhs = u.general.lhs->Expand (ns, s, 1); /* an lval */
    tlhs = alhs->getInstType (s, 0);

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
      fatal_error ("Type-checking failed on connection");
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
#if 0
	AExprstep *aes = alhs->stepper();
	AExprstep *bes = arhs->stepper();
	/* any other parameter assignment */
	s->BindParam (alhs->(ActId *)e->u.e.l, arhs);
#endif
      }
    }
    else {
#if 0      
      /* YYY: a real connection */
#endif
    }

    delete tlhs;
    delete trhs;
    delete arhs;
    delete alhs;
#endif
    
    break;
  default:
    fatal_error ("Should not be here");
    break;
  }
}

void ActBody_Loop::Expand (ActNamespace *ns, Scope *s)
{
  int ilo, ihi;
  ValueIdx *vx;
  Expr *e;
  ActBody *bi;
  
  Assert (t == ActBody_Loop::SEMI, "What loop is this?");

  Assert (s->Add (id, TypeFactory::Factory()->NewPInt()), "Should have been caught earlier");
  vx = s->LookupVal (id);
  vx->init = 1;
  vx->u.idx = s->AllocPInt();

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
    FREE (e);
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
  FREE (e);

  if (!lo) {
    ilo = 0;
    ihi--;
  }

  for (; ilo <= ihi; ilo++) {
    s->setPInt (vx->u.idx, ilo);
    b->Expandlist (ns, s);
  }

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

void ActBody_Lang::Expand (ActNamespace *ns, Scope *s)
{
  switch (t) {
  case ActBody_Lang::LANG_PRS:
    prs_expand ((act_prs *)lang, ns, s);
    break;

  case ActBody_Lang::LANG_CHP:
  case ActBody_Lang::LANG_HSE:
    chp_expand ((act_chp *)lang, ns, s);
    break;
  default:
    fatal_error ("Unknown language");
    break;
  }
}

void ActBody_Namespace::Expand (ActNamespace *_ns, Scope *s)
{
  /* expand the top-level of the namespace that was imported */
  ns->Expand ();
}


void ActBody::Expandlist (ActNamespace *ns, Scope *s)
{
  ActBody *b;

  for (b = this; b; b = b->Next()) {
    b->Expand (ns, s);
  }
}
