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
#include <stdio.h>
#include <act/act.h>
#include <act/types.h>
#include <act/iter.h>
#include <string.h>
#include <common/misc.h>
#include <common/hash.h>

/*------------------------------------------------------------------------
 *
 *  Function inlining support
 *
 *------------------------------------------------------------------------
 */
struct act_inline_table {
  int ex_func;		    /* expand functions recursively or not? */
  bool macro_mode;	    /* true for macros, false otherwise */
  bool allow_dag;	    /* true for dag expressions, false otherwise */
  Scope *sc;
  struct Hashtable *state;
  act_inline_table *parent;
};


act_inline_table *act_inline_new (Scope *sc, act_inline_table *parent,
				  bool ismacro, bool allow_dag)
{
  act_inline_table *ret;
  NEW (ret, act_inline_table);
  ret->ex_func = 0;
  ret->sc = sc;
  ret->parent = parent;

  if (parent) {
    ret->macro_mode = parent->macro_mode;
    ret->allow_dag = parent->allow_dag;
  }
  else {
    ret->macro_mode = ismacro;
    ret->allow_dag = allow_dag;
  }

  if (!ret->sc && ret->parent) {
    ret->sc = ret->parent->sc;
  }

  if (!ret->sc) {
    /*-- you need a scope at all times --*/
    FREE (ret);
    return NULL;
  }
  ret->state = hash_new (4);
  return ret;
}

void act_inline_free (act_inline_table *T)
{
  hash_iter_t it;
  hash_bucket_t *b;
  hash_iter_init (T->state, &it);
  while ((b = hash_iter_next (T->state, &it))) {
    if (b->v) {
      act_inline_value *iv = (act_inline_value *) b->v;
      iv->clear ();
      delete iv;
    }
  }
  hash_free (T->state);
  FREE (T);
}
		      

int act_inline_isbound (act_inline_table *tab, const char *name)
{
  while (tab) {
    if (hash_lookup (tab->state, name)) {
      return 1;
    }
    tab = tab->parent;
  }
  return 0;
}

void act_dump_table (act_inline_table *tab)
{
  printf ("Bindings:\n\t");
  while (tab) {
    hash_iter_t it;
    hash_bucket_t *b;
    hash_iter_init (tab->state, &it);
    while ((b = hash_iter_next (tab->state, &it))) {
      printf (" %s -> ", b->key);
      act_inline_value *ev = (act_inline_value *) b->v;
      Assert (ev, "What?");
      ev->Print (stdout);
      printf ("\n");
    }
    printf ("\n\t");
    tab = tab->parent;
  }
}

