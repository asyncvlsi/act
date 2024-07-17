/*************************************************************************
 *
 *  Copyright (c) 2024 Rajit Manohar
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


ActDflowSplitMerge::ActDflowSplitMerge (Act *a) : ActPass (a, "dflow_split_merge")
{
  if (config_exists ("act.dflow.split_merge_limit")) {
    _split_merge_limit = config_get_int ("act.dflow.split_merge_limit");
    if (_split_merge_limit < 2) {
      _split_merge_limit = 2;
    }
  }
  else {
    _split_merge_limit = 0;
  }
}


void *ActDflowSplitMerge::local_op (Process *p, int mode)
{
  if (!p) return NULL;
  if (!p->getlang()) return NULL;
  if (!p->getlang()->getdflow()) return NULL;
  if (_split_merge_limit == 0) return NULL;

  // ok, now walk through and fix it!
  listitem_t *li;
  _idx = 0;
  _sc = p->CurScope();
  for (li = list_first (p->getlang()->getdflow()->dflow); li;
       li = list_next (li)) {
    act_dataflow_element *e = (act_dataflow_element *) list_value (li);
    if (e->t == ACT_DFLOW_SPLIT || e->t == ACT_DFLOW_MERGE) {
      _apply_recursive_decomp (e, p->getlang()->getdflow()->dflow);
    }
  }
  return NULL;
}

void ActDflowSplitMerge::free_local (void *x)
{
  Assert (x == NULL, "What?");
}


static int _ceil_log2 (int w)
{
  int i;
  int addone = 0;

  i = 0;
  while (w > 1) {
    if (w & 1) {
      addone = 1;
    }
    w = w >> 1;
    i = i + 1;
  }
  return i + addone;
}

static Expr *_varexpr (ActId *ch)
{
  Expr *e;
  NEW (e, Expr);
  e->type = E_VAR;
  e->u.e.l = (Expr *) ch;
  e->u.e.r = NULL;
  return e;
}

static void _myfree (Expr *e)
{
  if (!e) return;
  if (e->type == E_INT) return;
  if (e->type == E_VAR) {
    FREE (e);
    return;
  }
  if (e->u.e.l) {
    _myfree (e->u.e.l);
  }
  if (e->u.e.r) {
    _myfree (e->u.e.r);
  }
  FREE (e);
}

static Expr *_lt (Expr *lhs, int val)
{
  Expr *e;
  NEW (e, Expr);
  e->type = E_LT;
  e->u.e.l = lhs;
  e->u.e.r = const_expr (val);
  return e;
}

static Expr *_minus (ActId *ch, int val)
{
  Expr *e;
  NEW (e, Expr);
  e->type = E_MINUS;
  e->u.e.l = _varexpr (ch);
  e->u.e.r = const_expr (val);
  return e;
}

/*
  Returns:
   (ch < cmp ? val : <NULL>)
*/
static Expr *_query_expr (ActId *ch, int cmp, int val)
{
  Expr *e;
  NEW (e, Expr);
  e->type = E_QUERY;

  e->u.e.l = _lt (_varexpr (ch), cmp);
  NEW (e->u.e.r, Expr);
  e->u.e.r->type = E_COLON;
  e->u.e.r->u.e.l = const_expr (val);
  e->u.e.r->u.e.r = NULL;

  return e;
}

