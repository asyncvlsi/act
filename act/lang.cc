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
#include <act/inline.h>
#include <act/extlang.h>
#include "prs.h"
#include <common/qops.h>
#include <common/config.h>
#include <string.h>


static ActId *expand_var_read (ActId *id, ActNamespace *ns, Scope *s)
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

act_prs_lang_t *prs_expand (act_prs_lang_t *, ActNamespace *, Scope *);

void _merge_duplicate_rules (act_prs_lang_t *p)
{
  /* XXX: fix this */
}

act_prs *prs_expand (act_prs *p, ActNamespace *ns, Scope *s)
{
  act_prs *ret;

  if (!p) return NULL;

  NEW (ret, act_prs);

  ret->vdd = expand_var_read (p->vdd, ns, s);
  ret->gnd = expand_var_read (p->gnd, ns, s);
  ret->psc = expand_var_read (p->psc, ns, s);
  ret->nsc = expand_var_read (p->nsc, ns, s);
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
    switch (ACT_PRS_LANG_TYPE (p->type)) {
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

    case ACT_PRS_DEVICE:
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

      tmp->u.p.g = NULL;
      tmp->u.p._g = NULL;
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
	  ilo = etmp->u.ival.v;
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
	  ihi = etmp->u.ival.v;
	}
	else {
	  ihi = etmp->u.ival.v-1;
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
  switch (ACT_PRS_LANG_TYPE (prs->type)) {
  case ACT_PRS_RULE:
    if (prs->u.one.attr) {
      act_print_attributes (fp, prs->u.one.attr);
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
      act_print_attributes (fp, prs->u.p.attr);
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
  case ACT_PRS_DEVICE:
    if (prs->u.p.attr) {
      act_print_attributes (fp, prs->u.p.attr);
    }
    {
      int i = prs->type - ACT_PRS_DEVICE;
      Assert (0 <= i && i < config_get_table_size ("act.prs_device"),
	      "What?!");
      fprintf (fp, "%s", config_get_table_string ("act.prs_device")[i]);
    }
    if (prs->u.p.sz) {
      _print_size (fp, prs->u.p.sz);
    }
    fprintf (fp, "(");
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

void act_print_one_prs (FILE *fp, act_prs_lang_t *p)
{
  _print_one_prs (fp, p);
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

void hse_print (FILE *fp, act_chp *chp)
{
  if (chp) {
    fprintf (fp, "hse {\n");
    chp_print (fp, chp->c);
    fprintf (fp, "\n}\n");
    //chp = chp->next;
  }
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
  if (r->nsteps > 1) {
    fprintf (fp, "refine <%d> {\n", r->nsteps);
  }
  else {
    fprintf (fp, "refine {\n");
  }
  r->b->Print (fp);
  fprintf (fp, "}\n");
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


refine_override::~refine_override()
{
  if (it) { delete it; }
  if (ids) { list_free (ids); }
  if (next) {
    delete next;
  }
}


act_refine *refine_dup (act_refine *r, ActNamespace *orig, ActNamespace *newns)
{
  act_refine *ret;
  NEW (ret, act_refine);
  if (r->b) {
    ret->b = r->b->Clone (orig, newns);
  }
  else {
    ret->b = NULL;
  }
  ret->nsteps = r->nsteps;
  ret->refsublist = list_dup (r->refsublist);
  if (r->overrides) {
    ret->overrides = r->overrides->Clone (orig, newns);
  }
  else {
    ret->overrides = NULL;
  }
  return ret;
}

InstType *_act_clone_replace (ActNamespace *replace, ActNamespace *newns,
			      InstType *it);


refine_override *refine_override::Clone (ActNamespace *orig, ActNamespace *newns)
{
  refine_override *ret;
  ret = new refine_override;
  if (!orig) {
    ret->it = it;
  }
  else {
    ret->it = _act_clone_replace (orig, newns, it);
  }
  ret->plus = plus;
  ret->ids = list_dup (ids);
  if (next) {
    ret->next = next->Clone (orig, newns);
  }
  return ret;
}
