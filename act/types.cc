/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2017-2019 Rajit Manohar
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
#include <stdio.h>
#include <act/act.h>
#include <act/types.h>
#include <act/iter.h>
#include <string.h>
#include <common/misc.h>
#include <common/hash.h>

const char *act_builtin_method_name[ACT_NUM_STD_METHODS] =
  { "set", "get", "send_rest", "recv_rest", "send_up", "recv_up",
    "send_init", "recv_init" };

const char *act_builtin_method_expr[ACT_NUM_EXPR_METHODS] =
  { "send_probe", "recv_probe" };

const int act_builtin_method_boolret[ACT_NUM_EXPR_METHODS] =  { 1, 1 };

/*------------------------------------------------------------------------
 *
 *  Parameter types (preal, pbool, pint, ptype)
 *
 *------------------------------------------------------------------------
 */
void Type::Init ()
{
  TypeFactory::Init();
}

const char *UserDef::getName()
{
  return name;
}

char *UserDef::getFullName()
{
  if (_ns == NULL || _ns == ActNamespace::Global()) {
    return Strdup (name);
  }
  else {
    char *tmp = _ns->Name(true);
    char *buf;
    int l = strlen (tmp) + strlen (name) + 1;
    MALLOC (buf, char, l);
    snprintf (buf, l, "%s%s", tmp + 2, name);
    FREE (tmp);
    return buf;
  }
}

void UserDef::printActName (FILE *fp)
{
  int i = 0;
  int in_param = 0;

  while (name[i]) {
    if (!in_param && name[i] != '<') {
      fputc (name[i], fp);
    }
    else if (in_param) {
      if (name[i] == 't' && name[i-1] == ',') {
	fprintf (fp, "true");
      }
      else if (name[i] == 'f' && name[i-1] == ',') {
	fprintf (fp, "false");
      }
      else {
	fputc (name[i], fp);
      }
    }
    else {
      /* name[i] == '<' */
      if (name[i+1] != '>') {
	fputc (name[i], fp);
	in_param = 1;
      }
      else {
	return;
      }
    }
    i++;
  }
}

void UserDef::snprintActName (char *buf, int sz)
{
  int i = 0;
  int in_param = 0;
  int pos = 0;

  buf[pos] = '\0';
  while (name[i]) {
    if (!in_param && name[i] != '<') {
      if (pos < sz-1) {
	buf[pos++] = name[i];
      }
    }
    else if (in_param) {
      if (name[i] == 't' && name[i-1] == ',') {
	snprintf (buf+pos, sz-pos-1, "true");
	pos += strlen (buf+pos);
      }
      else if (name[i] == 'f' && name[i-1] == ',') {
	snprintf (buf+pos, sz-pos-1, "false");
	pos += strlen (buf+pos);
      }
      else {
	if (pos < sz-1) {
	  buf[pos++] = name[i];
	}
      }
    }
    else {
      /* name[i] == '<' */
      if (name[i+1] != '>') {
	if (pos < sz-1) {
	  buf[pos++] = name[i];
	}
	in_param = 1;
      }
      else {
	buf[pos] = '\0';
	return;
      }
    }
    i++;
  }
  buf[pos] = '\0';
}

int UserDef::AddMetaParam (InstType *t, const char *id, AExpr *ae)
{
  int i;

  for (i=0; i < nt; i++) {
    if (strcmp (pn[i], id) == 0) {
      return 0;
    }
  }

  //printf ("ADD: %s to userdef: %p\n", id, this);

  if (!I->Add (id, t)) {
    return 0;
  }
  if (expanded) {
    ValueIdx *vx = I->LookupVal (id);
    Assert (vx, "What?");
    /* port parameters are immutable */
    vx->immutable = 1;
  }

  nt++;
  
  if (pn) {
    REALLOC (pn, const char *, nt);
  }
  else {
    NEW (pn, const char *);
  }
  pn[nt-1] = id;

  if (pt) {
    REALLOC (pt, InstType *, nt);
  }
  else {
    NEW (pt, InstType *);
  }
  pt[nt-1] = t;

  if (pdefault) {
    REALLOC (pdefault, AExpr *, nt);
  }
  else {
    NEW (pdefault, AExpr *);
  }
  pdefault[nt-1] = ae;

  return 1;
}



const char *UserDef::getPortName (int pos) const
{
  if (pos < 0) {
    pos = -pos;
    Assert (1 <= pos && pos < 1+getNumParams(), "Invalid pos!");
    return pn[pos-1];
  }
  else {
    Assert (0 <= pos && pos < getNumPorts(), "Invalid pos!");
    return port_n[pos];
  }
}

InstType *UserDef::getPortType (int pos) const
{
  if (pos < 0) {
    pos = -pos;
    Assert (1 <= pos && pos < 1+getNumParams(), "Invalid pos!");
    return pt[pos-1];
  }
  else {
    Assert (0 <= pos && pos < getNumPorts(), "Invalid pos!");
    return port_t[pos];
  }
}

void UserDef::refinePortType (int pos, InstType *u)
{
  Assert (pos >= 0, "Can't refine parameter types");
  Assert (pos < getNumPorts(), "Invalid pos!");
  port_t[pos] = port_t[pos]->refineBaseType (u);
}

int UserDef::AddPort (InstType *t, const char *id)
{
  int i;
  
  /*printf ("ADDport: %s to scope %x\n", id, I);*/

  for (i=0; i < nt; i++) {
    if (strcmp (pn[i], id) == 0) {
      return 0;
    }
  }
  for (i=0; i < nports; i++) {
    if (strcmp (port_n[i], id) == 0) {
      return 0;
    }
  }

  if (!I->Add (id, t)) {
    return 0;
  }

  nports++;

  if (!port_n) {
    NEW (port_n, const  char *);
  }
  else {
    REALLOC (port_n, const char *, nports);
  }
  if (!port_t) {
    NEW (port_t, InstType *);
  }
  else {
    REALLOC (port_t, InstType *, nports);
  }

  port_n[nports-1] = id;
  port_t[nports-1] = t;

  return 1;
}

int UserDef::FindPort (const char *id)
{
  int i;
  
  for (i=0; i < nt; i++) {
    if (strcmp (pn[i], id) == 0) {
      return -(i+1);
    }
  }
  for (i=0; i < nports; i++) {
    if (strcmp (port_n[i], id) == 0) {
      return (i+1);
    }
  }
  return 0;
}


UserDef::UserDef (ActNamespace *ns)
{
  level = ACT_MODEL_PRS;	/* default */
  parent = NULL;

  defined = 0;
  expanded = 0;
  pending = 0;

  nt = 0;
  pt = NULL;
  pn = NULL;
  pdefault = NULL;
  exported = 0;

  name = NULL;
  
  nports = 0;
  port_t = NULL;
  port_n = NULL;
  unexpanded = NULL;

  b = NULL;

  lang = new act_languages ();

  I = new Scope(ns->CurScope());
  I->setUserDef (this);
  _ns = ns;

  file = NULL;
  lineno = 0;

  has_refinement = 0;

  inherited_templ = 0;
  inherited_param = NULL;

  A_INIT (um);
}

