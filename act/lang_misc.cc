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
#include "prs.h"
#include <common/qops.h>
#include <common/config.h>
#include <string.h>


/*------------------------------------------------------------------------
 *
 *
 *  Attributes
 *
 *
 *------------------------------------------------------------------------
 */
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
    } else if (current_attr_table[pos][0] == 'r' &&
	       (tmp->e->type != E_REAL && tmp->e->type != E_INT)) {
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

void act_print_attributes (FILE *fp, act_attr_t *a)
{
  _print_attr (fp, a);
}




/*------------------------------------------------------------------------
 *
 *   spec { ... } body
 * 
 *------------------------------------------------------------------------
 */

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
    if (tmp->count > 0) {
      MALLOC (tmp->ids, ActId *, tmp->count);
      for (int i=0; i < tmp->count-1; i++) {
	if (s->ids[i]) {
	  tmp->ids[i] = s->ids[i]->Expand (ns, sc);
	  if (tmp->ids[i]->Tail()->arrayInfo() &&
	      tmp->ids[i]->Tail()->arrayInfo()->isDeref()) {
	    if (!tmp->ids[i]->validateDeref (sc)) {
	      act_error_ctxt (stderr);
	      fprintf (stderr, "ID: ");
	      tmp->ids[i]->Print (stderr);
	      fprintf (stderr, "\n");
	      fatal_error ("Dereference out of range!");
	    }
	  }
	}
	else {
	  tmp->ids[i] = NULL;
	}
      }
      if (!ACT_SPEC_ISTIMING (tmp)) {
	tmp->ids[tmp->count-1] = s->ids[tmp->count-1]->Expand (ns, sc);
	if (tmp->ids[tmp->count-1]->Tail()->arrayInfo() &&
	    tmp->ids[tmp->count-1]->Tail()->arrayInfo()->isDeref()) {
	  if (!tmp->ids[tmp->count-1]->validateDeref (sc)) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "ID: ");
	    tmp->ids[tmp->count-1]->Print (stderr);
	    fprintf (stderr, "\n");
	    fatal_error ("Dereference out of range!");
	  }
	}
      }
      else {
	tmp->ids[tmp->count-1] = (ActId *) (s->ids[tmp->count-1] ?
				expr_expand ((Expr *)s->ids[tmp->count-1],
					     ns, sc) : NULL);
      }
    }
    else {
      tmp->ids = NULL;
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
      else if (spec->type == -2) {
	fprintf (fp, " << ");
      }
      else if (spec->type == -3) {
	fprintf (fp, " -> ");
      }
      else {
	fprintf (fp, " #> ");
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
      if (spec->count == -1) {
	fprintf (fp, "*");
      }
      fprintf (fp, ")\n");
    }
    spec = spec->next;
  }
  fprintf (fp, "}\n");
}


/*------------------------------------------------------------------------
 *
 *
 *   sizing { ... }
 *
 *
 *------------------------------------------------------------------------
 */


static void sizing_print_directive (FILE *fp, act_sizing_directive *d)
{
  d->id->Print (fp);
  fprintf (fp, " { ");
  if (d->eup) {
    fprintf (fp, "+ ");
    print_expr (fp, d->eup);
    if (d->flav_up != 0) {
      fprintf (fp, ",%s", act_dev_value_to_string (d->flav_up));
    }
    if (d->upfolds) {
      fprintf (fp, ",");
      print_expr (fp, d->upfolds);
    }
  }
  if (d->edn) {
    if (d->eup) {
      fprintf (fp, "; ");
    }
    fprintf (fp, "- ");
    print_expr (fp, d->edn);
    if (d->flav_dn != 0) {
      fprintf (fp, ",%s", act_dev_value_to_string (d->flav_dn));
    }
    if (d->dnfolds) {
      fprintf (fp, ",");
      print_expr (fp, d->dnfolds);
    }
  }
  fprintf (fp, "}");
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
      if (s->d[i].loop_id) {
	fprintf (fp, "(;%s:", s->d[i].loop_id);
	print_expr (fp, s->d[i].lo);
	if (s->d[i].hi) {
	  fprintf (fp, "..");
	  print_expr (fp, s->d[i].hi);
	}
	fprintf (fp, ":");
	for (int j=0; j < A_LEN (s->d[i].d); j++) {
	  sizing_print_directive (fp, &(s->d[i].d[j]));
	  if (j != A_LEN (s->d[i].d)-1) {
	    fprintf (fp, "; ");
	  }
	}
      }
      else {
	sizing_print_directive (fp, &s->d[i]);
      }
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
      ret->p_n_mode = te->u.ival.v;
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
      ret->unit_n = te->u.ival.v;
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
      ret->leak_adjust = te->u.ival.v;
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
  if (sz->next) {
    ret->next = sizing_expand (sz->next, ns, s);
  }
  return ret;
}

