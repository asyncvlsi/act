/*************************************************************************
 *
 *  Copyright (c) 2011-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */

/* 
   Language bodies
*/

language_body[ActBody *]: lang_chp 
| lang_hse 
| lang_prs 
{{X:
    return $1;
}}
| lang_spec 
| lang_size 
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

lang_prs[ActBody *]: "prs" [ supply_spec ] "{" [ prs_body ] "}" 
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
    }
    if (p) {
      b = new ActBody_Lang (p);
    }
    $0->supply.vdd = NULL;
    $0->supply.gnd = NULL;
    $0->supply.psc = NULL;
    $0->supply.nsc = NULL;
    OPT_FREE ($2);
    return b;
}}
;

lang_spec[ActBody *]: "spec" "{" [ spec_body ] "}"
;

chp_body[act_chp_lang_t *]: { chp_comma_list ";" }*
{{X:
    act_chp_lang_t *c;

    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SEMI;
    c->u.semi_comma.cmd = $1;
    return c;
}}
;

chp_comma_list[act_chp_lang_t *]: { chp_body_item "," }*
{{X:
    act_chp_lang_t *c;

    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_COMMA;
    c->u.semi_comma.cmd = $1;
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

send_stmt[act_chp_lang_t *]: expr_id "!" send_data
{{X:
    $3->u.comm.chan = $1;
    return $3;
}}
    
;

send_data[act_chp_lang_t *]: w_expr 
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SEND;
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
    c->u.comm.chan = NULL;
    c->u.comm.rhs = $2;
    return c;
}}
;

recv_stmt[act_chp_lang_t *]: expr_id "?" recv_id
{{X:
    $3->u.comm.chan = $1;
    return $3;
}}
;

recv_id[act_chp_lang_t *]: expr_id
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_RECV;
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
    c->u.assign.id = $1;
    c->u.assign.e = $3;
    return c;
}}
| expr_id dir
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_ASSIGN;
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
    NEW (c->u.gc, struct act_chp_gc);
    c->u.gc->g = $2;
    c->u.gc->s = NULL;
    c->u.gc->next = NULL;
    return c;
}}
;


guarded_cmd[act_chp_gc_t *]: wbool_expr "->" chp_body
{{X:
    act_chp_gc_t *gc;
    NEW (gc, act_chp_gc_t);
    gc->g = $1;
    gc->s = $3;
    gc->next = NULL;
    return gc;
}}
| "else" "->" chp_body
{{X:
    act_chp_gc_t *gc;
    NEW (gc, act_chp_gc_t);
    gc->g = NULL;
    gc->s = $3;
    gc->next = NULL;
    return gc;
}}
;

loop_stmt[act_chp_lang_t *]: "*[" chp_body "]"
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_LOOP;
    NEW (c->u.gc, act_chp_gc_t);
    c->u.gc->next = NULL;
    c->u.gc->g = NULL;
    c->u.gc->s = $2;
    return c;
}}
| "*[" { guarded_cmd "[]" }* "]"
{{X:
    return apply_X_select_stmt_opt0 ($0, $2);
}}
;

hse_body[act_chp_lang_t *]: { hse_body_item ";" }*
{{X:
    return apply_X_chp_body_opt0 ($0, $1);
}}
;

hse_body_item[act_chp_lang_t *]: { hse_assign_stmt "," }* 
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

hse_assign_stmt[act_chp_lang_t *]: expr_id dir 
{{X:
    return apply_X_assign_stmt_opt1 ($0, $1, $2);
}}
;

hse_select_stmt[act_chp_lang_t *]: "[" { hse_guarded_cmd "[]" }* "]"
{{X:
    return apply_X_select_stmt_opt0 ($0, $2);
}}
| "[" wbool_expr "]" 
{{X:
    return apply_X_select_stmt_opt1 ($0, $2);
}}
;

hse_guarded_cmd[act_chp_gc_t *]: wbool_expr "->" hse_body
{{X:
    return apply_X_guarded_cmd_opt0 ($0, $1, $3);
}}
| "else" "->" hse_body
{{X:
    return apply_X_guarded_cmd_opt1 ($0, $3);
}}
;

hse_loop_stmt[act_chp_lang_t *]: "*[" hse_body "]"
{{X:
    return apply_X_loop_stmt_opt0 ($0, $2);
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
    $2->next=  $3;
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
      r = (ActRet *)list_value (li);
      $A(r);
      $A(r->type == R_STRING);
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
    act_prs_lang_t *p;

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
| "(" [":"] ID 
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier `%s' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt ());
    OPT_FREE ($2);
}}
":" !noreal wint_expr [ ".." wint_expr ] ":" prs_body ")"
{{X:
    act_prs_lang_t *p;
    $0->scope->Del ($3);

    NEW (p, act_prs_lang_t);
    p->type = ACT_PRS_LOOP;
    p->next = NULL;
    p->u.l.id = $3;
    p->u.l.lo = $5;
    
    if (OPT_EMPTY ($6)) {
      p->u.l.hi = $5;
      p->u.l.lo = NULL;
    }
    else {
      ActRet *r;
      r = OPT_VALUE ($6);
      $A(r->type == R_EXPR);
      p->u.l.hi = r->u.exp;
      FREE (r);
    }
    OPT_FREE ($6);
    p->u.l.p = $8;
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
    if (t == T_ERR) 
    if (t != T_BOOL) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is not of type bool");
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
size_spec[act_size_spec_t *]: "<" wnumber_expr [ "," wnumber_expr ] [ "," ID [ ":" INT ] ] ">"
{{X:
    act_size_spec_t *s;
    
    NEW (s, act_size_spec_t);
    s->w = $2;
    s->l = NULL;
    s->flavor = ACT_FET_STD;
    s->subflavor = -1;
    
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

      s->flavor = act_fet_string_to_value (r->u.str);
      if (s->flavor == ACT_FET_END) {
	$E("Unknown transistor flavor ``%s''", r->u.str);
      }
      FREE (r);
      
      r = OPT_VALUE2 ($4);
      $A(r->type == R_LIST);
      
      if (!OPT_EMPTY (r->u.l)) {
	ActRet *r2;

	r2 = OPT_VALUE (r->u.l);
	$A(r2->type == R_INT);
	s->subflavor = r2->u.ival;
	FREE (r2);
      }
      list_free (r->u.l);
      FREE (r);
    }
    return s;
}}
|  /* empty */
;

/*
  Specification body
*/
spec_body: spec_body_item spec_body 
| spec_body_item 
;

spec_body_item: ID "(" { bool_expr_id "," }* ")" 
| "$(" wbool_expr ")"
;

/*
  Sizing body: specify drive strength for rules
*/
lang_size: "size" "{" [ size_body ] "}"
;

strength_directive: bool_expr_id dir "->" wint_expr
;

size_body: { strength_directive ";" }*
;