UserDef::~UserDef()
{
  int i;
  if (pt) {
    for (i=0; i < nt; i++) {
      if (pt[i]) {
	delete pt[i];
      }
    }
    FREE (pt);
  }
  if (pn) {
    FREE (pn);
  }
  if (port_t) {
    for (i=0; i < nports; i++) {
      if (port_t[i]) {
	delete port_t[i];
      }
    }
    FREE (port_t);
  }
  if (port_n) {
    FREE (port_n);
  }
  if (I) {
    delete I;
  }
  if (b) {
    delete b;
  }

  if (A_LEN (um) > 0) {
    for (int i=0; i < A_LEN (um); i++) {
      Assert (um[i], "NULL macro?");
      delete um[i];
    }
    A_FREE (um);
  }

  if (inherited_param) {
    FREE (inherited_param);
  }

  if (lang) {
    delete lang;
  }
}

void UserDef::MkCopy (UserDef *u)
{
  parent = u->parent; u->parent = NULL;
  defined  = u->defined; u->defined = 0;
  expanded = u->expanded; u->expanded = 0;
  pending = u->pending; u->pending = 0;

  lang = u->lang;
  u->lang = new act_languages ();

  nt = u->nt; u->nt = 0;
  
  pt = u->pt; u->pt = NULL;
  pn = u->pn; u->pn = NULL;
  pdefault = u->pdefault; u->pdefault = NULL;

  exported = u->exported; u->exported = 0;

  nports = u->nports; u->nports = 0;
  port_t = u->port_t; u->port_t = NULL;
  port_n = u->port_n; u->port_n = NULL;

  I = u->I; u->I = NULL;
  I->setUserDef (this);

  name = u->name; u->name = NULL;
  b = u->b; u->b = NULL;
  _ns = u->_ns; u->_ns = NULL;

  unexpanded = u->unexpanded;
  level = u->level;

  file = u->file;
  lineno = u->lineno;
  has_refinement = u->has_refinement;

  inherited_templ = u->inherited_templ;
  inherited_param = u->inherited_param;
  u->inherited_param = NULL;
  u->inherited_templ = 0;

  A_ASSIGN (um, u->um);
  u->um = NULL;

  for (int i=0; i < A_LEN (um); i++) {
    if (um[i]->getFunction()) {
      um[i]->updateFn (this);
    }
  }
}


Function::Function (UserDef *u) : UserDef (u)
{
  b = NULL;
  ret_type = NULL;
  is_simple_inline = 0;
}

Function::~Function ()
{
  if (b) {
    delete b;
  }
}


Interface::Interface (UserDef *u) : UserDef (u)
{

}

Interface::~Interface() { }


void UserDef::SetParent (InstType *t)
{
  UserDef *x;
  parent = t;

  if (!t) return;

  x = dynamic_cast<UserDef *>(t->BaseType());
  if (x && x->getNumParams() > 0) {
    if (x->inherited_templ > 0 || t->getNumParams() > 0) {
      int i = 0;
      inst_param *ip = NULL;
      MALLOC (inherited_param, inst_param *, nt);

      for (int j=0; j < nt; j++) {
	inherited_param[j] = NULL;
      }

      if (x->inherited_templ > 0) {
	i = nt - x->getNumParams();
	for (int j=0; j < x->inherited_templ; j++) {
	  inherited_param[i+j] = x->inherited_param[j];
	  inherited_templ++;
	}
      }
      if (t->getNumParams() > 0) {
	ip = t->allParams ();
	i = nt - x->getNumParams();
	for (int j=0; j < t->getNumParams(); j++) {
	  while (inherited_param[i] && i < nt) {
	    i++;
	  }
	  if (i == nt) {
	    fatal_error ("Too many template parameters specified?");
	  }
	  inherited_param[i++] = &ip[j];
	  inherited_templ++;
	}
      }
    }
  }
}


