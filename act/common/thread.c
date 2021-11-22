/*************************************************************************
 *
 *  Thread library
 *
 *  Copyright (c) 1997-99, 2019 Rajit Manohar
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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "act/common/thread.h"
#include "act/common/qops.h"

#define DEBUG_MODE

/* the ready queue */
lthread_t *readyQh = NULL;
lthread_t *readyQt = NULL;

#ifndef THREAD_FAST_ALLOC
#define THREAD_FAST_ALLOC  100	/* a bunch of statically allocated threads */
#endif

static lthread_t *thread_freeq = NULL;
static lthread_t THREADS[THREAD_FAST_ALLOC];
void (*thread_cleanup)(void) = NULL;

static Time_t inconsistent_timer = 0;

lthread_t *timerQh = NULL;
lthread_t *timerQt = NULL;

/*------------------------------------------------------------------------
 *
 *   Lazy timer: guaranteed to go off just when some process in the
 *   system exceeds this time when it gets context switched out.
 *
 *------------------------------------------------------------------------
 */
static void thread_insert_timer (lthread_t *t, Time_t tm)
{
  lthread_t *x;

  if (t->time >= tm) return;

  t->time = tm;

  for (x = timerQh; x; x = x->next) {
    if (x->time > tm)
      break;
  }
  if (!x) {
    if (timerQh) {
      timerQt->next = t;
    }
    else {
      timerQh = t;
    }
    timerQt = t;
  }
  else {
    t->next = x->next;
    x->next = t;
    if (!t->next)
      timerQt = t;
  }
}

void thread_pause (int delay)
{
  context_disable ();
  if (current_process) {
    thread_insert_timer (current_process, time_add (delay,
						    current_process->time));
  }
  context_switch (context_select ());
}

/*------------------------------------------------------------------------
 *
 * Interface to context library
 *
 *------------------------------------------------------------------------
 */

/* pre: interrupts disabled */
void thread_print_name (lthread_t *t)
{
#ifdef DEBUG_MODE
  if (!t) t = current_process;

  if (t) {
    if (t->file) {
      if (t->name)
	printf ("[Thread %2d (%s), initiated in file `%s', line %d] ",
		t->tid, t->name, t->file, t->line);
      else
	printf ("[Thread %2d, initiated in file `%s', line %d] ",
		t->tid, t->file, t->line);
    }
    else {
      if (t->name)
	printf ("[Thread %2d (%s)] ", t->tid, t->name);
      else
	printf ("[Thread %2d] ", t->tid);
    }
  }
  else {
    printf ("[Main thread] ");
  }
#else
  if (t)
    printf ("[Thread %2d] ", t->tid);  
  else
    printf ("[Main thread] ");
#endif
}

/* print message */
/* pre: interrupts disabled */
void thread_printf (char *msg, ...)
{
  va_list ap;
  
  thread_print_name (NULL);
  va_start (ap, msg);
  vfprintf (stdout, msg, ap);
  va_end (ap);
}

/* pre: interrupts disabled */
void context_destroy (lthread_t *t)
{
#ifdef DEBUG_MODE
#ifdef DEBUG_MODE2
  thread_print_name (t);
  printf ("EXIT CODE: %d\n", t->exit_code);
#endif
  if (t->name) free ((void *)t->name);
  if (t->file) free ((void *)t->file);
#endif /* DEBUG_MODE */
  if (t->c.sz == DEFAULT_STACK_SIZE) {
    t->next = thread_freeq;
    thread_freeq = t;
  }
  else
    free (t);
}


/* pre: interrupts disabled */


/* pre: interrupts disabled */
void context_timeout (void)
{
  process_t *p;

  if (current_process != NULL) {
    current_process->in_readyq = 1;
    q_ins (readyQh, readyQt, current_process);
    p = context_select ();
    context_switch (p);
  }
  else
    context_enable();
}

