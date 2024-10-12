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
 *   Grammar for definitions of processes, data, channels.
 *
 *------------------------------------------------------------------------
 */

definition: defproc_or_cell
| defdata
| defchan
| defenum
| deffunc
| defiface
;

/*-- distinguish between a process and a cell --*/
def_or_proc[int]: "defproc" 
{{X:
    return 0;
}}
| "defcell"
{{X:
    return 1;
}}
;

/*
 * Templates
 */
template_spec: [ "export" ] "template"
{{X:
    $A($0->u == NULL);
    $0->u = new UserDef ($0->curns);
    $0->u->setFile ($n);
    $0->u->setLine ($l);
    if (!OPT_EMPTY ($1)) {
      $0->u->MkExported();
    }
    OPT_FREE ($1);
    $0->strict_checking = 1;
    $0->scope = $0->u->CurScope ();
}}
"<" { param_inst ";" }* ">"
{{X:
    list_free ($4);
    return NULL;
}}
| "export"
{{X:
    $A($0->u == NULL);
    $0->u = new UserDef ($0->curns);
    $0->u->setFile ($n);
    $0->u->setLine ($l);
    $0->u->MkExported();
    return NULL;
}}
;

param_inst: param_type param_id_list
{{X:
    listitem_t *li;
    ActRet *r;
    InstType *it;
    int has_default = 0;

    for (li = list_first ($2); li; li = list_next (li)) {

      r = (ActRet *)list_value (li);
      $A(r->type == R_STRING);
      const char *id_name = r->u.str;
      FREE (r);

      li = list_next (li);

      r = (ActRet *)list_value (li);
      $A(r->type == R_LIST);
      list_t *m = r->u.l;
      FREE (r);

      if (OPT_EMPTY (m)) {
	/* nothing needs to be done */
	it = $1;
      }
      else {
	it = new InstType ($1);
	r = OPT_VALUE (m);
	$A(r->type == R_ARRAY);
	if (TypeFactory::isPTypeType (it->BaseType())) {
	  $E("ptype parameters cannot be arrays!");
	}
	r->u.array->mkArray();
	it->MkArray (r->u.array);
	it->MkCached();
	FREE (r);
      }
      list_free (m);

      li = list_next (li);

      r = (ActRet *)list_value (li);
      $A(r->type == R_LIST);
      m = r->u.l;
      FREE (r);
      AExpr *ae;
      if (OPT_EMPTY (m)) {
	ae = NULL;
	if (has_default) {
	  $E("Default template parameters must be trailing parameters!");
	}
      }
      else {
	r = OPT_VALUE (m);
	$A(r->type == R_AEXPR);
	ae = r->u.ae;
	FREE (r);
	has_default = 1;
      }
      list_free (m);
      
      if ($0->u) {
	if ($0->u->AddMetaParam (it, id_name, ae) != 1) {
	  $E("Duplicate meta-parameter name in port list: ``%s''", id_name);
	}
      }
      else if ($0->u_f) {
	if ($0->u_f->AddMetaParam (it, id_name, ae) != 1) {
	  $E("Duplicate meta-parameter name in port list: ``%s''", id_name);
	}
      }
      else {
	$A(0);
      }
      if (ae) {
	ActId *tmp = new ActId (id_name);
	type_set_position ($l, $c, $n);
	if (!act_type_conn ($0->scope, tmp, ae)) {
	  $e("Typechecking failed for default parameter!");
	  fprintf ($f, "\n\t%s\n", act_type_errmsg ());
	  exit (1);
	}
	delete tmp;
      }
    }
    list_free ($2);
    return NULL;
}}
;

/*------------------------------------------------------------------------
 *  Identifier list with optional array
 *------------------------------------------------------------------------
 */
id_list[list_t *]: { ID [ dense_range ] "," }**
{{X: return $1; }}
;


param_id_list[list_t *]: { ID [ dense_range ] [ "=" array_expr ] "," }**
{{X: return $1; }}
;


/*------------------------------------------------------------------------
 *
 * Type signature for a process definition
 *
 *------------------------------------------------------------------------
 */
defproc_or_cell: [ template_spec ] 
{{X:
    if (OPT_EMPTY ($1)) {
      $0->u = new UserDef($0->curns);
      $0->u->setFile ($n);
      $0->u->setLine ($l);
    }
    else {
      $A($0->u);
    }
    OPT_FREE ($1);
    $0->strict_checking = 1;
}}
def_or_proc ID 
{{X:
    Process *p;
    UserDef *u;

    p = new Process ($0->u);
    delete $0->u;
    $0->u = NULL;
    
    if ($2) { 
      p->MkCell(); /* cell */
    }
    else {
      /* process */
    }

    switch ($0->curns->findName ($3)) {
    case 0:
      /* whew */
      break;
    case 1:
      $A(u = $0->curns->findType ($3));
      if (TypeFactory::isProcessType(u)) {
	/* there is hope */
      }
      else {
	$E("Name ``%s'' already used as a previous type definition", $3);
      }
      break;
    case 2:
      $E("Name ``%s'' already used as a namespace", $3);
      break;
    case 3:
      $E("Name ``%s'' already used as an instance", $3);
      break;
    default:
      $E("Should not be here ");
      break;
    }
    $0->u_p = p;
    /*printf ("Orig scope: %x\n", $0->scope);*/
    $0->scope = $0->u_p->CurScope ();
}}
[ "<:" physical_inst_type ]
{{X:
    if (!OPT_EMPTY ($4)) {
      ActRet *r;
      InstType *it;

      r = OPT_VALUE ($4);
      $A(r->type == R_INST_TYPE);
      it = r->u.inst;

      if (it->hasinstGlobal()) {
	$E("Type specifier in implementation relation cannot use globals");
      }
      
      FREE (r);

      if (!TypeFactory::isProcessType (it)) {
	$e("Parent type of a process must be a process type!");
	fprintf ($f, "\nParent type: ");
	it->Print ($f);
	fprintf ($f, "\n");
	exit (1);
      }
      if (it->getDir() != Type::NONE) {
	$E("Direction flags not permitted on parent type");
      }

      /* now merge parameters */
      Process *pp = dynamic_cast<Process *>(it->BaseType());
      $A(pp);

      for (int i=0; i < pp->getNumParams(); i++) {
	const char *s = pp->getPortName (-(i+1));
	InstType *st = pp->getPortType (-(i+1));
	if ($0->u_p->AddMetaParam (st, s, pp->getDefaultParam(i)) != 1) {
	  $e("Duplicate meta-parameter name in port list: ``%s''", s);
	  fprintf ($f, "\n\tConflict occurs due to parent type: ");
	  it->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
      }

      /* XXX: inheritence type v, parent it */
      $0->u_p->SetParent (it);

      /* now add in port parameters */
      for (int i=0; i < pp->getNumPorts(); i++) {
	const char *s = pp->getPortName (i);
	InstType *st = pp->getPortType (i);
	if ($0->u_p->AddPort (st, s) != 1) {
	  $e("Duplicate port name in port list: ``%s''", s);
	  fprintf ($f, "\n\tConflict occurs due to parent type: ");
	  it->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
      }
    }
    OPT_FREE ($4);
}}
 "(" [ port_formal_list ] ")"
{{X:
    /* Create type here */
    UserDef *u;

    if ((u = $0->curns->findType ($3))) {
      /* check if the type signature is identical */
      if (u->isEqual ($0->u_p)) {
	delete $0->u_p;
	$0->u_p = dynamic_cast<Process *>(u);
	$A($0->u_p);
      }
      else {
	$E("Name ``%s'' previously defined as a different process", $3);
      }
    }
    else {
      $A($0->curns->CreateType ($3, $0->u_p));
    }
    OPT_FREE ($6);
    $0->scope = $0->u_p->CurScope ();
    $0->strict_checking = 0;

    if ($0->u_p->getParent()) {
      UserDef *ux = dynamic_cast <UserDef *> ($0->u_p->getParent()->BaseType());
      /* re-play the body, and copy it in */
      if (ux->getBody()) {
	$0->scope->playBody (ux->getBody());
	$0->u_p->AppendBody (ux->getBody()->Clone());
      }
    }
}}
proc_body
{{X:
    $0->u_p = NULL;
    $0->scope = $0->curns->CurScope();
    return NULL;
}}
;

proc_body:
[ ":>" interface_spec ]
[ "+{" override_spec "}" ]
";"
{{X:
    OPT_FREE ($1);
    OPT_FREE ($2);
    return NULL;
}}
|  [ ":>" interface_spec ]
   [ "+{" override_spec "}" ]
{{X:
    if ($0->u_p->isDefined()) {
      $E("Process ``%s'': duplicate definition with the same type signature", $0->u_p->getName());
    }
}}
  "{" def_body  [ methods_body ] "}"
{{X:
    OPT_FREE ($1);
    OPT_FREE ($2);
    OPT_FREE ($5);
    $0->u_p->MkDefined ();
    return NULL;
}}
;

interface_spec: { interface_one_spec "," }*
{{X:
    OPT_FREE ($1);
    return NULL;
}}
;

interface_one_spec: iface_inst_type
"{" { idmap "," }*  "}"
{{X:
    listitem_t *li;
    list_t *ret;
    Interface *iface = dynamic_cast <Interface *>($1->BaseType());

    ret = NULL;
    for (li = list_first ($3); li; li = list_next (li)) {
      list_t *tlist = (list_t *) list_value (li);
      if (!ret) {
	ret = tlist;
      }
      else {
	list_concat (ret, tlist);
	list_free (tlist);
      }
    }

    $A(iface);
    $A($0->u_p);
    for (li = list_first (ret); li; li = list_next (li)) {
      if (!iface->isPort ((char *)list_value (li))) {
	$E("``%s'' is not a port in interface ``%s''",
	   (char *)list_value (li), iface->getName());
      }
      li = list_next (li);
      $A(li);
      if (!$0->u_p->isPort ((char *)list_value (li))) {
	$E("``%s'' is not a port in process ``%s''",
	   (char *)list_value (li), $0->u_p->getName());
      }
    }
    $0->u_p->addIface ($1, ret);
    return NULL;
}}
;

idmap[list_t *]: ID "->" ID
{{X:
    list_t *ret = list_new ();
    list_append (ret, $1);
    list_append (ret, $3);
    return ret;
}}
;    

override_spec: override_one_spec override_spec
{{X: return NULL; }}
| override_one_spec
{{X: return NULL; }}
;

override_one_spec: user_type [ "+" ] bare_id_list ";"
{{X:
    listitem_t *li;
    int append_params = 0;

    if ($1->getDir() != Type::NONE) {
      $e("Override specification must not have direction flags\n");
      fprintf ($f, "\tOverride: ");
      $1->Print ($f);
      fprintf ($f, "\n");
      exit (1);
    }

    if (OPT_EMPTY ($2)) {
      append_params = 0;
    }
    else {
      append_params = 1;
    }
    OPT_FREE ($2);

    ActBody *port_override_asserts = NULL;
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
	if (!port_override_asserts) {
	  port_override_asserts =
	    new ActBody_OverrideAssertion ($l, s, it, $1);
	}
	else {
	  port_override_asserts->Append
	    (new ActBody_OverrideAssertion ($l, s, it, $1));
	}
      }
    }
    /* Now actually perform the override! */

    /* 1. Walk through the body, replace the instances.
       2. Replace in scope table, port table if necessary.
    */
    ActBody *b;
    if ($0->u_p) {
      b = $0->u_p->getBody();
    }
    else if ($0->u_d) {
      b = $0->u_d->getBody();
    }
    else if ($0->u_c) {
      b = $0->u_c->getBody();
    }
    else {
      $A(0);
    }
    /* walk through the body, editing instances */
    if (b) {
      b->updateInstType ($3, $1);
      if (port_override_asserts) {
	b->Tail()->Append (port_override_asserts);
      }
    }
    else {
      if (port_override_asserts) {
	if ($0->u_p) {
	  $0->u_p->AppendBody (port_override_asserts);
	}
	else if ($0->u_d) {
	  $0->u_d->AppendBody (port_override_asserts);
	}
	else if ($0->u_c) {
	  $0->u_c->AppendBody (port_override_asserts);
	}
      }
    }

    /* walk through the ports, editing instances */
    UserDef *px;
    if ($0->u_p) {
      px = $0->u_p;
    }
    else if ($0->u_d) {
      px = $0->u_d;
    }
    else if ($0->u_c) {
      px = $0->u_c;
    }
    else {
      $A(0);
    }
    for (int i=0; i < px->getNumPorts(); i++) {
      for (li = list_first ($3); li; li = list_next (li)) {
	if (strcmp (px->getPortName (i), (char *) list_value (li)) == 0) {
	  break;
	}
      }
      if (li) {
	px->refinePortType (i, $1);
      }
    }
    for (li = list_first ($3); li; li = list_next (li)) {
      $0->scope->refineBaseType ((char *)list_value (li), $1);
    }
    list_free ($3);
    return NULL;
}}
;

