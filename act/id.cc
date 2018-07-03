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

#if 0
    printf ("checking: ");
    id->Print (stdout);
    printf (" in [%x] ", it->BaseType());
    fflush (stdout);
    it->Print (stdout);
    printf ("\n");
#endif
    
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
    
      Assert (it->isExpanded(), "This should be expanded");
      
      u = dynamic_cast<UserDef *>(it->BaseType ());
    
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
    if (!vx->init && !is_lval) {
      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Uninitialized identifier");
    }

    /* now we check for each type */
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

act_connection *act_mk_id_canonical (act_connection *c)
{
  act_connection *root = NULL;
  act_connection *tmp;
  
  tmp = c;
  /* find root */
  while (tmp->up) {
    tmp = tmp->up;
  }
  root = tmp;

  /* flatten connection */
  while (c->up) {
    tmp = c->up;
    c->up = root;
    c = tmp;
  }
  return root;
}

ValueIdx *ActId::rawValueIdx (Scope *s)
{
  ValueIdx *vx;
  act_connection *cx;
  vx = NULL;
  
  Assert (s->isExpanded(), "ActId::FindCanonical called on unexpanded scope");

  while (s && !vx) {
    vx = s->LookupVal (getName());
    s = s->Parent ();
  }

  if (!vx) {
    act_error_ctxt (stderr);
    fprintf (stderr, " Id: ");
    Print (stderr);
    fprintf (stderr, "\nNot found in scope!\n");
    exit (1);
  }

  if (!vx->init) {
    vx->init = 1;
    NEW (cx, act_connection);
    vx->u.obj.c = cx;
    cx->vx = vx;
    cx->parent = NULL;  /* no parent pointer; this is the root */
    cx->a = NULL;	/* no slots; lazy allocation */

    /* union-find tree and search list */
    cx->up = NULL;
    cx->next = cx;
    vx->u.obj.name = getName();
  }

  return vx;
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

#if 0
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
#endif  
}

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
    