/*------------------------------------------------------------------------
 *
 *  Initialize { ... } 
 *
 *------------------------------------------------------------------------
 */

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


/*------------------------------------------------------------------------
 *
 *  dataflow { ... }
 *
 *------------------------------------------------------------------------
 */
static void dflow_print (FILE *fp, list_t *dflow, int isexp)
{
  listitem_t *li;
  act_dataflow_element *e;
  
  for (li = list_first (dflow); li; li = list_next (li)) {
    e = (act_dataflow_element *) list_value (li);
    dflow_print(fp, e, isexp);
    if (list_next (li)) {
      fprintf (fp, ";");
    }
    fprintf (fp, "\n");
  }
}

void dflow_print (FILE *fp, act_dataflow_element *e, int isexp)
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
      if (isexp) {
	if (e->u.splitmerge.multi[i]) {
	  e->u.splitmerge.multi[i]->Print (fp);
	}
	else {
	  fprintf (fp, "*");
	}
      }
      else {
	if (e->u.splitmerge.pre_exp[i]->id) {
	  fprintf (fp, "(,%s:", e->u.splitmerge.pre_exp[i]->id);
	  print_expr (fp, e->u.splitmerge.pre_exp[i]->lo);
	  if (e->u.splitmerge.pre_exp[i]->hi) {
	    fprintf (fp, "..");
	    print_expr (fp, e->u.splitmerge.pre_exp[i]->hi);
	  }
	  fprintf (fp, ":");
	}
	if (e->u.splitmerge.pre_exp[i]->chanid) {
	  e->u.splitmerge.pre_exp[i]->chanid->Print (fp);
	}
	else {
	  fprintf (fp, "*");
	}
	if (e->u.splitmerge.pre_exp[i]->id) {
	  fprintf (fp, ")");
	}
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
      if (isexp) {
	if (e->u.splitmerge.multi[i]) {
	  e->u.splitmerge.multi[i]->Print (fp);
	}
	else {
	  fprintf (fp, "*");
	}
      }
      else {
	if (e->u.splitmerge.pre_exp[i]->id) {
	  fprintf (fp, "(,%s:", e->u.splitmerge.pre_exp[i]->id);
	  print_expr (fp, e->u.splitmerge.pre_exp[i]->lo);
	  if (e->u.splitmerge.pre_exp[i]->hi) {
	    fprintf (fp, "..");
	    print_expr (fp, e->u.splitmerge.pre_exp[i]->hi);
	  }
	  fprintf (fp, ":");
	}
	if (e->u.splitmerge.pre_exp[i]->chanid) {
	  e->u.splitmerge.pre_exp[i]->chanid->Print (fp);
	}
	else {
	  fprintf (fp, "*");
	}
	if (e->u.splitmerge.pre_exp[i]->id) {
	  fprintf (fp, ")");
	}
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
    dflow_print (fp, e->u.dflow_cluster, isexp);
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
  if (!d) return;
  fprintf (fp, "dataflow {\n");
  dflow_order_print (fp, d->order);
  dflow_print (fp, d->dflow, d->isexpanded);
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
      f->u.func.lhs = expr_expand (e->u.func.lhs, ns, s, ACT_EXPR_EXFLAG_CHPEX);
      f->u.func.nbufs = NULL;
      f->u.func.init  = NULL;
      f->u.func.istransparent = e->u.func.istransparent;
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
      {
	int ilog, w;
	InstType *it;

	// all channels must have the same bit-width
	int u_bitwidth = -1;
	A_DECL (ActId *, newarray);
	A_INIT (newarray);
	
	for (int i=0; i < e->u.splitmerge.nmulti; i++) {
	  A_NEW (newarray, ActId *);
	  if (e->u.splitmerge.pre_exp[i]->id) {
	    // loop expand!
	    ValueIdx *vx;
	    int ilo, ihi, ii;
	    act_syn_loop_setup (ns, s, e->u.splitmerge.pre_exp[i]->id,
				e->u.splitmerge.pre_exp[i]->lo,
				e->u.splitmerge.pre_exp[i]->hi,
				&vx, &ilo, &ihi);
	    for (ii=ilo; ii <= ihi; ii++) {
	      s->setPInt (vx->u.idx, ii);
	      A_NEW (newarray, ActId *);
	      A_NEXT (newarray) =
		e->u.splitmerge.pre_exp[i]->chanid->Expand (ns, s);
	      act_type_var (s, A_NEXT (newarray), &it);
	      if (u_bitwidth == -1) {
		u_bitwidth = TypeFactory::bitWidth (it);
	      }
	      else {
		if (u_bitwidth != TypeFactory::bitWidth (it)) {
		  act_error_ctxt (stderr);
		  fprintf (stderr, "ERROR: split/merge/mixer/arbiter data channels must have uniform bitwidth.\n");
		  fprintf (stderr, "  Current width: %d; channel `", u_bitwidth);
		  A_NEXT (newarray)->Print (stderr);
		  fprintf (stderr, "' width %d\n", TypeFactory::bitWidth (it));
		  exit (1);
		}
	      }
	      A_INC (newarray);
	    }
	    act_syn_loop_teardown (ns, s, e->u.splitmerge.pre_exp[i]->id, vx);
	  }
	  else {
	    if (e->u.splitmerge.pre_exp[i]->chanid) {
	      A_NEXT (newarray) =
		e->u.splitmerge.pre_exp[i]->chanid->Expand (ns, s);
	      act_type_var (s, A_NEXT (newarray), &it);
	      if (u_bitwidth == -1) {
		u_bitwidth = TypeFactory::bitWidth (it);
	      }
	      else {
		if (u_bitwidth != TypeFactory::bitWidth (it)) {
		  act_error_ctxt (stderr);
		  fprintf (stderr, "ERROR: split/merge/mixer/arbiter data channels must have uniform bitwidth.\n");
		  fprintf (stderr, "  Current width: %d; channel `", u_bitwidth);
		  A_NEXT (newarray)->Print (stderr);
		  fprintf (stderr, "' width %d\n", TypeFactory::bitWidth (it));
		  exit (1);
		}
	      }
	    }
	    else {
	      A_NEXT (newarray) = NULL;
	    }
	    A_INC (newarray);
	  }
	}
	f->u.splitmerge.nmulti = A_LEN (newarray);
	f->u.splitmerge.multi = newarray;
	if (f->u.splitmerge.nmulti < 2) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "ERROR: ");
	  int side = 0;
	  if (e->t == ACT_DFLOW_ARBITER) {
	    fprintf (stderr, "arbiter");
	  }
	  else if (e->t == ACT_DFLOW_MIXER) {
	    fprintf (stderr, "mixer");
	  }
	  else if (e->t == ACT_DFLOW_MERGE) {
	    fprintf (stderr, "merge");
	  }
	  else if (e->t == ACT_DFLOW_SPLIT) {
	    fprintf (stderr, "split");
	    side = 1;
	  }
	  else {
	    Assert (0, "What?");
	  }
	  fprintf (stderr, " requires more than one channel after expansion on %s.\n", side ? "LHS" : "RHS");
	  exit (1);
	}

	w = 0;
	ilog = f->u.splitmerge.nmulti;
        ilog--;
	while (ilog > 0) {
	  w++;
	  ilog >>= 1;
	}
	ilog = w;
	
	if (e->u.splitmerge.guard) {
	  f->u.splitmerge.guard = e->u.splitmerge.guard->Expand (ns, s);
	  act_type_var (s, f->u.splitmerge.guard, &it);
	  w = TypeFactory::bitWidth (it);
	  if (w != ilog) {
	    act_error_ctxt (stderr);
	    if (w < ilog) {
	      warning ("Bit-width of control input is less than the number of options.");
	    }
	    else {
	      fprintf (stderr, "ERROR: Bit-width of control input is wider than necessary.\n");
	    }
	    fprintf (stderr, "   Bit-width: %d; options: %d (%d bits); ID: ",
		     w, e->u.splitmerge.nmulti, ilog);
	    f->u.splitmerge.guard->Print (stderr);
	    fprintf (stderr , "\n");
	    if (w > ilog) {
	      exit (1);
	    }
	  }
	  delete it;
	}
	else {
	  f->u.splitmerge.guard = NULL;
	}

	f->u.splitmerge.single = e->u.splitmerge.single->Expand (ns, s);
	if (u_bitwidth != -1) {
	  act_type_var (s, f->u.splitmerge.single, &it);
	  if (u_bitwidth != TypeFactory::bitWidth (it)) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "ERROR: split/merge/mixer/arbiter data channels must have uniform bitwidth.\n");
	    fprintf (stderr, "  Current width: %d; channel `", u_bitwidth);
	    f->u.splitmerge.single->Print (stderr);
	    fprintf (stderr, "' width %d\n", TypeFactory::bitWidth (it));
	    exit (1);
	  }
	}
	if (e->u.splitmerge.nondetctrl) {
	  f->u.splitmerge.nondetctrl = e->u.splitmerge.nondetctrl->Expand (ns, s);
	  act_type_var (s, f->u.splitmerge.nondetctrl, &it);
	  w = TypeFactory::bitWidth (it);
	  if (w != ilog) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "ERROR: bit-width of generated control `");
	    f->u.splitmerge.nondetctrl->Print (stderr);
	    fprintf (stderr, "' is incorrect.\n");
	    fprintf (stderr, "   Bit-width: %d; expected: %d\n", w, ilog);
	    exit (1);
	  }
	}
	else {
	  f->u.splitmerge.nondetctrl = NULL;
	}
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
  ret->isexpanded = 1;
  
  return ret;
}

