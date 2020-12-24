/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011-2019 Rajit Manohar
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

/*------------------------------------------------------------------------
 *
 *    
 *   Expression building blocks
 *
 *
 *------------------------------------------------------------------------
 */


/*------------------------------------------------------------------------
 *
 *  I. Array expressions
 *
 *------------------------------------------------------------------------
 */
array_expr[AExpr *]: { array_term "#" }*
{{X:
    AExpr *a, *ret;
    listitem_t *li;

    if (list_length ($1) == 1) {
      ret = (AExpr *) list_value (list_first ($1));
    }
    else {
      li = list_first ($1);
      ret = new AExpr (AExpr::CONCAT, (AExpr *)list_value (li), NULL);
      a = ret;
      for (li = list_next (li); li; li = list_next (li)) {
	a->SetRight (new AExpr (AExpr::CONCAT, (AExpr *)list_value (li), NULL));
	a = a->GetRight();
      }
    }
    list_free ($1);

    /*printf ("Returning %x\n", ret);*/

    return ret;
}}
;

array_term[AExpr *]: "{" { array_expr "," }* "}"
{{X:
    AExpr *a, *ret;
    listitem_t *li;

    li = list_first ($2);
    ret = new AExpr (AExpr::COMMA, (AExpr *)list_value (li), NULL);
    a = ret;
    for (li = list_next (li); li; li = list_next (li)) {
      a->SetRight (new AExpr (AExpr::COMMA, (AExpr *)list_value (li), NULL));
      a = a->GetRight ();
    }
    list_free ($2);
    return ret;
}}
| w_expr
{{X:
    AExpr *a;
    a = new AExpr ($1);
    /*printf ("Expr: %x\n", a);
    printf ("It is: ");
    a->Print (stdout);
    printf ("\n");*/
    return a;
}}
;

lhs_array_expr[AExpr *]: { lhs_array_term "#" }*
{{X:
    AExpr *a, *ret;
    listitem_t *li;

    if (list_length ($1) == 1) {
      ret = (AExpr *) list_value (list_first ($1));
    }
    else {
      li = list_first ($1);
      ret = new AExpr (AExpr::CONCAT, (AExpr *)list_value (li), NULL);
      a = ret;
      for (li = list_next (li); li; li = list_next (li)) {
	a->SetRight (new AExpr (AExpr::CONCAT, (AExpr *)list_value (li), NULL));
	a = a->GetRight();
      }
    }
    list_free ($1);

    /*printf ("Returning %x\n", ret);*/

    return ret;
}}
;

