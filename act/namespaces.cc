/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#include <iostream>
#include <act/namespaces.h>
#include <act/types.h>
#include <act/inst.h>
#include <act/iter.h>
#include <act/act.h>
#include <act/body.h>
#include <string.h>
#include <common/misc.h>
#include <common/array.h>

ActNamespace *ActNamespace::global = NULL;
int ActNamespace::creating_global = 0;
Act *ActNamespace::act = NULL;

/**
 * Initialize namespaces. This function creates the ``Global''
 * namespace, and initializes static member function global
 */
void ActNamespace::Init ()
{
  if (global) {
    /* this function has already been called */
    return;
  }
  else {
    creating_global = 1;
    global = new ActNamespace ("Global");
    global->I->setNamespace (global);
    creating_global = 0;
  }
}
      
void ActNamespace::_init (ActNamespace *ns, const char *s)
{
  if (creating_global) {
    /* skip the checks */
  }
  else {
    ActNamespace::Init();
    if (!ns) {
      ns = global;
    }
    if (ns->findNS (s)) {
      fatal_error ("Cannot create duplicate namespace `%s'\n", s);
    }
  }
  N = hash_new (2);
  if (!creating_global) {
    Assert (ns, "This should not happen");
    Link (ns, s);
  }
  else {
    parent = ns;
  }
  T = hash_new (4);
  if (creating_global) {
    I = new Scope  (NULL);
    I->setNamespace (this);
  }
  else {
    I = new Scope(ns->CurScope());
    I->setNamespace (this);
  }
  B = NULL;
  exported = 0;
  lang = new act_languages ();
}

void ActNamespace::AppendBody (ActBody *b)
{
  if (B) { 
    B->Append (b); 
  } else { 
    B = b;
  }
}


/**
 * Create a new namespace within the specified namespace
 */
ActNamespace::ActNamespace (ActNamespace *ns, const char *s)
{
  _init (ns, s);
}

/**
 *  Create a new namespace within the global namespace
 */
ActNamespace::ActNamespace (const char *s)
{
  _init (NULL, s);
}


/**
 * Find a namespace in the current context
 */
ActNamespace *ActNamespace::findNS (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (N, s);
  if (b) {
    return (ActNamespace *)b->v;
  }
  else {
    return NULL;
  }
}


/**
 * Unlink namespace
 */
void ActNamespace::Unlink ()
{
  ActNamespace *ns = parent;

  Assert (ns, "No parent to this namespace?");
  hash_delete (ns->N, self_bucket->key);
  self_bucket = NULL;
  parent = NULL;
}


/**
 * Link namespace to parent
 */
void ActNamespace::Link (ActNamespace *up, const char *name)
{
  self_bucket = hash_add (up->N, name);
  self_bucket->v = this;
  parent = up;
}


/**
 * String name
 */
char *ActNamespace::Name (bool add_colon)
{
  int sz = 1, len;
  ActNamespace *ns;
  char *ret;

  ns = this;

  if (ns == global) {
    if (add_colon) {
      return Strdup ("::");
    }
    else {
      return Strdup ("");
    }
  }
  
  while (ns->parent) {
    sz += strlen (ns->self_bucket->key);
    sz += 2;
    ns = ns->parent;
  }

  if (add_colon) {
    sz += 2;
  }

  MALLOC (ret, char, sz);
  ns = this;
  ret[sz-1] = '\0';
  if (add_colon) {
    ret[sz-2] = ':';
    ret[sz-3] = ':';
    sz-=3;
  }
  else {
    sz--;
  }
  
  while (ns->parent) {
    len = strlen (ns->self_bucket->key);
    strncpy (&ret[sz]-len, ns->self_bucket->key, len);
    sz -= len;
    ret[--sz] = ':';
    ret[--sz] = ':';
    ns = ns->parent;
  }
  Assert (sz == 0, "Eh?");
  return ret;
}

/**
 * Functions for class ActOpen
 *
 */

ActOpen::ActOpen ()
{
  search_path = list_new ();
}


ActOpen::~ActOpen ()
{
  list_free (search_path);
}


int ActOpen::Open(ActNamespace *ns, const char *newname)
{
  listitem_t *li;

  if (newname) {
    if (ActNamespace::Global()->findNS (newname)) {
      return 0;
    }
    ns->Unlink ();
    ns->Link (ActNamespace::Global(), newname);
    // top level namespace, no export flag required
    ns->clrExported ();
  }
  else {
    /* add to the search path */
    for (li = list_first (search_path); li; li = list_next (li)) {
      if (list_value (li) == (void *)ns)
	return 1;
    }
    stack_push (search_path, ns);
  }
  return 1;
}