static act_inline_value
_lookup_binding (act_inline_table *Hs,
		 const char *name,
		 Array *deref,
		 ActId *rest, int err = 1)
{
  hash_bucket_t *b;
  act_inline_table *origHs = Hs;
  act_inline_value rv;

  while (Hs) {
    b = hash_lookup (Hs->state, name);
    if (b) {
      int ni, nb;
      int sz, array_sz;
      InstType *it = Hs->sc->Lookup (name);
      Data *d = NULL;

      if (!b->v) {
	// return invalid binding
	return rv;
      }
      
      if (TypeFactory::isStructure (it)) {
	d = dynamic_cast<Data *>(it->BaseType());
	d->getStructCount (&nb, &ni);
	sz = nb + ni;
      }
      else {
	sz = -1;
      }

      if (it->arrayInfo()) {
	// XXX: deal with deref
	array_sz = it->arrayInfo()->size();
      }
      else {
	array_sz = -1;
      }

      /* return a copy */
      act_inline_value bval;
      bval = *((act_inline_value *)b->v);
      if (rest) {
	if (!d) {
	  act_error_ctxt (stderr);
	  fatal_error ("Structure binding lookup without structure?");
	}
	int sz2;
	int off = d->getStructOffset (rest, &sz2);
	Assert (off >= 0 && off <= sz, "What?");
	Assert (off + sz2 <= sz, "What?");
	Assert (sz2 > 0, "Hmm");

	int array_off = 0;

	if (array_sz != -1) {
	  Assert (deref, "Array var without deref?");
	  array_off = it->arrayInfo()->Offset (deref);
	  act_error_ctxt (stderr);
	  fatal_error ("Access to index that is out of bounds in _lookup!");
	}

	Assert (bval.is_struct, "What?");

	// get type of the "rest"
	InstType *rit;
	act_type_var (d->CurScope(), rest, &rit);
	Assert (rit, "Hmm");

	if (sz2 == 1 && !TypeFactory::isStructure (rit)) {
	  rv.is_struct = 0;
	}
	else {
	  rv.is_struct = 1;
	}
	rv.is_just_id = 0;
	if (rv.is_struct) {
	  MALLOC (rv.u.arr, Expr *, sz2);
	  rv.struct_count = sz2;
	}

	ActId **fields = NULL;
	if (bval.isSimple()) {
	  int *types;
	  fields = d->getStructFields (&types);
	  FREE (types);
	}
	for (int i=0; i < sz2; i++) {
	  Expr *bind_val;
	  if (bval.isSimple()) {
	    ActId *xnew;
	    if (bval.getVal()->type != E_VAR) {
	      fprintf (stderr, " PTR is %p\n", bval.getVal());
	      bval.Print (stderr);
	      fprintf (stderr, "\n");
	    }
	    Assert (bval.getVal()->type == E_VAR, "Hmm");
	    xnew = ((ActId *)bval.getVal()->u.e.l)->Clone ();
	    if (deref) {
	      xnew->Tail()->setArray (deref->Clone());
	    }
	    xnew->Tail()->Append (fields[i+off]->Clone());
	    NEW (bind_val, Expr);
	    bind_val->type = E_VAR;
	    bind_val->u.e.r = NULL;
	    bind_val->u.e.l = (Expr *)xnew;
	  }
	  else {
	    bind_val = bval.u.arr[array_off*sz+i+off];
	  }
	  if (rv.is_struct) {
	    rv.u.arr[i] = bind_val;
	  }
	  else {
	    rv.u.val = bind_val;
	  }
	  if (err && (bind_val == NULL)) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Access to: `%s.", name);
	    rest->Print (stderr);
	    fprintf (stderr, "' has a NULL binding.\n");
	    fatal_error ("Uninitialized fields for `%s'.", name);
	  }
	}
	if (fields) {
	  for (int i=0; i < sz; i++) {
	    delete fields[i];
	  }
	  FREE (fields);
	}
      }
      else if (deref) {
	if (bval.isSimple()) {
	  ActId *xnew;
	  if (bval.getVal()->type != E_VAR) {
	    fprintf (stderr, " PTR is %p\n", bval.getVal());
	    bval.Print (stderr);
	    fprintf (stderr, "\n");
	  }
	  Assert (bval.getVal()->type == E_VAR, "Hmm");
	  xnew = ((ActId *)bval.getVal()->u.e.l)->Clone ();
	  xnew->Tail()->setArray (deref->Clone());

	  Expr *bind_val;
	  NEW (bind_val, Expr);
	  bind_val->type = E_VAR;
	  bind_val->u.e.r = NULL;
	  bind_val->u.e.l = (Expr *)xnew;
	  rv.u.val = bind_val;
	  rv.is_just_id = 1;
	}
	else {
	  int array_off = it->arrayInfo()->Offset (deref);
	  Assert (array_off != -1, "Out of bounds?");
	  rv.u.val = bval.u.arr[array_off];
	  if (rv.u.val->type == E_VAR) {
	    rv.is_just_id = 1;
	  }
	}
      }
      else {
	// either simple ID access or structure access
	rv = bval;
	if (!bval.isSimple()) {
	  Assert (sz == -1 || bval.struct_count == sz, "What?");
	  Assert (array_sz == -1 || bval.array_sz == array_sz, "What?!");
	  MALLOC (rv.u.arr, Expr *,
		  (sz == -1 ? 1 : bval.struct_count)*
		  (array_sz == -1 ? 1 : bval.array_sz));
	  for (int i=0; i < sz; i++) {
	    rv.u.arr[i] = bval.u.arr[i];
	    if (err) {
	      if (rv.u.arr[i] == NULL) {
		act_error_ctxt (stderr);
		fatal_error ("Found NULL binding for `%s': certain fields are undefined.", name);
	      }
	    }
	  }
	}
      }
      return rv;
    }
    Hs = Hs->parent;
  }
  if (err) {
    act_error_ctxt (stderr);
    fatal_error ("Inlining failed! Variable `%s' used before being defined",
		 name);
  }
  return rv;
}

