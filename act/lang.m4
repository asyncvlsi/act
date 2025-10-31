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
| lang_initialize
{{X:
    /* only permitted in the top-level global namespace */
    if ($0->u_p || $0->u_f || $0->u_i || $0->u_c) {
      $E("Initialize { } block is only permitted in the top-level namespace!");
    }
    return $1;
}}
| lang_dataflow
{{X:
    return $1;
}}
| lang_extern
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

lang_chp[ActBody *]: "chp" [ supply_spec ] "{" [ chp_body_top ] "}"
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
      //chp->next = NULL;
      chp->is_synthesizable = 1;
      chp->c = r->u.chp;
      FREE (r);
      chp->vdd = $0->supply.vdd;
      chp->gnd = $0->supply.gnd;
      chp->nsc = $0->supply.nsc;
      chp->psc = $0->supply.psc;
    }
    OPT_FREE ($4);
    
    if (chp) {
      b = new ActBody_Lang ($l, chp);
    }
      
    $0->supply.vdd = NULL;
    $0->supply.gnd = NULL;
    $0->supply.psc = NULL;
    $0->supply.nsc = NULL;
    OPT_FREE ($2);

    return b;
}}
| "chp-txt" [ supply_spec ] "{" [ chptxt_body_top ] "}"
{{X:
    return apply_X_lang_chp_opt0 ($0,$2,$4);
}}
;

lang_hse[ActBody *]: "hse" [ supply_spec ] "{" [ hse_bodies ] "}" 
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
      //chp->next = NULL;
      chp->is_synthesizable = 1;
      chp->c = r->u.chp;
      FREE (r);
      chp->vdd = $0->supply.vdd;
      chp->gnd = $0->supply.gnd;
      chp->nsc = $0->supply.nsc;
      chp->psc = $0->supply.psc;
    }
    
    if (chp) {
      b = new ActBody_Lang ($l, chp, 1);
    }
      
    $0->supply.vdd = NULL;
    $0->supply.gnd = NULL;
    $0->supply.psc = NULL;
    $0->supply.nsc = NULL;
    OPT_FREE ($2);

    return b;
}}
;

