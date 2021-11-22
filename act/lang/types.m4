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
| "pbool"
{{X:
    return $0->tf->NewPBool();
}}
| "preal"
{{X:
    return $0->tf->NewPReal();
}}
| "ptype" "(" iface_inst_type ")"
{{X:
    return $0->tf->NewPType($0->scope, $3);
}}
;

T_INT[int]: "int"
{{X: return 0; }}
| "ints"
{{X: return 1; }}
;

data_type[InstType *]: T_INT [ chan_dir ] [ "<" !endgt wpint_expr ">" !noendgt ]
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
      width = NULL; //const_expr (32);
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
| "enum" [ chan_dir ] "<" wpint_expr ">"
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

chan_type[InstType *]: "chan" [ chan_dir ] "(" physical_inst_type [ "," physical_inst_type ] ")" [ chan_dir ]
{{X:
    ActRet *r;
    Type::direction d;
    InstType *ret;
    
    if (OPT_EXISTS ($2)) {
      if (OPT_EXISTS ($7)) {
	$E("Direction flags can only be specified once");
      }
      r = OPT_VALUE ($2);
      $A(r->type == R_DIR);
      d = r->u.dir;
      FREE (r);
    }
    else {
      if (OPT_EXISTS ($7)) {
	r = OPT_VALUE ($7);
	$A(r->type == R_DIR);
	d = r->u.dir;
	FREE (r);
      }
      else {
	d = Type::NONE;
      }
    }
    OPT_FREE ($2);
    OPT_FREE ($7);

    InstType *t = $4;
    InstType *ack = NULL;
    if (OPT_EXISTS ($5)) {
      ActRet *r = OPT_VALUE ($5);
      $A(r->type == R_INST_TYPE);
      ack = r->u.inst;
      FREE (r);
    }
    OPT_FREE ($5);
    if ((!TypeFactory::isDataType (t) && !TypeFactory::isStructure (t)) ||
	(ack && !TypeFactory::isDataType (ack) && !TypeFactory::isStructure (t))) {
      $E("Channels can only send/receive data.");
    }
    if (!TypeFactory::isValidChannelDataType (t) ||
	(ack && !TypeFactory::isValidChannelDataType (ack))) {
      $e("User-defined channel data type must be a structure with pure data.\n");
      fprintf ($f, "\tType%s: ", ack ? "s" : "");
      t->Print ($f);
      if (ack) {
	fprintf ($f, ", ");
	ack->Print ($f);
      }
      fprintf ($f, "\n");
      exit (1);
    }
	
    ret = $0->tf->NewChan ($0->scope, d, t, ack);

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
{{X:
    if (TypeFactory::isInterfaceType ($1) && !$0->ptype_expand) {
      $E("An interface type cannot be used in this context");
    }
    $0->ptype_expand = 0;
    return $1;
}}
;

/*------------------------------------------------------------------------
 *
 *  Physical type: data or channel. Not a parameter.
 *
 *------------------------------------------------------------------------
 */
iface_inst_type[InstType *]: user_type
{{X:
    if (!TypeFactory::isInterfaceType ($1)) {
      $E("An interface type is expected in this context");
    }
    return $1;
}}
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
user_type[InstType *]: qualified_type  [ chan_dir ] [ template_args ]
{{X:
    InstType *ui;
    ActRet *r;
    listitem_t *li;
    list_t *l;
    UserDef *ud;

    if ($0->ptype_expand) {
      if (!OPT_EMPTY ($2) || !OPT_EMPTY ($3)) {
	$E("Parameter types are interfaces; no templates/direction flags allowed.");
      }
    }

    ud = $1;
    $A(ud);

    ui = new InstType ($0->scope, ud);

    if ($0->ptype_expand) {
      ui->setPTypeID ($0->ptype_name);

      /* set template parameters, if any */
      if ($0->ptype_templ_nargs > 0) {
	if ($0->ptype_templ_nargs > ud->getNumParams()) {
	  $E("Number of template parameters specified (%d) > available parameters (%d) for `%s'", $0->ptype_templ_nargs, ud->getNumParams(), ud->getName());
	}

	ui->setNumParams ($0->ptype_templ_nargs);
	type_set_position ($l, $c, $n);
	for (int i=0; i < $0->ptype_templ_nargs; i++) {
	  InstType *lhs, *rhs;
	  inst_param *ip;

	  ip = & $0->ptype_templ[i];
	  
	  if (ip->isatype) {
	    ui->setParam (i, ip->u.tt);
	    lhs = ud->getPortType (-(1+i));
	    if (type_connectivity_check (lhs, ip->u.tt) != 1) {
	      $E("Typechecking failed for template parameter #%d\n\t%s", i,
		 act_type_errmsg());
	    }
	    delete lhs;
	  }
	  else {
	    ui->setParam (i, ip->u.tp);
	    lhs = ud->getPortType (-(1+i));
	    rhs = ip->u.tp->getInstType ($0->scope, NULL);
	    if (type_connectivity_check (lhs, rhs) != 1) {
	      $E("Typechecking failed for template parameter #%d\n\t%s", i,
		 act_type_errmsg());
	    }
	    delete lhs;
	    delete rhs;
	  }
	}
      }
    }

    /* begin: set normal template parameters, if they exist */
    
    if (!OPT_EMPTY ($3)) {
      r = OPT_VALUE ($3);
      $A(r->type == R_LIST);
      l = r->u.l;
      FREE (r);
    }
    else {
      l = list_new ();
    }
    OPT_FREE ($3);
   
    if (ud->getNumParams() < list_length (l)) {
      $E("Number of template parameters specified (%d) > available parameters (%d) for `%s'", list_length (l), ud->getNumParams(), ud->getName());
    }
      
    /* 
       Now we have to examine the qualified_type to see if there are
       any derived parameters.

       For the *base* parameters for the type, we pass through the
       template arguments.

       For all the rest, we have to figure out which parameters are
       pre-specified.
    */
    InstType *instparent;
    UserDef *uparent;
    int param = 0;
    int *param_map;

    instparent = ud->getParent();
    if (instparent) {
      uparent = dynamic_cast<UserDef *>(instparent->BaseType());
      /* could be NULL, if parent type is not user-defined */
    }
    else {
      uparent = NULL;
    }

    if (list_length (l) > 0) {
      MALLOC (param_map, int, list_length (l));
      for (int i=0; i < list_length (l); i++) {
	param_map[i] = -1;
      }
    }
    else {
      param_map = NULL;
    }

    li = list_first (l);
    int li_count = 0;
    while (1) {
      if (param > ud->getNumParams()) {
	$E("Number of template parameters specified (%d) > defineable parameters for `%s'", list_length (l), ud->getName());
      }
      if (uparent && (param >= ud->getNumParams() || uparent->isPort (ud->getPortName (-(param+1))))) {
	/* walk through instparent and populate m */
	if (instparent->getNumParams() > 0) {
	  /* keep adding parameters from here into m */
	  /* increment param as you go */
	  param += instparent->getNumParams();
	}
	/* move to parent's parent */
	instparent = uparent->getParent();
	if (instparent) {
	  uparent = dynamic_cast<UserDef *>(instparent->BaseType());
	}
	else {
	  uparent = NULL;
	}
      }
      else {
	if (!li) {
	  break;
	}
	li = list_next (li);
	param_map[li_count++] = param;
	param++;
      }
    }

    if (list_length (l) > 0) {
      int i = 0;
      ui->setNumParams (list_length (l));

      type_set_position ($l, $c, $n);
      for (li = list_first (l); li; li = list_next (li)) {
	InstType *lhs, *rhs;
	inst_param *ip;

	ip = (inst_param *)list_value (li);
	if (ip->isatype) {
	  ui->setParam (i++, ip->u.tt);
	  lhs = ud->getPortType (-(1+param_map[i-1]));
	  if (type_connectivity_check (lhs, ip->u.tt) != 1) {
	    $E("Typechecking failed for template parameter #%d\n\t%s", (i-1),
	       act_type_errmsg());
	  }
	  delete lhs;
	}
	else {
	  ui->setParam (i++, ip->u.tp);
	  lhs = ud->getPortType (-(1+param_map[i-1]));
	  rhs = ip->u.tp->getInstType ($0->scope, NULL);
	  if (type_connectivity_check (lhs, rhs) != 1) {
	    $E("Typechecking failed for template parameter #%d\n\t%s", (i-1),
	       act_type_errmsg());
	  }
	  delete lhs;
	  delete rhs;
	}
	FREE (ip);
      }
    }
    list_free (l);
    if (param_map) {
      FREE (param_map);
    }
    /* end: set template params */

    /* begin: set direction flags for type */
    Type::direction d;
    if (!OPT_EMPTY ($2)) {
      r = OPT_VALUE ($2);
      $A(r->type == R_DIR);
      d = r->u.dir;
      FREE (r);
    }
    else {
      d = Type::NONE;
    }
    OPT_FREE ($2);
    ui->SetDir (d);
    /* end: set dir flags */
    return $0->tf->NewUserDef ($0->scope, ui);
}}
;

template_args[list_t *]: "<" !endgt { array_expr_or_type "," }* ">" !noendgt
{{X: return $2; }}
;

array_expr_or_type[inst_param *]: "@" user_type
{{X:
    inst_param *ip;
    NEW (ip, inst_param);
    ip->isatype = 1;
    ip->u.tt = $2;
    return ip;
}}  
| array_expr
{{X:
    inst_param *ip;
    NEW (ip, inst_param);
    ip->isatype = 0;
    ip->u.tp = $1;
    return ip;
}}
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
      g = $0->global;
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
	if (OPT_EMPTY ($1)) {
	  /* this might be a parameter */
	  InstType *it = $0->scope->Lookup ((char *)list_value (li));
	  if (it && TypeFactory::isPTypeType (it)) {
	    PType *x = dynamic_cast<PType *> (it->BaseType());
	    $A(x);
	    /* unexpanded ptype: it's the instance */
	    it = it->getTypeParam (0);
	    $A(it);
	    Interface *iface = dynamic_cast<Interface *>(it->BaseType ());
	    $A(iface);
	    $0->ptype_name = (char *)list_value (li);
	    $0->ptype_expand = 1;
	    $0->ptype_templ_nargs = it->getNumParams();
	    $0->ptype_templ = it->allParams();
	      
	    OPT_FREE ($1);
	    list_free ($2);
	    return iface;
	  }
	}
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
    if (TypeFactory::isFuncType (t)) {
      $e("Functions cannot be used to create instances: %s", gs);
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

dense_one_range[Array *]: "[" wpint_expr "]" 
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

sparse_one_range[Array *]: "[" !noreal wpint_expr [ ".." wpint_expr ] "]"
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

xsparse_range[Array *]: xsparse_one_range xsparse_range
{{X:
    $1->Concat ($2);
    delete $2;
    return $1;
}}
| xsparse_one_range
{{X:
    return $1;
}}
;

xsparse_one_range[Array *]: "[" !noreal wint_expr [ ".." wint_expr ] "]"
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
