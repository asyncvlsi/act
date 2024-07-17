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
#include <act/inline.h>
#include <string.h>
#include <common/misc.h>
#include <common/hash.h>
#include <common/qops.h>
#include <common/int.h>

/*------------------------------------------------------------------------
 *
 *
 *   Macros
 *
 *
 *------------------------------------------------------------------------
 */
UserMacro::UserMacro (UserDef *u, const char *name)
{
  _nm = string_cache (name);
  nports = 0;
  port_t = NULL;
  port_n = NULL;
  c = NULL;
  rettype = NULL;
  _exf = NULL;
  _builtinmacro = false;
  parent = u;
}


UserMacro::~UserMacro ()
{
  if (port_t) {
    for (int i=0; i < nports; i++) {
      if (port_t[i]) {
	delete port_t[i];
      }
    }
    FREE (port_t);
  }
  if (port_n) {
    FREE (port_n);
  }
  if (_exf) {
    delete _exf;
  }
  act_chp_free (c);
}


void UserMacro::Print (FILE *fp)
{
  fprintf (fp, "  macro %s (", _nm);
  for (int i=0; i < nports; i++) {
    port_t[i]->Print (fp);
    fprintf (fp, " %s", port_n[i]);
    if (i != nports-1) {
      fprintf (fp, "; ");
    }
  }
  fprintf (fp, ")");
  if (rettype) {
    fprintf (fp, " : ");
    rettype->Print (fp);
  }
  fprintf (fp, " {\n");
  if (c) {
    fprintf (fp, "   ");
    chp_print (fp, c);
    fprintf (fp, "\n");
  }
  fprintf (fp, "  }\n");
}


int UserMacro::addPort (InstType *t, const char *id)
{
  int i;
  
  for (i=0; i < nports; i++) {
    if (strcmp (port_n[i], id) == 0) {
      return 0;
    }
  }

  nports++;

  if (!port_n) {
    NEW (port_n, const  char *);
  }
  else {
    REALLOC (port_n, const char *, nports);
  }
  if (!port_t) {
    NEW (port_t, InstType *);
  }
  else {
    REALLOC (port_t, InstType *, nports);
  }

  port_n[nports-1] = string_cache (id);
  port_t[nports-1] = t;

  return 1;
}

