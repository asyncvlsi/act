/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2022 Rajit Manohar
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
#include "chpdecomp.h"
#include <act/iter.h>
#include <string.h>


ActCHPMemory::ActCHPMemory (Act *a) : ActPass (a, "chpmem")
{
  ActPass *ap = a->pass_find ("booleanize");
  if (ap) {
    _bp = dynamic_cast<ActBooleanizePass *> (ap);
  }
  else {
    _bp = NULL;
  }
  if (!_bp) {
    _bp = new ActBooleanizePass (a);
  }
  AddDependency ("booleanize");
  disableUpdate ();

  _memdata_len = 0;
  _memdata_var = NULL;
  _curbnl = NULL;

  config_set_default_string ("act.decomp.mem", "std::ram");
  config_set_default_string ("act.decomp.mem_suffix", "_m");
}

static const char *MEMVAR_STRING = "_memdatv";
static const char *MEMSTR_STRING = "_memdats";

int ActCHPMemory::_fresh_memdata (Scope *sc, int bw, Data *isstruct)
{
  char buf[100];
  int xval;

  xval = 0;
  for (int i=0; i < _memdata_len; i++) {
    if ((isstruct ? _memdata_var[i].isstruct->isEqual (isstruct)
	 : _memdata_var[i].bw == bw) && _memdata_var[i].used == 0) {
      _memdata_var[i].used = 1;
      return _memdata_var[i].idx;
    }
    if (xval < _memdata_var[i].idx) {
      xval = _memdata_var[i].idx;
    }
  }
      
  do {
    xval++;
    snprintf (buf, 100, "%s%d", MEMVAR_STRING, xval);
  } while (sc->Lookup (buf));

  if (isstruct) {
    bw = TypeFactory::totBitWidth (isstruct);
  }

  /* add this! */
  InstType *it = TypeFactory::Factory()->NewInt (_curbnl->cur, Type::NONE,
						 0, const_expr (bw));
  it = it->Expand (ActNamespace::Global(), sc);
  sc->Add (buf, it);

  if (isstruct) {
    snprintf (buf, 100, "%s%d", MEMSTR_STRING, xval);
    it = new InstType (sc, isstruct, 0);
    it = it->Expand (ActNamespace::Global(), sc);
    sc->Add (buf, it);
  }

  if (_memdata_len == 0) {
    MALLOC (_memdata_var, struct memvar_info, 1);
  }
  else {
    REALLOC (_memdata_var, struct memvar_info, _memdata_len + 1);
  }
  _memdata_var[_memdata_len].isstruct = isstruct;
  _memdata_var[_memdata_len].used = 1;
  _memdata_var[_memdata_len].bw = bw;
  _memdata_var[_memdata_len].idx = xval;
  
  _memdata_len++;
  
  return xval;
}



void ActCHPMemory::_fresh_release (int idx)
{
  for (int i=0; i < _memdata_len; i++) {
    if (_memdata_var[i].idx == idx) {
      _memdata_var[i].used = 0;
      return;
    }
  }
}