int UserDef::isStrictPort (const  char *s)
{
  int i;

  /*printf ("mt = %d, nt = %d [%x]\n", mt, nt, this);*/

  for (i=0; i < nt; i++) {
    if (strcmp (s, pn[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int UserDef::isPort (const  char *s)
{
  int i;

  /*printf ("mt = %d, nt = %d [%x]\n", mt, nt, this);*/

  for (i=0; i < nports; i++) {
    if (strcmp (s, port_n[i]) == 0) {
      return 1;
    }
  }
  for (i=0; i < nt; i++) {
    if (strcmp (s, pn[i]) == 0) {
      return 1;
    }
  }
  return 0;
}


Data::Data (UserDef *u) : UserDef (u)
{
  int i;

  is_enum = 0;
  is_eint = 0;
  b = NULL;
  enum_vals = NULL;

  for (i=0; i < ACT_NUM_STD_METHODS; i++) {
    methods[i] = NULL;
  }
}

Data::~Data()
{
  if (enum_vals) {
    list_free (enum_vals);
  }
  enum_vals = NULL;
}

Channel::Channel (UserDef *u) : UserDef (u)
{
  int i;

  b = NULL;

  for (i=0; i < ACT_NUM_STD_METHODS; i++) {
    methods[i] = NULL;
  }
  for (i=0; i < ACT_NUM_EXPR_METHODS; i++) {
    emethods[i] = NULL;
  }
}

Channel::~Channel()
{
  
}


/*------------------------------------------------------------------------
 *  Compare two user-defined types
 *------------------------------------------------------------------------*/
int UserDef::isEqual (const UserDef *u) const
{
  int i;

  if (parent != u->parent) {
    if (!parent || !u->parent) return 0;
    if (!parent->isEqual (u->parent)) {
      return 0;
    }
  }
  if (nt != u->nt) return 0;
  if (nports != u->nports) return 0;

  if (exported != u->exported) return 0;
  if (expanded != u->expanded) return 0;

  for (i=0; i < nt; i++) {
    if (pn[i] != u->pn[i]) return 0;
    if (!pt[i]->isEqual (u->pt[i])) return 0;
    if (pdefault[i] && !u->pdefault[i] ||
	!pdefault[i] && u->pdefault[i]) return 0;
    if (pdefault[i]) {
      if (!pdefault[i]->isEqual (u->pdefault[i])) return 0;
    }
  }

  for (i=0; i < nports; i++) {
    if (port_n[i] != u->port_n[i]) return 0;
    if (!port_t[i]->isEqualDir (u->port_t[i])) return 0;
  }
  
  return 1;
}



  

static int recursion_depth = 0;

/*------------------------------------------------------------------------
 *
 *   Expand user-defined type! 
 *
 *------------------------------------------------------------------------
 */
UserDef *UserDef::Expand (ActNamespace *ns, Scope *s,
			  int spec_nt, inst_param *u,
			  int *cache_hit, int is_proc)
{
  UserDef *ux;
  int k, sz, len;
  InstType *x, *p;  
  Array *xa;

  recursion_depth++;

  if (recursion_depth >= Act::max_recurse_depth) {
    act_error_ctxt (stderr);
    fatal_error ("Exceeded maximum recursion depth of %d\n", Act::max_recurse_depth);
  }

  /* nt = # of specified parameters
     u = expanded instance paramters
  */
#if 0
  fprintf (stderr, "[In expand userdef] %s [parent: %d]\n", getName(),
	   inherited_templ);
  fprintf (stderr, "  nt=%d, spec_nt=%d  <", nt, spec_nt);
  for (int i=0; i < spec_nt; i++) {
    u[i].u.tp->Print (stderr);
    if (i != spec_nt-1) {
      fprintf (stderr, " , ");
    }
  }
  fprintf (stderr, ">\n");
#endif

  /* create a new userdef type */
  ux = new UserDef (_ns);
  ux->unexpanded = this;
  ux->file = file;
  ux->lineno = lineno;
  ux->has_refinement = has_refinement;

  if (defined) {
    ux->MkDefined();
  }

  /* set its scope to "expanded" mode */
  ux->I->FlushExpand();
  if (is_proc == 2) {
    ux->I->mkFunction();
  }
  /* set to pending */
  ux->pending = 1;
  ux->expanded = 1;

  InstType *instparent;
  UserDef *uparent;

  instparent = parent;
  if (instparent) {
    uparent = dynamic_cast <UserDef *> (instparent->BaseType());
  }
  else {
    uparent = NULL;
  }

  /* create bindings for type parameters */
  int i;
  int ii = 0;

  for (i=0; i < nt; i++) {
    p =  getPortType (-(i+1));
    Assert (p, "What?");
    x = p->Expand (ns, ux->I); // this is the real type of the
                               // parameter

    if (x->arrayInfo() && x->arrayInfo()->size() == 0) {
      act_error_ctxt (stderr);
      fatal_error ("Template parameter `%s': zero-length array creation not permitted", getPortName (-(i+1)));
    }

    /* add parameter to the scope */
    ux->AddMetaParam (x, pn[i], NULL);

    /* this one gets bound to:
       - the next specified parameter, if it exists
       - the next inherited parameter, if it exists
    */
    inst_param *bind_param = NULL;
    inst_param tmp_val;
    if (inherited_templ > 0 && inherited_param[i]) {
      bind_param = inherited_param[i];
    }
    else if (ii < spec_nt) {
      bind_param = &u[ii];
      ii++;
    }
    else {
      if (pdefault[i]) {
	tmp_val.isatype = 0;
	tmp_val.u.tp = pdefault[i]->Expand (ns, ux->I);
	bind_param = &tmp_val;
      }
      else {
	bind_param = NULL;
      }
    }

    if (!bind_param) continue;

    if (TypeFactory::isPTypeType (p->BaseType())) {
      if (!bind_param->isatype) {
	AExpr *rhsval = bind_param->u.tp;
	x = rhsval->isType();
      }
      else {
	x = bind_param->u.tt;
      }
      if (x) {
	x = x->Expand (ns, ux->I);
	ux->I->BindParam (pn[i], x);
      }
      else {
	act_error_ctxt (stderr);
	fprintf (stderr, "   Parameter: ");
	bind_param->u.tp->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Typechecking failed for parameter %d: expected type.", i);
      }
    }
    else  {
      InstType *rhstype;

      if (bind_param->isatype) {
	act_error_ctxt (stderr);
	fprintf (stderr, "   Parameter: ");
	bind_param->u.tt->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Typechecking failed for parameter %d: expected value.", i);
      }

      AExpr *rhsval = bind_param->u.tp;
      if (!rhsval->isArrayExpr()) {
	rhsval = rhsval->Expand (ns, ux->I);
      }
      rhstype = rhsval->getInstType (ux->I, NULL, 1);
      if (type_connectivity_check (x, rhstype) != 1) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Typechecking failed, ");
	x->Print (stderr);
	fprintf (stderr, "  v/s ");
	rhstype->Print (stderr);
	fprintf (stderr, "\n\t%s\n", act_type_errmsg());
	exit (1);
      }
#if 0	
      printf ("    <> bind %s := ", pn[i]);
      rhsval->Print (stdout);
      printf ("\n");
#endif	
      ux->I->BindParam (pn[i], rhsval);
      delete rhstype;
      if (!rhsval->isArrayExpr()) {
	delete rhsval;
      }
    }
  }

  int parent_start;
  if (parent && TypeFactory::isUserType (parent)) {
    parent_start = (nt - (dynamic_cast<UserDef *> (parent->BaseType()))->getNumParams());
    if (parent_start >= spec_nt) {
      parent_start = -1;
    }
  }
  else {
    parent_start = -1;
  }
  
  /*
     create a name for it!
     (old name)"<"string-from-types">"

     create a fresh scope

     expand out the body of the type

     insert the new type into the namespace into the xt table
  */

  sz = 0;

  /* type< , , , ... , > + end-of-string */
  /* for arrays, originally we had {a,b,c}... the expanded values.
     instead, we now have scope#idx
  */
  sz = strlen (getName()) + 3 + nt;

  for (int i=0; i < nt; i++) {
    InstType *x;
    Array *xa;
    ValueIdx *vx;

    x = ux->getPortType (-(i+1));
    vx = ux->I->LookupVal (pn[i]);
    xa = x->arrayInfo();
    if (TypeFactory::isPTypeType (x->BaseType())) {
      if (xa) {
	act_error_ctxt (stderr);
	fatal_error ("ptype array parameters not supported");
      }

      if (i < spec_nt) {
	if (u[i].isatype) {
	  x = u[i].u.tt;
	}
	else {
	  x = u[i].u.tp->isType();
	  if (!x) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Typechecking failed for param #%d ", i);
	    u[i].u.tp->Print (stderr);
	    exit (1);
	  }
	}
      }
      else {
	x = NULL;
      }
      /* x is now the value of the parameter */
      if (x) {
	sz += strlen (x->BaseType()->getName()) + 3;
      }
      /* might have directions, upto 2 characters worth, @ for type */
    }
    else {
      /* check array info */
      if (vx->init) {
	if (xa) {
	  Assert (xa->isExpanded(), "Array info is not expanded");
	  sz += 32*xa->size()+2;
	}
	else {
	  sz += 32;
	}
      }
    }
  }

  char *buf;
  MALLOC (buf, char, sz);
  k = 0;
  buf[k] = '\0';
  snprintf (buf+k, sz, "%s<", getName());
  len = strlen (buf+k);
  k += len;
  sz -= len;

  ii=0;

  for (int i=0; i < nt; i++) {
    ValueIdx *vx;

    if (inherited_templ > 0 && inherited_param[i]) {
      continue;
    }
    if (ii == spec_nt) {
      break;
    }

    if (ii != 0) {
      snprintf (buf+k, sz, ",");
      len = strlen (buf+k); k += len; sz -= len;
    }
    Assert (sz > 0, "Check");
    x = ux->getPortType (-(i+1));
    vx = ux->I->LookupVal (pn[i]);
    xa = x->arrayInfo();
    if (TypeFactory::isPTypeType (x->BaseType())) {
      if (ii < spec_nt) {
	if (u[ii].isatype) {
	  x = u[ii].u.tt;
	}
	else {
	  x = u[ii].u.tp->isType();
	}	  
      }
      else {
	x = NULL;
      }
      if (x) {
	snprintf (buf+k, sz, "@%s%s", x->BaseType()->getName(),
		  Type::dirstring (x->getDir()));
	len = strlen (buf+k); k += len; sz -= len;
      }
    }
    else {
      if (vx->init) {
	if (xa) {
	  Arraystep *as;
	  
	  snprintf (buf+k, sz, "{");
	  k++; sz--;

	  as = xa->stepper();
	  while (!as->isend()) {
	    if (TypeFactory::isPIntType (x->BaseType())) {
	      snprintf (buf+k, sz, "%lu",
			ux->I->getPInt (vx->u.idx + as->index()));
	    }
	    else if (TypeFactory::isPIntsType (x->BaseType())) {
	      snprintf (buf+k, sz, "%ld",
			ux->I->getPInts (vx->u.idx + as->index()));
	    }
	    else if (TypeFactory::isPRealType (x->BaseType())) {
	      snprintf (buf+k, sz, "%g",
			ux->I->getPReal (vx->u.idx + as->index()));
	    }
	    else if (TypeFactory::isPBoolType (x->BaseType())) {
	      snprintf (buf+k, sz, "%c",
			ux->I->getPBool (vx->u.idx + as->index()) ? 't' : 'f');
	    }
	    else {
	      fatal_error ("What type is this?");
	    }
	    len = strlen (buf+k); k+= len; sz-= len;
	    as->step();
	    if (!as->isend()) {
	      snprintf (buf+k, sz, ",");
	      k++; sz--;
	    }
	  }
	  delete as;
	  snprintf (buf+k, sz, "}");
	  k++; sz--;
	}
	else {
	  if (TypeFactory::isPIntType (x->BaseType())) {
	    snprintf (buf+k, sz, "%lu", ux->I->getPInt (vx->u.idx));
	  }
	  else if (TypeFactory::isPIntsType (x->BaseType())) {
	    snprintf (buf+k, sz, "%ld", ux->I->getPInts (vx->u.idx));
	  }
	    else if (TypeFactory::isPRealType (x->BaseType())) {
	      snprintf (buf+k, sz, "%g", ux->I->getPReal (vx->u.idx));
	    }
	    else if (TypeFactory::isPBoolType (x->BaseType())) {
	      snprintf (buf+k, sz, "%c", ux->I->getPBool (vx->u.idx) ? 't' : 'f');
	    }
	    else {
	      fatal_error ("What type is this?");
	    }
	    len = strlen (buf+k); k+= len; sz-= len;
	}
      }
    }
    ii++;
  }
  snprintf (buf+k, sz, ">");
  k++; sz--;

  Assert (sz >= 0, "Hmmmmm");

  /* now we have the string for the type! */
  UserDef *uy;

  uy = _ns->findType (buf);

  if (uy) {
    if (uy->pending) {
      act_error_ctxt (stderr);
      fatal_error ("Recursive construction of type `%s'", buf);
    }
    FREE (buf);
    /* we found one! */
    delete ux;
    recursion_depth--;
    *cache_hit = 1;
    return uy;
  }
  *cache_hit = 0;

  Assert (_ns->CreateType (buf, ux), "Huh");
  FREE (buf);

  if (parent) {
    uparent = dynamic_cast <UserDef *> (parent->BaseType());
    if (uparent) {
      if (parent_start != -1) {
	InstType *parent2 = new InstType (parent);
	parent2->appendParams (spec_nt - parent_start, u+parent_start);
	ux->SetParent (parent2->Expand (ns, ux->CurScope()));
	delete parent2;
      }
      else {
	ux->SetParent (parent->Expand (ns, ux->CurScope()));
      }
    }
    else {
      ux->SetParent (parent->Expand (ns, ux->CurScope()));
    }
  }
  if (ux->parent) {
    ux->parent->MkCached ();
  }
  ux->exported = exported;

  /*-- create ports --*/
  for (int i=0; i < nports; i++) {
    InstType *chk;
    Assert (ux->AddPort ((chk = getPortType(i)->Expand (ns, ux->I)),
			 getPortName (i)), "What?");

    if (chk->arrayInfo() && chk->arrayInfo()->size() == 0) {
      act_error_ctxt (stderr);
      fatal_error ("Port `%s': zero-length array creation not permitted",
		   getPortName (i));
    }
    
    ActId *tmp = new ActId (getPortName (i));
    tmp->Canonical (ux->CurScope());
    delete tmp;
  }

  /*-- XXX: create this, if necessary --*/
  if (dynamic_cast<Channel *>(this)) {
    InstType *x;
    Assert (parent, "Hmm...");
    x = ux->root();

    Chan *ch = dynamic_cast <Chan *>(x->BaseType());
    Assert (ch, "Hmm?!");
    Assert (ch->datatype(), "What?");
    ux->CurScope()->Add ("self", ch->datatype());

    if (ch->acktype()) {
      ux->CurScope()->Add ("selfack", ch->acktype());
    }
  }
  else if (dynamic_cast<Data *>(this)) {
    InstType *x;

    if (TypeFactory::isStructure (this) || TypeFactory::isUserEnum (this)) {
      /* no self for structures */
    }
    else {
      Assert (parent, "Hmm...");
      x = ux->root();
      ux->CurScope()->Add ("self", x);
    }
  }
  else if (dynamic_cast<Function *>(this)) {
    InstType *x = dynamic_cast<Function *>(this)->getRetType();
    x = x->Expand (ns, ux->I);
    ux->CurScope()->Add ("self", x);
  }
  else {
    /* process, no this pointer */
  }

  /*-- expand body --*/

  /* handle any refinement overrides! */
  if (ux && (ux->getRefineList() != NULL) &&
      ActNamespace::Act()->getRefSteps() >=
      list_ivalue (list_first (ux->getRefineList()))) {
    // find any refinement overrides that might apply, and apply them!
    Assert (b, "What?");
    ux->_apply_ref_overrides (b, b);
  }
  
  if (b) {
    b->Expandlist (ns, ux->I);
  }

  /*-- expand macros --*/
  for (int i=0; i < A_LEN (um); i++) {
    A_NEW (ux->um, UserMacro *);
    A_NEXT (ux->um) = um[i]->Expand (ux, ns, ux->I, (is_proc == 1) ? 1 : 0);
    A_INC (ux->um);
  }

  ux->pending = 0;
  recursion_depth--;
  return ux;
}



Data *Data::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Data *xd;
  UserDef *ux;
  int cache_hit;
  int i;

  if (isExpanded() && nt == 0) {
    return this;
  }
  
  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Data *>(ux);
  }

  xd = new Data (ux);
  delete ux;

  Assert (_ns->EditType (xd->name, xd) == 1, "What?");
  xd->is_enum = is_enum;
  xd->is_eint = is_eint;

  for (i=0; i < ACT_NUM_STD_METHODS; i++) {
    xd->methods[i] = chp_expand (methods[i], ns, xd->CurScope());
  }
  if (enum_vals) {
    xd->enum_vals = list_dup (enum_vals);
  }

  if (TypeFactory::isPureStruct (xd)) {
    for (int i=0; i < A_LEN (xd->um); i++) {
      xd->um[i]->setParent (xd);
      if (xd->um[i]->isBuiltinMacro()) {
	xd->um[i]->populateCHP ();
      }
    }
  }
  
  return xd;
}


Channel *Channel::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Channel *xc;
  UserDef *ux;
  int cache_hit;
  int i;

  if (isExpanded() && nt == 0) {
    return this;
  }
  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Channel *>(ux);
  }

  xc = new Channel (ux);
  delete ux;

  Assert (_ns->EditType (xc->name, xc) == 1, "What?");

  for (i=0; i < ACT_NUM_STD_METHODS; i++) {
    xc->methods[i] = chp_expand (methods[i], ns, xc->CurScope());
  }
  for (i=0; i < ACT_NUM_EXPR_METHODS; i++) {
    xc->emethods[i] = expr_expand (emethods[i], ns, xc->CurScope(), 0);
  }

  return xc;
}


