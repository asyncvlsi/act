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
#include <act/tech.h>
#include <string.h>
#include <dlfcn.h>
#include <common/config.h>

class InternalDummyPass : public ActPass
{
 public:
  InternalDummyPass (Act *_a);

};


InternalDummyPass::InternalDummyPass (Act *_a) : ActPass (_a, "_refresh_", 0)
{

}



ActPass::ActPass (Act *_a, const char *s, int doroot)
{
  Assert (_a, "ActPass: requires non-NULL Act pointer!");
  _finished = 0;
  a = _a;
  deps = list_new ();
  fwdeps = list_new ();
  _a->pass_register (s, this);
  name = _a->pass_name (s);
  pmap = NULL;
  visited_flag = NULL;
  _root_dirty = doroot;
  _root = NULL;
  _update_propagate = 1;
  _sticky_visited = 0;

  ActPass *_tmp = _a->pass_find ("_refresh_");
  if (!_tmp) {
    new InternalDummyPass (_a);
  }
  if (strcmp ("_refresh_", s) != 0) {
    AddDependency ("_refresh_");
  }
}

void ActPass::refreshAll (Act *a, Process *p)
{
  ActPass *_tmp = a->pass_find ("_refresh_");
  Assert (_tmp, "How is this possible?");

  _tmp->update (p);
}


ActPass::~ActPass ()
{
  free_map ();
  a->pass_unregister (getName());
  list_free (deps);
  list_free (fwdeps);
}

int ActPass::AddDependency (const char *pass)
{
  ActPass *p = a->pass_find (pass);
  if (!p) {
    fatal_error ("Adding dependency to unknown pass `%s'", pass);
  }
  list_append (deps, p);
  list_append (p->fwdeps, this);
  
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

  _root = p;

  if (!rundeps (p)) {
    return 0;
  }

  visited_flag = new std::unordered_set<UserDef *> ();
  
  /* do the work */
  if (p) {
    act_error_push (p->getName(), p->getFile(), p->getLine());
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

  if (!visited_flag) {
    visited_flag = new std::unordered_set<UserDef *> ();
  }

  if (p) {
    act_error_push (p->getName(), p->getFile(), p->getLine());
  }
  else {
    act_error_push ("-toplevel-", NULL, 0);
  }
  recursive_op (p, mode);
  act_error_pop ();

  if (!_sticky_visited) {
    delete visited_flag;
    visited_flag = NULL;
  }
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
void *ActPass::pre_op (Process *p, int mode) { return NULL; }
void *ActPass::pre_op (Channel *c, int mode) { return NULL; }
void *ActPass::pre_op (Data *d, int mode) { return NULL; }
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

  if (mode >= 0) {
    if (TypeFactory::isProcessType (p) || (p == NULL)) {
      void *v = pre_op (dynamic_cast<Process *>(p), mode);
      if (v) {
	(*pmap)[p] = v;
      }
    }
    else if (TypeFactory::isChanType (p)) {
      void *v = pre_op (dynamic_cast<Channel *>(p), mode);
      if (v) {
	(*pmap)[p] = v;
      }
    }
    else {
      Assert (TypeFactory::isDataType (p) || TypeFactory::isStructure (p),
	      "What?");
      void *v = pre_op (dynamic_cast<Data *>(p), mode);
      if (v) {
	(*pmap)[p] = v;
      }
    }
  }

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
	act_error_push (tmp, x->getFile(), x->getLine());
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
	  act_error_push (tmp, x->getFile(), x->getLine());
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
	  act_error_push (tmp, x->getFile(), x->getLine());
	  recursive_op (x, mode);
	  act_error_pop ();
	  FREE (tmp);
	}
      }
    }
  }

  if (mode >= 0) {
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
  else {
    void *v = (*pmap)[p];
    free_local (v);
    (*pmap)[p] = NULL;
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

  _config_state = NULL;

  getConfig ();

  T = Technology::T;
  _params = NULL;

  if (!_sh_libs) {
    _sh_libs = list_new ();
  }

  lib_ptr = NULL;

  _d._init = NULL;
  _d._run = NULL;
  _d._recursive = NULL;
  _d._proc = NULL;
  _d._data = NULL;
  _d._chan = NULL;
  _d._free = NULL;
  _d._done = NULL;

  _load_success = false;

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
    FILE *fp;

    fp = fopen (lib, "r");
    if (fp) {
      fclose (fp);
      lib_ptr = dlopen (lib, RTLD_LAZY);
      if (!lib_ptr) {
	fprintf (stderr, "Error loading dynamic pass `%s\n", lib);
	if (dlerror()) {
	  fprintf (stderr, "%s\n", dlerror());
	}
	return;
      }
    }
    else {
      if (getenv ("ACT_HOME")) {
	snprintf (buf, 1024, "%s/lib/%s", getenv ("ACT_HOME"), lib);
	fp = fopen (buf, "r");
	if (fp) {
	  fclose (fp);
	  lib_ptr = dlopen (buf, RTLD_LAZY);
	  if (!lib_ptr) {
	    fprintf (stderr, "Error loading dynamic pass `%s\n", lib);
	    if (dlerror()) {
	      fprintf (stderr, "%s\n", dlerror());
	    }
	    return;
	  }
	}
      }
    }
    if (!lib_ptr) {
      fprintf (stderr, "Dynamic pass: `%s' not found\n", lib);
      if (dlerror()) {
	fprintf (stderr, "%s\n", dlerror());
      }
      return;
    }
    
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
    warning ("Dynamic pass `%s': missing %s\n", prefix, buf);
  }

  snprintf (buf, 1024, "%s_run", prefix);
  _d._run = (void (*)(ActPass *, Process *))dlsym (lib_ptr, buf);

  snprintf (buf, 1024, "%s_recursive", prefix);
  _d._recursive = (void (*)(ActPass *, Process *, int))dlsym (lib_ptr,  buf);
  
  snprintf (buf, 1024, "%s_proc", prefix);
  _d._proc = (void *(*)(ActPass *, Process *, int))dlsym (lib_ptr, buf);
  
  snprintf (buf, 1024, "%s_data", prefix);
  _d._data = (void *(*)(ActPass *, Data *, int))dlsym (lib_ptr, buf);

  snprintf (buf, 1024, "%s_chan", prefix);
  _d._chan = (void *(*)(ActPass *, Channel *, int))dlsym (lib_ptr, buf);

  snprintf (buf, 1024, "%s_free", prefix);
  _d._free = (void(*)(ActPass *, void *))dlsym (lib_ptr, buf);

  snprintf (buf, 1024, "%s_done", prefix);
  _d._done = (void(*)(ActPass *))dlsym (lib_ptr, buf);

  snprintf (buf, 1024, "%s_runcmd", prefix);
  _d._runcmd = (int(*)(ActPass *, const char *))dlsym (lib_ptr, buf);
  

  if (_d._init) {
    (*_d._init)(this);
  }

  _load_success = true;
  
}

