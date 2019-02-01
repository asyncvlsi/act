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
#ifndef __THREAD_H__
#define __THREAD_H__

#include "contexts.h"
#include "mytime.h"

#ifdef __cplusplus
extern "C" {
#endif

struct process_record {
  context_t c;
  int sz;			/* size of stack */
  int tid;
  const char *name;
  const char *file;
  int line;
  int exit_code;
  Time_t time;			/* thread local time */
  void *cdata1;			/* this space for rent */
  void *cdata2;			/* more space for rent */
  int color;			/* odd/even queue setup */
  int in_readyq;		/* 1 if in the readyq */
  struct process_record *next;
  double s[(DEFAULT_STACK_SIZE+sizeof(double)-1)/sizeof(double)]; 
         /* stack; this crazy construct is to make the "s" field aligned
	    properly on a sparc
         */
};

typedef struct process_record lthread_t;

lthread_t *_thread_new (void (*f)(void), int stksz, const char *name, int
		       ready, char *file, int line);

#define thread_new(x,y)  _thread_new(x,y,NULL,1,__FILE__,__LINE__)
  /* create new thread */

#define thread_named_new(x,y,nm) _thread_new(x,y,nm,1,__FILE__,__LINE__)
  /* create new thread with a specified name */

void thread_idle (void);
  /* voluntary context switch */

void thread_exit (int);
  /* quit thread */

const char *thread_name (void);
  /* returns the name of the current thread if the thread was named,
     otherwise "-unknown-"
  */

void thread_printf (char *msg, ...);
  /* prints [thread id (name) file and line number] before the rest of 
     the message
  */

void thread_print_name (lthread_t *);
  /* prints [thread id (name) file and line number] without a newline
   */

void thread_pause (int delay);
  /* suspend current process for some approximate amount of time */

#define lthread_self ((lthread_t*)current_process)

extern void (*thread_cleanup)(void);


void simulate (void (*f)(void));
  /* call as the last thing from the main process to begin simulation */

  /* thread timing functions */
void Delay (Time_t);
Time_t CurrentTime ();

int thread_id (void);

  /* thread save/restore functions */
void thread_write (FILE *fp, lthread_t *t, int save_ctxt);
void thread_read (FILE *fp, lthread_t *t, int save_ctxt);  

#ifdef __cplusplus
}
#endif

#endif /* __THREAD_H__ */
