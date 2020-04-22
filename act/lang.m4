/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011, 2018-2019 Rajit Manohar
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

/* 
   Language bodies
*/

language_body[ActBody *]: lang_chp
{{X:
    return $1;
}}
| lang_hse
{{X:
    return $1;
}}
| lang_prs 
{{X:
    return $1;
}}
| lang_spec
{{X:
    return $1;
}}
| lang_refine
{{X:
    return $1;
}}
| lang_size
{{X:
    return $1;
}}
;

supply_spec: "<" bool_expr_id [ "," bool_expr_id ]
                 [ "|" bool_expr_id "," bool_expr_id ] 
             ">"
{{X:
    ActRet *r;

    $0->supply.vdd = $2;
    if (!OPT_EMPTY ($3)) {
      r = OPT_VALUE ($3);
      $A(r->type == R_ID);
      $0->supply.gnd = r->u.id;
      FREE (r);
    }
    else {
      $0->supply.gnd = NULL;
    }
    OPT_FREE ($3);

    if (!OPT_EMPTY ($4)) {
      r = OPT_VALUE ($4);
      $A(r->type == R_ID);
      $0->supply.psc = r->u.id;
      FREE (r);
      r = OPT_VALUE2 ($4);
      $A(r->type == R_ID);
      $0->supply.nsc = r->u.id;
      FREE (r);
    }
    else {
      $0->supply.psc = NULL;
      $0->supply.nsc = NULL;
    }
    OPT_FREE ($4);
    return NULL;
}}
;

lang_chp[ActBody *]: "chp" [ supply_spec ] "{" [ chp_body ] "}"
{{X:
    ActBody *b;
    act_chp *chp;

    b = NULL;
    chp = NULL;
    if (!OPT_EMPTY ($4)) {
      ActRet *r;
      
      r = OPT_VALUE ($4);
      $A(r->type == R_CHP_LANG);
      NEW (chp, act_chp);
      chp->c = r->u.chp;
      FREE (r);
      chp->vdd = $0->supply.vdd;
      chp->gnd = $0->supply.gnd;
      chp->nsc = $0->supply.nsc;
      chp->psc = $0->supply.psc;
    }
    
    if (chp) {
      b = new ActBody_Lang (chp);
    }
      
    $0->supply.vdd = NULL;
    $0->supply.gnd = NULL;
    $0->supply.psc = NULL;
    $0->supply.nsc = NULL;
    OPT_FREE ($2);

    return b;
}}
;

lang_hse[ActBody *]: "hse" [ supply_spec ] "{" [ hse_body ] "}" 
{{X:
    ActBody *b;
    act_chp *chp;

    b = NULL;
    chp = NULL;
    if (!OPT_EMPTY ($4)) {
      ActRet *r;
      
      r = OPT_VALUE ($4);
      $A(r->type == R_CHP_LANG);
      NEW (chp, act_chp);
      chp->c = r->u.chp;
      FREE (r);
      chp->vdd = $0->supply.vdd;
      chp->gnd = $0->supply.gnd;
      chp->nsc = $0->supply.nsc;
      chp->psc = $0->supply.psc;
    }
    
    if (chp) {
      b = new ActBody_Lang (chp, 1);
    }
      
    $0->supply.vdd = NULL;
    $0->supply.gnd = NULL;
    $0->supply.psc = NULL;
    $0->supply.nsc = NULL;
    OPT_FREE ($2);

    return b;
}}
;

lang_prs[ActBody *]: "prs" [ supply_spec ] "{"
{{X:
    $0->attr_num = config_get_table_size ("act.prs_attr");
    $0->attr_table = config_get_table_string ("act.prs_attr");
}}
[ prs_body ] "}" 
{{X:
    ActBody *b;
    act_prs *p;

    b = NULL;
    p = NULL;
    if (!OPT_EMPTY ($4)) {
      ActRet *r;

      r = OPT_VALUE ($4);
      $A(r->type == R_PRS_LANG);
      NEW (p, act_prs);
      p->p = r->u.prs;
      FREE (r);
      p->vdd = $0->supply.vdd;
      p->gnd = $0->supply.gnd;
      p->nsc = $0->supply.nsc;
      p->psc = $0->supply.psc;
      p->next = NULL;
    }
    if (p) {
      b = new ActBody_Lang (p);
    }
    $0->supply.vdd = NULL;
    $0->supply.gnd = NULL;
    $0->supply.psc = NULL;
    $0->supply.nsc = NULL;
    OPT_FREE ($2);

    $0->attr_num = config_get_table_size ("act.instance_attr");
    $0->attr_table = config_get_table_string ("act.instance_attr");

    return b;
}}
;

