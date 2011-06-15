/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <act/types.h>
#include <act/inst.h>
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

void ActBody_Inst::Expand (ActNamespace *ns, Scope *s)
{
  InstType *x;
  /* typechecking should all pass, so there shouldn't be an issue
     beyond creating the actual object. For arrays, there may be
     duplicate dereference issues
  */

  printf ("Expand inst: ");
  t->Print (stdout);
  printf (" : id = %s\n", id);

  /* 
     expand instance type!
  */
  t = t->Expand (ns, s);

  x = s->Lookup (id);
  if (x) {
    /* sparse array */
    printf ("Sparse array!\n");
  }
  else {
    Assert (s->Add (id, t), "Should succeed; what happened?!");
  }
}



void ActBody_Conn::Expand (ActNamespace *ns, Scope *s)
{
  printf ("Expand conn!\n");
}

void ActBody_Loop::Expand (ActNamespace *ns, Scope *s)
{
  printf ("Expand loop!\n");
}

void ActBody_Select::Expand (ActNamespace *ns, Scope *s)
{
  printf ("Expand select\n");
}
