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


ActCHPArbiter::ActCHPArbiter (Act *a) : ActPass (a, "chparb")
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
}

static int _found_pair (list_t *l, act_chp_gc_t *gc)
{
  listitem_t *li;
  ActId *id1, *id2;
  id1 = (ActId *)gc->g->u.e.l;
  id2 = (ActId *)gc->next->g->u.e.l;

  for (li = list_first (l); li; li = list_next (li)) {
    ActId *x1 = (ActId *) list_value (li);
    li = list_next (li);
    ActId *x2 = (ActId *) list_value (li);

    if (x1 == id1 || x1->isEqual (id1)) {
      if (x2 == id2 || x2->isEqual (id2)) {
	return 0;
      }
      /* partial match */
      return 1;
    }
    else if (x1 == id2 || x1->isEqual (id2)) {
      if (x2 == id1 || x2->isEqual (id1)) {
	return 0;
      }
      return 1;
    }
    else if (x2 == id1 || x2->isEqual (id1)) {
      return 1;
    }
    else if (x2 == id2 || x2->isEqual (id2)) {
      return 1;
    }
  }
  list_append (l, id1);
  list_append (l, id2);
  return 0;
}

static const char *PROXYVAR_STRING = "_chpx";
static const char *ARB_STRING = "_arbx";

static char *_gen_name (int idx)
{
  static char buf[100];
  snprintf (buf, 100, "%s%d", PROXYVAR_STRING, idx);
  return buf;
}
  
int ActCHPArbiter::_fresh_channel (Scope *sc, int bw)
{
  int xval = 0;
  char *tmp;

  do {
    xval++;
    tmp = _gen_name (xval);
  } while (sc->Lookup (tmp));

  /* add this! */
  InstType *it = TypeFactory::Factory()->NewChan (_curbnl->cur, Type::NONE,
						  TypeFactory::Factory()->
						  NewInt (_curbnl->cur,
							  Type::NONE,
							  0,
							  const_expr (bw)),
						  NULL);
  it = it->Expand (ActNamespace::Global(), sc);
  sc->Add (tmp, it);

  return xval;
}


static int _find_channel (list_t *l1, list_t *l2, ActId *id)
{
  listitem_t *li1, *li2;

  for (li1 = list_first (l1), li2 = list_first (l2);
       li1 && li2; li1 = list_next (li1), li2 = list_next (li2)) {
    ActId *x = (ActId *) list_value (li1);
    if (x == id || x->isEqual (id)) {
      return list_ivalue (li2);
    }
  }
  return -1;
}