void *ActCHPMemory::local_op (Process *p, int mode)
{
  Scope *sc;
  list_t *ret = NULL;
  
  if (!p) return NULL;
  if (!p->getlang()) return NULL;
  if( !p->getlang()->getchp()) return NULL;

  _curbnl = _bp->getBNL (p);
  if (!_curbnl) {
    return NULL;
  }

  const char *mem_procname = config_get_string ("act.decomp.mem");

  char *tmpname = p->getFullName();
  int pos = 0, pos2;

  // skip any "::"
  while (tmpname[pos] && tmpname[pos] == ':')
    pos++;

  pos2 = pos;
  while (tmpname[pos2] && tmpname[pos2] != '<') {
    pos2++;
  }
  if (tmpname[pos2]) {
    tmpname[pos2] = '\0';
  }

  if (strcmp (mem_procname, tmpname+pos) == 0) {
    warning ("Design has manually instantiated `%s'; skipped by memory extraction pass.", mem_procname);
    FREE (tmpname);
    return NULL;
  }
  FREE (tmpname);


  _extract_memory (p->getlang()->getchp()->c);

  /* now delete all memory references! */
  phash_iter_t it;
  phash_bucket_t *b;
  phash_iter_init (_curbnl->cdH, &it);
  while ((b = phash_iter_next (_curbnl->cdH, &it))) {
    act_dynamic_var_t *v = (act_dynamic_var_t *)b->v;

    /*-- delete dynamic variable! --*/
    _curbnl->cur->Del (v->aid->getName());

    /*-- replace with instance! --*/
    const char *mem_procname = config_get_string ("act.decomp.mem");
    Process *p = a->findProcess (mem_procname);
    if (!p) {
      fatal_error ("Could not find process `%s'", mem_procname);
    }

    InstType *it = new InstType (_curbnl->cur, p, 0);
    it->setNumParams (2);
    it->setParam (0, const_expr (v->a->size()));
    if (v->isstruct) {
      it->setParam (1, const_expr (TypeFactory::totBitWidth (v->isstruct)));
    }
    else {
      it->setParam (1, const_expr (v->width));
    }
    it = it->Expand (ActNamespace::Global(), _curbnl->cur);
    
    /*-- replace with new type dynamic variable, add suffix to be safe! --*/
    const char *newname;
    {
      char *tbuf;
      int tlen;
      tlen = strlen (v->aid->getName()) +
	strlen (config_get_string ("act.decomp.mem_suffix")) + 1;
      MALLOC (tbuf, char, tlen);
      snprintf (tbuf, tlen, "%s%s", v->aid->getName(),
		config_get_string ("act.decomp.mem_suffix"));
      newname = string_cache (tbuf);
      FREE (tbuf);
      _curbnl->cur->Add (newname, it);
    }

    if (!_decomp_info) {
      _decomp_info = list_new ();
    }
    list_append (_decomp_info, it->BaseType());

    if (!ret) {
      ret = list_new ();
    }
    list_append (ret, _curbnl->cur->LookupVal (newname));
  }
  
  if (_memdata_len > 0) {
    char buf[100];
    for (int i=0; i < _memdata_len; i++) {
      snprintf (buf, 100, "%s%d", MEMVAR_STRING, _memdata_var[i].idx);
      list_append (ret, _curbnl->cur->LookupVal (buf));
    }
    FREE (_memdata_var);
  }
  _memdata_len = 0;
  _memdata_var = NULL;
  _curbnl = NULL;

  return ret;
}

void ActCHPMemory::free_local (void *v)
{
  list_t *l = (list_t *)v;
  if (l) {
    list_free (l);
  }
}

int ActCHPMemory::run (Process *p)
{
  int ret = ActPass::run (p);

  /* the netlist has changed; we need to run all passes again */
  ActPass::refreshAll (a, p);
  
  return ret;
}

static Expr *_gen_address (InstType *it, Array *a)
{
  Expr *tmp = NULL;
  for (int i=a->nDims()-1; i >=0; i--) {
    if (!tmp) {
      tmp = expr_dup ((Expr *)a->getDeref (i));
    }
    else {
      Expr *x;
      NEW (x, Expr);
      x->type = E_PLUS;
      NEW (x->u.e.r, Expr);
      x->u.e.r->type = E_MULT;
      x->u.e.r->u.e.r = tmp;
      x->u.e.r->u.e.l = const_expr (it->arrayInfo()->range_size (i));
      x->u.e.l = a->getDeref (i);
      tmp = x;
    }
  }
  return tmp;
}


