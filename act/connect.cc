/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2019 Rajit Manohar
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
#include <string.h>
#include "config.h"


/*
  Given a subconnection of the current type,
  search for it and return its index.
*/
int act_connection::suboffset (act_connection *c)
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
	tmp = vx->t->arrayInfo()->unOffset (c->myoffset());
	t = new ActId (vx->u.obj.name, tmp);
      }
      else {
	UserDef *ux;
	ux = dynamic_cast<UserDef *> (vx->t->BaseType());

	t = new ActId (vx->u.obj.name);
	t->Append (new ActId (ux->getPortName (c->myoffset())));
      }
    }
    else {
      vx = c->parent->parent->vx;
      Assert (vx, "What?");
      
      Array *tmp;
      tmp = vx->t->arrayInfo()->unOffset (c->parent->myoffset());
      UserDef *ux;
      ux = dynamic_cast<UserDef *> (vx->t->BaseType());
      Assert (ux, "what?");

      t = new ActId (vx->u.obj.name, tmp);
      t->Append (new ActId (ux->getPortName (c->myoffset())));
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

static void _merge_subtrees (act_connection *c1, act_connection *c2)
{
  ValueIdx *vx;
  int sz;

  vx = c1->getvx();

  if (!c1->a) {
    if (c2->a) {
      c1->a = c2->a;
      c2->a = NULL;
      /* now fix parent pointers */
      if (vx->t->arrayInfo()) {
	/* the value is an array, but are we connecting arrays or derefs? */
	if (c1->getctype() == 1) {
	  UserDef *ux;
	  ux = dynamic_cast <UserDef *> (vx->t->BaseType());
	  Assert (ux, "Why are we here?!");
	  sz = ux->getNumPorts();
	}
	else {
	  sz = vx->t->arrayInfo()->size();
	}
      }
      else {
	UserDef *ux;
	ux = dynamic_cast <UserDef *> (vx->t->BaseType());
	Assert (ux, "Hmm!");
	sz = ux->getNumPorts();
      }
      for (int i=0; i < sz; i++) {
	if (c1->a[i]) {
	  c1->a[i]->parent = c1;
	}
      }
    }
  }
  else if (c2->a) {
    if (vx->t->arrayInfo()) {
      if (c1->getctype() == 1) {
	UserDef *ux;
	ux = dynamic_cast <UserDef *> (vx->t->BaseType());
	Assert (ux, "Why are we here?!");
	sz = ux->getNumPorts();
      }
      else {
	sz = vx->t->arrayInfo()->size();
      }
    }
    else {
      UserDef *ux;
      ux = dynamic_cast <UserDef *> (vx->t->BaseType());
      Assert (ux, "Hmm?!");
      sz = ux->getNumPorts();
    }
    for (int i=0; i < sz; i++) {
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
}

/* debugging */
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
      tmp = vx->t->arrayInfo()->unOffset (c->parent->myoffset());
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



static void _merge_attributes (act_attr_t **x, act_attr *a);


void act_mk_connection (UserDef *ux, const char *s1, act_connection *c1,
			const char *s2, act_connection *c2)
{
  int p1, p2;
  act_connection *tmp;
  ValueIdx *vx1, *vx2, *vxtmp;
  int do_swap = 0;

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
    /* set p1, p2 to -1, -1 if a priority has been set;
       0, 0 otherwise
    */
    if (vx2->global && !vx1->global) {
      /* c2 is primary, so swap(c1,c2) */
      do_swap = 1;
      p1 = -1;
      p2 = -1;

    }
    else if (vx1->global && !vx2->global) {
      /* nothing has to be done; c1 is primary */
      p1 = -1;
      p2 = -1;
    }
    else {
      /* this test is insufficient */
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
	/* this should be enough to determine which one is primary */
	if (p2 > 0 && (p1 == 0 || (p2 < p1))) {
	  do_swap = 1;
	}
      }
    }
  }
  if (!ux || (p1 == 0 && p2 == 0)) {
    /* without refinement, this would be resolved by string
       comparisons. we need to check subtyping relationship at this
       point */
    InstType *it1, *it2;
    int ct;
    ct = c1->getctype();
    it1 = c1->getvx()->t;
    if (ct == 2 || ct == 3) {
      UserDef *ux;
      ux = dynamic_cast<UserDef *>(it1->BaseType());
      Assert (ux, "Hmm");
      it1 = ux->getPortType (c1->myoffset());
      Assert (it1, "Hmm");
    }
    ct = c2->getctype();
    it2 = c2->getvx()->t;
    if (ct == 2 || ct == 3) {
      UserDef *ux;
      ux = dynamic_cast<UserDef *>(it2->BaseType());
      Assert (ux, "Hmm");
      it2 = ux->getPortType (c2->myoffset());
      Assert (it2, "Hmm");
    }
    if (it1->BaseType() == it2->BaseType()) {
      Type::direction d1, d2;
      d1 = c1->getDir();
      d2 = c2->getDir();
      if (d2 != Type::NONE && d1 != Type::NONE && d1 != d2) {
	/* error */
      }
      if (d1 != Type::NONE && d2 == Type::NONE) {
	do_swap = 1;
      }
      else {
	/* ok, use string names! */
	p1 = strlen (s1);
	p2 = strlen (s2);
	if (p2 < p1) {
	  do_swap = 1;
	}
	else if (p1 == p2) {
	  p1 = strcmp (s1, s2);
	  if (p1 > 0) {
	    do_swap = 1;
	  }
	}
      }
    }
    else {
      /* more specific type wins */
      Type *t = it1->isRelated (it2);
      if (it2->BaseType() == t) {
	do_swap = 1;
      }
    }
  }

  if (do_swap) {
    tmp = c1;
    c1 = c2;
    c2 = tmp;
    vxtmp = vx1;
    vx1 = vx2;
    vx2 = vxtmp;
  }
  
  /* verify that c1 can be the primary! */
  {
    InstType *it1, *it2;
    int ct;
    ct = c1->getctype();
    it1 = c1->getvx()->t;
    if (ct == 2 || ct == 3) {
      UserDef *ux;
      ux = dynamic_cast<UserDef *>(it1->BaseType());
      Assert (ux, "Hmm");
      it1 = ux->getPortType (c1->myoffset());
      Assert (it1, "Hmm");
    }
    ct = c2->getctype();
    it2 = c2->getvx()->t;
    if (ct == 2 || ct == 3) {
      UserDef *ux;
      ux = dynamic_cast<UserDef *>(it2->BaseType());
      Assert (ux, "Hmm");
      it2 = ux->getPortType (c2->myoffset());
      Assert (it2, "Hmm");
    }
    Type *t = it1->isRelated (it2);
    if (t != it1->BaseType()) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Connecting `%s' and `%s' failed.\n", s1, s2);
      fprintf (stderr, "\tType 1: ");
      it1->Print (stderr);
      fprintf (stderr, "\n\tType 2: ");
      it2->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Illegal combination of types");
    }

    /* now check direction flags */
    Type::direction d1, d2;

    d1 = c1->getDir();
    d2 = c2->getDir();
#if 0
    printf ("dir flags: %s, %s\n", Type::dirstring (d1),
	    Type::dirstring (d2));
    dump_conn (c1);
    dump_conn (c2);
#endif
    
    if (d1 != d2 && d1 != Type::NONE) {
      /* error */
      act_error_ctxt (stderr);
      fprintf (stderr, "Connecting `%s' and `%s' failed.\n", s1, s2);
      fprintf (stderr, "\tType 1: ");
      it1->Print (stderr);
      fprintf (stderr, " [dir=%s]\n\tType 2: ", Type::dirstring (d1));
      it2->Print (stderr);
      fprintf (stderr, " [dir=%s]\n", Type::dirstring (d2));
      fatal_error ("Illegal combination of types");
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
	  act_merge_attributes (&vx1->a, vx2->a);
	  vx2->a = NULL;
	  if (vx2->array_spec) {
	    if (!vx1->array_spec) {
	      vx1->array_spec = vx2->array_spec;
	      vx2->array_spec = NULL;
	    }
	    else {
	      Assert (vx1->t->arrayInfo(), "Hmm");
	      for (int i=0; i < vx1->t->arrayInfo()->size(); i++) {
		act_merge_attributes (&vx1->array_spec[i], vx2->array_spec[i]);
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
	  int i = c1->myoffset ();

	  if (!vx1->array_spec) {
	    int sz;

	    sz = vx1->t->arrayInfo()->size();
	    MALLOC (vx1->array_spec, act_attr_t *, sz);
	    for (; sz > 0; sz--) {
	      vx1->array_spec[sz-1] = NULL;
	    }
	  }
	  if (vx2->a) {
	    act_merge_attributes (&vx1->array_spec[i], vx2->a);
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
    int i = c2->myoffset ();

    if (vx1 == c1->vx) {
      if (!c1->parent) {
	/* look at the attributes on vx2 */
	if (vx2->array_spec && vx2->array_spec[i]) {
	  act_merge_attributes (&vx1->a, vx2->array_spec[i]);
	  vx2->array_spec[i] = NULL;
	}
      }
      else {
	if (c1->parent->vx && (c1->parent->vx == vx1) && (vx1->t->arrayInfo())) {
	  int j = c1->myoffset ();

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
	    act_merge_attributes (&vx1->array_spec[j], vx2->array_spec[i]);
	    vx2->array_spec[i] = NULL;
	  }
	}
      }
    }
  }

  /* actually connect them! */
  mk_raw_connection (c1, c2);
  
  /* now merge any subtrees */
  _merge_subtrees (c1, c2);
}

void act_merge_attributes (act_attr_t **x, act_attr *a)
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

/*
  Get subconnection pointer, allocating various pieces as necessary 
*/
act_connection *act_connection::getsubconn(int idx, int sz)
{
  Assert (0 <= idx && idx < sz, "What?");
  if (!a) {
    MALLOC (a, act_connection *, sz);
    for (int i=0; i < sz; i++) {
      a[i] = NULL;
    }
  }
  if (!a[idx]) {
    a[idx] = new act_connection(this);
  }
  return a[idx];
}

/*
  Constructor
*/
act_connection::act_connection (act_connection *_parent)
{
  // value pointer
  vx = NULL;

  // parent connection object
  parent = _parent;

  // union-find tree
  up = NULL;

  // circular list of aliases
  next = this;

  // no subconnection slots; lazy allocation
  // getsubconn() does the allocation as needed
  a = NULL;
}


Type::direction act_connection::getDir ()
{
  act_connection *tmp;
  int polarity;
  int isfirst;
  Type::direction ret;

  ret = Type::NONE;
  isfirst = 1;

  // to identify my direction flag:
  //   go to the root of the connection. that type has a direction
  //   if anything in the hierarchy doesn't have a direction flag
  tmp = this;

  polarity = 0;
  while (tmp) {
    if (tmp->vx) {
      if (tmp->vx->t->getDir() == Type::NONE ||
	  tmp->vx->t->getDir() == Type::IN  ||
	  tmp->vx->t->getDir() == Type::OUT) {
	if (tmp->vx->t->getDir() == Type::NONE) {
	  return Type::NONE;
	}
	else if (tmp->vx->t->getDir() == Type::IN) {
	  if (polarity) {
	    return Type::OUT;
	  }
	  else {
	    return Type::IN;
	  }
	}
	else {
	  if (polarity) {
	    return Type::IN;
	  }
	  else {
	    return Type::OUT;
	  }
	}
      }
      if (isfirst) {
	ret = tmp->vx->t->getDir();
	isfirst = 0;
      }
      if (tmp->vx->t->getDir() == Type::OUTIN) {
	polarity = 1 - polarity;
      }
    }
    tmp = tmp->parent;
  }
  return ret;
}


act_connection *act_connection::primary()
{
  act_connection *root = NULL;
  act_connection *tmp;
  act_connection *c = this;

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
