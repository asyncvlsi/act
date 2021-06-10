/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#include <act/types.h>
#include <act/lang.h>
#include <act/inst.h>
#include <act/body.h>
#include <act/value.h>
#include "prs.h"
#include <common/qops.h>
#include <common/config.h>
#include <string.h>

/*--- 
  chp expressions have a slightly different expansion, because
  they might use real arrays
 *---
 */
static Expr *chp_expr_expand (Expr *e, ActNamespace *ns, Scope *s)
{
  return expr_expand (e, ns, s,
		      ACT_EXPR_EXFLAG_PARTIAL|ACT_EXPR_EXFLAG_CHPEX);
}

void act_chp_free (act_chp_lang_t *c)
{
  /* XXX FIXME */
}

act_prs_lang_t *prs_expand (act_prs_lang_t *, ActNamespace *, Scope *);

act_size_spec_t *act_expand_size (act_size_spec_t *sz, ActNamespace *ns, Scope *s);

static ActId *fullexpand_var (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;
  
  idtmp = id->Expand (ns, s);
  
  etmp = idtmp->Eval (ns, s);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}

/*
 * This is a chp variable to be read. So it can have array
 * dereferences
 */
static ActId *expand_var_read (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;

  /* this needs to be changed  to: expand as much as possible, not
     full expand. It needs to be a non-array variable type.
  */
  idtmp = id->Expand (ns, s);
  
  etmp = idtmp->Eval (ns, s);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}

/*
 * This is a chp variable to be written. So it can have array
 * dereferences
 */
ActId *expand_var_write (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;

  /* this needs to be changed  to: expand as much as possible, not
     full expand. It needs to be a non-array variable type.
  */
  idtmp = id->ExpandCHP (ns, s);
  
  etmp = idtmp->EvalCHP (ns, s, 1);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}


static ActId *expand_var_chan (ActId *id, ActNamespace *ns, Scope *s)
{
  return expand_var_read (id, ns, s);
}

void _merge_duplicate_rules (act_prs_lang_t *p)
{
  /* XXX: fix this */
}

act_prs *prs_expand (act_prs *p, ActNamespace *ns, Scope *s)
{
  act_prs *ret;

  if (!p) return NULL;

  NEW (ret, act_prs);

  ret->vdd = fullexpand_var (p->vdd, ns, s);
  ret->gnd = fullexpand_var (p->gnd, ns, s);
  ret->psc = fullexpand_var (p->psc, ns, s);
  ret->nsc = fullexpand_var (p->nsc, ns, s);
  ret->p = prs_expand (p->p, ns, s);
  ret->leak_adjust = p->leak_adjust;
  _merge_duplicate_rules (ret->p);
 
  if (p->next) {
    ret->next = prs_expand (p->next, ns, s);
  }
  else {
    ret->next = NULL;
  }

  return ret;
}


act_prs_expr_t *prs_expr_expand (act_prs_expr_t *p, ActNamespace *ns, Scope *s)
{
  int pick;
  
  act_prs_expr_t *ret = NULL;
  if (!p) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = p->type;
  switch (p->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l = prs_expr_expand (p->u.e.l, ns, s);
    ret->u.e.r = prs_expr_expand (p->u.e.r, ns, s);
    ret->u.e.pchg = prs_expr_expand (p->u.e.pchg, ns, s);
    ret->u.e.pchg_type = p->u.e.pchg_type;

    pick = -1;

    if (ret->u.e.l->type == ACT_PRS_EXPR_TRUE) {
      /* return ret->u.e.r; any pchg is skipped */
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 0; /* RIGHT */
      }
      else {
	pick = 1; /* LEFT */
      }
    }
    else if (ret->u.e.l->type == ACT_PRS_EXPR_FALSE) {
	if (p->type == ACT_PRS_EXPR_AND) {
	  pick = 1; /* LEFT */
	}
	else {
	  pick = 0; /* RIGHT */
	}
    }
    else if (ret->u.e.r->type == ACT_PRS_EXPR_TRUE) {
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 1; /* LEFT */
      }
      else {
	pick = 0; /* RIGHT */
      }
    }
    else if (ret->u.e.r->type == ACT_PRS_EXPR_FALSE) {
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 0; /* RIGHT */
      }
      else {
	pick = 1; /* LEFT */
      }
    }
    if (pick == 0) {
      act_prs_expr_t *t;
      /* RIGHT */
      act_free_a_prs_exexpr (ret->u.e.l);
      act_free_a_prs_exexpr (ret->u.e.pchg);
      t = ret->u.e.r;
      FREE (ret);
      ret = t;
    }
    else if (pick == 1) {
      act_prs_expr_t *t;
      /* LEFT */
      act_free_a_prs_exexpr (ret->u.e.r);
      act_free_a_prs_exexpr (ret->u.e.pchg);
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    break;

  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = prs_expr_expand (p->u.e.l, ns, s);
    ret->u.e.r = NULL;
    if (ret->u.e.l->type == ACT_PRS_EXPR_FALSE) {
      act_prs_expr_t *t;
      ret->u.e.l->type = ACT_PRS_EXPR_TRUE;
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    else if (ret->u.e.l->type == ACT_PRS_EXPR_TRUE) {
      act_prs_expr_t *t;
      ret->u.e.l->type = ACT_PRS_EXPR_FALSE;
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l.label = p->u.l.label;
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v.sz = act_expand_size (p->u.v.sz, ns, s);
    ret->u.v.id = expand_var_read (p->u.v.id, ns, s);
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    {
      ValueIdx *vx;
      int ilo, ihi, i;

      act_syn_loop_setup (ns, s, p->u.loop.id, p->u.loop.lo, p->u.loop.hi,
			  &vx, &ilo, &ihi);
      
      if (ihi < ilo) {
	/* empty */
	if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	  ret->type = ACT_PRS_EXPR_TRUE;
	}
	else {
	  ret->type = ACT_PRS_EXPR_FALSE;
	}
      }
      else {
	FREE (ret);
	ret = NULL;

	for (i=ilo; i <= ihi; i++) {
	  act_prs_expr_t *at;

	  s->setPInt (vx->u.idx, i);
	  
	  at = prs_expr_expand (p->u.loop.e, ns, s);

	  if (!ret) {
	    ret = at;
	    if (ret->type == ACT_PRS_EXPR_TRUE) {
	      if (p->type == ACT_PRS_EXPR_ANDLOOP) {
		act_free_a_prs_exexpr (ret);
		ret = NULL;
	      }
	      else {
		break;
	      }
	    }
	    else if (ret->type == ACT_PRS_EXPR_FALSE) {
	      if (p->type == ACT_PRS_EXPR_ANDLOOP) {
		break;
	      }
	      else {
		act_free_a_prs_exexpr (ret);
		ret = NULL;
	      }
	    }
	  }
	  else if (at->type == ACT_PRS_EXPR_TRUE) {
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      /* nothing */
	    }
	    else {
	      /* we're done! */
	      act_free_a_prs_exexpr (ret);
	      ret = at;
	      break;
	    }
	  }
	  else if (at->type == ACT_PRS_EXPR_FALSE) {
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      /* we're done */
	      act_free_a_prs_exexpr (ret);
	      ret = at;
	      break;
	    }
	    else {
	      /* nothing */
	    }
	  }
	  else {
	    /* ok now & and | combine! */
	    act_prs_expr_t *tmp;
	    NEW (tmp, act_prs_expr_t);
	    tmp->u.e.l = ret;
	    tmp->u.e.r = at;
	    tmp->u.e.pchg = NULL;
	    tmp->u.e.pchg_type = -1;
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      tmp->type = ACT_PRS_EXPR_AND;
	    }
	    else {
	      tmp->type = ACT_PRS_EXPR_OR;
	    }
	    ret= tmp;
	  }
	}
	if (ret == NULL) {
	  NEW (ret, act_prs_expr_t);
	  if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	    ret->type = ACT_PRS_EXPR_TRUE;
	  }
	  else {
	    ret->type = ACT_PRS_EXPR_FALSE;
	  }
	}
      }
      act_syn_loop_teardown (ns, s, p->u.loop.id, vx);
    }
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;
    
  default:
    Assert (0, "Eh?");
    break;
  }
  
  
  return ret;
}

static int current_attr_num;
static  char **current_attr_table;
static act_attr_t *attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  act_attr_t *hd = NULL, *tl = NULL, *tmp;
  int pos;

  while (a) {
    NEW (tmp, act_attr_t);
    tmp->attr = a->attr;

    for (pos = 0; pos < current_attr_num; pos++) {
      if (strcmp (current_attr_table[pos]+4, a->attr) == 0) break;
    }
    Assert (pos != current_attr_num, "What?");
    tmp->e = expr_expand (a->e, ns, s);

    if (current_attr_table[pos][0] == 'i' && tmp->e->type != E_INT) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Integer attribute %s is assigned a non-integer/non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "  evaluated to: ");
      print_expr (stderr, tmp->e);
      fprintf (stderr, "\n");
      exit (1);
    } else if (current_attr_table[pos][0] == 'r' && tmp->e->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Real attribute %s is assigned a non-real/non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "  evaluated to: ");
      print_expr (stderr, tmp->e);
      fprintf (stderr, "\n");
      exit (1);
    } else if (current_attr_table[pos][0] == 'b' &&
	       (tmp->e->type != E_TRUE && tmp->e->type != E_FALSE)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Boolean attribute %s is assigned a non-Boolean/non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "  evaluated to: ");
      print_expr (stderr, tmp->e);
      fprintf (stderr, "\n");
      exit (1);
    }
    q_ins (hd, tl, tmp);
    a = a->next;
  }
  return hd;
}


