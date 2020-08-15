/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2020 Rajit Manohar
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
#include <string.h>
#include <map>
#include <utility>
#include <config.h>
#include <math.h>
#include <act/passes/statepass.h>
#include <act/passes/sizing.h>

static int getconst_int (Expr *e, int *val)
{
  if (!e) return 0;
  if (e->type == E_INT) {
    *val = e->u.v;
  }
  else if (e->type == E_REAL) {
    *val = e->u.f;
  }
  else {
    return 0;
  }
  return 1;
}

static int getconst_real (Expr *e, double *val)
{
  if (!e) return 0;
  if (e->type == E_INT) {
    *val = e->u.v;
  }
  else if (e->type == E_REAL) {
    *val = e->u.f;
  }
  else {
    return 0;
  }
  return 1;
}

int _no_sizing (act_prs_expr_t *e)
{
  if (!e) return 1;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    return _no_sizing (e->u.e.l) && _no_sizing (e->u.e.r) &&
      _no_sizing (e->u.e.pchg);
    break;
    
  case ACT_PRS_EXPR_NOT:
    return _no_sizing (e->u.e.l);
    break;

  case ACT_PRS_EXPR_VAR:
    if (e->u.v.sz) {
      return 0;
    }
    else {
      return 1;
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    return 0;
    break;
    
  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    return 1;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
  default:
    fatal_error ("What?");
    break;
  }
  return 0;
}

static std::map<act_prs_expr_t *, int> *_depthmap = NULL;

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (a) : (b))
#endif

static void _depth_map (act_prs_expr_t *e, int pad, int flip = 0)
{
  if (!e) return;

  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    if ((e->type == ACT_PRS_EXPR_AND && flip == 0) ||
	(e->type == ACT_PRS_EXPR_OR && flip == 1)) {
      /* and */
      _depth_map (e->u.e.l, pad, flip);
      _depth_map (e->u.e.r, (*_depthmap)[e->u.e.l], flip);
      (*_depthmap)[e] = (*_depthmap)[e->u.e.r];
    }
    else {
      /* or */
      _depth_map (e->u.e.l, pad, flip);
      _depth_map (e->u.e.r, pad, flip);
      (*_depthmap)[e] = MAX((*_depthmap)[e->u.e.l], (*_depthmap)[e->u.e.r]);
    }
    break;
    
  case ACT_PRS_EXPR_NOT:
    _depth_map (e->u.e.l, pad, 1 - flip);
    (*_depthmap)[e] = (*_depthmap)[e->u.e.l];
    break;

  case ACT_PRS_EXPR_VAR:
    (*_depthmap)[e] = 1 + pad;
    break;

  case ACT_PRS_EXPR_LABEL:
  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
  default:
    fatal_error ("What?");
    break;
  }
  return;
}

static int allow_long_channel;
static int min_width;
static int std_n_length, std_p_length;
static int is_p_type;

static void _in_place_sizing (act_prs_expr_t *e, int flav, double sz, int nf,
			      int flip = 0)
{
  if (!e) return;

  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    if ((e->type == ACT_PRS_EXPR_AND && flip == 0) ||
	(e->type == ACT_PRS_EXPR_OR && flip == 1)) {
      /* and */
      int d1, d2;
      d1 = (*_depthmap)[e->u.e.l];
      d2 = (*_depthmap)[e->u.e.r];
#if 0
      if (d2 <= d1) {
	printf ("depth d1=%d, d2=%d; flip=%d\n", d1, d2, flip);
	act_print_prs_expr (stdout, e->u.e.l);
	printf ("\n");
	act_print_prs_expr (stdout, e->u.e.r);
	printf ("\n");
      }
#endif      
      Assert (d2 > d1, "What?");
      _in_place_sizing (e->u.e.l, flav, sz*d2/d1, nf, flip);
      _in_place_sizing (e->u.e.r, flav, sz*d2/(d2-d1), nf, flip);
    }
    else {
      /* or */
      _in_place_sizing (e->u.e.l, flav, sz, nf, flip);
      _in_place_sizing (e->u.e.r, flav, sz, nf, flip);
    }
    break;
    
  case ACT_PRS_EXPR_NOT:
    _in_place_sizing (e->u.e.l, flav, sz, nf, 1 - flip);
    break;

  case ACT_PRS_EXPR_VAR:
    /* add sizing */
    NEW (e->u.v.sz, act_size_spec_t);
    e->u.v.sz->l = NULL;
    e->u.v.sz->flavor = flav;
    if (allow_long_channel && ((int)(sz+0.5) < min_width)) {
      int w, l;
      w = min_width;
      if (is_p_type) {
	l = (0.5 + (min_width+0.0)*(std_p_length)/(sz+0.0));
      }
      else {
	l = (0.5 + (min_width+0.0)*(std_n_length)/(sz+0.0));
      }
      e->u.v.sz->l = const_expr (l);
      e->u.v.sz->w = const_expr (min_width);
    }
    else {
      e->u.v.sz->w = const_expr ((int)(sz + 0.5));
    }
    if (nf != 1) {
      e->u.v.sz->folds = const_expr (nf);
    }
    else {
      e->u.v.sz->folds = NULL;
    }
    break;

  case ACT_PRS_EXPR_LABEL:
  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
  default:
    fatal_error ("What?");
    break;
  }
  return;
}