Function *Function::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Function *xd;
  UserDef *ux;
  int cache_hit;

  if (isExpanded() && nt == 0) {
    return this;
  }

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit, 2);
  ux->CurScope()->mkFunction();

  if (cache_hit) {
    return dynamic_cast<Function *>(ux);
  }

  xd = new Function (ux);
  delete ux;

  Assert (_ns->EditType (xd->name, xd) == 1, "What?");

  if (!ret_type->isExpanded()) {
    xd->setRetType (ret_type->Expand (ns, xd->I));
  }
  else {
    xd->setRetType (ret_type);
  }
  
  xd->chkInline();
  
  return xd;
}

Interface *Interface::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Interface *xd;
  UserDef *ux;
  int cache_hit;

  if (isExpanded() && nt == 0) {
    return this;
  }

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Interface *>(ux);
  }

  xd = new Interface (ux);
  delete ux;

  Assert (_ns->EditType (xd->name, xd) == 1, "What?");
  return xd;
}

PType *PType::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Assert (nt == 1, "What?");

  return TypeFactory::Factory()->NewPType (u[0].u.tt->Expand (ns, s));
}

const char *PType::getName ()
{
  if (!i) {
    return "ptype";
  }
  else if (!name) {
    char buf[10240];
    snprintf (buf, 10240, "ptype(");
    i->sPrint (buf+strlen (buf), 10240-strlen (buf));
    snprintf (buf+strlen(buf), 10240-strlen (buf), ")");
    name = Strdup (buf);
  }
  return name;
}