port_formal_list: { single_port_item ";" }*
{{X:
    /* handled in single_port_item */
    list_free ($1);
    return NULL;
}}
;

macro_formal_list: { single_macro_port_item ";" }*
{{X:
    list_free ($1);
    return NULL;
}}
;

macro_param_formal_list: { macro_param_inst ";" }*
{{X:
    list_free ($1);
    return NULL;
}}
;

macro_param_inst: param_type id_list
{{X:
    listitem_t *li;
    ActRet *r;
    InstType *it;

    $A($0->um);

    for (li = list_first ($2); li; li = list_next (li)) {
      r = (ActRet *)list_value (li);
      $A(r->type == R_STRING);
      const char *id_name = r->u.str;
      FREE (r);

      li = list_next (li);

      r = (ActRet *)list_value (li);
      $A(r->type == R_LIST);
      list_t *m = r->u.l;
      FREE (r);

      if (OPT_EMPTY (m)) {
	/* nothing needs to be done */
	it = $1;
      }
      else {
	it = new InstType ($1);
	r = OPT_VALUE (m);
	$A(r->type == R_ARRAY);
	if (TypeFactory::isPTypeType (it->BaseType())) {
	  $E("ptype parameters cannot be arrays!");
	}
	r->u.array->mkArray();
	it->MkArray (r->u.array);
	it->MkCached();
	FREE (r);
      }
      list_free (m);

      if ($0->um->addPort (it, id_name) != 1) {
	$E("Duplicate port name in port list: ``%s''", id_name);
      }
      $0->scope->Add (id_name, it);
    }
    list_free ($2);
    return NULL;
}}
;


function_formal_list: port_formal_list | param_formal_list | /* empty */;

param_formal_list: { param_inst ";" }*
{{X:
    if ($0->func_template) {
      $E("Function cannot be templated and have template parameters");
    }
    list_free ($1);
    return NULL;
}}
;

func_ret_type[InstType *]: physical_inst_type
{{X:
    return $1;
}}
| param_type
{{X:
    return $1;
}}
;

single_port_item: physical_inst_type id_list
{{X:
    listitem_t *li;
    ActRet *r;
    InstType *it;
    UserDef *u;


    /* Make sure that port types are acceptable */
    if ($0->u_p || $0->u_i) {
      const char *msg = "process";
      /* We are currently processing a defproc port list or an
	 interface port list */
      if ($0->u_p) {
	u = $0->u_p;
      }
      else {
	msg = "interface";
	u = $0->u_i;
      }
      if (TypeFactory::isProcessType ($1->BaseType())) {
	r = (ActRet *) list_value (list_first ($2));
	$A(r->type == R_STRING);
	$E("Parameter ``%s'': port parameter for a %s cannot be a process", r->u.str, msg);
      }
      if ($1->getDir() == Type::INOUT ||
	  $1->getDir() == Type::OUTIN) {
	r = (ActRet *) list_value (list_first ($2));
	$A(r->type == R_STRING);
	$E("Parameter ``%s'': !? and ?! direction flags only used by data and channel types", r->u.str);
      }
    }
    else if ($0->u_d) {
      /* This is a user-defined data type */
      u = $0->u_d;
      const char *err = NULL;

      if (TypeFactory::isProcessType ($1->BaseType())) {
	err = "process";
      }
      else if (TypeFactory::isChanType ($1->BaseType()) &&
	       !TypeFactory::isStructure (u)) {
	err = "channel";
      }
      if (err) {
	r = (ActRet *) list_value (list_first ($2));
	$A(r->type == R_STRING);
	$E("Parameter ``%s'': port parameter for a data-type cannot be a %s",
	   r->u.str, err);
      }
    }
    else if ($0->u_f) {
      /* This is a user-defined function */
      u = $0->u_f;
      const char *err = NULL;

      if (TypeFactory::isProcessType ($1->BaseType())) {
	err = "process";
      }
      else if (TypeFactory::isChanType ($1->BaseType())) {
	err = "channel";
      }
      else if (TypeFactory::isStructure ($1->BaseType()) &&
	       !TypeFactory::isPureStruct ($1->BaseType())) {
	err = "structure with channels";
      }
      if (err) {
	r = (ActRet *) list_value (list_first ($2));
	$A(r->type == R_STRING);
	$E("Parameter ``%s'': port parameter for a function cannot be a %s",
	   r->u.str, err);
      }
    }
    else if ($0->u_c) {
      /* This is a user-defined channel type */
      u = $0->u_c;
      if (TypeFactory::isProcessType ($1->BaseType())) {
	r = (ActRet *) list_value (list_first ($2));
	$A(r->type == R_STRING);
	$E("Parameter ``%s'': port parameter for a channel cannot be a process", r->u.str);
      }
    }
    else {
      /* should not be here */
      $A(0);
    }

    /* Walk through identifiers */
    for (li = list_first ($2); li; li = list_next (li)) {
      r = (ActRet *) list_value (li);
      $A(r->type == R_STRING);
      const char *id_name = r->u.str;
      FREE (r);

      li = list_next (li);

      r = (ActRet *) list_value (li);
      $A(r->type == R_LIST);
      list_t *m = r->u.l;
      FREE (r);

      if (OPT_EMPTY (m)) {
	/* nothing---use the base insttype directly */
	it = $1;
      }
      else {
	/* we need to replicate the insttype */
	it = new InstType ($1);
	r = OPT_VALUE (m);
	$A(r->type == R_ARRAY);
	r->u.array->mkArray ();
	it->MkArray (r->u.array);
	it->MkCached ();
	FREE (r);
      }
      list_free (m);

      if (u->AddPort (it, id_name) != 1) {
	if ($0->u_f) {
	  $E("Duplicate parameter name in argument list: ``%s''", id_name);
	}
	else {
	  $E("Duplicate parameter name in port list: ``%s''", id_name);
	}
      }
    }
    list_free ($2);
    return NULL;
}}
;