/**
 * Find namespace in the current context, given the context of opens
 *
 */
list_t *ActOpen::findAll (ActNamespace *cur, const char *s)
{
  listitem_t *li;
  ActNamespace *ns;
  list_t *ret = NULL;

  /* Search in the current namespace */
  if (cur) {
    ns = cur->findNS (s);
    if (ns) {
      if (!ret) {
	ret = list_new ();
      }
      list_append (ret, ns);
    }
  }

  /* Search in the path */
  Assert (search_path, "Constructor didn't get called?");
  for (li = list_first (search_path); li; li = list_next (li)) {
    ns = (ActNamespace *) list_value (li);
    Assert (ns, "What?");
    ns = ns->findNS (s);
    if (ns) {
      if (!ret) {
	ret = list_new ();
      }
      list_append (ret, ns);
    }
  }

  /* Look in the global namespace */
  ns = ActNamespace::Global()->findNS (s);
  if (ns) {
    if (!ret) {
      ret = list_new ();
    }
    list_append (ret, ns);
  }

  return ret;
}

/**
 * Find namespace in the current context, given the context of opens
 *
 */
ActNamespace *ActOpen::find (ActNamespace *cur, const char *s)
{
  listitem_t *li;
  ActNamespace *ns;

  /* Search in the current namespace */
  if (cur) {
    ns = cur->findNS (s);
    if (ns) {
      return ns;
    }
  }

  /* Search in the path */
  Assert (search_path, "Constructor didn't get called?");
  for (li = list_first (search_path); li; li = list_next (li)) {
    ns = (ActNamespace *) list_value (li);
    Assert (ns, "What?");
    ns = ns->findNS (s);
    if (ns) {
      return ns;
    }
  }

  /* Look in the global namespace */
  ns = ActNamespace::Global()->findNS (s);

  return ns;
}

/**
 * Find namespace containing the type in the current context, given
 * the context of opens
 *
 */
ActNamespace *ActOpen::findType (ActNamespace *cur, const char *s)
{
  listitem_t *li;
  ActNamespace *ns;

  /* Search in the current namespace */
  while (cur) {
    if (cur->findType (s)) {
      return cur;
    }
    cur = cur->Parent ();
  }

  /* Search in the path */
  Assert (search_path, "Constructor didn't get called?");
  for (li = list_first (search_path); li; li = list_next (li)) {
    ns = (ActNamespace *) list_value (li);
    if (ns->findType (s)) {
      return ns;
    }
  }

  /* Look in the global namespace */
  if (ActNamespace::Global()->findType (s)) {
    return ActNamespace::Global();
  }
  
  return NULL;
}


/**
 * Find a type in the current namespace
 */
UserDef *ActNamespace::findType (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (T, s);
  if (b) {
    return (UserDef *)b->v;
  }
  else {
    return NULL;
  }
}


/**
 * Find an instance in the current namespace 
 */
InstType *ActNamespace::findInstance (const char *s)
{
  return I->Lookup (s);
}


/**
 * Create a new type
 */
int ActNamespace::CreateType (const char *s, UserDef *u)
{
  hash_bucket_t *b;

  b = hash_lookup (T, s);
  if (b) {
    /* failure: name already exists */
    return 0;
  }
  else {
    b = hash_add (T, s);
    b->v = u;
    u->setName (b->key);

    return 1;
  }
}

int ActNamespace::EditType (const char *s, UserDef *u)
{
  hash_bucket_t *b;
  b = hash_lookup (T, s);
  if (!b) {
    return 0;
  }
  b->v = u;
  u->setName (b->key);
  return 1;
}


/**
 *  Check if a name is free
 */
int ActNamespace::findName (const char *s)
{
  if (findNS (s)) {
    return 2;
  }
  else if (findType (s)) {
    return 1;
  }
  else if (findInstance (s)) {
    return 3;
  }
  return 0;
}




/*------------------------------------------------------------------------
 *
 * Expand a namespace
 *
 *------------------------------------------------------------------------
 */