Int *Int::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Int *ix;
  AExpr *ae;
  InstType *it;
  
  Assert (nt == 1, "What?");

  if (u[0].u.tp) {
    ae = u[0].u.tp->Expand (ns, s);
  }
  else {
    ae = new AExpr (const_expr (32));
  }
  it = ae->getInstType (s, NULL);
  if (!TypeFactory::isPIntType (it->BaseType()) ||
      it->arrayInfo()) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Expanding integer type; parameter is not an integer\n");
    fprintf (stderr, " parameter: ");
    ae->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }
  delete it;

  AExprstep *step = ae->stepper();

  ix = TypeFactory::Factory()->NewInt (kind, step->getPInt());

  step->step();
  Assert (step->isend(), "Hmm?");

  delete step;
  delete ae;

  return ix;
}

const char *Int::getName()
{
  const char *k2v;

  if (name) return name;
  
  if (kind == 0) {
    k2v = "int";
  }
  else if (kind == 1) {
    k2v = "ints";
  }
  else {
    k2v = "enum";
  }
  
  if (w == -1) {
    name = k2v;
  }
  else {
    char buf[1024];
    snprintf (buf, 1024, "%s<%d>", k2v, w);
    name = Strdup (buf);
  }
  return name;
}

const char *Chan::getName ()
{
  char buf[10240];
  if (name) return name;

  if (!p) {
    snprintf (buf, 10240, "chan");
  }
  else {
    snprintf (buf, 10240, "chan(");
    p->sPrint (buf+strlen (buf), 10239-strlen(buf));
    if (ack) {
      strcat (buf, ",");
      ack->sPrint (buf + strlen (buf), 10239 - strlen (buf));
    }
    strcat (buf, ")");
  }
  name = Strdup (buf);
  return name;
}


Chan *Chan::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Chan *cx;
  InstType *cp, *ack;

  Assert (nt == 1 || nt == 2, "Hmm");
  cp = u[0].u.tt->Expand (ns, s);
  if (TypeFactory::isParamType (cp)) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Parameter to channel type is not a datatype\n");
    fprintf (stderr, " parameter: ");
    cp->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }
  ack = NULL;
  if (nt == 2) {
    ack = u[1].u.tt->Expand (ns, s);
    if (TypeFactory::isParamType (ack)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Parameter to channel type is not a datatype\n");
      fprintf (stderr, " parameter: ");
      cp->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  cx = TypeFactory::Factory()->NewChan (cp, ack);
  if (cx->p != cp) {
    /*-- got a cached version --*/
    delete cp;
    if (ack) {
      delete ack;
    }
  }
  return cx;
}


void UserDef::PrintHeader (FILE *fp, const char *type, bool unmangle)
{
  char buf[10240];
  int n;

  if (exported) {
    fprintf (fp, "export ");
  }
  n = getNumParams();
  if (!expanded && n > 0) {
    fprintf (fp, "template <");
    for (int i=0; i < n; i++) {
      InstType *it = getPortType (-(i+1));
      Array *a = it->arrayInfo();
      it->clrArray();
      it->Print (fp);
      fprintf (fp, " %s", getPortName (-(i+1)));
      if (a) {
	a->Print (fp);
      }
      it->MkArray (a);

      AExpr *ae = getDefaultParam (i);
      if (ae) {
	fprintf (fp, " = ");
	ae->Print (fp);
      }

      if (i != n-1) {
	fprintf (fp, "; ");
      }
    }
    fprintf (fp, ">\n");
  }
  fprintf (fp, "%s ", type);
  if (expanded) {
    ActNamespace::Act()->mfprintfproc (fp, this, 1);
    fprintf (fp, " ");
  }
  else {
    /* ok there is a possibility of a name conflict here but lets not
       mangle more stuff */
    fprintf (fp, "%s ", getName());
  }

  int skip_ports = 0;

  if (parent) {
    fprintf (fp, "<: ");
    if (!expanded || !TypeFactory::isUserType (parent)) {
      parent->Print (fp);
    }
    else {
      UserDef *u = dynamic_cast<UserDef *>(parent->BaseType());
      Assert (u, "what?");
      if (unmangle) {
        u->snprintActName (buf, 10240);
        fprintf (fp, "%s", buf);
      }
      else {
        ActNamespace::Act()->mfprintfproc (fp, u);
      }
      skip_ports = u->getNumPorts();
    }
    fprintf (fp, " ");
  }

  n = getNumPorts ();
  fprintf (fp, "(");
  if (skip_ports < n) {
    for (int i=skip_ports; i < n; i++) {
      InstType *it = getPortType (i);
      Array *a = it->arrayInfo();
      it->clrArray ();
      if (it->isExpanded() && TypeFactory::isUserType (it)) {
	UserDef *tmpu = dynamic_cast<UserDef *> (it->BaseType());
	ActNamespace *ns;
	char *ns_name = NULL;
	ns = tmpu->getns();
	Assert (ns, "Hmm");
	if (ns && ns != ActNamespace::Global() && ns != getns()) {
	  ns_name = ns->Name();
	}
	if (ns_name) {
	  fprintf (fp, "%s::", ns_name);
	  FREE (ns_name);
	}
	ActNamespace::Act()->mfprintfproc (fp, tmpu, 1);
      }
      else {
	it->Print (fp, 1);
      }
      fprintf (fp, " %s", getPortName (i));
      if (a) {
	a->Print (fp);
      }
      it->MkArray (a);
      if (i != n-1) {
	fprintf (fp, "; ");
      }
    }
  }
  fprintf (fp, ")");
}


/*------------------------------------------------------------------------
 *
 *  Returns first built-in type in the parent type hierarchy.
 *
 *     THIS SHOULD ONLY BE CALLED FOR user-defined channels and data
 *     types. 
 *
 *------------------------------------------------------------------------
 */
InstType *UserDef::root () const
{
  if (!parent) {
    return NULL;
  }
  if (TypeFactory::isUserType (parent)) {
    UserDef *ux = dynamic_cast<UserDef *> (parent->BaseType());
    return ux->root();
  }
  return parent;
}


/*------------------------------------------------------------------------
 *  Printing functions
 *------------------------------------------------------------------------
 */

void Channel::Print (FILE *fp)
{
  PrintHeader (fp, "defchan");
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }

  lang->Print (fp);

  int firstmeth = 1;

#define EMIT_METHOD_HEADER(id)			\
  do {						\
    if (methods[id]) {				\
      if (firstmeth) {				\
	fprintf (fp, "  methods {\n");		\
      }						\
      firstmeth = 0;				\
    }						\
  } while (0)

#define EMIT_METHOD_EXPRHEADER(id)		\
  do {						\
    if (emethods[id-ACT_NUM_STD_METHODS]) {	\
      if (firstmeth) {				\
	fprintf (fp, "  methods {\n");		\
      }						\
      firstmeth = 0;				\
    }						\
  } while (0)

#define EMIT_METHOD(id)						\
  do {								\
    EMIT_METHOD_HEADER(id);					\
    if (methods[id]) {						\
      fprintf (fp, "  %s {\n", act_builtin_method_name[id]);	\
      chp_print (fp, methods[id]);				\
      fprintf (fp, "}\n");					\
    }								\
  } while (0)

#define EMIT_METHODEXPR(id)						\
  do {									\
    EMIT_METHOD_EXPRHEADER(id);						\
    if (emethods[id-ACT_NUM_STD_METHODS]) {				\
      fprintf (fp, "  %s =", act_builtin_method_expr[id-ACT_NUM_STD_METHODS]); \
      print_expr (fp, emethods[id-ACT_NUM_STD_METHODS]);		\
      fprintf (fp, ";\n");						\
    }									\
  } while (0)

  for (int i=0; i < ACT_NUM_STD_METHODS; i++) {
    EMIT_METHOD (i);
  }
  for (int i=0; i < ACT_NUM_EXPR_METHODS; i++) {
    EMIT_METHODEXPR (i + ACT_NUM_STD_METHODS);
  }

  firstmeth = emitMacros (fp) ? 1 : firstmeth;
  
  if (!firstmeth) {
    fprintf (fp, "}\n");
  }
  fprintf (fp, "}\n\n");
}