lang_spec[ActBody *]: "spec" "{" spec_body "}"
{{X:
    ActBody *b;

    if (!$3) {
      b = NULL;
    }
    else {
      b = new ActBody_Lang ($3);
    }
    return b;
}}    
;

chp_body[act_chp_lang_t *]: { chp_comma_list ";" }*
{{X:
    act_chp_lang_t *c;

    if (list_length ($1) > 1) {
      NEW (c, act_chp_lang_t);
      c->type = ACT_CHP_SEMI;
      c->space = NULL;
      c->u.semi_comma.cmd = $1;
    }
    else {
      $A(list_length ($1) == 1);
      c = (act_chp_lang_t *) list_value (list_first ($1));
      list_free ($1);
    }
    return c;
}}
;

chp_comma_list[act_chp_lang_t *]: { chp_body_item "," }*
{{X:
    act_chp_lang_t *c;

    if (list_length ($1) > 1) {
      NEW (c, act_chp_lang_t);
      c->space = NULL;
      c->type = ACT_CHP_COMMA;
      c->u.semi_comma.cmd = $1;
    }
    else {
      $A(list_length ($1) == 1);
      c = (act_chp_lang_t *) list_value (list_first ($1));
      list_free ($1);
    }
    return c;
}}
;

chp_body_item[act_chp_lang_t *]: base_stmt
{{X:
    return $1;
}}
| select_stmt
{{X:
    return $1;
}}
| loop_stmt
{{X:
    return $1;
}}
;

base_stmt[act_chp_lang_t *]: send_stmt
{{X:
    return $1;
}}
| recv_stmt
{{X:
	return $1;
}}
| assign_stmt
{{X:
	return $1;
}}
| "skip" 
{{X:
	act_chp_lang_t *c;
	NEW (c, act_chp_lang_t);
	c->type = ACT_CHP_SKIP;
	c->space = NULL;
	return c;
}}
| "(" chp_body ")"
{{X:
	return $2;
}}
| ID "(" { chp_log_item "," }* ")" /* log */
{{X:
	act_chp_lang_t *c;
	NEW (c, act_chp_lang_t);
	c->type = ACT_CHP_FUNC;
	c->space = NULL;
	c->u.func.name = string_create ($1);
	c->u.func.rhs = $3;
	return c;
}}
;


chp_log_item[act_func_arguments_t *]: w_expr
{{X:
    act_func_arguments_t *arg;
    NEW (arg, struct act_func_arguments);
    arg->isstring = 0;
    arg->u.e = $1;
    return arg;
}}
|  STRING
{{X:
    act_func_arguments_t *arg;
    NEW (arg, struct act_func_arguments);
    arg->isstring = 1;
    arg->u.s = string_create ($1);
    return arg;
}}
;

send_stmt[act_chp_lang_t *]: expr_id snd_typ send_data
{{X:
    $3->u.comm.chan = $1;
    return $3;
}}
;

snd_typ[int]: "!"
{{X: return 0; }}
| "!+"
{{X: return 1; }}
| "!-"
{{X: return 2; }}
;

send_data[act_chp_lang_t *]: w_expr 
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SEND;
    c->space = NULL;
    c->u.comm.chan = NULL;
    c->u.comm.rhs = list_new ();
    list_append (c->u.comm.rhs, $1);
    return c;
}}
| "(" { w_expr "," }* ")" 
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SEND;
    c->space = NULL;
    c->u.comm.chan = NULL;
    c->u.comm.rhs = $2;
    return c;
}}
;

recv_stmt[act_chp_lang_t *]: expr_id rcv_type recv_id
{{X:
    $3->u.comm.chan = $1;
    return $3;
}}
;

rcv_type[int]: "?"
{{X: return 0; }}
| "?+"
{{X: return 1; }}
| "?-"
{{X: return 2; }}
;

recv_id[act_chp_lang_t *]: expr_id
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_RECV;
    c->space = NULL;
    c->u.comm.chan = NULL;
    c->u.comm.rhs = list_new ();
    list_append (c->u.comm.rhs, $1);
    return c;
}}
| "(" { expr_id "," }* ")" 
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_RECV;
    c->space = NULL;
    c->u.comm.chan = NULL;
    c->u.comm.rhs = $2;
    return c;
}}
;