single_macro_port_item: physical_inst_type id_list
{{X:
    listitem_t *li;
    ActRet *r;
    InstType *it;
    UserDef *u;

    if (!TypeFactory::isDataType ($1) && !TypeFactory::isStructure($1)) {
      r = (ActRet *) list_value (list_first ($2));
      $A(r->type == R_STRING);
      $E("Parameter ``%s'': port parameter for a macro must be a data type", r->u.str);
    }
    
    if ($0->u_p) {
      u = $0->u_p;
    }
    else if ($0->u_c) {
      u = $0->u_c;
    }
    else if ($0->u_d) {
      u = $0->u_d;
    }
    else {
      $A(0);
    }

    /* Walk through identifiers */
    for (li = list_first ($2); li; li = list_next (li)) {
      r = (ActRet *) list_value (li);
      $A(r->type == R_STRING);
      const char *id_name = r->u.str;
      FREE (r);

      li = list_next (li);

      r = (ActRet *) list_value (li);
      $A(r->type == R_LIST);
      list_t *m = r->u.l;
      FREE (r);

      if (OPT_EMPTY (m)) {
	/* nothing---use the base insttype directly */
	it = $1;
      }
      else {
	/* we need to replicate the insttype */
	it = new InstType ($1);
	r = OPT_VALUE (m);
	$A(r->type == R_ARRAY);
	r->u.array->mkArray ();
	it->MkArray (r->u.array);
	it->MkCached ();
	FREE (r);
      }
      list_free (m);

      $0->scope->Add (id_name, it);

      if (u->FindPort (id_name) != 0) {
	$E("Macro argument conflicts with port/parameter name: ``%s''", id_name);
      }
      if (!$0->um->addPort (it, id_name)) {
	$E("Duplicate macro port name: ``%s''", id_name);
      }
    }
    list_free ($2);
    return NULL;
}}
;


/*------------------------------------------------------------------------
 *
 *  Data type definition (deftype)
 *
 *------------------------------------------------------------------------
 */
defdata: [ template_spec ]
{{X:
    if (OPT_EMPTY ($1)) {
      $0->u = new UserDef ($0->curns);
      $0->u->setFile ($n);
      $0->u->setLine ($l);
    }
    else {
      $A($0->u);
    }
    OPT_FREE ($1);
    $0->strict_checking = 1;
}}
"deftype" ID 
{{X:
    Data *d;
    UserDef *u;

    d = new Data ($0->u);
    delete $0->u;
    $0->u = NULL;

    switch ($0->curns->findName ($3)) {
    case 0:
      break;
    case 1:
      $A(u = $0->curns->findType ($3));
      if (TypeFactory::isDataType (u)) {
	Data *td = dynamic_cast<Data *>(u);
	$A(td);
	if (td->isEnum()) {
	  $E("Name ``%s'' already used in as an enumeration data type", $3);
	}
	/* now there is hope */
      }
      else {
	$E("Name ``%s'' already used in a previous type definition", $3);
      }
      break;
    case 2:
      $E("Name ``%s'' already used as a namespace", $3);
      break;
    case 3:
      $E("Name ``%s'' already used as an instance", $3);
      break;
    default:
      $E("Should not be here ");
      break;
    }
    $0->u_d = d;
    $0->scope = d->CurScope ();
}}
[ "<:" physical_inst_type ]
{{X:
    InstType *ir;
    InstType *phys_inst;
    /* parent type cannot be
       - process
       - channel
       
       Note that meta-params are ruled out by parser rules 
    */
    if (OPT_EMPTY ($4)) {
      phys_inst = NULL;
    }
    else {
      ActRet *r = OPT_VALUE ($4);
      $A(r->type == R_INST_TYPE);
      phys_inst = r->u.inst;
      FREE (r);
    }
    OPT_FREE ($4);

    if (phys_inst) {
      if (phys_inst->hasinstGlobal()) {
	$E("Type specifier in implementation relation cannot use globals");
      }
      if (TypeFactory::isProcessType (phys_inst->BaseType())) {
	$E("A data type cannot be related to a process");
      }
      if (TypeFactory::isChanType (phys_inst->BaseType())) {
	$E("A data type cannot be related to a channel");
      }
      if (TypeFactory::isInterfaceType (phys_inst->BaseType())) {
	$E("A data type cannot be related to an interface");
      }
      ir = phys_inst;

      if (phys_inst->getDir() != Type::NONE) {
	$E("Direction flags not permitted on parent type");
      }
    
      if (TypeFactory::isUserType (phys_inst->BaseType ())) {

	Data *dp = dynamic_cast<Data *>(phys_inst->BaseType());
	$A(dp);

	for (int i=0; i < dp->getNumParams(); i++) {
	  const char *s = dp->getPortName (-(i+1));
	  InstType *st = dp->getPortType (-(i+1));
	  if ($0->u_d->AddMetaParam (st, s, dp->getDefaultParam(i)) != 1) {
	    $e("Duplicate meta-parameter name in port list: ``%s''", s);
	    fprintf ($f, "\n\tConflict occurs due to parent type: ");
	    ir->Print ($f);
	    fprintf ($f, "\n");
	    exit (1);
	  }
	}

	/* now add in port parameters */
	for (int i=0; i < dp->getNumPorts(); i++) {
	  const char *s = dp->getPortName (i);
	  InstType *st = dp->getPortType (i);
	  if ($0->u_d->AddPort (st, s) != 1) {
	    $e("Duplicate port name in port list: ``%s''", s);
	    fprintf ($f, "\n\tConflict occurs due to parent type: ");
	    phys_inst->Print ($f);
	    fprintf ($f, "\n");
	    exit (1);
	  }
	}

	if (dp->getBody()) {
	  $0->scope->playBody (dp->getBody());
	  $0->u_d->AppendBody (dp->getBody()->Clone());
	}
	$0->u_d->copyMethods (dp);
      }
    }
    else {
      ir = NULL;
    }

    if (ir) {
      $0->scope->Add ("self", ir);
    }
    $0->u_d->SetParent (phys_inst);
}}
"(" [ port_formal_list ] ")"
{{X:
    UserDef *u;

    if ((u = $0->curns->findType ($3))) {
      if (u->isEqual ($0->u_d)) {
	delete $0->u_d;
	$0->u_d = dynamic_cast<Data *>(u);
	$A($0->u_d);
      }
      else {
	$E("Name ``%s'' previously defined as a different data type", $3);
      }
    }
    else {
      $A($0->curns->CreateType ($3, $0->u_d));
    }
    OPT_FREE ($6);
    $0->strict_checking = 0;
}}
data_chan_body
{{X:
    /* body of data type: this gets updated in the actions for
       data_body */
    $0->u_d = NULL;
    $0->scope = $0->curns->CurScope ();
    return NULL;
}}
;

data_chan_body: ";"
{{X:
    return NULL;
}}
| [ "+{" override_spec "}" ]
{{X:
    if ($0->u_c) {
      if ($0->u_c->isDefined ()) {
	$E("Channel definition ``%s'': duplicate definition with the same type signature", $0->u_c->getName ());
      }
    }
    else if ($0->u_d) {
      if ($0->u_d->isDefined ()) {
	$E("Data definition ``%s'': duplicate definition with the same type signature", $0->u_d->getName ());
      }
    }
    else {
      $A(0);
    }
    OPT_FREE ($1);
}}
"{" base_body [ methods_body ] "}"
{{X:
    if ($0->u_c) {
      $0->u_c->MkDefined ();
      $0->u_c->AppendBody ($3);
    }
    else if ($0->u_d) {
      $0->u_d->MkDefined();
      $0->u_d->AppendBody ($3);
    }
    else {
      $A(0);
    }
    OPT_FREE ($4);
    return NULL;
}}
;

methods_body: "methods" "{" [ method_list ] "}"
{{X:
    OPT_FREE ($3);
    return NULL;
}}
;

method_list: one_method method_list 
| one_method
;