void Data::Print (FILE *fp)
{
  if (isEnum()) {
    fprintf (fp, "defenum ");
    ActNamespace::Act()->mfprintfproc (fp, this, 1);
    if (!isPureEnum() || expanded) {
      fprintf (fp, " : int");
    }
    fprintf (fp, " {\n");
    for (listitem_t *li = list_first (enum_vals); li; li = list_next (li)) {
      if (li != list_first (enum_vals)) {
	fprintf (fp, ", ");
      }
      else {
	fprintf (fp, "   ");
      }
      fprintf (fp, "%s", (char *) list_value (li));
    }
    fprintf (fp, "\n};\n");
    return;
  }
  
  PrintHeader (fp, "deftype");
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }
  
  lang->Print (fp);

  int firstmeth = 1;

  EMIT_METHOD(ACT_METHOD_SET);
  EMIT_METHOD(ACT_METHOD_GET);

  firstmeth = emitMacros (fp) ? 1 : firstmeth;

  if (!firstmeth) {
    fprintf (fp, " }\n");
  }
  fprintf (fp, "}\n\n");
}

void Function::Print (FILE *fp)
{
  PrintHeader (fp, "function");
  fprintf (fp, " : ");
  if (expanded) {
    if (TypeFactory::isUserType (getRetType())) {
      UserDef *u = dynamic_cast<UserDef *> (getRetType()->BaseType());
      ActNamespace::Act()->mfprintfproc (fp, u, 1);
    }
    else {
      getRetType()->Print (fp);
    }
  }
  else {
    char *ns_name = NULL;
    if (TypeFactory::isUserType (getRetType())) {
      UserDef *u = dynamic_cast<UserDef *> (getRetType()->BaseType());
      ActNamespace *ns = u->getns();
      if (ns && ns != ActNamespace::Global() && ns != getns()) {
	ns_name = ns->Name();
      }
    }
    if (ns_name) {
      fprintf (fp, "%s::", ns_name);
      FREE (ns_name);
    }
    getRetType()->Print (fp);
  }
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }

  /* print language bodies */
  lang->Print (fp);

  
  fprintf (fp, "}\n\n");
}


void UserDef::AppendBody (ActBody *x)
{
  if (b) {
    b->Append (x);
  }
  else {
    b = x;
  }
}


void Data::copyMethods (Data *d)
{
  for (int i=0; i < ACT_NUM_STD_METHODS; i++) {
    methods[i] = d->getMethod ((datatype_methods)i);
  }
}

void Channel::copyMethods (Channel *c)
{
  for (int i=0; i < ACT_NUM_STD_METHODS; i++) {
    methods[i] = c->getMethod ((datatype_methods)i);
  }
  for (int i=0; i < ACT_NUM_EXPR_METHODS; i++) {
    emethods[i] = c->geteMethod ((datatype_methods)(i+ACT_NUM_STD_METHODS));
  }
}


act_prs *UserDef::getprs ()
{
  return lang->getprs();
}

act_spec *UserDef::getspec ()
{
  return lang->getspec();
}

int UserDef::isEqual (const Type *t) const
{
  const UserDef *u = dynamic_cast<const UserDef *>(t);
  if (!u) return 0;
  if (u == this) return 1;
  return 0;
}

int Int::isEqual (const Type *t) const
{
  const Int *x = dynamic_cast<const Int *>(t);
  if (!x) return 0;
  if (x->kind == kind && x->w == w) return 1;
  return 0;
}
  
int Chan::isEqual (const Type *t) const
{
  const Chan *x = dynamic_cast<const Chan *>(t);
  if (!x) return 0;
  if (x == this) return 1;
  if (!p && !x->p) return 1;
  if (!p || !x->p) return 0;

  if (p->isExpanded()) {
    if (!p->isEqual (x->p)) return 0;
  }
  else {
    if (!p->isEqual (x->p, 1)) return 0;
  }

  if (!ack && !x->ack) return 1;
  if (!ack || !x->ack) return 0;

  if (ack->isExpanded()) {
    if (!ack->isEqual (x->ack)) return 0;
  }
  else {
    if (!ack->isEqual (x->ack, 1)) return 0;
  }
  return 1;
}