lang_prs[ActBody *]: "prs" [ supply_spec ] [ "*" ] "{"
{{X:
    $0->attr_num = config_get_table_size ("act.prs_attr");
    $0->attr_table = config_get_table_string ("act.prs_attr");
}}
[ prs_body ] "}" 
{{X:
    ActBody *b;
    act_prs *p;

    b = NULL;

    NEW (p, act_prs);
    p->leak_adjust = 0;
    p->p = NULL;
    p->vdd = $0->supply.vdd;
    p->gnd = $0->supply.gnd;
    p->nsc = $0->supply.nsc;
    p->psc = $0->supply.psc;
    p->next = NULL;

    if (!OPT_EMPTY ($3)) {
      p->leak_adjust = 1;
    }
    OPT_FREE ($3);
    
    if (!OPT_EMPTY ($5)) {
      ActRet *r;
      r = OPT_VALUE ($5);
      $A(r->type == R_PRS_LANG);
      p->p = r->u.prs;
      FREE (r);
    }
    b = new ActBody_Lang ($l, p);
    $0->supply.vdd = NULL;
    $0->supply.gnd = NULL;
    $0->supply.psc = NULL;
    $0->supply.nsc = NULL;
    OPT_FREE ($2);
    OPT_FREE ($5);

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
      b = new ActBody_Lang ($l, $3);
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
      c->label = NULL;
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

chptxt_body[act_chp_lang_t *]: { chp_txtcomma_list ";" }* 
{{X:
    return apply_X_chp_body_opt0 ($0, $1);
}}
;

chp_comma_list[act_chp_lang_t *]: { chp_body_item "," }*
{{X:
    act_chp_lang_t *c;

    if (list_length ($1) > 1) {
      NEW (c, act_chp_lang_t);
      c->label = NULL;
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

chp_body_top[act_chp_lang_t *]: { par_chp_body "||" }*
{{X:
    return apply_X_chp_comma_list_opt0 ($0, $1);
}}
;

par_chp_body[act_chp_lang_t *]: chp_body
{{X:
    return $1;
}}
| "(" "||" ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" chp_body_top ")"
{{X:
    act_chp_lang_t *c;
    Expr *hi = NULL;
    $0->scope->Del ($3);
    if (!OPT_EMPTY ($6)) {
      ActRet *r;
      r = OPT_VALUE ($6);
      $A(r->type == R_EXPR);
      hi = r->u.exp;
      FREE (r);
    }
    OPT_FREE ($6);

    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_COMMALOOP;
    c->label = NULL;
    c->space = NULL;
    c->u.loop.id = $3;
    c->u.loop.lo = $5;
    c->u.loop.hi = hi;
    c->u.loop.body = $8;
    return c;
}}
;


chptxt_body_top[act_chp_lang_t *]: { par_chptxt_body "||" }*
{{X:
    return apply_X_chp_comma_list_opt0 ($0, $1);
}}
;

par_chptxt_body[act_chp_lang_t *]: chptxt_body
{{X:
    return $1;
}}
| "(" "||" ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" chptxt_body_top ")"
{{X:
    act_chp_lang_t *c;
    Expr *hi = NULL;
    $0->scope->Del ($3);
    if (!OPT_EMPTY ($6)) {
      ActRet *r;
      r = OPT_VALUE ($6);
      $A(r->type == R_EXPR);
      hi = r->u.exp;
      FREE (r);
    }
    OPT_FREE ($6);

    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_COMMALOOP;
    c->label = NULL;
    c->space = NULL;
    c->u.loop.id = $3;
    c->u.loop.lo = $5;
    c->u.loop.hi = hi;
    c->u.loop.body = $8;
    return c;
}}
;


chp_txtcomma_list[act_chp_lang_t *]: { chp_txtbody_item "," }*
{{X:
    return apply_X_chp_comma_list_opt0 ($0, $1);
}}
;


chp_body_item[act_chp_lang_t *]: [ ID ":" ] base_stmt
{{X:
    const char *lab;
    if (OPT_EMPTY ($1)) {
      lab = NULL;
    }
    else {
      ActRet *r = OPT_VALUE ($1);
      $A(r->type == R_STRING);
      lab = r->u.str;
      FREE (r);
    }
    OPT_FREE ($1);
    $2->label = lab;
    return $2;
}}
| [ ID ":" ] select_stmt
{{X:
    return apply_X_chp_body_item_opt0 ($0, $1, $2);
}}
| [ ID ":" ] loop_stmt
{{X:
    return apply_X_chp_body_item_opt0 ($0, $1, $2);
}}
| "(" ";" ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" chp_body ")"
{{X:
    act_chp_lang_t *c;
    Expr *hi = NULL;
    $0->scope->Del ($3);
    if (!OPT_EMPTY ($6)) {
      ActRet *r;
      r = OPT_VALUE ($6);
      $A(r->type == R_EXPR);
      hi = r->u.exp;
      FREE (r);
    }
    OPT_FREE ($6);

    NEW (c, act_chp_lang_t);
    c->label = NULL;
    c->space = NULL;
    c->type = ACT_CHP_SEMILOOP;
    c->u.loop.id = $3;
    c->u.loop.lo = $5;
    c->u.loop.hi = hi;
    c->u.loop.body = $8;
    return c;
}}
| "(" "," ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" chp_body ")"
{{X:
    return apply_X_par_chp_body_opt1 ($0, $3, $5, $6, $8);
}}
;


chp_txtbody_item[act_chp_lang_t *]: [ ID ":" ] txtselectloop_stmt
{{X:
    return apply_X_chp_body_item_opt0 ($0, $1, $2);
}}
| [ ID ":" ] txtbase_stmt
{{X:
    return apply_X_chp_body_item_opt0 ($0, $1, $2);
}}
| "(" ";" ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" chptxt_body ")"
{{X:
    return apply_X_chp_body_item_opt3 ($0, $3, $5, $6, $8);
}}
| "(" "," ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" chptxt_body ")"
{{X:
    return apply_X_chp_body_item_opt4 ($0, $3, $5, $6, $8);
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
    c->label = NULL;
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
    c->label = NULL;
    c->space = NULL;
    c->u.func.name = string_create ($1);
    c->u.func.rhs = $3;
    return c;
}}
| { base_id "." }* "(" [ { w_expr "," }** ] ")"
{{X:
    act_chp_lang_t *c;
    ActId *id1;
    const char *methname;
    listitem_t *li;
    
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_MACRO;
    c->label = NULL;
    c->space = NULL;

    id1 = (ActId *) list_value (list_first ($1));
    if (list_length ($1) < 2) {
      $E("This looks like a method call for ``%s'', but is missing a method name.", id1->getName());
    }

    /* method name is the last value */
    ActId *id2 = (ActId *) list_value (list_tail ($1));
    methname = id2->getName();
    if (id2->arrayInfo()) {
      $E("Method name cannot have square brackets!");
    }
    delete id2;

    c->u.macro.id = id1;
    ActId *cur = id1;

    InstType *it = $0->scope->FullLookup (id1->getName());
    if (!it) {
      $E("The identifier ``%s'' does not exist in the current scope.",
	 id1->getName());
    }
    if (cur->isRange()) {
      $e("Invalid use of array sub-range specifier: ");
      id1->Print ($f);
      fprintf ($f, "\n");
      exit (1);
    }
    if (cur->isDeref()) {
      if (!it->arrayInfo() || (cur->arrayInfo()->nDims () != it->arrayInfo()->nDims ())) {
	if (it->arrayInfo ()) {
	  $e("Mismatch in array dimensions (%d v/s %d): ",
	     cur->arrayInfo()->nDims(), it->arrayInfo()->nDims ());
	}
	else {
	  $e("Array reference (%d dims) for a non-arrayed identifier: ",
	     cur->arrayInfo()->nDims ());
	}
	id1->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
    }
    UserDef *ud = NULL;
    
    for (li = list_next (list_first ($1)); li != list_tail ($1);
	 li = list_next (li)) {
      ud = dynamic_cast<UserDef *> (it->BaseType());
      if (!ud) {
	$e("Invalid use of ``.'' for an identifier that is not a user-defined type: ");
	id1->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }

      if (cur->isRange()) {
	$e("Invalid use of array sub-range specifier: ");
	id1->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
      if (cur->isDeref()) {
	if (!it->arrayInfo() || (cur->arrayInfo()->nDims () != it->arrayInfo()->nDims ())) {
	  if (it->arrayInfo ()) {
	    $e("Mismatch in array dimensions (%d v/s %d): ",
	       cur->arrayInfo()->nDims(), it->arrayInfo()->nDims ());
	  }
	  else {
	    $e("Array reference (%d dims) for a non-arrayed identifier: ",
	       cur->arrayInfo()->nDims ());
	  }
	  id1->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
      }

      it = ud->Lookup ((ActId *) list_value (li));
      if (!it) {
	$e("Port name ``%s'' does not exist for the identifier: ", 
	   ((ActId *)list_value (li))->getName());
	id1->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
      else {
	if (!ud->isPort (((ActId *)list_value (li))->getName())) {
	  $E("``%s'' is not a port for ``%s''", ((ActId *)list_value (li))->getName(),
	     ud->getName());
	}
      }
      cur->Append ((ActId *)list_value (li));
      cur = (ActId *) list_value (li);
    }

    if (!TypeFactory::isUserType (it)) {
      $e("''");
      id1->Print ($f);
      fprintf ($f, "'': macro calls can only be made to a user-defined type.\n");
      exit (1);
    }

    UserDef *u = dynamic_cast<UserDef *> (it->BaseType());
    $A(u);

    if (!u->isDefined() || !u->getMacro (methname)) {
      $e("Macro ``%s'' not found for ``", methname);
      id1->Print ($f);
      fprintf ($f, "'' (type ``%s'')\n", u->getName());
      exit (1);
    }

    if (u->getMacro (methname)->getRetType()) {
      /* Make this into a warning, and change how this is inlined */
      $E("Macro ``%s'' is a function macro; need to use returned value.",
	 methname);
    }

    c->u.macro.name = string_create (methname);
    
    if (OPT_EMPTY ($3)) {
      c->u.macro.rhs = NULL;
    }
    else {
      ActRet *r = OPT_VALUE ($3);
      $A(r->type == R_LIST);
      c->u.macro.rhs = list_new ();
      for (listitem_t *li = list_first (r->u.l); li; li = list_next (li)) {
	ActRet *r2 = (ActRet *) list_value (li);
	$A(r2->type == R_EXPR);
	list_append (c->u.macro.rhs, r2->u.exp);
	FREE (r2);
      }
      list_free (r->u.l);
      FREE (r);
    }
    OPT_FREE ($3);
    list_free ($1);
    return c;
}}
;


txtbase_stmt[act_chp_lang_t *]: send_stmt
{{X:
    return apply_X_base_stmt_opt0 ($0, $1);
}}
| recv_stmt
{{X:
    return apply_X_base_stmt_opt1 ($0, $1);
}}
| assign_stmt
{{X:
    return apply_X_base_stmt_opt2 ($0, $1);
}}
| "skip"
{{X:
    return apply_X_base_stmt_opt3 ($0);
}}
| "(" chptxt_body ")"
{{X:
    return apply_X_base_stmt_opt4 ($0, $2);
}}
| "wait-for" "(" wbool_allow_chan_expr ")"
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SELECT;
    c->label = NULL;
    c->space = NULL;
    NEW (c->u.gc, act_chp_gc_t);
    c->u.gc->id = NULL;
    c->u.gc->next = NULL;
    c->u.gc->s = NULL;
    c->u.gc->g = $3;
    return c;
}}
| ID "(" chan_expr_id "," gen_assignable_id ")"
{{X:
    ActRet *r;
    if (strcmp ($1, "recv") == 0) {
      list_t *l = list_new ();
      NEW (r, ActRet);
      r->type = R_ID;
      r->u.id = $5;
      list_append (l, r);
      return apply_X_recv_stmt_opt0 ($0, $3, 0, l, list_new ());
    }
    else if (strcmp ($1, "send") == 0) {
      Expr *e;
      list_t *l;
      e = act_expr_var ($5);
      if ($0->is_assignable_override == 0) {
	/* switch to bool(e), repurpose storage */
	e->type = E_BUILTIN_BOOL;
	e->u.e.l = act_expr_var ($5);
	e->u.e.r = NULL;
      }
      else if ($0->is_assignable_override == 1) {
	/*  switch to int(e), repurpose storage */
	e->type = E_BUILTIN_INT;
	e->u.e.l = act_expr_var ($5);
	e->u.e.r = NULL;
      }
      l = list_new ();
      NEW (r, ActRet);
      r->type = R_EXPR;
      r->u.exp = e;
      list_append (l, r);
      return apply_X_send_stmt_opt0 ($0, $3, 0, l, list_new ());
    }
    else {
      list_t *l;
      act_func_arguments_t *arg;
      NEW (arg, struct act_func_arguments);
      arg->isstring = 0;
      arg->u.e = act_expr_var ($3);
      l = list_new ();
      list_append (l, arg);

      NEW (arg, struct act_func_arguments);
      arg->isstring = 0;
      arg->u.e = act_expr_var ($5);
      list_append (l, arg);
      return apply_X_base_stmt_opt5 ($0, $1, l);
    }
}}
| ID "(" chan_expr_id "," w_expr ")"
{{X:
    if (strcmp ($1, "send") == 0) {
      list_t *l = list_new ();
      ActRet *r;
      NEW (r, ActRet);
      r->type = R_EXPR;
      r->u.exp = $5;
      list_append (l, r);
      return apply_X_send_stmt_opt0 ($0, $3, 0, l, list_new ());
    }
    else {
      list_t *l;
      act_func_arguments_t *arg;
      NEW (arg, struct act_func_arguments);
      arg->isstring = 0;
      arg->u.e = act_expr_var ($3);
      l = list_new ();
      list_append (l, arg);

      NEW (arg, struct act_func_arguments);
      arg->isstring = 0;
      arg->u.e = $5;
      list_append (l, arg);
      return apply_X_base_stmt_opt5 ($0, $1, l);
    }
}}
| ID "(" { chp_log_item "," }* ")"
{{X:
    return apply_X_base_stmt_opt5 ($0, $1, $3);
}}
| { base_id "." }* "(" [ { w_expr "," }** ] ")"
{{X:
    return apply_X_base_stmt_opt6 ($0, $1, $3);
}}
;


chp_log_item[act_func_arguments_t *]: w_expr
{{X:
    act_func_arguments_t *arg;
    NEW (arg, struct act_func_arguments);
    arg->isstring = 0;
    arg->u.e = $1;

    InstType *it = act_expr_insttype ($0->scope, $1, NULL, 2);
    $A(it);
    if (it->arrayInfo()) {
      $e("Can't display an entire array; please select an element.\n");
      fprintf ($f, "Array expression: ");
      print_uexpr ($f, $1);
      fprintf ($f, "\n");
      exit (1);
    }
    if (TypeFactory::isStructure (it)) {
      $e("Can't display a structure; please select a member.\n");
      fprintf ($f, "Expression: ");
      print_uexpr ($f, $1);
      fprintf ($f, "\n");
      exit (1);
    }
    delete it;
    return arg;
}}
|  STRING
{{X:
    act_func_arguments_t *arg;
    char *tmp;
    NEW (arg, struct act_func_arguments);
    arg->isstring = 1;
    MALLOC (tmp, char, strlen ($1)-1);
    strncpy (tmp, $1+1, strlen ($1)-2);
    tmp[strlen($1)-2] = '\0';
    arg->u.s = string_create (tmp);
    FREE (tmp);
    return arg;
}}
;

send_stmt[act_chp_lang_t *]: chan_expr_id snd_type [ w_expr_chp ]
                                                  [ rcv_type gen_assignable_id ]
{{X:
    act_chp_lang_t *c;
    InstType *it;
    Channel *ch1;
    Chan *ch2;
    
    /*t = */(void)(act_type_var ($0->scope, $1, &it));

    if (!TypeFactory::isChanType (it)) {
      $E("Send action requires a channel identifier ``%s''", $1->getName());
    }

    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SEND;
    c->label = NULL;
    c->space = NULL;
    c->u.comm.chan = $1;
    c->u.comm.var = NULL;
    c->u.comm.e = NULL;
    c->u.comm.flavor = $2;

    if (OPT_EMPTY ($3)) {
      if (!OPT_EMPTY ($4)) {
	$E("Bidirectional data communication needs data in both directions!");
      }
    }
    else {
      ActRet *r;
      r = OPT_VALUE ($3);
      $A(r->type == R_EXPR);
      c->u.comm.e = r->u.exp;
      FREE (r);

      if (!OPT_EMPTY ($4)) {
	r = OPT_VALUE ($4);
	$A(r->type == R_INT);
	if (c->u.comm.flavor != r->u.ival) {
	  $E("Cannot mix flavors of ! and ?");
	}
	FREE (r);
	r = OPT_VALUE2 ($4);
	$A(r->type == R_ID);
	c->u.comm.var = r->u.id;
	FREE (r);
      }
    }
    int isbidir = 0;
    ch1 = dynamic_cast<Channel *> (it->BaseType());
    if (ch1) {
      isbidir = ch1->isBiDirectional();
      Assert (ch1->root(), "What?");
      ch2 = dynamic_cast<Chan *>(ch1->root()->BaseType());
      Assert (ch2, "What?");
    }
    else {
      ch2 = dynamic_cast<Chan *> (it->BaseType());
      Assert (ch2, "Hmm");
      isbidir = ch2->isBiDirectional();
    }
    if (isbidir && OPT_EMPTY ($4)) {
      $E("Bidirectional channel: missing ? operator");
    }
    else if (!isbidir && !OPT_EMPTY ($4)) {
      $E("Unidirectional channel with bidirectional syntax?");
    }
    OPT_FREE ($3);
    OPT_FREE ($4);

    /* now typecheck channel */
    if (!act_type_chan ($0->scope, ch2, 1, c->u.comm.e, c->u.comm.var,
			$0->is_assignable_override)) {
      $E("CHP send: type-checking failed.\n\t%s", act_type_errmsg());
    }
    c->u.comm.convert = ($0->is_assignable_override + 1);

    if (c->u.comm.flavor == 2 && (c->u.comm.var || c->u.comm.e)) {
      $E("Second half of communication action cannot have data");
    }

    $0->is_assignable_override = -1;

    return c;
}}
;

snd_type[int]: "!"
{{X: return 0; }}
| "!+"
{{X: return 1; }}
| "!-"
{{X: return 2; }}
;

gen_assignable_id[ActId *]: assignable_expr_id
{{X:
    $0->is_assignable_override = -1;
    return $1;
}}
| "bool" "(" assignable_expr_id ")"
{{X:
    $0->is_assignable_override = 0;
    return $3;
}}
| "int" "(" assignable_expr_id ")"
{{X:
    $0->is_assignable_override = 1;
    return $3;
}}
;

recv_stmt[act_chp_lang_t *]: chan_expr_id rcv_type [ gen_assignable_id ]
                                        [ snd_type w_expr_chp ]
{{X:
    act_chp_lang_t *c;
    Channel *ch1;
    Chan *ch2;
    InstType *it;
    
    /*t = */(void)(act_type_var ($0->scope, $1, &it));

    if (!TypeFactory::isChanType (it)) {
      $E("Receive action requires a channel identifier ``%s''", $1->getName());
    }
    
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_RECV;
    c->label = NULL;
    c->space = NULL;
    c->u.comm.chan = $1;
    c->u.comm.flavor = $2;
    c->u.comm.var = NULL;
    c->u.comm.e = NULL;

    if (OPT_EMPTY ($3)) {
      if (!OPT_EMPTY ($4)) {
	$E("Bidirectional data communication needs data in both directions!");
      }
    }
    else {
      ActRet *r;
      r = OPT_VALUE ($3);
      $A(r->type == R_ID);
      c->u.comm.var = r->u.id;
      FREE (r);

      if (!OPT_EMPTY ($4)) {
	r = OPT_VALUE ($4);
	$A(r->type == R_INT);
	if (c->u.comm.flavor != r->u.ival) {
	  $E("Cannot mix flavors of ! and ?");
	}
	FREE (r);
	r = OPT_VALUE2 ($4);
	$A(r->type == R_EXPR);
	c->u.comm.e = r->u.exp;
	FREE (r);
      }
    }
    int isbidir = 0;
    ch1 = dynamic_cast<Channel *> (it->BaseType());
    if (ch1) {
      isbidir = ch1->isBiDirectional();
      Assert (ch1->root(), "What?");
      ch2 = dynamic_cast<Chan *>(ch1->root()->BaseType());
      Assert (ch2, "What?");
    }
    else {
      ch2 = dynamic_cast<Chan *> (it->BaseType());
      Assert (ch2, "Hmm");
      isbidir = ch2->isBiDirectional();
    }
    if (isbidir && OPT_EMPTY ($4)) {
      $E("Bidirectional channel: missing ? operator");
    }
    else if (!isbidir && !OPT_EMPTY ($4)) {
      $E("Unidirectional channel with bidirectional syntax?");
    }
    OPT_FREE ($3);
    OPT_FREE ($4);

    if (!act_type_chan ($0->scope, ch2, 0, c->u.comm.e, c->u.comm.var,
			$0->is_assignable_override)) {
      $E("CHP receive: type-checking failed.\n\t%s", act_type_errmsg());
    }
    c->u.comm.convert = ($0->is_assignable_override + 1);

    if (c->u.comm.flavor == 2 && (c->u.comm.var || c->u.comm.e)) {
      $E("Second half of communication action cannot have data");
    }

    $0->is_assignable_override = -1;
    
    return c;
}}
;

rcv_type[int]: "?"
{{X: return 0; }}
| "?+"
{{X: return 1; }}
| "?-"
{{X: return 2; }}
;

assign_stmt[act_chp_lang_t *]: assignable_expr_id [ "{" !noreal wpint_expr [ ".." wpint_expr ] "}" ] ":="
{{X:
    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
}}
w_expr_chp
{{X:
    act_chp_lang_t *c;

    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_ASSIGN;
    c->label = NULL;
    c->space = NULL;
    c->u.assign.id = $1;
    c->u.assign.e = $4;

    InstType *lhs, *rhs;
    int val;
    val = act_type_var ($0->scope, $1, &lhs);
    rhs = act_expr_insttype ($0->scope, $4, NULL, 2);

    Array *lhs_a, *rhs_a;

    lhs_a = lhs->arrayInfo();
    rhs_a = rhs->arrayInfo();

    lhs->clrArray ();
    rhs->clrArray ();

    if (!type_chp_check_assignable (lhs, rhs)) {
      $e("Typechecking failed on CHP assignment\n");
      fprintf ($f, "  stmt: ");
      $1->Print ($f, NULL);
      fprintf ($f, " := ");
      print_uexpr ($f, $4);
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if (!OPT_EMPTY ($2)) {
      if (!T_BASETYPE_INT(val)) {
	$e("Bit-field assignment requires an integer variable.\n");
	fprintf ($f, "  stmt: ");
	$1->Print ($f, NULL);
	fprintf ($f, " := ");
	print_uexpr ($f, $4);
	fprintf ($f, "\n");
	exit (1);
      }
      ActRet *r, *r2;
      Expr *b_field, *a_field;
      r = OPT_VALUE ($2);
      $A(r->type == R_EXPR);
      b_field = r->u.exp;
      FREE (r);
      r2 = OPT_VALUE2 ($2);
      $A(r2->type == R_LIST);
      if (!OPT_EMPTY (r2->u.l)) {
	r = OPT_VALUE (r2->u.l);
	$A(r->type == R_EXPR);
	a_field = r->u.exp;
	FREE (r);
      }
      else {
	a_field = NULL;
      }
      OPT_FREE (r2->u.l);
      FREE (r2);
      Expr *tmp;
      NEW (tmp, Expr);
      tmp->type = E_BITSLICE;
      tmp->u.e.l = (Expr *) $1->Clone ();
      NEW (tmp->u.e.r, Expr);
      tmp->u.e.r->type = E_LT;
      tmp->u.e.r->u.e.l = c->u.assign.e;
      c->u.assign.e = tmp;
      tmp = tmp->u.e.r;
      NEW (tmp->u.e.r, Expr);
      tmp->u.e.r->type = E_LT;
      tmp->u.e.r->u.e.l = b_field;
      tmp->u.e.r->u.e.r = a_field;
    }
    OPT_FREE ($2);
    
    lhs->MkArray (lhs_a);
    rhs->MkArray (rhs_a);
    if (rhs_a && !rhs_a->isDeref()) {
      $E("Typechecking failed on CHP assignment: array/non-array assignment");
    }
    delete lhs;
    delete rhs;
    return c;
}}
| bool_expr_id dir
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_ASSIGN;
    c->label = NULL;
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
    c->label = NULL;
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

    /* check that only the last clause is else, if there is an else */
    gc = c->u.gc;

    if (gc && !gc->g) {
      $E("`else' cannot be the first clause in a selection statement");
    }
    while (gc) {
      if (!gc->g && gc->next) {
	$E("`else' can only be the last clause in a selection statement");
      }
      gc = gc->next;
    }
    return c;
}}
| "[" wbool_allow_chan_expr "]" 
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_SELECT;
    c->label = NULL;
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
    act_chp_gc_t *gc;
    c = apply_X_select_stmt_opt0 ($0, $2);
    c->type = ACT_CHP_SELECT_NONDET;
    
    gc = c->u.gc;
    while (gc) {
      if (!gc->g) {
	$W("`else' has been used in non-deterministic selections.\n\tI hope you know what you're doing.");
      }
      gc = gc->next;
    }

    return c;
}}
;