assign_stmt[act_chp_lang_t *]: expr_id ":=" w_expr
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_ASSIGN;
    c->space = NULL;
    c->u.assign.id = $1;
    c->u.assign.e = $3;
    return c;
}}
| expr_id dir
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_ASSIGN;
    c->space = NULL;
    c->u.assign.id = $1;
    NEW (c->u.assign.e, Expr);
    if ($2) {
      c->u.assign.e->type = E_TRUE;
    }
    else {
      c->u.assign.e->type = E_FALSE;
    }
    return c;
}}
;

select_stmt[act_chp_lang_t *]: "[" { guarded_cmd "[]" }* "]"
{{X:
    act_chp_lang_t *c;
    act_chp_gc_t *gc;
    listitem_t *li;

    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SELECT;
    c->space = NULL;
    c->u.gc = NULL;
    for (li = list_first ($2); li; li = list_next (li)) {
      gc = (act_chp_gc_t *) list_value (li);
      if (!c->u.gc) {
	c->u.gc = gc;
      }
      if (list_next (li)) {
	gc->next = (act_chp_gc_t *)list_value (list_next (li));
      }
      else {
	gc->next = NULL;
      }
    }
    list_free ($2);
    return c;
}}
| "[" wbool_expr "]" 
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SELECT;
    c->space = NULL;
    NEW (c->u.gc, struct act_chp_gc);
    c->u.gc->g = $2;
    c->u.gc->s = NULL;
    c->u.gc->next = NULL;
    c->u.gc->id = NULL;
    return c;
}}
| "[|" { guarded_cmd "[]" }* "|]"
{{X:
    act_chp_lang_t *c;
    c = apply_X_select_stmt_opt0 ($0, $2);
    c->type = ACT_CHP_SELECT_NONDET;
    return c;
}}
;

guarded_cmd[act_chp_gc_t *]: wbool_expr "->" chp_body
{{X:
    act_chp_gc_t *gc;
    NEW (gc, act_chp_gc_t);
    gc->id = NULL;
    gc->lo = NULL;
    gc->hi = NULL;
    gc->g = $1;
    gc->s = $3;
    gc->next = NULL;
    return gc;
}}
| "(" "[]" ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wint_expr [ ".." wint_expr ] ":" wbool_expr "->" chp_body ")"
{{X:
    ActRet *r;
    Expr *hi = NULL;
    $0->scope->Del ($3);
    if (!OPT_EMPTY ($6)) {
      r = OPT_VALUE ($6);
      $A(r->type == R_EXPR);
      hi = r->u.exp;
      FREE (r);
    }
    OPT_FREE ($6);
    
    act_chp_gc_t *gc;
    NEW (gc, act_chp_gc_t);
    gc->id = $3;
    gc->lo = $5;
    gc->hi = hi;
    gc->g = $8;
    gc->s = $10;
    gc->next = NULL;
    return gc;
}}
| "else" "->" chp_body
{{X:
    act_chp_gc_t *gc;
    NEW (gc, act_chp_gc_t);
    gc->id = NULL;
    gc->lo = NULL;
    gc->hi = NULL;
    gc->g = NULL;
    gc->s = $3;
    gc->next = NULL;
    return gc;
}}
;

loop_stmt[act_chp_lang_t *]: "*[" chp_body [ "<-" wbool_expr ] "]" 
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_LOOP;
    c->space = NULL;
    NEW (c->u.gc, act_chp_gc_t);
    c->u.gc->next = NULL;
    c->u.gc->id = NULL;
    c->u.gc->g = NULL;
    c->u.gc->s = $2;
    if (!OPT_EMPTY ($3)) {
      ActRet *r;
      r = OPT_VALUE ($3);
      $A(r->type == R_EXPR);
      c->u.gc->g = r->u.exp;
      c->type = ACT_CHP_DOLOOP;
      FREE (r);
    }
    OPT_FREE ($3);
    return c;
}}
| "*[" { guarded_cmd "[]" }* "]"
{{X:
    act_chp_lang_t *c;
    c = apply_X_select_stmt_opt0 ($0, $2);
    c->type = ACT_CHP_LOOP;
    return c;
}}
;

hse_body[act_chp_lang_t *]: { hse_body_item ";" }*
{{X:
    return apply_X_chp_body_opt0 ($0, $1);
}}
;

hse_body_item[act_chp_lang_t *]: { assign_stmt "," }* 
{{X:
    return apply_X_chp_comma_list_opt0 ($0, $1);
}}
| hse_loop_stmt
{{X:
    return $1;
}}
| hse_select_stmt
{{X:
    return $1;
}}
| "skip"
{{X:
    return apply_X_base_stmt_opt3 ($0);
}}
| "(" hse_body ")"
{{X:
    return $2;
}}
;