static void _populate_widths (Data *d, int *widths, int *pos)
{
  Assert (d, "What?");
  for (int i=0; i < d->getNumPorts(); i++) {
    int sz = 1;
    InstType *it = d->getPortType (i);
    if (it->arrayInfo()) {
      sz = it->arrayInfo()->size();
    }
    if (TypeFactory::isStructure (it)) {
      while (sz > 0) {
	_populate_widths (dynamic_cast<Data *>(it->BaseType()), widths, pos);
	sz--;
      }
    }
    else if (TypeFactory::isBoolType (it)) {
      while (sz > 0) {
	widths[*pos] = -1;
	*pos = *pos + 1;
	sz--;
      }
    }
    else if (TypeFactory::isIntType (it)) {
      while (sz > 0) {
	widths[*pos] = TypeFactory::bitWidth (it);
	*pos = *pos + 1;
	sz--;
      }
    }
    else {
      Assert (0, "Structure?!");
    }
  }
}


static Expr *_wrap_width (Expr *e, int w)
{
  Expr *tmp;
  
  if (!e) return e;

  if (e->type == E_BUILTIN_INT) {
    int val;
    if (e->u.e.r) {
      Assert (act_expr_getconst_int (e->u.e.r, &val), "Hm...");
    }
    else {
      val = 1;
    }
    if (val == w) {
      return e;
    }
  }
  NEW (tmp, Expr);
  tmp->type = E_BUILTIN_INT;
  tmp->u.e.l = e;
  tmp->u.e.r = const_expr (w);
  return tmp;
}