one_method: ID "{" hse_body "}"
{{X:
    if ($0->u_d) {
      /* data methods */
      if (strcmp ($1, "set") == 0) {
	if ($0->u_d->getMethod(ACT_METHOD_SET)) {
	  $E("Duplicate ``set'' method");
	}
	$0->u_d->setMethod (ACT_METHOD_SET, $3);
      }
      else if (strcmp ($1, "get") == 0) {
	if ($0->u_d->getMethod(ACT_METHOD_GET)) {
	  $E("Duplicate ``get'' method");
	}
	$0->u_d->setMethod (ACT_METHOD_GET, $3);
      }
      else {
	$E("Method ``%s'' is not supported", $1);
      }
    }
    else if ($0->u_c) {
      int found = 0;
      /* channel methods */
      for (int i=0; i < ACT_NUM_STD_METHODS; i++) {
	if (strcmp ($1, act_builtin_method_name[i]) == 0) {
	  if ($0->u_c->getMethod(i)) {
	    $E("Duplicate ``%s'' method", act_builtin_method_name[i]);
	  }
	  $0->u_c->setMethod (i, $3);
	  found = 1;
	  break;
	}
      }
      if (!found) {
	for (int i=0; i < ACT_NUM_EXPR_METHODS; i++) {
	  if (strcmp ($1, act_builtin_method_expr[i]) == 0) {
	    $E("method ``%s'' uses method expressions", act_builtin_method_expr[i]);
	  }
	}
	$E("Method ``%s'' is not supported", $1);
      }
    }
    else {
      $E("This particlar style of method body is only permitted in channels or data types");
    }
    return NULL;
}}
| ID "=" wint_or_bool_expr ";"
{{X:
    if ($0->u_c) {
      int found = 0;
      for (int i=0; i < ACT_NUM_EXPR_METHODS; i++) {
	if (strcmp ($1, act_builtin_method_expr[i]) == 0) {
	  int tc = act_type_expr ($0->scope, $3, NULL);
	  if (tc & T_INT) {
	    if (act_builtin_method_boolret[i]) {
	      $E("``%s'' method must be of type bool", act_builtin_method_expr[i]);
	    }
	  }
	  if ($0->u_c->geteMethod (i + ACT_NUM_STD_METHODS)) {
	    $E("Duplicate ``%s'' method", act_builtin_method_expr[i]);
	  }
	  else {
	    $0->u_c->setMethod (i + ACT_NUM_STD_METHODS, $3);
	  }
	  found = 1;
	}
      }
      if (!found) {
 	$E("Method-expression ``%s'' is not supported", $1);
      }
    }
    else {
      $E("This particlar style of method body is only permitted in channel types");
    }
    return NULL;
}}
| "macro" ID
{{X:
    if ($0->u_p) {
      $0->um = $0->u_p->newMacro ($2);
    }
    else if ($0->u_c) {
      $0->um = $0->u_c->newMacro ($2);
    }
    else if ($0->u_d) {
      if (TypeFactory::isPureStruct ($0->u_d)) {
	if (strcmp ($2, "int") == 0) {
	  $E("``int'' cannot be used as a macro name; it is built-in!");
	}
	else if (strcmp ($2, $0->u_d->getName()) == 0) {
	  $E("``%s'' cannot be used as a macro name; it is built-in!",
	     $0->u_d->getName());
	}
      }
      $0->um = $0->u_d->newMacro ($2);
    }
    else {
      $E("Macro in invalid unknown context");
    }
    if (!$0->um) {
      $E("Duplicate macro name: ``%s''", $2);
    }
    $0->scope = new Scope ($0->scope, 0);
}}  
"(" [ macro_formal_list ] ")" "{" [ chp_body ] "}"
{{X:
    OPT_FREE ($4);
    /* function formal list must be data types; no parameters allowed */
    if (!OPT_EMPTY ($7)) {
      ActRet *r = OPT_VALUE ($7);
      $A(r->type == R_CHP_LANG);
      $0->um->setBody (r->u.chp);
      FREE (r);
    }
    OPT_FREE ($7);
    $0->um = NULL;

    Scope *tmp = $0->scope;
    $0->scope = tmp->Parent ();
    delete tmp;
    
    return NULL;
}}
| "function" ID
{{X:
    if ($0->u_p) {
      $E("Function methods cannot be used by processes!");
    }
    else if ($0->u_c) {
      $E("Function methods cannot be used by channels!");
    }
    else if ($0->u_d) {
      if (TypeFactory::isPureStruct ($0->u_d)) {
	if (strcmp ($2, "int") == 0) {
	  $E("``int'' cannot be used as a macro name; it is built-in!");
	}
	else if (strcmp ($2, $0->u_d->getName()) == 0) {
	  $E("``%s'' cannot be used as a macro name; it is built-in!",
	     $0->u_d->getName());
	}
      }
      $0->um = $0->u_d->newMacro ($2);
    }
    else {
      $E("Function method in invalid unknown context");
    }
    if (!$0->um) {
      $E("Duplicate method name: ``%s''", $2);
    }
    $0->scope = new Scope ($0->scope, 0);
}}  
"(" macro_fn_formal ")" ":" func_ret_type
{{X:
    if ($0->um->getNumPorts() > 0) {
      if (TypeFactory::isParamType ($0->um->getPortType (0))) {
	if (!TypeFactory::isParamType ($7)) {
	  $E("Macro function with parameter types: return type must be a parameter!");
	}
      }
      else {
	if (TypeFactory::isParamType ($7)) {
	  $E("Macro function with non-parameter types: return type cannot be a parameter!");
	}
      }
    }
    if (TypeFactory::isParamType ($7)) {
      $A($0->u_d);
      for (int i=0; i < $0->u_d->getNumParams (); i++) {
	if ($0->um->addPort ($0->u_d->getPortType (-(i+1)),
			     $0->u_d->getPortName (-(i+1))) != 1) {
	  $E("Macro function `%s': duplicate port name for parameter type `%s'.",
	     $0->um->getName(), $0->u_d->getPortName (-(i+1)));
	}
      }
    }
    $0->um->setRetType ($7);
    $0->scope->Add ("self", $7);
}}
"{" [ alias_or_inst_list "chp" "{" chp_body "}" ] "}"
{{X:
    /* function formal list must be data types; no parameters allowed */
    if (!OPT_EMPTY ($9)) {
      ActRet *r = OPT_VALUE2 ($9);
      $A(r->type == R_CHP_LANG);
      $0->um->setBody (r->u.chp);
      FREE (r);
      r = OPT_VALUE ($9);
      $A(r->type == R_ACT_BODY);
      $0->um->setActBody (r->u.body);
      FREE (r);
    }
    OPT_FREE ($9);
    $0->um = NULL;

    Scope *tmp = $0->scope;
    $0->scope = tmp->Parent ();
    delete tmp;
    
    return NULL;
}}
;

macro_fn_formal: macro_formal_list | macro_param_formal_list | /* empty */
;

/*
  For both channel and data types you can have spec bodies and aliases
*/
base_body[ActBody *]: base_body_item  base_body
{{X:
    if ($1) {
      $1->Append ($2);
      return $1;
    }
    else {
      return $2;
    }
}}
| /* empty */
;

base_body_item[ActBody *]: lang_spec
{{X:
    return $1;
}}
| alias
{{X:
    return $1;
}}
| loop_base
{{X:
    return $1;
}}
| conditional_base
{{X:
    return $1;
}}
| assertion
{{X:
    return $1;
}}
| debug_output
{{X:
    return $1;
}}
;

/*------------------------------------------------------------------------
 *
 *  User-defined channel definition. "defchan"
 *
 *------------------------------------------------------------------------
 */