static void _update_expr (Expr **x, ActNamespace *orig, ActNamespace *newns)
{
  *x = expr_update (expr_predup (*x), orig, newns);
}

static void _clone_sz_dir (act_sizing_directive *tgt,
			   act_sizing_directive *src,
			   ActNamespace *orig,
			   ActNamespace *newns)
{
  *tgt = *src;
  if (tgt->id) { // could be NULL prior to expansion
    tgt->id = tgt->id->Clone (orig, newns);
  }
  _update_expr (&tgt->eup, orig, newns);
  _update_expr (&tgt->edn, orig, newns);
  tgt->upfolds = expr_update (expr_predup (tgt->upfolds), orig, newns);
  tgt->dnfolds = expr_update (expr_predup (tgt->dnfolds), orig, newns);
  if (tgt->loop_id) {
    tgt->lo = expr_update (expr_predup (tgt->lo), orig, newns);
    tgt->hi = expr_update (expr_predup (tgt->hi), orig, newns);
    A_INIT (tgt->d);
    if (A_LEN (src->d) > 0) {
      A_NEWP (tgt->d, act_sizing_directive, A_LEN (src->d));
      for (int i=0; i < A_LEN (src->d); i++) {
	A_NEW (tgt->d, act_sizing_directive);
	_clone_sz_dir (&A_NEXT (tgt->d), &src->d[i], orig, newns);
	A_INC (tgt->d);
      }
    }
  }
}