lhs_array_term[AExpr *]: "{" { lhs_array_expr "," }* "}"
{{X:
    AExpr *a, *ret;
    listitem_t *li;

    li = list_first ($2);
    ret = new AExpr (AExpr::COMMA, (AExpr *)list_value (li), NULL);
    a = ret;
    for (li = list_next (li); li; li = list_next (li)) {
      a->SetRight (new AExpr (AExpr::COMMA, (AExpr *)list_value (li), NULL));
      a = a->GetRight ();
    }
    list_free ($2);
    return ret;
}}
| expr_id
{{X:
    AExpr *a;
    Expr *e;
    int tc;

    NEW (e, Expr);
    e->type = E_VAR;
    e->u.e.l = (Expr *)$1;

    tc = act_type_expr ($0->scope, e, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
      
    a = new AExpr (e);
    /*printf ("Expr: %x\n", a);
    printf ("It is: ");
    a->Print (stdout);
    printf ("\n");*/
    return a;
}}
;


opt_array_expr[AExpr *]: [ array_expr ]
{{X:
    ActRet *r;
    AExpr *a;
    if (OPT_EMPTY ($1)) {
      OPT_FREE ($1);
      return NULL;
    }
    else {
      r = OPT_VALUE ($1);
      $A(r->type == R_AEXPR);
      a = r->u.ae;
      FREE (r);
      OPT_FREE ($1);
      return a;
    }
}}
;

/*------------------------------------------------------------------------
 *
 *  II. Identifiers. This could be either a scalar, or a subrange. The
 *  subrange specifier is always the last item in the identifier
 *  (i.e. no dots after it).
 *
 *------------------------------------------------------------------------
 */


expr_id[ActId *]: { base_id "." }*
/* At this point we know the identifier exists, the components exist
 and are accessible, and array accesses are to arrays. */
{{X:
    listitem_t *li;
    ActId *ret, *cur;
    Scope *s;
    InstType *it;
    UserDef *ud;

    /* check we are in _some_ scope! */
    $A($0->scope);

    ret = (ActId *) list_value (li = list_first ($1));
    cur = ret;

    /* check if we are "true", "false", or "self" -- special, we
       aren't going to be found in any scope! */
    if (list_length ($1) == 1 && ret->arrayInfo() == NULL) {
      const char *tmp;
      tmp = ret->getName ();
      if (strcmp (tmp, "true") == 0 || strcmp (tmp, "false") == 0 ||
	  strcmp (tmp, "self") == 0) {
	/* ok done */
	return ret;
      }
    }

    if ($0->skip_id_check && list_length ($1) == 1 && ret->arrayInfo() == NULL) {
      const char *tmp = ret->getName();
      if (act_dev_string_to_value (tmp) != -1) {
	$0->skip_id_check = 2;
	return ret;
      }
    }

    s = $0->scope;
    /* step 1: check that ret exists in the current scope */
    it = s->FullLookup (cur->getName());
    if (!it) {
      $E("The identifier ``%s'' does not exist in the current scope", cur->getName());
    }
    ud = NULL;
    for (li = list_next (li); li; li = list_next (li)) {
      /* it = the inst type of where we are so far; li has the next
	 item after the dot 
	 cur = the ActId that points to where we are right now.
      */
      ud = dynamic_cast<UserDef *>(it->BaseType());
      if (!ud) {
	$e("Invalid use of ``.'' for an identifer that is not a user-defined type: ");
	ret->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
      if (list_next (li) && cur->isRange ()) {
	/* a subrange specifier can occur, but it must be the *last*
	   part of the identifier (!) */
	$e("Invalid use of array sub-range specifier: ");
	ret->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
      if (cur->isDeref()) {
	/* if there is an array specifier, check that the dimensions
	   match */
	if (!it->arrayInfo() || (cur->arrayInfo()->nDims () != it->arrayInfo()->nDims ())) {
	  if (it->arrayInfo ()) {
	    $e("Mismatch in array dimensions (%d v/s %d): ",
	       cur->arrayInfo()->nDims(), it->arrayInfo()->nDims ());
	  }
	  else {
	    $e("Array reference (%d dims) for a non-arrayed identifier: ",
	       cur->arrayInfo()->nDims ());
	  }
	  ret->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
      }
      /* check that the id fragment exists in the scope of the inst
	 type */
      it = ud->Lookup ((ActId *)list_value (li));
      if (!it) {
	$e("Port name ``%s'' does not exist for the identifier: ", 
	   ((ActId *)list_value (li))->getName());
	ret->Print ($f);
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
    
    /* array check! */
    if (cur->isDeref()) {
      /* if there is an array specifier, check that the dimensions
	 match */
      if (!it->arrayInfo() || (cur->arrayInfo()->nDims () != it->arrayInfo()->nDims ())) {
	if (it->arrayInfo ()) {
	  $e("Mismatch in array dimensions (%d v/s %d): ",
	     cur->arrayInfo()->nDims(), it->arrayInfo()->nDims ());
	}
	else {
	  $e("Array reference (%d dims) for a non-arrayed identifier: ",
	     cur->arrayInfo()->nDims ());
	}
	ret->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
    }

    list_free ($1);
    return ret;
}}
;

base_id[ActId *]: ID [ xsparse_range ]
/* completely unchecked */
{{X:
    Array *a;
    ActRet *r;
    if (OPT_EMPTY ($2)) {
      a = NULL;
    }
    else {
      r = OPT_VALUE ($2);
      $A(r->type == R_ARRAY);
      a = r->u.array;
      FREE (r);
    }
    list_free ($2);
    return new ActId ($1, a);
}}
;

/*------------------------------------------------------------------------
 *
 *  III. Walked expressions.
 *
 *------------------------------------------------------------------------
 */
w_expr[Expr *]: expr
{{X:
    Expr *e;
    int tc;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    e = act_walk_X_expr ($0, $1);
    $A($0->scope);
    tc = act_type_expr ($0->scope, e, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
    return e;
}}
;

w_chan_int_expr[Expr *]: expr
{{X:
    Expr *e;
    int tc;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    e = act_walk_X_expr ($0, $1);
    $A($0->scope);
    tc = act_type_expr ($0->scope, e, NULL, 1);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
    return e;
}}
;


wnumber_expr[Expr *]: expr
{{X:
    Expr *e;
    int tc;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    e = act_walk_X_expr ($0, $1);
    $A($0->scope);
    tc = act_type_expr ($0->scope, e, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
    if (!(tc & (T_INT|T_REAL))) {
      $E("Expression must be of type int or real");
    }
    if (!(tc & T_PARAM)) {
      $E("Expression must be of type pint or preal");
    }
    return e;
}}
;

wnumber_flav_expr[Expr *]: expr
{{X:
    Expr *e;
    int tc;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    if ($1->type == E_VAR) {
      $0->skip_id_check = 1;
    }
    e = act_walk_X_expr ($0, $1);
    if ($0->skip_id_check == 2) {
      $0->skip_id_check = 0;
      return e;
    }
    $0->skip_id_check = 0;
    $A($0->scope);
    tc = act_type_expr ($0->scope, e, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
    if (!(tc & (T_INT|T_REAL))) {
      $E("Expression must be of type int or real");
    }
    if (!(tc & T_PARAM)) {
      $E("Expression must be of type pint or preal");
    }
    return e;
}}
;


wint_or_bool_expr[Expr *]: expr
{{X:
    Expr *e;
    int tc;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    e = act_walk_X_expr ($0, $1);
    $A($0->scope);
    tc = act_type_expr ($0->scope, e, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
    if (!(tc & (T_INT|T_BOOL))) {
      $E("Expression must be of type int or bool");
    }
    return e;
}}
;


/*
  CONSISTENCY: _wint_expr in prs.c
*/
wint_expr[Expr *]: int_expr
{{X:
    Expr *e;
    int tc;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    e = act_walk_X_expr ($0, $1);
    $A($0->scope);
    tc = act_type_expr ($0->scope, e, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t");
      print_expr ($f, e);
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
    if (!(tc & T_INT)) {
      $E("Expression must be of type int");
    }
    return e;
}}
;

wpint_expr[Expr *]: int_expr
{{X:
    Expr *e;
    int tc;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    e = act_walk_X_expr ($0, $1);
    $A($0->scope);
    tc = act_type_expr ($0->scope, e, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t");
      print_expr ($f, e);
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
    if (!(tc & T_INT)) {
      $E("Expression must be of type pint");
    }
    if (!(tc & T_PARAM)) {
      $E("Expression must be of type pint");
    }
    return e;
}}
;


wbool_expr[Expr *]: bool_expr
{{X:
    Expr *e;
    int tc;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    e = act_walk_X_expr ($0, $1);
    $A($0->scope);
    tc = act_type_expr ($0->scope, e, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
    if (!(tc & T_BOOL)) {
      $E("Expression must be of type bool");
    }
    return e;
}}
;

wbool_allow_chan_expr[Expr *]: bool_expr
{{X:
    Expr *e;
    int tc;

    $0->line = $l;
    $0->column = $c;
    $0->file = $n;
    e = act_walk_X_expr ($0, $1);
    $A($0->scope);
    tc = act_type_expr ($0->scope, e, NULL, 2);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    if ($0->strict_checking && ((tc & T_STRICT) == 0)) {
      $E("Expressions in port parameter list can only use strict template parameters");
    }
    if (!(tc & T_BOOL)) {
      $E("Expression must be of type bool");
    }
    return e;
}}
;

