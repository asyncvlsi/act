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
}

static const char *MEMVAR_STRING = "_memdatv";

int ActCHPMemory::_fresh_memdata (Scope *sc, int bw)
{
  char buf[100];
  int xval;

  xval = 0;
  for (int i=0; i < _memdata_len; i++) {
    if (_memdata_var[i].bw == bw && _memdata_var[i].used == 0) {
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

  /* add this! */
  InstType *it = TypeFactory::Factory()->NewInt (_curbnl->cur, Type::NONE,
						 0, const_expr (bw));
  it = it->Expand (ActNamespace::Global(), sc);
  sc->Add (buf, it);

  if (_memdata_len == 0) {
    MALLOC (_memdata_var, struct memvar_info, 1);
  }
  else {
    REALLOC (_memdata_var, struct memvar_info, _memdata_len + 1);
  }
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
    it->setParam (1, const_expr (v->width));
    it = it->Expand (ActNamespace::Global(), _curbnl->cur);
    
    /*-- delete dynamic variable! --*/
    _curbnl->cur->Add (v->aid->getName(), it);

    if (!_global_info) {
      _global_info = list_new ();
    }
    list_append ((list_t *) _global_info, it->BaseType());

    if (!ret) {
      ret = list_new ();
    }
    list_append (ret, _curbnl->cur->LookupVal (v->aid->getName()));
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

static void _append_mem_read (list_t *top, ActId *access, int idx, Scope *sc)
{
  act_chp_lang_t *c;
  ActId *tmp;
  Array *a;
  list_t *l = list_new ();
  tmp = new ActId (access->getName());
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
  c->u.comm.chan = new ActId (access->getName());
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
  c->u.comm.chan = new ActId (access->getName());
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

  list_append (top, c);
}

static void _append_mem_write (list_t *top, ActId *access, Expr *e, Scope *sc)
{
  act_chp_lang_t *c;
  ActId *tmp;
  Array *a;
  list_t *l = list_new ();
  
  tmp = new ActId (access->getName());
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
  c->u.comm.chan = new ActId (access->getName());
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
  c->u.comm.chan = new ActId (access->getName());
  c->u.comm.chan->Append (new ActId ("din"));
  c->u.comm.e = e;

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
	/* check index to see if it has a dynamic var as well! */
	{
	  ActId *tmp = (ActId *)e->u.e.l;
	  Array *tmpa = tmp->arrayInfo();
	  Assert (tmpa, "Hmm?!");
	  list_t *tmpl = list_new ();
	  _subst_dynamic_array (tmpl, tmpa->getDeref(0));
	  list_concat (l, tmpl);
	  list_free (tmpl);
	}
	
	/* we need to re-write this! */
	if (v->isstruct) {
	  warning ("Ignoring structure memory");
	}
	int idx = _fresh_memdata (_curbnl->cur, v->width);
	char buf[100];
	snprintf (buf, 100, "%s%d", MEMVAR_STRING, idx);
	list_iappend (l, idx);
	list_append (l, e->u.e.l);
	e->u.e.l = (Expr *) new ActId (buf);
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
	if (v->isstruct) {
	  warning ("Structure memory, problem!");
	}
	int idx = _fresh_memdata (_curbnl->cur, v->width);
	Expr *var;
	post = list_new ();

	char buf[100];
	snprintf (buf, 100, "%s%d", MEMVAR_STRING, idx);

	Expr *e;
	NEW (e, Expr);
	e->type = E_VAR;
	e->u.e.l = (Expr *) new ActId (buf);
	if (c->type == ACT_CHP_ASSIGN) {
	  _append_mem_write (post, c->u.assign.id, e, _curbnl->cur);
	  c->u.assign.id = new ActId (buf);
	}
	else {
	  _append_mem_write (post, c->u.comm.var, e, _curbnl->cur);
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
    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->g) {
	/* XXX: check expr for memory */
      }
      if (gc->s) {
	_extract_memory (gc->s);
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