act_attr_t *prs_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  current_attr_num = config_get_table_size ("act.prs_attr");
  current_attr_table = config_get_table_string ("act.prs_attr");
  return attr_expand (a, ns, s);
}

act_attr_t *inst_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  current_attr_num = config_get_table_size ("act.instance_attr");
  current_attr_table = config_get_table_string ("act.instance_attr");
  return attr_expand (a, ns, s);
}

act_size_spec_t *act_expand_size (act_size_spec_t *sz, ActNamespace *ns, Scope *s)
{
  act_size_spec_t *ret;
  if (!sz) return NULL;

  NEW (ret, act_size_spec_t);
  if (sz->w) {
    ret->w = expr_expand (sz->w, ns, s);
    if (ret->w->type != E_INT && ret->w->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size expression is not of type pint or preal\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->w);
      fprintf (stderr, "\n");
      exit (1);
   }
  }
  else {
    ret->w = NULL;
  }
  if (sz->l) {
    ret->l = expr_expand (sz->l, ns, s);
    if (ret->l->type != E_INT && ret->l->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size expression is not of type pint or preal\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->l);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  else {
    ret->l = NULL;
  }
  if (sz->folds) {
    ret->folds = expr_expand (sz->folds, ns, s);
    if (ret->folds->type != E_INT) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size folding amount is not a pint\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->folds);
      exit (1);
    }
  }
  else {
    ret->folds = NULL;
  }
  ret->flavor = sz->flavor;
  
  return ret;
}

struct act_prsmerge {
  act_prs_lang_t *vup;		/* previous prs */
  act_prs_lang_t *vdn;		/* previous prs */
};
  
  
act_prs_lang_t *prs_expand (act_prs_lang_t *p, ActNamespace *ns, Scope *s)
{
  act_prs_lang_t *hd = NULL;
  act_prs_lang_t *tl = NULL;
  act_prs_lang_t *tmp;
  ActId *idtmp;
  Expr *etmp;

  while (p) {
    /* expand one prs */
    NEW (tmp, act_prs_lang_t);
    tmp->type = p->type;
    tmp->next = NULL;
    switch (p->type) {
    case ACT_PRS_RULE:
      tmp->u.one.attr = prs_attr_expand (p->u.one.attr, ns, s);
      tmp->u.one.e = prs_expr_expand (p->u.one.e, ns, s);
      if (p->u.one.label == 0) {
	act_connection *ac;
	
	idtmp = p->u.one.id->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hmm");
	tmp->u.one.id = (ActId *)etmp->u.e.l;
	FREE (etmp);

	ac = idtmp->Canonical (s);
	if (ac->getDir() == Type::IN) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "\t");
	  idtmp->Print (stderr);
	  fprintf (stderr, " has a directional type that is not writable.\n");
	  fatal_error ("A `bool?' cannot be on the RHS of a production rule.");
	}
      }
      else {
	/* it is a char* */
	tmp->u.one.id = p->u.one.id;
	if (p->u.one.arrow_type != 0) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "@-expressions must only use -> arrows");
	  exit (1);
	}
      }
      tmp->u.one.arrow_type = p->u.one.arrow_type;
      tmp->u.one.dir = p->u.one.dir;
      tmp->u.one.label = p->u.one.label;
      break;
      
    case ACT_PRS_GATE:
      tmp->u.p.attr = prs_attr_expand (p->u.p.attr, ns, s);
      idtmp = p->u.p.s->Expand (ns, s);
      etmp = idtmp->Eval (ns, s);
      Assert (etmp->type == E_VAR, "Hm");
      tmp->u.p.s = (ActId *)etmp->u.e.l;
      FREE (etmp);

      idtmp = p->u.p.d->Expand (ns, s);
      etmp = idtmp->Eval (ns, s);
      Assert (etmp->type == E_VAR, "Hm");
      tmp->u.p.d = (ActId *)etmp->u.e.l;
      FREE (etmp);

      if (p->u.p.g) {
	idtmp = p->u.p.g->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hm");
	tmp->u.p.g = (ActId *)etmp->u.e.l;
	FREE (etmp);
      }
      else {
	tmp->u.p.g = NULL;
      }

      if (p->u.p._g) {
	idtmp = p->u.p._g->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hm");
	tmp->u.p._g = (ActId *)etmp->u.e.l;
	FREE (etmp);
      }
      else {
	tmp->u.p._g = NULL;
      }
      tmp->u.p.sz = act_expand_size (p->u.p.sz, ns, s);
      break;
      
    case ACT_PRS_LOOP:
      Assert (s->Add (p->u.l.id, TypeFactory::Factory()->NewPInt()),
	      "Should have been caught earlier");
      {
	ValueIdx *vx;
	int ilo, ihi, i;
	act_prs_lang_t *px, *pxrethd, *pxrettl;
	Expr *etmp;
	
	if (p->u.l.lo) {
	  etmp = expr_expand (p->u.l.lo, ns, s);
	  if (etmp->type != E_INT) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	    print_expr (stderr, p->u.l.lo);
	    fprintf (stderr, "\nNot a constant int\n");
	    exit (1);
	  }
	  ilo = etmp->u.v;
	  //FREE (etmp);
	}
	else {
	  ilo = 0;
	}
	etmp = expr_expand (p->u.l.hi, ns, s);
	if (etmp->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	  print_expr (stderr, p->u.l.hi);
	  fprintf (stderr, "\nNot a constant int\n");
	  exit (1);
	}
	if (p->u.l.lo) {
	  ihi = etmp->u.v;
	}
	else {
	  ihi = etmp->u.v-1;
	}
	//FREE (etmp);

	vx = s->LookupVal (p->u.l.id);
	vx->init = 1;
	vx->u.idx = s->AllocPInt();

	pxrethd = NULL;
	pxrettl = NULL;
	for (i = ilo; i <= ihi; i++) {
	  s->setPInt (vx->u.idx, i);
	  px = prs_expand (p->u.l.p, ns, s);
	  if (!pxrethd) {
	    pxrethd = px;
	    pxrettl = pxrethd;
	    while (pxrettl->next) {
	      pxrettl = pxrettl->next;
	    }
	  }
	  else {
	    pxrettl->next = px;
	    while (pxrettl->next) {
	      pxrettl = pxrettl->next;
	    }
	  }
	}
	FREE (tmp);
	tmp = pxrethd;
	s->DeallocPInt (vx->u.idx, 1);
      }
      s->Del (p->u.l.id);
      break;
      
    case ACT_PRS_TREE:
      if (p->u.l.lo) {
	etmp = expr_expand (p->u.l.lo, ns, s);
	if (etmp->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "tree<> expression is not of type pint\n");
	  fprintf (stderr, " expr: ");
	  print_expr (stderr, p->u.l.lo);
	  fprintf (stderr, "\n");
	  exit (1);
	}
      }
      else {
	etmp = NULL;
      }
      tmp->u.l.lo = etmp;
      tmp->u.l.hi = NULL;
      tmp->u.l.id = NULL;
      tmp->u.l.p = prs_expand (p->u.l.p, ns, s);
      break;
      
    case ACT_PRS_SUBCKT:
      tmp->u.l.id = p->u.l.id;
      tmp->u.l.lo = NULL;
      tmp->u.l.hi = NULL;
      tmp->u.l.p = prs_expand (p->u.l.p, ns, s);
      break;
      
    default:
      Assert (0, "Should not be here");
      break;
    }
    if (tmp) {
      if (!hd) {
	hd = tmp;
	tl = tmp;
      }
      else {
	tl->next = tmp;
      }
      while (tl->next) {
	tl = tl->next;
      }
    }
    p = q_step (p);
  }
  return hd;
}

act_chp *chp_expand (act_chp *c, ActNamespace *ns, Scope *s)
{
  act_chp *ret;
  if (!c) return NULL;

  NEW (ret, act_chp);
  
  ret->vdd = fullexpand_var (c->vdd, ns, s);
  ret->gnd = fullexpand_var (c->gnd, ns, s);
  ret->psc = fullexpand_var (c->psc, ns, s);
  ret->nsc = fullexpand_var (c->nsc, ns, s);
  _act_chp_is_synth_flag = 1;
  ret->c = chp_expand (c->c, ns, s);
  ret->is_synthesizable = _act_chp_is_synth_flag;

  return ret;
}