act_sizing *sizing_dup (act_sizing *sz, ActNamespace *orig, ActNamespace *newns)
{
  act_sizing *ret;
  if (!sz) return NULL;
  NEW (ret, act_sizing);
  *ret = *sz;
  if (sz->p_specified) {
    ret->p_n_mode_e = expr_update (expr_predup (sz->p_n_mode_e), orig, newns);
  }
  else {
    ret->p_n_mode_e = NULL;
  }
  if (sz->unit_n_specified) {
    ret->unit_n_e = expr_update (expr_predup (sz->unit_n_e), orig, newns);
  }
  else {
    ret->unit_n_e = NULL;
  }
  if (sz->leak_adjust_specified) {
    ret->leak_adjust_e = expr_update (expr_predup (sz->leak_adjust_e), orig, newns);
  }
  else {
    ret->leak_adjust_e = NULL;
  }
  if (A_LEN (sz->d) > 0) {
    A_INIT (ret->d);
    A_NEWP (ret->d, act_sizing_directive, A_LEN (sz->d));
    for (int i=0; i < A_LEN (sz->d); i++) {
      A_NEW (ret->d, act_sizing_directive);
      _clone_sz_dir (&A_NEXT (ret->d), &sz->d[i], orig, newns);
      A_INC (ret->d);
    }
  }
  return ret;
}