void ActNamespace::Expand ()
{
  ActNamespace *ns;
  hash_bucket_t *bkt;
  hash_iter_t iter;

  if (this == global) {
    act_error_push ("::<Global>", NULL, 0);
  }
  else {
    act_error_push (getName(), NULL, 0);
  }

  Assert (I, "No scope?");

  /* flush the scope, and re-create it! */
  I->FlushExpand ();

  /* 
     YYY: fixme

     This has to be expanded in the order it was encountered in the
     file!

     import globals;
     import foo;
     import bar;

     ... 
     defns

     more stuff
  */

  /* Expand any sub-namespaces */
  hash_iter_init (N, &iter);
  while ((bkt = hash_iter_next (N, &iter))) {
    ns = (ActNamespace *)bkt->v;
    ns->Expand();
  }

  /* Expand all meta parameters at the top level of the namespace. */
  if (B) {
    B->Expandlist (this, I);
  }

  act_error_pop ();
}


void ActNamespace::enum2Int ()
{
  ActNamespace *ns;
  hash_bucket_t *bkt;
  hash_iter_t iter;

  Assert (I, "No scope?");

  /* Expand any sub-namespaces */
  hash_iter_init (N, &iter);
  while ((bkt = hash_iter_next (N, &iter))) {
    ns = (ActNamespace *)bkt->v;
    ns->enum2Int ();
  }

  hash_iter_init (T, &iter);
  while ((bkt = hash_iter_next (T, &iter))) {
    UserDef *u = (UserDef *) bkt->v;
    Data *d = dynamic_cast<Data *> (u);
    if (d && d->isEnum()) {
      d->MkEnum (1);
    }
  }
}


void ActNamespace::setAct (class Act *a)
{
  ActNamespace::act = a;
  ActNamespace::global = a->Global();
}


/*
  Print namespace
*/
void ActNamespace::Print (FILE *fp)
{
  if (this != ActNamespace::Global()) {
    fprintf (fp, "namespace %s {\n", getName());
  }

  /* print subnamespaces */
  ActNamespaceiter i(this);
  for (i = i.begin(); i != i.end(); i++) {
    ActNamespace *n = *i;
    n->Print (fp);
  }

  /* print type declarations  */
  ActTypeiter it(this);
  for (it = it.begin(); it != it.end(); it++) {
    Type *t = *it;
    UserDef *u = dynamic_cast<UserDef *>(t);
    Assert (u, "Hmm...");
    /* print type! */
    if (CurScope()->isExpanded() == u->isExpanded()) {
      if (TypeFactory::isProcessType (t)) {
	Process *p = dynamic_cast<Process *>(t);
	if (p->isCell()) {
	  u->PrintHeader (fp, "defcell");
	}
	else {
	  u->PrintHeader (fp, "defproc");
	}
	fprintf (fp, ";\n");
      }
      else if (TypeFactory::isDataType (t)) {
	Data *d = dynamic_cast<Data *> (t);
	if (d->isEnum()) {
	  // nothing! 
	}
	else {
	  u->PrintHeader (fp, "deftype");
	  fprintf (fp, ";\n");
	}
      }
      else if (TypeFactory::isChanType (t)) {
	u->PrintHeader (fp, "defchan");
	fprintf (fp, ";\n");
      }
      else if (TypeFactory::isInterfaceType (t)) {
	u->PrintHeader (fp, "interface");
	fprintf (fp, ";\n");
      }
      else if (TypeFactory::isFuncType (t)) {
	Function *f = dynamic_cast<Function *> (t);
	if (!u->isExpanded()  || !TypeFactory::isParamType (f->getRetType())) {
	  u->PrintHeader (fp, "function");
	  fprintf (fp, " : ");
	  if (TypeFactory::isUserType (f->getRetType())) {
	    UserDef *tu = dynamic_cast<UserDef *> (f->getRetType()->BaseType());
	    ActNamespace::Act()->mfprintfproc (fp, tu, 1);
	  }
	  else {
	    f->getRetType()->Print (fp);
	  }
	  fprintf (fp, ";\n");
	}
      }
      else if (TypeFactory::isStructure (t)) {
	u->PrintHeader (fp, "deftype");
	fprintf (fp, ";\n");
      }
      else {
	fprintf (stderr, "Got: %s\n", t->getName());
	fatal_error ("Unhandled case...");
      }
    }
  }
  fprintf (fp, "\n");

  /* print types */
  for (it = it.begin(); it != it.end(); it++) {
    Type *t = *it;
    UserDef *u = dynamic_cast<UserDef *>(t);
    Assert (u, "Hmm...");
    /* print type! */
    if (CurScope()->isExpanded() == u->isExpanded()) {
      Function *f = dynamic_cast<Function *> (t);
      if (!f || !u->isExpanded() || !TypeFactory::isParamType (f->getRetType())) {
	u->Print (fp);
      }
    }
  }

  /* print instances */
  CurScope()->Print (fp);
  
  if (this != ActNamespace::Global()) {
    fprintf (fp, "}\n");
  }
  else {
    /* languages */
    lang->Print (fp);
  }
}