static  void _do_sizing (act_prs_expr_t *e, int flav, double sz, int nf)
{
  _depthmap = new std::map<act_prs_expr_t *, int> ();
  _depth_map (e, 0);
  _in_place_sizing (e, flav, sz, nf);
  delete _depthmap;
  _depthmap = NULL;
}


static void _apply_sizing (act_connection *c, int flav_up, int flav_dn,
			   double up, int upf,
			   double dn, int dnf, double ratio,
			   act_prs_lang_t *prs, Scope *sc)
{
  act_prs_lang_t *tmp;
  act_connection *cid;
  for (tmp = prs; tmp; tmp = tmp->next) {
    switch (tmp->type) {
    case ACT_PRS_RULE:
      if (!tmp->u.one.label) {
	cid = tmp->u.one.id->Canonical (sc);
	if (cid == c) {
	  if (_no_sizing (tmp->u.one.e)) {
	    if (tmp->u.one.arrow_type == 0) {
	      is_p_type = tmp->u.one.dir;
	      _do_sizing (tmp->u.one.e,
			  tmp->u.one.dir ? flav_up : flav_dn,
			  tmp->u.one.dir ? up : dn,
			  tmp->u.one.dir ? upf : dnf);
	    }
	    else {
	      /* create a new production rule and size both */
	      act_prs_lang_t *newrule;
	      NEW (newrule, act_prs_lang_t);
	      newrule->type = ACT_PRS_RULE;
	      newrule->u.one.id = tmp->u.one.id;
	      newrule->u.one.attr = NULL;
	      newrule->u.one.arrow_type = 0;
	      newrule->u.one.dir = 1 - tmp->u.one.dir;
	      newrule->u.one.label = 0;
	      if (tmp->u.one.arrow_type == 1) {
		newrule->u.one.e = act_prs_complement_rule (tmp->u.one.e);
	      }
	      else {
		newrule->u.one.e = act_prs_celement_rule (tmp->u.one.e);
	      }
	      newrule->next = tmp->next;
	      tmp->next = newrule;
	      tmp->u.one.arrow_type = 0;
	      
	      is_p_type = tmp->u.one.dir;
	      _do_sizing (tmp->u.one.e,
			  tmp->u.one.dir ? flav_up : flav_dn,
			  tmp->u.one.dir ? up : dn,
			  tmp->u.one.dir ? upf : dnf);

	      is_p_type = newrule->u.one.dir;
	      _do_sizing (newrule->u.one.e,
			  newrule->u.one.dir ? flav_up : flav_dn,
			  newrule->u.one.dir ? up : dn,
			  newrule->u.one.dir ? upf : dnf);

	      tmp = newrule;
	    }
	  }
	  else {
	    warning ("Sizing directive {+%g,-%g} specified for already sized rule", up/ratio, dn);
	    fprintf (stderr, "\tskipped signal: `");
	    tmp->u.one.id->Print (stderr);
	    fprintf (stderr, "'\n");
	  }
	}
      }
      break;
    case ACT_PRS_GATE:
      /* no tgate sizing here */
      break;
    case ACT_PRS_SUBCKT:
    case ACT_PRS_TREE:
      _apply_sizing (c, flav_up, flav_dn,
		     up, upf, dn, dnf, ratio, tmp->u.l.p, sc);
      break;
    case ACT_PRS_LOOP:
      fatal_error ("Why are there loops?");
      break;
    default:
      fatal_error ("What?");
    }
  }
}
	