act_spec *spec_dup (act_spec *s, ActNamespace *orig, ActNamespace *newns)
{
  act_spec *ret, *tmp;

  ret = NULL;
  while (s) {
    if (!ret) {
      NEW (ret, act_spec);
      tmp = ret;
    }
    else {
      NEW (tmp->next, act_spec);
      tmp = tmp->next;
    }
    tmp->next = NULL;
    tmp->type = s->type;
    tmp->isrequires = s->isrequires;
    tmp->count = s->count;
    tmp->extra = s->extra;
    MALLOC (tmp->ids, ActId *, s->count);
    if (ACT_SPEC_ISTIMING (s)) {
      MALLOC (tmp->extra, int, s->count-1);
      for (int i=0; i < s->count-1; i++) {
	tmp->extra[i] = s->extra[i];
        if (s->ids[i]) {
   	   tmp->ids[i] = s->ids[i]->Clone (orig, newns);
        }
        else {
           tmp->ids[i] = NULL;
        }
      }
      if (s->ids[s->count-1]) {
	tmp->ids[s->count-1] =
	  (ActId *) expr_update (expr_predup ((Expr *)s->ids[s->count-1]),
				 orig, newns);
      }
      else {
	tmp->ids[s->count-1] = NULL;
      }
    }
    else {
      for (int i=0; i < s->count; i++) {
	tmp->ids[i] = s->ids[i]->Clone (orig, newns);
      }
    }
    s = s->next;
  }
  return ret;
}


