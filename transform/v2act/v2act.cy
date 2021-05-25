/*
  Structural Verilog to ACT file
*/
%type[X] {{ VNet VRet }};

ver_file: one_file_item  ver_file | one_file_item ;

one_file_item: module
| "`timescale" INT ID "/" INT ID
;

module: "module" ID
{{X:
    module_t *m;

    NEW (m, module_t);
    m->space = NULL;
    m->next = NULL;
    A_INIT (m->conn);
    A_INIT (m->port_list);
    A_INIT (m->dangling_list);
    m->H = hash_new (128);
    m->b = hash_lookup ($0->M, $2);
    m->inst_exists = 0;
    m->flags = 0;
    m->hd = NULL;
    m->tl = NULL;
    if (m->b) {
      $E("Duplicate module name `%s'", $2);
    }
    m->b = hash_add ($0->M, $2);
    m->b->v = m;

    /* m is now the current module */
    q_ins ($0->hd, $0->tl, m);

    hash_bucket_t *x;
    x = hash_lookup ($0->missing, $2);
    if (x) {
      id_info_t *nm, *tmp;
      /* patching missing module links */
      nm = (id_info_t *)x->v;
      while (nm) {
	tmp = nm->nxt;
	nm->nxt = NULL;
	nm->m = m;
	update_id_info (nm);
	nm = tmp;
      }
      /* now that we've found it, it should no longer be missing! */
      hash_delete ($0->missing, $2);
    }
}}
"(" port_list ")" ";" module_body
;

port_list: { portname "," }* 
;

portname: id
{{X:
    $1->isport = 1;
    A_NEW (CURMOD($0)->port_list, id_info_t *);
    A_NEXT (CURMOD($0)->port_list) = $1;
    A_INC (CURMOD($0)->port_list);
    return NULL;
}}
| one_port
{{X:
    $1->isport = 1;
    A_NEW (CURMOD($0)->port_list, id_info_t *);
    A_NEXT (CURMOD($0)->port_list) = $1;
    A_INC (CURMOD($0)->port_list);
    return NULL;
}}
;

id[id_info_t *]: ID
{{X:
    id_info_t *id;
    char *tmp = NULL;

    if ($1[0] == '\\') {
      char buf[10240];
      MALLOC (tmp, char, strlen ($1));
      strcpy (tmp, $1+1);
      if ($0->a->mangle_string (tmp, buf, 10240) != 0) {
	$E("Act::mangle_string failed on string `%s'", $1);
      }
      FREE (tmp);

      id = verilog_find_id ($0, buf);
      if (id) {
	$0->flag = 0;
      }
      else {
	$0->flag = 1;
	id = verilog_gen_id ($0, buf);
      }
    }
    else {
      id = verilog_find_id ($0, $1);
      if (id) {
	$0->flag = 0;
      }
      else {
	$0->flag = 1;
	id = verilog_gen_id ($0, $1);
      }
    }
    return id;
}}
;

module_body: decls 
{{X:
    int i;
    conn_info_t **c;
    conn_rhs_t *r;

    c = CURMOD($0)->conn;
    
    for (i=0; i < A_LEN (CURMOD($0)->conn); i++) {
      $A(c[i]->id.id->isport);
      if (c[i]->id.id->isinput || c[i]->id.id->isoutput) continue;
      if (c[i]->l) {
	r = (conn_rhs_t *) list_value (list_first (c[i]->l));
      }
      else {
	r = c[i]->r;
      }
      $A(r);
      if (r->id.id->isinput) {
	c[i]->id.id->isinput = 1;
      }
      else if (r->id.id->isoutput) {
	c[i]->id.id->isoutput = 1;
      }
      else {
	$W("Port name `%s' in module `%s': input/output direction unknown\n", c[i]->id.id->myname, CURMOD($0)->b->key);
      }
    }
}}
assigns_or_instances "endmodule" 
;

