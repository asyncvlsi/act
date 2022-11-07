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
#include <common/config.h>

static int set_suboffset_limit = -1;

//#define DEBUG_CONNECTIONS

/*
  Given a subconnection of the current type,
  search for it and return its index.
*/
int act_connection::suboffset (act_connection *c)
{
  int i;

  if (set_suboffset_limit == -1) {
    if (config_exists ("act.subconnection_limit")) {
      set_suboffset_limit = config_get_int ("act.subconnection_limit");
    }
    if (set_suboffset_limit < 0) { set_suboffset_limit = 30000; }
  }
  
  i = 0;
  while (1) {
    if (a[i] == c) return i;
    i++;
    if (i > set_suboffset_limit) {
      warning ("Exceeded internal limit on subconnection computation.");
      return -1;
    }
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

ActNamespace *act_connection::getnsifglobal()
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
  return vx->global;
}

list_t *_act_create_connection_stackidx (act_connection *c, act_connection **cret);

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
  if (!a) { return 0; }
  return numTotSubconnections ();
}

int act_connection::numTotSubconnections()
{
  ValueIdx *_vx;
  int type;

  _vx = getvx();
  type = getctype();

  Assert (type == 0 || type == 1, "Hmm");

  if (type == 0) {
    if (_vx->t->arrayInfo()) {
      /* it is an array! */
      return _vx->t->arrayInfo()->size();
    }
    else {
      UserDef *ux = dynamic_cast<UserDef *>(_vx->t->BaseType());
      Assert (ux, "hmm...");
      return ux->getNumPorts ();
    }
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

    if (_vx->t->arrayInfo()) {
      return ux->getNumPorts ();
    }
    else {
      int off = myoffset();
      InstType *itmp;
      Assert (off >= 0 && off < ux->getNumPorts(), "Hmm");
      itmp = ux->getPortType (off);
      if (itmp->arrayInfo()) {
	return itmp->arrayInfo()->size();
      }
      else {
	ux = dynamic_cast<UserDef *>(itmp->BaseType());
	Assert (ux, "Hmm");
	return ux->getNumPorts();
      }
    }
  }
}