int ActCHPMemory::_elemwise_assign (list_t *l, int idx, ActId *field, Data *d, int off,
				    Scope *sc)
{
  Assert (d && TypeFactory::isStructure (d), "What?");
  ActId *ftail, *oldtail;

  if (field) {
    ftail = field->Tail ();
    oldtail = ftail;
  }
  else {
    ftail = NULL;
    oldtail = NULL;
  }

  for (int i=0; i < d->getNumPorts(); i++) {
    int sz;
    InstType *it = d->getPortType (i);
    Array *a = it->arrayInfo();
    Arraystep *as;
    if (a) {
      as = a->stepper();
    }
    else {
      as = NULL;
    }

    if (!field) {
      field = new ActId (d->getPortName (i));
      ftail = field;
    }
    else {
      ftail->Append (new ActId (d->getPortName (i)));
      ftail = ftail->Rest();
    }
    
    sz = TypeFactory::bitWidth (it);
    while (!as || !as->isend()) {
      Array *fa;
      if (as) {
	fa = as->toArray();
      }
      else {
	fa = NULL;
      }
      ftail->setArray (fa);
      if (TypeFactory::isStructure (it)) {
	Data *sub = dynamic_cast <Data *> (it->BaseType());
	Assert (field, "what?");
	off = _elemwise_assign (l, idx, field, sub, off, sc);
      }
      else {
	act_chp_lang_t *c;
	char buf[100];
	ActId *rhs;
	NEW (c, act_chp_lang_t);
	c->space = NULL;
	c->label = NULL;
	c->type = ACT_CHP_ASSIGN;
	snprintf (buf, 100, "%s%d", MEMSTR_STRING, idx);
	c->u.assign.id = new ActId (buf);
	c->u.assign.id->Append (field->Clone());

	snprintf (buf, 100, "%s%d", MEMVAR_STRING, idx);
	rhs = new ActId (buf);
	
	NEW (c->u.assign.e, Expr);
	c->u.assign.e->type = E_BITFIELD;
	c->u.assign.e->u.e.l = (Expr *) rhs;
	NEW (c->u.assign.e->u.e.r, Expr);
	c->u.assign.e->u.e.r->type = E_BITFIELD;
	c->u.assign.e->u.e.r->u.e.l = const_expr (off);
	c->u.assign.e->u.e.r->u.e.r = const_expr (off + sz - 1);

	Expr *tmp = expr_expand (c->u.assign.e, ActNamespace::Global(), sc,
				 ACT_EXPR_EXFLAG_CHPEX);
	expr_free (c->u.assign.e);
	c->u.assign.e = tmp;

	list_append (l, c);
	
	off += sz;
      }
      ftail->setArray (NULL);
      if (fa) {
	delete fa;
      }
      if (!as) {
	break;
      }
      else {
	as->step();
      }
    }
    if (as) {
      delete as;
    }
    if (oldtail) {
      delete oldtail->Rest();
      oldtail->prune();
    }
    else {
      delete field;
      field = NULL;
    }
  }
  return off;
}


Expr *ActCHPMemory::_elemwise_fieldlist (int idx, ActId *field, Data *d)
{
  Assert (d && TypeFactory::isStructure (d), "What?");
  ActId *ftail, *oldtail;
  Expr *ret = NULL;
  Expr *rtail = NULL;

  if (field) {
    ftail = field->Tail ();
    oldtail = ftail;
  }
  else {
    ftail = NULL;
    oldtail = NULL;
  }

  for (int i=0; i < d->getNumPorts(); i++) {
    int sz;
    InstType *it = d->getPortType (i);
    Array *a = it->arrayInfo();
    Arraystep *as;
    if (a) {
      as = a->stepper();
    }
    else {
      as = NULL;
    }

    if (!field) {
      field = new ActId (d->getPortName (i));
      ftail = field;
    }
    else {
      ftail->Append (new ActId (d->getPortName (i)));
      ftail = ftail->Rest();
    }
    
    sz = TypeFactory::bitWidth (it);
    while (!as || !as->isend()) {
      Expr *elem;
      Array *fa;
      if (as) {
	fa = as->toArray();
      }
      else {
	fa = NULL;
      }
      ftail->setArray (fa);
      if (TypeFactory::isStructure (it)) {
	Data *sub = dynamic_cast <Data *> (it->BaseType());
	Assert (field, "what?");
	elem = _elemwise_fieldlist (idx, field, sub);
      }
      else {
	NEW (elem, Expr);
	elem->type = E_CONCAT;
	elem->u.e.r = NULL;
	NEW (elem->u.e.l, Expr);
	elem->u.e.l->type = E_VAR;
	elem->u.e.l->u.e.r = NULL;

	ActId *rhs;
	char buf[100];
	snprintf (buf, 100, "%s%d", MEMSTR_STRING, idx);
	rhs = new ActId (buf);
	rhs->Append (field->Clone());

	elem->u.e.l->u.e.l = (Expr *)rhs;
      }
      ftail->setArray (NULL);
      if (fa) {
	delete fa;
      }

      if (!ret) {
	ret = elem;
	rtail = ret;
      }
      else {
	rtail->u.e.r = elem;
      }
      while (rtail->u.e.r) {
	rtail = rtail->u.e.r;
      }
      if (!as) {
	break;
      }
      else {
	as->step();
      }
    }
    if (as) {
      delete as;
    }
    if (oldtail) {
      delete oldtail->Rest();
      oldtail->prune();
    }
    else {
      delete field;
      field = NULL;
    }
  }
  return ret;
}