UserMacro *UserMacro::Expand (UserDef *ux, ActNamespace *ns, Scope *s, int is_proc)
{
  UserMacro *ret = new UserMacro (ux, _nm);

  ret->nports = nports;
  if (nports > 0) {
    MALLOC (ret->port_t, InstType *, nports);
    MALLOC (ret->port_n, const char *, nports);
  }
  ret->_builtinmacro = _builtinmacro;

  Scope *tsc = new Scope (s, 1);
  
  for (int i=0; i < nports; i++) {
    ret->port_t[i] = port_t[i]->Expand (ns, s);
    ret->port_n[i] = string_cache (port_n[i]);

    tsc->Add (ret->port_n[i], ret->port_t[i]);
  }

  for (int i=0; i < ux->getNumPorts(); i++) {
    tsc->Add (ux->getPortName (i), ux->getPortType (i));
  }

  if (rettype) {
    if (isBuiltinMacro()) {
      InstType *it;
      if (strcmp (getName(), "int") == 0) {
	ux->Print (stdout);
	it = new InstType (s, TypeFactory::Factory()->NewInt (0,TypeFactory::totBitWidthSpecial (ux)), 0);
	it->setNumParams (1);
	it->setParam (0, const_expr (TypeFactory::totBitWidthSpecial (ux)));
	it = it->Expand (ns, s);
      }
      else {
	it = new InstType (s, ux, 0);
	it->mkExpanded();
	it->MkCached();
      }
      ret->rettype = it;
    }
    else {
      ret->rettype = rettype->Expand (ns, s);
    }
    tsc->Add ("self", ret->rettype);
  }

  if (is_proc) {
    chp_expand_macromode (2);
  }
  else {
    chp_expand_macromode (1);
  }
  ret->c = chp_expand (c, ns, tsc);
  chp_expand_macromode (0);

  if (ret->rettype) {
    /*--- we need to create a dummy function ---*/
    InstType *it;
    UserDef *u = new UserDef (ux->getns());
    int len;

    Assert (ux->isExpanded(), "What?");

    if (!isBuiltinMacro() || strcmp (getName(), "int") == 0) {
      it = new InstType (s, ux, 0);
      it->mkExpanded ();
    }
    else {
      it = new InstType (s, TypeFactory::Factory()->NewInt (0,
							    TypeFactory::totBitWidthSpecial (ux)));
      it->setNumParams (1);
      it->setParam (0, const_expr (TypeFactory::totBitWidthSpecial (ux)));
      it = it->Expand (ns, s);
    }
    
    /* add the ports to this function! */

    Assert (u->AddPort (it, string_cache ("$internal")) == 1,
	    "What?");
    
    for (int i=0; i < nports; i++) {
      Assert (u->AddPort (port_t[i] , port_n[i]) == 1, 
	      "Not sure what went wrong!");
    }

    len = strlen (ux->getName()) + 2 + strlen (_nm);
    char *buf;
    MALLOC (buf, char, len);
    snprintf (buf, len, "%s/%s", ux->getName(), _nm);
    
    Function *tmpf = new Function (u);
    ux->getns()->CreateType (buf, tmpf);
    FREE (buf);

    tmpf->setRetType (ret->rettype);
    
    ret->_exf = tmpf->Expand (ux->getns(), ux->CurScope(), 0, NULL);

    // now we set the CHP body!
    // substitute with a special ID
    ActId *prefix = new ActId (string_cache ("$internal"));
    act_languages *all_lang = ret->_exf->getlang();
    act_chp *xchp;
    NEW (xchp, act_chp);
    xchp->vdd = NULL;
    xchp->gnd = NULL;
    xchp->psc = NULL;
    xchp->nsc = NULL;
    xchp->is_synthesizable = 1;
    xchp->c = NULL;

    if (isBuiltinMacro()) {
      // nothing to do here... put this code in populateCHP()
    }
    else {
      act_inline_table *tab = act_inline_new (tsc, NULL);

      Expr **tmp_exprs;
      MALLOC (tmp_exprs, Expr *, nports+1);
      for (int i=0; i < nports; i++) {
	ActId *tid;
	NEW (tmp_exprs[i], Expr);
	tmp_exprs[i]->type = E_VAR;
	tid = new ActId (port_n[i]);
	tmp_exprs[i]->u.e.l = (Expr *) tid;
	tmp_exprs[i]->u.e.r = NULL;
	act_inline_setval (tab, tid, tmp_exprs+i);
      }
      NEW (tmp_exprs[nports], Expr);
      tmp_exprs[nports]->type = E_VAR;
      tmp_exprs[nports]->u.e.l = (Expr *) new ActId (string_cache ("self"));
      tmp_exprs[nports]->u.e.r = NULL;
      act_inline_setval (tab, (ActId *)tmp_exprs[nports]->u.e.l, tmp_exprs + nports);

      FREE (tmp_exprs);
    
      xchp->c = ret->substitute (prefix, tab);
      act_inline_free (tab);
    }
    
    all_lang->setchp (xchp);
    ret->_exf->chkInline ();
  }
  delete tsc;
  
  return ret;
}


void UserMacro::setBody (struct act_chp_lang *chp)
{
  c = chp;
}

void UserMacro::setRetType (InstType *it)
{
  rettype = it;
}


static ActId *_chp_id_subst (ActId *id, act_inline_table *tab, ActId *inp)
{
  if (act_inline_isbound (tab, inp->getName())) {
    Expr **e  = act_inline_getval (tab, inp->getName());
    Expr *tmp = e[0];
    FREE (e);
    if (tmp->type == E_VAR) {
      return ((ActId *)tmp->u.e.l)->Clone();
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("Expanding macro: `%s' substituted by expression in a context where an identifier is needed.", inp->getName());
      exit (1);
    }
  }
  else {
    ActId *ret = id->Clone ();
    ret->Tail()->Append (inp->Clone());
    return ret;
  }
}

static void _replace_ids (ActId *id, act_inline_table *tab, Expr *e)
{
  if (!e) return;
  switch (e->type) {
  case E_INT:
  case E_TRUE:
  case E_FALSE:
  case E_REAL:
    break;

  case E_FUNCTION:
    _replace_ids (id, tab, e->u.fn.r);
    break;

  case E_VAR:
    if (e->u.e.l) {
      ActId *tmp = (ActId *)e->u.e.l;
      if (act_inline_isbound (tab, tmp->getName())) {
	Expr **res = act_inline_getval (tab, tmp->getName());
	*e = *(res[0]);
	if (e->type == E_INT && e->u.ival.v_extra) {
	  e->u.ival.v_extra = (Expr *) new BigInt (*((BigInt *)e->u.ival.v_extra));
	}
	FREE (res);
      }
      else {
	ActId *ret = id->Clone ();
	ret->Tail()->Append (tmp->Clone());
	e->u.e.l = (Expr *)ret;
      }
      delete tmp;
    }
    break;
    
  case E_PROBE:
  case E_BITFIELD:
    if (e->u.e.l) {
      ActId *tmp = (ActId *)e->u.e.l;
      e->u.e.l = (Expr *) _chp_id_subst (id, tab, tmp);
      delete tmp;
    }
    break;

  default:
    if (e->u.e.l) _replace_ids (id, tab, e->u.e.l);
    if (e->u.e.r) _replace_ids (id, tab, e->u.e.r);
    break;
  }
}

