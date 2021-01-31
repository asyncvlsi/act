/*************************************************************************
 *
 *  Copyright (c) 2019 Rajit Manohar
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
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
#include <act/act.h>
#include <act/iter.h>
#include <string.h>


ActPass::ActPass (Act *_a, const char *s)
{
  Assert (_a, "ActPass: requires non-NULL Act pointer!");
  _finished = 0;
  a = _a;
  deps = list_new ();
  _a->pass_register (s, this);
  name = _a->pass_name (s);
  pmap = NULL;
  visited_flag = NULL;
}

ActPass::~ActPass ()
{
  free_map ();
  a->pass_unregister (getName());
  list_free (deps);
}

int ActPass::AddDependency (const char *pass)
{
  ActPass *p = a->pass_find (pass);
  if (!p) {
    fatal_error ("Adding dependency to unknown pass `%s'", pass);
  }
  list_append (deps, p);
  return 1;
}

int ActPass::rundeps (Process *p)
{
  listitem_t *li;

  if (completed()) return 1;

  if (!pending()) {
    _finished = 1;
  }
  for (li = list_first (deps); li; li = list_next (li)) {
    ActPass *x = (ActPass *) list_value (li);
    if (x->pending()) {
      fatal_error ("Cyclic dependency to pass %s", x->getName());
      return 0;
    }
    if (!x->completed()) {
      if (!x->run (p)) {
	fatal_error ("Pass %s failed; aborting", x->getName());
	return 0;
      }
    }
  }
  return 1;
}


const char *ActPass::getName ()
{
  return name;
}

/* defaults */
int ActPass::run (Process *p)
{
  init();

  if (!rundeps (p)) {
    return 0;
  }

  visited_flag = new std::unordered_set<UserDef *> ();
  
  /* do the work */
  if (p) {
    act_error_push (p->getName(), NULL, 0);
  }
  else {
    act_error_push ("-toplevel-", NULL, 0);
  }
  recursive_op (p);
  act_error_pop ();
  
  delete visited_flag;
  visited_flag = NULL;

  _finished = 2;

  return 1;
}

void ActPass::run_recursive (Process *p, int mode)
{
  if (!completed()) {
    return;
  }

  visited_flag = new std::unordered_set<UserDef *> ();

  if (p) {
    act_error_push (p->getName(), NULL, 0);
  }
  else {
    act_error_push ("-toplevel-", NULL, 0);
  }
  recursive_op (p, mode);
  act_error_pop ();
  
  delete visited_flag;
  visited_flag = NULL;
}

int ActPass::init ()
{
  init_map();

  _finished = 1;
    
  return 0;
}

void *ActPass::local_op (Process *p, int mode) { return NULL; }
void *ActPass::local_op (Channel *c, int mode) { return NULL; }
void *ActPass::local_op (Data *d, int mode) { return NULL; }
void ActPass::free_local (void *v) { if (v) { FREE (v); } }


void ActPass::init_map ()
{
  free_map ();
  pmap = new std::map<UserDef *, void *> ();
}

void ActPass::free_map ()
{
  if (pmap) {
    std::map<UserDef *, void *>::iterator it;
    for (it = pmap->begin(); it != pmap->end(); it++) {
      free_local (it->second);
    }
    delete pmap;
  }
  pmap = NULL;
}

void ActPass::recursive_op (UserDef *p, int mode)
{
  ActInstiter i(p ? p->CurScope() : ActNamespace::Global()->CurScope());

  if (visited_flag->find (p) != visited_flag->end()) {
    return;
  }
  visited_flag->insert (p);

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      Process *x = dynamic_cast<Process *> (vx->t->BaseType());
      if (x->isExpanded()) {
	char *tmp;
	int len;
	len = strlen (x->getName()) + strlen (vx->getName()) + 10;
	MALLOC (tmp, char, len);
	snprintf (tmp, len, "%s (inst: %s)", x->getName(), vx->getName());
	act_error_push (tmp, NULL, 0);
	recursive_op (x, mode);
	act_error_pop ();
	FREE (tmp);
      }
    }
    else if (TypeFactory::isUserType (vx->t)) {
      if (TypeFactory::isChanType (vx->t)) {
	Channel *x = dynamic_cast<Channel *> (vx->t->BaseType());
	Assert (x, "what?");
	if (x->isExpanded()) {
	  char *tmp;
	  int len;
	  len = strlen (x->getName()) + strlen (vx->getName()) + 10;
	  MALLOC (tmp, char, len);
	  snprintf (tmp, len, "%s (inst: %s)", x->getName(), vx->getName());
	  act_error_push (tmp, NULL, 0);
	  recursive_op (x, mode);
	  act_error_pop ();
	  FREE (tmp);
	}
      }
      else {
	Data *x = dynamic_cast<Data *> (vx->t->BaseType());
	Assert (x, "what?");
	if (x->isExpanded()) {
	  char *tmp;
	  int len;
	  len = strlen (x->getName()) + strlen (vx->getName()) + 10;
	  MALLOC (tmp, char, len);
	  snprintf (tmp, len, "%s (inst: %s)", x->getName(), vx->getName());
	  act_error_push (tmp, NULL, 0);
	  recursive_op (x, mode);
	  act_error_pop ();
	  FREE (tmp);
	}
      }
    }
  }
  
  if (TypeFactory::isProcessType (p) || (p == NULL)) {
    (*pmap)[p] = local_op (dynamic_cast<Process *>(p), mode);
  }
  else if (TypeFactory::isChanType (p)) {
    (*pmap)[p] = local_op (dynamic_cast<Channel *>(p), mode);
  }
  else {
    Assert (TypeFactory::isDataType (p) || TypeFactory::isStructure (p),
	    "What?");
    (*pmap)[p] = local_op (dynamic_cast<Data *>(p), mode);
  }
}

 
void *ActPass::getMap (Process *p)
{
  return (*pmap)[p];
}