/*
hse_assign_stmt[act_chp_lang_t *]: expr_id dir 
{{X:
    return apply_X_assign_stmt_opt1 ($0, $1, $2);
}}
;
*/

hse_select_stmt[act_chp_lang_t *]: "[" { hse_guarded_cmd "[]" }* "]"
{{X:
    return apply_X_select_stmt_opt0 ($0, $2);
}}
| "[" wbool_expr "]" 
{{X:
    return apply_X_select_stmt_opt1 ($0, $2);
}}
| "[|" { hse_guarded_cmd "[]" }* "|]"
{{X:
    return apply_X_select_stmt_opt2 ($0, $2);
}}
;

hse_guarded_cmd[act_chp_gc_t *]: wbool_expr "->" hse_body
{{X:
    return apply_X_guarded_cmd_opt0 ($0, $1, $3);
}}
| "(" "[]" ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wint_expr [ ".." wint_expr ] ":" wbool_expr "->" chp_body ")"
{{X:
    return apply_X_guarded_cmd_opt1 ($0, $3, $5, $6, $8, $10);
}}
| "else" "->" hse_body
{{X:
    return apply_X_guarded_cmd_opt2 ($0, $3);
}}
;

hse_loop_stmt[act_chp_lang_t *]: "*[" hse_body "]"
{{X:
    return apply_X_loop_stmt_opt0 ($0, $2, NULL);
}}
| "*[" { hse_guarded_cmd "[]" }* "]"
{{X:
    return apply_X_loop_stmt_opt1 ($0, $2);
}}
;

prs_body[act_prs_lang_t *]: [ attr_list ] 
{{X:
    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
}}
single_prs prs_body
{{X:
    if (!OPT_EMPTY ($1)) {
      ActRet *r;

      r = OPT_VALUE ($1);
      $A(r->type == R_ATTR);

      if ($2->type == ACT_PRS_LOOP || $2->type == ACT_PRS_TREE ||
	  $2->type == ACT_PRS_SUBCKT || ($2->type == ACT_PRS_RULE && $2->u.one.label)) {
	$E("Attributes cannot be associated with tree { }, subckt { }, loop, or label");
      }
      if ($2->type == ACT_PRS_RULE) {
	$2->u.one.attr = r->u.attr;
      }
      else {
	$2->u.p.attr = r->u.attr;
      }
      FREE (r);
    }
    OPT_FREE ($1);
    
    $2->next = $3;
    return $2;
}}
| [ attr_list ] 
{{X:
    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
}}
single_prs
{{X:
    if (!OPT_EMPTY ($1)) {
      ActRet *r;

      r = OPT_VALUE ($1);
      $A(r->type == R_ATTR);

      if ($2->type == ACT_PRS_LOOP || $2->type == ACT_PRS_TREE ||
	  $2->type == ACT_PRS_SUBCKT || ($2->type == ACT_PRS_RULE && $2->u.one.label)) {
	$E("Attributes cannot be associated with tree { }, subckt { }, loop, or label");
      }
      if ($2->type == ACT_PRS_RULE) {
	$2->u.one.attr = r->u.attr;
      }
      else {
	$2->u.p.attr = r->u.attr;
      }
      FREE (r);
    }
    OPT_FREE ($1);

    return $2;
}}
;

attr_list[act_attr_t *]: "[" { ID "=" w_expr ";" }** "]"
{{X: 
    listitem_t *li;
    act_attr_t *a, *ret, *prev;
    ActRet *r;

    a = ret = prev = NULL;
    for (li = list_first ($2); li; li = list_next (li)) {
      int i;
      
      r = (ActRet *)list_value (li);
      $A(r);
      $A(r->type == R_STRING);

      /* validate this attribute */
      for (i=0; i < $0->attr_num; i++) {
	if (strcmp (r->u.str, $0->attr_table[i]+4) == 0)
	  break;
      }
      if (i == $0->attr_num) {
	$e("Attribute ``%s'' is not defined in this ACT configuration\n", r->u.str);
	fprintf (stderr, "Valid options:");
	for (i=0; i < $0->attr_num; i++) {
	  fprintf (stderr, " %s", $0->attr_table[i]+4);
	}
	fprintf (stderr, "\n");
	exit (1);
      }
      
      if (!ret) {
	NEW (ret, act_attr_t);
	a = ret;
      }
      else {
	NEW (a, act_attr_t);
      }
      a->next = NULL;
      a->attr = r->u.str;
      FREE (r);
      if (prev) {
	prev->next = a;
      }
      prev = a;
	
      li = list_next (li);
      $A(li);
      r = (ActRet *)list_value (li);
      $A(r);
      $A(r->type == R_EXPR);
      a->e = r->u.exp;
      FREE (r);
    }
    list_free ($2);
    return ret;
}}
;