void *ActCHPArbiter::local_op (Process *p, int mode)
{
  Scope *sc;
  
  if (!p) return NULL;
  if (!p->getlang()) return NULL;
  if( !p->getlang()->getchp()) return NULL;

  _curbnl = _bp->getBNL (p);
  if (!_curbnl) {
    return NULL;
  }

  /* find and extract arbiters! */

  list_t *l = list_new ();
  _find_potential_arbiters (l, p->getlang()->getchp()->c);

  if (!list_isempty (l)) {
    list_t *probe_pairs = list_new ();
    /*-- collect probe pairs --*/

    for (listitem_t *li = list_first (l); li; li = list_next (li)) {
      int res = _found_pair (probe_pairs,
			     ((act_chp_lang_t *) list_value (li))->u.gc);
      if (res == 0) {
	/* not found, added to the probe pair list, or found exact match */
      }
      else {
	/* error! */
	act_error_ctxt (stderr);
	warning ("Arbiter decomposition failed; overlapping probe pairs in guards");
	list_free (l);
	return NULL;
      }
    }

    /* probe pairs are the ones to be replaced! */
    list_t *probe_proxy = list_new ();

    for (listitem_t *li = list_first (probe_pairs); li; li = list_next (li)) {
      ActId *id1 = (ActId *) list_value (li);
      li = list_next (li);
      ActId *id2 = (ActId *) list_value (li);

      InstType *it1 = _curbnl->cur->FullLookup (id1, NULL);
      InstType *it2 = _curbnl->cur->FullLookup (id2, NULL);

      Assert (TypeFactory::isChanType (it1), "Probing a non-channel?");
      Assert (TypeFactory::isChanType (it2), "Probing a non-channel?");

      int idx1, idx2;
      idx1 = _fresh_channel (_curbnl->cur, TypeFactory::bitWidth (it1));
      idx2 = _fresh_channel (_curbnl->cur, TypeFactory::bitWidth (it2));

      /* new channel name */
      list_iappend (probe_proxy, idx1);
      list_iappend (probe_proxy, idx2);
    }

    /* now substitute! */
    _substitute (p->getlang()->getchp()->c, probe_pairs, probe_proxy);

    for (listitem_t *li = list_first (l); li; li = list_next (li)) {
      act_chp_gc_t *gc = ((act_chp_lang_t *) list_value (li))->u.gc;
      int idx = _find_channel (probe_pairs, probe_proxy,
			       (ActId *) gc->g->u.e.l);
      Assert (idx >= 0, "What?!");
      gc->g->u.e.l = (Expr *) new ActId (_gen_name (idx));
      idx = _find_channel (probe_pairs, probe_proxy,
			   (ActId *) gc->next->g->u.e.l); 
      Assert (idx >= 0, "What?!");
      gc->next->g->u.e.l = (Expr *) new ActId (_gen_name (idx));
    }

    /* and introduce arbiters! */
    {
      listitem_t *li1, *li2;
      char buf[100];
      int aid = 0;

      ActNamespace *std_ns = a->findNamespace ("std");
      if (!std_ns) {
	fatal_error ("Could not find the std namespace!");
      }
      UserDef *arb_p = std_ns->findType ("arbiter");
      if (!arb_p) {
	fatal_error ("Could not find process std::arbiter!");
      }

      
      for (li1 = list_first (probe_proxy), li2 = list_first (probe_pairs);
	   li1 && li2; li1 = list_next (li1), li2 = list_next (li2)) {

	ActId *id1 = (ActId *) list_value (li2);
	int idx1 = list_ivalue (li1);

	li1 = list_next (li1);
	li2 = list_next (li2);

	ActId *id2 = (ActId *) list_value (li2);
	int idx2 = list_ivalue (li1);

	do {
	  snprintf (buf, 100, "%s%d", ARB_STRING, aid++);
	} while (_curbnl->cur->Lookup (buf));

	InstType *it = new InstType (_curbnl->cur, arb_p, 0);
	it->setNumParams (2);
	
	InstType *it1 = _curbnl->cur->FullLookup (id1, NULL);
	InstType *it2 = _curbnl->cur->FullLookup (id2, NULL);
	it->setParam (0, const_expr (TypeFactory::bitWidth (it1)));
	it->setParam (1, const_expr (TypeFactory::bitWidth (it2)));
	it = it->Expand (ActNamespace::Global(), _curbnl->cur);

	_curbnl->cur->Add (buf, it);

	/* XXX: now we add connections! */

	ActId *inst;
	inst = new ActId (buf);
	inst->Append (new ActId ("A"));
	AExpr *rhs;
	rhs = new AExpr (id1->Clone());

	ActBody_Conn *ac1 = new ActBody_Conn (-1, inst, rhs);
	
	inst = new ActId (buf);
	inst->Append (new ActId ("B"));
	rhs = new AExpr (id2->Clone());

	ActBody_Conn *ac2 = new ActBody_Conn (-1, inst, rhs);

	inst = new ActId (buf);
	inst->Append (new ActId ("Ap"));
	rhs = new AExpr (new ActId (_gen_name (idx1)));

	ActBody_Conn *ac3 = new ActBody_Conn (-1, inst, rhs);
	
	inst = new ActId (buf);
	inst->Append (new ActId ("Bp"));
	rhs = new AExpr (new ActId (_gen_name (idx2)));

	ac3->Append (new ActBody_Conn (-1, inst, rhs));
	ac2->Append (ac3);
	ac1->Append (ac2);

	int oval = Act::double_expand;
	Act::double_expand = 0;
	ac1->Expandlist (NULL, _curbnl->cur);
	Act::double_expand = oval;
      }

      for (li1 = list_first (l); li1; li1 = list_next (li1)) {
	act_chp_lang_t *c = (act_chp_lang_t *) list_value (li1);
	c->type = ACT_CHP_SELECT;
      }
    }

    list_free (probe_proxy);
    list_free (probe_pairs);
  }
  list_free (l);

  return NULL;
}

void ActCHPArbiter::free_local (void *v)
{
  
}

int ActCHPArbiter::run (Process *p)
{
  int ret = ActPass::run (p);

  /* the netlist has changed; we need to run all passes again */
  ActPass::refreshAll (a, p);
  
  return ret;
}


void ActCHPArbiter::_find_potential_arbiters (list_t *l, act_chp_lang_t *c)
{
  int found_arb = 0;
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
  case ACT_CHP_ASSIGN:
    break;

  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    {
      listitem_t *li = list_first (c->u.semi_comma.cmd);
      while (li) {
	act_chp_lang_t *x = (act_chp_lang_t *) list_value (li);
	_find_potential_arbiters (l, x);
	li = list_next (li);
      }
    }
    break;

  case ACT_CHP_SELECT_NONDET:
    /*-- only two-way arbiters are supported --*/
    if (c->u.gc->g && c->u.gc->next && c->u.gc->g && !c->u.gc->next->next) {
      if (c->u.gc->g->type == E_PROBE && c->u.gc->next->g->type == E_PROBE) {
	list_append (l, c);
	found_arb = 1;
      }
    }
    if (!found_arb) {
      act_error_ctxt (stderr);
      warning ("Arbiter decomposition only supports two-way arbitration between probes.");
    }
    break;

  case ACT_CHP_SELECT:
    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->s) {
	_find_potential_arbiters (l, gc->s);
      }
    }
    break;

  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->s) {
	_find_potential_arbiters (l, gc->s);
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
 
void ActCHPArbiter::_substitute (act_chp_lang_t *c, list_t *l1, list_t *l2)
{
  int id;
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    /* check channel */
    if ((id = _find_channel (l1, l2, c->u.comm.chan)) != -1) {
      char *tmp = _gen_name (id);
      /* substitute */
      c->u.comm.chan = new ActId (tmp);
    }
    break;
    
  case ACT_CHP_ASSIGN:
    break;

  case ACT_CHP_SEMI:
  case ACT_CHP_COMMA:
    {
      listitem_t *li = list_first (c->u.semi_comma.cmd);
      while (li) {
	act_chp_lang_t *x = (act_chp_lang_t *) list_value (li);
	_substitute (x, l1, l2);
	li = list_next (li);
      }
    }
    break;

  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_SELECT:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      if (gc->s) {
	_substitute (gc->s, l1, l2);
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
