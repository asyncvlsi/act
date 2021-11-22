/*************************************************************************
 *
 *  Copyright (c) 1996, 2020 Rajit Manohar
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
/*************************************************************************
 *
 *  lispTrace.c -- 
 *
 *   This module manipulates the stack trace information used for
 *   error reporting.
 *
 *************************************************************************
 */
#include <stdio.h>
#include "act/miniscm/lisp.h"
#include "act/miniscm/lispInt.h"


typedef struct stack {
  struct stack *n;
  char *s;
  struct stack *next;
} TRACE;

static TRACE *current = NULL;

static TRACE *freeQ = NULL;

static
TRACE *
StackNew (void)
{
  TRACE *t;
  if (freeQ) {
    t = freeQ;
    freeQ = freeQ->n;
  }
  else {
    NEW (t, TRACE);
  }
  t->n = NULL;
  return t;
}

static
void
StackFree (TRACE *t)
{
  t->n = freeQ;
  freeQ = t;
}
    

/*------------------------------------------------------------------------
 *
 *  LispStackPush --
 *
 *      Push a name onto the call stack.
 *
 *  Results:
 *      none.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

void
LispStackPush (char *name)
{
  TRACE *t;
  t = StackNew();
  t->s = name;
  t->next = current;
  current = t;
}


/*------------------------------------------------------------------------
 *
 *  LispStackPop --
 *
 *      Pop a frame off the evaluation stack.
 *
 *  Results:
 *      none.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

void
LispStackPop (void)
{
  TRACE *t;
  t = current;
  if (!current)
    fprintf (stderr, "Internal error!\n");
  else {
    current = current->next;
    StackFree (t);
  }
}


/*------------------------------------------------------------------------
 *
 *  LispStackDisplay --
 *
 *      Display call stack.
 *
 *  Results:
 *      none.
 *
 *  Side effects:
 *      text appears in window.
 *
 *------------------------------------------------------------------------
 */

void
LispStackDisplay (void)
{
  extern Sexp *LispMainFrame;
  LispObj *l;
  TRACE *t = current;
  int i = 0;
  int depth;
  l = LispFrameLookup (LispNewString ("scm-stack-display-depth"),
		       LispMainFrame);
  if (l && LTYPE(l) == S_INT)
    depth = LINTEGER(l);
  else
    depth = 5;
  fprintf (stderr, "Execution aborted.\n");
  if (depth > 0)
    fprintf (stderr, "Stack trace:\n");
  while (t && i < depth) {
    i++;
    fprintf (stderr, "\tcalled from: %s\n", t->s);
    t = t->next;
  }
  if (i < depth || depth == 0)
    fprintf (stderr, "\tcalled from: -top-level-\n");
}


/*------------------------------------------------------------------------
 *
 *  LispStackClear --
 *
 *      Clear the call stack.
 *
 *  Results:
 *      none.
 *
 *  Side effects:
 *      none.
 *
 *------------------------------------------------------------------------
 */

void
LispStackClear (void)
{
  while (current)
    LispStackPop ();
}
