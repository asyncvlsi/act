/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <act/types.h>
#include <act/inst.h>
#include <string.h>


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

ActId *ActId::Expand (ActNamespace *ns, Scope *s)
{
  ActId *ret;
  Array *ax;

  if (a) {
    ax = a->ExpandRef (ns, s);
  }
  else {
    ax = NULL;
  }

  ret = new ActId (string_char (name), ax);

  if (next) {
    ret->next = next->Expand (ns, s);
  }

#if 0
  fprintf (stderr, "expanding id: ");
  this->Print (stderr, NULL);
  fprintf (stderr, " -> ");
  ret->Print (stderr, NULL);
  fprintf (stderr, "\n");
#endif

  return ret;
}

/*------------------------------------------------------------------------
 *
 *  Evaluate identifier: It either returns an evaluated identifier
 *  (i.e. all items expanded out), or in the case of something that
 *  isn't an lval, the value of the parameter.
 *
 *  Return types are:
 *    for lval: E_VAR
 *    otherwise: E_VAR (for non-parameter types)
 *               E_TRUE, E_FALSE, E_TYPE, E_REAL, E_INT,
 *               E_ARRAY, E_SUBRANGE -- for parameter types
 *
 *------------------------------------------------------------------------
 */
Expr *ActId::Eval (ActNamespace *ns, Scope *s, int is_lval)
{
  InstType *it;
  ActId *id;
  Expr *ret;
  Type *base;

  NEW (ret, Expr);

  do {
    it = s->Lookup (getName ());
    if (!it) {
      s = s->Parent ();
    }
  } while (!it && s);
  if (!s) {
    act_error_ctxt (stderr);
    fprintf (stderr, " id: ");
    this->Print (stderr);
    fprintf (stderr, "\n");
    fatal_error ("Not found. Should have been caught earlier...");
  }
  Assert (it->isExpanded (), "Hmm...");

  id = this;
  do {
    /* insttype is "it";
       scope is "s"
       id is "id"

       check array deref:
       if the id has one, then:
          -- either the type is an array type, yay
	  -- otherwise error
    */
    if (id->arrayInfo() && !it->arrayInfo()) {
      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Array de-reference without array type.");
    }
    if (it->arrayInfo() && !id->arrayInfo() && id->Rest()) {
      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Missing array dereference for ``%s''.", id->getName());
    }

    if (id->arrayInfo()) {
      if (!id->arrayInfo()->isDeref() && id->Rest()) {
	act_error_ctxt (stderr);
	fprintf (stderr, " id: ");
	this->Print (stderr);
	fprintf (stderr, "; deref: ");
	id->arrayInfo()->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Using `.' for an array, not an array de-reference");
      }
      if (!it->arrayInfo()->Validate (id->arrayInfo())) {
	act_error_ctxt (stderr);
	fprintf (stderr, " id: ");
	this->Print (stderr);
	fprintf (stderr, "\n type: ");
	it->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Dereference out of range");
      }
    }

    if (id->Rest()) {
      UserDef *u;
    
      u = dynamic_cast<UserDef *>(it->BaseType ());
    
      Assert (it->isExpanded(), "This should be expanded");
      /* WWW: here we would have to check the array index for relaxed
	 parameters */

      id = id->Rest ();
      s = u->CurScope ();
      it = s->Lookup (id->getName ());
    }
    else {
      break;
    }
  } while (1);

  /* identifier is "id"
     scope is "s"
     type is "it"
     any array index has been validated
  */

  base = it->BaseType ();

  /* now, verify that the type is a parameter v/s not a parameter */
  if (TypeFactory::isParamType (base)) {
    ValueIdx *vx = s->LookupVal (id->getName ());
    int offset = 0;
    if (!vx->init) {
      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Uninitialized identifier");
    }

    /* now we check for each type */
    offset = -1;
    if (it->arrayInfo() && id->arrayInfo()) {
      offset = it->arrayInfo()->Offset (id->arrayInfo());
      if (offset == -1) {
	act_error_ctxt (stderr);
	fprintf (stderr, " id: ");
	this->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Index is out of range");
      }
      Assert (offset != -1, "Hmm...");
    }

    if (is_lval) {
      ret->type = E_VAR;
      ret->u.e.l = (Expr *)this;
      ret->u.e.r = (Expr *)it;
      return ret;
    }

    if (it->arrayInfo() &&
	(!id->arrayInfo() || !id->arrayInfo()->isDeref())) {
      int k, idx;
      int sz;

      if (!id->arrayInfo()) {
	sz = it->arrayInfo()->size();
      }
      else {
	sz = id->arrayInfo()->size();
      }
      /* check that the entire array is set! */
      for (k=0; k < sz; k++) {
	if (id->arrayInfo()) {
	  Assert (offset != -1, "what?");
	  idx = vx->u.idx + offset + k;
	}
	else {
	  /* dense array, simple indexing */
	  idx = vx->u.idx + k;
	}
	/* check the appropriate thing is set */
	if ((TypeFactory::isPIntType (base) && !s->issetPInt (idx)) ||
	    (TypeFactory::isPIntsType (base) && !s->issetPInts (idx)) ||
	    (TypeFactory::isPBoolType (base) && !s->issetPBool (idx)) ||
	    (TypeFactory::isPRealType (base) && !s->issetPReal (idx)) ||
	    (TypeFactory::isPTypeType (base) && !s->issetPType (idx))) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " id: ");
	  this->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Array has an uninitialized element");
	}
      }
      if (!id->arrayInfo()) {
	ret->type = E_ARRAY;
	ret->u.e.l = (Expr *) vx;
	ret->u.e.r = (Expr *) s;
      }
      else {
	ret->type = E_SUBRANGE;
	ret->u.e.l = (Expr *) vx;
	NEW (ret->u.e.r, Expr);
	ret->u.e.r->u.e.l = (Expr *)s;
	ret->u.e.r->u.e.r = (Expr *)id->arrayInfo();
      }
      return ret;
    }
    if ((TypeFactory::isPIntType(base) && !s->issetPInt (vx->u.idx + offset)) ||
	(TypeFactory::isPIntsType(base) && !s->issetPInts (vx->u.idx + offset)) ||
	(TypeFactory::isPBoolType(base) && !s->issetPBool (vx->u.idx + offset)) ||
	(TypeFactory::isPRealType(base) && !s->issetPReal (vx->u.idx + offset)) ||
	(TypeFactory::isPTypeType(base) && !s->issetPType (vx->u.idx + offset))) {
      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Uninitialized value");
    }
    if (TypeFactory::isPIntType (base)) {
      ret->type = E_INT;
      ret->u.v = s->getPInt (offset + vx->u.idx);
      return ret;
    }
    else if (TypeFactory::isPIntsType (base)) {
      ret->type = E_INT;
      ret->u.v = s->getPInts (offset + vx->u.idx);
      return ret;
    }
    else if (TypeFactory::isPBoolType (base)) {
      if (s->getPBool (offset + vx->u.idx)) {
	ret->type = E_TRUE;
      }
      else {
	ret->type = E_FALSE;
      }
      return ret;
    }
    else if (TypeFactory::isPRealType (base)) {
      ret->type = E_REAL;
      ret->u.f = s->getPReal (offset + vx->u.idx);
      return ret;
    }
    else if (TypeFactory::isPTypeType (base)) {
      ret->type = E_TYPE;
      ret->u.e.l = (Expr *) s->getPType (offset + vx->u.idx);
      return ret;
    }
    else {
      Assert (0, "Should not be here");
    }
  }
  else {
    /* all this for nothing! */
    ret->type = E_VAR;
    ret->u.e.l = (Expr *)this;
    ret->u.e.r = (Expr *)it;
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
  char buf[10240];
  buf[0] = '\0';
  sPrint (buf, 10240, end);
  fprintf (fp, "%s", buf);
}

#define PRINT_STEP				\
  do {						\
    len = strlen (buf+k);			\
    k += len;					\
    sz -= len;					\
    if (sz <= 0) return;			\
  } while (0)

void ActId::sPrint (char *buf, int sz, ActId *end)
{
  ActId *start = this;
  int k = 0;
  int len;

  while (start && start != end) {
    if (start != this) {
      snprintf (buf+k, sz, ".");
      PRINT_STEP;
    }
    snprintf (buf+k, sz, "%s", string_char(start->name));
    PRINT_STEP;
    if (start->a) {
      start->a->sPrint (buf+k, sz);
      PRINT_STEP;
    }
    start = start->next;
  }
}