single_prs[act_prs_lang_t *]: EXTERN[prs_expr] arrow bool_expr_id dir
{{X:
    act_prs_lang_t *p;

    NEW (p, act_prs_lang_t);
    p->next = NULL;
    p->type = ACT_PRS_RULE;
    p->u.one.attr = NULL;
    p->u.one.e = (act_prs_expr_t *) $1;
    p->u.one.arrow_type = $2;
    p->u.one.id = $3;
    p->u.one.dir = $4;
    p->u.one.label = 0;
    return p;
}}
| EXTERN[prs_expr] arrow "@" ID dir
{{X:
    act_prs_lang_t *p;

    NEW (p, act_prs_lang_t);
    p->next = NULL;
    p->type = ACT_PRS_RULE;
    p->u.one.attr = NULL;
    p->u.one.e = (act_prs_expr_t *) $1;
    p->u.one.arrow_type = $2;
    p->u.one.id = (ActId *)$4;
    p->u.one.dir = $5;
    p->u.one.label = 1;
    return p;
}}
| ID [ tree_subckt_spec ]
{{X:
    ActRet *r;

    if (!OPT_EMPTY ($2)) {
      r = OPT_VALUE ($2);
    }
    else {
      r = NULL;
    }
    if (strcmp ($1, "tree") == 0) {
      if ($0->in_tree) {
	$E("tree { } directive in prs cannot be nested");
      }
      $0->in_tree++;
      if (r && (r->type != R_EXPR)) {
	$E("tree < > parameter must be an expression");
      }
    }
    else if (strcmp ($1, "subckt") == 0) {
      if ($0->in_subckt) {
	$E("subckt { } directive in prs cannot be nested");
      }
      $0->in_subckt++;
      if (r && (r->type != R_STRING)) {
	$E("subckt < > parameter must be a string");
      }
    }
    else {
      $E("Unknown type of body within prs { }: ``%s''", $1);
    }
}}
 "{" prs_body "}"
{{X:
    ActRet *r;
    act_prs_lang_t *p;

    if (OPT_EMPTY ($2)) {
      r = NULL;
    }
    else {
      r = OPT_VALUE ($2);
    }
    OPT_FREE ($2);

    NEW (p, act_prs_lang_t);
    p->next = NULL;
    p->u.l.p = $4;

    if (strcmp ($1, "tree") == 0) {
      p->type = ACT_PRS_TREE;
      if (r) {
	$A(r->type == R_EXPR);
	p->u.l.lo = r->u.exp;
      }
      else {
	p->u.l.lo = NULL;
      }
      p->u.l.hi = NULL;
      p->u.l.id = NULL;
      $0->in_tree--;
    }
    else if (strcmp ($1, "subckt") == 0) {
      p->type = ACT_PRS_SUBCKT;
      p->u.l.lo = NULL;
      p->u.l.hi = NULL;
      if (r) {
	$A(r->type == R_STRING);
	p->u.l.id = r->u.str;
      }
      else {
	p->u.l.id = NULL;
      }
      $0->in_subckt--;
    }
    if (r) { FREE (r); }
    return p;
}}
| "(" ID 
{{X:
    if ($0->scope->Lookup ($2)) {
      $E("Identifier `%s' already defined in current scope", $2);
    }
    $0->scope->Add ($2, $0->tf->NewPInt ());
}}
":" !noreal wint_expr [ ".." wint_expr ] ":" prs_body ")"
{{X:
    act_prs_lang_t *p;
    
    $0->scope->Del ($2);

    NEW (p, act_prs_lang_t);
    p->type = ACT_PRS_LOOP;
    p->next = NULL;
    p->u.l.id = $2;
    p->u.l.lo = $4;
    
    if (OPT_EMPTY ($5)) {
      p->u.l.hi = $4;
      p->u.l.lo = NULL;
    }
    else {
      ActRet *r;
      r = OPT_VALUE ($5);
      $A(r->type == R_EXPR);
      p->u.l.hi = r->u.exp;
      FREE (r);
    }
    OPT_FREE ($5);
    p->u.l.p = $7;
    return p;
}}
/* gate, source, drain */
| "passn" size_spec "(" bool_expr_id "," bool_expr_id "," bool_expr_id ")"
{{X:
    act_prs_lang_t *p;

    NEW (p, act_prs_lang_t);
    p->type = ACT_PRS_GATE;
    p->next = NULL;
    p->u.p.sz = $2;
    p->u.p.g = $4;
    p->u.p.s = $6;
    p->u.p.d = $8;
    p->u.p._g = NULL;
    p->u.p.attr = NULL;

    return p;
}}
| "passp" size_spec "(" bool_expr_id "," bool_expr_id "," bool_expr_id ")"
{{X:
    act_prs_lang_t *p;

    NEW (p, act_prs_lang_t);
    p->type = ACT_PRS_GATE;
    p->next = NULL;
    p->u.p.sz = $2;
    p->u.p._g = $4;
    p->u.p.s = $6;
    p->u.p.d = $8;
    p->u.p.g = NULL;
    p->u.p.attr = NULL;

    return p;
}}
/* n first, then p */
| "transgate" size_spec "(" bool_expr_id "," bool_expr_id "," bool_expr_id "," bool_expr_id ")"
{{X:
    act_prs_lang_t *p;

    NEW (p, act_prs_lang_t);
    p->type = ACT_PRS_GATE;
    p->next = NULL;
    p->u.p.sz = $2;
    p->u.p.g = $4;
    p->u.p._g = $6;
    p->u.p.s = $8;
    p->u.p.d = $10;
    p->u.p.attr = NULL;

    return p;
}}
;