one_decl: decl_type [ "[" INT ":" INT "]" ] { id "," }**  ";"
{{X:
    listitem_t *li;
    int isarray;
    int lo, hi;
    VRet *r;
    id_info_t *id;
    int i;

    if (OPT_EMPTY ($2)) {
      isarray = 0;
    }
    else {
      r = LIST_VALUE(list_first ($2));
      $A(r->type == V_INT);
      lo = r->u.i;
      FREE (r);
      r = LIST_VALUE (list_next (list_first ($2)));
      $A(r->type == V_INT);
      hi = r->u.i;
      FREE (r);
      list_free ($2);
      if (lo > hi) {
	isarray = hi;
	hi = lo;
	lo = isarray;
      }
      isarray = 1;
    }
    for (li = list_first ($3); li; li = list_next (li)) {
      r = LIST_VALUE (li);
      $A(r->type == V_ID);
      id = r->u.id;
      FREE (r);

      if (A_LEN (id->a) > 0) {
	if (!((A_LEN (id->a) == 1 && isarray == 1))) {
	  $E("Identifier `%s' error in repeated array definition", id->myname);
	}
	if (id->a[0].lo != lo || id->a[0].hi != hi) {
	  $E("Identifier `%s' dims don't match", id->myname);
	}
	/* good, move on */
      }
      else {
	if (isarray) {
	  /* this id is an array! */
	  A_NEW (id->a, struct array_idx);
	  A_NEXT (id->a).lo = lo;
	  A_NEXT (id->a).hi = hi;
	  A_INC (id->a);
	}
      }
      /* now set to decl type */
      switch ($1) {
      case 0:
	id->isinput = 1;
	break;
      case 1:
	id->isoutput = 1;
	break;
      case 2:
	/* nothing */
	break;
      default:
	$E("Unknown decl type %d", $1);
	break;
      }
    }
    return NULL;
}}
;

decl_type[int]: "input" 
{{X: 
    return 0; 
}} 
| "output"
{{X:
    return 1;
}}
| "wire"
{{X:
    return 2;
}}
;

decls: one_decl decls | /* empty */ ;

assigns_or_instances: assign_or_inst assigns_or_instances
| /* empty */;


assign_or_inst: one_assign |  one_instance ;

assigns: one_assign assigns | /* empty */ ;

one_assign: "assign" id_deref "=" id_deref ";"
{{X:
    conn_rhs_t *r;
    conn_info_t *ci;

    NEW (r, conn_rhs_t);
    r->id = *($4);
    FREE ($4);
    r->issubrange = 0;

    NEW (ci, conn_info_t);

    A_NEW (CURMOD($0)->conn, conn_info_t *);
    A_NEXT (CURMOD ($0)->conn) = ci;
    A_INC (CURMOD ($0)->conn);
    
    ci->r = r;
    ci->l = NULL;
    ci->prefix = NULL;
    ci->id = *($2);
    ci->isclk = 0;
    FREE ($2);

    return NULL;
}}
| "assign" id_deref "=" id_deref_range_list ";"
{{X:
    conn_rhs_t *r;
    conn_info_t *ci;
    listitem_t *li;
    int idx;
    int len_gt_1 = 0;

    if (list_length ($4) > 1) {
      id_info_t *x;
      len_gt_1 = 1;
      if ($2->isderef) {
	$E("LHS of assign statement cannot include array deref");
      }

      x = $2->id;
      if (A_LEN (x->a) != 1) {
	$E("LHS must be a simple 1-D array only");
      }
      idx = x->a[0].hi;
    }
    else {
      idx = -1;
    }

    for (li = list_first ($4); li; li = list_next (li)) {
      id_deref_t *x;

      x = (id_deref_t *) list_value (li);
      
      NEW (r, conn_rhs_t);
      r->id = *x;
      FREE (x);
      r->issubrange = 0;

      NEW (ci, conn_info_t);

      A_NEW (CURMOD($0)->conn, conn_info_t *);
      A_NEXT (CURMOD ($0)->conn) = ci;
      A_INC (CURMOD ($0)->conn);
      
      ci->r = r;
      ci->l = NULL;
      ci->prefix = NULL;
      ci->id = *($2);
      ci->isclk = 0;

      if (len_gt_1) {
	ci->id.isderef = 1;
	ci->id.deref = idx--;
      }
    }
    list_free ($4);
    FREE ($2);
    return NULL;
}}
| "assign" id_deref_range "=" id_deref_range_list ";"
{{X:
    listitem_t *li1, *li2;

    if (list_length ($2) != list_length ($4)) {
      $W("assign statement is incompatible? (%d vs %d items)",
	 list_length ($2), list_length ($4));
    }

    for (li1 = list_first ($2), li2 = list_first ($4);
	 li1 && li2; li1 = list_next (li1), li2 = list_next (li2)) {
      id_deref_t *x1, *x2;

      x1 = (id_deref_t *) list_value (li1);
      x2 = (id_deref_t *) list_value (li2);

      apply_X_one_assign_opt0 ($0, x1, x2);
    }
    list_free ($2);
    list_free ($4);
    return NULL;
}}
;