static int _has_probe (Expr *e)
{
  if (!e) return 0;
  switch (e->type) {
  case E_AND:
  case E_OR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    return _has_probe (e->u.e.l) || _has_probe (e->u.e.r);
    break;
    
  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    return _has_probe (e->u.e.l);
    break;

  case E_QUERY:
    return _has_probe (e->u.e.l) ||
      _has_probe (e->u.e.r->u.e.l) ||
      _has_probe (e->u.e.r->u.e.r);
    break;
    
  case E_FUNCTION:
    {
      Expr *tmp = e->u.fn.r;
      Assert (!tmp || tmp->type == E_LT, "Function elaboration?");
      while (tmp) {
	if (_has_probe (tmp->u.e.l)) {
	  return 1;
	}
	tmp = tmp->u.e.r;
      }
    }
    return 0;
    break;

  case E_PROBE:
    return 1;
    break;

  case E_VAR:
  case E_TRUE:
  case E_FALSE:
  case E_INT:
    return 0;
    break;

  case E_BITFIELD:
    return _has_probe (e->u.e.r->u.e.l) ||
      _has_probe (e->u.e.r->u.e.r);
    break;
    
  case E_CONCAT:
    while (e) {
      if (_has_probe (e->u.e.l)) {
	return 1;
      }
      e = e->u.e.r;
    }
    return 0;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    return _has_probe (e->u.e.l);
    break;

  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    return 1;
    break;
  }
}
    

static Expr *_chp_fix_nnf (Expr *e, int invert)
{
  Expr *t;

  if (!e) return e;
  
  switch (e->type) {
  case E_AND:
  case E_OR:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, invert);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, invert);
    if (invert) {
      if (e->type == E_AND) {
	e->type = E_OR;
      }
      else {
	e->type = E_AND;
      }
    }
    break;

  case E_NOT:
    t = _chp_fix_nnf (e->u.e.l, 1-invert);
    FREE (e);
    e = t;
    break;

  case E_XOR:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, invert);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    break;

  case E_LT:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_GE;
    break;
    
  case E_GT:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_LE;
    break;
    
  case E_LE:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_GT;
    break;
    
  case E_GE:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_LT;
    break;

  case E_EQ:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_NE;
    break;
    
  case E_NE:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    if (invert) e->type = E_EQ;
    break;
    
    
  case E_COMPLEMENT:
  case E_UMINUS:
    Assert (invert == 0, "What?");
    e->u.e.l = _chp_fix_nnf (e->u.e.l, invert);
    break;

  case E_QUERY:
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r->u.e.l = _chp_fix_nnf (e->u.e.r->u.e.l, invert);
    e->u.e.r->u.e.r = _chp_fix_nnf (e->u.e.r->u.e.r, invert);
    break;
    

  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    if (invert) {
      fatal_error ("Unexpected type %d with inversion?", e->type);
    }
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e->u.e.r = _chp_fix_nnf (e->u.e.r, 0);
    break;
    
  case E_FUNCTION:
    if (invert) {
      NEW (t, Expr);
      t->type = E_NOT;
      t->u.e.l = e;
      t->u.e.r = NULL;
    }
    {
      Expr *tmp = e->u.fn.r;
      Assert (!tmp || tmp->type == E_LT, "Function elaboration?");
      while (tmp) {
	if (_has_probe (tmp->u.e.l)) {
	  warning ("Probes in function arguments not correctly supported");
	}
	tmp = tmp->u.e.r;
      }
    }
    if (invert) {
      e = t;
    }
    break;

  case E_PROBE:
    if (invert) {
      NEW (t, Expr);
      t->type = E_NOT;
      t->u.e.l = e;
      t->u.e.r = NULL;
      e = t;
    }
    break;

  case E_VAR:
    if (invert) {
      NEW (t, Expr);
      t->type = E_NOT;
      t->u.e.l = e;
      t->u.e.r = NULL;
      e = t;
    }
    break;

  case E_BUILTIN_BOOL:
    NEW (t, Expr);
    t->type = E_NOT;
    t->u.e.l = e;
    t->u.e.r = NULL;
    e->u.e.l = _chp_fix_nnf (e->u.e.l, 0);
    e = t;
    break;

  case E_TRUE:
    if (invert) e->type = E_FALSE;
    break;
    
  case E_FALSE:
    if (invert) e->type = E_TRUE;
    break;

  case E_CONCAT:
    if (invert) {
      fatal_error ("Unexpected type %d with inversion?", e->type);
    }
    break;

  case E_BUILTIN_INT:
  case E_BITFIELD:
  case E_INT:
    if (invert) {
      fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    }
    break;
    
  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    break;
  }
  return e;
}

static pHashtable *pmap = NULL;
static int _expr_has_probes = 0;

static Expr *_process_probes (Expr *e)
{
  phash_iter_t iter;
  phash_bucket_t *b;
  
  if (pmap->n > 0) {
    _expr_has_probes = 1;
    phash_iter_init (pmap, &iter);
    while ((b = phash_iter_next (pmap, &iter))) {
      if (b->i == 0) {
	Expr *t;
	act_connection *c = (act_connection *)b->key;
	NEW (t, Expr);
	t->type = E_AND;
	t->u.e.r = e;
	e = t;
	NEW (t->u.e.l, Expr);
	t = t->u.e.l;
	t->type = E_PROBE;
	t->u.e.l = (Expr *)c->toid();
	t->u.e.r = NULL;
      }
    }
    phash_clear (pmap);
  }
  return e;
}


static Expr *_chp_add_probes (Expr *e, ActNamespace *ns, Scope *s, int isbool)
{
  Expr *t;
  phash_bucket_t *b;
  act_connection *c;

  if (!e) return e;
  
  switch (e->type) {
  case E_AND:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, isbool);
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, isbool);
    if (isbool) {
      e = _process_probes (e);
    }
    break;

  case E_OR:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, isbool);
    if (isbool) {
      e->u.e.l = _process_probes (e->u.e.l);
    }
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, isbool);
    if (isbool) {
      e->u.e.r = _process_probes (e->u.e.r);
    }
    break;

  case E_NOT:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, isbool);
    if (isbool) {
      e = _process_probes (e);
    }
    break;

  case E_XOR:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, isbool);
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, isbool);
    if (isbool) {
      e = _process_probes (e);
    }
    break;
    
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 0);
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, 0);
    e = _process_probes (e);
    break;

  case E_COMPLEMENT:
  case E_UMINUS:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 0);
    break;

  case E_QUERY:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 1);
    e->u.e.r->u.e.l = _chp_add_probes (e->u.e.r->u.e.l, ns, s, isbool);
    e->u.e.r->u.e.r = _chp_add_probes (e->u.e.r->u.e.r, ns, s, isbool);
    break;

  case E_PROBE:
    c = ((ActId *)e->u.e.l)->Canonical (s);
    b = phash_lookup (pmap, c);
    if (!b) {
      b = phash_add (pmap, c);
      b->i = 1;
    }
    break;
    
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 0);
    e->u.e.r = _chp_add_probes (e->u.e.r, ns, s, 0);
    break;
    
  case E_FUNCTION:
    /*-- nothing to do --*/
    break;

  case E_BITFIELD:
  case E_VAR:
    /*--  check if this is an channel! --*/
    {
      InstType *it = s->FullLookup ((ActId *)e->u.e.l, NULL);
      if (TypeFactory::isChanType (it)) {
	c = ((ActId *)e->u.e.l)->Canonical (s);
	b = phash_lookup (pmap, c);
	if (b) {
	  /*-- ok has been recorded already --*/
	}
	else {
	  b = phash_add (pmap, c);
	  b->i = 0;		/* 0 = used as a variable */
	}
      }
    }
    break;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    e->u.e.l = _chp_add_probes (e->u.e.l, ns, s, 0);
    break;

  case E_TRUE:
  case E_FALSE:
  case E_INT:
    break;
    
  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    break;
  }
  return e;
}


static Expr *_chp_fix_guardexpr (Expr *e, ActNamespace *ns, Scope *s)
{
  e = _chp_fix_nnf (e, 0);

  pmap = phash_new (4);
  e = _chp_add_probes (e, ns, s, 1);
  if (pmap->n > 0) {
    _expr_has_probes = 1;
  }
  phash_free (pmap);
  return e;
}