act_prs *ActNamespace::getprs ()
{
  return lang->getprs();
}

act_spec *ActNamespace::getspec()
{
  return lang->getspec();
}


list_t *ActNamespace::getSubNamespaces()
{
  list_t *tmp;
  hash_bucket_t *b;
  hash_iter_t it;

  tmp = list_new ();

  hash_iter_init (N, &it);
  while ((b = hash_iter_next (N, &it))) {
    list_append (tmp, b->key);
  }
  return tmp;
}

list_t *ActNamespace::getProcList ()
{
  list_t *tmp;
  hash_bucket_t *b;
  hash_iter_t it;

  tmp = list_new ();

  hash_iter_init (T, &it);
  while ((b = hash_iter_next (T, &it))) {
    UserDef *u = (UserDef *)b->v;
    if (TypeFactory::isProcessType (u)) {
      list_append (tmp, b->key);
    }
  }
  return tmp;
}

list_t *ActNamespace::getDataList ()
{
  list_t *tmp;
  hash_bucket_t *b;
  hash_iter_t it;

  tmp = list_new ();

  hash_iter_init (T, &it);
  while ((b = hash_iter_next (T, &it))) {
    UserDef *u = (UserDef *)b->v;
    if (TypeFactory::isDataType (u)) {
      list_append (tmp, b->key);
    }
  }
  return tmp;
}

list_t *ActNamespace::getChanList ()
{
  list_t *tmp;
  hash_bucket_t *b;
  hash_iter_t it;

  tmp = list_new ();

  hash_iter_init (T, &it);
  while ((b = hash_iter_next (T, &it))) {
    UserDef *u = (UserDef *)b->v;
    if (TypeFactory::isChanType (u)) {
      list_append (tmp, b->key);
    }
  }
  return tmp;
}


void ActNamespace::_subst_globals (list_t *defs, InstType *it, const char *s)
{
  ActNamespace *ns;
  hash_bucket_t *bkt;
  hash_iter_t iter;

  Assert (I, "No scope?");
  Assert (!I->isExpanded(), "What?!");

  /* substitute in any sub-namespaces */
  hash_iter_init (N, &iter);
  while ((bkt = hash_iter_next (N, &iter))) {
    ns = (ActNamespace *)bkt->v;
    ns->_subst_globals (defs, it, s);
  }

  /* For each process, apply this substitution.

     Phase 1: if the signal is used, add it to the port list
  */
  hash_iter_init (T, &iter);
  while ((bkt = hash_iter_next (T, &iter))) {
    UserDef *u = (UserDef *) bkt->v;
    if (TypeFactory::isProcessType (u)) {
      Process *up = dynamic_cast<Process *> (u);
      Assert (up->isExpanded() == 0, "What!");
      if (up->findGlobal (s)) {
	/* XXX FIXME */
	up->AddPort (it, s);
	list_append (defs, up);
      }
    }
  }
}