arrow[int]: "->" {{X: return 0; }}
| "=>" {{X: return 1; }}
| "#>" {{X: return 2; }}
;

dir[int]: "+"  {{X: return 1; }}
| "-" {{X: return 0; }}
;

tree_subckt_spec: "<" wint_expr ">"
{{X:
    ActRet *r;
    NEW (r, ActRet);
    r->type = R_EXPR;
    r->u.exp = $2;
    return r;
}}
| "<" STRING ">"
{{X:
    ActRet *r;
    NEW (r, ActRet);
    r->type = R_STRING;
    r->u.str = $2;
    return r;
}}
;

/*
  CONSISTENCY: Check _process_id in prs.c
*/
bool_expr_id[ActId *]: expr_id
{{X:
    int t;
    t = act_type_var ($0->scope, $1);
    if (t != T_BOOL) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is not of type bool\n");
      exit (1);
    }
    return $1;
}}
;

bool_expr_id_or_array[ActId *]: expr_id
{{X:
    int t;
    t = act_type_var ($0->scope, $1);
    if (t != T_BOOL && t != (T_BOOL|T_ARRAYOF)) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is not of type bool or bool[]\n");
      exit (1);
    }
    return $1;
}}
;


/*
  CONSISTENCY: MAKE SURE THIS IS CONSISTENT WITH prs.c
  XXX: main change: this one allows real expressions, whereas
      prs.c is integer expressions.

      < width , length, flavor : somethingelse >

*/
size_spec[act_size_spec_t *]: "<" wnumber_expr [ "," wnumber_expr ] [ "," ID ] [ ";" wint_expr ] ">"
{{X:
    act_size_spec_t *s;
    
    NEW (s, act_size_spec_t);
    s->w = $2;
    s->l = NULL;
    s->flavor = 0;
    s->folds = NULL;
    
    if (!OPT_EMPTY ($3)) {
      ActRet *r;

      r = OPT_VALUE ($3);
      $A(r->type == R_EXPR);
      s->l = r->u.exp;
      FREE (r);
    }
    OPT_FREE ($3);

    if (!OPT_EMPTY ($4)) {
      ActRet *r;

      r = OPT_VALUE ($4);
      $A(r->type == R_STRING);

      s->flavor = act_dev_string_to_value (r->u.str);
      if (s->flavor == -1) {
	$E("Unknown device flavor ``%s''", r->u.str);
      }
      FREE (r);
    }
    OPT_FREE ($4);

    if (!OPT_EMPTY ($5)) {
      ActRet *r;
      
      r = OPT_VALUE ($5);
      $A(r->type == R_EXPR);
      s->folds = r->u.exp;
      FREE (r);
    }
    OPT_FREE ($5);
    return s;
}}
|  /* empty */
;

