/*************************************************************************
 *
 *  Simulation objects
 *
 *  Copyright (c) 1999, 2019 Rajit Manohar
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
#include "sim.h"

/* globals for sim object */
int SimObject::Number = 0;
SimObject *SimObject::obj_list = NULL;

/*
 *  Create a simulation object task! This is very cool.
 *
 */
#if defined(__OMIT_DEFAULT_PARAMS__)
void SimCreateTask (SimObject *s, const char *name, int sz)
#else
void SimCreateTask (SimObject *s, const char *name, int sz)
#endif
{
#if defined (ASYNCHRONOUS)
  lthread_t *t;
#elif defined (SYNCHRONOUS)
  task *t;
#endif
  
  if (!s) fatal_error ("SimCreateTask:: NULL simulation object!");

#if defined (ASYNCHRONOUS)
  t = _thread_new (SimObject::Thread_Stub, sz, name, s->_ready, NULL, 0);
#elif defined (SYNCHRONOUS)
  t = create_task (SimObject::Thread_Stub, sz, name);
  if (!s->_ready)
    remove_last_task (t);
#endif
  t->cdata2 = s;
  s->t = t;
}

void SimObject::Thread_Stub (void)
{
  SimObject *s;


#if defined (ASYNCHRONOUS)
  s = (SimObject *) current_process->cdata2; // trust me, this is right. :)
#elif defined (SYNCHRONOUS)
  s = (SimObject *) curtask->cdata2; // trust me, this is right. :)
#endif
  s->MainLoop();		// ohmigosh!
  context_disable ();
  s->DumpStats ();
  delete s;
  context_enable ();
#if defined (ASYNCHRONOUS)
  /* here, we need to dump statistics */
  thread_exit (0);
#endif
}


/* create and destroy */
SimObject::SimObject (int ready) 
{
  my_id = Number++;		/* assign unique id */
  _ready = ready;

  /* insert into simulation object global list */
  if (!obj_list) {
    obj_list = this;
    next = prev = NULL;
  }
  else {
    prev = NULL;
    next = obj_list;
    obj_list->prev = this;
    obj_list = this;
  }
}

SimObject::SimObject (void)
{
  my_id = Number++;		/* assign unique id */
  _ready = 1;

  /* insert into simulation object global list */
  if (!obj_list) {
    obj_list = this;
    next = prev = NULL;
  }
  else {
    prev = NULL;
    next = obj_list;
    obj_list->prev = this;
    obj_list = this;
  }
}


/* this is for the checkpointing stuff */
void SimObject::UndoNumbering (void)
{
  Number--;
  my_id = -1;
}


SimObject::~SimObject ()  
{
  /* delete myself from object list */
  Assert (obj_list, "something terrible happened!");
  if (obj_list == this) {
    Assert (prev == NULL, "obj_list == current and I'm not the head?");
    if (obj_list->next)
      obj_list->next->prev = prev;
    obj_list = obj_list->next;
  }
  else {
    Assert (prev, "not at head of list, prev == NULL?");
    prev->next = next;
    if (next)
      next->prev = prev;
  }
}


int SimObject::SimCheckDeadlock (void)
{
  SimObject *s;
  if (obj_list == NULL) return 0;

  for (s = obj_list; s; s = s->next)
    if (s->_ready == 1) break;

  if (s == NULL) return 0;

  printf ("The following tasks are suspended:\n");
  for (s = obj_list; s; s = s->next) {
    if (s->_ready == 0) continue;
#if defined (ASYNCHRONOUS)
    current_process = s->t;
    if (current_process)
      printf ("\t%s\n", thread_name ());
    else
      printf ("\tNULL\n");
#elif defined (SYNCHRONOUS)
    curtask = s->t;
    if (curtask)
      printf ("\t%s\n", get_tname ());
    else
      printf ("\tNULL\n");
#endif
  }
  return 1;
}


void SimObject::SimDeadlockedDumpStats (void)
{
  SimObject *s;
  if (obj_list == NULL) return;

  for (s = obj_list; s; s = s->next)
    if (s->_ready == 1) break;

  if (s == NULL) return;

  for (s = obj_list; s; s = s->next) {
    if (s->_ready == 0) continue;
#if defined (ASYNCHRONOUS)
    current_process = s->t;
#else
    curtask = s->t;
#endif
    s->DumpStats ();
  }
}