static void _update_binding (act_inline_table *Hs, ActId *id,
			     act_inline_value update)
{
  InstType *xit = Hs->sc->Lookup (id->getName());
  int sz = 1;
  Data *xd;
  int *widths;
  Array *deref = id->arrayInfo();

#if 0
  printf ("update binding for: ");
  id->Print (stdout);
  printf (" / type: ");
  xit->Print (stdout);
  printf ("\n");


  printf ("| updated value should be: ");
  update.Print (stdout);
  printf ("\n");
#endif
  
  Assert (xit, "What?");
  xd = NULL;

  if (TypeFactory::isStructure (xit)) {
    xd = dynamic_cast<Data *> (xit->BaseType());
    Assert (xd, "Hmm");
    int nb, ni;
    xd->getStructCount (&nb, &ni);
    sz = nb + ni;
    MALLOC (widths, int, sz);
    int v = 0;
    _populate_widths (xd, widths, &v);
  }
  else {
    Assert (sz == 1, "Hmm");
    MALLOC (widths, int, sz);
    widths[0] = TypeFactory::bitWidth (xit);
    if (TypeFactory::isBoolType (xit)) {
      widths[0] = -1;
    }
  }

  hash_bucket_t *b;
  
  /* find partial or total update, and update entry in the hash table! 
     tmp is the FULL structure binding.
     if id->Rest() then we need to do a partial assignment.
   */
  b = hash_lookup (Hs->state, id->getName());
  if (!b) {
    act_inline_value lv, bindv;
    lv = _lookup_binding (Hs, id->getName(), id->arrayInfo(), NULL, 0);
#if 0
    printf ("I'm here, lv = ");
    lv.Print (stdout);
    printf ("\n");
#endif
    if (lv.isValid()) {
      // found a valid binding
      bindv = lv;
      if (bindv.isSimple() && (xd || xit->arrayInfo())) {
#if 0
	printf ("| populate and elaborate binding\n");
#endif
	// structure, elaborate the binding
	bindv.elaborateStructId (xd, xit->arrayInfo());
#if 0
	printf ("| ");
	bindv.Print (stdout);
	printf ("\n");
#endif
      }
    }
    else {
      // create a dummy NULL binding
      if (xd || xit->arrayInfo()) {
	if (xd) {
	  bindv.is_struct = 1;
	  bindv.struct_count = sz;
	}

	if (xit->arrayInfo()) {
	  bindv.is_array = 1;
	  bindv.array_sz = xit->arrayInfo()->size();
	}

	MALLOC (bindv.u.arr, Expr *, sz*(bindv.is_array ? bindv.array_sz : 1));

	for (int i=0; i < sz*(bindv.is_array ? bindv.array_sz : 1); i++) {
	  bindv.u.arr[i] = NULL;
	}

#if 0
	printf ("| ");
	bindv.Print (stdout);
	printf ("\n");
#endif
      }
    }
    b = hash_add (Hs->state, id->getName());

    /* add int(.) wrapper if needed */
    if (xd || xit->arrayInfo()) {
      int tot = sz * (bindv.is_array ? bindv.array_sz : 1);
      for (int i=0; i < tot; i++) {
	if (!Hs->macro_mode && bindv.u.arr[i]) {
	  if (xd) {
	    if (widths[i % sz] > 0) {
	      bindv.u.arr[i] = _wrap_width (bindv.u.arr[i], widths[i]);
	    }
	  }
	  else {
	    if (widths[0] > 0) {
	      bindv.u.arr[i] = _wrap_width (bindv.u.arr[i], widths[0]);
	    }
	  }
	}
      }
    }
    else {
      if (!Hs->macro_mode && bindv.u.val && widths[0] > 0) {
	bindv.u.val = _wrap_width (bindv.u.val, widths[0]);
	bindv.is_just_id = 0;
      }
    }
    b->v = new act_inline_value();
    *((act_inline_value *)b->v) = bindv;

#if 0
    printf ("F| ");
    bindv.Print (stdout);
    printf ("\n");
#endif
  }
  act_inline_value resv = _lookup_binding (Hs, id->getName(),
					   id->arrayInfo(), NULL, 0);

#if 0
    printf (" >> update scenario:\n ");
    printf ("   >> orig: ");
    resv.Print (stdout);
    printf ("\n   >>> update: ");
    update.Print (stdout);
    printf ("\n");
#endif

  if (id->Rest()) {
    Assert (xd, "What?!");
    int sz2;

    int off = xd->getStructOffset (id->Rest(), &sz2);
    int array_off = 0;
    if (deref) {
      array_off = xit->arrayInfo()->Offset (deref);
      Assert (array_off != -1, "Invalid array index?");
    }
    Assert (off >= 0 && off < sz, "What?");
    Assert (off + sz2 <= sz, "What?");
    Assert (!update.isSimple() || sz2 == 1, "Hmm");
    if (resv.isSimple()) {
      resv.elaborateStructId (xd, xit->arrayInfo());
    }
    Assert (!resv.isSimple(), "Hmm");
    for (int i=0; i < sz2; i++) {
      Expr *updatev;
      if (update.isSimple()) {
	updatev = update.getVal();
      }
      else {
	updatev = update.u.arr[i];
      }
      if (!Hs->macro_mode && updatev && widths[off+i] > 0) {
	resv.u.arr[array_off*sz + off+i] = _wrap_width (updatev, widths[off+i]);
      }
      else {
	resv.u.arr[array_off*sz + off+i] = updatev;
      }
#if 0      
      printf ("set offset %d to ", off+i);
      if (update.u.arr[i]) {
	print_expr (stdout, update.u.arr[i]);
      }
      else {
	printf ("null");
      }
      printf ("\n");
#endif      
    }
#if 0
    for (int i=0; i < sz; i++) {
      printf ("cur %d = ", i);
      if (res[i]) {
	print_expr (stdout, resv.u.arr[i]);
      }
      else {
	printf ("null");
      }
      printf ("\n");
    }
#endif	
  }
  else {
    if (xd || deref) {
      if (update.isSimple() && !deref) {
	resv = update;
      }
      else {
	if (resv.isSimple()) {
	  resv.elaborateStructId (xd, xit->arrayInfo());
	}
	int array_off = 0;
	if (deref) {
	  array_off = xit->arrayInfo()->Offset (deref);
	  Assert (array_off != -1, "Out of bounds");
	}
	if (xd) {
	  for (int i=0; i < sz; i++) {
	    if (!Hs->macro_mode && update.u.arr[i] && widths[i] > 0) {
	      resv.u.arr[array_off*sz + i] = _wrap_width (update.u.arr[i], widths[i]);
	    }
	    else {
	      resv.u.arr[array_off*sz + i] = update.u.arr[i];
	    }
	  }
	}
	else {
	  if (!Hs->macro_mode && update.u.val && widths[0] > 0) {
	    resv.u.arr[array_off] = _wrap_width (update.u.val, widths[0]);
	  }
	  else {
	    resv.u.arr[array_off] = update.u.val;
	  }
	}
      }
    }
    else {
      Assert (update.isSimple(), "Hmm");

      if (resv.isValid()) {
	if (resv.isSimple()) {
	  resv.u.val = update.u.val;
	  resv.is_just_id = update.is_just_id;
	}
	else {
	  if (resv.u.arr) {
	    FREE (resv.u.arr);
	  }
	  resv.u.val = update.u.val;
	  resv.is_just_id = update.is_just_id;
	}
      }
      else {
	resv = update;
      }

      if (!Hs->macro_mode && update.u.val && widths[0] > 0) {
	if (!xit->arrayInfo()) {
	  resv.u.val = _wrap_width (resv.u.val, widths[0]);
	  resv.is_just_id = 0;
	}
      }
    }
  }
  b = hash_lookup (Hs->state, id->getName());
  Assert (b, "Hmm");
  act_inline_value *iv = (act_inline_value *) b->v;
  iv->clear ();
  delete iv;
  iv = new act_inline_value();
  *iv = resv;
#if 0
  printf ("    ==> final binding is: ");
  iv->Print (stdout);
  printf ("\n");
#endif  
	  
  b->v = iv;
  FREE (widths);
}