guarded_cmd[act_chp_gc_t *]: wbool_allow_chan_expr "->" chp_body
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
":" !noreal wpint_expr [ ".." wpint_expr ] ":" wbool_allow_chan_expr "->" chp_body ")"
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

txtselectloop_stmt[act_chp_lang_t *]: ID "{"
     txtguarded_commands_or_cmds "}"
     [ ID "(" wbool_expr ")" ]
{{X:
    act_chp_lang_t *c = NULL;
    NEW (c, act_chp_lang_t);
    
    if (strcmp ($1, "select") == 0 || strcmp ($1, "arb_select") == 0) {
      if ($0->txtchp_type == 2) {
	$E("select/arb_select require case statements in their definition");
      }
      if (!OPT_EMPTY ($5)) {
	$E("select/arb_select can't have a dangling clause");
      }
      if (strcmp ($1, "select") == 0) {
	c->type = ACT_CHP_SELECT;
      }
      else {
	c->type = ACT_CHP_SELECT_NONDET;
      }
      c->label = NULL;
      c->space = NULL;
      c->u.gc = (act_chp_gc_t *) $3;
    }
    else if (strcmp ($1, "while") == 0) {
      if (!OPT_EMPTY ($5)) {
	$E("while can't have a dangling clause");
      }
      if ($0->txtchp_type != 0) {
	$E("while { } loop must only contain cases without a default clause");
      }
      c->type = ACT_CHP_LOOP;
      c->label = NULL;
      c->space = NULL;
      c->u.gc = (act_chp_gc_t *) $3;
    }
    else if (strcmp ($1, "do") == 0) {
      ActRet *r;
      if (OPT_EMPTY ($5)) {
	$E("do-loop needs a while clause");
      }
      r = OPT_VALUE ($5);
      $A(r->type == R_STRING);
      if (strcmp (r->u.str, "while") != 0) {
	$E("do-loop needs a while clause");
      }
      FREE (r);

      if ($0->txtchp_type != 2) {
	$E("do-loop body can't have guarded commands");
      }
      
      r = OPT_VALUE2 ($5);
      $A(r->type == R_EXPR);
      c->type = ACT_CHP_DOLOOP;
      c->label = NULL;
      c->space = NULL;
      NEW (c->u.gc, act_chp_gc_t);
      c->u.gc->next = NULL;
      c->u.gc->id = NULL;
      c->u.gc->g = r->u.exp;
      FREE (r);
      c->u.gc->s = (act_chp_lang_t *) $3;
    }
    else if (strcmp ($1, "forever") == 0) {
      ActRet *r;
      if (!OPT_EMPTY ($5)) {
	$E("forever can't have a dangling clause");
      }
      if ($0->txtchp_type != 2) {
	$E("forever body can't have guarded commands");
      }
      c->type = ACT_CHP_LOOP;
      c->label = NULL;
      c->space = NULL;
      NEW (c->u.gc, act_chp_gc_t);
      c->u.gc->next = NULL;
      c->u.gc->id = NULL;
      c->u.gc->g = NULL;
      c->u.gc->s = (act_chp_lang_t *) $3;
    }
    else {
      $E("Unknown command ``%s''", $1);
    }
    OPT_FREE ($5);
    return c;
}}
| ID "(" wbool_expr ")" "{" chptxt_body "}"
{{X:
    act_chp_lang_t *c;
    if (strcmp ($1, "while") != 0) {
      $E("expecting while loop!");
    }
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_LOOP;
    c->label = NULL;
    c->space = NULL;
    NEW (c->u.gc, act_chp_gc_t);
    c->u.gc->next = NULL;
    c->u.gc->id = NULL;
    c->u.gc->g = $3;
    c->u.gc->s = $6;
    return c;
}}
;