static list_t *_clone_dflow_list (list_t *l,
				  ActNamespace *orig, ActNamespace *newns,
				  int isexp)
{
  list_t *ret;
  listitem_t *li;
  
  ret = list_dup (l);
  for (li = list_first (ret); li; li = list_next (li)) {
    act_dataflow_element *x, *xnew;
    x = (act_dataflow_element *) list_value (li);
    NEW (xnew, act_dataflow_element);
    *xnew = *x;
    switch (x->t) {
    case ACT_DFLOW_FUNC:
      _update_expr (&xnew->u.func.lhs, orig, newns);
      _update_expr (&xnew->u.func.nbufs, orig, newns);
      if (xnew->u.func.nbufs) {
	_update_expr (&xnew->u.func.init, orig, newns);
      }
      xnew->u.func.rhs = xnew->u.func.rhs->Clone (orig, newns);
      break;
    case ACT_DFLOW_SPLIT:
    case ACT_DFLOW_MERGE:
    case ACT_DFLOW_MIXER:
    case ACT_DFLOW_ARBITER:
      if (xnew->u.splitmerge.guard) {
	xnew->u.splitmerge.guard = xnew->u.splitmerge.guard->Clone (orig, newns);
      }
      if (isexp) {
	MALLOC (xnew->u.splitmerge.multi, ActId *, xnew->u.splitmerge.nmulti);
	for (int i=0; i < xnew->u.splitmerge.nmulti; i++) {
	  xnew->u.splitmerge.multi[i] =
	    x->u.splitmerge.multi[i]->Clone (orig, newns);
	}
      }
      else {
	MALLOC (xnew->u.splitmerge.pre_exp, act_dataflow_loop *,
		xnew->u.splitmerge.nmulti);
	for (int i=0; i < xnew->u.splitmerge.nmulti; i++) {
	  xnew->u.splitmerge.pre_exp[i] = new act_dataflow_loop;
	  *(xnew->u.splitmerge.pre_exp[i]) = *(x->u.splitmerge.pre_exp[i]);
	  _update_expr (&xnew->u.splitmerge.pre_exp[i]->lo, orig, newns);
	  _update_expr (&xnew->u.splitmerge.pre_exp[i]->hi, orig, newns);
	  if (xnew->u.splitmerge.pre_exp[i]->chanid) {
	    xnew->u.splitmerge.pre_exp[i]->chanid =
	      xnew->u.splitmerge.pre_exp[i]->chanid->Clone (orig, newns);
	  }
	}
      }
      if (xnew->u.splitmerge.single) {
	xnew->u.splitmerge.single =
	  xnew->u.splitmerge.single->Clone (orig, newns);
      }
      if (xnew->u.splitmerge.nondetctrl) {
	xnew->u.splitmerge.nondetctrl =
	  xnew->u.splitmerge.nondetctrl->Clone (orig, newns);
      }
      break;
    case ACT_DFLOW_SINK:
      xnew->u.sink.chan = xnew->u.sink.chan->Clone (orig, newns);
      break;

    case ACT_DFLOW_CLUSTER:
      xnew->u.dflow_cluster = _clone_dflow_list (xnew->u.dflow_cluster,
						 orig, newns, isexp);
      break;

    default:
      Assert (0, "Why am i here?");
      break;
    }
  }
  return ret;
}
			
act_dataflow *dflow_dup (act_dataflow *d, ActNamespace *orig,
			 ActNamespace *newns)
{
  if (!d) return NULL;
  act_dataflow *ret;
  listitem_t *li;
  NEW (ret, act_dataflow);
  ret->isexpanded = d->isexpanded;
  ret->dflow = _clone_dflow_list (d->dflow, orig, newns, ret->isexpanded);

  if (d->order) {
    ret->order = list_dup (ret->order);
    for (li = list_first (ret->order); li; li = list_next (li)) {
      act_dataflow_order *x, *xnew;
      listitem_t *mi;
      x = (act_dataflow_order *) list_value (li);
      NEW (xnew, act_dataflow_order);
      xnew->lhs = list_dup (x->lhs);
      xnew->rhs = list_dup (x->rhs);
      list_value (li) = xnew;
      for (mi = list_first (xnew->lhs); mi; mi = list_next (mi)) {
	list_value (mi) = ((ActId *)list_value (mi))->Clone (orig, newns);
      }
      for (mi = list_first (xnew->rhs); mi; mi = list_next (mi)) {
	list_value (mi) = ((ActId *)list_value (mi))->Clone (orig, newns);
      }
    }
  }
  else {
    ret->order = NULL;
  }
  return ret;
}