int Data::isEqual (const Type *t) const
{
  const Data *x = dynamic_cast<const Data *>(t);
  if (!x) {
    return 0;
  }
  if (x->isEnum() != isEnum()) {
    return 0;
  }
  if (x->isPureEnum() != isPureEnum()) {
    return 0;
  }
  if (!x->isEnum()) {
    return UserDef::isEqual (t);
  }
  Assert (x->enum_vals, "What?");
  Assert (enum_vals, "What!");
  if (list_length (x->enum_vals) != list_length (enum_vals)) {
    return 0;
  }
  listitem_t *li, *mi;
  li = list_first (enum_vals);
  mi = list_first (x->enum_vals);
  while (li && mi) {
    char *s = (char *) list_value (li);
    char *t = (char *) list_value (mi);
    if (strcmp (s, t) != 0) {
      return 0;
    }
    li = list_next (li);
    mi = list_next (mi);
  }
  return 1;
}

int UserDef::isLeaf ()
{
  ActInstiter i(I);

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = (*i);
    if (TypeFactory::isProcessType (vx->t)) {
      return 0;
    }
  }
  return 1;
}

void Data::getStructCount (int *nbools, int *nints)
{
  if (!TypeFactory::isStructure (this) || !isExpanded()) {
    *nbools = -1;
    *nints = -1;
    return;
  }
  *nbools = 0;
  *nints = 0;
  _get_struct_count (nbools, nints);
}

void Data::_get_struct_count (int *nb, int *ni)
{
  for (int i=0; i < getNumPorts(); i++ ) {
    InstType *it = getPortType (i);
    int sz;
    if (it->arrayInfo()) {
      sz = it->arrayInfo()->size();
    }
    else {
      sz = 1;
    }
    if (TypeFactory::isIntType (it)) {
      *ni += sz;
    }
    else if (TypeFactory::isBoolType (it)) {
      *nb += sz;
    }
    else if (TypeFactory::isStructure (it)) {
      int tmpb, tmpi;
      Data *d = dynamic_cast<Data *>(it->BaseType());
      Assert (d, "Hmm");
      d->getStructCount (&tmpb, &tmpi);
      *nb += sz*tmpb;
      *ni += sz*tmpi;
    }
    else {
      Data *d = dynamic_cast<Data *> (it->BaseType());
      Assert (d, "Hmm");
      if (TypeFactory::isIntType (d->root())) {
	*ni += sz;
      }
      else if (TypeFactory::isBoolType (d->root())) {
	*nb += sz;
      }
      else {
	Assert (0, "structure/data invariant violated!");
      }
    }
  }
}

int Data::getStructOffset (ActId *field, int *sz, InstType **rit)
{
  int loc_offset = 0;
  Assert (TypeFactory::isStructure (this), "What?");
  if (!field) return -1;
  
  for (int i=0; i < getNumPorts(); i++) {
    InstType *it = getPortType (i);
    int field_sz = 1;

    /* compute the size of the field in terms of # of values */
    if (TypeFactory::isStructure (it)) {
      Data *xd = dynamic_cast<Data *> (it->BaseType());
      int nb, ni;
      xd->getStructCount (&nb, &ni);
      field_sz = nb + ni;
    }

    if (strcmp (field->getName(), getPortName (i)) == 0) {
      /* a partial match */
      if (!field->Rest()) {
	/* a complete match */
        if (field->arrayInfo()) {
	  /* an array index */
	  Assert (it->arrayInfo(), "What?");
	  loc_offset += it->arrayInfo()->Offset (field->arrayInfo()) * field_sz;
	}
	if (sz) {
	  *sz = field_sz;
	  if (it->arrayInfo() && !field->arrayInfo()) {
	    *sz = *sz * it->arrayInfo()->size();
	  }
	}
	if (rit) {
	  *rit = it;
	}
	return loc_offset;
      }
      else {
	Data *d;
	int x;
	Assert (TypeFactory::isStructure (it), "What?");

	if (field->arrayInfo()) {
	  Assert (it->arrayInfo(), "What?");
	  loc_offset += it->arrayInfo()->Offset (field->arrayInfo()) * field_sz;
	}
	d = dynamic_cast <Data *> (it->BaseType());
	x = d->getStructOffset (field->Rest(), sz, rit);
	if (x == -1) {
	  return -1;
	}
	return loc_offset + x;
      }
    }

    if (it->arrayInfo()) {
      loc_offset += field_sz * it->arrayInfo()->size();
    }
    else {
      loc_offset += field_sz;
    }
  }
  return -1;
}

ActId **Data::getStructFields (int **types)
{
  Assert (TypeFactory::isStructure (this), "What?");
  int nb, ni;
  getStructCount (&nb, &ni);

  ActId **ret;
  int idx;
  MALLOC (ret, ActId *, nb + ni);
  MALLOC (*types, int, nb + ni);

  idx = 0;

  _get_struct_fields (ret, *types, &idx, NULL);
  Assert (idx == nb + ni, "What?");

  return ret;
}

void Data::_get_struct_fields (ActId **a, int *types, int *pos, ActId *prefix)
{
  Assert (TypeFactory::isStructure (this), "What?!");

  for (int i=0; i < getNumPorts(); i++) {
    InstType *it = getPortType (i);
    Array *arr;
    Arraystep *as = NULL;

    if (it->arrayInfo()) {
      as = it->arrayInfo()->stepper();
    }

    while (!as || !as->isend()) {
      if (!as) {
	arr = NULL;
      }
      else {
	arr = as->toArray();
      }
      if (TypeFactory::isStructure (it)) {
	Data *d = dynamic_cast<Data *> (it->BaseType());
	ActId *tmp = new ActId (getPortName (i), arr);
	if (prefix) {
	  ActId *tl;
	  tl = prefix->Tail();
	  tl->Append (tmp);
	  d->_get_struct_fields (a, types, pos, prefix);
	  tl->prune();
	}
	else {
	  d->_get_struct_fields (a, types, pos, tmp);
	}
	delete tmp;
      }
      else {
	ActId *tmp = new ActId (getPortName (i), arr);
	if (prefix) {
	  a[*pos] = prefix->Clone();
	  a[*pos]->Tail()->Append (tmp);
	}
	else {
	  a[*pos] = tmp;
	}
	if (TypeFactory::isBoolType (getPortType (i))) {
	  types[*pos] = 0;
	}
	else {
	  Assert (TypeFactory::isIntType (getPortType (i)), "Structure invariant violated?");
	  types[*pos] = 1;
	}
	*pos = *pos + 1;
      }
      if (as) {
	as->step();
      }
      else {
	break;
      }
    }
  }
}