static Expr *_expr_subst_helper (ActId *id, act_inline_table *tab, Expr *e)
{
  Expr *ret = expr_dup (e);
  _replace_ids (id, tab, ret);
  return ret;
}


static act_chp_lang_t *_chp_subst_helper (ActId *id, act_inline_table *tab,
					  act_chp_lang_t *c)
{
  act_chp_gc_t *gchd, *gctl, *gctmp, *tmp;
  act_chp_lang_t *ret;
  listitem_t *li;
  
  if (!c) return NULL;

  NEW (ret, act_chp_lang_t);
  ret->type = c->type;
  ret->label = c->label;

  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    ret->u.semi_comma.cmd = list_new ();
    for (li = list_first (c->u.semi_comma.cmd); li;
	 li = list_next (li)) {
      list_append (ret->u.semi_comma.cmd,
		   _chp_subst_helper (id, tab,
				      (act_chp_lang_t *)list_value (li)));
    }
    break;

  case ACT_CHP_COMMALOOP:
  case ACT_CHP_SEMILOOP:
    warning ("Semi/comma loops not supported in macros");
    ret->type = ACT_CHP_SKIP;
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    gchd = NULL;
    gctl = NULL;
    for (gctmp = c->u.gc; gctmp; gctmp = gctmp->next) {
      if (gctmp->id) {
	warning ("Guard loops not supported in macros");
      }
      NEW (tmp, act_chp_gc_t);
      tmp->id = NULL;
      tmp->next = NULL;
      tmp->g = _expr_subst_helper (id, tab, gctmp->g);
      if (tmp->g && expr_is_a_const (tmp->g) && tmp->g->type == E_FALSE &&
	  c->type != ACT_CHP_DOLOOP) {
	FREE (tmp);
      }
      else {
	tmp->s = _chp_subst_helper (id, tab, gctmp->s);
	q_ins (gchd, gctl, tmp);
      }
    }
    if (!gchd) {
      /* ok this is false -> skip */
      NEW (tmp, act_chp_gc_t);
      tmp->id = NULL;
      tmp->next = NULL;
      
      tmp->g = const_expr_bool (0);
      NEW (tmp->s, act_chp_lang);
      tmp->s->type = ACT_CHP_SKIP;
      q_ins (gchd, gctl, tmp);
    }
    if (c->type != ACT_CHP_DOLOOP) {
      if (gchd->next == NULL && (!gchd->g || expr_is_a_const (gchd->g))) {
	/* loops and selections that simplify to a single guard that
	   is constant */
	if (c->type == ACT_CHP_LOOP) {
	  if (gchd->g && gchd->g->type == E_FALSE) {
	    /* whole thing is a skip */
	    /* XXX: need chp_free */
	    ret->u.gc = gchd;
	    act_chp_free (ret);
	    NEW (ret, act_chp_lang);
	    ret->type = ACT_CHP_SKIP;
	    return ret;
	  }
	}
	else {
	  if (!gchd->g || gchd->g->type == E_TRUE) {
	    /* whole thing is the body */
	    
	    FREE (ret);
	    ret = gchd->s;
	    FREE (gchd);
	    if (!ret) {
	      NEW (ret, act_chp_lang);
	      ret->type = ACT_CHP_SKIP;
	    }
	    return ret;
	  }
	}
      }
    }
    ret->u.gc = gchd;
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
  case ACT_CHP_ASSIGNSELF:
    ret->u.assign.id = _chp_id_subst (id, tab, c->u.assign.id);
    ret->u.assign.e = _expr_subst_helper (id, tab, c->u.assign.e);
    break;
    
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    ret->u.comm.chan = _chp_id_subst (id, tab, c->u.comm.chan);
    ret->u.comm.flavor = c->u.comm.flavor;
    if (c->u.comm.var) {
      ret->u.comm.var = _chp_id_subst (id, tab, c->u.comm.var);
    }
    else {
      ret->u.comm.var = NULL;
    }
    if (c->u.comm.e) {
      ret->u.comm.e = _expr_subst_helper (id, tab, c->u.comm.e);
    }
    else {
      ret->u.comm.e = NULL;
    }
    break;
    
  case ACT_CHP_FUNC:
    ret->u.func.name = c->u.func.name;
    ret->u.func.rhs = list_new ();
    for (li = list_first (c->u.func.rhs); li; li = list_next (li)) {
      act_func_arguments_t *arg, *ra;
      NEW (arg, act_func_arguments_t);
      ra = (act_func_arguments_t *) list_value (li);
      arg->isstring = ra->isstring;
      if (ra->isstring) {
	arg->u.s = ra->u.s;
      }
      else {
	arg->u.e = _expr_subst_helper (id, tab, ra->u.e);
      }
      list_append (ret->u.func.rhs, arg);
    }
    break;

  case ACT_CHP_MACRO:
    {
      act_error_ctxt (stderr);
      fatal_error ("Nested macros not permitted");
    }
    break;
    
  default:
    fatal_error ("Unknown chp type %d", ret->type);
    break;
  }
  return ret;
}