struct Hashtable *ActDynamicPass::getConfig (void)
{
  if (!_config_state) {
    _config_state = config_get_state();
  }
  return _config_state;
}


int ActDynamicPass::run (Process *p)
{
  int ret = ActPass::run (p);

  if (_d._run) {
    (*_d._run)(this, p);
  }
  
  return ret;
}

void ActDynamicPass::run_recursive (Process *p, int mode)
{
  ActPass::run_recursive (p, mode);
 
  if (_d._recursive) {
    (*_d._recursive) (this, p, mode);
  }
}

int ActDynamicPass::runcmd (const char *name)
{
  if (_d._runcmd) {
    return (*_d._runcmd) (this, name);
  }
  else {
    return -1;
  }
}

void *ActDynamicPass::local_op (Process *p, int mode)
{
  if (_d._proc) {
    return (*_d._proc)(this, p, mode);
  }
  return NULL;
}

void *ActDynamicPass::local_op (Channel *c, int mode)
{
  if (_d._chan) {
    return (*_d._chan)(this, c, mode);
  }
  return NULL;
}

void *ActDynamicPass::local_op (Data *d, int mode)
{
  if (_d._data) {
    return (*_d._data)(this, d, mode);
  }
  return NULL;
}

void ActDynamicPass::free_local (void *v)
{
  if (_d._free) {
    (*_d._free) (this, v);
  }
}

