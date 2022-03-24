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
  Scope *sc;
  struct Hashtable *state;
  act_inline_table *parent;
};


act_inline_table *act_inline_new (Scope *sc, act_inline_table *parent)
{
  act_inline_table *ret;
  NEW (ret, act_inline_table);
  ret->ex_func = 0;
  ret->sc = sc;
  ret->parent = parent;

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
      FREE (b->v);
    }
  }
  hash_free (T->state);
  FREE (T);
}
		      

static Expr *_ex_one (Expr **x)
{
  Expr *e = x[0];
  FREE (x);
  return e;
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

static Expr **_lookup_binding (act_inline_table *Hs,
			       const char *name, ActId *rest, int err = 1)
{
  hash_bucket_t *b;
  while (Hs) {
    b = hash_lookup (Hs->state, name);
    if (b) {
      int ni, nb;
      int sz = 1;
      InstType *it = Hs->sc->Lookup (name);
      Data *d = NULL;
      if (TypeFactory::isStructure (it)) {
	d = dynamic_cast<Data *>(it->BaseType());
	d->getStructCount (&nb, &ni);
	sz = nb + ni;
      }
      /* return a copy */
      Expr **ret;
      MALLOC (ret, Expr *, sz);
      for (int i=0; i < sz; i++) {
	ret[i] = ((Expr **)b->v)[i];
      }
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
	Expr **newret;
	MALLOC (newret, Expr *, sz2);
	for (int i=0; i < sz2; i++) {
	  newret[i] = ret[i+off];
	  if (err && (newret[i] == NULL)) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Access to: `%s.", name);
	    rest->Print (stderr);
	    fprintf (stderr, "' has a NULL binding.\n");
	    fatal_error ("Uninitialized fields for `%s'.", name);
	  }
	}
	FREE (ret);
	ret = newret;
      }
      else {
	if (err) {
	  for (int i=0; i < sz; i++) {
	    if (ret[i] == NULL) {
	      act_error_ctxt (stderr);
	      fatal_error ("Found NULL binding for `%s': certain fields are undefined.", name);
	    }
	  }
	}
      }
      return ret;
    }
    Hs = Hs->parent;
  }
  if (err) {
    act_error_ctxt (stderr);
    fatal_error ("Inlining failed! Variable `%s' used before being defined",
		 name);
  }
  return NULL;
}

static void _update_binding (act_inline_table *Hs, ActId *id, Expr **update)
{
  InstType *xit = Hs->sc->Lookup (id->getName());
  int sz = 1;
  Data *xd;

#if 0
  printf ("update binding for: ");
  id->Print (stdout);
  printf ("\n");

  printf ("entry 0 is %p: ", update[0]);
  fflush (stdout);
  if (update[0]) {
    print_expr (stdout, update[0]);
  }
  else {
    printf ("-null-");
  }
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
  }

  hash_bucket_t *b;
  
  /* find partial or total update, and update entry in the hash table! 
     tmp is the FULL structure binding.
     if id->Rest() then we need to do a partial assignment.
   */
  b = hash_lookup (Hs->state, id->getName());
  if (!b) {
    Expr **bind;
    Expr **xtmp = _lookup_binding (Hs, id->getName(), NULL, 0);

    if (xtmp) {
      bind = xtmp;
    }
    else {
      MALLOC (bind, Expr *, sz);
      for (int i=0; i < sz; i++) {
	bind[i] = NULL;
      }
    }
    b = hash_add (Hs->state, id->getName());
    b->v = bind;
  }
  Expr **res = _lookup_binding (Hs, id->getName(), NULL, 0);

  if (id->Rest()) {
    Assert (xd, "What?!");
    int sz2;
    int off = xd->getStructOffset (id->Rest(), &sz2);
    Assert (off >= 0 && off < sz, "What?");
    Assert (off + sz2 <= sz, "What?");
    for (int i=0; i < sz2; i++) {
      res[off+i] = update[i];
#if 0      
      printf ("set offset %d to ", off+i);
      if (update[i]) {
	print_expr (stdout, update[i]);
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
	print_expr (stdout, res[i]);
      }
      else {
	printf ("null");
      }
      printf ("\n");
    }
#endif	
  }
  else {
    for (int i=0; i < sz; i++) {
      res[i] = update[i];
    }
  }
  b = hash_lookup (Hs->state, id->getName());
  Assert (b, "Hmm");
  Expr **xtmp = (Expr **)b->v;
  b->v = res;