lthread_t* context_select (void)
{
  lthread_t *t, *x;

#ifdef CLASS_HACKERY_NONDET
  if (!readyQh) {
    ch_update_readyQ ();
  }
#endif
  q_del (readyQh, readyQt, t);
  if (!t) {
    context_cleanup ();
    if (thread_cleanup) (*thread_cleanup)();
#ifdef DEBUG_MODE2
    printf("\nNo more processes.\n");
#endif /* DEBUG_MODE2 */
    exit (0);
  }
  t->in_readyq = 0;
  inconsistent_timer = time_max (inconsistent_timer, t->time);
  while (timerQh && inconsistent_timer >= timerQh->time) {
    q_del (timerQh, timerQl, x);
    x->in_readyq = 1;
    q_ins (readyQh, readyQt, x);
  }
  return t;
}


/*------------------------------------------------------------------------
 *
 *  _thread_new --
 *
 *     Create a new thread with the specified stack size
 *
 *------------------------------------------------------------------------
 */
lthread_t *
_thread_new (void (*f)(void), int stksz, const char *name, int ready, 
	     char *file, int line)
{
  static int tid = 0;
  lthread_t *t;

  context_disable ();
  if (tid == 0) {
    int i;
    for (i=0; i < THREAD_FAST_ALLOC; i++) {
      THREADS[i].next = thread_freeq;
      thread_freeq = THREADS+i;
    }
  }
  if (stksz == 0 || stksz == DEFAULT_STACK_SIZE) {
    stksz = DEFAULT_STACK_SIZE;
    if (thread_freeq) {
      t = thread_freeq;
      thread_freeq = thread_freeq->next;
    }
    else
      t = (lthread_t*)malloc(sizeof(lthread_t));
  }
  else
    t = (lthread_t*)malloc(sizeof(lthread_t)-DEFAULT_STACK_SIZE+stksz);
  if (!t) {
    printf ("Thread allocation failed, stack size=%d\n",stksz);
    exit (1);
  }
  t->sz = stksz;
  t->c.stack = (char*) t->s;
  t->c.sz = stksz;
  t->tid = tid++;
  t->line = line;
  t->exit_code = 0;
  t->in_readyq = 0;
  time_init (t->time);

  /*#ifdef DEBUG_MODE*/
  if (file) {
    char *tmp = (char*)malloc(1+strlen(file));
    t->file = tmp;
    if (!t->file) {
      printf ("MALLOC failed\n");
      exit (1);
    }
    strcpy (tmp, file);
  }
  else {
    t->file = NULL;
  }
  if (name) {
    char *tmp = (char*)malloc(1+strlen(name));
    t->name = tmp;
    if (!t->name) {
      printf ("MALLOC failed\n");
      exit (1);
    }
    strcpy (tmp, name);
  }
  else
    t->name = NULL;
  /*#else  !DEBUG_MODE */
  /*t->name = NULL;*/
  /*#endif  DEBUG_MODE */

  context_init (t, f);
  if (ready) {
    t->in_readyq = 1;
    q_ins (readyQh, readyQt, t);
  }
  context_enable ();
  return t;
}


/* quit thread */
void thread_exit (int code)
{
  context_disable ();
  if (current_process)
    current_process->exit_code = code;
  context_exit ();
}

/* voluntary context switch */
void thread_idle (void)
{
  context_disable ();
  context_timeout ();
}

/* return name of the thread */
const char *thread_name (void)
{
  return ((current_process) ? ((current_process)->name ? (current_process)->name : "-unknown-") : "-Main-thread-");
}

/* simulate function */
void simulate (void (*f)(void))
{
  thread_cleanup = f;
  thread_exit (0);
}

/*
 *
 *  Time stuff
 *
 */
void Delay (Time_t d)
{
  lthread_t *t;

  t = (lthread_t*)current_process;
  time_inc (t->time, d);
}

Time_t CurrentTime (void)
{
  lthread_t *t;

  t = (lthread_t*)current_process;
  if (!t) return 0;
  return t->time;
}

int thread_id (void)
{
  lthread_t *t = (lthread_t*)current_process;
  return t->tid;
}

/*------------------------------------------------------------------------
 *
 *  Save/restore thread state
 *
 *------------------------------------------------------------------------
 */
void thread_write (FILE *fp, lthread_t *t, int save_ctxt) { }
void thread_read (FILE *fp, lthread_t *t, int save_ctxt) { }