int ActCHPMemory::_inv_idx (int idx)
{
  for (int i=0; i < _memdata_len; i++) {
    if (_memdata_var[i].used && _memdata_var[i].idx == idx) {
      return i;
    }
  }
  return -1;
}

void ActCHPMemory::_append_mem_read (list_t *top, ActId *access, int idx, Scope *sc)
{
  act_chp_lang_t *c;
  ActId *tmp;
  Array *a;
  list_t *l = list_new ();
  const char *newname;

  {
    char *tmpn;
    int tmpl;
    tmpl = strlen (access->getName())
      + strlen (config_get_string ("act.decomp.mem_suffix")) + 1;
    MALLOC (tmpn, char, tmpl);
    snprintf (tmpn, tmpl, "%s%s", access->getName(), config_get_string ("act.decomp.mem_suffix"));
    tmp = new ActId (string_cache (tmpn));
    FREE (tmpn);
    newname = tmp->getName();
  }
  tmp->Append (new ActId ("addr"));

  NEW (c, act_chp_lang_t);
  c->label = NULL;
  c->space = NULL;
  c->type = ACT_CHP_SEND;
  c->u.comm.flavor = 0;
  c->u.comm.convert = 0;
  c->u.comm.var = NULL;
  c->u.comm.chan = tmp;

  a = access->arrayInfo();
  c->u.comm.e = _gen_address (sc->Lookup (access->getName()), a);
  delete a;

  access->setArray (NULL);

  /* mem.addr!e */
  list_append (l, c);

  NEW (c, act_chp_lang_t);
  c->label = NULL;
  c->space = NULL;
  c->type = ACT_CHP_SEND;
  c->u.comm.flavor = 0;
  c->u.comm.convert = 0;
  c->u.comm.var = NULL;
  c->u.comm.chan = new ActId (newname);
  c->u.comm.chan->Append (new ActId ("rd"));
  c->u.comm.e = const_expr (1);

  /* mem.rd!1 */
  list_append (l, c);

  NEW (c, act_chp_lang_t);
  c->label = NULL;
  c->space = NULL;
  c->type = ACT_CHP_RECV;
  c->u.comm.flavor = 0;
  c->u.comm.convert = 0;
  c->u.comm.e = NULL;
  c->u.comm.chan = new ActId (newname);
  c->u.comm.chan->Append (new ActId ("dout"));

  char buf[100];
  snprintf (buf, 100, "%s%d", MEMVAR_STRING, idx);
  c->u.comm.var = new ActId (buf);

  /* mem.dout?v */
  list_append (l, c);
  delete access;

  NEW (c, act_chp_lang_t);
  c->space = NULL;
  c->label = NULL;
  c->type = ACT_CHP_COMMA;
  c->u.semi_comma.cmd = l;

  /* check if structure */
  int idx_i = _inv_idx (idx);
  Assert (idx_i >= 0, "What!");
  
  if (_memdata_var[idx_i].isstruct) {
    act_chp_lang_t *d;
    list_t *nl;

    nl = list_new ();
    list_append (nl, c);

    // do element-wise structure assignment
    _elemwise_assign (nl, idx, NULL, _memdata_var[idx_i].isstruct, 0, sc);
     
    NEW (d, act_chp_lang_t);
    d->space = NULL;
    d->label = NULL;
    d->type = ACT_CHP_SEMI;
    d->u.semi_comma.cmd = nl;
    c = d;
  }

  list_append (top, c);
}