#if 0  
  for (int i=0; i < sz; i++) {
    if (xtmp[i]) {
      expr_ex_free (xtmp[i]);
    }
  }
#endif  
  FREE (xtmp);
}

static Expr **_expand_inline (act_inline_table *Hs, Expr *e, int recurse)
{
  Expr *tmp, *tmp2;
  Expr *ret;
  Expr **rets;
    
  if (!e) return NULL;

  rets = NULL;
  
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
    ret->u.e.l = _ex_one (_expand_inline (Hs, e->u.e.l, recurse));
    ret->u.e.r = _ex_one (_expand_inline (Hs, e->u.e.r, recurse));
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
  case E_BUILTIN_INT:
  case E_BUILTIN_BOOL:
    ret->u.e.l = _ex_one (_expand_inline (Hs, e->u.e.l, recurse));
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_BITFIELD:
    /* we can work with bitfield so long as it is a basic varaible
       only */
    {
      Expr **res = _lookup_binding (Hs, ((ActId *)e->u.e.l)->getName(),
				    ((ActId *)e->u.e.l)->Rest());
      Expr *r = res[0];
      FREE (res);
      if (r->type != E_VAR) {
	/* use shifts to implement bitfields */
	if (e->u.e.r->u.e.l) {
	  unsigned long mask_amt;
	  Assert (e->u.e.r->u.e.l->type == E_INT &&
		  e->u.e.r->u.e.r->type == E_INT, "WHat?");

	  /* (r >> (e->u.e.r->u.e.l)) & ((1 << (r - l + 1)) - 1) */

	  /* XXX bitwidth */
	  if (e->u.e.r->u.e.r->u.v - e->u.e.r->u.e.l->u.v > 64) {
	    warning ("Bitwidth limit exceeded?");
	  }
	  else if (e->u.e.r->u.e.r->u.v - e->u.e.r->u.e.l->u.v == 64) {
	    mask_amt = 0xffffffffffffffff;
	  }
	  else {
	    mask_amt = (1UL << (e->u.e.r->u.e.r->u.v - e->u.e.r->u.e.l->u.v + 1)) - 1;
	  }
	  ret->type = E_AND;
	  ret->u.e.r = const_expr (mask_amt);
	  NEW (ret->u.e.l, Expr);
	  ret->u.e.l->type = E_LSR;
	  ret->u.e.l->u.e.l = expr_dup (r);
	  ret->u.e.l->u.e.r = e->u.e.r->u.e.l;
	}
	else {
	  ret->type = E_AND;
	  ret->u.e.r = const_expr (1);
	  NEW (ret->u.e.l, Expr);
	  ret->u.e.l->type = E_LSR;
	  ret->u.e.l->u.e.l = expr_dup (r);
	  ret->u.e.l->u.e.r = e->u.e.r->u.e.r;
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
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_QUERY:
    ret->u.e.l = _ex_one (_expand_inline (Hs, e->u.e.l, recurse));
    ret->u.e.r->u.e.l = _ex_one (_expand_inline (Hs, e->u.e.r->u.e.l, recurse));
    ret->u.e.r->u.e.r = _ex_one (_expand_inline (Hs, e->u.e.r->u.e.r, recurse));
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_CONCAT:
    tmp = e;
    tmp2 = ret;
    do {
      tmp2->u.e.l = _ex_one (_expand_inline (Hs, tmp->u.e.l, recurse));
      tmp = tmp->u.e.r;
      if (tmp) {
	NEW (tmp2->u.e.r, Expr);
	tmp2 = tmp2->u.e.r;
	tmp2->type = E_COMMA;
	tmp2->u.e.l = NULL;
	tmp2->u.e.r = NULL;
      }
    } while (tmp);

    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;

  case E_FUNCTION:

    if (recurse) {
      int args;
      Expr **arglist;
      
      tmp = e;
      tmp = tmp->u.fn.r;
      /* fix arguments */
      args = 0;
      while (tmp) {
	args++;
	tmp = tmp->u.e.r;
      }

      if (args > 0) {
	MALLOC (arglist, Expr *, args);
	tmp = e->u.fn.r;
	args = 0;
	while (tmp) {
	  arglist[args++] = _ex_one (_expand_inline (Hs, tmp->u.e.l, recurse));
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

      rets = fx->toInline (args, arglist);
      Assert (rets, "What?!");
      
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
	args->u.e.l = _ex_one (_expand_inline (Hs, tmp->u.e.l, recurse));
	tmp = tmp->u.e.r;
      }
      MALLOC (rets, Expr *, 1);
      rets[0] = ret;
    }
    break;
    
  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
    FREE (ret);
    ret = e;
    MALLOC (rets, Expr *, 1);
    rets[0] = ret;
    break;
    
  case E_PROBE:
    fatal_error ("Probes in functions?");
    break;
    
  case E_SELF:
    rets = _lookup_binding (Hs, "self", NULL);
    break;
    
  case E_VAR:
    {
      ActId *tid = (ActId *)e->u.e.l;
      rets = _lookup_binding (Hs, tid->getName(), tid->Rest());
    }
    break;

  default:
    fatal_error ("Unknown expression type (%d)\n", e->type);
    break;
  }
  return rets;
}


Expr *act_inline_expr_simple (act_inline_table *T, Expr *e, int recurse)
{
  if (!e) return NULL;
  return _ex_one (_expand_inline (T, e, recurse));
}

Expr **act_inline_expr (act_inline_table *T, Expr *e, int recurse)
{
  if (!e) return NULL;
  return _expand_inline (T, e, recurse);
}



act_inline_table *act_inline_merge_tables (int nT, act_inline_table **T, Expr **cond)
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
    Expr **inpval;
    int *types;

    inpval = _lookup_binding (Tret, b->key, NULL, 0);

    InstType *et = sc->Lookup (b->key);
    Assert (et, "What?");
    if (TypeFactory::isIntType (et)) {
      MALLOC (types, int, 1);
      types[0] = 1;
    }
    else if (TypeFactory::isBoolType (et)) {
      MALLOC (types, int, 1);
      types[0] = 0;
    }
    else {
      Assert (TypeFactory::isStructure (et), "What?");
      Data *d = dynamic_cast <Data *> (et->BaseType());
      int nb, ni;
      ActId **fields;
      d->getStructCount (&nb, &ni);
      sz = nb + ni;
      fields = d->getStructFields (&types);
      for (int i=0; i < sz; i++) {
	delete fields[i];
      }
      FREE (fields);
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
	    update->u.e.l = expr_expand (cond[i], NULL, NULL,
					 ACT_EXPR_EXFLAG_DUPONLY);
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
	Expr *curval;
	if (xb) {
	  Expr **val = (Expr **) xb->v;
	  curval = val[idx];
	}
	else {
	  if (inpval && inpval[idx]) {
	    curval = inpval[idx];
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
	    if (inpval && inpval[idx]) {
	      update->u.e.r = expr_expand (inpval[idx], NULL, NULL,
					   ACT_EXPR_EXFLAG_DUPONLY);
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
    FREE (types);
    ActId *tmpid = new ActId (b->key);
    _update_binding (Tret, tmpid, newval);
    delete tmpid;
  }
  hash_free (Hmerge);

  return Tret;
}


void act_inline_setval (act_inline_table *Hs, ActId *id, Expr **update)
{
  _update_binding (Hs, id, update);
}

Expr **act_inline_getval (act_inline_table *Hs, const char *s)
{
  Expr **rets = _lookup_binding (Hs, s, NULL);
  return rets;
}