instances: one_instance instances | /* empty */ ;

one_instance: id id 
{{X:
    hash_bucket_t *b;
    int i;

    q_ins ($0->tl->hd, $0->tl->tl, $2);

    $1->ismodname = 1;
    /* lookup id in the module table */
    b = hash_lookup ($0->M, $1->myname);
    if (!b) {
      $2->m = NULL;
      /* external module */
      if (($2->p = verilog_find_lib ($0->a, $1->myname))) {
	/* okay */
      }
      else {
	/* could be a module that appears later; add it to the
	   back-patch list */
	//$E("Reference to missing library module `%s'", $1->b->key);

	/* maybe this is later? */
	b = hash_lookup ($0->missing, $1->myname);
	if (!b) {
	  b = hash_add ($0->missing, $1->myname);
	  b->v = NULL;
	}
	$2->nxt = (id_info_t *)b->v;
	b->v = $2;
      }
      if ($2->p) {
	$2->nused = $2->p->getNumPorts();
      }
    }
    else {
      module_t *m;

      m = (module_t *)b->v;
      m->inst_exists = 1;
      $2->p = NULL;
      $2->m = m;
      $2->nused = A_LEN ($2->m->port_list);
    }
    /*
    MALLOC ($2->used, int, $2->nused);
    for (i=0; i < $2->nused; i++) {
      $2->used[i] = 0;
    }
    */
    update_id_info ($2);
    $2->isinst = 1;
    $2->nm = $1->myname;
    $0->prefix = $2;
}}
"(" port_conns ")" ";"
{{X:
    update_conn_info ($2);
    $0->prefix = NULL;
    return NULL;
}}
 ;

port_conns: { one_port2 "," }*
| { bad_port_style "," }*
{{X:
    $0->port_count = 0;
    return NULL;
}}
;

one_port[id_info_t *]: "." id
{{X:
    /* THIS .id should not be part of the type table */
    $0->tmpid = $2;
    if ($0->flag) {
      $0->tmpid = verilog_alloc_id (Strdup ($2->myname));
      /* delete from table! */
      verilog_delete_id ($0, $2->myname);
      FREE ($2);
    }
}}
"(" id_or_const_or_array ")" 
{{X:
    int len;
    /* now add pending connections! id = blah */
    $4->id.id = $0->tmpid;
    $4->id.isderef = 0;

    /* this is retarded */
    A_NEW (CURMOD($0)->conn, conn_info_t *);
    A_NEXT (CURMOD($0)->conn) = $4;
    A_INC (CURMOD($0)->conn);

    len = array_length ($4);
    if (len > 0) {
      /* excitement, this is an array, make sure the id is in fact an
	 array */
      if (A_LEN ($0->tmpid->a) == 0) {
	A_NEW ($0->tmpid->a, struct array_idx);
	A_NEXT ($0->tmpid->a).lo = 0;
	A_NEXT ($0->tmpid->a).hi = len-1;
	A_INC ($0->tmpid->a);
      }
    }
    return $0->tmpid;
}};

