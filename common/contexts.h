/*************************************************************************
 *
 *  Context switch routines (user-level threading).
 *
 *  Copyright (c) 1998, 1999, 2019 Rajit Manohar
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
#ifndef __CONTEXTS_H__
#define __CONTEXTS_H__

#include <stdlib.h>
#include <setjmp.h>
#include <sys/time.h>
#include <stdio.h>

#ifndef DEFAULT_STACK_SIZE
#ifdef SYNCHRONOUS
#define DEFAULT_STACK_SIZE 0x2000
#else
#define DEFAULT_STACK_SIZE 0x8000
#endif
#define LARGE_STACK_SIZE (0x1000 * 16)
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*
 * A process record. The first field of a process record must be
 * a context.
 *
 * The user must provide:
 *
 *       struct process_record {
 *             context_t c;        // The c.stack field must be initialized
 *             ...
 *       };
 *
 */
typedef struct process_record process_t;


/*
 * A context consists of the state of the process, which is the
 * value of all registers (including pc) and the execution stack.
 *
 * For implementation purposes, we need the start address of the process
 * so that we can handle termination/invocation in a clean manner.
 *
 */
typedef struct {
  jmp_buf buf;			/* state  */
  char *stack;			/* stack  */
  int sz;			/* stack size */
  void (*start) ();		/* entry point */
/*#ifdef FAIR*/
  int in_cs;			/* in critical section */
  int pending;			/* interrupt pending */
  int interrupted;              /* have you been interrupted lately? */
/*#endif*/
} context_t;


/*
 * At any given instant, "current_process" points to the process record
 * for the currently executing thread of control.
 */
extern process_t *current_process;


/*
 * Under the timed scheduling option, the following user-defined routine
 * will be called when the time slice for a particular process is over.
 *
 * This routine is called with interrupts disabled, and should enable
 * interrupts on exit (usually by performing a context switch)
 *
 * (Provided by user)
 */
extern void context_timeout (void);


/*
 * The following routine returns the next process to be executed.
 *
 * (Provided by user)
 */
extern process_t *context_select (void);


/*
 * Disable/enable timer interrupts.
 */
#ifdef FAIR
extern void context_disable (void);
extern void context_enable (void);
#else
#define context_disable() do { } while (0)
#define context_enable()  do { } while (0)
#endif


/*
 * Perform a context switch. This routine must be called with interrupts
 * disabled. It re-enables interrupts.
 */
extern void context_switch (process_t *);

/*
 *
 *  Call once before terminating computation. It must be called with
 *  interrupts disabled.
 *
 */
extern void context_cleanup (void);


/*
 * Initialize the context field of the process record.
 * This procedure must be called with p->c.stack initialized.
 */
extern void context_init (process_t *, void (*f)(void));


/*
 * Unfair scheduling
 */
extern void context_unfair (void);

/*
 * Fair scheduling: can be used to undo the first unfair call
 */
extern void context_fair (void);

/*
 *  Exit
 */
extern void context_exit (void);

/*
 * Set time slice to that of process p: must be called with interrupts
 * disabled
 */
extern void context_set_timer (process_t *p);

/*
 * User must provide a function that deletes the process record.
 */
extern void context_destroy (process_t *);

/*
 *  Save context to file. Modifies stack state. NEVER call this function
 *  when p == current_process.
 *  Call only with interrupts disabled.
 */
extern void context_write (FILE *fp, process_t *p);

/*
 *  Read context from file. Modifies stack state. NEVER call this function
 *  when p == current_process.
 *  Call only with interrupts disabled.
 */
extern void context_read (FILE *fp, process_t *p);



#ifdef __cplusplus
}
#endif

#endif /* __CONTEXTS_H__ */