static void _process_body_conn (Process *proc, InstType *it, const char *s,
				list_t *deflist, listitem_t *start, ActBody *b)
{
  while (b) {
    ActBody_Inst *inst = dynamic_cast<ActBody_Inst*> (b);
    ActBody_Loop *l = dynamic_cast<ActBody_Loop *> (b);
    ActBody_Select *sel = dynamic_cast<ActBody_Select *> (b);
    ActBody_Genloop *gl = dynamic_cast<ActBody_Genloop *> (b);
    if (inst) {
      InstType *inst_it = inst->getType ();
      if (TypeFactory::isProcessType (inst_it)) {
	listitem_t *li;
	int found = 0;
	Process *myp = dynamic_cast<Process *> (inst->BaseType ());
	Assert (myp, "What?");
	for (li = start; li; li = list_next (li)) {
	  Process *p = (Process *) list_value (li);
	  if (p == myp) {
	    found = 1;
	    break;
	  }
	}
	if (found) {
	  /* check if I am in the def list */
	  listitem_t *ti = NULL;
	  for (ti = list_first (deflist); ti; ti = list_next (ti)) {
	    if (proc == (Process *) list_value (ti)) {
	      break;
	    }
	  }
	  if (ti == NULL) {
	    /* XXX: add port! */
	    proc->AddPort (it, s);
	    list_append (deflist, proc);
	  }
	  
	  /* add connection */
	  if (inst_it->arrayInfo()) {
	    int dims = inst_it->arrayInfo()->nDims();
	    int idxnum = 0;
	    char buf[100];
	    char **idxnames;

	    Assert (dims > 0, "What?");
	    MALLOC (idxnames, char *, dims);
	    for (int i=0; i < dims; i++) {
	      do {
		snprintf (buf, 100, "_idx%d", idxnum++);
	      } while (proc->Lookup (buf));
	      idxnames[i] = Strdup (buf);
	    }

	    ActBody_Conn *ac;
	    ActId *tid = new ActId (s);
	    
	    Array *a = NULL;

	    for (int i=0; i < dims; i++) {
	      Expr *ex;
	      NEW (ex, Expr);
	      ex->type = E_VAR;
	      ex->u.e.l = (Expr *) new ActId (idxnames[i]);
	      ex->u.e.r = NULL;
	      if (!a) {
		a = new Array (ex);
	      }
	      else {
		a->Concat (new Array (ex));
	      }
	    }
	      
	    ActId *lhs = new ActId (inst->getName(), a);
	    lhs->Append (new ActId (s));
	    ac = new ActBody_Conn (b->getLine(), lhs, new AExpr (tid));

	    ActBody_Loop *prev = NULL;
	    for (int i=0; i < dims; i++) {
	      ActBody_Loop *tmp;
	      Expr *lo, *hi;
	      if (inst_it->arrayInfo()->lo(i)) {
		lo = inst_it->arrayInfo()->lo(i);
		hi = inst_it->arrayInfo()->hi(i);
	      }
	      else {
		lo = inst_it->arrayInfo()->hi(i);
		hi = NULL;
	      }
	      tmp = new ActBody_Loop (b->getLine(),
				      ActBody_Loop::SEMI,
				      string_cache (idxnames[i]),
				      lo, hi, prev ? (ActBody *)prev :
				      (ActBody *)ac);
	      prev = tmp;
	    }
	    b->Append (prev);

	    for (int i=0; i < dims; i++) {
	      FREE (idxnames[i]);
	    }
	    FREE (idxnames);
	  }
	  else {
	    ActBody_Conn *ac;
	    ActId *tid = new ActId (s);
	    ActId *lhs = new ActId (inst->getName());
	    lhs->Append (new ActId (s));
	    ac = new ActBody_Conn (b->getLine(), lhs, new AExpr (tid));
	    b->Append (ac);
	  }
	}
      }
    }
    else if (l) {
      _process_body_conn (proc, it, s, deflist, start, l->getBody());
    }
    else if (sel || gl) {
      ActBody_Select_gc *gc;
      if (sel) {
	gc = sel->getGC ();
      }
      else {
	gc = gl->getGC ();
      }
      Assert (gc, "What?");
      while (gc) {
	_process_body_conn (proc, it, s, deflist, start, gc->getBody ());
	gc = gc->getNext ();
      }
    }
    b = b->Next();
  }
}



void ActNamespace::_subst_globals_addconn (list_t *defs, listitem_t *start,
					   InstType *it, const char *s)
{
  ActNamespace *ns;
  hash_bucket_t *bkt;
  hash_iter_t iter;

  Assert (I, "No scope?");
  Assert (!I->isExpanded(), "What?!");

  /* substitute in any sub-namespaces */
  hash_iter_init (N, &iter);
  while ((bkt = hash_iter_next (N, &iter))) {
    ns = (ActNamespace *)bkt->v;
    ns->_subst_globals_addconn (defs, start, it, s);
  }

  /* For each process, apply this substitution.
     Phase 2:
       * whenever an instance of a type in the defs subst list is
         found starting from "start", we add a connection. 
	 If it is an array instance, we need to add
	 an ActBody_Loop that contains the connection; and a nested set
	 for multi-dimensional arrays.
       
       * when we do this update, we must CHECK to see if the current
       process is in the full def list; if it is, nothing has to be done

       * if the process is NOT in the defs list, then
          - if it has a port with the same name as the global, we need
	    to pick a fresh name; try "globalN"
	  - add the fresh port
	  - add the process to the defs list
	  - add to fresh defs list
  */
  hash_iter_init (T, &iter);
  while ((bkt = hash_iter_next (T, &iter))) {
    UserDef *u = (UserDef *) bkt->v;
    if (TypeFactory::isProcessType (u)) {
      Process *up = dynamic_cast<Process *> (u);
      Assert (up->isExpanded() == 0, "What!");
      /* walk through the body of this process */
      _process_body_conn (up, it, s, defs, start, up->getBody ());
    }
  }
}