txtguarded_commands_or_cmds[void *]: { txtgc_listitem ";" }*
{{X:
    act_chp_gc_t *gc, *ret;
    listitem_t *li;
    $0->txtchp_type = 0;

    ret = NULL;
    for (li = list_first ($1); li; li = list_next (li)) {
      gc = (act_chp_gc_t *) list_value (li);
      if (!ret) {
	ret = gc;
      }
      if (list_next (li)) {
	gc->next = (act_chp_gc_t *)list_value (list_next (li));
      }
      else {
	gc->next = NULL;
      }
      if (!gc->g) {
	$0->txtchp_type = 1;
      }
    }
    list_free ($1);
    return (void *) ret;
}}
| chptxt_body
{{X:
    $0->txtchp_type = 2;
    return (void *)$1;
}}
;

txtgc_listitem[act_chp_gc_t *]: ID wbool_allow_chan_expr ":" chptxt_body
{{X:
    if (strcmp ($1, "case") != 0) {
      $E("Expected case statement for guarded command");
    }
    return apply_X_guarded_cmd_opt0 ($0, $2, $4);
}}
| "else" ":" chptxt_body
{{X:
    return apply_X_guarded_cmd_opt2 ($0, $3);
}}
| "(" ";" ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" ID wbool_allow_chan_expr ":" chptxt_body ")"
{{X:
    if (strcmp ($8, "case") != 0) {
      $E("Expected case statement in syntactic replication construct");
    }
    return apply_X_guarded_cmd_opt1 ($0, $3, $5, $6, $9, $11);
}}
;


loop_stmt[act_chp_lang_t *]: "*[" chp_body [ "<-" wbool_expr ] "]" 
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_LOOP;
    c->label = NULL;
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
    act_chp_gc_t *gc;
    c = apply_X_select_stmt_opt0 ($0, $2);
    c->type = ACT_CHP_LOOP;

    gc = c->u.gc;
    if (gc && !gc->g) {
      $E("`else' cannot be the first clause in a loop statement");
    }
    while (gc) {
      if (!gc->g && gc->next) {
	$E("`else' can only be the last clause in a loop statement");
      }
      gc = gc->next;
    }
    return c;
}}
;

hse_bodies[act_chp_lang_t *]: hse_body_top
{{X:
    return $1;
}}
| labelled_hse_bodies
{{X:
    return $1;
}}
;

labelled_hse_bodies[act_chp_lang_t *]: { label_hse_fragment "||" }*
{{X:
    listitem_t *li;
    act_chp_lang_t *ret, *prev;
    ret = NULL;
    prev = NULL;
    for (li = list_first ($1); li; li = list_next (li)) {
      if (!ret) {
	ret = (act_chp_lang_t *)list_value (li);
	prev = ret;
      }
      else {
	prev->u.frag.next = (act_chp_lang_t *)list_value (li);
	prev = prev->u.frag.next;
      }
    }
    return ret;
}}
;

label_hse_fragment[act_chp_lang_t *]: ID ":" hse_body ":"
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_HSE_FRAGMENTS;
    c->label = $1;
    c->u.frag.nextlabel = NULL;
    c->u.frag.exit_conds = NULL;
    c->u.frag.body = $3;
    c->u.frag.next = NULL;
    $0->clang = c;
}}
hse_fragment_tail
{{X:
    act_chp_lang_t *c = $0->clang;
    $0->clang = NULL;
    return c;
}}
;

hse_fragment_tail: ID
{{X:
    $0->clang->u.frag.nextlabel = $1;
    return NULL;
}}
| "[" hse_exit_list "]"
{{X:
    $0->clang->u.frag.exit_conds = $2;
    return NULL;
}}
;

hse_exit_list[list_t *]: { hse_guarded_cmd_exit "[]" }*
{{X:
    list_t *tmp = list_new ();
    listitem_t *li;
    for (li = list_first ($1); li; li = list_next (li)) {
      list_concat (tmp, ((list_t *) list_value (li)));
      list_free ((list_t *)list_value (li));
    }
    return tmp;
}}
;

hse_guarded_cmd_exit[list_t *]: wbool_expr "->" ID
{{X:
    list_t *l = list_new ();
    list_append (l, $1);
    list_append (l, $3);
    return l;
}}
;

hse_body_top[act_chp_lang_t *]: { hse_body "||" }*
{{X:
    return apply_X_chp_comma_list_opt0 ($0, $1);
}}
;


hse_body[act_chp_lang_t *]: { hse_comma_item ";" }*
{{X:
    return apply_X_chp_body_opt0 ($0, $1);
}}
;

hse_comma_item[act_chp_lang_t *]: { hse_body_item "," }*
{{X:
    return apply_X_chp_comma_list_opt0 ($0, $1);
}}
;