defchan: [ template_spec ]
{{X:
    if (OPT_EMPTY ($1)) {
      $0->u = new UserDef($0->curns);
      $0->u->setFile ($n);
      $0->u->setLine ($l);
    }
    else {
      $A($0->u);
    }
    OPT_FREE ($1);
    $0->strict_checking = 1;
}}
"defchan" ID 
{{X:
    Channel *c;
    UserDef *u;

    c = new Channel ($0->u);
    delete $0->u;
    $0->u = NULL;

    switch ($0->curns->findName ($3)) {
    case 0:
      /* good */
      break;
    case 1:
      $A(u = $0->curns->findType ($3));
      if (TypeFactory::isChanType (u)) {
	/* there is hope */
      }
      else {
	$E("Name ``%s'' already used in a previous type definition", $3);
      }
      break;
    case 2:
      $E("Name ``%s'' already used as a namespace", $3);
      break;
    case 3:
      $E("Name ``%s'' already used as an instance", $3);
      break;
    default:
      $E("Should not be here ");
      break;
    }
    $0->u_c = c;
    $0->scope = $0->u_c->CurScope ();
}}
"<:" physical_inst_type
{{X:
    InstType *ir, *ir2;

    if ($5->hasinstGlobal()) {
      $E("Type specifier in implementation relation cannot use globals");
    }
    
    if (TypeFactory::isProcessType ($5->BaseType())) {
      $E("A channel type cannot be related to a process");
    }
    if (TypeFactory::isDataType ($5->BaseType())) {
      $E("A channel type cannot be related to a data type");
    }
    if (TypeFactory::isInterfaceType ($5->BaseType())) {
      $E("A channel type cannot be related to an interface");
    }
    
    ir = $5;
    ir2 = NULL;

    if (ir->getDir() != Type::NONE) {
      $E("Direction flags not permitted on parent type");
    }
    
    if (TypeFactory::isUserType ($5->BaseType ())) {
      Channel *ch = dynamic_cast<Channel *>($5->BaseType());
      $A(ch);

      for (int i=0; i < ch->getNumParams(); i++) {
	const char *s = ch->getPortName (-(i+1));
	InstType *st = ch->getPortType (-(i+1));
	if ($0->u_c->AddMetaParam (st, s, ch->getDefaultParam(i)) != 1) {
	  $e("Duplicate meta-parameter name in port list: ``%s''", s);
	  fprintf ($f, "\n\tConflict occurs due to parent type: ");
	  ir->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
      }

      /* now add in port parameters */
      for (int i=0; i < ch->getNumPorts(); i++) {
	const char *s = ch->getPortName (i);
	InstType *st = ch->getPortType (i);
	if ($0->u_c->AddPort (st, s) != 1) {
	  $e("Duplicate port name in port list: ``%s''", s);
	  fprintf ($f, "\n\tConflict occurs due to parent type: ");
	  $5->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
      }

      if (ch->getBody()) {
	$0->scope->playBody (ch->getBody());
	$0->u_c->AppendBody (ch->getBody()->Clone());
      }      
      $0->u_c->copyMethods (ch);

      InstType *chparent = NULL;
      while (ch->getParent()) {
	chparent = ch->getParent ();
	ch = dynamic_cast<Channel *>(chparent->BaseType());
	if (!ch) break;
      }
      $A(chparent);

      Chan *rootchan = dynamic_cast<Chan *>(chparent->BaseType());
      $A(rootchan);
      //ir = chparent->getTypeParam (0);
      ir = rootchan->datatype();
      ir2 = rootchan->acktype();
    }
    else {
      //ir = $5->getTypeParam (0);
      Chan *ch = dynamic_cast<Chan *>($5->BaseType());
      $A(ch);
      ir = ch->datatype();
      ir2 = ch->acktype();
    }
    if (!ir) {
      $E("Cannot find root built-in type");
    }
    $0->scope->Add ("self", ir);
    if (ir2) {
      $0->scope->Add ("selfack", ir2);
    }
    $0->u_c->SetParent ($5);
}}
 "(" [ port_formal_list ] ")" 
{{X:
    UserDef *u;

    if ((u = $0->curns->findType ($3))) {
      if (u->isEqual ($0->u_c)) {
	delete $0->u_c;
	$0->u_c = dynamic_cast<Channel *>(u);
	$A($0->u_c);
      }
      else {
	$E("Name ``%s'' previously defined as a different channel", $3);
      }
    }
    else {
      $A($0->curns->CreateType ($3, $0->u_c));
    }
    list_free ($7);
    $0->strict_checking = 0;
}}
data_chan_body
{{X:
    $0->u_c = NULL;
    $0->scope = $0->curns->CurScope ();
    return NULL;
}}
;

/*------------------------------------------------------------------------
 *
 * Enumerations: not implemented
 *
 *------------------------------------------------------------------------
 */
defenum: [ "export"] "defenum" ID [ ":" "int" ]
{{X:
    Data *d;
    UserDef *u;
    Data *td = NULL;

    switch ($0->curns->findName ($3)) {
    case 0:
      /* good */
      break;
    case 1:
      $A(u = $0->curns->findType ($3));
      if (TypeFactory::isDataType (u)) {
	td = dynamic_cast<Data *>(u);
	$A(td);
	if (!td->isEnum()) {
	  $E("Name ``%s''already used as a non-enumeration data type", $3);
	}
      }
      else {
	$E("Name ``%s'' already used in a previous type definition", $3);
      }
      break;
    case 2:
      $E("Name ``%s'' already used as a namespace", $3);
      break;
    case 3:
      $E("Name ``%s'' already used as an instance", $3);
      break;
    default:
      $E("Should not be here ");
      break;
    }

    int is_int = 0;
    if (!OPT_EMPTY ($4)) {
      is_int = 1;
    }
    OPT_FREE ($4);

    if (td && (is_int != !td->isPureEnum())) {
      $E("Name ``%s'' is a previous enum, with different :int attribute", $3);
    }

    $0->u = new UserDef($0->curns);
    $0->u->setFile ($n);
    $0->u->setLine ($l);
    d = new Data ($0->u);
    delete $0->u;
    $0->u = NULL;
    $0->u_d = d;
    $0->u_d->MkEnum(is_int);
    $0->u_d->MkDefined ();

    if (!OPT_EMPTY ($1)) {
      $0->u_d->MkExported ();
    }
    OPT_FREE ($1);
}}
enum_body
{{X:
    UserDef *u;

    $0->u_d->SetParent ($0->tf->NewEnum ($0->scope, Type::NONE,
					 const_expr ($0->u_d->numEnums())));
    
    if ((u = $0->curns->findType ($3))) {
      if (u->isDefined() && u->isEqual ($0->u_d)) {
	delete $0->u_d;
	$0->u_d = dynamic_cast <Data *> (u);
	$A($0->u_d);
      }
      else {
	$E("Name ``%s'' previously defined as a different enumeration", $3);
      }
    }
    else {
      $A($0->curns->CreateType ($3, $0->u_d));
    }

    $0->u_d = NULL;
    $0->scope = $0->curns->CurScope ();
    return NULL;
}};

enum_body[int]: "{" bare_id_list "}" ";"
{{X:
    listitem_t *li;

    for (li = list_first ($2); li; li = list_next (li)) {
      const char *s = (char *)list_value (li);
      $0->u_d->addEnum (s);
    }
    list_free ($2);
    return 1;
}}
;

bare_id_list[list_t *]: { ID "," }*
{{X:
    return $1;
}}
;


/*------------------------------------------------------------------------
 *
 * Functions
 *
 *------------------------------------------------------------------------
 */
deffunc: [ template_spec ] "function"
{{X:
    if (OPT_EMPTY ($1)) {
      $0->u = new UserDef ($0->curns);
      $0->u->setFile ($n);
      $0->u->setLine ($l);
    }
    else {
      $A($0->u);
    }
    OPT_FREE ($1);
    if ($0->u->getNumParams() > 0) {
      $0->func_template = 1;
    }
    else {
      $0->func_template = 0;
    }
}}
ID 
{{X:
    Function *f;
    UserDef *u;

    f = new Function ($0->u);
    delete $0->u;
    $0->u = NULL;
    
    switch ($0->curns->findName ($3)) {
    case 0:
      /* ok! */
      break;
    case 1:
      $A(u = $0->curns->findType ($3));
      if (TypeFactory::isFuncType (u)) {
	/* hope */
      }
      else {
	$E("Name ``%s'' already used in a previous type definition", $3);
      }
      break;
    case 2:
      $E("Name ``%s'' already used as a namespace", $3);
      break;
    case 3:
      $E("Name ``%s'' already used as an instance", $3);
      break;
    default:
      $E("Should not be here ");
      break;
    }
    $0->u_f = f;
    $0->strict_checking = 1;
    $0->scope = $0->u_f->CurScope ();
}}
"(" function_formal_list  ")"  ":" func_ret_type
{{X:
    UserDef *u;
    /* let's check to see if this matches any previous definition */

    if ((u = $0->curns->findType ($3))) {
      if (u->isEqual ($0->u_f)) {
	delete $0->u_f;
	$0->u_f = dynamic_cast<Function *>(u);
	$A($0->u_f);
	$0->scope = $0->u_f->CurScope();
      }
      else {
	$E("Name ``%s'' previously defined as a different type", $3);
      }
    }
    else {
      $A($0->curns->CreateType($3, $0->u_f));
    }
    if ($0->u_f->getNumPorts() > 0 && TypeFactory::isParamType ($8)) {
      $E("Function ``%s'': return type incompatible with arguments", $3);
    }
    if (($0->u_f->getNumParams() > 0 && !$0->func_template) && !TypeFactory::isParamType ($8)) {
      $E("Function ``%s'': return type incompatible with arguments", $3);
    }

    $0->scope->Add ("self", $8);
    $0->u_f->setRetType ($8);
    $0->strict_checking = 0;
}}
func_body
{{X:
    $0->u_f->setBody ($9);
    $0->u_f = NULL;
    $0->scope = $0->curns->CurScope();
    return NULL;
}}
;

func_body[ActBody *]: ";" 
{{X:
    return NULL;
}}
| "{" [ func_body_items ] "}"
{{X:
    ActRet *r;
    ActBody *b;

    $A($0->u_f);
    if ($0->u_f->isDefined ()) {
      $E("Function ``%s'' previously defined with same type signature", 
	 $0->u->getName ());
    }

    if (OPT_EMPTY ($2)) {
      OPT_FREE ($2);
      return NULL;
    }
    else {
      r = OPT_VALUE ($2);
      $A(r->type == R_ACT_BODY);

      b = r->u.body;
      
      FREE (r);
      OPT_FREE ($2);

      $0->u_f->MkDefined ();

      return b;
    }
}}
;

func_body_items[ActBody *]: alias_or_inst_list lang_chp
{{X:
    ActBody *b;

    b = $1;
    if (b) {
      b->Append ($2);
      return b;
    }
    else {
      return $2;
    }
}}
;

alias_or_inst_list[ActBody *]: al_item alias_or_inst_list
{{X:
    $1->Append ($2);
    return $1;
}}
| /* nothing */
;

al_item[ActBody *]: instance
{{X:
    ActBody_Inst *inst = dynamic_cast<ActBody_Inst *> ($1);
    $A(inst);
    if (TypeFactory::isDataType (inst->getType()) ||
	TypeFactory::isStructure (inst->getType()) ||
	TypeFactory::isParamType (inst->getType())) {
      return $1;
    }
    else {
      $E("Declaration for ``%s'': function body can only declare data types!",
	 inst->getName());
      return NULL;
    }
}}
| assertion
{{X:
    return $1;
}}
| debug_output
{{X:
    return $1;
}}
;


/*------------------------------------------------------------------------
 *
 * Core ACT language: body of process defintions
 *
 *------------------------------------------------------------------------
 */
def_body: base_item_list 
{{X:
    $0->u_p->AppendBody ($1);
    return NULL;
}}
| /* nothing */;


base_item_list[ActBody *]: base_item base_item_list 
{{X:
    if ($1) {
      $1->Append ($2);
      return $1;
    }
    else {
      return $2;
    }
}}
| base_item
{{X:
    return $1;
}}
;

base_item[ActBody *]: instance 
{{X: return $1; }}
| connection 
{{X: return $1; }}
| alias 
{{X: return $1; }}
| language_body
{{X:
    if ($1) {
      ActBody_Lang *l = dynamic_cast<ActBody_Lang *>($1);
      $A(l);
      if (l->gettype() == ActBody_Lang::LANG_REFINE) {
	if ($0->u_p) {
	  act_refine *r = (act_refine *) l->getlang();
	  $A(r);
	  if ($0->ref_level == 0) {
	    $0->u_p->mkRefined(r->nsteps);
	  }
	}
	else {
	  $E("refine { ... } blocks can only be used in processes.");
	}
      }
    }
    return $1;
}}
| loop 
{{X: return $1; }}
| conditional
{{X: return $1; }}
| assertion
{{X: return $1; }}
| debug_output
{{X: return $1; }}
;

debug_output[ActBody *]: "${" { chp_log_item "," }* "}" ";"
{{X:
    return new ActBody_Print ($l, $2);
}}
;

assertion[ActBody *]: "{" wbool_expr [ ":" STRING ] "}" ";"
{{X:
    ActBody *b;
    
    InstType *it = act_expr_insttype ($0->scope, $2, NULL, 0);
    if (!TypeFactory::isPBoolType (it->BaseType()) ||
	it->arrayInfo() != NULL) {
      $E("Assertion requires a Boolean expression of parameters/consts only");
    }
    delete it;
    
    if (OPT_EMPTY ($3)) {
      b = new ActBody_Assertion ($l, $2);
    }
    else {
      ActRet *r;
      r = OPT_VALUE ($3);
      $A(r->type == R_STRING);
      b = new ActBody_Assertion ($l, $2, r->u.str);
      FREE (r);
    }
    OPT_FREE ($3);
    return b;
}}
| "{" expr_id conn_op expr_id [ ":" STRING ] "}" ";"
{{X:
    int tc;
    ActBody *b;
    tc = act_type_var ($0->scope, $2, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    tc = act_type_var ($0->scope, $4, NULL);
    if (tc == T_ERR) {
      $e("Typechecking failed on expression!");
      fprintf ($f, "\n\t%s\n", act_type_errmsg ());
      exit (1);
    }
    
    if (OPT_EMPTY ($5)) {
      b = new ActBody_Assertion ($l, $2, $4, $3);
    }
    else {
      ActRet *r;
      r = OPT_VALUE ($5);
      $A(r->type == R_STRING);
      b = new ActBody_Assertion ($l, $2, $4, $3, r->u.str);
      FREE (r);
    }
    return b;
}}
;

conn_op[int]: "==="
{{X:
    return 0;
}}
| "!=="
{{X:
  return 1;
}}
;

instance[ActBody *]: inst_type
{{X:
    $0->t = $1;
    if ($1->getDir() == Type::INOUT || $1->getDir() == Type::OUTIN) {
      $E("`!?' and `?!' direction specifiers are reserved for port lists");
    }
}}
{ instance_id "," }* ";" 
{{X:
    listitem_t *li;
    ActBody *ret, *cur, *tl;

    /* Don't delete any TypeFactory cached instance types */
    if ($0->t->isTemp()) {
      delete $0->t;
    }
    $0->t = NULL;

    /* reserve a slot for this in the type table. the work is done in
       the instance_id thing */

    cur = NULL;
    ret = NULL;
    tl = NULL;

    for (li = list_first ($2); li; li = list_next (li)) {
      cur = (ActBody *)list_value (li);
      if (!cur) continue;

      if (!ret) {
	ret = cur;
	tl = ret->Tail ();
      }
      else {
	tl->Append (cur);
	tl = tl->Tail ();
      }
    }
    list_free ($2);
    return ret;
}}
;

special_connection_id[ActBody *]: ID [ dense_range ] 
{{X:
    $0->i_id = $1;
    $0->i_t = $0->scope->Lookup ($1);
    if (!$0->i_t) {
      $E("Identifier ``%s'' not found in current scope", $1);
    }
    $0->t_inst = NULL;
    if (!OPT_EMPTY ($2)) {
      ActRet *r;

      r = OPT_VALUE ($2);
      $A(r->type == R_ARRAY);
      $0->a_id = new ActId ($1, r->u.array);
      FREE (r);
    }
    else {
      $0->a_id = NULL;
    }
    OPT_FREE ($2);
}}
"(" port_conn_spec ")" [ "@" attr_list ]
{{X:
    ActBody *b;
    /* connections handled already */
    b = NULL;
    if (!OPT_EMPTY ($6)) {
      ActRet *r;

      r = OPT_VALUE ($6);
      $A(r->type == R_ATTR);
      if ($0->a_id) {
	b = new ActBody_Attribute ($l, $1, r->u.attr, $0->a_id->arrayInfo());
      }
      else {
	b = new ActBody_Attribute ($l, $1, r->u.attr);
      }
      FREE (r);
    }
    OPT_FREE ($6);
    $0->a_id = NULL;
    if (b) {
      b->Tail()->Append ($0->t_inst);
    }
    else {
      b = $0->t_inst;
    }
    $0->t_inst = NULL;
    return b;
}}
| ID [ dense_range ] "@" attr_list
{{X:
    Array *a = NULL;

    if (!OPT_EMPTY ($2)) {
      ActRet *r;
      r = OPT_VALUE ($2);
      $A(r->type == R_ARRAY);
      a = r->u.array;
      FREE (r);
    }
    OPT_FREE ($2);
    
    return new ActBody_Attribute ($l, $1, $4, a);
}}
;

instance_id[ActBody *]: ID [ sparse_range ] 
{{X:
    InstType *it;
    ActRet *r;

    $0->i_t = NULL;
    $0->i_id = $1;

    $A($0->t);

    /* Create the instance */
    //$0->i_t = $0->t;

    if (!OPT_EMPTY ($2)) {
      $A($0->t->arrayInfo() == NULL);
	
      r = OPT_VALUE ($2);
      $A(r->type == R_ARRAY);

      it = new InstType ($0->t);
      r->u.array->mkArray ();
      it->MkArray (r->u.array);
      FREE (r);
    }
    else {
      it = $0->t;
      /*
      if ($0->t->isTemp()) {
	it = new InstType ($0->t);
      }
      else {
	it = $0->t;
      }
      */
    }
    $0->i_t = it;

    $A($0->scope);
      
    /*printf ("scope: %x\n", $0->scope);*/

    InstType *prev_it;

    if ((prev_it = $0->scope->Lookup ($1))) {
      if (OPT_EMPTY ($2)) {
	/* not an array instance */

	/* What happens if I say 
	   [ .. -> bool a; [] ... -> bool a; ]
	   Check if conditional, and ignore it.
	*/
	if (!prev_it->isEqual (it, 1)) {
	  $E("Duplicate instance for name ``%s'' with incompatible types", $1);
	}
	else {
	  if ($0->in_cond == 0) {
	    $E("Duplicate instance for name ``%s''", $1);
	  }
	}
      }
      else {
	/* check weak compatibility of instance types, since this
	   could be a sparse array */
	if (!prev_it->isEqual (it, 1)) {
	  $e("Array instance for ``%s'' is incompatible with previous instance of the same name", $1);
	  fprintf ($f, "\n prev: ");
	  prev_it->Print ($f);
	  fprintf ($f, "\n here: ");
	  it->Print ($f);
	  fprintf ($f, "\n");
	  exit (1);
	}
	/* Type is fine; check if we are a port! If so, we have a
	   problem. To do this, we have to check if this instance_id
	   is within a UserDef or not. 
	*/
	if ($0->u_p) {
	  if ($0->u_p->FindPort ($1) != 0) {
	    $E("Array instance for ``%s'': cannot extend a port array", $1);
	  }
	}
      }
    }
    else {
      /* check if it shadows something */
      if ($0->scope->FullLookup ($1)) {
	if (_act_shadow_warning()) {
	  $W("Instance ``%s'' shadows another instance of the same name", $1);
	}
      }

      /* create a slot */
      if (it->arrayInfo()) {
	/* force it to be an array, and not a de-reference */
	it->arrayInfo()->mkArray ();
      }
      it = $0->tf->NewUserDef ($0->scope, it);
      $A($0->scope->Add ($1, it));
    }
    $0->t_inst = new ActBody_Inst ($l, it, $1);
}}
[ "(" port_conn_spec ")" ] [ "@" attr_list ] opt_extra_conn 
{{X:
    ActBody *b = NULL;
    ActRet *r;

    /* 1: Connections in the port conn spec are
       processed in port_conn_spec itself
    */
    OPT_FREE ($3);

    b = $0->t_inst;
    $0->t_inst = NULL;

    /* Handle attributes, if any */
    if (!OPT_EMPTY ($4)) {
      ActBody *btmp;
      /* An attribute list is a list of wrapped (string, expr)s */
      r = OPT_VALUE ($4);
      $A(r->type == R_ATTR);

      btmp = new ActBody_Attribute ($l, $1, r->u.attr);

      if (b) {
	b->Tail()->Append (btmp);
      }
      else {
	b = btmp;
      }
      FREE (r);
    }
    OPT_FREE ($4);

    /* 2: Connections on the RHS */
    if ($5) {
      listitem_t *li;
      ActBody *tmp;
      
      if (!OPT_EMPTY ($2)) {
	$E("Connection can only be specified for non-array instances");
      }

      if (b) {
	tmp = b->Tail ();
      }
      else {
	tmp = NULL;
      }

      /*printf ("Length: %d\n", list_length ($5));*/

      for (li = list_first ($5); li; li = list_next (li)) {
	ActId *a = new ActId ($1);
	AExpr *ae;
	ActRet *ar;

	ar = (ActRet *)list_value (li);
	$A(ar);
	$A(ar->type == R_AEXPR);

	ae = ar->u.ae;
	FREE (ar);

	/*printf ("Got: %x\n", ae);

	printf ("Connect: ");
	a->Print (stdout);
	printf (" to: ");
	ae->Print (stdout);
	printf ("\n");*/

	type_set_position ($l, $c, $n);
	if (!act_type_conn ($0->scope, a, ae)) {
	  $e("Typechecking failed on connection!");
	  fprintf ($f, "\n\t%s\n", act_type_errmsg ());
	  exit (1);
	}

	if (!tmp) {
	  tmp = new ActBody_Conn ($l, a, ae);
	}
	else {
	  tmp->Tail()->Append (new ActBody_Conn ($l, a, ae));
	  tmp = tmp->Tail ();
	}
      }
      list_free ($5);
    }
    OPT_FREE ($2);
    
    return b;
}}
;

opt_extra_conn[list_t *]: [ "=" { array_expr "=" }** ]
{{X:
    list_t *l;

    if (OPT_EMPTY ($1)) {
      OPT_FREE ($1);
      l = NULL;
    }
    else {
      ActRet *r;

      r = OPT_VALUE ($1);

      $A(r->type == R_LIST);

      l = r->u.l;
      FREE (r);
      OPT_FREE ($1);
    }
    return l;
}}
;

/* the "CONNECT" body statements are returned in t_inst */
port_conn_spec:  { "." ID "=" array_expr "," }**
{{X:
    UserDef *ud;
    ActBody *b, *tmp, *ret;
    ActRet *r;
    listitem_t *li;
    const char *str;
    AExpr *ae;
    int i;
    
    $A($0->i_t);
    $A($0->i_id);

    ud = dynamic_cast<UserDef *>($0->i_t->BaseType());

    if (!ud) {
      $E("Connection specifier used for instance ``%s'' whose root type is ``%s''\n\tnot a user-defined type", $0->i_id, $0->i_t->BaseType()->getName());
    }

    if ($0->i_t->arrayInfo()) {
      if (!$0->a_id) {
	$E("Connection specifier for an array instance ``%s''", $0->i_id);
      }
      else {
	$A($0->a_id->arrayInfo ());
	if ($0->a_id->arrayInfo()->nDims() !=
	    $0->i_t->arrayInfo()->nDims()) {
	  $E("Array de-reference for ``%s'': mismatch in dimensions (%d v/s %d)", $0->i_id, $0->i_t->arrayInfo()->nDims (), $0->a_id->arrayInfo()->nDims());
	}
      }
    }
	
    b = NULL;
    ret = NULL;

    for (li = list_first ($1); li; li = list_next (li)) {
      r = (ActRet *)list_value (li);
      $A(r->type == R_STRING);
      str = r->u.str;
      FREE (r);

      li = list_next (li);
      $A(li);
      r = (ActRet *)list_value (li);
      $A(r->type == R_AEXPR);
      ae = r->u.ae;
      FREE (r);
      
      for (i=ud->getNumPorts()-1; i >= 0; i--) {
	if (strcmp (str, ud->getPortName (i)) == 0) {
	  break;
	}
      }
      if (i < 0) {
	$E("``%s'' is not a valid port name for type ``%s''", str, 
	   ud->getName ());
      }

      ActId *id;

      if ($0->a_id) {
	id = $0->a_id->Clone();
      }
      else {
	id  = new ActId ($0->i_id, NULL);
      }
      id->Append (new ActId (str, NULL));
      type_set_position ($l, $c, $n);
      if (!act_type_conn ($0->scope, id, ae)) {
	$e("Typechecking failed on connection!");
	fprintf ($f, "\n\t%s\n", act_type_errmsg ());
	exit (1);
      }
      tmp = new ActBody_Conn ($l, id, ae);
      if (!b) {
	b = tmp;
	ret = b;
      }
      else {
	b->Append (tmp);
	b = b->Tail ();
      }
    }
    list_free ($1);
    if ($0->t_inst) {
      $0->t_inst->Tail()->Append (ret);
    }
    else {
      $0->t_inst = ret;
    }
    return NULL;
}}
| { opt_array_expr "," }*
{{X:
    int pos = 0;
    listitem_t *li;
    AExpr *ae;
    UserDef *ud;
    ActBody *b, *ret;

    $A($0->i_t);
    $A($0->i_id);
    
    ud = dynamic_cast<UserDef *>($0->i_t->BaseType());
    if (!ud) {
      $E("Connection specifier used for instance ``%s'' whose root type is ``%s''\n\t(not a user-defined type)", $0->i_id, $0->i_t->BaseType()->getName());
    }

    b = NULL;
    ret = NULL;

    /* If the ID is an array type, there had better be a deref; in
       this case, a_id is set with the appropriate ActId */
    if ($0->i_t->arrayInfo ()) {
      if (!$0->a_id) {
	$E("Connection specifier for an array instance ``%s''", $0->i_id);
      }
      else {
	$A($0->a_id->arrayInfo ());
	if ($0->a_id->arrayInfo()->nDims() !=
	    $0->i_t->arrayInfo()->nDims()) {
	  $E("Array de-reference for ``%s'': mismatch in dimensions (%d v/s %d)", $0->i_id, $0->i_t->arrayInfo()->nDims (), $0->a_id->arrayInfo()->nDims());
	}
      }
    }

    for (li = list_first ($1); li; li = list_next (li)) {
      if ((pos >= ud->getNumPorts()) && list_value (li)) {
	$E("Too many ports specified in connection specifier.\n\tType ``%s'' only has %d ports!", ud->getName(), ud->getNumPorts());
      }
      ae = (AExpr *) list_value (li);
      if (ae) {
	const char *pn = ud->getPortName (pos);
	ActId *id;
	ActBody *tmp;

	if ($0->a_id) {
	  /* array deref */
	  id = $0->a_id->Clone ();
	}
	else {
	  id = new ActId ($0->i_id, NULL);
	}
	id->Append (new ActId (pn, NULL));

	type_set_position ($l, $c, $n);
	if (!act_type_conn ($0->scope, id, ae)) {
	  $e("Typechecking failed on connection! (position: %d)", pos);
	  fprintf ($f, "\n\t%s\n", act_type_errmsg ());
	  exit (1);
	}
	tmp = new ActBody_Conn ($l, id, ae);
	if (!b) {
	  b = tmp;
	  ret = b;
	}
	else {
	  b->Append (tmp);
	  b = b->Tail ();
	}
      }
      pos++;
    }
    list_free ($1);
    if ($0->t_inst) {
      $0->t_inst->Tail()->Append (ret);
    }
    else {
      $0->t_inst = ret;
    }
    if ($0->a_id) {
      delete $0->a_id;
    }
    $0->a_id = NULL;
    return NULL;
}}
;


alias[ActBody *]: lhs_array_expr "=" array_expr ";"
//{ array_expr "=" }* ";"
{{X:
    ActBody *b, *tmp, *ret;
    AExpr *ae;

    b = NULL;
    ret = NULL;

    //for (li = list_first ($3); li; li = list_next (li)) {
    //ae = (AExpr *) list_value (li);
    ae = $3;
      type_set_position ($l, $c, $n);
      if (!act_type_conn ($0->scope, $1, ae)) {
	$e("Typechecking failed on connection!");
	fprintf ($f, "\n\t%s\n", act_type_errmsg ());
	exit (1);
      }
      //if (li == list_first ($3)) {
      tmp = new ActBody_Conn ($l, $1, ae);
	//}
	//else {
	//tmp = new ActBody_Conn ($1->Clone(), ae);
	//}
      if (!b) {
	b = tmp;
	ret = b;
      }
      else {
	b->Append (tmp);
	b = b->Tail ();
      }
  // }
  //list_free ($3);
    return ret;
}}
;

connection[ActBody *]: special_connection_id ";"
{{X: return $1; }}
;
                      
loop[ActBody *]: "(" [ ";" ] ID ":" !noreal wpint_expr [ ".." wpint_expr ] ":" 
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier `%s' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
    $0->in_cond++;
    if (!OPT_EMPTY ($2)) {
      $W("Using ``;'' in a ACT loop is deprecated. To fix this, just remove the semicolon.");
    }
    OPT_FREE ($2);
}}
   base_item_list ")"
{{X:
    $0->scope->Del ($3);
    $0->in_cond--;
    if (OPT_EMPTY ($6)) {
      OPT_FREE ($6);
      return new ActBody_Loop ($l, $3, $5, NULL, $8);
    }
    else {
      ActRet *r;
      Expr *tmp;
      r = OPT_VALUE ($6);
      $A(r->type == R_EXPR);
      tmp = r->u.exp;
      FREE (r);
      OPT_FREE ($6);
      return new ActBody_Loop ($l, $3, $5, tmp, $8);
    }
}}
| "*["
{{X: $0->in_cond++; }}

    { gc_1 "[]" }* "]" [";"]
    
{{X:
    listitem_t *li;
    ActBody_Select_gc *ret, *prev, *stmp;

    OPT_FREE ($4);

    ret = NULL;
    for (li = list_first ($2); li; li = list_next (li)) {
      stmp = (ActBody_Select_gc *) list_value (li);
      if (stmp->isElse()) {
	$E("`else' clause not permitted in a looping construct!");
      }
      if (!ret) {
	ret = stmp;
	prev = stmp;
      }
      else {
	prev->Append (stmp);
	prev = stmp;
      }
    }
    $0->in_cond--;
    return new ActBody_Genloop ($l, ret);
}}
;