act_chp_lang_t *chp_expand (act_chp_lang_t *c, ActNamespace *ns, Scope *s)
{
  act_chp_lang_t *ret;
  act_chp_gc_t *gchd, *gctl, *gctmp, *tmp;
  listitem_t *li;
  ValueIdx *vx;
  int expr_has_else, expr_has_probes;
  
  if (!c) return NULL;
  NEW (ret, act_chp_lang_t);
  ret->label = c->label;
  ret->space = NULL;
  ret->type = c->type;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    ret->u.semi_comma.cmd = list_new ();
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      list_append (ret->u.semi_comma.cmd, chp_expand ((act_chp_lang_t *)list_value (li), ns, s));
    }
    break;

  case ACT_CHP_COMMALOOP:
  case ACT_CHP_SEMILOOP:
    {
      int ilo, ihi;
      act_syn_loop_setup (ns, s, c->u.loop.id, c->u.loop.lo, c->u.loop.hi,
		      &vx, &ilo, &ihi);

      if (c->type == ACT_CHP_COMMALOOP) {
	ret->type = ACT_CHP_COMMA;
      }
      else {
	ret->type = ACT_CHP_SEMI;
      }
      ret->u.semi_comma.cmd = list_new ();
      for (int iter=ilo; iter <= ihi; iter++) {
	s->setPInt (vx->u.idx, iter);
	list_append (ret->u.semi_comma.cmd,
		     chp_expand (c->u.loop.body, ns, s));
	
      }
      act_syn_loop_teardown (ns, s, c->u.loop.id, vx);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    gchd = NULL;
    gctl = NULL;
    expr_has_probes = 0;
    expr_has_else = 0;
    for (gctmp = c->u.gc; gctmp; gctmp = gctmp->next) {
      if (gctmp->id) {
	int ilo, ihi;

	act_syn_loop_setup (ns, s, gctmp->id, gctmp->lo, gctmp->hi,
			&vx, &ilo, &ihi);
	
	for (int iter=ilo; iter <= ihi; iter++) {
	  s->setPInt (vx->u.idx, iter);
	  NEW (tmp, act_chp_gc_t);
	  tmp->next = NULL;
	  tmp->id = NULL;
	  tmp->g = chp_expr_expand (gctmp->g, ns, s);
	  _expr_has_probes = 0;
	  tmp->g = _chp_fix_guardexpr (tmp->g, ns, s);
	  expr_has_probes = _expr_has_probes || expr_has_probes;
	  if (tmp->g && expr_is_a_const (tmp->g) && tmp->g->type == E_FALSE) {
	    FREE (tmp);
	  }
	  else {
	    tmp->s = chp_expand (gctmp->s, ns, s);
	    q_ins (gchd, gctl, tmp);
	  }
	}
	act_syn_loop_teardown (ns, s, gctmp->id, vx);
      }
      else {
	NEW (tmp, act_chp_gc_t);
	tmp->id = NULL;
	tmp->next = NULL;
	tmp->g = chp_expr_expand (gctmp->g, ns, s);
	_expr_has_probes = 0;
	tmp->g = _chp_fix_guardexpr (tmp->g, ns, s);
	expr_has_probes = _expr_has_probes || expr_has_probes;
	if (!tmp->g && gctmp != c->u.gc) {
	  /*-- null guard in head is the loop shortcut --*/
	  expr_has_else = 1;
	}
	if (tmp->g && expr_is_a_const (tmp->g) && tmp->g->type == E_FALSE &&
	    c->type != ACT_CHP_DOLOOP) {
	  FREE (tmp);
	}
	else {
	  tmp->s = chp_expand (gctmp->s, ns, s);
	  q_ins (gchd, gctl, tmp);
	}
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
    if (expr_has_else && expr_has_probes) {
      act_error_ctxt (stderr);
      fatal_error ("Cannot have an else clause when guards contain channels");
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
    ret->u.assign.id = expand_var_write (c->u.assign.id, ns, s);
    ret->u.assign.e = chp_expr_expand (c->u.assign.e, ns, s);
    break;
    
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    ret->u.comm.chan = expand_var_chan (c->u.comm.chan, ns, s);
    ret->u.comm.flavor = c->u.comm.flavor;
    if (c->u.comm.var) {
      ret->u.comm.var = expand_var_write (c->u.comm.var, ns, s);
    }
    else {
      ret->u.comm.var = NULL;
    }
    if (c->u.comm.e) {
      ret->u.comm.e = chp_expr_expand (c->u.comm.e, ns, s);
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
	arg->u.e = chp_expr_expand (ra->u.e, ns, s);
      }
      list_append (ret->u.func.rhs, arg);
    }
    break;
    
  default:
    break;
  }
  return ret;
}


act_spec *spec_expand (act_spec *s, ActNamespace *ns, Scope *sc)
{
  act_spec *ret = NULL;
  act_spec *prev = NULL;
  act_spec *tmp = NULL;

  while (s) {
    NEW (tmp, act_spec);
    tmp->type = s->type;
    tmp->count = s->count;
    tmp->isrequires = s->isrequires;
    MALLOC (tmp->ids, ActId *, tmp->count);
    for (int i=0; i < tmp->count-1; i++) {
      tmp->ids[i] = s->ids[i]->Expand (ns, sc);
    }
    if (!ACT_SPEC_ISTIMING (tmp)) {
      tmp->ids[tmp->count-1] = s->ids[tmp->count-1]->Expand (ns, sc);
    }
    else {
      tmp->ids[tmp->count-1] = (ActId *) (s->ids[tmp->count-1] ?
				expr_expand ((Expr *)s->ids[tmp->count-1],
					     ns, sc) : NULL);
    }
    tmp->extra = s->extra;
    tmp->next = NULL;
    if (prev) {
      prev->next = tmp;
    }
    else {
      ret = tmp;
    }
    prev = tmp;
    s = s->next;
  }
  return ret;
}

const char *act_spec_string (int type)
{
  static int num = -1;
  static char **opts;

  if (num == -1) {
    num = config_get_table_size ("act.spec_types");
    opts = config_get_table_string ("act.spec_types");
  }

  if (type == -1) {
    return "timing";
  }

  if (type < 0 || type >= num) {
    return NULL;
  }
  return opts[type];
}

static void _print_attr (FILE *fp, act_attr_t *a)
{
  fprintf (fp, "[");
  while (a) {
    fprintf (fp, "%s=", a->attr);
    print_expr (fp, a->e);
    if (a->next) {
      fprintf (fp, "; ");
    }
    a = a->next;
  }
  fprintf (fp, "]");
}

static void _print_size (FILE *fp, act_size_spec_t *sz)
{
  if (sz) {
    fprintf (fp, "<");
    if (sz->w) {
      print_expr (fp, sz->w);
      if (sz->l) {
	fprintf (fp, ",");
	print_expr (fp, sz->l);
      }
      if (sz->flavor != 0) {
	fprintf (fp, ",%s", act_dev_value_to_string (sz->flavor));
      }
    }
    if (sz->folds) {
      fprintf (fp, ";");
      print_expr (fp, sz->folds);
    }
    fprintf (fp, ">");
  }
}

void act_print_size (FILE *fp, act_size_spec_t *sz)
{
  _print_size (fp, sz);
}

static void _print_prs_expr (FILE *fp, act_prs_expr_t *e, int prec)
{
  if (!e) return;
 
#define PREC_BEGIN(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (fp, "(");			\
    }						\
  } while (0)

#define PREC_END(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (fp, ")");			\
    }						\
  } while (0)

#define EMIT_BIN(myprec,sym)					\
  do {								\
    PREC_BEGIN(myprec);						\
    _print_prs_expr (fp, e->u.e.l, (myprec));			\
    fprintf (fp, "%s", (sym));					\
    if (e->u.e.pchg) {						\
      fprintf (fp, "{%c", e->u.e.pchg_type ? '+' : '-');	\
      _print_prs_expr (fp, e->u.e.pchg, 0);				\
      fprintf (fp, "}");					\
    }								\
    _print_prs_expr (fp, e->u.e.r, (myprec));			\
    PREC_END (myprec);						\
  } while (0)

#define EMIT_UNOP(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    fprintf (fp, "%s", sym);			\
    _print_prs_expr (fp, e->u.e.l, (myprec));	\
    PREC_END (myprec);				\
  } while (0)
    
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
    EMIT_BIN(3,"&");
    break;
  case ACT_PRS_EXPR_OR:
    EMIT_BIN(2,"|");
    break;
  case ACT_PRS_EXPR_NOT:
    EMIT_UNOP(4, "~");
    break;

  case ACT_PRS_EXPR_TRUE:
    fprintf (fp, "true");
    break;

  case ACT_PRS_EXPR_FALSE:
    fprintf (fp, "false");
    break;

  case ACT_PRS_EXPR_LABEL:
    fprintf (fp, "@%s", e->u.l.label);
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fprintf (fp, "(%c%s:", e->type == ACT_PRS_EXPR_ANDLOOP ? '&' : '|',
	     e->u.loop.id);
    print_expr (fp, e->u.loop.lo);
    if (e->u.loop.hi) {
      fprintf (fp, " .. ");
      print_expr (fp, e->u.loop.hi);
    }
    fprintf (fp, ":");
    _print_prs_expr (fp, e->u.loop.e, 0);
    break;

  case ACT_PRS_EXPR_VAR:
    e->u.v.id->Print (fp);
    if (e->u.v.sz) {
      _print_size (fp, e->u.v.sz);
    }
    break;
  default:
    fatal_error ("What?");
    break;
  }
}