/*
  Return canonical connection slot for identifier in scope.
  If it is a subrange identifier, it will be the array id rather than
  a reference to the subrange.
*/
act_connection *ActId::Canonical (Scope *s)
{
  ValueIdx *vx;
  act_connection *cx, *tmpx;
  act_connection *parent_cx;
  ActId *id;
  ActId *idrest;
  
  Assert (s->isExpanded(), "ActId::FindCanonical called on unexpanded scope");

  vx = rawValueIdx(s);
  cx = vx->u.obj.c;

  cx = act_mk_id_canonical (cx);

  /* now cx could be something else; but it has the same type has the
     id's core name 
          i.e. for foo[x].bar...  or foo.bar...
               the type of vx/cx is the same as "foo"

	  foo = q[5];

           foo.bar --> foo's cx would be q[5]
        so really it is q[5].bar

            foo became q[5]
  */

  id = this;
  idrest = id->Rest();
  if (idrest) {
    /* now check to see if id->Reset() has a *DIFFERENT* 
       canonical name in the subtype! 

       1. It is a global! In that case, abort this loop and return
       the connection idx for the global.

       2. It is the same, no change. Continue as below.
       
       3. It is a different name. By construction, the new name must
       be a port.
       
       Replace id->Rest with the new id!
    */

    act_connection *cxrest;
    ValueIdx *vxrest;
    UserDef *ux;

    /* find canonical connection of rest in the scope for the inst
       type */

    ux = dynamic_cast<UserDef *>(vx->t->BaseType());
    Assert (ux, "What?");
    cxrest = id->Rest()->Canonical (ux->CurScope());

#if 0
    printf ("------\n");
    printf ("Original name: ");
    id->Rest()->Print (stdout);
    printf ("\nUnique name: ");
    print_id (cxrest);
    printf("\n");
    dump_conn (cxrest);
    printf ("------\n");
#endif    

    /* YYY: convert cxrest into an ID! */
    vxrest = cxrest->vx;
    if (!vxrest) {
      vxrest = cxrest->parent->vx;
      if (!vxrest) {
	vxrest = cxrest->parent->parent->vx;
      }
    }
    Assert (vxrest, "What?");
    if (vxrest->global) {
      /* we're done! */
      return cxrest;
    }

    /* not global, so re-write the port to be the canonical port name.
       Note that this *will* be canonical at all levels. 
    */

    /* compute new idrest! */
    list_t *stk = list_new ();

    stack_push (stk, cxrest);
    while (cxrest) {
      if (cxrest->vx) {
	cxrest = cxrest->parent;
      }
      else if (cxrest->parent->vx) {
	cxrest = cxrest->parent->parent;
      }
      else {
	Assert (cxrest->parent->parent->vx, "What?");
	cxrest = cxrest->parent->parent->parent;
      }
    }
    list_free (stk);
  }

#if 0
  printf ("canonical: ");
  id->Print (stdout);
  printf (" [rest=");
  idrest->Print (stdout);
  printf ("] ");
  fflush (stdout);
#endif
  
  do {
#if 0 
    printf (" --> ");
    fflush (stdout);
#endif    
    /* vx is the value 
       cx is the object
    */
    if (id->arrayInfo() && id->arrayInfo()->isDeref()) {
      /* find array slot, make vx the value, cx the connection id for
	 the canonical value */
      if (!cx->a) {
	/* no slots */
	int sz = vx->t->arrayInfo()->size();
	MALLOC (cx->a, act_connection *, sz);
	for (int i=0; i < sz; i++) {
	  cx->a[i] = NULL;
	}
      }

      int idx = vx->t->arrayInfo()->Offset (id->arrayInfo());
      Assert (idx != -1, "This should have been caught earlier");

      if (!cx->a[idx]) {
	/* slot is empty, need to allocate it */
	NEW (cx->a[idx], act_connection);

	cx->a[idx]->vx = NULL; /* get array valueidx slot from the parent */
	cx->a[idx]->parent = cx;

	/* no connections */
	cx->a[idx]->up = NULL;
	cx->a[idx]->next = cx->a[idx];

	/* no subslots */
	cx->a[idx]->a = NULL;
      }

      cx = cx->a[idx];
      cx = act_mk_id_canonical (cx);

      /* find the value slot for the canonical name */
      if (cx->vx) {
	vx = cx->vx;
      }
      else if (cx->parent->vx) {
	/* array */
	vx = cx->parent->vx;
      }
      else {
	/* array and subtype */
	vx = cx->parent->parent->vx;
      }
      Assert (vx, "What?");
    }
    
    if (idrest) {
      int portid;
      UserDef *ux;
      /* find port id */

      ux = dynamic_cast<UserDef *>(vx->t->BaseType());
      if (cx->vx == vx) {
	/* raw connection, basetype is fine */
      }
      else {
	if (id->arrayInfo()) {
	  if (cx->parent->vx == vx) {
	    /* cx is just array, basetype is fine */
	  }
	  else {
	    /* foo[].bar; arrays are first, so bar is the offset */
	    ux = dynamic_cast <UserDef *>
	      (ux->getPortType (offset (cx->parent->a, cx))->BaseType());
	  }
	}
	else {
	  ux = dynamic_cast<UserDef *>
	    (ux->getPortType (offset (cx->parent->a, cx))->BaseType());
	}
      }
      Assert (ux, "Should have been caught earlier!");

#if 0
      printf ("Type: %s, port %s\n", ux->getName(), idrest->getName());
#endif      
      
      portid = ux->FindPort (idrest->getName());
      Assert (portid > 0, "What?");

      portid--;

      if (!cx->a) {
	MALLOC (cx->a, act_connection *, ux->getNumPorts());
	for (int i=0; i < ux->getNumPorts(); i++) {
	  cx->a[i] = NULL;
	}
      }

      if (!cx->a[portid]) {
	/* slot empty */
	NEW (cx->a[portid], act_connection);

	cx->a[portid]->vx = NULL;
	cx->a[portid]->parent = cx;
	cx->a[portid]->up = NULL;
	cx->a[portid]->next = cx->a[portid];
	cx->a[portid]->a = NULL;
      }
      
      cx = cx->a[portid];
      cx = act_mk_id_canonical (cx);

      if (cx->vx) {
	vx = cx->vx;
      }
      else if (cx->parent->vx) {
	vx = cx->parent->vx;
      }
      else {
	vx = cx->parent->parent->vx;
      }
      Assert (vx, "What?");
    }
    id = idrest;
    if (idrest) {
      // we are fine, because we re-computed
      // the canonical port at the top.
      idrest = idrest->Rest();
    }
  } while (id);

#if 0
  printf ("\n");
  fflush (stdout);
#endif  

  return cx;
}  