ValueIdx *act_connection::getvx()
{
  if (vx) {
    return vx;
  }
  else if (parent->vx) {
    /* a.y or a[i] */
    return parent->vx;
  }
  else if (parent->parent->vx) {
    /* a[i].x */
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

#ifdef DEBUG_CONNECTIONS
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


static void dump_conn_rec (act_connection *c, int non_prim = 0)
{
  static int level = 0;

  level++;

  if (level == 1) printf ("-- conn block --\n");
  dump_conn (c);

  if (c->up != NULL) {
    non_prim = 1;
  }
  
  if (c->hasSubconnections()) {
    for (int i=0; i < c->numSubconnections(); i++) {
      if (c->a[i]) {
	printf ("[%2d]", level);
	if (non_prim && c->a[i]->up == NULL && c->a[i]->next != c->a[i]) {
	  printf ("[ERR]");
	}
	dump_conn_rec (c->a[i], non_prim);
      }
    }
  }
  level--;
  if (level == 0) printf ("^^^^^^^^^^^^^^^^\n");
}
#endif


/*
  Return a list of parent connection pointers. used to check for
  self-connections using the following _internal_connection()
  function.
*/
list_t *_parent_list (act_connection *c)
{
  list_t *l = list_new ();
  do {
    list_append (l, c);
    c = c->parent;
  } while (c);
  return l;
}

static int _internal_connection (list_t *l, act_connection *c)
{
  while (c) {
    for (listitem_t *li = list_first (l); li; li = list_next (li)) {
      if (c == (act_connection *) list_value (li)) {
	return 1;
      }
    }
    c = c->parent;
  }
  return 0;
}

void _act_mk_raw_connection (act_connection *c1, act_connection *c2)
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

#if 0
  printf ("after_ack_mk_raw: ");
  c1 = c1->primary();
  dump_conn (c1);
#endif
}

static void _merge_subtrees (UserDef *ux,
			     act_connection *c1, act_connection *c2);
static int _should_swap (UserDef *ux, act_connection *c1,
			 act_connection *c2);
static int _raw_should_swap (UserDef *ux, act_connection *c1,
			 act_connection *c2);
static void _verify_subconn_canonical (UserDef *ux, act_connection *c);

/*
  merge c2 into c1 (primary)

  NOTE: their parents are already connected
*/
static void mk_raw_skip_connection (UserDef *ux,
				    act_connection *c1, act_connection *c2)
{
  act_connection *tmp = c2;
  /* c1 is the root, not c2 */

#if 0
  printf ("here!\n");
  printf ("[raw-skip] entry *****\n");
  dump_conn_rec (c1);
  dump_conn_rec (c2);
  printf ("**********************\n");
#endif  

  if (c2->next == c2) {
    /* nothing to do: c2 has no other connections */
  }
  else {
    /* c2 has connections; merge them into c1, but remove c2 from the
       connection list */

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
      /* not already connected: merge union/find trees */
      int do_swap = _should_swap (ux, c1, c2);
      if (do_swap && t2 != c2) {
	//Assert (t2 != c2, "Hmm..."); why?
	t1->up = t2;
      }
      else {
	// if c2 was the canonical one, it now means that c1 will
	// become canonical as c1's parent is more important than c2's parent
	t2->up = t1;
      }
    }

    /* union-find trees are merged at this point */

    /* any up pointers in c2's union-find tree connection ring that point to c2
       should be updated to c2->up */
    t1 = c2->next;
    while (t1 != c2) {
      if (t1->up == c2) {
	t1->up = c2->up;
      }
      t1 = t1->next;
    }

    c2 = tmp;

    if (!in_ring) {
      /* merge c1, c2 connection ring, and drop c2 itself */
      t1 = c1->next;
      t2 = c2->next;
      c1->next = t2;
      c2->next = t1;
    }
    t1 = c2;
    while (t1->next != c2) {
      t1 = t1->next;
    }
    t1->next = t1->next->next;

    c2->next = NULL;
    c2->up = NULL;
  }
  
  /* now we need to merge any further subconnections between the array
     elements of c1->a and c2->a, if any */

  if (!c2->a) {
    /* no subconnections. done. */
    delete c2;
    return;
  }

  /*-- XXX check this --*/
  c1 = c1->primary();

  if (!c1->a) {
    c1->a = c2->a;
    for (int i=0; i < c2->numSubconnections(); i++) {
      if (c1->a[i]) {
	c1->a[i]->parent = c1;
	_verify_subconn_canonical (ux, c1->a[i]);
      }
    }
    c2->a = NULL;
    delete c2;
    return;
  }

  _merge_subtrees (ux, c1, c2);
  delete c2;
  return;
}


/*
 * Recursively traverse the sub connection structure and ensure
 * that in fact the primary connection pointer should be primary.;
 * This is needed when sub-connection trees are merged.
 */
static void _verify_subconn_canonical (UserDef *ux, act_connection *c)
{
  act_connection *d;

#if 0
  printf ("  > subconn_canon:\n");
  printf ("  > ");
  dump_conn_rec (c);
  printf ("  >\n");
#endif

  d = c->primary();
  if ((c != d) && _raw_should_swap (ux, d, c)) {
    c->up = NULL;
    d->up = c;
  }
  if (c->hasSubconnections()) {
    for (int i=0; i < c->numSubconnections(); i++) {
      if (c->a[i]) {
	_verify_subconn_canonical (ux, c->a[i]);
      }
    }
  }
}

static void _merge_subtrees (UserDef *ux,
			     act_connection *c1, act_connection *c2)
{
  ValueIdx *vx;
  int sz;

  vx = c2->getvx();

  if (!c1->a) {
    if (c2->a) {
      c1->a = c2->a;
      c2->a = NULL;
      /* now fix parent pointers */
      if (vx->t->arrayInfo()) {
	/* the value is an array, but are we connecting arrays or derefs? */
	if (c2->getctype() == 1) {
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
	  _verify_subconn_canonical (ux,c1->a[i]);
	}
      }
      /* now go through each subconnection and make sure it has the
	 right canonical pointer */
    }
  }
  else if (c2->a) {
    if (vx->t->arrayInfo()) {
      if (c2->getctype() == 1) {
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
	mk_raw_skip_connection (ux, c1->a[i], c2->a[i]);
	_verify_subconn_canonical (ux, c1->a[i]);
      }
      else if (c2->a[i]) {
	c1->a[i] = c2->a[i];
	c2->a[i] = NULL;
	c1->a[i]->parent = c1;
	_verify_subconn_canonical (ux, c1->a[i]);
      }
    }
    FREE (c2->a);
    c2->a = NULL;
  }
}