static void _print_one_prs (FILE *fp, act_prs_lang_t *prs)
{
  if (!prs) return;
  switch (prs->type) {
  case ACT_PRS_RULE:
    if (prs->u.one.attr) {
      _print_attr (fp, prs->u.one.attr);
    }
    _print_prs_expr (fp, prs->u.one.e, 0);
    if (prs->u.one.arrow_type == 0) {
      fprintf (fp, " -> ");
    }
    else if (prs->u.one.arrow_type == 1) {
      fprintf (fp, " => ");
    }
    else if (prs->u.one.arrow_type == 2) {
      fprintf (fp, " #> ");
    }
    else {
      fatal_error ("Eh?");
    }
    if (prs->u.one.label) {
      fprintf (fp, "@%s", (char *)prs->u.one.id);
    }
    else {
      prs->u.one.id->Print (fp);
    }
    if (prs->u.one.dir) {
      fprintf (fp, "+\n");
    }
    else {
      fprintf (fp, "-\n");
    }
    break;
  case ACT_PRS_GATE:
    if (prs->u.p.attr) {
      _print_attr (fp, prs->u.p.attr);
    }
    if (prs->u.p.g && prs->u.p._g) {
      fprintf (fp, "transgate");
    }
    else if (prs->u.p.g) {
      fprintf (fp, "passn");
    }
    else if (prs->u.p._g) {
      fprintf (fp, "passp");
    }
    else {
      Assert (0, "Hmm");
    }
    if (prs->u.p.sz) {
      _print_size (fp, prs->u.p.sz);
    }
    fprintf (fp, "(");
    if (prs->u.p.g) {
      prs->u.p.g->Print (fp);
      fprintf (fp, ",");
    }
    if (prs->u.p._g) {
      prs->u.p._g->Print (fp);
      fprintf (fp, ",");
    }
    prs->u.p.s->Print (fp);
    fprintf (fp, ",");
    prs->u.p.d->Print (fp);
    fprintf (fp, ")\n");
    break;
  case ACT_PRS_LOOP:
    fprintf (fp, "(%s:", prs->u.l.id);
    if (prs->u.l.lo) {
      print_expr (fp, prs->u.l.lo);
      fprintf (fp, " .. ");
    }
    print_expr (fp, prs->u.l.hi);
    fprintf (fp, ":");
    for (act_prs_lang_t *i = prs->u.l.p; i; i = i->next) {
      _print_one_prs (fp, i);
    }
    fprintf (fp, ")\n");
    break;
  case ACT_PRS_TREE:
    fprintf (fp, "tree");
    if (prs->u.l.lo) {
      fprintf (fp, "<");
      print_expr (fp, prs->u.l.lo);
      fprintf (fp, ">");
    }
    fprintf (fp, "{\n");
    for (act_prs_lang_t *i = prs->u.l.p; i; i = i->next) {
      _print_one_prs (fp, i);
    }
    fprintf (fp, "}\n");
    break;
  case ACT_PRS_SUBCKT:
    fprintf (fp, "subckt");
    if (prs->u.l.id) {
      fprintf (fp, "<%s>", prs->u.l.id);
    }
    fprintf (fp, "{\n");
    for (act_prs_lang_t *i = prs->u.l.p; i; i = i->next) {
      _print_one_prs (fp, i);
    }
    fprintf (fp, "}\n");
    break;
  default:
    fatal_error ("Unsupported");
    break;
  }
}

void prs_print (FILE *fp, act_prs *prs)
{
  while (prs) {
    fprintf (fp, "prs ");
    if (prs->vdd) {
      fprintf (fp, "<");
      prs->vdd->Print (fp);
      if (prs->gnd) {
	fprintf (fp, ", ");
	prs->gnd->Print (fp);
      }
      if (prs->psc) {
	fprintf (fp, " | ");
	prs->psc->Print (fp);
	fprintf (fp, ", ");
	prs->nsc->Print (fp);
      }
      fprintf (fp, "> ");
    }
    if (prs->leak_adjust) {
      fprintf (fp, " * ");
    }
    fprintf (fp, "{\n");
    act_prs_lang_t *p;
    for (p = prs->p; p; p = p->next) {
      _print_one_prs (fp, p);
    }
    fprintf (fp, "}\n");
    prs = prs->next;
  }
}

static void _chp_print (FILE *fp, act_chp_lang_t *c, int prec = 0)
{
  int lprec;
  
  if (!c) return;

  if (c->label) {
    fprintf (fp, "%s:", c->label);
  }
  
  switch (c->type) {
  case ACT_CHP_COMMALOOP:
  case ACT_CHP_SEMILOOP:
    fprintf (fp, "(");
    if (c->type == ACT_CHP_COMMALOOP) {
      fprintf (fp, ",");
    }
    else {
      fprintf (fp, ";");
    }
    fprintf (fp, "%s", c->u.loop.id);
    fprintf (fp, ":");
    print_expr (fp, c->u.loop.lo);
    if (c->u.loop.hi) {
      fprintf (fp, "..");
      print_expr (fp, c->u.loop.hi);
    }
    fprintf (fp, ":");
    _chp_print (fp, c->u.loop.body);
    fprintf (fp, ")");
    break;
    
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    {
      listitem_t *li;

      lprec = (c->type == ACT_CHP_SEMI ? 0 : 1);

      if (prec > lprec) {
	fprintf (fp, "(");
      }

      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	_chp_print (fp, (act_chp_lang_t *)list_value (li), lprec);
	if (list_next (li)) {
	  if (c->type == ACT_CHP_COMMA) {
	    fprintf (fp, ",");
	  }
	  else {
	    fprintf (fp, ";");
	  }
	}
      }

      if (prec > lprec) {
	fprintf (fp, ")");
      }

    }
    break;

  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    fprintf (fp, "*");
  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    fprintf (fp, "[");
    if (c->type == ACT_CHP_SELECT_NONDET) {
      fprintf (fp, "|");
    }
    {
      act_chp_gc_t *gc = c->u.gc;

      if (c->type == ACT_CHP_DOLOOP) {
	fprintf (fp, " ");
	_chp_print (fp, gc->s, 0);
	fprintf (fp, " <- ");
	if (gc->g) {
	  print_uexpr (fp, gc->g);
	}
	else {
	  fprintf (fp, "true");
	}
      }
      else {
	while (gc) {
	  if (!gc->g) {
	    if (c->type == ACT_CHP_LOOP) {
	      fprintf (fp, "true");
	    }
	    else {
	      fprintf (fp, "else");
	    }
	  }
	  else {
	    print_uexpr (fp, gc->g);
	  }
	  if (gc->s) {
	    fprintf (fp, " -> ");
	    _chp_print (fp, gc->s, 0);
	  }
	  if (gc->next) {
	    fprintf (fp, " [] ");
	  }
	  gc = gc->next;
	}
      }
    }
    if (c->type == ACT_CHP_SELECT_NONDET) {
      fprintf (fp, "|");
    }
    fprintf (fp, "]");
    break;
    
  case ACT_CHP_SKIP:
    fprintf (fp, "skip");
    break;

  case ACT_CHP_ASSIGN:
    c->u.assign.id->Print (fp);
    fprintf (fp, ":=");
    print_uexpr (fp, c->u.assign.e);
    break;
    
  case ACT_CHP_SEND:
    c->u.comm.chan->Print (fp);
    fprintf (fp, "!");
    {
      listitem_t *li;
      if (c->u.comm.e) {
	print_uexpr (fp, c->u.comm.e);
      }
      if (c->u.comm.var) {
	fprintf (fp, "?");
	c->u.comm.var->Print (fp);
      }
    }
    break;
    
  case ACT_CHP_RECV:
    c->u.comm.chan->Print (fp);
    fprintf (fp, "?");
    {
      if (c->u.comm.var) {
	c->u.comm.var->Print (fp);
      }
      if (c->u.comm.e) {
	fprintf (fp, "!");
	print_uexpr (fp, c->u.comm.e);
      }
    }
    break;

  case ACT_CHP_FUNC:
    fprintf (fp, "%s(", string_char (c->u.func.name));
    for (listitem_t *li = list_first (c->u.func.rhs); li; li = list_next (li)) {
      act_func_arguments_t *a = (act_func_arguments_t *) list_value (li);
      if (li != list_first (c->u.func.rhs)) {
	fprintf (fp, ",");
      }
      if (a->isstring) {
	fprintf (fp, "\"%s\"", string_char (a->u.s));
      }
      else {
	print_uexpr (fp, a->u.e);
      }
    }
    fprintf (fp, ")");
    break;

  case ACT_CHP_HOLE: /* to support verification */
    fprintf (fp, "_");
    break;
    
  default:
    fatal_error ("Unknown type");
    break;
  }
}

void chp_print (FILE *fp, act_chp_lang_t *c)
{
  _chp_print (fp, c);
}

void chp_print (FILE *fp, act_chp *chp)
{
  if (chp) {
    fprintf (fp, "chp {\n");
    chp_print (fp, chp->c);
    fprintf (fp, "\n}\n");
    //chp = chp->next;
  }
}

void hse_print (FILE *fp, act_chp *chp)
{
  if (chp) {
    fprintf (fp, "hse {\n");
    chp_print (fp, chp->c);
    fprintf (fp, "\n}\n");
    //chp = chp->next;
  }
}


void spec_print (FILE *fp, act_spec *spec)
{
  int count = config_get_table_size ("act.spec_types");
  char **specs = config_get_table_string ("act.spec_types");
  fprintf (fp, "spec {\n");
  while (spec) {
    if (ACT_SPEC_ISTIMING (spec)) {
      fprintf (fp, "   timing ");

#define SPEC_PRINT_ID(x)			\
      do {					\
	if (spec->extra[x] & 0x04) {		\
	  fprintf (fp, "?");			\
	}					\
	spec->ids[x]->Print (fp);		\
	if (spec->extra[x] & 0x8) {		\
	  fprintf (fp, "*");			\
	}					\
	if (spec->extra[x] & 0x3) {		\
	  if ((spec->extra[x] & 0x03) == 1) {	\
	    fprintf (fp, "+");			\
	  }					\
	  else {				\
	    fprintf (fp, "-");			\
	  }					\
	}					\
      } while (0)

      if (spec->ids[0]) {
	SPEC_PRINT_ID(0);
	fprintf (fp, " : ");
      }
      SPEC_PRINT_ID (1);
      if (spec->type == -1) {
	fprintf (fp, " < ");
      }
      else {
	fprintf (fp, " << ");
      }
      if (spec->ids[3]) {
	fprintf (fp, "[");
	print_expr (fp, (Expr *)spec->ids[3]);
	fprintf (fp, "] ");
      }
      SPEC_PRINT_ID(2);
      fprintf (fp, "\n");
    }
    else {
      Assert (spec->type >= 0 && spec->type < count, "What?");
      fprintf (fp, "   %s(", specs[spec->type]);
      for (int i=0; i < spec->count; i++) {
	if (i != 0) {
	  fprintf (fp, ", ");
	}
	spec->ids[i]->Print (fp);
      }
      fprintf (fp, ")\n");
    }
    spec = spec->next;
  }
  fprintf (fp, "}\n");
}

