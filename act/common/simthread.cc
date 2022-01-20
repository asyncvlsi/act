/*************************************************************************
 *
 *  Threaded simulation objects
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
#include "simthread.h"

/*
 *  Create a simulation object task! This is very cool.
 *
 */
void SimCreateTask (SimThread *s, const char *name, int sz)
{
  lthread_t *t;
  
  if (!s) fatal_error ("SimCreateTask:: NULL simulation object!");

  t = _thread_new (SimThread::Thread_Stub, sz, name, s->_ready, NULL, 0);
  t->cdata2 = s;
  s->t = t;
}

void SimThread::Thread_Stub (void)
{
  SimThread *s;

  s = (SimThread *) current_process->cdata2; // trust me, this is right. :)

  /* run! */
  s->MainLoop();

  /* finished up */
  /* we need to dump statistics */
  context_disable ();
  s->DumpStats ();
  delete s;
  context_enable ();
  thread_exit (0);
}

const char *SimThread::Name (void)
{
  if (!t) return "-null-";
  return t->name;
}

/* create and destroy */
SimThread::SimThread (int ready) : Sim(ready)
{
  /* nothing to be done here! */
}

SimThread::~SimThread ()  
{
  /* nothing, default */
}