void ActCHPMemory::_append_mem_write (list_t *top, ActId *access, int idx, Expr *e, Scope *sc)
{
  act_chp_lang_t *c;
  ActId *tmp;
  Array *a;
  list_t *l = list_new ();
  const char *newname;

  {  
    char *tmpn;
    int tmpl;
    tmpl = strlen (access->getName())
      + strlen (config_get_string ("act.decomp.mem_suffix")) + 1;
    MALLOC (tmpn, char, tmpl);
    snprintf (tmpn, tmpl, "%s%s", access->getName(), config_get_string ("act.decomp.mem_suffix"));
    tmp = new ActId (string_cache (tmpn));
    FREE (tmpn);
    newname  = tmp->getName();
  }
  tmp->Append (new ActId ("addr"));

  NEW (c, act_chp_lang_t);
  c->label = NULL;
  c->space = NULL;
  c->type = ACT_CHP_SEND;
  c->u.comm.flavor = 0;
  c->u.comm.convert = 0;
  c->u.comm.var = NULL;
  c->u.comm.chan = tmp;

  a = access->arrayInfo();
  c->u.comm.e = _gen_address (sc->Lookup (access->getName()), a);
  delete a;

  access->setArray (NULL);

  /* mem.addr!e */
  list_append (l, c);

  NEW (c, act_chp_lang_t);
  c->label = NULL;
  c->space = NULL;
  c->type = ACT_CHP_SEND;
  c->u.comm.flavor = 0;
  c->u.comm.convert = 0;
  c->u.comm.var = NULL;
  c->u.comm.chan = new ActId (newname);
  c->u.comm.chan->Append (new ActId ("rd"));
  c->u.comm.e = const_expr (0);

  /* mem.rd!0 */
  list_append (l, c);

  NEW (c, act_chp_lang_t);
  c->label = NULL;
  c->space = NULL;
  c->type = ACT_CHP_SEND;
  c->u.comm.flavor = 0;
  c->u.comm.convert = 0;
  c->u.comm.var = NULL;
  c->u.comm.chan = new ActId (newname);
  c->u.comm.chan->Append (new ActId ("din"));

  int inv_idx = _inv_idx (idx);
  Assert (inv_idx >= 0, "What?");
  if (_memdata_var[inv_idx].isstruct) {
    expr_free (e);
    c->u.comm.e = _elemwise_fieldlist (idx, NULL, _memdata_var[inv_idx].isstruct);
  }
  else {
    c->u.comm.e = e;
  }

  /* mem.din!e */
  list_append (l, c);

  delete access;

  NEW (c, act_chp_lang_t);
  c->space = NULL;
  c->label = NULL;
  c->type = ACT_CHP_COMMA;
  c->u.semi_comma.cmd = l;

  list_append (top, c);
}