/*
  Specification body: need to add assume-guarntee clause once we have
  converged on a syntax for it.
*/
spec_body[act_spec *]: [ requires_clause ] [ ensures_clause ] [ generic_clause ]
{{X:
    act_spec *t = NULL;
    if (!OPT_EMPTY ($1)) {
      ActRet *r;
      r = OPT_VALUE ($1);
      $A(r->type == R_SPEC_LANG);
      t = r->u.spec;
      FREE (r);
    }
    OPT_FREE ($1);
    if (!OPT_EMPTY ($2)) {
      act_spec *tmp;
      ActRet *r;
      r = OPT_VALUE ($2);
      $A(r->type == R_SPEC_LANG);
      if (!t) {
	t = r->u.spec;
      }
      else {
	tmp = t;
	while (tmp->next) {
	  tmp = tmp->next;
	}
	tmp->next = r->u.spec;
      }
      FREE (r);
    }
    OPT_FREE ($2);
    if (!OPT_EMPTY ($3)) {
      act_spec *tmp;
      ActRet *r;
      r = OPT_VALUE ($3);
      $A(r->type == R_SPEC_LANG);
      if (!t) {
	t = r->u.spec;
      }
      else {
	tmp = t;
	while (tmp->next) {
	  tmp = tmp->next;
	}
	tmp->next = r->u.spec;
      }
      FREE (r);
    }
    OPT_FREE ($3);
    return t;
}}
;

requires_clause[act_spec *]: "requires"
{{X: $0->req_ensures = 1; }}
"{" base_spec_body "}"
{{X:
    $0->req_ensures = -1;
    return $3;
}}
;

ensures_clause[act_spec *]: "ensures" 
{{X: $0->req_ensures = 0; }}
"{" base_spec_body "}"
{{X:
    $0->req_ensures = -1;
    return $3;
}}
;

generic_clause[act_spec *]: base_spec_body
{{X:
    act_spec *tmp;
    for (tmp = $1; tmp; tmp = tmp->next) {
      tmp->isrequires = -1;
    }
    return $1;
}}
;

base_spec_body[act_spec *]: spec_body_item base_spec_body
{{X:
    $1->next = $2;
    return $1;
}}
| spec_body_item
{{X:
    $1->isrequires = $0->req_ensures;
    return $1;
}}
;


spec_body_item[act_spec *]: ID "(" { bool_expr_id_or_array "," }* ")"
{{X:
    int count = config_get_table_size ("act.spec_types");
    char **specs = config_get_table_string ("act.spec_types");
    int i;

    for (i=0; i < count; i++) {
      if (strcmp ($1, specs[i]) == 0)
	break;
    }
    if (i == count) {
      $e("Unknown spec body directive ``%s''\n", $1);
      fprintf ($f, "Valid:");
      for (i=0; i < count; i++) {
	fprintf ($f, " %s", specs[i]);
      }
      fprintf ($f, "\n");
      exit (1);
    }

    listitem_t *li;

    act_spec *s;

    NEW (s, act_spec);

    s->count = list_length ($3);
    s->type = i;
    MALLOC (s->ids, ActId *, s->count);
    i = 0;
    for (li = list_first ($3); li; li = list_next (li)) {
      s->ids[i++] = (ActId *)list_value (li);
    }
    s->extra = NULL;
    s->next = NULL;
    list_free ($3);
    return s;
}}
| "timing" [ bool_expr_id [ dir ] ":" ] [ "?" ] bool_expr_id_or_array [ "*" ] [ dir ] "<" [ "[" wint_expr "]" ] bool_expr_id_or_array [ "*" ] [ dir ]
{{X:
    /* a timing fork */
    act_spec *s;
    int i;
    
    NEW (s, act_spec);
    s->type = -1;
    s->count = 4; 		/* the last value is the delay
				   margin, if any */
    MALLOC (s->ids, ActId *, s->count);
    MALLOC (s->extra, int, s->count-1);
    i = 0;
    if (!OPT_EMPTY ($2)) {
      ActRet *r;
      r = OPT_VALUE ($2);
      $A(r->type == R_ID);
      s->extra[i] = 0;
      s->ids[i++] = r->u.id;
      FREE (r);

      r = OPT_VALUE2 ($2);
      $A(r->type == R_LIST);
      if (!OPT_EMPTY (r->u.l)) {
	ActRet *r2;
	r2 = OPT_VALUE (r->u.l);
	$A(r2->type == R_INT);
	s->extra[i-1] = r2->u.ival ? 1 : 2;
	FREE (r2);
      }
      OPT_FREE (r->u.l);
      FREE (r);
    }
    else {
      s->extra[i] = 0;
      s->ids[i++] = NULL;
    }
    OPT_FREE ($2);

    if (!OPT_EMPTY ($6)) {
      ActRet *r;
      r = OPT_VALUE ($6);
      $A(r->type == R_INT);
      s->extra[i] = r->u.ival ? 1 : 2;
      FREE (r);
    }
    else {
      s->extra[i] = 0;
    }
    OPT_FREE ($6);
    if (!OPT_EMPTY ($3)) {
      s->extra[i] |= 0x4;
    }
    OPT_FREE ($3);

    // $5
    if (!OPT_EMPTY ($5)) {
      s->extra[i] |= 0x8;
    }
    OPT_FREE ($5);
    
    s->ids[i++] = $4;
    if (!OPT_EMPTY ($11)) {
      ActRet *r;
      r = OPT_VALUE ($11);
      $A(r->type == R_INT);
      s->extra[i] = r->u.ival ? 1 : 2;
      FREE (r);
    }
    else {
      s->extra[i] = 0;
    }
    OPT_FREE ($11);

    // $10

    if (!OPT_EMPTY ($10)) {
      s->extra[i] |= 0x8;
    }
    OPT_FREE ($10);
    
    s->ids[i++] = $9;
    s->next = NULL;

    if (OPT_EMPTY ($8)) {
      s->ids[i] = NULL;
    }
    else {
      ActRet *r;
      r = OPT_VALUE ($8);
      $A(r->type == R_EXPR);
      s->ids[i] = (ActId *) r->u.exp;
      FREE (r);
    }
    OPT_FREE ($8);
    return s;
}}
;