act_chp_lang_t *UserMacro::substitute (ActId *id, act_inline_table *tab)
{
  return _chp_subst_helper (id, tab, c);
}


void UserMacro::updateFn (UserDef *u)
{
  InstType *it;
  Assert (_exf, "UserMacro::updateFn() SHould not be called!");

  it = new InstType (_exf->CurScope(), u, 0);

  if (isBuiltinStructMacro()) {
    _exf->CurScope()->refineBaseType ("self", it);
    _exf->setRetType (it);
  }
  else {
    _exf->CurScope()->refineBaseType ("$internal", it);
    _exf->refinePortType (0, it);
  }
  _exf->CurScope()->updateParent (u->CurScope());
}


void UserMacro::populateCHP()
{
  Data *xd;
  act_chp_lang_t *c;

  Assert (isBuiltinMacro(), "Should not be called for non-built-in macros");

  xd = dynamic_cast<Data *> (Parent());
  Assert (xd, "What?");
  
  int nbools, nints;
  xd->getStructCount (&nbools, &nints);

  if (nbools + nints == 0) {
    return;
  }
  
  int *typecodes;
  ActId **xfield = xd->getStructFields (&typecodes);

#if 0  
  for (int i=0; i < nbools + nints; i++) {
    printf ("field: ");
    xfield[i]->Print (stdout);
    printf ("; typecode: %d; type: ", typecodes[i]);
    InstType *it = xd->CurScope()->FullLookup (xfield[i], NULL);
    it->Print (stdout);
    printf ("\n");
  }
#endif

  if (isBuiltinStructMacro()) {
    // create a sequence of assignments to each part of "self"
    int pos = TypeFactory::totBitWidth (xd);
    Assert (pos > 0, "What?");
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SEMI;
    c->space = NULL;
    c->label = NULL;
    c->u.semi_comma.cmd = list_new ();
    for (int i=0; i < nbools + nints; i++) {
      act_chp_lang_t *tmp;
      Expr *e;
      ActId *tid = new ActId (string_cache ("self"));
      tid->Append (xfield[i]);
      NEW (e, Expr);
      e->type = E_BITFIELD;
      e->u.e.l = (Expr *) new ActId (string_cache ("$internal"));
      NEW (e->u.e.r, Expr);
      e->u.e.r->type = E_BITFIELD;
      e->u.e.r->u.e.r = const_expr (pos-1);
      pos -= TypeFactory::bitWidth (xd->CurScope()->FullLookup(xfield[i], NULL));
      e->u.e.r->u.e.l = const_expr (pos);
      
      NEW (tmp, act_chp_lang_t);
      tmp->type = ACT_CHP_ASSIGN;
      tmp->space = NULL;
      tmp->label = NULL;
      tmp->u.assign.e = e;
      tmp->u.assign.id = tid;
      list_append (c->u.semi_comma.cmd, tmp);
    }
  }
  else {
    // create chp body
    //   self := {$internal.one,$internal.two, etc.}
    // if there is a bool type, wrap it in an int() directive.
    Expr *e, *f;
    NEW (e, Expr);
    e->type = E_CONCAT;
    e->u.e.l =  NULL;
    e->u.e.r = NULL;
    f = e;
    for (int i=0; i < nbools + nints; i++) {
      ActId *tid = new ActId (string_cache ("$internal"));
      tid->Append (xfield[i]);
      NEW (f->u.e.l, Expr);
      f->u.e.l->type = E_VAR;
      f->u.e.l->u.e.l = (Expr *) tid;
      f->u.e.l->u.e.r = NULL;
      if (i != (nbools + nints - 1)) {
	NEW (f->u.e.r, Expr);
	f = f->u.e.r;
	f->u.e.l = NULL;
	f->u.e.r = NULL;
	f->type = E_CONCAT;
      }
    }
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_ASSIGN;
    c->label = NULL;
    c->space = NULL;
    c->u.assign.e = e;
    c->u.assign.id = new ActId (string_cache ("self"));
  }
  Assert (_exf, "What?");
  act_chp *xchp = _exf->getlang()->getchp();
  xchp->c = c;
#if 0
  printf ("CHP:\n");
  chp_print (stdout, xchp->c);
  printf ("\n");
#endif  
}