conditional[ActBody *]: "["

{{X: $0->in_cond++; }}

                    guarded_cmds "]" [";"]

{{X:
    $0->in_cond--;
    OPT_FREE ($4);
    return $2; 
}}
;

guarded_cmds[ActBody *]: { gc_1 "[]" }*
{{X:
    listitem_t *li;
    ActBody_Select_gc *ret, *prev, *stmp;

    ret = NULL;
    for (li = list_first ($1); li; li = list_next (li)) {
      stmp = (ActBody_Select_gc *) list_value (li);
      if (!ret) {
	ret = stmp;
	prev = stmp;
      }
      else {
	prev->Append (stmp);
	prev = stmp;
      }
      if (stmp->isElse() && list_next (li)) {
	$E("`else' clause can only be the last clause in a selection");
      }
    }
    list_free ($1);
    return new ActBody_Select ($l, ret);
}}
;

gc_1[ActBody_Select_gc *]: wbool_expr "->" base_item_list
{{X:
    InstType *it = act_expr_insttype ($0->scope, $1, NULL, 0);
    if (!TypeFactory::isPBoolType (it->BaseType()) ||
	it->arrayInfo() != NULL) {
      $E("Conditional in ACT requires a Boolean expression of parameters/consts only");
    }
    delete it;
    
    return new ActBody_Select_gc ($1, $3);
}}
| "(" "[]" ID
{{X:
    if ($0->scope->Lookup ($3)) {
      $E("Identifier ``%s'' already defined in current scope", $3);
    }
    $0->scope->Add ($3, $0->tf->NewPInt());
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" wbool_expr "->" base_item_list ")"
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

    InstType *it = act_expr_insttype ($0->scope, $8, NULL, 0);
    if (!TypeFactory::isPBoolType (it->BaseType()) ||
	it->arrayInfo() != NULL) {
      $E("Conditional in ACT requires a Boolean expression of parameters/consts only");
    }
    delete it;
    
    return new ActBody_Select_gc ($3, $5, hi, $8, $10);
}}
| "else" "->" base_item_list
{{X:
    return new ActBody_Select_gc (NULL, $3);
}}
;