static int _find_earlier_port_array_conn (act_connection *d1,
					  act_connection *d2)
{
  /* p1 = p2: find "earlier" connections */
  list_t *l1 = _act_create_connection_stackidx (d1, NULL);
  list_t *l2 = _act_create_connection_stackidx (d2, NULL);
  listitem_t *li1, *li2;
  li1 = list_first (l1);
  li2 = list_first (l2);
  while (li1 && li2) {
    int x1, x2;
    x1 = (int)(long)list_value (li1);
    x2 = (int)(long)list_value (li2);
    if (x1 < x2) {
      list_free (l1);
      list_free (l2);
      return 0;
    }
    else if (x1 > x2) {
      list_free (l1);
      list_free (l2);
      return 1;
    }
    li1 = list_next (li1);
    li2 = list_next (li2);
  }
  list_free (l1);
  list_free (l2);
  if (li1) {
    return 1;
  }
  else if (li2) {
    return 0;
  }
  //Assert (0, "Should not be here");
  return 0;
}

/*
  d1 original canonical, d2 other connection
  should d2 become the new canonical one?
*/
static int _raw_should_swap (UserDef *ux, act_connection *d1,
			     act_connection *d2)
{
  act_connection *tmp;
  ValueIdx *vx1, *vx2;
  int depth1, depth2;
  act_connection *t1, *t2;

  int p1, p2;

  depth1 = 0;
  depth2 = 0;
  
  /* for global flag, find the root value */
  tmp = d1;
  while (tmp->parent) {
    depth1++;
    tmp = tmp->parent;
  }
  t1 = tmp;
  Assert (tmp->vx, "What?");
  vx1 = tmp->vx;

  tmp = d2;
  while (tmp->parent) {
    depth2++;
    tmp = tmp->parent;
  }
  t2 = tmp;
  Assert (tmp->vx, "What?!");
  vx2 = tmp->vx;

  if (vx1->global || vx2->global) {
    /* set p1, p2 to -1, -1 if a priority has been set;
       0, 0 otherwise
    */
    if (vx2->global && !vx1->global) {
      /* c2 is primary, so swap(c1,c2) */
      return 1;
    }
    else if (vx1->global && !vx2->global) {
      /* nothing has to be done; c1 is primary */
      return 0;
    }
    else {
      /* this test is insufficient */
      p1 = 0;
      p2 = 0;
    }
  }
  else {
    /* not global */
    if (ux) {
      /* user defined type */
      p1 = ux->FindPort (vx1->getName());
      p2 = ux->FindPort (vx2->getName());

      if (p1 > 0 || p2 > 0) {
	/* this should be enough to determine which one is primary */
	if (p2 == 0) return 0;
	/* p2 > 0 */
	if (p1 == 0 || (p2 < p1)) {
	  return 1;
	}
	/* p1 > 0 */
	if (p1 < p2) {
	  return 0;
	}
	return _find_earlier_port_array_conn (d1, d2);
      }
      else {
	if (depth1 > 0 && depth2 > 0) {
	  /* if they share a common prefix, then use the same test */
	  if (t1 == t2) {
	    /* XXX: this really needs to be double-checked... */
	    return _find_earlier_port_array_conn (d2, d2);
	  }
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
    ct = d1->getctype();
    it1 = d1->getvx()->t;
    if (ct == 2 || ct == 3) {
      UserDef *ux;
      ux = dynamic_cast<UserDef *>(it1->BaseType());
      Assert (ux, "Hmm");
      it1 = ux->getPortType (d1->myoffset());
      Assert (it1, "Hmm");
    }
    ct = d2->getctype();
    it2 = d2->getvx()->t;
    if (ct == 2 || ct == 3) {
      UserDef *ux;
      ux = dynamic_cast<UserDef *>(it2->BaseType());
      Assert (ux, "Hmm");
      it2 = ux->getPortType (d2->myoffset());
      Assert (it2, "Hmm");
    }
    if (it1->BaseType() == it2->BaseType()) {
      Type::direction dir1, dir2;
      dir1 = d1->getDir();
      dir2 = d2->getDir();
      if (dir2 != Type::NONE && dir1 != Type::NONE && dir1 != dir2) {
	/* error */
      }
      if (dir1 != Type::NONE && dir2 == Type::NONE) {
	return 1;
      }
      else {
	/* find depth: use lower depth name */
	int l1, l2;
	l1 = 0; tmp = d1;
	while (tmp) { l1++; tmp = tmp->parent; }
	l2 = 0; tmp = d2;
	while (tmp) { l2++; tmp = tmp->parent; }
	if (l2 < l1) {
	  return 1;
	}
	else if (l1 < l2) {
	    return 0;
	}
	else {
	  const char *s1, *s2;

	  s1 = vx1->getName();
	  s2 = vx2->getName();
	  
	  /* ok, use string names! */
	  p1 = strlen (s1);
	  p2 = strlen (s2);
	  if (p2 < p1) {
	    return 1;
	  }
	  else if (p1 == p2) {
	    p1 = strcmp (s1, s2);
	    if (p1 > 0) {
	      return 1;
	    }
	  }
	}
      }
    }
    else {
      /* more specific type wins */
      Type *t = it1->isRelated (it2);
      if (it2->BaseType() == t) {
	return 1;
      }
    }
  }
  return 0;
}


/*
  should we make c2 canonical, rather than c1? 
*/
static int _should_swap (UserDef *ux, act_connection *c1,
			 act_connection *c2)
{
  return _raw_should_swap (ux, c1->primary(), c2->primary());
}

static void _fix_noncanonical_subconns (act_connection *c)
{
  act_connection *prim = c->primary();
  if (c == prim) return;

#ifdef DEBUG_CONNECTIONS
  printf ("In _fix_noncanonical_subconns!\n");
#endif

  list_t *l = _parent_list (c);

  if (c->hasSubconnections()) {
    for (int i=0; i < c->numSubconnections(); i++) {
      if (c->a[i]) {
	act_connection *tmp = c->a[i]->next;
	act_connection *prev = c->a[i];
	while (tmp != c->a[i]) {
	  if (!_internal_connection (l, tmp)) {
	    /* we need to cut tmp out of this connection ring */
	    Assert (tmp->up != tmp, "Residual is canonical?");

	    act_connection *pc = prim->getsubconn (i, prim->numTotSubconnections());

	    prev->next = tmp->next; /* cut */

	    pc = pc->primary();
	    tmp->up = pc;
	    act_connection *old_pc_next = pc->next;
	    pc->next = tmp;
	    tmp->next = old_pc_next;
#ifdef DEBUG_CONNECTIONS
	    printf ("found residual: ");
	    print_id (tmp);
	    printf ("\n");
#endif
	    tmp = prev->next;
	  }
	  else {
	    prev = tmp;
	    tmp = tmp->next;
	  }
	}
      }
    }
  }

  list_free (l);
}


/*
 * if a node in the connection tree is non-canonical, then
 * all subtrees must be non-canonical. In particular, the sub-tree
 * name should include the canonical name derived from the parent as
 * an option.
 */
static void _fix_noncan_to_can2 (UserDef *ux, act_connection *orig, act_connection *can_ver)
{
  if (orig->hasSubconnections()) {
#ifdef DEBUG_CONNECTIONS    
    printf (" -- check subconn\n");
#endif    
    for (int i=0; i < orig->numSubconnections(); i++) {
      if (orig->a[i]) {
	act_connection *tmp = can_ver->getsubconn (i, can_ver->numTotSubconnections());
	if (orig->a[i]->next != orig->a[i]) {
	  if (tmp->primary() != orig->a[i]->primary()) {
#ifdef DEBUG_CONNECTIONS	    
	    printf ("*** need to merge: ");
	    print_id (tmp->primary());
	    printf (" AND ");
	    print_id (orig->a[i]->primary());
	    printf ("\n");
#endif
	    act_connection *x1 = tmp->primary();
	    act_connection *x2 = orig->a[i]->primary();
	    if (_raw_should_swap (ux, x1, x2)) {
	      /* x2 is canonical */
	      act_connection *x3 = x1;
	      x1 = x2;
	      x2 = x3;
	    }
	    _act_mk_raw_connection (x1, x2);
	  }
	}
	_fix_noncan_to_can2 (ux, orig->a[i], tmp);
      }
    }
  }
}


static void _fix_noncan_to_can_transition (UserDef *ux, act_connection *c)
{
  /* check this level */
  if (c != c->primary()) {
#ifdef DEBUG_CONNECTIONS    
    printf ("check: "); print_id (c); printf (" vs "); print_id (c->primary()); printf ("\n");
#endif    
    _fix_noncan_to_can2 (ux, c, c->primary());
  }
  else {
    if (c->hasSubconnections()) {
      for (int i=0; i < c->numSubconnections(); i++) {
	if (c->a[i]) {
	  _fix_noncan_to_can_transition (ux, c->a[i]);
	}
      }
    }
  }
}


void act_mk_connection (UserDef *ux, ActId *id1, act_connection *c1,
			ActId *id2, act_connection *c2)
{
  act_connection *tmp;
  act_connection *d1, *d2;
  ValueIdx *vx1, *vx2, *vxtmp;
  int do_swap = 0;

#ifdef DEBUG_CONNECTIONS
  printf ("before-connect:\n");
  dump_conn_rec (c1);
  dump_conn_rec (c2);
#endif
  
  if (c1 == c2) return;
  d1 = c1->primary();
  d2 = c2->primary();
  if (d1 == d2) return;

  /* for global flag, find the root value */
  tmp = d1;
  while (tmp->parent) {
    tmp = tmp->parent;
  }
  Assert (tmp->vx, "What?");
  vx1 = tmp->vx;

  tmp = d2;
  while (tmp->parent) {
    tmp = tmp->parent;
  }
  Assert (tmp->vx, "What?!");
  vx2 = tmp->vx;
  
  do_swap = _should_swap (ux, c1, c2);
#if 0
  printf ("[do_swap=%d]\n", do_swap);
#endif  

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
    if ((t != it1->BaseType()) && (!t || !t->isEqual (it1->BaseType()))) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Connecting `");
      id1->Print (stderr);
      fprintf (stderr, "' and `");
      id2->Print (stderr);
      fprintf (stderr, "' failed.\n");
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
    
    if (d1 != d2 && (d1 != Type::NONE && d2 != Type::NONE)) {
      /* error */
      act_error_ctxt (stderr);
      fprintf (stderr, "Connecting `");
      id1->Print (stderr);
      fprintf (stderr, "' and `");
      id2->Print (stderr);
      fprintf (stderr, "' failed.\n");
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
	  act_merge_attributes (vx1, vx2, &vx1->a, vx2->a);
	  vx2->a = NULL;
	  if (vx2->array_spec) {
	    if (!vx1->array_spec) {
	      vx1->array_spec = vx2->array_spec;
	      vx2->array_spec = NULL;
	    }
	    else {
	      Assert (vx1->t->arrayInfo(), "Hmm");
	      for (int i=0; i < vx1->t->arrayInfo()->size(); i++) {
		act_merge_attributes (vx1, vx2, &vx1->array_spec[i], vx2->array_spec[i]);
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
	    act_merge_attributes (vx1, vx2, &vx1->array_spec[i], vx2->a);
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
	  act_merge_attributes (vx1, vx2, &vx1->a, vx2->array_spec[i]);
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
	    act_merge_attributes (vx1, vx2, &vx1->array_spec[j], vx2->array_spec[i]);
	    vx2->array_spec[i] = NULL;
	  }
	}
      }
    }
  }

  /* actually connect them! */
  _act_mk_raw_connection (c1, c2);

  
  /* now merge any subtrees */
  _merge_subtrees (ux, c1, c2);

  /* if c1 has sub-connections that are canonical and are not in the
     c1 primary connection list, then they need to be removed and
     migrated into c1 primary.
  */
  _fix_noncanonical_subconns (c1);

  _fix_noncan_to_can_transition (ux, c1);

#ifdef DEBUG_CONNECTIONS
  printf ("after-connect: ");
  dump_conn_rec (c1);

  if (c1 != c1->primary()) {
    printf ("after-connect-on-primary: ");
    c1 = c1->primary();
    dump_conn_rec (c1);
  }

  printf ("-- remaining --\n");
  c2 = c2->primary();
  dump_conn_rec (c2);
#endif
}

void act_merge_attributes (ValueIdx *vx1, ValueIdx *vx2,
			   act_attr_t **x, act_attr *a)
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
	      if (vx1 == vx2) {
		fprintf (stderr, "In adding attributes to: %s\n",
			 vx1->getName());
	      }
	      else {
		fprintf (stderr, "In merging attributes for %s and %s\n",
			 vx1->getName(), vx2->getName());
	      }
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
	      if (vx1 == vx2) {
		fprintf (stderr, "In adding attributes to: %s\n",
			 vx1->getName());
	      }
	      else {
		fprintf (stderr, "In merging attributes for %s and %s\n",
			 vx1->getName(), vx2->getName());
	      }
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
		t->e->u.v_extra = NULL;
	      }
	    }
	    else {
	      act_error_ctxt (stderr);
	      if (vx1 == vx2) {
		fprintf (stderr, "In adding attributes to: %s\n",
			 vx1->getName());
	      }
	      else {
		fprintf (stderr, "In merging attributes for %s and %s\n",
			 vx1->getName(), vx2->getName());
	      }
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
		t->e->u.v_extra = NULL;
	      }
	    }
	    else {
	      act_error_ctxt (stderr);
	      if (vx1 == vx2) {
		fprintf (stderr, "In adding attributes to: %s\n",
			 vx1->getName());
	      }
	      else {
		fprintf (stderr, "In merging attributes for %s and %s\n",
			 vx1->getName(), vx2->getName());
	      }
	      fatal_error ("Attribute `%s': merge rule MIN only for reals and ints", t->attr);
	    }
	  }
	  else if (z[i][2] == '|') {
	    /* OR */
	    if (t->e->type == E_INT) {
	      t->e->u.v |= a->e->u.v;
	    }
	    else {
	      act_error_ctxt (stderr);
	      if (vx1 == vx2) {
		fprintf (stderr, "In adding attributes to: %s\n",
			 vx1->getName());
	      }
	      else {
		fprintf (stderr, "In merging attributes for %s and %s\n",
			 vx1->getName(), vx2->getName());
	      }
	      fatal_error ("Attribute `%s': merge rule OR only for ints", t->attr);
	    }
	  }
	  else if (z[i][2] == '&') {
	    /* AND */
	    if (t->e->type == E_INT) {
	      t->e->u.v &= a->e->u.v;
	      t->e->u.v_extra = NULL;
	    }
	    else {
	      act_error_ctxt (stderr);
	      if (vx1 == vx2) {
		fprintf (stderr, "In adding attributes to: %s\n",
			 vx1->getName());
	      }
	      else {
		fprintf (stderr, "In merging attributes for %s and %s\n",
			 vx1->getName(), vx2->getName());
	      }
	      fatal_error ("Attribute `%s': merge rule AND only for ints", t->attr);
	    }
	  }
	  else if (z[i][2] == 'h') {
	    /* hierarchy: override! */
	    t->e->u = a->e->u;
	  }
	  else if (z[i][2] == 'H') {
	    /* inv hierarchy: skip */
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
	expr_ex_free (t->e);
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

  // check if the root is a process; if so return none
  tmp = this;
  while (tmp->parent) {
    tmp = tmp->parent;
  }
  Assert (tmp->vx, "What?");
  if (TypeFactory::isProcessType (tmp->vx->t)) {
    return Type::NONE;
  }

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


bool act_connection::hasAnyConnection (void)
{
  if (!isPrimary()) return true;
  if (hasDirectconnections()) return true;
  
  if (hasSubconnections()) {
    for (int i=0; i < numSubconnections(); i++) {
      if (hasSubconnections (i)) {
	if (a[i]->hasAnyConnection()) {
	  return true;
	}
      }
    }
  }
  
  return false;
}



/*------------------------------------------------------------------------
 *
 *  disconnect --
 *
 *    Only works for non-canonical names. If a connection is not
 *    canonical, this will remove it from the connection list.
 *
 *------------------------------------------------------------------------
 */
bool act_connection::disconnectable ()
{
  if (isPrimary()) {
    /* primary name cannot be disconnected */
    return false;
  }
  if (a) {
    /*-- also no subconnections! --*/
    return false;
  }
  return true;
}

bool act_connection::disconnect ()
{
  act_connection *prim;
  if (!disconnectable()) {
    return false;
  }

  prim = primary();

  act_connection *tmp;
  tmp = prim;
  while (tmp->next != prim) {
    if (tmp->next == this) {
      /*-- delete it from the ring --*/
      tmp->next = this->next;
      this->next = this;
      this->up = NULL;
      break;
    }
    tmp = tmp->next;
  }
  if (this->up) {
    fatal_error ("Not sure what happened!");
  }
  return true;
}


int ValueIdx::numAttrIdx()
{
  if (!t->isExpanded()) {
    return -1;
  }
  if (!t->arrayInfo()) {
    return 0;
  }
  return t->arrayInfo()->size();
}