void refine_expand (act_refine *r, ActNamespace *ns, Scope *s)
{
  if (!r) return;
  if (r->b) {
    r->b->Expandlist (ns, s);
  }
}

act_languages *act_languages::Expand (ActNamespace *ns, Scope *s)
{
  act_languages *ret = new act_languages ();
  if (chp) {
    ret->chp = chp_expand (chp, ns, s);
    chp_check_channels (ret->chp->c, s);
  }
  if (hse) {
    ret->hse = chp_expand (hse, ns, s);
  }
  if (prs) {
    ret->prs = prs_expand (prs, ns, s);
  }
  if (refine) {
    refine_expand (refine, ns, s);
  }
  if (sizing) {
    ret->sizing = sizing_expand (sizing, ns, s);
  }
  return ret;
}


void refine_print (FILE *fp, act_refine *r)
{
  if (!r || !r->b) return;
  fprintf (fp, "refine {\n");
  r->b->Print (fp);
  fprintf (fp, "}\n");
}

void sizing_print (FILE *fp, act_sizing *s)
{
  int prev;

#define SEMI_NEWLINE				\
  do {						\
      if (prev) {				\
	fprintf (fp, ";\n");			\
      }						\
      else {					\
	fprintf (fp, "\n");			\
      }						\
      prev = 1;					\
  } while (0)
    
  while (s) {
    fprintf (fp, "sizing {");
    prev = 0;
    if (s->p_specified) {
      SEMI_NEWLINE;
      fprintf (fp, "   p_n_mode <- ");
      print_expr (fp, s->p_n_mode_e);
    }
    if (s->unit_n_specified) {
      SEMI_NEWLINE;
      fprintf (fp, "   unit_n <- ");
      print_expr (fp, s->unit_n_e);
    }
    if (s->leak_adjust_specified) {
      SEMI_NEWLINE;
      fprintf (fp, "   leak_adjust <- ");
      print_expr (fp, s->leak_adjust_e);
    }
    for (int i=0; i < A_LEN (s->d); i++) {
      SEMI_NEWLINE;
      fprintf (fp, "   ");
      s->d[i].id->Print (fp);
      fprintf (fp, " { ");
      if (s->d[i].eup) {
	fprintf (fp, "+ ");
	print_expr (fp, s->d[i].eup);
	if (s->d[i].flav_up != 0) {
	  fprintf (fp, ",%s", act_dev_value_to_string (s->d[i].flav_up));
	}
	if (s->d[i].upfolds) {
	  fprintf (fp, ";");
	  print_expr (fp, s->d[i].upfolds);
	}
      }
      if (s->d[i].edn) {
	if (s->d[i].eup) {
	  fprintf (fp, ", ");
	}
	fprintf (fp, "- ");
	print_expr (fp, s->d[i].edn);
	if (s->d[i].flav_dn != 0) {
	  fprintf (fp, ",%s", act_dev_value_to_string (s->d[i].flav_dn));
	}
	if (s->d[i].dnfolds) {
	  fprintf (fp, ";");
	  print_expr (fp, s->d[i].dnfolds);
	}
      }
      fprintf (fp, "}");
    }
    fprintf (fp, "\n}\n");
    s = s->next;
  }
}

static void _fill_sizing_directive (ActNamespace *ns, Scope *s,
				    act_sizing *ret, act_sizing_directive *d)
{
  if (d->loop_id) {
    int ilo, ihi;
    ValueIdx *vx;

    act_syn_loop_setup (ns, s, d->loop_id, d->lo, d->hi,
			&vx, &ilo, &ihi);

    for (int iter=ilo; iter <= ihi; iter++) {
      s->setPInt (vx->u.idx, iter);
      for (int i=0; i < A_LEN (d->d); i++) {
	_fill_sizing_directive (ns, s, ret, &d->d[i]);
      }
    }
    act_syn_loop_teardown (ns, s, d->loop_id, vx);
	
  }
  else {
    A_NEW (ret->d, act_sizing_directive);
    A_NEXT (ret->d).loop_id = NULL;
    A_NEXT (ret->d).id = d->id->Expand (ns, s);
    A_NEXT (ret->d).eup = expr_expand (d->eup, ns, s);
    A_NEXT (ret->d).edn = expr_expand (d->edn, ns, s);
    A_NEXT (ret->d).upfolds = expr_expand (d->upfolds, ns, s);
    A_NEXT (ret->d).dnfolds = expr_expand (d->dnfolds, ns, s);
    A_NEXT (ret->d).flav_up = d->flav_up;
    A_NEXT (ret->d).flav_dn = d->flav_dn;
    A_INC (ret->d);
  }
}

act_sizing *sizing_expand (act_sizing *sz, ActNamespace *ns, Scope *s)
{
  if (!sz) return NULL;
  act_sizing *ret;
  Expr *te;
  NEW (ret, act_sizing);
  ret->next = NULL;
  ret->p_specified = sz->p_specified;
  ret->unit_n_specified = sz->unit_n_specified;
  ret->leak_adjust_specified = sz->leak_adjust_specified;
  if (ret->p_specified) {
    te = expr_expand (sz->p_n_mode_e, ns, s);
    ret->p_n_mode_e = te;
    if (te && te->type == E_INT) {
      ret->p_n_mode = te->u.v;
    }
    else if (te && te->type == E_REAL) {
      ret->p_n_mode = te->u.f;
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("Expression for p_n_mode is not a const?");
    }
  }
  if (ret->unit_n_specified) {
    te = expr_expand (sz->unit_n_e, ns, s);
    ret->unit_n_e = te;
    if (te && te->type == E_INT) {
      ret->unit_n = te->u.v;
    }
    else if (te && te->type == E_REAL) {
      ret->unit_n = te->u.f;
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("Expression for unit_n is not a const?");
    }
  }
  if (ret->leak_adjust_specified) {
    te = expr_expand (sz->leak_adjust_e, ns, s);
    ret->leak_adjust_e = te;
    if (te && te->type == E_INT) {
      ret->leak_adjust = te->u.v;
    }
    else if (te && te->type == E_REAL) {
      ret->leak_adjust = te->u.f;
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("Expression for leak_adjust is not a const?");
    }
  }
  A_INIT (ret->d);
  if (A_LEN (sz->d) > 0) {
    for (int i=0; i < A_LEN (sz->d); i++) {
      _fill_sizing_directive (ns, s, ret, &sz->d[i]);
    }
  }
  return ret;
}


/* utility functions for expanded rules */

static act_prs_expr_t *_copy_rule (act_prs_expr_t *e)
{
  act_prs_expr_t *ret;
  
  if (!e) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = e->type;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l = _copy_rule (e->u.e.l);
    ret->u.e.r = _copy_rule (e->u.e.r);
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = _copy_rule (e->u.e.l);
    ret->u.e.r = NULL;
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v = e->u.v;
    ret->u.v.sz = NULL;
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l = e->u.l;
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
  return ret;
}

act_prs_expr_t *act_prs_complement_rule (act_prs_expr_t *e)
{
  act_prs_expr_t *r;
  NEW (r, act_prs_expr_t);
  r->type = ACT_PRS_EXPR_NOT;
  r->u.e.r = NULL;
  r->u.e.pchg = NULL;
  r->u.e.pchg_type = -1;
  r->u.e.l = _copy_rule (e);
  return r;
}

static void _twiddle_leaves (act_prs_expr_t *e)
{
  act_prs_expr_t *tmp;
  
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    _twiddle_leaves (e->u.e.l);
    _twiddle_leaves (e->u.e.r);
    break;

  case ACT_PRS_EXPR_NOT:
    _twiddle_leaves (e->u.e.l);
    break;

  case ACT_PRS_EXPR_VAR:
    tmp = _copy_rule (e);
    e->type =ACT_PRS_EXPR_NOT;
    e->u.e.l = tmp;
    e->u.e.r = NULL;
    e->u.e.pchg = NULL;
    e->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_LABEL:
    tmp = _copy_rule (e);
    e->type =ACT_PRS_EXPR_NOT;
    e->u.e.l = tmp;
    e->u.e.r = NULL;
    e->u.e.pchg = NULL;
    e->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_TRUE:
    e->type = ACT_PRS_EXPR_FALSE;
    break;
    
  case ACT_PRS_EXPR_FALSE:
    e->type = ACT_PRS_EXPR_TRUE;
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
}

act_prs_expr_t *act_prs_celement_rule (act_prs_expr_t *e)
{
  act_prs_expr_t *r = _copy_rule (e);
  _twiddle_leaves (r);
  return r;
}