void ActDflowSplitMerge::_apply_recursive_decomp (act_dataflow_element *e,
						  list_t *l)
{
  Assert (e->t == ACT_DFLOW_SPLIT || e->t == ACT_DFLOW_MERGE, "What?");
  if (e->u.splitmerge.nmulti > _split_merge_limit) {
    int num_stage1 = 0;
    int group_sz, group_mod;
    
    if (e->u.splitmerge.nmulti > _split_merge_limit*_split_merge_limit) {
      // multi-stage decomposition, just use the max first stage
      num_stage1 = _split_merge_limit;
    }
    else {
      num_stage1 = 2;
      while (num_stage1*num_stage1 < e->u.splitmerge.nmulti) {
	num_stage1++;
      }
    }
    Assert (num_stage1 <= _split_merge_limit, "What happened?");

    group_sz = e->u.splitmerge.nmulti/num_stage1;
    group_mod = e->u.splitmerge.nmulti % num_stage1;
    
#if 0
    printf ("stage1: %d\n", num_stage1);
    printf ("  groups: %d\n", group_sz);
    printf ("  left-over: %d\n", group_mod);
#endif

    //
    //    control -> split to num_stage1's
    //                 ... for each, we need to apply a function to
    //                     convert it to the right control value
    //
    //    control -> function to select the correct recursive
    //    components (num_stage1)
    //
    // We need a total of 3 * num_stage1 channels
    //       2* for control, 1* for control
    // Plus we need another control channel for the final split/merge
    //

    int *idx;
    int chpos = 0;
    MALLOC (idx, int, num_stage1*3 + 2);
    char buf[1024];

    /* chan: 0 .. num_stage1-1 are from splitting the control value */
    InstType *it = _sc->FullLookup (e->u.splitmerge.guard, NULL);
    for (int i=0; i < num_stage1; i++) {
      _sc->findFresh ("_splmrg", &_idx);
      idx[chpos++] = _idx;
      snprintf (buf, 1024, "_splmrg%d", _idx);
      _sc->Add (buf, it);
    }

    /* chan: num_stage1 = function applied to the channel control to
       select 0, 1, ..., num_stage1-1
       (chan < (x + (group_mod > 0)) ? 0  : (chan < 2*x) ? 1 : .. etc.
    */
    Expr *ch_recode, *tmp;
    ch_recode = _query_expr (e->u.splitmerge.guard,
			     group_sz + (group_mod > 0 ? 1 : 0), 0);
    tmp = ch_recode;
    int running_sum = group_sz + (group_mod > 0 ? 1 : 0);
    for (int i=1; i < num_stage1-1; i++) {
      running_sum += group_sz + (group_mod > i ? 1 : 0);
      tmp->u.e.r->u.e.r =
	_query_expr (e->u.splitmerge.guard, running_sum, i);
      tmp = tmp->u.e.r->u.e.r;
    }
    tmp->u.e.r->u.e.r = const_expr (num_stage1-1);

    // ok, we have the channel expression!
    tmp = expr_expand (ch_recode, NULL, _sc, ACT_EXPR_EXFLAG_CHPEX);
    _myfree (ch_recode);
    
    act_dataflow_element *extra;


    // create channel recoder
    NEW (extra, act_dataflow_element);
    extra->t = ACT_DFLOW_FUNC;
    extra->u.func.init = NULL;
    extra->u.func.istransparent = 0;
    extra->u.func.nbufs = NULL;
    extra->u.func.lhs = tmp;

    _sc->findFresh ("_splmrg", &_idx);
    idx[chpos++] = _idx;
    snprintf (buf, 1024, "_splmrg%d", _idx);
    it = TypeFactory::Factory()->NewInt (_sc, Type::direction::NONE, 0,
					 const_expr (_ceil_log2(num_stage1)));
    it = TypeFactory::Factory()->NewChan (_sc, Type::direction::NONE,
					  it, NULL);
    it->MkCached();
    it = it->Expand (NULL, _sc);

    _sc->Add (buf, it);

    extra->u.func.rhs = new ActId (string_cache (buf));
    list_append (l, extra);

    // create control split
    NEW (extra, act_dataflow_element);
    extra->t = ACT_DFLOW_SPLIT;
    extra->u.splitmerge.nondetctrl = NULL;
    extra->u.splitmerge.nmulti = num_stage1;
    extra->u.splitmerge.single = e->u.splitmerge.guard->Clone();
    extra->u.splitmerge.guard = new ActId (string_cache (buf));

    MALLOC (extra->u.splitmerge.multi, ActId *, num_stage1);
    for (int i=0; i < num_stage1; i++) {
      snprintf (buf, 1024, "_splmrg%d", idx[i]);
      extra->u.splitmerge.multi[i] = new ActId (string_cache (buf));
    }
    list_append (l, extra);

    // create the smaller split or merge
    ActId **orig_ids = e->u.splitmerge.multi;
    
    MALLOC (e->u.splitmerge.multi, ActId *, num_stage1);
    it = _sc->FullLookup (e->u.splitmerge.single, NULL);
    for (int i=0; i < num_stage1; i++) {
      _sc->findFresh ("_splmrg", &_idx);
      idx[chpos++] = _idx;
      snprintf (buf, 1024, "_splmrg%d", _idx);
      _sc->Add (buf, it);
      e->u.splitmerge.multi[i] = new ActId (string_cache (buf));
    }
    e->u.splitmerge.nmulti = num_stage1;
    e->u.splitmerge.guard = extra->u.splitmerge.guard->Clone();

    // now create the next stage of the tree!
    int loc = 0;
    running_sum = 0;
    for (int i=0; i < num_stage1; i++) {

      /* de-generate case: exactly one element */
      if ((group_sz + ((group_mod > i) ? 1 : 0)) == 1) {
	NEW (extra, act_dataflow_element);
	extra->t = ACT_DFLOW_FUNC;
	extra->u.func.init = NULL;
	extra->u.func.istransparent = 0;
	extra->u.func.nbufs = NULL;
	if (e->t == ACT_DFLOW_MERGE) {
	  extra->u.func.rhs = e->u.splitmerge.multi[i]->Clone ();
	  extra->u.func.lhs = _varexpr (orig_ids[loc++]);
	}
	else {
	  extra->u.func.lhs = _varexpr (e->u.splitmerge.multi[i]->Clone ());
	  extra->u.func.rhs = orig_ids[loc++];
	}
	list_append (l, extra);
	running_sum++;
	continue;
      }

      NEW (extra, act_dataflow_element);
      extra->t = e->t;
      extra->u.splitmerge.nondetctrl = NULL;
      extra->u.splitmerge.single = e->u.splitmerge.multi[i]->Clone();
      extra->u.splitmerge.nmulti = group_sz + ((group_mod > i) ? 1 : 0);
      MALLOC (extra->u.splitmerge.multi, ActId *, extra->u.splitmerge.nmulti);
      for (int j=0; j < extra->u.splitmerge.nmulti; j++) {
	extra->u.splitmerge.multi[j] = orig_ids[loc++];
      }
      _sc->findFresh ("_splmrg", &_idx);
      idx[chpos++] = _idx;
      snprintf (buf, 1024, "_splmrg%d", _idx);
      int bw = _ceil_log2 (extra->u.splitmerge.nmulti);
      
      it = TypeFactory::Factory()->NewInt (_sc, Type::direction::NONE, 0,
					   const_expr (bw));
      it = TypeFactory::Factory()->NewChan (_sc, Type::direction::NONE,
					    it, NULL);
      it->MkCached ();
      it = it->Expand (NULL, _sc);
      _sc->Add (buf, it);
      extra->u.splitmerge.guard = new ActId (string_cache (buf));

      list_append (l, extra);

      // now we add the conversion function!
      NEW (extra, act_dataflow_element);
      extra->t = ACT_DFLOW_FUNC;
      extra->u.func.init = NULL;
      extra->u.func.istransparent = 0;
      extra->u.func.nbufs = NULL;
      extra->u.func.rhs = new ActId (string_cache (buf));
      NEW (extra->u.func.lhs, Expr);
      extra->u.func.lhs->type = E_BUILTIN_INT;

      snprintf (buf, 1024, "_splmrg%d", idx[i]);

      extra->u.func.lhs->u.e.l = _minus (new ActId (buf), running_sum);
      running_sum += group_sz + (group_mod > i ? 1 : 0);
      extra->u.func.lhs->u.e.r = const_expr (bw);

      tmp = extra->u.func.lhs;
      extra->u.func.lhs = expr_expand (tmp, NULL, _sc, ACT_EXPR_EXFLAG_CHPEX);
      _myfree (tmp);
      
      list_append (l, extra);
    }
    

    FREE (orig_ids);
    FREE (idx);
  }
}


int ActDflowSplitMerge::run (Process *p)
{
  return ActPass::run (p);
}
