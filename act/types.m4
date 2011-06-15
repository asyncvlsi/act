/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */


/**
 *************************************************************************
 *
 * Standard types in ACT
 *
 *************************************************************************
 */

/*------------------------------------------------------------------------
 *
 * A. Native types
 *
 *------------------------------------------------------------------------
 */
param_type[InstType *]: "pint"
{{X:
    return $0->tf->NewPInt();
}}
| "pints"
{{X:
    return $0->tf->NewPInts();
}}
| "pbool"
{{X:
    return $0->tf->NewPBool();
}}
| "preal"
{{X:
    return $0->tf->NewPReal();
}}
| "ptype" "<" physical_inst_type ">"
{{X:
    return $0->tf->NewPType($0->scope, $3);
}}
;

T_INT[int]: "int"
{{X: return 0; }}
| "ints"
{{X: return 1; }}
;

data_type[InstType *]: T_INT [ chan_dir ] [ "<" wint_expr ">" ]
{{X:
    ActRet *r;
    Type::direction d;
    Expr *width;

    if (OPT_EXISTS ($2)) {
      r = OPT_VALUE ($2);
      $A(r->type == R_DIR);
      d = r->u.dir;
      FREE (r);
    }
    else {
      d = Type::NONE;
    }
    OPT_FREE ($2);
    if (OPT_EXISTS ($3)) {
      r = OPT_VALUE ($3);
      $A(r->type == R_EXPR);
      width = r->u.exp;
      FREE (r);
    }
    else {
      width = const_expr (32);
    }
    OPT_FREE ($3);
    return $0->tf->NewInt ($0->scope, d, $1, width);
}}
| "bool" [ chan_dir ]
{{X:
    ActRet *r;
    Type::direction d;

    if (OPT_EXISTS ($2)) {
      r = OPT_VALUE ($2);
      $A(r->type == R_DIR);
      d = r->u.dir;
      FREE (r);
    }
    else {
      d = Type::NONE;
    }
    OPT_FREE ($2);
    return $0->tf->NewBool (d);
}}
| "enum" [ chan_dir ] "<" wint_expr ">"
{{X:
    ActRet *r;
    Type::direction d;

    if (OPT_EXISTS ($2)) {
      r = OPT_VALUE ($2);
      $A(r->type == R_DIR);
      d = r->u.dir;
      FREE (r);
    }
    else {
      d = Type::NONE;
    }
    OPT_FREE ($2);
    return $0->tf->NewEnum ($0->scope, d, $4);
}};

chan_type[InstType *]: "chan" [ chan_dir ] "(" { data_type "," }* ")"
{{X:
    ActRet *r;
    Type::direction d;
    listitem_t *li;
    int c = 0;
    InstType *ret;
    
    if (OPT_EXISTS ($2)) {
      r = OPT_VALUE ($2);
      $A(r->type == R_DIR);
      d = r->u.dir;
      FREE (r);
    }
    else {
      d = Type::NONE;
    }
    OPT_FREE ($2);
    for (li = list_first ($4); li; li = list_next (li)) {
      c++;
    }
    $A(c > 0);
    InstType **t;

    MALLOC (t, InstType *, c);

    c = 0;
    for (li = list_first ($4); li; li = list_next (li)) {
      t[c++] = (InstType *) list_value (li);
    }
    ret = $0->tf->NewChan ($0->scope, d, c, t);
    FREE (t);
    list_free ($4);
    return ret;
}}
;

chan_dir[Type::direction]: "?" 
{{X:
    return Type::IN;
}}
| "!" 
{{X:
    return Type::OUT;
}}
| "?!"
{{X:
    return Type::INOUT;
}}
| "!?"
{{X:
    return Type::OUTIN;
}}
;

/*------------------------------------------------------------------------
 *
 *  Physical type: data or channel. Not a parameter.
 *
 *------------------------------------------------------------------------
 */
physical_inst_type[InstType *]: data_type
{{X: return $1; }}
| chan_type 
{{X: return $1; }}
| user_type
{{X: return $1; }}
;


/*------------------------------------------------------------------------
 *
 *  Inst Type. This is any valid type (including templated types)
 *
 *------------------------------------------------------------------------
 */
inst_type[InstType *]: physical_inst_type
{{X: return $1; }}
| param_type 
{{X: return $1; }}
;


/*------------------------------------------------------------------------
 *
 *  User-defined type.
 *
 *------------------------------------------------------------------------
 */