loop_base[ActBody *]: "(" [ ";" ] ID ":" !noreal wpint_expr [ ".." wpint_expr ] ":" 
{{X:
    lapply_X_loop_0_6 ($0, $2, $3, $5, $6);
}}
   base_body ")" [";"]
{{X:
    OPT_FREE ($10);
    return apply_X_loop_opt0 ($0, $2, $3, $5, $6, $8);
}}
| "*["
{{X:
    lapply_X_loop_1_0 ($0);
}}
  { gc_1_base "[]" }* "]" [";"]
{{X:
    return apply_X_loop_opt1 ($0, $2, $4);
}}
;

conditional_base[ActBody *]: "["
{{X:
    lapply_X_conditional_0_0 ($0);
}}
guarded_cmds_base "]" [";"]
{{X:
    return apply_X_conditional_opt0 ($0, $2, $4);
}}
;

guarded_cmds_base[ActBody *]: { gc_1_base "[]" }*
{{X:
    return apply_X_guarded_cmds_opt0 ($0, $1);
}}
;

gc_1_base[ActBody_Select_gc *]: wbool_expr "->" base_body
{{X:
    return apply_X_gc_1_opt0 ($0, $1, $3);
}}
| "(" "[]" ID
{{X:
    lapply_X_gc_1_1_2 ($0, $3);
}}
":" !noreal wpint_expr [ ".." wpint_expr ] ":" wbool_expr "->" base_body ")"
{{X:
    return apply_X_gc_1_opt1 ($0, $3, $5, $6, $8, $10);
}}
| "else" "->" base_body
{{X:
    return apply_X_gc_1_opt2 ($0, $3);
}}
;


