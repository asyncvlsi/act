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
 *  lispFrame.c -- 
 *
 *   This module contains routines that muck around with frames.
 *
 *************************************************************************
 */

#include <stdio.h>
#include "act/miniscm/lisp.h"
#include "act/miniscm/lispInt.h"

/*------------------------------------------------------------------------
 *
 *   A frame is a list of a list of dotted-pairs.
 *   Each dotted-pair is a ( name . binding ).
 *
 *------------------------------------------------------------------------
 */

Sexp *LispMainFrame = NULL;	/* toplevel frame */
LispObj *LispMainFrameObj = NULL;

/*-----------------------------------------------------------------------------
 *
 *  LispFrameInit --
 *
 *      Initialize top-level frame.
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      modifies LispMainFrame
 *
 *-----------------------------------------------------------------------------
 */

void
LispFrameInit (void)
{
  LispMainFrame = LispNewSexp ();
  CAR(LispMainFrame) = LispNewObj ();
  CDR(LispMainFrame) = LispNewObj ();
  LTYPE(CAR(LispMainFrame)) = S_LIST;
  LTYPE(CDR(LispMainFrame)) = S_LIST;
  LLIST(CAR(LispMainFrame)) = NULL;
  LLIST(CDR(LispMainFrame)) = NULL;

  LispGCAddSexp (LispMainFrame);
}


/*
  returns (name val) thing if exists, else null
*/
static
Sexp *
findbinding (const char *name, Sexp *f)
{
  Sexp *t, *t1;

  t = f;
  while (t) {
    t1 = LLIST(CAR(t));
    while (t1) {
      if (LSYM(CAR(LLIST(CAR(t1)))) == name)
	return LLIST(CAR(t1));
      t1 = LLIST(CDR(t1));
    }
    t = LLIST(CDR(t));
  }
  return NULL;
}



/*
  returns (name val) thing if exists, else null
*/
static
Sexp *
revfindbinding (LispObj *l, Sexp *f)
{
  Sexp *t, *t1;

  t = f;
  while (t) {
    t1 = LLIST(CAR(t));
    while (t1) {
      if (CDR(LLIST(CAR(t1))) == l)
	return LLIST(CAR(t1));
      t1 = LLIST(CDR(t1));
    }
    t = LLIST(CDR(t));
  }
  return NULL;
}


/*------------------------------------------------------------------------
 *
 *  LispFrameLookup --
 *
 *      Lookup symbol in frame. The arguments must be canonicalized using
 *      the LispNewString() function, since string comparisons are done
 *      using pointer equality.
 *
 *  Results:
 *      Returns pointer to symbol if found, NULL otherwise.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

LispObj *
LispFrameLookup (const char *s, Sexp *f)
{
  Sexp *f1;
  f1 = findbinding (s,f);
  if (!f1)
    return NULL;
  else
    return CDR(f1);
}


/*------------------------------------------------------------------------
 *
 *  LispFrameRevLookup --
 *
 *      Lookup object in frame. This returns the symbol corresponding to
 *      the object. Used for debugging purposes.
 *
 *  Results:
 *      Returns symbol if found, NULL otherwise.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

char *
LispFrameRevLookup (LispObj *l, Sexp *f)
{
  Sexp *f1;
  f1 = revfindbinding (l,f);
  if (!f1) 
    return NULL;
  else
    return LSYM(CAR(f1));
}
  


/*------------------------------------------------------------------------
 *
 *  LispAddBinding --
 *
 *      Add a  ( name . value ) binding to the frame.
 *
 *  Results:
 *      modifies the frame.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

void
LispAddBinding (LispObj *name, LispObj *val, Sexp *f)
{
  Sexp *t;
  LispObj *l;

  if (LTYPE(name) != S_SYM) {
    fprintf (stderr, "LispAddBinding: invalid argument!\n");
    return;
  }
  if (f) {
    t = LLIST(CAR(f));
    while (t) {
      if (LSYM(name) == LSYM(CAR(LLIST(CAR(t))))) {
	CDR(LLIST(CAR(t))) = val;
	return;
      }
      t = LLIST(CDR(t));
    }
  }
  t = LispNewSexp ();
  CAR(t) = name;
  CDR(t) = val;
  l = LispNewObj ();
  LTYPE(l) = S_LIST;
  LLIST(l) = t;
  t = LispNewSexp ();
  CAR(t) = l;
  CDR(t) = CAR(f);
  l = LispNewObj ();
  LTYPE(l) = S_LIST;
  LLIST(l) = t;
  CAR(f) = l;
}


/*------------------------------------------------------------------------
 *
 *  LispModifyBinding --
 *
 *      Modify a  ( name . value ) binding from the frame.
 *
 *  Results:
 *      Returns 0 on failure, and non-zero on success.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

int
LispModifyBinding (LispObj *name, LispObj *val, Sexp *f)
{
  Sexp *t;
  LispObj *l;

  if (LTYPE(name) != S_SYM) return 0;
  if ((t = findbinding (LSYM(name),f))) {
    CDR(t) = val;
    return 1;
  }
  return 0;
}

		
/*-----------------------------------------------------------------------------
 *
 *  LispFramePush --
 *
 *      Return a new frame which consists of the old frame augmented by
 *      the empty list.
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Sexp *
LispFramePush (Sexp *f)
{
  Sexp *nf;
  nf = LispNewSexp ();
  CAR(nf) = LispNewObj ();
  CDR(nf) = LispNewObj ();
  LTYPE(CAR(nf)) = S_LIST;
  LLIST(CAR(nf)) = NULL;
  LTYPE(CDR(nf)) = S_LIST;
  LLIST(CDR(nf)) = f;
  return nf;
}