/*
  Sizing body: specify drive strength for rules
*/
lang_size[ActBody *]: "sizing" "{"
{{X:
    NEW ($0->sizing_info, act_sizing);
    $0->sizing_info->p_specified = 0;
    $0->sizing_info->unit_n_specified = 0;
    A_INIT ($0->sizing_info->d);
}}
[ size_body ]
"}"
{{X:
    act_sizing *sz;
    OPT_FREE ($3);
    sz = $0->sizing_info;
    $0->sizing_info = NULL;
    return new ActBody_Lang (sz);
}}
;

size_directive: bool_expr_id "{" dir wreal_expr ["," dir wreal_expr ] "}"
{{X:
    A_NEW ($0->sizing_info->d, act_sizing_directive);
    A_NEXT ($0->sizing_info->d).id = $1;
    if ($3) {
      A_NEXT ($0->sizing_info->d).eup = $4;
      A_NEXT ($0->sizing_info->d).edn = NULL;
    }
    else {
      A_NEXT ($0->sizing_info->d).edn = $4;
      A_NEXT ($0->sizing_info->d).eup = NULL;
    }
    if (!OPT_EMPTY ($5)) {
      ActRet *r1, *r2;
      r1 = OPT_VALUE ($5);
      r2 = OPT_VALUE2 ($5);
      $A(r1->type == R_INT);
      $A(r2->type == R_EXPR);
      if (r1->u.ival == $3) {
	$E("Sizing directive with duplicate drive strength directions?");
      }
      if (r1->u.ival) {
	A_NEXT ($0->sizing_info->d).eup = r2->u.exp;
      }
      else {
	A_NEXT ($0->sizing_info->d).edn = r2->u.exp;
      }
      FREE (r1);
      FREE (r2);
    }
    list_free ($5);
    A_INC ($0->sizing_info->d);
    return NULL;
}}
| ID wreal_expr
{{X:
    /* ID can be:
           p_n_mode  0/1  0 = default, 1 = sqrt sizing
	   unit_n 10
    */
    if (strcmp ($1, "p_n_mode") == 0) {
      $0->sizing_info->p_specified = 1;
      $0->sizing_info->p_n_mode_e = $2;
    }
    else if (strcmp ($1, "unit_n") == 0) {
      $0->sizing_info->unit_n_specified = 1;
      $0->sizing_info->unit_n_e = $2;
    }
    else {
      $E("Unknown sizing directive ``%s''", $1);
    }
    return NULL;
}}
;

size_body: { size_directive ";" }*
{{X:
    list_free ($1);
    return NULL;
}}  
;

lang_refine[ActBody *]: "refine" "{"
refine_body "}"
{{X:
    act_refine *r;
    ActBody *b;
    NEW (r, act_refine);
    r->b = $3;

    b = new ActBody_Lang (r);
    return b;
}}
;

refine_body[ActBody *]: refine_item refine_body
{{X:
    if ($1 == NULL) {
      return $2;
    }
    if ($2 == NULL) {
      return $1;
    }
    $1->Append ($2);
    return $1;
}}
| refine_item
{{X:
    return $1;
}}
;

refine_item[ActBody *]: instance
{{X:
    return $1;
}}
| connection
{{X:
    return $1;
}}
| alias
{{X:
    return $1;
}}
;