void ActCHPMemory::_subst_dynamic_array (list_t *l, Expr *e)
{
  if (!e) return;
  switch (e->type) {
  case E_PLUS: case E_MULT: case E_MINUS:
  case E_DIV:  case E_MOD:
  case E_AND:  case E_OR:  case E_XOR:
  case E_LSL:  case E_LSR:  case E_ASR:
  case E_LT:  case E_GT:
  case E_LE:  case E_GE:
  case E_EQ:  case E_NE:
    _subst_dynamic_array (l, e->u.e.l);
    _subst_dynamic_array (l, e->u.e.r);
    break;
    
  case E_NOT:
  case E_UMINUS:
  case E_COMPLEMENT:
    _subst_dynamic_array (l, e->u.e.l);
    break;

  case E_QUERY:
    _subst_dynamic_array (l, e->u.e.l);
    _subst_dynamic_array (l, e->u.e.r->u.e.l);
    _subst_dynamic_array (l, e->u.e.r->u.e.r);
    break;

  case E_INT:
  case E_TRUE:
  case E_FALSE:
  case E_PROBE:
    break;
    
  case E_VAR:
  case E_BITFIELD:
    /* check e->u.e.l */
    {
      act_dynamic_var_t *v = _bp->isDynamicRef (_curbnl, (ActId *)e->u.e.l);
      if (v) {
	ActId *tail;
	/* check index to see if it has a dynamic var as well! */
	{
	  ActId *tmp = (ActId *)e->u.e.l;
	  Array *tmpa = tmp->arrayInfo();
	  Assert (tmpa, "Hmm?!");
	  list_t *tmpl = list_new ();
	  _subst_dynamic_array (tmpl, tmpa->getDeref(0));
	  list_concat (l, tmpl);
	  list_free (tmpl);
	  tail = tmp->Rest();
	}
	
	/* we need to re-write this! */
	int idx = _fresh_memdata (_curbnl->cur, v->width, v->isstruct);
	char buf[100];

	if (v->isstruct) {
	  snprintf (buf, 100, "%s%d", MEMSTR_STRING, idx);
	}
	else {
	  snprintf (buf, 100, "%s%d", MEMVAR_STRING, idx);
	}
	list_iappend (l, idx);
	list_append (l, e->u.e.l);
	e->u.e.l = (Expr *) new ActId (buf);
	if (tail) {
	  ((ActId *)e->u.e.l)->Append (tail->Clone());
	}
      }
    }
    break;

  case E_CONCAT:
    {
      Expr *f = e;
      while (f) {
	_subst_dynamic_array (l, f->u.e.l);
	f = f->u.e.r;
      }
    }
    break;
    
  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    _subst_dynamic_array (l, e->u.e.l);
    break;

  case E_FUNCTION:
    {
      Expr *f = e->u.fn.r;
      while (f) {
	_subst_dynamic_array (l, f->u.e.l);
	f = f->u.e.r;
      }
    }
    break;
    
  case E_COMMA:
  case E_COLON:
  default:
    fatal_error ("What? %d\n", e->type);
    break;
  }
}
			      
