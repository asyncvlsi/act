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
#include <dlfcn.h>


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



/*
 *
 * Load a pass from a .so/.dylib file
 *
 */
list_t *ActDynamicPass::_sh_libs = NULL;


ActDynamicPass::ActDynamicPass (Act *a, const char *name, const char *lib,
				const char *prefix) : ActPass (a, name)
{
  listitem_t *li;
  void *lib_ptr;
  char buf[1024];

  if (!_sh_libs) {
    _sh_libs = list_new ();
  }

  lib_ptr = NULL;

  for (li = list_first (_sh_libs); li; li = list_next (li)) {
    act_sh_passlib_info *tmp = (act_sh_passlib_info *) list_value (li);
    if (strcmp (lib, tmp->lib) == 0) {
      lib_ptr = tmp->lib_ptr;
      tmp->refs++;
      break;
    }
  }

  if (!lib_ptr) {
    act_sh_passlib_info *tmp;
    lib_ptr = dlopen (lib, RTLD_LAZY);
    NEW (tmp, act_sh_passlib_info);
    tmp->lib = Strdup (lib);
    _libused = tmp->lib;
    tmp->lib_ptr = lib_ptr;
    tmp->refs = 1;
    list_append (_sh_libs, tmp);
  }

  /* -- maps to everything -- */
  Assert (lib_ptr, "What?!");

  /* -- names:

     <prefix>_init
     <prefix>_run
     <prefix>_local_proc
     <prefix>_local_chan
     <prefix>_local_data
     <prefix>_free_local

  -- */

  snprintf (buf, 1024, "%s_init", prefix);
  _d._init = (void (*)(ActPass *))dlsym (lib_ptr, buf);
  if (!_d._init) {
    warning ("Dynamic pass `%s': missing %s\n", buf);
  }

  snprintf (buf, 1024, "%s_run", prefix);
  _d._run = (void (*)(Process *))dlsym (lib_ptr, buf);

  snprintf (buf, 1024, "%s_recursive", prefix);
  _d._recursive = (void (*)(Process *, int))dlsym (lib_ptr,  buf);
  
  snprintf (buf, 1024, "%s_local_proc", prefix);
  _d._proc = (void *(*)(Process *, int))dlsym (lib_ptr, buf);
  
  snprintf (buf, 1024, "%s_local_data", prefix);
  _d._data = (void *(*)(Data *, int))dlsym (lib_ptr, buf);

  snprintf (buf, 1024, "%s_local_chan", prefix);
  _d._chan = (void *(*)(Channel *, int))dlsym (lib_ptr, buf);

  snprintf (buf, 1024, "%s_free", prefix);
  _d._free = (void(*)(void *))dlsym (lib_ptr, buf);

  snprintf (buf, 1024, "%s_done", prefix);
  _d._done = (void(*)(void))dlsym (lib_ptr, buf);

  if (_d._init) {
    (*_d._init)(this);
  }
}

int ActDynamicPass::run (Process *p)
{
  int ret = ActPass::run (p);

  if (_d._run) {
    (*_d._run)(p);
  }
  
  return ret;
}

void ActDynamicPass::run_recursive (Process *p, int mode)
{
  ActPass::run_recursive (p, mode);
 
  if (_d._recursive) {
    (*_d._recursive) (p, mode);
  }
}

void *ActDynamicPass::local_op (Process *p, int mode)
{
  if (_d._proc) {
    return (*_d._proc)(p, mode);
  }
  return NULL;
}

void *ActDynamicPass::local_op (Channel *c, int mode)
{
  if (_d._chan) {
    return (*_d._chan)(c, mode);
  }
  return NULL;
}

void *ActDynamicPass::local_op (Data *d, int mode)
{
  if (_d._data) {
    return (*_d._data)(d, mode);
  }
  return NULL;
}

void ActDynamicPass::free_local (void *v)
{
  if (_d._free) {
    (*_d._free) (v);
  }
}

ActDynamicPass::~ActDynamicPass ()
{
  listitem_t *li;
  listitem_t *prev;

  if (_d._done) {
    (*_d._done) ();
  }
  
  Assert (_sh_libs, "What?");
  prev = NULL;
  for (li = list_first (_sh_libs); li; li = list_next (li)) {
    act_sh_passlib_info *tmp = (act_sh_passlib_info *) list_value (li);
    Assert (tmp->refs > 0, "What?");
    if (strcmp (tmp->lib, _libused) == 0) {
      tmp->refs--;
      if (tmp->refs == 0) {
	/* unload library */
	dlclose (tmp->lib_ptr);
	FREE (tmp->lib);
	FREE (tmp);
	list_delete_next (_sh_libs, prev);
      }
      break;
    }
  }
  if (list_length (_sh_libs) == 0) {
    list_free (_sh_libs);
    _sh_libs = NULL;
  }
}
