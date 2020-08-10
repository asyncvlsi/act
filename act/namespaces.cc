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
#include "misc.h"
#include "array.h"

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
char *ActNamespace::Name ()
{
  int sz = 1, len;
  ActNamespace *ns;
  char *ret;

  ns = this;

  if (ns == global) {
    return Strdup ("::");
  }
  
  while (ns->parent) {
    sz += strlen (ns->self_bucket->key);
    sz += 2;
    ns = ns->parent;
  }

  MALLOC (ret, char, sz);
  ns = this;
  ret[sz-1] = '\0';
  sz--;
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
  int i;
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
      }
      else if (TypeFactory::isDataType (t)) {
	u->PrintHeader (fp, "deftype");
      }
      else if (TypeFactory::isChanType (t)) {
	u->PrintHeader (fp, "defchan");
      }
      fprintf (fp, ";\n");
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
      u->Print (fp);
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