void ActCHPMemory::_extract_memory (act_chp_lang_t *c)
{
  list_t *pre, *post;
  act_chp_lang_t *d = NULL;
  act_chp_lang_t *x = NULL;
  
  if (!c) return;

  switch (c->type) {
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
  case ACT_CHP_ASSIGN:
    /* check if this is a memory access; if so, change it to
       v := mem[..]
    */

    /* in-place substitution of expression */
    pre = list_new ();
    if (c->type == ACT_CHP_ASSIGN) {
      _subst_dynamic_array (pre, c->u.assign.e);
    }
    else {
      _subst_dynamic_array (pre, c->u.comm.e);
    }
    if (list_isempty (pre)) {
      list_free (pre);
      pre = NULL;
    }
    if (pre) {
      /* idx (ActId *) sequence */
      NEW (x, act_chp_lang_t);
      x->label = NULL;
      x->space = NULL;
      x->type = ACT_CHP_SEMI;
      x->u.semi_comma.cmd = list_new ();

      /* 
	 these are all reads:
	 mem.addr!address,mem.rd!1,mem.dout?v
      */
      for (listitem_t *li = list_first (pre); li; li = list_next (li)) {
	int idx = list_ivalue (li);
	li = list_next (li);
	ActId *mem = (ActId *) list_value (li);
	_append_mem_read (x->u.semi_comma.cmd, mem, idx, _curbnl->cur);
	_fresh_release (idx);
      }
    }
    list_free (pre);
    pre = NULL;

    /* x = NULL if nothing to do on the expression front, 
       otherwise it is all the "pre" operations 
    */

    post = NULL;

    if ((c->type == ACT_CHP_ASSIGN && c->u.assign.id) ||
	(c->type != ACT_CHP_ASSIGN && c->u.comm.var)) {
      act_dynamic_var_t *v = _bp->isDynamicRef (_curbnl,
						c->type == ACT_CHP_ASSIGN ?
						c->u.assign.id : c->u.comm.var);
      if (v) {
	{
	  ActId *tmp = (c->type == ACT_CHP_ASSIGN ?
			c->u.assign.id : c->u.comm.var);
	  Array *tmpa = tmp->arrayInfo();
	  Assert (tmpa, "Hmm?!");
	  list_t *tmpl = list_new ();
	  _subst_dynamic_array (tmpl, tmpa->getDeref(0));
	  if (list_isempty (tmpl)) {
	    list_free (tmpl);
	  }
	  else {
	    list_t *newcmds = list_new ();
	    /* 
	       these are all reads:
	       mem.addr!address,mem.rd!1,mem.dout?v
	    */
	    for (listitem_t *li = list_first (tmpl); li; li = list_next (li)) {
	      int idx = list_ivalue (li);
	      li = list_next (li);
	      ActId *mem = (ActId *) list_value (li);
	      _append_mem_read (newcmds, mem, idx, _curbnl->cur);
	      _fresh_release (idx);
	    }
	    list_free (tmpl);
	    if (x) {
	      list_append (x->u.semi_comma.cmd, newcmds);
	    }
	    else {
	      NEW (x, act_chp_lang_t);
	      x->label = NULL;
	      x->space = NULL;
	      x->type = ACT_CHP_SEMI;
	      x->u.semi_comma.cmd = newcmds;
	    }
	  }
	}
	
	/* write */
	int idx = _fresh_memdata (_curbnl->cur, v->width, v->isstruct);
	Expr *var;
	post = list_new ();

	char buf[100];

	if (v->isstruct) {
	  snprintf (buf, 100, "%s%d", MEMSTR_STRING, idx);
	}
	else {
	  snprintf (buf, 100, "%s%d", MEMVAR_STRING, idx);
	}

	Expr *e;
	e = act_expr_var (new ActId (buf));
	if (c->type == ACT_CHP_ASSIGN) {
	  _append_mem_write (post, c->u.assign.id, idx, e, _curbnl->cur);
	  c->u.assign.id = new ActId (buf);
	}
	else {
	  _append_mem_write (post, c->u.comm.var, idx, e, _curbnl->cur);
	  c->u.comm.var = new ActId (buf);
	}
	_fresh_release (idx);
      }
    }

    if (x || post) {
      list_t *l;

      if (x) {
	l = x->u.semi_comma.cmd;
      }
      else {
	l = list_new ();
	NEW (x, act_chp_lang_t);
      }

      /* save the command */
      *x = *c;
      x->space = NULL;
      x->label = NULL;
      c->type = ACT_CHP_SEMI;
      c->u.semi_comma.cmd = l;
      list_append (l, x);

      if (post) {
	list_concat (l, post);
	list_free (post);
      }
    }
    break;

  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    {
      listitem_t *li = list_first (c->u.semi_comma.cmd);
      while (li) {
	act_chp_lang_t *x = (act_chp_lang_t *) list_value (li);
	_extract_memory (x);
	li = list_next (li);
      }
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:

    /* in-place substitution of expression */
    pre = list_new ();

    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->g) {
	_subst_dynamic_array (pre, gc->g);
      }
    }

    if (!list_isempty (pre)) {
      /* idx (ActId *) sequence */
      NEW (x, act_chp_lang_t);
      x->label = NULL;
      x->space = NULL;
      x->type = ACT_CHP_SEMI;
      x->u.semi_comma.cmd = list_new ();

      /* 
	 these are all reads:
	 mem.addr!address,mem.rd!1,mem.dout?v
      */
      for (listitem_t *li = list_first (pre); li; li = list_next (li)) {
	int idx = list_ivalue (li);
	li = list_next (li);
	ActId *mem = (ActId *) list_value (li);
	_append_mem_read (x->u.semi_comma.cmd, mem, idx, _curbnl->cur);
	_fresh_release (idx);
      }
    }
    list_free (pre);
    pre = NULL;

    /* x = NULL if nothing to do on the expression front, 
       otherwise it is all the "pre" operations 
    */

    if (x) {
      list_t *l = x->u.semi_comma.cmd;

      *x = *c;
      x->label = NULL;
      x->space = NULL;

      list_append (l, x);
      c->type = ACT_CHP_SEMI;
      c->u.semi_comma.cmd = l;

      for (act_chp_gc_t *gc = x->u.gc; gc; gc = gc->next) {
	if (gc->s) {
	  _extract_memory (gc->s);
	}
      }
    }
    else {
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	if (gc->s) {
	  _extract_memory (gc->s);
	}
      }
    }
    break;

  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:

    pre = list_new ();

    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->g) {
	_subst_dynamic_array (pre, gc->g);
      }
    }

    if (!list_isempty (pre)) {
      /* idx (ActId *) sequence */
      NEW (x, act_chp_lang_t);
      x->label = NULL;
      x->space = NULL;
      x->type = ACT_CHP_SEMI;
      x->u.semi_comma.cmd = list_new ();

      /* 
	 these are all reads:
	 mem.addr!address,mem.rd!1,mem.dout?v
      */
      for (listitem_t *li = list_first (pre); li; li = list_next (li)) {
	int idx = list_ivalue (li);
	li = list_next (li);
	ActId *mem = (ActId *) list_value (li);
	_append_mem_read (x->u.semi_comma.cmd, mem, idx, _curbnl->cur);
	_fresh_release (idx);
      }
      list_free (pre);

      // we need to re-structure this into a do-loop + a selection,
      // with an extra Boolean variable for the do-loop guard
      act_chp_gc_t *gc;
      int xtmp = 0;
      char buf[100];

      Assert (_curbnl, "What?");
      Assert (_curbnl->cur, "What?!");
      _curbnl->cur->findFresh ("_tmpx", &xtmp);
      InstType *it = TypeFactory::Factory()->NewBool (Type::direction::NONE);
      it = it->Expand (NULL, _curbnl->cur);
      snprintf (buf, 100, "_tmpx%d", xtmp);
      _curbnl->cur->Add (buf, it);
      ActId *id = new ActId (string_cache (buf));

      /* tmpx := true .. this goes BEFORE the entire do loop */
      act_chp_lang_t *tmpc;
      NEW (tmpc, act_chp_lang_t);
      tmpc->space = NULL;
      tmpc->label = NULL;
      tmpc->type = ACT_CHP_ASSIGN;
      tmpc->u.assign.id = id;
      tmpc->u.assign.e = const_expr_bool (1);

      /* the do loop has a single guard, which is the tmpx variable */
      gc = c->u.gc;
      c->type = ACT_CHP_DOLOOP;
      NEW (c->u.gc, act_chp_gc_t);
      c->u.gc->id = NULL;
      c->u.gc->lo = NULL;
      c->u.gc->hi = NULL;
      NEW (c->u.gc->g, Expr);
      c->u.gc->g->type = E_VAR;
      c->u.gc->g->u.e.l = (Expr *) id->Clone ();
      c->u.gc->g->u.e.r = NULL;
      c->u.gc->next = NULL;

      /* body of the loop starts with PRE */
      c->u.gc->s = x;
      
      act_chp_lang_t *sel;

      NEW (sel, act_chp_lang_t);
      sel->space = NULL;
      sel->label = NULL;
      sel->type = ACT_CHP_SELECT;
      sel->u.gc = gc;

      act_chp_gc_t *prev = NULL;
      while (gc) {
	Assert (gc->g, "Waht?");
	if (gc->s) {
	  _extract_memory (gc->s);
	}
	prev = gc;
	gc = gc->next;
      }
      Assert (prev, "What?");
      NEW (prev->next, act_chp_gc_t);
      prev = prev->next;
      prev->id = NULL;
      prev->lo = NULL;
      prev->hi = NULL;
      prev->g = NULL; // else
      NEW (prev->s, act_chp_lang_t);
      prev->s->label = NULL;
      prev->s->space = NULL;
      prev->s->type = ACT_CHP_ASSIGN;
      prev->s->u.assign.id = id->Clone();
      prev->s->u.assign.e = const_expr_bool (0);
      prev->next = NULL;
      
      list_append (x->u.semi_comma.cmd, tmpc);
      list_append (x->u.semi_comma.cmd, sel);
    }
    else {
      list_free (pre);
      pre = NULL;
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	if (gc->s) {
	  _extract_memory (gc->s);
	}
      }
    }
    break;

  case ACT_CHP_FUNC:
  case ACT_CHP_SKIP:
  case ACT_CHP_HOLE:
    break;

  case ACT_CHP_SEMILOOP:
  case ACT_CHP_COMMALOOP:
  default:
    fatal_error ("Unknown CHP type %d", c->type);
    break;
  }
}