void act_prs_expr_free (act_prs_expr_t *e)
{
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    act_prs_expr_free (e->u.e.l);
    act_prs_expr_free (e->u.e.r);
    break;

  case ACT_PRS_EXPR_NOT:
    act_prs_expr_free (e->u.e.l);
    break;

  case ACT_PRS_EXPR_VAR:
    break;

  case ACT_PRS_EXPR_LABEL:
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
  FREE (e);
}

void initialize_print (FILE *fp, act_initialize *init)
{
  listitem_t *li;
  if (!init) return;
  if (!init->actions) return;

  while (init) {
    fprintf (fp, "Initialize {\n");
    for (li = list_first (init->actions); li; li = list_next (li)) {
      act_chp_lang_t *c = (act_chp_lang_t *)list_value (li);
      fprintf (fp, " actions { ");
      chp_print (fp, c);
      fprintf (fp, " }");
      if (list_next (li)) {
	fprintf (fp, ";");
      }
      fprintf (fp, "\n");
    }
    fprintf (fp, "}\n");
    init = init->next;
  }
}
  
act_initialize *initialize_expand (act_initialize *init, ActNamespace *ns,
				   Scope *s)
{
  act_initialize *ret;
  if (!init) return NULL;
  NEW (ret, act_initialize);
  if (!init->actions) {
    ret->actions = NULL;
  }
  else {
    listitem_t *li;
    ret->actions = list_new ();
    for (li = list_first (init->actions); li; li = list_next (li)) {
      act_chp_lang_t *c = (act_chp_lang_t *) list_value (li);
      list_append (ret->actions, chp_expand (c, ns, s));
    }
  }
  ret->next = initialize_expand (init->next, ns, s);
  return ret;
}

static void dflow_print (FILE *fp, list_t *dflow)
{
  listitem_t *li;
  act_dataflow_element *e;
  
  for (li = list_first (dflow); li; li = list_next (li)) {
    e = (act_dataflow_element *) list_value (li);
    dflow_print(fp, e);
    if (list_next (li)) {
      fprintf (fp, ";");
    }
    fprintf (fp, "\n");
  }
}

void dflow_print (FILE *fp, act_dataflow_element *e)
{
  switch (e->t) {
  case ACT_DFLOW_FUNC:
    print_uexpr (fp, e->u.func.lhs);
    fprintf (fp, " -> ");
    if (e->u.func.nbufs) {
      if (e->u.func.istransparent) {
        fprintf (fp, "(");
      }
      else {
        fprintf (fp, "[");
      }
      print_expr (fp, e->u.func.nbufs);
      if (e->u.func.init) {
        fprintf (fp, ",");
        print_expr (fp, e->u.func.init);
      }
      if (e->u.func.istransparent) {
        fprintf (fp, ") ");
      }
      else {
        fprintf (fp, "] ");
      }
    }
    e->u.func.rhs->Print (fp);
    break;

  case ACT_DFLOW_SPLIT:
    fprintf (fp, "{");
    e->u.splitmerge.guard->Print (fp);
    fprintf (fp, "} ");
    e->u.splitmerge.single->Print (fp);
    fprintf (fp, " -> ");
    for (int i=0; i < e->u.splitmerge.nmulti; i++) {
      if (e->u.splitmerge.multi[i]) {
        e->u.splitmerge.multi[i]->Print (fp);
      }
      else {
        fprintf (fp, "*");
      }
      if (i != e->u.splitmerge.nmulti-1) {
        fprintf (fp, ", ");
      }
    }
    break;

  case ACT_DFLOW_MERGE:
  case ACT_DFLOW_MIXER:
  case ACT_DFLOW_ARBITER:
    fprintf (fp, "{");
    if (e->t == ACT_DFLOW_MERGE) {
      e->u.splitmerge.guard->Print (fp);
    }
    else if (e->t == ACT_DFLOW_MIXER) {
      fprintf (fp, "*");
    }
    else {
      fprintf (fp, "|");
    }
    fprintf (fp, "} ");
    for (int i=0; i < e->u.splitmerge.nmulti; i++) {
      if (e->u.splitmerge.multi[i]) {
        e->u.splitmerge.multi[i]->Print (fp);
      }
      else {
        fprintf (fp, "*");
      }
      if (i != e->u.splitmerge.nmulti-1) {
        fprintf (fp, ", ");
      }
    }
    fprintf (fp, " -> ");
    e->u.splitmerge.single->Print (fp);
    if (e->u.splitmerge.nondetctrl) {
      fprintf (fp, ", ");
      e->u.splitmerge.nondetctrl->Print (fp);
    }
    break;

  case ACT_DFLOW_SINK:
    e->u.sink.chan->Print (fp);
    fprintf (fp, " -> *");
    break;

  case ACT_DFLOW_CLUSTER:
    fprintf (fp, "dataflow_cluster {\n");
    dflow_print (fp, e->u.dflow_cluster);
    fprintf (fp, "}\n");
    break;
    
  default:
    fatal_error ("What?");
    break;
  }
}

static void dflow_order_print (FILE *fp, list_t *l)
{
  if (!l) return;
  fprintf (fp, "order {\n");
  for (listitem_t *li = list_first (l); li; li = list_next (li)) {
    act_dataflow_order *x = (act_dataflow_order *) list_value (li);
    for (listitem_t *mi = list_first (x->lhs); mi; mi = list_next (mi)) {
      ActId *tmp = (ActId *) list_value (mi);
      tmp->Print (fp);
      if (list_next (mi)) {
	fprintf (fp, ",");
      }
    }
    fprintf (fp, " < ");
    for (listitem_t *mi = list_first (x->rhs); mi; mi = list_next (mi)) {
      ActId *tmp = (ActId *) list_value (mi);
      tmp->Print (fp);
      if (list_next (mi)) {
	fprintf (fp, ",");
      }
    }
    if (list_next (li)) {
      fprintf (fp, ";\n");
    }
    else {
      fprintf (fp, "\n");
    }
  }
  fprintf (fp, "}\n");
}

void dflow_print (FILE *fp, act_dataflow *d)
{
  listitem_t *li;
  act_dataflow_element *e;

  if (!d) return;
  fprintf (fp, "dataflow {\n");
  dflow_order_print (fp, d->order);
  dflow_print (fp, d->dflow);
  fprintf (fp, "}\n");
}

static list_t *dflow_expand (list_t *dflow, ActNamespace *ns, Scope *s)
{
  listitem_t *li;
  act_dataflow_element *e, *f;
  list_t *ret;
  
  if (!dflow) return NULL;
  ret = list_new ();
  for (li = list_first (dflow); li; li = list_next (li)) {
    e = (act_dataflow_element *) list_value (li);
    NEW (f, act_dataflow_element);
    f->t = e->t;
    switch (e->t) {
    case ACT_DFLOW_FUNC:
      //f->u.func.lhs = expr_expand (e->u.func.lhs, ns, s, 0, 0);
      f->u.func.lhs = expr_expand (e->u.func.lhs, ns, s, 0);
      f->u.func.nbufs = NULL;
      f->u.func.init  = NULL;
      if (e->u.func.nbufs) {
	f->u.func.nbufs = expr_expand (e->u.func.nbufs, ns, s);
	if (e->u.func.init) {
	  f->u.func.init = expr_expand (e->u.func.init, ns, s);
	}
      }
      f->u.func.rhs = e->u.func.rhs->Expand (ns, s);
      break;

    case ACT_DFLOW_SPLIT:
    case ACT_DFLOW_MERGE:
    case ACT_DFLOW_MIXER:
    case ACT_DFLOW_ARBITER:
      if (e->u.splitmerge.guard) {
	f->u.splitmerge.guard = e->u.splitmerge.guard->Expand (ns, s);
      }
      else {
	f->u.splitmerge.guard = NULL;
      }
      f->u.splitmerge.nmulti = e->u.splitmerge.nmulti;
      MALLOC (f->u.splitmerge.multi, ActId *, f->u.splitmerge.nmulti);
      for (int i=0; i < f->u.splitmerge.nmulti; i++) {
	if (e->u.splitmerge.multi[i]) {
	  f->u.splitmerge.multi[i] = e->u.splitmerge.multi[i]->Expand (ns, s);
	}
	else {
	  f->u.splitmerge.multi[i] = NULL;
	}
      }
      f->u.splitmerge.single = e->u.splitmerge.single->Expand (ns, s);
      if (e->u.splitmerge.nondetctrl) {
	f->u.splitmerge.nondetctrl = e->u.splitmerge.nondetctrl->Expand (ns, s);
      }
      else {
	f->u.splitmerge.nondetctrl = NULL;
      }
      break;

    case ACT_DFLOW_CLUSTER:
      f->u.dflow_cluster = dflow_expand (e->u.dflow_cluster, ns, s);
      break;

    case ACT_DFLOW_SINK:
      f->u.sink.chan = e->u.sink.chan->Expand (ns, s);
      break;

    default:
      fatal_error ("What?");
      break;
    }
    list_append (ret, f);
  }
  
  return ret;
}