hse_body_item[act_chp_lang_t *]: "(" ";" ID
{{X:
    lapply_X_chp_body_item_3_2 ($0, $3);
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" hse_body ")"
{{X:
    return apply_X_chp_body_item_opt3 ($0, $3, $5, $6, $8);
}}
| "(" "," ID
{{X:
    lapply_X_chp_body_item_4_2 ($0, $3);
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" hse_body ")"
{{X:
    return apply_X_chp_body_item_opt4 ($0, $3, $5, $6, $8);
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
| hse_assign_stmt
{{X:
    return $1;
}}
| ID "(" { chp_log_item "," }* ")" /* log */
{{X:
    act_chp_lang_t *c;
    NEW (c, act_chp_lang_t);
    c->type = ACT_CHP_FUNC;
    c->label = NULL;
    c->space = NULL;
    c->u.func.name = string_create ($1);
    c->u.func.rhs = $3;
    return c;
}}
;

hse_assign_stmt[act_chp_lang_t *]: bool_expr_id dir 
{{X:
    return apply_X_assign_stmt_opt1 ($0, $1, $2);
}}
| assignable_expr_id ":=" w_expr
{{X:
    list_t *l = list_new ();
    act_chp_lang_t *ret = apply_X_assign_stmt_opt0 ($0, $1, l, $3);
    if (!$1->Rest() && _act_is_reserved_id ($1->getName())) {
      return ret;
    }
    else {
      $E("Assignments only permitted to ``%s'' in HSE body", $1->getName());
    }
    return NULL;
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
":" !noreal wpint_expr [ ".." wpint_expr ] ":" wbool_expr "->" chp_body ")"
{{X:
    return apply_X_guarded_cmd_opt1 ($0, $3, $5, $6, $8, $10);
}}
| "else" "->" hse_body
{{X:
    return apply_X_guarded_cmd_opt2 ($0, $3);
}}
;

hse_loop_stmt[act_chp_lang_t *]: "*[" hse_body [ "<-" wbool_expr ] "]"
{{X:
    return apply_X_loop_stmt_opt0 ($0, $2, $3);
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
":" !noreal wpint_expr [ ".." wpint_expr ] ":" prs_body ")"
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
| ID size_spec "(" bool_expr_id "," bool_expr_id ")"
{{X:
    act_prs_lang_t *p;
    int count;
    char **table;
    int id = -1;

    count = config_get_table_size ("act.prs_device");
    table = config_get_table_string ("act.prs_device");

    for (id=0; id < count; id++) {
      if (strcmp ($1, table[id]) == 0) {
	break;
      }
    }
    if (id == count) {
      fprintf (stderr, "Device ``%s'' specified; valid options:", $1);
      for (id=0; id < count; id++) {
	fprintf (stderr, " %s", table[id]);
      }
      fprintf (stderr, "\n");
      $E("Unknown device specifier");
    }
    NEW (p, act_prs_lang_t);
    p->type = ACT_PRS_DEVICE + id;
    p->next = NULL;
    p->u.p.sz = $2;
    p->u.p.g = NULL;
    p->u.p._g = NULL;
    p->u.p.s = $4;
    p->u.p.d = $6;
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

tree_subckt_spec: "<" wpint_expr ">"
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
    if (_act_id_is_true_false ($1)) {
      $E("Can't use true/false as an ID!");
    }
    t = act_type_var ($0->scope, $1, NULL);
    if (T_BASETYPE (t) != T_BOOL) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is not of type bool\n");
      exit (1);
    }
    else if (t & T_ARRAYOF) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is not of type bool\n");
      exit (1);
    }
    else if (t & T_PARAM) {
      if (strcmp ($1->getName(), "self") != 0 && !$0->u_f) {
	$e("Identifier ``");
	$1->Print ($f, NULL);
	fprintf ($f, "'' is a parameter\n");
	exit (1);
      }
    }
    return $1;
}}
;

bool_expr_id_or_array[ActId *]: expr_id
{{X:
    int t;
    if (_act_id_is_true_false ($1)) {
      $E("Can't assign to true/false!");
    }
    t = act_type_var ($0->scope, $1, NULL);
    if (T_BASETYPE (t) != T_BOOL) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is not of type bool or bool[]\n");
      exit (1);
    }
    else if (t & T_PARAM) {
      if (strcmp ($1->getName(), "self") != 0 && !$0->u_f) {
	$e("Identifier ``");
	$1->Print ($f, NULL);
	fprintf ($f, "'' is a parameter\n");
	exit (1);
      }
    }
    return $1;
}}
;

chan_expr_id[ActId *]: expr_id
{{X:
    int t;
    if (_act_id_is_true_false ($1)) {
      $E("Can't use true/false as a channel name!");
    }
    t = act_type_var ($0->scope, $1, NULL);
    if (T_BASETYPE(t) != T_CHAN) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is not of a channel type\n");
      exit (1);
    }
    else if (t & T_ARRAYOF) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is of array type\n");
      exit (1);
    }
    return $1;
}}
;

assignable_expr_id[ActId *]: expr_id
{{X:
    int t;
    if (_act_id_is_true_false ($1)) {
      $E("Can't assign to true/false!");
    }
    t = act_type_var ($0->scope, $1, NULL);
    if (t == T_ERR) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'': type-checking error\n");
      fprintf ($f, "\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if (t & T_ARRAYOF) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is of array type\n");
      exit (1);
    }
    if (!T_BASETYPE_ISINTBOOL (t) && (T_BASETYPE(t) != T_DATA) &&
	(T_BASETYPE (t) != T_DATA_ENUM) &&
	(T_BASETYPE (t) != T_IFACE)) {
      $e("Identifier ``");
      $1->Print ($f, NULL);
      fprintf ($f, "'' is not of type bool/int/structure\n");
      exit (1);
    }
    if (t & T_PARAM) {
      if (strcmp ($1->getName(), "self") != 0 && !$0->u_f) {
	$e("Identifier ``");
	$1->Print ($f, NULL);
	fprintf ($f, "'' is a parameter\n");
	exit (1);
      }
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
size_spec[act_size_spec_t *]: "<" wnumber_expr [ "," wnumber_flav_expr ] [ "," ID ] [ ";" wpint_expr ] ">"
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

      if (s->l->type == E_VAR) {
	ActId *tmp = (ActId *)s->l->u.e.l;
	if (!tmp->Rest() && !tmp->arrayInfo() &&
	    (act_dev_string_to_value (tmp->getName()) != -1)) {
	  s->flavor = act_dev_string_to_value (tmp->getName());
	  FREE (s->l);
	  s->l = NULL;
	  if (!OPT_EMPTY ($4)) {
	    $E("Multiple device flavors?");
	  }
	}
      }
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
| "timing" [ bool_expr_id [ dir ] ":" ] [ "?" ] bool_expr_id_or_array [ "*" ] [ dir ] timing_type [ "[" wpint_expr "]" ] bool_expr_id_or_array [ "*" ] [ dir ]
{{X:
    /* a timing fork */
    act_spec *s;
    int i;
    int intick = 0;
    
    NEW (s, act_spec);
    s->type = $7;
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
      if (s->type == -3 || s->type == -4) {
	$E("Timing directive ``%s'' spec error", s->type == -3 ? "->" : "#>");
      }

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
      if (s->type == -3 || s->type == -4) {
	$E("Timing directive ``%s'' spec error", s->type == -3 ? "->" : "#>");
      }
      s->extra[i] = 0;
    }
    OPT_FREE ($6);
    if (!OPT_EMPTY ($3)) {
      s->extra[i] |= 0x4;
      //if (s->type == -3) {
      //$E("Timing directive ``->'' spec error");
      //}
      // we use this to say all incoming edges are ticked
      intick = 1;
    }
    OPT_FREE ($3);

    // $5
    if (!OPT_EMPTY ($5)) {
      s->extra[i] |= 0x8;
      if (s->type == -3 || s->type == -4) {
	$E("Timing directive ``%s'' spec error", s->type == -3 ? "->" : "#>");
      }
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
      if (s->type == -3 || s->type == -4) {
	$E("Timing directive ``%s'' spec error", s->type == -3 ? "->" : "#>");
      }
      s->extra[i] = 0;
    }
    OPT_FREE ($11);

    // $10

    if (!OPT_EMPTY ($10)) {
      s->extra[i] |= 0x8;
      if (s->type == -3 || s->type == -4) {
	$E("Timing directive ``%s'' spec error", s->type == -3 ? "->" : "#>");
      }
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
      if (s->type == -3 || s->type == -4) {
	$E("Timing directive ``%s'' spec error", s->type == -3 ? "->" : "#>");
      }
    }
    OPT_FREE ($8);

    if (intick && (s->type == -3 || s->type == -4)) {
      if (!s->ids[1]->isEqual (s->ids[2])) {
	$E("Timing directive *: LHS and RHS IDs must match!");
      }
      if ((s->extra[1] & 0x3) != (s->extra[2] & 0x3)) {
	$E("Timing directive *: LHS and RHS transitions must match!");
      }
    }

    return s;
}}
| ID "(" "*" ")"
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

    act_spec *s;
    NEW (s, act_spec);
    s->count = -1;
    s->type = i;
    s->ids = NULL;
    s->extra = NULL;
    s->next = NULL;
    return s;
}}
;

timing_type[int]: "<"
{{X:
    return -1;
}}
| "<<"
{{X:
    return -2;
}}
| "->"
{{X:
    return -3;
}}
| "#>"
{{X:
    return -4;
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
    $0->sizing_info->leak_adjust_specified = 0;
    A_INIT ($0->sizing_info->d);
    $0->sizing_info->next = NULL;
}}
[ size_body ]
"}"
{{X:
    act_sizing *sz;
    OPT_FREE ($3);
    sz = $0->sizing_info;
    $0->sizing_info = NULL;
    return new ActBody_Lang ($l, sz);
}}
;

size_directive: bool_expr_id "{" dir wnumber_expr ["," ID ] [ "," wpint_expr]  [";" dir wnumber_expr [ "," ID ] [ "," wpint_expr ] ] "}"
{{X:
    act_sizing_directive *d;

    if (!list_isempty ($0->sz_loop_stack)) {
      act_sizing_directive *parent;
      parent = (act_sizing_directive *) stack_peek ($0->sz_loop_stack);
      A_NEW (parent->d, act_sizing_directive);
      d = &A_NEXT (parent->d);
    }
    else {
      A_NEW ($0->sizing_info->d, act_sizing_directive);
      d = &A_NEXT ($0->sizing_info->d);
    }

    d->loop_id = NULL;
    
    d->id = $1;
    d->upfolds = NULL;
    d->dnfolds = NULL;
    d->flav_up = 0;
    d->flav_dn = 0;
    
    if ($3) {
      d->eup = $4;
      d->edn = NULL;
      
      if (!OPT_EMPTY ($5)) {
	ActRet *r = OPT_VALUE ($6);
	$A(r->type == R_STRING);
	d->flav_up = act_dev_string_to_value (r->u.str);
	if (d->flav_up == -1) {
	  $E("Unknown device flavor ``%s''", r->u.str);
	}
	FREE (r);
      }
      OPT_FREE ($5);
      
      if (!OPT_EMPTY ($6)) {
	ActRet *r = OPT_VALUE ($6);
	$A(r->type == R_EXPR);
	d->upfolds = r->u.exp;
	FREE (r);
      }
      OPT_FREE ($6);
    }
    else {
      d->edn = $4;
      d->eup = NULL;
      
      if (!OPT_EMPTY ($5)) {
	ActRet *r = OPT_VALUE ($5);
	$A(r->type == R_STRING);
	d->flav_dn = act_dev_string_to_value (r->u.str);
	if (d->flav_dn == -1) {
	  $E("Unknown device flavor ``%s''", r->u.str);
	}
	FREE (r);
      }
      OPT_FREE ($5);
      
      if (!OPT_EMPTY ($6)) {
	ActRet *r = OPT_VALUE ($6);
	$A(r->type == R_EXPR);
	d->dnfolds = r->u.exp;
	FREE (r);
      }
      OPT_FREE ($6);
    }
    if (!OPT_EMPTY ($7)) {
      ActRet *r1, *r2, *r3, *r4;
      Expr *fold;
      int flav;
      r1 = OPT_VALUE ($7);
      r2 = OPT_VALUE2 ($7);
      r3 = OPT_VALUE3 ($7);
      r4 = OPT_VALUE4 ($7);
      $A(r1->type == R_INT);
      $A(r2->type == R_EXPR);
      $A(r3->type == R_LIST);
      $A(r4->type == R_LIST);
      if (r1->u.ival == $3) {
	$E("Sizing directive with duplicate drive strength directions?");
      }

      flav = 0;
      if (!OPT_EMPTY (r3->u.l)) {
	ActRet *r;
	r = OPT_VALUE (r3->u.l);
	$A(r->type == R_STRING);
	flav = act_dev_string_to_value (r->u.str);
	if (flav == -1) {
	  $E("Unknown device flavor ``%s''", r->u.str);
	}
	FREE (r);
      }
      OPT_FREE (r3->u.l);
      
      if (!OPT_EMPTY (r4->u.l)) {
	ActRet *r5;
	r5 = OPT_VALUE (r4->u.l);
	$A(r5->type == R_EXPR);
	fold = r5->u.exp;
	FREE (r5);
      }
      else {
	fold = NULL;
      }
      OPT_FREE (r4->u.l);
      if (r1->u.ival) {
	d->eup = r2->u.exp;
	d->upfolds = fold;
	d->flav_up = flav;
      }
      else {
	d->edn = r2->u.exp;
	d->dnfolds = fold;
	d->flav_dn = flav;
      }
      FREE (r1);
      FREE (r2);
      FREE (r3);
      FREE (r4);
    }
    OPT_FREE ($7);

    if (!list_isempty ($0->sz_loop_stack)) {
      act_sizing_directive *parent;
      parent = (act_sizing_directive *) stack_peek ($0->sz_loop_stack);
      A_INC (parent->d);
    }
    else {
      A_INC ($0->sizing_info->d);
    }
    return NULL;
}}
| "(" ";" ID ":" !noreal wpint_expr [ ".." wpint_expr ]
{{X:
    act_sizing_directive *d;
    Expr *hi = NULL;
    
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());

    if (!list_isempty ($0->sz_loop_stack)) {
      act_sizing_directive *parent;
      parent = (act_sizing_directive *) stack_peek ($0->sz_loop_stack);
      A_NEW (parent->d, act_sizing_directive);
      d = &A_NEXT (parent->d);
    }
    else {
      A_NEW ($0->sizing_info->d, act_sizing_directive);
      d = &A_NEXT ($0->sizing_info->d);
    }
    
    d->loop_id = $3;
    
    if (!OPT_EMPTY ($6)) {
      ActRet *r;
      r = OPT_VALUE ($6);
      $A(r->type == R_EXPR);
      hi = r->u.exp;
      FREE (r);
    }
    OPT_FREE ($6);

    d->lo = $5;
    d->hi = hi;

    A_INIT (d->d);
    stack_push ($0->sz_loop_stack, d);
}}
 ":" { size_directive ";" }* ")"
{{X:
    $0->scope->Del ($3);
    stack_pop ($0->sz_loop_stack);

    if (!list_isempty ($0->sz_loop_stack)) {
      act_sizing_directive *parent;
      parent = (act_sizing_directive *) stack_peek ($0->sz_loop_stack);
      A_INC (parent->d);
    }
    else {
      A_INC ($0->sizing_info->d);
    }
    return NULL;
}}
;


size_setup: ID "<-" wnumber_expr
{{X:
    /* ID can be:
           p_n_mode  0/1  0 = default, 1 = sqrt sizing
	   unit_n 10
    */
    if (strcmp ($1, "p_n_mode") == 0) {
      $0->sizing_info->p_specified = 1;
      $0->sizing_info->p_n_mode_e = $3;
    }
    else if (strcmp ($1, "unit_n") == 0) {
      $0->sizing_info->unit_n_specified = 1;
      $0->sizing_info->unit_n_e = $3;
    }
    else if (strcmp ($1, "leak_adjust") == 0) {
      $0->sizing_info->leak_adjust_specified = 1;
      $0->sizing_info->leak_adjust_e = $3;
    }
    else {
      $E("Unknown sizing directive ``%s''", $1);
    }
    return NULL;
}}
| /* nothing */
;

size_body: { size_setup ";" }* { size_directive ";" }*
{{X:
    list_free ($1);
    list_free ($2);
    return NULL;
}}  
;

lang_refine[ActBody *]: "refine"
{{X:
    $0->ref_level++;
    // we save away the scope on the top of the scope refinement stack
    stack_push ($0->tmpscope, $0->scope);
}}
[ "<" INT ">" ]
[ "+{" ref_override_spec "}" ]
"{" base_item_list "}"
{{X:
    act_refine *r;
    ActBody *b;
    ActRet *rtype;
    int refidx;

    if (OPT_EMPTY ($2)) {
      refidx = 1;
    }
    else {
      rtype = OPT_VALUE ($2);
      $A(rtype->type == R_INT);
      refidx = rtype->u.ival;
      FREE (rtype);
    }
    OPT_FREE ($2);
    if (refidx < 1) {
      $W("refinement level is less than 1; assuming 1");
      refidx = 1;
    }

    NEW (r, act_refine);

    if (OPT_EMPTY ($3)) {
      r->overrides = NULL;
    }
    else {
      rtype = OPT_VALUE ($3);
      $A(rtype->type == R_OVERRIDES);
      r->overrides = rtype->u.ro;
      FREE (rtype);
    }
    OPT_FREE ($3);
    r->nsteps = refidx;
    r->b = $5;
    r->refsublist = NULL;
    for (ActBody *tb = $5; tb; tb = tb->Next()) {
      ActBody_Lang *l = dynamic_cast <ActBody_Lang *> (tb);
      if (l && l->gettype() == ActBody_Lang::LANG_REFINE) {
	act_refine *tmp = (act_refine *) l->getlang();
	UserDef::mkRefineList (&r->refsublist, tmp->nsteps);
      }
    }
    b = new ActBody_Lang ($l, r);
    $0->ref_level--;

    if ($0->scope != (Scope *) stack_peek ($0->tmpscope)) {
      delete $0->scope;
    }
    $0->scope = (Scope *) stack_pop ($0->tmpscope);

    return b;
}}
;


ref_override_spec[refine_override *]: one_ref_override_spec ref_override_spec
{{X:
    $1->next = $2;
    return $1;
}}
| one_ref_override_spec
{{X:
    return $1;
}}
;

one_ref_override_spec[refine_override *]: user_type [ "+" ] bare_id_list ";"
{{X:
    listitem_t *li;
    int append_params = 0;
    refine_override *ro;

    if ($0->ref_level > 1) {
      $E("Refinement overrides cannot be included in nested refinement blocks!");
    }
    if ($0->in_cond > 0) {
      $E("Refinement overrides cannot be within a conditional construct!");
    }

    if ($0->scope == (Scope *) stack_peek ($0->tmpscope)) {
      $0->scope = $0->scope->localClone ();
    }

    ro = new refine_override;

    if ($1->getDir() != Type::NONE) {
      $e("Override specification must not have direction flags\n");
      fprintf ($f, "\tOverride: ");
      $1->Print ($f);
      fprintf ($f, "\n");
      exit (1);
    }

    if (OPT_EMPTY ($2)) {
      append_params = 0;
      ro->plus = false;
    }
    else {
      append_params = 1;
      ro->plus = true;
    }
    OPT_FREE ($2);

    ro->it = $1;

    for (li = list_first ($3); li; li = list_next (li)) {
      const char *s = (char *)list_value (li);
      InstType *it = $0->scope->Lookup (s);
      if (!it) {
	$E("Override specified for ``%s'': not found in type", s);
      }

      InstType *chk = $1;
      $A(chk->arrayInfo() == NULL);
      if (chk->isEqual (it, 1)) {
	$e("Override is not necessary if type is not being refined!\n");
	fprintf ($f, "\tOverride: ");
	$1->Print ($f);
	fprintf ($f, "\n\tOriginal: ");
	it->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
      Array *tmpa;
      tmpa = it->arrayInfo();
      it->clrArray();

      int num_params = $1->getNumParams();
      int start_pos = 0;

      if (append_params) {
	if (TypeFactory::isUserType (it) && (it->getNumParams() > 0)) {
	  if (list_length ($3) != 1) {
	    $E("Append parameter syntax requires only one ID on the RHS (found %d)", list_length ($3));
	  }
	  $1->appendParams (it->getNumParams(), it->allParams());
	  num_params += it->getNumParams();
	}
      }

      while (chk) {
	//printf("CHECK [%d]: ", num_params);
	//chk->Print (stdout);
	//printf ("  v/s ");
	//it->Print (stdout);
	//printf ("\n");

	if (chk->isEqual (it, 1)) {
	  break;
	}

	UserDef *ux = dynamic_cast <UserDef *> (chk->BaseType());
	if (!ux) {
	  chk = NULL;
	}
	else {
	  chk = ux->getParent();
	}
	if (chk) {
	  UserDef *tmpu;
	  $A(chk->arrayInfo() == NULL);

	  tmpu = dynamic_cast<UserDef *> (chk->BaseType());

	  //printf ("num-params: %d\n", num_params);
	  //printf ("base params: %d\n", ux->getNumParams());
	  if (tmpu) {
	    //printf ("base parent params: %d\n", tmpu->getNumParams());
	    start_pos += (ux->getNumParams() - tmpu->getNumParams());
	    num_params -= (ux->getNumParams() - tmpu->getNumParams());
	  }
	  else {
	    start_pos += ux->getNumParams();
	    num_params-= ux->getNumParams();
	  }

	  //printf ("residual params: %d\n", num_params);
	  if (num_params > 0) {
	    chk = new InstType (chk);
	    inst_param *ap;
	    ap = $1->allParams ();
	    chk->appendParams (num_params, ap + start_pos);
	  }
	}
      }
      it->MkArray (tmpa);
      if (!chk) {
	$e("Illegal override; the new type doesn't implement the original.\n");
	fprintf ($f, "\tOriginal: ");
	it->Print ($f);
	fprintf ($f, "\n\tNew: ");
	$1->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
      /* insert expansion-time assertion that $1 <: it */
      int is_port = 0;
      if ($0->u_p) {
	is_port = $0->u_p->isPort (s);
      }
      else if ($0->u_d) {
	is_port = $0->u_d->isPort (s);
      }
      else if ($0->u_c) {
	is_port = $0->u_c->isPort (s);
      }
      else {
	$A(0);
      }
      if (is_port) {
	$E("Cannot override port type (port ``%s'') in refinement overrides.",
	   s);
      }
    }
    for (li = list_first ($3); li; li = list_next (li)) {
      $0->scope->refineBaseType ((char *)list_value (li), $1);
    }
    ro->ids = $3;
    return ro;
}}
;


lang_initialize[ActBody *]: "Initialize" "{" { action_items ";" }* "}"
{{X:
    act_initialize *init;
    NEW (init, act_initialize);
    init->actions = $3;
    init->next = NULL;
    return new ActBody_Lang ($l, init);
}}
;

action_items[act_chp_lang_t *]: ID "{" hse_body "}"
{{X:
    if (strcmp ($1, "actions") != 0) {
      $E("Actions within an initialize block have to be called `actions'");
    }
    return $3;
}}
;

lang_dataflow[ActBody *]: "dataflow" "{" [ dataflow_ordering ]
{{X:
    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    $0->allow_chan = true;
}}
{ dataflow_items ";" }* "}"
{{X:
    act_dataflow *dflow;
    NEW (dflow, act_dataflow);
    dflow->isexpanded = 0;
    dflow->dflow = $4;
    if (OPT_EMPTY ($3)) {
      dflow->order = NULL;
    }
    else {
      ActRet *r;
      r = OPT_VALUE ($3);
      $A(r->type == R_LIST);
      dflow->order = r->u.l;
      FREE (r);
    }
    OPT_FREE ($3);

    $0->allow_chan = false;

    return new ActBody_Lang ($l, dflow);
}}
;

dataflow_ordering[list_t *]: ID "{" { order_list ";" }* "}"
{{X:
    if (strcmp ($1, "order") == 0) {
      return $3;
    }
    else {
      $E("Only `order' directives currently supported in dataflow block");
    }
    return NULL;
}}
;

order_list[act_dataflow_order *]: { w_chan_id "," }* "<" { w_chan_id "," }*
{{X:
    act_dataflow_order *d;
    NEW (d, act_dataflow_order);
    d->lhs = $1;
    d->rhs = $3;
    return d;
}}    
;
    

dataflow_items[act_dataflow_element *]:
w_chan_int_expr "->" [ "[" wpint_expr [ "," wpint_expr ] "]" ] expr_id
{{X:
    act_dataflow_element *e;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    
    NEW (e, act_dataflow_element);
    e->t = ACT_DFLOW_FUNC;
    e->u.func.lhs = $1;
    e->u.func.istransparent = 0;

    if (_act_id_is_true_false ($4)) {
      $E("Dataflow RHS can't be true/false!");
    }
    if (_act_id_is_enum_const ($0->os, $0->curns, $4)) {
      $E("Can't use an enumeration constant in this context!");
    }
    InstType *it;
    if (act_type_var ($0->scope, $4, &it) != T_CHAN) {
      $e("Identifier on the RHS of a dataflow expression must be of channel type");
      fprintf ($f, "\n   ID: ");
      $4->Print ($f);
      fprintf ($f, "\n");
      exit (1);
    }

    if (it->getDir() == Type::IN) {
      InstType *it2 = $0->scope->FullLookup ($4->getName());
      if (!($4->Rest() && TypeFactory::isProcessType (it2))) {
	$e("Identifier on the RHS of a dataflow expression is an input?");
	fprintf ($f, "\n   ID: ");
	$4->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
    }
    
    e->u.func.rhs = $4;
    e->u.func.nbufs = NULL;
    e->u.func.init = NULL;
    if (!OPT_EMPTY ($3)) {
      ActRet *r1 = OPT_VALUE ($3);
      ActRet *r2 = OPT_VALUE2 ($3);

      $A(r1->type == R_EXPR);
      e->u.func.nbufs = r1->u.exp;
      FREE (r1);
      
      $A(r2->type == R_LIST);
      if (!OPT_EMPTY (r2->u.l)) {
	ActRet *r = OPT_VALUE (r2->u.l);
	$A(r->type == R_EXPR);
	e->u.func.init = r->u.exp;
	FREE (r);
      }
      OPT_FREE (r2->u.l);
      FREE (r2);
    }
    OPT_FREE ($3);
    return e;
}}
| w_chan_int_expr "->" [ "(" wpint_expr [ "," wpint_expr ] ")" ] expr_id
{{X:
    act_dataflow_element *e;
    e = apply_X_dataflow_items_opt0 ($0, $1, $3, $4);
    e->u.func.istransparent = 1;
    return e;
}}
| "{" expr_id_or_star_or_bar "}" { expr_id_or_star_or_rep "," }* "->" { expr_id_or_star_or_rep "," }*
{{X:
    act_dataflow_element *e;
    listitem_t *li;
    list_t *l;
    InstType *it;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    
    NEW (e, act_dataflow_element);

    e->u.splitmerge.guard = $2;
    e->u.splitmerge.nondetctrl = NULL;
    if ($2) {
      if (act_type_var ($0->scope, $2, &it) != T_CHAN) {
	$e("Identifier in the condition of a dataflow expression must be of channel type");
	fprintf ($f, "\n   ID: ");
	$2->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
      if (it->getDir() == Type::OUT) {
	InstType *it2 = $0->scope->FullLookup ($2->getName());
	if (!($2->Rest() && TypeFactory::isProcessType (it2))) {
	  $e("Identifier in the condition of a dataflow expression is an output?");
	  fprintf ($f, "\n   ID: ");
	  $2->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
      }
    }
    else {
      if ($0->non_det) {
	e->t = ACT_DFLOW_ARBITER;
      }
      else {
	e->t = ACT_DFLOW_MIXER;
      }
    }
    if (list_length ($4) == 1 && NO_LOOP (list_value (list_first ($4)))) {
      // this is a split
      if (!$2) {
	$E("A split requires a control channel specifier");
      }
      if (list_length ($6) < 2) {
	if (NO_LOOP (list_value (list_first ($6)))) {
	  $E("RHS needs more than one item for a split");
	}
      }
      l = $6;
      e->t = ACT_DFLOW_SPLIT;
      act_dataflow_loop *loop =
	(act_dataflow_loop *) list_value (list_first ($4));
      e->u.splitmerge.single = loop->chanid;
      delete loop;
      if (!e->u.splitmerge.single) {
	$E("Can't split from `*'");
      }
      list_free ($4);
    }
    else {
      // this could be a merge or an arbiter or a mixer
      listitem_t *li;
      for (li = list_first ($6); li; li = list_next (li)) {
	if (!NO_LOOP (list_value (li))) {
	  $E("Can't use a loop on the RHS of a arbiter/mixer/merge");
	}
      }
      if (list_length ($6) != 1) {
        if (!(list_length ($6) == 2 && (e->t == ACT_DFLOW_MIXER ||
                                        e->t == ACT_DFLOW_ARBITER))) {
          $E("RHS has too many channels for a merge");
        }   
      }
      l = $4;
      if ($2) {
        e->t = ACT_DFLOW_MERGE;
      }
      act_dataflow_loop *loop =
	(act_dataflow_loop *) list_value (list_first ($6));
      e->u.splitmerge.single = loop->chanid;
      delete loop;
      if (!e->u.splitmerge.single) {
	$E("Can't merge to `*'");
      }
      if (list_length ($6) == 2) {
	loop = (act_dataflow_loop *) list_value (list_next (list_first ($6)));
	e->u.splitmerge.nondetctrl = loop->chanid;
	delete loop;
      }
      list_free ($6);
    }
    if (act_type_var ($0->scope, e->u.splitmerge.single, &it) != T_CHAN) {
      $e("Identifier on the LHS/RHS of a dataflow expression must be of channel type");
      fprintf ($f, "\n   ID: ");
      e->u.splitmerge.single->Print ($f);
      fprintf ($f, "\n");
      exit (1);
    }
    if (e->t == ACT_DFLOW_SPLIT) {
      if (it->getDir() == Type::OUT) {
	InstType *it2 = $0->scope->FullLookup (e->u.splitmerge.single->getName());
	if (!(e->u.splitmerge.single->Rest() && TypeFactory::isProcessType (it2))) {
	  $e("Split input is of type output?");
	  fprintf ($f, "\n   ID: ");
	  e->u.splitmerge.single->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
      }
    }
    else {
      if (it->getDir() == Type::IN) {
	InstType *it2 = $0->scope->FullLookup (e->u.splitmerge.single->getName());
	if (!(e->u.splitmerge.single->Rest() && TypeFactory::isProcessType (it2))) {
	  $e("Merge output is of type input?");
	  fprintf ($f, "\n   ID: ");
	  e->u.splitmerge.single->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
      }
    }
    
    if (e->u.splitmerge.nondetctrl &&
	act_type_var ($0->scope, e->u.splitmerge.single, NULL) != T_CHAN) {
      $e("Identifier on the LHS/RHS of a dataflow expression must be of channel type");
      fprintf ($f, "   ");
      e->u.splitmerge.nondetctrl->Print ($f);
      fprintf ($f, "\n");
      exit (1);
    }
    e->u.splitmerge.nmulti = list_length (l);
    MALLOC (e->u.splitmerge.pre_exp, act_dataflow_loop *, e->u.splitmerge.nmulti);
    li = list_first (l);
    for (int i=0; i < e->u.splitmerge.nmulti; i++) {
      e->u.splitmerge.pre_exp[i] = (act_dataflow_loop *) list_value (li);
      if (e->u.splitmerge.pre_exp[i]->chanid) {
	if (act_type_var ($0->scope, e->u.splitmerge.pre_exp[i]->chanid, &it) != T_CHAN) {
	  $e("Identifier on the LHS/RHS of a dataflow expression must be of channel type");
	  fprintf ($f, "   ");
	  e->u.splitmerge.pre_exp[i]->chanid->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
	if (e->t == ACT_DFLOW_SPLIT) {
	  if (it->getDir() == Type::IN) {
	    InstType *it2 = $0->scope->FullLookup (e->u.splitmerge.pre_exp[i]->chanid->getName());
	    if (!(e->u.splitmerge.pre_exp[i]->chanid->Rest() && TypeFactory::isProcessType (it2))) {
	      $e("Split output is of type input?");
	      fprintf ($f, "\n   ID: ");
	      e->u.splitmerge.pre_exp[i]->chanid->Print ($f);
	      fprintf ($f, "\n");
	      exit (1);
	    }
	  }
	}
	else {
	  if (it->getDir() == Type::OUT) {
	    InstType *it2 = $0->scope->FullLookup (e->u.splitmerge.pre_exp[i]->chanid->getName());
	    if (!(e->u.splitmerge.pre_exp[i]->chanid->Rest() && TypeFactory::isProcessType (it2))) {
	      $e("Merge input is of type output?");
	      fprintf ($f, "\n   ID: ");
	      e->u.splitmerge.pre_exp[i]->chanid->Print ($f);
	      fprintf ($f, "\n");
	      exit (1);
	    }
	  }
	}
      }
      else {
	if (e->t != ACT_DFLOW_SPLIT) {
	  $E("Can't use ``*'' specifier for any type of merge");
	}
      }
      li = list_next (li);
    }
    list_free (l);
    return e;
}}
| "dataflow_cluster" "{"
{{X:
    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
}}
{ dataflow_items ";" }* "}"
{{X:
    act_dataflow_element *e;
    NEW (e, act_dataflow_element);
    e->t = ACT_DFLOW_CLUSTER;
    e->u.dflow_cluster = $3;
    return e;
}}
| expr_id "->" "*"
{{X:
    act_dataflow_element *e;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;

    NEW (e, act_dataflow_element);
    e->t = ACT_DFLOW_SINK;

    if (_act_id_is_true_false ($1)) {
      $E("Can't use true/false as a channel!");
    }

    InstType *it;
    if (act_type_var ($0->scope, $1, &it) != T_CHAN) {
      $e("Identifier on the LHS of a dataflow sink must be of channel type");
      fprintf ($f, "\n   ID: ");
      $1->Print ($f);
      fprintf ($f, "\n");
      exit (1);
    }
    if (it->getDir() == Type::OUT) {
      InstType *it2 = $0->scope->FullLookup ($1->getName());
      if (!($1->Rest() && TypeFactory::isProcessType (it2))) {
	$e("Identifier on the LHS of a dataflow sink is an output?");
	fprintf ($f, "\n   ID: ");
	$1->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
    }
    e->u.sink.chan = $1;
    return e;
}}
;

expr_id_or_star[ActId *]: expr_id
{{X:
    if (_act_id_is_true_false ($1)) {
      $E("Can't use true/false in this context!");
    }
    if (_act_id_is_enum_const ($0->os, $0->curns, $1)) {
      $E("Can't use an enumeration constant in this context!");
    }
    return $1;
}}
| "*"
{{X:
    return NULL;
}}
;

expr_id_or_star_or_rep[act_dataflow_loop *]: expr_id_or_star
{{X:
    act_dataflow_loop *ret;
    ret = new act_dataflow_loop();
    ret->chanid = $1;
    return ret;
}}
| "(" "," ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" expr_id_or_star ")"
{{X:
    act_dataflow_loop *ret;
    ret = new act_dataflow_loop();
    ret->id = $3;
    ret->lo = $5;
    if (!OPT_EMPTY ($6)) {
      ActRet *r;
      r = OPT_VALUE ($6);
      $A(r->type == R_EXPR);
      ret->hi = r->u.exp;
      FREE (r);
    }
    OPT_FREE($6);
    ret->chanid = $8;
    $0->scope->Del ($3);
    return ret;
}}
;

expr_id_or_star_or_bar[ActId *]: expr_id_or_star
{{X:
    $0->non_det = 0;
    return $1;
}}
| "|"
{{X:
    $0->non_det = 1;
    return NULL;
}}
;

lang_extern[ActBody *]: "extern" "(" ID !push ")" EXTERN[extern_lang]
{{X:
    return new ActBody_Lang ($l, $3, $5);
}}
;