int Data::getStructOffsetPair (ActId *field, int *boff, int *ioff)
{
  int i_offset = 0;
  int b_offset = 0;
  Assert (TypeFactory::isStructure (this), "What?");
  if (!field) return 0;
  
  for (int i=0; i < getNumPorts(); i++) {
    InstType *it = getPortType (i);
    int field_sz_b, field_sz_i;
    int cnt;

    /* compute the size of the field in terms of # of values */
    if (TypeFactory::isStructure (it)) {
      Data *xd = dynamic_cast<Data *> (it->BaseType());
      xd->getStructCount (&field_sz_b, &field_sz_i);
    }
    else if (TypeFactory::isBaseBoolType (it)) {
      field_sz_b = 1;
      field_sz_i = 0;
    }
    else {
      field_sz_i = 1;
      field_sz_b = 0;
    }

    if (strcmp (field->getName(), getPortName (i)) == 0) {
      /* a partial match */
      if (!field->Rest()) {
	/* a complete match */
        if (field->arrayInfo()) {
	  /* an array index */
	  Assert (it->arrayInfo(), "What?");
	  cnt = it->arrayInfo()->Offset (field->arrayInfo());
	}
	else {
	  cnt = 0;
	}
	i_offset += cnt  * field_sz_i;
	b_offset += cnt * field_sz_b;
	*boff = b_offset;
	*ioff = i_offset;
	return 1;
      }
      else {
	Data *d;
	int x;
	Assert (TypeFactory::isStructure (it), "What?");

	if (field->arrayInfo()) {
	  Assert (it->arrayInfo(), "What?");
	  cnt = it->arrayInfo()->Offset (field->arrayInfo());
	  i_offset += cnt * field_sz_i;
	  b_offset += cnt * field_sz_b;
	}
	d = dynamic_cast <Data *> (it->BaseType());
	x = d->getStructOffsetPair (field->Rest(), boff, ioff);
	if (x == 0) {
	  return 0;
	}
	*boff += b_offset;
	*ioff += i_offset;
	return 1;
      }
    }
    if (it->arrayInfo()) {
      cnt = it->arrayInfo()->size();
    }
    else {
      cnt = 1;
    }
    i_offset += cnt * field_sz_i;
    b_offset += cnt * field_sz_b;
  }
  return 0;
}


int Function::isExternal ()
{
  return !getlang() || !getlang()->getchp();
}

/*
 *  return 1 for input, 2 for output
 */
int Channel::chanDir (ActId *id, int isinput)
{
  int iosend = 0, iorecv = 0;

  InstType *it = I->Lookup (id->getName());

  Assert (it, "Fragmented piece not in the port list?");
  
  int dir = it->getDir ();
  if (dir == Type::INOUT) {
    if (isinput) {
      return 1;
    }
    else {
      return 2;
    }
  }
  else if (dir == Type::OUTIN) {
    if (isinput) {
      return 2;
    }
    else {
      return 1;
    }
  }
  else if (dir == Type::IN) {
    if (isinput) {
      return 3;
    }
    else {
      return 0;
    }
  }
  else if (dir == Type::OUT) {
    if (isinput) {
      return 0;
    }
    else {
      return 3;
    }
  }

  /*-- now look through methods --*/
  if (methods[ACT_METHOD_SET]) {
    iosend = act_hse_direction (methods[ACT_METHOD_SET], id);
  }
  if (!iosend && methods[ACT_METHOD_SEND_REST]) {
    iosend = act_hse_direction (methods[ACT_METHOD_SEND_REST], id);
  }

  dir = 0;

  if (isinput) {
    if (iosend & 1) {
      dir |= 2;
    }
  }
  else {
    if (iosend & 2) {
      dir |= 2;
    }
  }
  
  if (methods[ACT_METHOD_GET]) {
    iorecv = act_hse_direction (methods[ACT_METHOD_GET], id);
  }
  if (!iorecv && methods[ACT_METHOD_RECV_REST]) {
    iorecv = act_hse_direction (methods[ACT_METHOD_RECV_REST], id);
  }

  if (isinput) {
    if (iorecv & 1) {
      dir |= 1;
    }
  }
  else {
    if (iorecv & 2) {
      dir |= 1;
    }
  }
  
  return dir;
}


int Channel::mustbeActiveSend ()
{
  if (geteMethod (ACT_METHOD_SEND_PROBE) &&
      geteMethod (ACT_METHOD_RECV_PROBE)) {
    fprintf (stderr, "Channel type: %s", getName());
    fatal_error ("Channel cannot define probes on both send and receive");
  }
  if (isBiDirectional()) {
    if (geteMethod (ACT_METHOD_SEND_PROBE)) {
      fprintf (stderr, "Channel type: %s", getName());
      fatal_error ("Bidirectional channel cannot define probe on send");
    }
    return 1;
  }
  if (geteMethod (ACT_METHOD_SEND_PROBE)) {
    return 0;
  }
  else if (geteMethod (ACT_METHOD_RECV_PROBE)) {
    return 1;
  }
  else {
    return -1;
  }
}

int Channel::mustbeActiveRecv ()
{
  int x = mustbeActiveSend();
  if (x == 0) {
    return 1;
  }
  else if (x == 1) {
    return 0;
  }
  else {
    return -1;
  }
}


int UserDef::emitMacros (FILE *fp)
{
  if (A_LEN (um) > 0) {
    for (int i=0; i < A_LEN (um); i++) {
      UserMacro *u = um[i];
      Assert (u, "Hmm");

      /* don't print built-in macros! */
      if (u->isBuiltinMacro()) {
	continue;
      }
      
      u->Print (fp);
      fprintf (fp, "\n");
    }
    return 1;
  }
  return 0;
}

/*--- macros ---*/


UserMacro *UserDef::newMacro (const char *name)
{
  name = string_cache (name);
  for (int i=0; i < A_LEN (um); i++) {
    if (strcmp (um[i]->getName(), name) == 0) {
      return NULL;
    }
  }
  A_NEW (um, UserMacro *);
  A_NEXT (um) = new UserMacro (this, name);
  A_INC (um);
  return um[A_LEN(um)-1];
}

UserMacro *UserDef::getMacro (const char *name)
{
  name = string_cache (name);
  for (int i=0; i < A_LEN (um); i++) {
    if (strcmp (um[i]->getName(), name) == 0) {
      return um[i];
    }
  }
  return NULL;
}


/* copy over userdef */
UserDef::UserDef (UserDef *x)
{
  A_INIT (um);
  MkCopy (x);
}


UserDef *UserDef::getUnexpanded()
{
  return unexpanded;
}

void Data::synthStructMacro ()
{
  UserDef *orig = getUnexpanded();
  UserMacro *tmp_um = orig->newMacro (orig->getName());
  tmp_um->mkBuiltin ();
  tmp_um->setRetType (new InstType (orig->CurScope(), orig, 0));

  newMacro (orig->getName());
  
  Assert (um[A_LEN (um)-1]->getName() == tmp_um->getName(), "What?");
  
  um[A_LEN(um)-1] = tmp_um->Expand (this, getns(), CurScope(), 0);
  um[A_LEN(um)-1]->setParent (this);
  um[A_LEN(um)-1]->populateCHP ();
  um[A_LEN(um)-1]->updateFn (this);
}



void UserDef::_apply_ref_overrides (ActBody *b, ActBody *srch)
{
  while (srch) {
    ActBody_Lang *l = dynamic_cast<ActBody_Lang *> (srch);
    if (l) {
      if (l->gettype() == ActBody_Lang::LANG_REFINE) {
	act_refine *r = (act_refine *) l->getlang();
	if (acceptRefine (ActNamespace::Act()->getRefSteps(), r->nsteps) &&
	    r->overrides) {
	  /* apply overrides! */
	  b->updateInstType (r->overrides->ids, r->overrides->it);
	}
      }
    }
    srch = srch->Next();
  }
}
