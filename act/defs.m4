/*************************************************************************
 *
 *  Copyright (c) 2011-2018 Rajit Manohar
 *  All Rights Reserved
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
template_spec: "export"
{{X:
    $A($0->u == NULL);
    $0->u = new UserDef ($0->curns);
    $0->u->MkExported();
}}
| [ "export" ] "template"
{{X:
    $A($0->u == NULL);
    $0->u = new UserDef ($0->curns);
    if (!OPT_EMPTY ($1)) {
      $0->u->MkExported();
    }
    $0->param_mode = 0;
    OPT_FREE ($1);
    $0->strict_checking = 1;
}}
"<" { param_inst ";" }* ">"
{{X:
    $0->param_mode = 1;
    list_free ($4);
    return NULL;
}}
;

param_inst: param_type id_list
{{X:
    listitem_t *li;
    ActRet *r;
    InstType *it;

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

      it = $1;
      
      if (OPT_EMPTY (m)) {
	/* nothing needs to be done */
      }
      else {
	r = OPT_VALUE (m);
	$A(r->type == R_ARRAY);

	if (TypeFactory::isPTypeType (it->BaseType())) {
	  $E("ptype parameters cannot be arrays!");
	}
	it->MkArray (r->u.array);
	FREE (r);
      }
      list_free (m);

      if ($0->u->AddMetaParam (it, id_name, $0->param_mode) != 1) {
	$E("Duplicate meta-parameter name in port list: ``%s''", id_name);
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
[ is_a ID ]
{{X:
    if (!OPT_EMPTY ($4)) {
      ActRet *r;
      void *v1, *v2;
      listitem_t *li;
      int v;
      const char *id;

      r = OPT_VALUE ($4);
      $A(r->type == R_LIST);
      
      li = list_first (r->u.l);
      $A(li);
      v1 = list_value (li);

      $A(list_next (li));
      li = list_next (li);
      v2 = list_value (li);
      
      $A(list_next (li) == NULL);
      list_free (r->u.l);
      FREE (r);

      r = (ActRet *) v1;
      $A(r->type == R_INT);
      v = r->u.ival;
      FREE (r);
      
      r = (ActRet *)v2;
      $A(r->type == R_STRING);
      id = r->u.str;
      FREE (r);

      /* XXX: inheritence type v, parent id */
    }
    OPT_FREE ($4);
}}
[ "(" port_formal_list ")" ] 
{{X:
    /* Create type here */
    UserDef *u;

    if (u = $0->curns->findType ($3)) {
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
    OPT_FREE ($5);
    $0->scope = $0->u_p->CurScope ();
    $0->strict_checking = 0;
}}
proc_body
{{X:
    $0->u_p = NULL;
    $0->scope = $0->curns->CurScope();
    return NULL;
}}
;

proc_body: ";"
{{X:
    return NULL;
}}
| "{" def_body  "}"
{{X:
    if ($0->u_p->isDefined()) {
      $E("Process ``%s'': duplicate definition with the same type signature", $0->u_p->getName());
    }
    $0->u_p->MkDefined ();
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

single_port_item: [ "+" ] /* override */  physical_inst_type id_list
{{X:
    listitem_t *li;
    ActRet *r;
    InstType *it;
    UserDef *u;
    int is_override;

    if (OPT_EMPTY ($1)) {
      is_override = 0;
    }
    else {
      is_override = 1;
    }
    OPT_FREE ($1);

    /* Make sure that port types are acceptable */
    if ($0->u_p) {
      /* We are currently processing a defproc port list */
      u = $0->u_p;
      if (TypeFactory::isProcessType ($2->BaseType())) {
	r = (ActRet *) list_value (list_first ($3));
	$A(r->type == R_STRING);
	$E("Parameter ``%s'': port parameter for a process cannot be a process", r->u.str);
      }
    }
    else if ($0->u_d) {
      /* This is a user-defined data type */
      u = $0->u_d;
      const char *err = NULL;

      if (TypeFactory::isProcessType ($2->BaseType())) {
	err = "process";
      }
      else if (TypeFactory::isChanType ($2->BaseType())) {
	err = "channel";
      }
      if (err) {
	r = (ActRet *) list_value (list_first ($3));
	$A(r->type == R_STRING);
	$E("Parameter ``%s'': port parameter for a function cannot be a %s",
	   r->u.str, err);
      }
    }
    else if ($0->u_f) {
      /* This is a user-defined function */
      u = $0->u_f;
      const char *err = NULL;

      if (TypeFactory::isProcessType ($2->BaseType())) {
	err = "process";
      }
      else if (TypeFactory::isChanType ($2->BaseType())) {
	err = "channel";
      }
      if (err) {
	r = (ActRet *) list_value (list_first ($3));
	$A(r->type == R_STRING);
	$E("Parameter ``%s'': port parameter for a data-type cannot be a %s",
	   r->u.str, err);
      }
    }
    else if ($0->u_c) {
      /* This is a user-defined channel type */
      u = $0->u_c;
      if (TypeFactory::isProcessType ($2->BaseType())) {
	r = (ActRet *) list_value (list_first ($3));
	$A(r->type == R_STRING);
	$E("Parameter ``%s'': port parameter for a channel cannot be a process", r->u.str);
      }
    }
    else {
      /* should not be here */
      $A(0);
    }

    /* Walk through identifiers */
    for (li = list_first ($3); li; li = list_next (li)) {
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
	it = $2;
      }
      else {
	/* we need to replicate the insttype */
	it = new InstType ($2);
	r = OPT_VALUE (m);
	$A(r->type == R_ARRAY);
	r->u.array->mkArray ();
	it->MkArray (r->u.array);
	it->MkCached ();
	FREE (r);
      }
      list_free (m);

      if (u->AddPort (it, id_name) != 1) {
	/* XXX: might be an override */
	if (is_override) {
	  $E("Could be an override, so fix it!\n");
	}
	else {
	  if ($0->u_f) {
	    $E("Duplicate parameter name in argument list: ``%s''", id_name);
	  }
	  else {
	    $E("Duplicate parameter name in port list: ``%s''", id_name);
	  }
	}
      }
      else {
	if (is_override && $0->u_f) {
	  $E("Overrides not permitted for function definitions");
	}
	if (is_override) {
	  $E("Override specified, but parameter ``%s'' does not exist!", 
	     id_name);
	}
      }
    }
    list_free ($3);
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
is_a physical_inst_type
{{X:
    /* parent type cannot be
       - process
       - channel
       
       Note that meta-params are ruled out by parser rules 
    */
    if (TypeFactory::isProcessType ($5->BaseType())) {
      $E("A data type cannot be related to a process");
    }
    if (TypeFactory::isChanType ($5->BaseType())) {
      $E("A data type cannot be related to a channel");
    }
    if (TypeFactory::isUserType ($5->BaseType ())) {
      $0->scope->Merge (dynamic_cast<UserDef *>($5->BaseType ())->CurScope ());
    }
    $0->u_d->SetParent ($5, $4);
}}
[ "("  port_formal_list ")" ]
{{X:
    UserDef *u;

    if (u = $0->curns->findType ($3)) {
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
data_body
{{X:
    /* body of data type: this gets updated in the actions for
       data_body */
    $0->u_d = NULL;
    $0->scope = $0->curns->CurScope ();
    return NULL;
}}
;

is_a[int]: "<:" 
{{X:
    return 0;
}}
| "="
{{X:
    return 1;
}}
;

data_body: ";"
	   /* empty */
{{X: return NULL; }}
| "{" base_body [ methods_body ] "}"
{{X:
    ActRet *r;

    $A($0->u_d);
    if ($0->u_d->isDefined ()) {
      $E("Data definition ``%s'': duplicate definition with the same type signature", $0->u_d->getName ());
    }
    $0->u_d->MkDefined();

    $0->u_d->setBody ($2);
    OPT_FREE ($3);
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
	if ($0->u_d->getMethodset()) {
	  $E("Duplicate ``set'' method");
	}
	$0->u_d->setMethodset ($3);
      }
      else if (strcmp ($1, "get") == 0) {
	if ($0->u_d->getMethodget()) {
	  $E("Duplicate ``get'' method");
	}
	$0->u_d->setMethodget ($3);
      }
      else {
	$E("Method ``%s'' is not supported", $1);
      }
    }
    else if ($0->u_c) {
      /* channel methods */
      if (strcmp ($1, "send") == 0) {
	if ($0->u_c->getMethodsend()) {
	  $E("Duplicate ``send'' method");
	}
	$0->u_c->setMethodsend ($3);
      }
      else if (strcmp ($1, "recv") == 0) {
	if ($0->u_c->getMethodrecv()) {
	  $E("Duplicate ``recv'' method");
	}
	$0->u_c->setMethodrecv ($3);
      }
      else {
	$E("Method ``%s'' is not supported", $1);
      }
    }
    else {
      $E("Methods body in unknown context?");
    }
    return NULL;
}}
;

/*
  For both channel and data types you can have spec bodies and aliases
*/
base_body[ActBody *]: lang_spec base_body
{{X:
    if ($1) {
      $1->Append ($2);
      return $1;
    }
    else {
      return $2;
    }
}}
| alias base_body
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
is_a physical_inst_type
{{X:
    if (TypeFactory::isProcessType ($5->BaseType())) {
      $E("A channel type cannot be related to a process");
    }
    if (TypeFactory::isDataType ($5->BaseType())) {
      $E("A channel type cannot be related to a data type");
    }
    if (TypeFactory::isUserType ($5->BaseType ())) {
      $0->scope->Merge (dynamic_cast<UserDef *>($5->BaseType ())->CurScope ());
    }
    $0->u_c->SetParent ($5, $4);
}}
 [ "(" port_formal_list ")" ] 
{{X:
    UserDef *u;

    if (u = $0->curns->findType ($3)) {
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
    list_free ($6);
    $0->strict_checking = 0;
}}
chan_body
{{X:
    $0->u_c = NULL;
    $0->scope = $0->curns->CurScope ();
    return NULL;
}}
;

chan_body: ";" | "{" base_body [ methods_body ] "}"
{{X:
    $A($0->u_c);
    if ($0->u_c->isDefined ()) {
      $E("Channel definition ``%s'': duplicate definition with the same type signature", $0->u_c->getName ());
    }
    $0->u_c->MkDefined ();

    $0->u_c->setBody ($2);
    OPT_FREE ($3);
    return NULL;
}}
;


/*------------------------------------------------------------------------
 *
 * Enumerations
 *
 *------------------------------------------------------------------------
 */
defenum: "defenum" ID
{{X:
    Data *d;
    UserDef *u;

    switch ($0->curns->findName ($2)) {
    case 0:
      /* good */
      break;
    case 1:
      $A(u = $0->curns->findType ($2));
      if (TypeFactory::isDataType (u)) {
	Data *td = dynamic_cast<Data *>(u);
	$A(td);
	if (!td->isEnum()) {
	  $E("Name ``%s''already used as a non-enumeration data type", $2);
	}
      }
      else {
	$E("Name ``%s'' already used in a previous type definition", $2);
      }
      break;
    case 2:
      $E("Name ``%s'' already used as a namespace", $2);
      break;
    case 3:
      $E("Name ``%s'' already used as an instance", $2);
      break;
    default:
      $E("Should not be here ");
      break;
    }

    u = new UserDef($0->curns);
    d = new Data (u);
    delete u;
    $0->u_d = d;
}}
enum_body
{{X:
    UserDef *u;

    if ((u = $0->curns->findType ($2))) {
      if (u->isDefined() && ($3 == 1)) {
	$E("enum ``%s'': duplicate definition", $2);
      }
      if (!u->isDefined() && ($3 == 1)) {
	u->MkCopy ($0->u_d);
	u->MkDefined();
      }
      delete $0->u_d;
    }
    else {
      $A($0->curns->CreateType ($2, $0->u_d));
    }
    $0->u_d = NULL;
    $0->scope = $0->curns->CurScope ();
    return NULL;
}};

enum_body[int]: ";" 
{{X:
    /* nothing to do here other than marking this as an enumeration */
    $0->u_d->MkEnum();
    return 0;
}}
| "{" bare_id_list "}" ";"
{{X:
    listitem_t *li;

    for (li = list_first ($2); li; li = list_next (li)) {
      const char *s = (char *)list_value (li);
      $0->u_d->AddMetaParam (NULL, s);
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
 * Functions: XXX fixme
 *
 *------------------------------------------------------------------------
 */
deffunc: [ "export" ] "function"
{{X:
    $0->u = new UserDef ($0->curns);
    if (!OPT_EMPTY ($1)) {
      $0->u->MkExported();
    }
    OPT_FREE ($1);
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
    $0->scope = $0->u_f->CurScope ();
}}
"(" port_formal_list ")" [ ":" param_type ] 
{{X:
    UserDef *u;
    /* let's check to see if this matches any previous definition */

    if (u = $0->curns->findType ($3)) {
      if (u->isEqual ($0->u_f)) {
	delete $0->u_f;
	$0->u_f = dynamic_cast<Function *>(u);
	$A($0->u_f);
      }
      else {
	$E("Name ``%s'' previously defined as a different type", $3);
      }
    }
    else {
      $A($0->curns->CreateType($3, $0->u_f));
    }

    if (!OPT_EMPTY ($7)) {
      ActRet *r;
      r = OPT_VALUE ($7);
      $A(r->type == R_INST_TYPE);
      $0->u_f->setRetType (r->u.inst);
      FREE (r);
    }
    else {
      $0->u_f->setRetType (NULL);
    }
    OPT_FREE ($7);
 
    $0->strict_checking = 0;
}}
func_body
{{X:
    $0->u_f->setBody ($8);
    $0->u_f = NULL;
    $0->scope = $0->curns->CurScope();
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

func_body_items[ActBody *]: { alias_or_inst ";" }* lang_chp
{{X:
    listitem_t *li;
    ActBody *b, *tmp, *tl;

    b = (ActBody *)list_value (list_first ($1));
    tl = b;
    for (li = list_next (list_first ($1)); li; li = list_next (li)) {
      tmp = (ActBody *)list_value (li);
      tl->Append (tmp);
      tl = tmp;
    }
    list_free ($1);
    tl->Append ($2);
    return b;
}}
;

alias_or_inst[ActBody *]: alias
{{X:
    return $1;
}}
| instance
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
    $0->u_p->setBody ($1);
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
{{X: return $1; }}
| loop 
{{X: return $1; }}
| conditional
{{X: return $1; }};


instance[ActBody *]: [ "+" ] inst_type
{{X:
    if (!OPT_EMPTY ($1)) {
      $0->override = 1;
    }
    else {
      $0->override = 0;
    }
    OPT_FREE ($1);
    $0->t = $2;
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

    for (li = list_first ($3); li; li = list_next (li)) {
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
    list_free ($3);
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
    if (!OPT_EMPTY ($6)) {
      /* XXX: handle attributes, if any */
    }
    OPT_FREE ($6);
    $0->a_id = NULL;
    b = $0->t_inst;
    $0->t_inst = NULL;
    return b;
}}
| ID [ dense_range ] "@" attr_list
{{X:
    /* XXX: attributes */
    return NULL;
}}
;

instance_id[ActBody *]: ID [ sparse_range ] 
{{X:
    InstType *it;
    ActRet *r;

    if ($0->override) {
      /* XXX: IT IS AN OVERRIDE */
      $W("Override, figure out what to do!\n");
    }

    $0->i_t = NULL;
    $0->i_id = $1;

    $A($0->t);

    /* Create the instance */
    $0->i_t = $0->t;

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

    $A($0->scope);
      
    /*printf ("scope: %x\n", $0->scope);*/

    InstType *prev_it;

    if (prev_it = $0->scope->Lookup ($1)) {
      if (OPT_EMPTY ($2)) {
	/* not an array instance */

	/* XXX: what happens if I say 
	   [ .. -> bool a; [] ... -> bool a; ]
	   Right now  the program will complain. Is that okay, or
	   should we punt? We could put in a check to see if the two
	   might be exclusive, and then check again at instantiation
	   time. 

	   Check if conditional, and ignore it.
	*/
	$E("Duplicate instance for name ``%s''", $1);
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
	$W("Instance ``%s'' shadows another instance of the same name", $1);
      }

      /* create a slot */
      if (it->arrayInfo()) {
	/* force it to be an array, and not a de-reference */
	it->arrayInfo()->mkArray ();
      }
      it = $0->tf->NewUserDef ($0->scope, it);
      $A($0->scope->Add ($1, it));
    }
    $0->t_inst = new ActBody_Inst (it, $1);
}}
[ "(" port_conn_spec ")" ] [ "@" attr_list ] opt_extra_conn 
{{X:
    ActBody *b = NULL;
    ActRet *r;

    /* Handle attributes, if any */
    if (!OPT_EMPTY ($4)) {
      /* XXX: ignoring attributes at the moment */
      /* XXX: add attributes to this instance */

      /* free attribute list
	 An attribute list is a list of wrapped (string, expr)s
      */
      r = OPT_VALUE ($4);
      $A(r->type == R_ATTR);
      FREE (r);
      OPT_FREE ($4);
    }

    /* XXX: Process connections */

    /* 1: Connections in the port conn spec are
       processed in port_conn_spec itself
    */
    OPT_FREE ($3);

    b = $0->t_inst;
    $0->t_inst = NULL;

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

	/*printf ("chk: "); a->Print (stdout); printf ("\n"); fflush (stdout);*/

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
	  tmp = new ActBody_Conn (a, ae);
	}
	else {
	  tmp->Tail()->Append (new ActBody_Conn (a, ae));
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
port_conn_spec: { opt_array_expr "," }*
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
      if (pos > ud->getNumPorts()) {
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
	  $e("Typechecking failed on connection!");
	  fprintf ($f, "\n\t%s\n", act_type_errmsg ());
	  exit (1);
	}
	tmp = new ActBody_Conn (id, ae);
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
| { "." ID "=" array_expr "," }**
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
      $E("Connection specifier used for instance ``%s'' whose root type is ``%s''\n\tnot a user-defined type", $0->i_id, ud->getName());
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
      ActId *id  = new ActId ($0->i_id, NULL);
      id->Append (new ActId (str, NULL));
      type_set_position ($l, $c, $n);
      if (!act_type_conn ($0->scope, id, ae)) {
	$e("Typechecking failed on connection!");
	fprintf ($f, "\n\t%s\n", act_type_errmsg ());
	exit (1);
      }
      tmp = new ActBody_Conn (id, ae);
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
;

alias[ActBody *]: array_expr "=" { array_expr "=" }* ";"
{{X:
    ActBody *b, *tmp, *ret;
    listitem_t *li;
    AExpr *ae;

    b = NULL;
    ret = NULL;

    for (li = list_first ($3); li; li = list_next (li)) {
      ae = (AExpr *) list_value (li);
      type_set_position ($l, $c, $n);
      if (!act_type_conn ($0->scope, $1, ae)) {
	$e("Typechecking failed on connection!");
	fprintf ($f, "\n\t%s\n", act_type_errmsg ());
	exit (1);
      }
      if (li == list_first ($3)) {
	tmp = new ActBody_Conn ($1, ae);
      }
      else {
	tmp = new ActBody_Conn ($1->Clone(), ae);
      }
      if (!b) {
	b = tmp;
	ret = b;
      }
      else {
	b->Append (tmp);
	b = b->Tail ();
      }
    }
    list_free ($3);
    return ret;
}}
;

connection[ActBody *]: special_connection_id ";"
{{X: return $1; }}
;

loop[ActBody *]: "(" ID ":" !noreal wint_expr [ ".." wint_expr ] ":" 
{{X:
    if ($0->scope->Lookup ($2)) {
      $E("Identifier `%s' already defined in current scope", $2);
    }
    $0->scope->Add ($2, $0->tf->NewPInt());
}}
   base_item_list ")"
{{X:
    $0->scope->Del ($2);
    if (OPT_EMPTY ($5)) {
      return new ActBody_Loop (ActBody_Loop::SEMI, $2, NULL, $4, $7);
    }
    else {
      ActRet *r;
      r = OPT_VALUE ($5);
      OPT_FREE ($5);
      $A(r->type == R_EXPR);
      return new ActBody_Loop (ActBody_Loop::SEMI, $2, $4, r->u.exp, $7);
    }
}}
;

conditional[ActBody *]: "[" guarded_cmds "]"
{{X:
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
    }
    return new ActBody_Select (ret);
}}
;

gc_1[ActBody_Select_gc *]: wbool_expr "->" base_item_list
{{X:
    return new ActBody_Select_gc ($1, $3);
}}
| "else" "->" base_item_list
{{X:
    return new ActBody_Select_gc (NULL, $3);
}}
;