one_port2: "." id
{{X:
    /* THIS .id should not be part of the type table */
    $0->tmpid = $2;
    if ($0->flag) {
      $0->tmpid = verilog_alloc_id (Strdup ($2->myname));
      /* delete from table! */
      verilog_delete_id ($0, $2->myname);
      FREE ($2);
    }
}}
"(" id_or_const_or_array ")" 
{{X:
    /* emit connection! */
    $4->prefix = $0->prefix;
    $4->id.id = $0->tmpid;
    $4->id.isderef = 0;
    A_NEW (CURMOD($0)->conn, conn_info_t *);
    A_NEXT (CURMOD($0)->conn) = $4;
    A_INC (CURMOD ($0)->conn);

    if ($0->prefix->p) {
      int k;
      /* check act process ports */

      k = $0->prefix->p->FindPort ($0->tmpid->myname);
      if (k != 0) {
	Assert (k > 0, "Hmm...");
	k--;
        $0->prefix->used[k] = 1;
      }
      else {
	$E("Connection to unknown port `%s'?", $0->tmpid->myname);
      }
    }
    else {
      int k;

      if ($0->prefix->m) {
	for (k=0; k < A_LEN ($0->prefix->m->port_list); k++) {
	  if (strcmp ($0->tmpid->myname,
		      $0->prefix->m->port_list[k]->myname) == 0) {
	    $0->prefix->used[k] = 1;
	    break;
	  }
	}
	if (k == A_LEN ($0->prefix->m->port_list)) {
	  $E("Connection to unknown port `%s'?", $0->tmpid->myname);
	}
      }
      else {
	/* punt */
	$0->prefix->mod = CURMOD ($0);
	if ($0->prefix->conn_start == -1) {
	  $0->prefix->conn_start = A_LEN (CURMOD ($0)->conn)-1;
	}
	$0->prefix->conn_end = A_LEN (CURMOD ($0)->conn)-1;
      }
    }
    return NULL;
}}
| "." id "(" ")" 
{{X:
    return NULL;
}};


bad_port_style: id_or_const_or_array
{{X:
    /* emit connection! */
    $1->prefix = $0->prefix;
    $1->id.id = NULL; /* to be filled later */
    $1->id.isderef = 0;
    A_NEW (CURMOD($0)->conn, conn_info_t *);
    A_NEXT (CURMOD($0)->conn) = $1;
    A_INC (CURMOD ($0)->conn);

    if ($0->prefix->p) {
      int k;
      /* check act process ports */
      k = $0->port_count++;
      if (k >= $0->prefix->p->getNumPorts()) {
	$E("Not enough ports in definition for `%s'?", $0->prefix->p->getName());
      }
      $1->id.id = verilog_find_id ($0, $0->prefix->p->getPortName (k));
      if ($1->id.id) {
	$0->flag = 0;
      }
      else {
	$0->flag = 1;
	$1->id.id = verilog_gen_id ($0, $0->prefix->p->getPortName (k));
      }
      $0->prefix->used[k] = 1;
    }
    else {
      int k = $0->port_count++;
      if ($0->prefix->m) {
	if (k >= A_LEN ($0->prefix->m->port_list)) {
	  $E("Not enough ports in definition for `%s'?",
	     $0->prefix->m->b->key);
	}
	$1->id.id = verilog_find_id ($0, $0->prefix->m->port_list[k]->myname);
	if ($1->id.id) {
	  $0->flag = 0;
	}
	else {
	  $0->flag = 1;
	  $1->id.id = verilog_gen_id ($0, $0->prefix->m->port_list[k]->myname);
	}
	$0->prefix->used[k] = 1;
      }
      else {
	/* punt */
	$1->id.cnt = $0->port_count++;
	$0->prefix->mod = CURMOD ($0);
	if ($0->prefix->conn_start == -1) {
	  $0->prefix->conn_start = A_LEN (CURMOD ($0)->conn)-1;
	}
	$0->prefix->conn_end = A_LEN (CURMOD ($0)->conn)-1;
      }
    }
    return NULL;
}}
;

id_deref_range_list[list_t *]: id_deref_range
{{X:
    return $1;
}}
| "{" { id_deref_or_range "," }* "}"
{{X:
    list_t *l;
    listitem_t *li;

    l = list_new ();
    for (li = list_first ($2); li; li = list_next (li)) {
      list_t *tmp = (list_t *)list_value (li);
      list_concat (l, tmp);
      list_free (tmp);
    }
    return l;
}}
| INT "'b" STRING
{{X:
    list_t *l;
    int pos = 1;

    l = list_new ();
    for (int i=0; i < $1; i++) {
      int digit;
      if (!$3[pos]) {
	$W("Binary constant doesn't have enough digits");
	digit = 0;
      }
      else {
	digit = $3[pos] == '0' ? 0 : 1;
	pos++;
      }

      id_deref_t *d;
      NEW (d, id_deref_t);
      d->isderef = 0;
      d->deref = 0;

      if (digit == 0) {
	d->id = verilog_gen_id ($0, "GND");
	d->id->isport = 1;
      }
      else {
	if (digit != 1) {
	  $W("Binary constant has non-binary digit (%d)", d);
	}
	d->id = verilog_gen_id ($0, "Vdd");
	d->id->isport = 1;
      }
      list_append (l, d);
    }
    return l;
}}
;