/*------------------------------------------------------------------------
 *
 * Interfaces
 *
 *------------------------------------------------------------------------
 */
defiface: [ template_spec ] 
{{X:
    if (OPT_EMPTY ($1)) {
      $0->u = new UserDef($0->curns);
      $0->u->setFile ($n);
      $0->u->setLine ($l);
    }
    else {
      $A($0->u);
    }
    OPT_FREE ($1);
    $0->strict_checking = 1;
}}
"interface" ID 
{{X:
    Interface *iface;
    UserDef *u;

    iface = new Interface ($0->u);
    delete $0->u;
    $0->u = NULL;
    
    switch ($0->curns->findName ($3)) {
    case 0:
      /* whew */
      break;
    case 1:
      $A(u = $0->curns->findType ($3));
      if (TypeFactory::isInterfaceType(u)) {
	/* there is hope */
      }
      else {
	$E("Name ``%s'' already used as a previous type definition", $3);
      }
      break;
    case 2:
      $E("Name ``%s'' already used as a namespace", $3);
      break;
    case 3:
      $E("Name ``%s'' already used as an instance", $3);
      break;
    default:
      $E("Should not be here ");
      break;
    }
    $0->u_i = iface;
    /*printf ("Orig scope: %x\n", $0->scope);*/
    $0->scope = $0->u_i->CurScope ();
}}
 "(" [ port_formal_list ] ")" ";"
{{X:
    /* Create type here */
    UserDef *u;

    if ((u = $0->curns->findType ($3))) {
      /* check if the type signature is identical */
      if (u->isEqual ($0->u_i)) {
	delete $0->u_i;
	$0->u_i = dynamic_cast<Interface *>(u);
	$A($0->u_i);
      }
      else {
	$E("Name ``%s'' previously defined as a different interface", $3);
      }
    }
    else {
      $A($0->curns->CreateType ($3, $0->u_i));
    }
    OPT_FREE ($5);
    $0->strict_checking = 0;
    $0->scope =$0->curns->CurScope();
    return NULL;
}}
;