static
list_t *dflow_order_expand (list_t *l, ActNamespace *ns, Scope *s)
{
  act_dataflow_order *x, *tmp;
  list_t *ret;

  if (!l) return NULL;
   
  ret = list_new ();
  for (listitem_t *li = list_first (l); li; li = list_next (li)) {
    tmp = (act_dataflow_order *) list_value (li);
    NEW (x, act_dataflow_order);
    x->lhs = list_dup (tmp->lhs);
    x->rhs = list_dup (tmp->rhs);

    /* expand ids */
    for (listitem_t *mi = list_first (x->lhs); mi; mi = list_next (mi)) {
      ActId *tmp = (ActId *) list_value (mi);
      list_value (mi) = tmp->Expand (ns, s);
    }    
    for (listitem_t *mi = list_first (x->rhs); mi; mi = list_next (mi)) {
      ActId *tmp = (ActId *) list_value (mi);
      list_value (mi) = tmp->Expand (ns, s);
    }    
    list_append (ret, x);
  }
  return ret;
}

act_dataflow *dflow_expand (act_dataflow *d, ActNamespace *ns, Scope *s)
{
  act_dataflow *ret;
  
  if (!d) return NULL;
  NEW (ret, act_dataflow);
  ret->dflow = dflow_expand (d->dflow, ns, s);
  ret->order = dflow_order_expand (d->order, ns, s);
  
  return ret;
}

static void chp_check_var (ActId *id, Scope *s)
{
  if (id->isFragmented (s)) {
    act_error_ctxt (stderr);
    fprintf (stderr, "In CHP, fragmented access `");
    id->Print (stderr);
    fprintf (stderr, "' to base identifier `");
    id = id->unFragment (s);
    id->Print (stderr);
    fprintf (stderr, "'\n");
    fatal_error ("Cannot access channel/integer fragments in CHP!");
  }
}

static void chp_check_expr (Expr *e, Scope *s)
{
  if (!e) return;
  
  switch (e->type) {
  case E_AND:
  case E_OR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    chp_check_expr (e->u.e.l, s);
    chp_check_expr (e->u.e.r, s);
    break;

  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    chp_check_expr (e->u.e.l, s);
    break;

  case E_QUERY:
    chp_check_expr (e->u.e.l, s);
    chp_check_expr (e->u.e.r->u.e.l, s);
    chp_check_expr (e->u.e.r->u.e.r, s);
    break;

  case E_FUNCTION:
    Assert (!e->u.fn.r || e->u.fn.r->type == E_LT, "Function expansion");
    e = e->u.fn.r;
    while (e) {
      chp_check_expr (e->u.e.l, s);
      e = e->u.e.r;
    }
    break;

  case E_PROBE:
    break;

  case E_VAR:
    chp_check_var ((ActId *)e->u.e.l, s);
    break;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    chp_check_expr (e->u.e.l, s);
    break;

  case E_TRUE:
  case E_FALSE:
  case E_INT:
    break;

  case E_CONCAT:
    while (e) {
      chp_check_expr (e->u.e.l, s);
      e = e->u.e.r;
    }
    break;
    
  case E_BITFIELD:
    chp_check_var ((ActId *)e->u.e.l, s);
    break;

  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    break;
  }
}

void chp_check_channels (act_chp_lang_t *c, Scope *s)
{
  listitem_t *li;
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      chp_check_channels ((act_chp_lang_t *) list_value (li), s);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (act_chp_gc_t *gctmp = c->u.gc; gctmp; gctmp = gctmp->next) {
      chp_check_channels (gctmp->s, s);
      chp_check_expr (gctmp->g, s);
    }
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    chp_check_var (c->u.assign.id, s);
    chp_check_expr (c->u.assign.e, s);
    break;
    
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
    if (c->u.comm.e) {
      chp_check_expr (c->u.comm.e, s);
    }
    if (c->u.comm.var) {
      chp_check_var (c->u.comm.var, s);
    }
    break;

  default:
    break;
  }
}

static int act_hse_direction (Expr *e, ActId *id)
{
  if (!e) return 0;
  
  switch (e->type) {
  case E_AND:
  case E_OR:
  case E_XOR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV: 
  case E_MOD:
  case E_LSL:
  case E_LSR:
  case E_ASR:
    return act_hse_direction (e->u.e.l, id) |
      act_hse_direction (e->u.e.r, id);
    break;
    
  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    return act_hse_direction (e->u.e.l, id);
    break;

  case E_QUERY:
    return act_hse_direction (e->u.e.l, id) |
      act_hse_direction (e->u.e.r->u.e.l, id) |
      act_hse_direction (e->u.e.r->u.e.r, id);
    break;

  case E_FUNCTION:
    Assert (!e->u.fn.r || e->u.fn.r->type == E_LT, "Function expand error");
    e = e->u.fn.r;
    while (e) {
      if (act_hse_direction (e->u.e.l, id)) {
	return 1;
      }
      e = e->u.e.r;
    }
    break;

  case E_PROBE:
    break;

  case E_VAR:
    if (strcmp (((ActId *)e->u.e.l)->getName(), id->getName()) == 0) {
      return 1;
    }
    break;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    return act_hse_direction (e->u.e.l, id);
    break;

  case E_TRUE:
  case E_FALSE:
  case E_INT:
    break;

  case E_CONCAT:
    while (e) {
      if (act_hse_direction (e->u.e.l, id)) {
	return 1;
      }
      e = e->u.e.r;
    }
    break;
    
  case E_BITFIELD:
    if (strcmp (((ActId *)e->u.e.l)->getName(), id->getName()) == 0) {
      return 1;
    }
    break;

  default:
    fatal_error ("Unknown/unexpected type (%d)\n", e->type);
    break;
  }
  return 0;
}

int act_hse_direction (act_chp_lang_t *c, ActId *id)
{
  listitem_t *li;
  int dir = 0;
  if (!c) return 0;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      dir = dir | act_hse_direction ((act_chp_lang_t *) list_value (li), id);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    for (act_chp_gc_t *gctmp = c->u.gc; gctmp; gctmp = gctmp->next) {
      dir |= act_hse_direction (gctmp->s, id);
      dir |= act_hse_direction (gctmp->g, id);
    }
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    if (strcmp (c->u.assign.id->getName(), id->getName()) == 0) {
      dir = 2;
    }
    dir |= act_hse_direction (c->u.assign.e, id);
    break;
    
  default:
    break;
  }
  return dir;
}


static act_prs_expr_t *_conv_nnf (void *cookie,
				  struct Hashtable *at_hash,
				  act_prs_expr_t *e,
				  void *(*conv)(void *, void *), int flip)
{
  act_prs_expr_t *ret;
  
  if (!e) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = e->type;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    if (flip) {
      ret->type = (e->type == ACT_PRS_EXPR_AND ? ACT_PRS_EXPR_OR :
		   ACT_PRS_EXPR_AND);
    }
    ret->u.e.l = _conv_nnf (cookie, at_hash, e->u.e.l, conv, flip);
    ret->u.e.r = _conv_nnf (cookie, at_hash, e->u.e.r, conv, flip);
    ret->u.e.pchg = NULL;
    ret->u.e.pchg_type = -1;
    break;

  case ACT_PRS_EXPR_NOT:
    FREE (ret);
    ret = _conv_nnf (cookie, at_hash, e->u.e.l, conv, 1-flip);
    break;

  case ACT_PRS_EXPR_VAR:
    if (conv) {
      ret->u.v.id = (ActId *) (*conv) (cookie, e->u.v.id);
    }
    else {
      ret->u.v = e->u.v;
    }
    ret->u.v.sz = NULL;
    
    if (flip) {
      act_prs_expr_t *t;
      NEW (t, act_prs_expr_t);
      t->type = ACT_PRS_EXPR_NOT;
      t->u.e.l = ret;
      t->u.e.r = NULL;
      t->u.e.pchg = NULL;
      t->u.e.pchg_type = -1;
      ret = t;
    }
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    if (flip) {
      ret->type = (e->type == ACT_PRS_EXPR_TRUE ?
		   ACT_PRS_EXPR_FALSE : ACT_PRS_EXPR_TRUE);
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    {
      act_prs_lang_t *tmp;
      hash_bucket_t *b;
      b = hash_lookup (at_hash, e->u.l.label);
      if (!b) {
	fatal_error ("Label `%s' not found", e->u.l.label);
      }
      tmp = (act_prs_lang_t *) b->v;
      Assert (tmp->type == ACT_PRS_RULE &&
	      tmp->u.one.label  &&
	      strcmp ((char *)tmp->u.one.e, e->u.l.label) == 0,
	      "Error in at_hash map for nnf conversion");
      if (flip) {
	if (tmp->u.one.dir != 0) {
	  fatal_error ("Label expression ~@%s used incorrectly", b->key);
	}
	FREE (ret);
	ret = _conv_nnf (cookie, at_hash, tmp->u.one.e, conv, 0);
      }
      else {
	if (tmp->u.one.dir != 1) {
	  fatal_error ("Label expression @%s used incorrectly", b->key);
	}
	FREE (ret);
	ret = _conv_nnf (cookie, at_hash, tmp->u.one.e, conv, 0);
      }
    }
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fatal_error ("and/or loop?!");
    break;

  default:
    fatal_error ("What?");
    break;
  }
  return ret;
}

/* -- return nnf version of expression -- */
act_prs_expr_t *act_prs_expr_nnf (void *cookie,
				  struct Hashtable *at_hash,
				  act_prs_expr_t *e,
				  void *(*conv)(void *, void *))
{
  return _conv_nnf (cookie, at_hash, e, conv, 0);
}