id_deref_or_range[list_t *]: id_deref_range
{{X:
    return $1;
}}
| id_deref
{{X:
    list_t *l;
    l = list_new ();
    list_append (l, $1);
    return l;
}}
;


id_deref_range[list_t *]: id "[" INT ":" INT "]"
{{X:
    list_t *l;
    int hi = $3;
    int lo = $5;

    l = list_new ();
    while (hi >= lo) {
      id_deref_t *d;
      NEW (d, id_deref_t);
      d->id = $1;
      d->isderef = 1;
      d->deref = hi;
      list_append (l, d);
      hi--;
    }
    return l;
}}
;

id_deref[id_deref_t *]: id [ "[" INT "]" ]
{{X:
    id_deref_t *d;
    VRet *r;
    
    NEW (d, id_deref_t);
    d->id = $1;
    if (OPT_EMPTY ($2)) {
      d->isderef = 0;
    }
    else {
      d->isderef = 1;
      r = OPT_VALUE ($2);
      $A(r->type == V_INT);
      d->deref = r->u.i;
      FREE (r);
      list_free ($2);
    }
    return d;
}}
| "1'b0"
{{X:
  id_deref_t *d;
  VRet *r;

  NEW (d, id_deref_t);
  d->id = verilog_gen_id ($0, "GND");
  d->id->isport = 1;
  d->isderef = 0;
  d->deref = 0;
  return d;
}}
| "1'b1"
{{X:
  id_deref_t *d;
  VRet *r;

  NEW (d, id_deref_t);
  d->id = verilog_gen_id ($0, "Vdd");
  d->id->isport = 1;
  d->isderef = 0;
  d->deref = 0;
  return d;
}}
;

id_or_const[conn_rhs_t *]: id_deref [ "[" INT ":" INT "]" ]
{{X:
    conn_rhs_t *r;
    VRet *v;

    NEW (r, conn_rhs_t);

    r->id = *($1);
    FREE ($1);
    if (OPT_EMPTY ($2)) {
      r->issubrange = 0;
    }
    else {
      r->issubrange = 1;
      v = LIST_VALUE (list_first ($2));
      $A(v->type == V_INT);
      r->hi = v->u.i;
      FREE (v);
      v = LIST_VALUE (list_next (list_first ($2)));
      $A(v->type == V_INT);
      r->lo = v->u.i;
      FREE (v);
      if (r->hi < r->lo) {
	int tmp;
	tmp = r->lo;
	r->lo = r->hi;
	r->hi = tmp;
      }
    }
    return r;
}}
| INT
{{X:
    id_info_t *id;
    conn_rhs_t *r;

    if ($1) {
      id = verilog_gen_id ($0, "Vdd");
      id->isport = 1;
    }
    else {
      id = verilog_gen_id ($0, "GND");
      id->isport = 1;
    }
    NEW (r, conn_rhs_t);
    r->id.id = id;
    r->id.isderef = 0;
    r->issubrange = 0;

    return r;
}}
;

id_or_const_or_array[conn_info_t *]: id_or_const
{{X:
    conn_info_t *c;

    NEW (c, conn_info_t);
    c->prefix = NULL;
    c->id.id = NULL;
    c->id.isderef = 0;
    c->l = NULL;
    c->r = $1;
    c->isclk = 0;
    return c;
}}
| "{" { id_or_const "," }* "}"
{{X:
    conn_info_t *c;

    NEW (c, conn_info_t);
    c->prefix = NULL;
    c->id.id = NULL;
    c->id.isderef = 0;
    c->r = NULL;
    c->l = $2;
    c->isclk = 0;
    return c;
}}
;