void *ActSizingPass::local_op (Process *p, int mode)
{
  act_prs *prs;
  act_sizing *sz;
  act_boolean_netlist_t *bn;

  Assert (mode == 0, "What?");
  
  if (p) {
    prs = p->getlang()->getprs();
    sz = p->getlang()->getsizing();
  }
  else {
    prs = ActNamespace::Global()->getprs();
    sz = ActNamespace::Global()->getlang()->getsizing();
  }

  if (!prs || !sz) return NULL;

  bn = bp->getBNL (p);
  if (!bn) {
    return NULL;
  }

  /* 
     we have a sizing body, plus production rules; now apply the
     sizing info 
  */

  int p_n_mode;
  int unit_n;

  if (sz->p_specified) {
    if (!getconst_int (sz->p_n_mode_e, &p_n_mode)) {
      fatal_error ("In `%s': p_n_mode expression is not a const",
		   p ? p->getName() : "-toplevel-");
    }
  }
  else {
    if (config_exists ("net.sizing.p_n_mode")) {
      p_n_mode = config_get_int ("net.sizing.p_n_mode");
    }
    else {
      p_n_mode = 0;
    }
  }
  if (sz->unit_n_specified) {
    if (!getconst_int (sz->unit_n_e, &unit_n)) {
      fatal_error ("In `%s': unit_n expression is not a const",
		   p ? p->getName() : "-toplevel-");
    }
  }
  else {
    if (config_exists ("net.sizing.unit_n")) {
      unit_n = config_get_int ("net.sizing.unit_n");
    }
    else {
      unit_n = 5;
    }
  }
  if (p_n_mode != 0 && p_n_mode != 1) {
    fatal_error ("In `%s': p_n_mode (%d) is not 0 or 1", p_n_mode,
		 p ? p->getName() : "-toplevel-");
  }
  if (unit_n < 1) {
    fatal_error ("In `%s': unit_n (%d) is less than 1 ", unit_n,
		 p ? p->getName() : "-toplevel-");
  }

  double ratio;
  ActNamespace *ns;
  Scope *sc;

  ratio = config_get_real ("net.p_n_ratio");
  if (p_n_mode == 1) {
    ratio = sqrt (ratio);
  }

  if (!p) {
    sc = ActNamespace::Global()->CurScope();
  }
  else {
    sc = p->CurScope();
  }

  if (config_exists ("net.sizing.use_long_channel")) {
    allow_long_channel = config_get_int ("net.sizing.use_long_channel");
    min_width = config_get_int ("net.min_width");
    std_p_length = config_get_int ("net.std_p_length");
    std_n_length = config_get_int ("net.std_n_length");
  }
  else {
    allow_long_channel = 0;
  }
    
  for (int i=0; i < A_LEN (sz->d); i++) {
    /* apply directive! */
    act_connection *c;
    double dup, ddn;
    int upf, dnf;
    int fup, fdn;

    upf = 1;
    dnf = 1;
    fup = 0;
    fdn = 0;
    
    c = sz->d[i].id->Canonical (sc);
    fup = sz->d[i].flav_up;
    fdn = sz->d[i].flav_dn;

    if (sz->d[i].eup) {
      if (!getconst_real (sz->d[i].eup, &dup)) {
	fatal_error ("Sizing expression is not a const");
      }
      if (dup < 0) {
	fatal_error ("Sizing expression is negative");
      }
      dup *= ratio;		/* keep the units fixed */
    }
    else {
      dup = -1;
    }
    if (sz->d[i].upfolds) {
      if (!getconst_int (sz->d[i].upfolds, &upf)) {
	fatal_error ("Sizing folding value is not a const");
      }
      if (upf < 0) {
	fatal_error ("Sizing folding is negative");
      }
    }
    if (sz->d[i].edn) {
      if (!getconst_real (sz->d[i].edn, &ddn)) {
	fatal_error ("Sizing expression is not a const");
      }
      if (ddn < 0) {
	fatal_error ("Sizing expression is negative");
      }
    }
    else {
      ddn = -1;
    }
    if (sz->d[i].dnfolds) {
      if (!getconst_int (sz->d[i].dnfolds, &dnf)) {
	fatal_error ("Sizing folding value is not a const");
      }
      if (dnf < 0) {
	fatal_error ("Sizing folding is negative");
      }
    }
    if (dup == -1) {
      dup = ddn*ratio;
      upf = dnf;
      fup = fdn;
    }
    if (ddn == -1) {
      ddn = dup/ratio;
      dnf = upf;
      fdn = fup;
    }
    dup *= unit_n;
    ddn *= unit_n;

    /* find all rules with this and apply sizing */
    for (act_prs *tprs = prs; tprs; tprs = tprs->next) {
      _apply_sizing (c, fup, fdn, dup, upf, ddn, dnf, ratio, tprs->p, sc);
    }
  }
  return NULL;
}


ActSizingPass::ActSizingPass (Act *a) : ActPass (a, "sizing")
{
  /*-- need the booleanize pass --*/
  ActPass *ap = a->pass_find ("booleanize");
  if (!ap) {
    bp = new ActBooleanizePass (a);
  }
  else {
    bp = dynamic_cast<ActBooleanizePass *>(ap);
    Assert (bp, "What?");
  }
  AddDependency ("booleanize");
}

void ActSizingPass::free_local (void *v)
{
  
}

int ActSizingPass::run (Process *p)
{
  return ActPass::run (p);
}