user_type[InstType *]: qualified_type [ template_args ] [ chan_dir ]
{{X:
    InstType *ui;
    ActRet *r;
    listitem_t *li;
    list_t *l;
    UserDef *ud;

    ud = $1;
    $A(ud);

    ui = new InstType ($0->scope, ud);

    /* begin: set template parameters, if they exist */
    if (!OPT_EMPTY ($2)) {
      r = OPT_VALUE ($2);
      $A(r->type == R_LIST);
      l = r->u.l;
      FREE (r);
      if (ud->getNumParams() < list_length (l)) {
	$E("Number of template parameters specified (%d) > available parameters (%d) for `%s'", list_length (l), ud->getNumParams(), ud->getName());
      }
      
      int i = 0;

      ui->setNumParams (list_length (l));

      for (li = list_first (l); li; li = list_next (li)) {
	ui->setParam (i++, (AExpr *)list_value (li));
      }
      list_free (l);
    }
    OPT_FREE ($2);
    /* end: set template params */

    /* begin: set direction flags for type */
    Type::direction d;
    if (!OPT_EMPTY ($3)) {
      r = OPT_VALUE ($3);
      $A(r->type == R_DIR);
      d = r->u.dir;
      FREE (r);
    }
    else {
      d = Type::NONE;
    }
    OPT_FREE ($3);
    ui->SetDir (d);
    /* end: set dir flags */
    return $0->tf->NewUserDef ($0->scope, ui);
}}
;

template_args[list_t *]: "<" { array_expr "," }* ">"
{{X: return $2; }}
;


/* This is a qualified type name */
qualified_type[UserDef *]: [ "::" ] { ID "::" }*
{{X:
    ActNamespace *g, *ns;
    listitem_t *li;
    char *id;
    const char *gs;
    int export_perms = 1;
    UserDef *t;

    if (OPT_EMPTY ($1)) {
      g = $0->curns;
      gs = "";
    }
    else {
      g = ActNamespace::Global();
      gs = "::";
    }

    if (list_length ($2) > 1) {
      /* okay, we have to search through the namespaces */

      li = list_first ($2);
      id = (char *)list_value (li);
    
      /* find first component of namespace in search path */
      /* XXX: if there are multiple matches, we pick the first one. If
	 namespace foo can be found in multiple ways but only one of
	 them has bar as a sub-namespace, then:
	     foo:bar::baz
	 may fail even though there exists a way to match foo so that
	 it wouldn't fail.
      */
      ns = $0->os->find (g, id);
      if (!ns) {
	$e("Could not find specified type: %s", gs);
	print_ns_string ($f, $2);
	fprintf ($f, "\n");
	exit (1);
      }

      /* look for sub-namespaces */
      li = list_next (li);
      for (; list_next (li) != NULL; li = list_next (li)) {
	ns = ns->findNS ((char *)list_value (li));
	if (!ns) {
	  break;
	}
	export_perms = export_perms & (ns->isExported() ? 1 : 0);
	if (ns == $0->curns) {
	  export_perms = 1;
	}
      }
      if (!ns || !export_perms) {
	if (!ns) {
	  $e("Could not find specified type: %s", gs);
	}
	else {
	  $e("Type is not exported up the namespace hierarchy: %s", gs);
	}
	print_ns_string ($f, $2);
	fprintf ($f, "\n");
	exit (1);
      }
    }
    else {
      /* search through the open namespaces for this type */
      li = list_first ($2);

      ns = $0->os->findType (g, (char *)list_value (li));
      if (!ns) {
	$E("Could not find specified type: %s", (char *)list_value (li));
      }
      export_perms = 1;
    }
    /* 
       "li" is the list item corresponding to the type name
       "ns" is the namespace that should contain that type definition
    */
    id = (char *)list_value (li);
    t = ns->findType (id);
    if (!t || (ns != $0->curns && !t->IsExported())) {
      if (!t) {
	$e("Could not find specified type: %s", gs);
      }
      else {
	$e("Type is not exported up the namespace hierarchy: %s", gs);
      }
      print_ns_string ($f, $2);
      fprintf ($f, "\n");
      exit (1);
    }
    OPT_FREE ($1);
    list_free ($2);
    return t;
}}
;

/*
 * Array ranges
 */
dense_range[Array *]: dense_one_range dense_range
{{X:
    $1->Concat ($2);
    delete $2;
    return $1;
}}
| dense_one_range
{{X: return $1; }}
;

dense_one_range[Array *]: "[" wint_expr "]" 
{{X:
    Array *a = new Array ($2);
    return a;
}}
;

sparse_range[Array *]: sparse_one_range sparse_range
{{X:
    $1->Concat ($2);
    delete $2;
    return $1;
}}
| sparse_one_range
{{X:
    return $1;
}}
;

sparse_one_range[Array *]: "[" !noreal wint_expr [ ".." wint_expr ] "]"
{{X:
    Array *a;
    ActRet *r;
    
    if (OPT_EMPTY ($3)) {
      a = new Array ($2);
    }
    else {
      r = OPT_VALUE ($3);
      $A(r->type == R_EXPR);
      a = new Array ($2, r->u.exp);
      FREE (r);
    }
    OPT_FREE ($3);
    return a;
}}
;