ActDynamicPass::~ActDynamicPass ()
{
  listitem_t *li;
  listitem_t *prev;

  if (_d._done) {
    (*_d._done) (this);
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

void ActDynamicPass::setParam (const char *name, void *v)
{
  hash_bucket_t *b;
  if (!_params) {
    _params = hash_new (4);
  }
  b = hash_lookup (_params, name);
  if (!b) {
    b = hash_add (_params, name);
  }
  b->v = v;
}

void ActDynamicPass::setParam (const char *name, int v)
{
  hash_bucket_t *b;
  if (!_params) {
    _params = hash_new (4);
  }
  b = hash_lookup (_params, name);
  if (!b) {
    b = hash_add (_params, name);
  }
  b->i = v;
}

void ActDynamicPass::setParam (const char *name, double v)
{
  hash_bucket_t *b;
  if (!_params) {
    _params = hash_new (4);
  }
  b = hash_lookup (_params, name);
  if (!b) {
    b = hash_add (_params, name);
  }
  b->f = v;
}

int ActDynamicPass::getIntParam (const char *name)
{
  hash_bucket_t *b;
  if (!_params) { return -1; }
  b = hash_lookup (_params, name);
  if (!b) {
    return -1;
  }
  return b->i;
}

double ActDynamicPass::getRealParam (const char *name)
{
  hash_bucket_t *b;
  if (!_params) { return -1; }
  b = hash_lookup (_params, name);
  if (!b) {
    return -1;
  }
  return b->f;
}

void *ActDynamicPass::getPtrParam (const char *name)
{
  hash_bucket_t *b;
  if (!_params) { return NULL; }
  b = hash_lookup (_params, name);
  if (!b) {
    return NULL;
  }
  return b->v;
}

void ActPass::_actual_update (Process *p)
{
  if (_root_dirty) {
    p = _root;
  }
  
  if (p) {
    act_error_push (p->getName(), p->getFile(), p->getLine());
  }
  else {
    act_error_push ("-toplevel-", NULL, 0);
  }

  /* -- free the information -- */
  visited_flag = new std::unordered_set<UserDef *> ();
  recursive_op (p, -1);
  delete visited_flag;

  /*-- re-compute --*/
  visited_flag = new std::unordered_set<UserDef *> ();
  recursive_op (p);
  delete visited_flag;
  
  act_error_pop ();
  
  visited_flag = NULL;
}

struct pass_edges {
  int from, to;
};
 
void ActPass::update (Process *p)
{
  /* need to find all forward dependencies, and then do a topological
     sort, and run the passes in that order */
  A_DECL (pass_edges, e);
  A_DECL (ActPass *, x);
  A_DECL (int, xfi);
  
  A_INIT (x);
  A_INIT (e);
  A_INIT (xfi);

  A_APPEND (x, ActPass *, this);
  A_APPEND (xfi, int, 0);

  for (int i=0; i < A_LEN (x); i++) {
    ActPass *tmp = x[i];
    for (listitem_t *li = list_first (tmp->fwdeps); li; li = list_next (li)) {
      ActPass *dep = (ActPass *) list_value (li);
      int to = -1;

      if (!dep->_update_propagate) continue;

      for (int j=0; j < A_LEN (x); j++) {
	if (dep == x[j]) {
	  to = j;
	  break;
	}
      }
      if (to == -1) {
	to = A_LEN (x);

	A_APPEND (x, ActPass *, dep);
	A_APPEND (xfi, int, 0);
      }
      
      pass_edges et;
      et.from = i;
      et.to = to;

      A_APPEND (e, pass_edges, et);
    }
  }

  /*-- we now have the dependency graph --*/
  for (int i=0; i < A_LEN (e); i++) {
    xfi[e[i].to]++;
  }

  list_t *l; // zero count vertices
  l = list_new ();
  for (int i=0; i < A_LEN (x); i++) {
    if (xfi[i] == 0) {
      list_iappend (l, i);
    }
  }

  int count = 0;
  while (!list_isempty (l)) {
    count++;
    int idx = stack_ipop (l);

    /* if the pass has completed then re-run it; if it hasn't been run
       yet we are fine */
    if (x[idx]->completed()) {
      x[idx]->_actual_update (p);
      if (x[idx]->_root_dirty) {
	p = _root;
      }
    }

    /*-- this can be made a lot faster too... --*/
    for (int i=0; i < A_LEN (e); i++) {
      if (e[i].from == idx) {
	xfi[e[i].to]--;
	Assert (xfi[e[i].to] >= 0, "What?");
	if (xfi[e[i].to] == 0) {
	  list_iappend (l, e[i].to);
	}
      }
    }
  }
  list_free (l);

  if (count != A_LEN (x)) {
    fprintf (stderr, "Did not run all forward dependencies. Is there a cycle?\n");
    for (int i=0; i < A_LEN (e); i++) {
      fprintf (stderr, "  %s -> %s\n", x[e[i].from]->name, x[e[i].to]->name);
    }
    exit (1);
  }

  A_FREE (e);
  A_FREE (x);
  A_FREE (xfi);
}