static act_inline_value _expand_inline (act_inline_table *Hs, Expr *e, int recurse)
{
  Expr *tmp, *tmp2;
  Expr *ret;
  act_inline_value retv;
  act_inline_value lv, rv;
    
  if (!e) return retv;

  NEW (ret, Expr);
  ret->type = e->type;
  ret->u.e.l = NULL;
  ret->u.e.r = NULL;
  
  switch (e->type) {
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
  case E_NE:
  case E_EQ:
    lv = _expand_inline (Hs, e->u.e.l, recurse);
    rv = _expand_inline (Hs, e->u.e.r, recurse);
    Assert (lv.isSimple() && rv.isSimple(), "What?");
    ret->u.e.l = lv.getVal();
    ret->u.e.r = rv.getVal();
    retv.u.val = ret;
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    lv = _expand_inline (Hs, e->u.e.l, recurse);
    Assert (lv.isSimple(), "What?");
    ret->u.e.l = lv.getVal ();
    if (e->type == E_BUILTIN_INT) {
      ret->u.e.r = e->u.e.r;
    }
    retv.u.val = ret;
    break;

  case E_BITFIELD:
    /* we can work with bitfield so long as it is a basic varaible
       only */
    {
      lv = _lookup_binding (Hs, ((ActId *)e->u.e.l)->getName(),
			    ((ActId *)e->u.e.l)->arrayInfo(),
			    ((ActId *)e->u.e.l)->Rest());
      Assert (lv.isSimple(), "What?");
      Expr *r = lv.getVal();
      if (r->type != E_VAR) {
	/* use shifts to implement bitfields */
	if (e->u.e.r->u.e.l) {
	  Assert (e->u.e.r->u.e.l->type == E_INT &&
		  e->u.e.r->u.e.r->type == E_INT, "WHat?");

	  ret->type = E_LSR;
	  ret->u.e.l = Hs->allow_dag ? r : expr_dup (r);
	  ret->u.e.r = e->u.e.r->u.e.l;
	  ret = _wrap_width (ret, e->u.e.r->u.e.r->u.ival.v - e->u.e.r->u.e.l->u.ival.v + 1);
	}
	else {
	  ret->type = E_LSR;
	  ret->u.e.l = Hs->allow_dag ? r : expr_dup (r);
	  ret->u.e.r = e->u.e.r->u.e.r;
	  ret = _wrap_width (ret, 1);
	}
#if 0
	if (Hs->sc->getUserDef()) {
	  fprintf (stderr, "While inlining: `%s'\n",
		   Hs->sc->getUserDef()->getName());
	  print_expr (stderr, r);
	  fprintf (stderr, "\n");
	}
	fatal_error ("Can't inline bitfields of expressions");
#endif
      }
      else {
	ret->u.e.l = r->u.e.l;
	NEW (ret->u.e.r, Expr);
	ret->u.e.r->type = e->u.e.r->type;
	ret->u.e.r->u.e.l = e->u.e.r->u.e.l;
	ret->u.e.r->u.e.r = e->u.e.r->u.e.r;
      }
    }
    retv.u.val = ret;
    break;

  case E_QUERY:
    lv = _expand_inline (Hs, e->u.e.l, recurse);
    Assert (lv.isSimple(), "Hmm");
    ret->u.e.l = lv.getVal ();
    NEW (ret->u.e.r, Expr);
    ret->u.e.r->type = E_COLON;
    lv = _expand_inline (Hs, e->u.e.r->u.e.l, recurse);
    rv = _expand_inline (Hs, e->u.e.r->u.e.r, recurse);
    Assert (lv.isSimple() && rv.isSimple(), "Hmm");
    ret->u.e.r->u.e.l = lv.getVal ();
    ret->u.e.r->u.e.r = rv.getVal ();
    retv.u.val = ret;
    break;

  case E_CONCAT:
    tmp = e;
    tmp2 = ret;
    do {
      lv = _expand_inline (Hs, tmp->u.e.l, recurse);
      Assert (lv.isSimple(), "Hmm");
      tmp2->u.e.l = lv.getVal();
      tmp = tmp->u.e.r;
      if (tmp) {
	NEW (tmp2->u.e.r, Expr);
	tmp2 = tmp2->u.e.r;
	tmp2->type = E_CONCAT;
	tmp2->u.e.l = NULL;
	tmp2->u.e.r = NULL;
      }
    } while (tmp);
    retv.u.val = ret;
    break;

  case E_FUNCTION:

    if (recurse) {
      int args;
      act_inline_value *arglist;
      
      tmp = e;
      tmp = tmp->u.fn.r;
      /* fix arguments */
      args = 0;
      while (tmp) {
	args++;
	tmp = tmp->u.e.r;
      }

      if (args > 0) {
	MALLOC (arglist, act_inline_value, args);
	tmp = e->u.fn.r;
	args = 0;
	while (tmp) {
	  // a structure argument will be an array or a simple ID!
	  arglist[args++] = _expand_inline (Hs, tmp->u.e.l, recurse);
	  tmp = tmp->u.e.r;
	}
      }
      else {
	arglist = NULL;
      }

      /*-- now simplify! --*/
      UserDef *ux = (UserDef *) e->u.fn.s;
      Assert (ux, "Hmm.");
      Function *fx = dynamic_cast<Function *> (ux);
      Assert (fx, "Hmm");

      Assert (!fx->isExternal(), "Why are we here?");
      Assert (fx->isSimpleInline(), "Why are we here?");

      retv = fx->toInline (args, arglist);
      Assert (retv.isValid(), "What?!");

      if (args > 0) {
	FREE (arglist);
      }
    }
    else {
      Expr *args;
      
      ret->u.fn.s = e->u.fn.s;
      ret->u.fn.r = NULL;
      tmp = e->u.fn.r;
      args = NULL;
      while (tmp) {
	if (!args) {
	  NEW (ret->u.fn.r, Expr);
	  args = ret->u.fn.r;
	}
	else {
	  NEW (args->u.e.r, Expr);
	  args = args->u.e.r;
	}
	args->u.e.r = NULL;
	lv = _expand_inline (Hs, tmp->u.e.l, recurse);
	Assert (lv.isSimple(), "What?");
	args->u.e.l = lv.getVal();
	tmp = tmp->u.e.r;
      }
      retv.u.val = ret;
    }
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
    FREE (ret);
    retv.u.val = e;
    break;
    
  case E_PROBE:
    fatal_error ("Probes in functions?");
    break;
    
  case E_SELF:
    retv = _lookup_binding (Hs, "self", NULL, NULL);
    break;

  case E_SELF_ACK:
    Assert (0, "selfack in this context?!");
    break;
    
  case E_VAR:
    {
      ActId *tid = (ActId *)e->u.e.l;
      retv = _lookup_binding (Hs, tid->getName(),
			      tid->arrayInfo(), tid->Rest());
    }
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return retv;
}

act_inline_value act_inline_expr (act_inline_table *T, Expr *e, int recurse)
{
  act_inline_value badret;
  if (!e) return badret;

  return _expand_inline (T, e, recurse);
}



act_inline_table *act_inline_merge_tables (int nT, act_inline_table **T,
					   Expr **cond)
{
  Assert (nT > 0, "What?");
  
  struct Hashtable *Hmerge = hash_new (4);

  Scope *sc = T[0]->sc;
  act_inline_table *Tret = T[0]->parent;
  
  hash_iter_t it;
  hash_bucket_t *b;
  Expr *intconst, *boolconst;

  intconst = const_expr (0);
  boolconst = const_expr_bool (0);

  for (int i=0; i < nT; i++)  {
    hash_iter_init (T[i]->state, &it);
    while ((b = hash_iter_next (T[i]->state, &it))) {
      if (!hash_lookup (Hmerge, b->key)) {
	hash_add (Hmerge, b->key);
      }
    }
  }
      
  /*-- Hmerge contains all the modified state --*/
  hash_iter_init (Hmerge, &it);
  while ((b = hash_iter_next (Hmerge, &it))) {
    hash_bucket_t *xb;
    /* construct expression for b->key! */
    unsigned int sz = 1;
    int *types;
    Data *d;
    act_inline_value vl;

    /* vl is the value before the merge, in the parent context */
    vl = _lookup_binding (Tret, b->key, NULL, NULL, 0);

    InstType *et = sc->Lookup (b->key);
    Assert (et, "What?");

    ActId **fields;
    fields = NULL;

    d = NULL;
    if (TypeFactory::isIntType (et)) {
      MALLOC (types, int, 1);
      types[0] = 1;
    }
    else if (TypeFactory::isBoolType (et)) {
      MALLOC (types, int, 1);
      types[0] = 0;
    }
    else {
      int nb, ni;
      Assert (TypeFactory::isStructure (et), "What?");
      d = dynamic_cast <Data *> (et->BaseType());
      d->getStructCount (&nb, &ni);
      sz = nb + ni;
      fields = d->getStructFields (&types);
    }

    Expr *update;
    Expr **newval;

    MALLOC (newval, Expr *, sz);

    for (int idx=0; idx < sz; idx++) {
      /* for each of the fields... */
      update = NULL;
      for (int i=0; i < nT; i++) {
	xb = hash_lookup (T[i]->state, b->key);
	if (!update || cond[i]) {
	  if (!update) {
	    Assert (cond[i], "weird else clause?!");
	    NEW (update, Expr);
	    newval[idx] = update;
	  }
	  else {
	    NEW (update->u.e.r, Expr);
	    update = update->u.e.r;
	  }
	  update->type = E_QUERY;
	  if (idx == 0) {
	    update->u.e.l = cond[i];
	  }
	  else {
	    update->u.e.l =
	      T[i]->allow_dag ? cond[i] : expr_dup (cond[i]);
	  }
	  update->u.e.r = NULL;
	}
	else {
	  /* 
	     update && !guards[i] : else clause, nothing to do here,
	     just update update->u.e.r 
	  */
	  Assert (i == nT-1, "else is not the last case?");
	}
	Expr *curval = NULL;
	act_inline_value *ival;
	if (xb) {
	  ival = (act_inline_value *) xb->v;
	}
	else {
	  ival = &vl;
	}
	if (ival) {
	  if (ival->isValid() && (d && !ival->isSimple())) {
	    curval = ival->u.arr[idx];
	  }
	  else if (ival->isValid() && (!d && idx == 0)) {
	    Assert (ival->isSimple(), "Hmm");
	    curval = ival->getVal();
	  }
	  else {
	    if (types[idx] == 0) {
	      curval = boolconst;
	    }
	    else {
	      curval = intconst;
	    }
	  }
	}
	if (curval == NULL) {
	  act_error_ctxt (stderr);
	  if (fields) {
	    fprintf (stderr, "Field: ");
	    fields[idx]->Print (stderr);
	    fprintf (stderr, "\n");
	  }
	  fatal_error ("Updating binding for `%s' (field #%d): uninitialized field.",
			 b->key, idx);
	}
	
	if (i == nT-1 && !cond[i]) {
	  update->u.e.r = curval;
	}
	else {
	  NEW (update->u.e.r, Expr);
	  update = update->u.e.r;
	  update->type = E_COLON;
	  update->u.e.l = curval;
	  update->u.e.r = NULL;

	  if (i == nT-1) {
	    if (vl.isValid() && !vl.isSimple() && vl.u.arr[idx]) {
	      update->u.e.r = T[i]->allow_dag ? vl.u.arr[idx] : expr_dup (vl.u.arr[idx]);
	    }
	    else if (types[idx] == 0) {
	      update->u.e.r = boolconst;
	    }
	    else {
	      update->u.e.r = intconst;
	    }
	  }
	}
      }
    }

    if (fields) {
      for (int i=0; i < sz; i++) {
	delete fields[i];
      }
      FREE (fields);
      fields = NULL;
    }

    act_inline_value nv;
    if (d) {
      nv.is_struct = 1;
      nv.struct_count = sz;
      nv.u.arr = newval;
    }
    else {
      nv.is_struct = 0;
      nv.u.val = newval[0];
      FREE (newval);
    }
    
    FREE (types);
    ActId *tmpid = new ActId (b->key);
    _update_binding (Tret, tmpid, nv);
    delete tmpid;
  }
  hash_free (Hmerge);

  return Tret;
}


void act_inline_setval (act_inline_table *Hs, ActId *id,
			act_inline_value update)
{
  _update_binding (Hs, id, update);
}

act_inline_value act_inline_getval (act_inline_table *Hs, const char *s)
{
  return _lookup_binding (Hs, s, NULL, NULL);
}


void act_inline_value::Print (FILE *fp)
{
  if (!isValid()) {
    fprintf (fp, "@invalid@");
    return;
  }
  if (is_array && !is_just_id) {
    fprintf (fp, "@arr[%d]", array_sz);
  }
  fprintf (fp, "@%s", is_struct ? "struct" : "elem");
  if (is_struct) {
    if (is_just_id) {
      fprintf (fp, " id: ");
      print_uexpr (fp, u.val);
    }
    else {
      fprintf (fp, "%d ", struct_count);
      for (int j=0; j < (array_sz == 0 ? 1 : array_sz); j++) {
	if (j != 0) {
	  fprintf(fp, " | ");
	}
	for (int i=0; i < struct_count; i++) {
	  if (i != 0) {
	    fprintf (fp, " ; ");
	  }
	  if (u.arr[i+j*struct_count]) {
	    print_uexpr (fp, u.arr[i+j*struct_count]);
	  }
	  else {
	    fprintf (fp, "nul");
	  }
	}
      }
    }
  }
  else {
    fprintf (fp, " ");
    if (is_array && !is_just_id) {
      for (int i=0; i < array_sz; i++) {
	if (i != 0) {
	  fprintf (fp, " | ");
	}
	if (u.arr[i]) {
	  print_uexpr (fp, u.arr[i]);
	}
	else {
	  fprintf (fp, "nul");
	}
      }
    }
    else {
      print_uexpr (fp, u.val);
    }
  }
  fprintf (fp, "@");
}


void act_inline_value::elaborateStructId (Data *d, Array *a)
{
  Assert (isSimple(), "hmm");
  Assert (is_struct, "What?");
  
  ActId **fields;
  ActId *baseid;
  int ni, nb;
  int sz;

  if (d) {
    int *types;
    d->getStructCount (&nb, &ni);
    sz = ni + nb;
    fields = d->getStructFields (&types);
    FREE (types);
  }
  else {
    sz = 1;
    fields = NULL;
  }
  Assert (getVal()->type == E_VAR, "Hmm");
  baseid = (ActId *)getVal()->u.e.l;

  if (a) {
    is_array = 1;
    array_sz = a->size();
    MALLOC (u.arr, Expr *, sz*array_sz);
  }
  else {
    is_array = 0;
    array_sz = 0;
    MALLOC (u.arr, Expr *, sz);
  }
  struct_count = sz;
  is_just_id = 0;

  for (int arr=0; arr < (array_sz == 0 ? 1 : array_sz); arr++) {
    Array *newa;

    if (a) {
      newa = a->unOffset (arr);
    }
    else {
      newa = NULL;
    }

    for (int i=0; i < sz; i++) {
      ActId *varnew;
      NEW (u.arr[arr*sz + i], Expr);
      u.arr[arr*sz + i]->type = E_VAR;
      u.arr[arr*sz + i]->u.e.r = NULL;
      varnew = baseid->Clone();
      if (fields) {
	varnew->Append (fields[i]);
      }
      varnew->setArray (newa);
      u.arr[arr*sz + i]->u.e.l = (Expr *) varnew;
    }
  }
  if (fields) {
    FREE (fields);
  }
  Assert (!isSimple(), "Hmm{");
  Assert (isValid(), "Check");
}
