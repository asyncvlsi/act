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
 *  lispArith.c --
 *
 *   This module contains the builtin mini-scheme arithmetic
 *   functions.
 *
 *************************************************************************
 */

#include <stdio.h>
#include "lisp.h"
#include "lispInt.h"
#include "lispargs.h"


/*-----------------------------------------------------------------------------
 *
 *  LispAdd --
 *
 *      "+"
 *
 *  Results:
 *      Returns the sum of two arguments.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispAdd (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  double d;

  if (!ARG1P(s) || !ARG2P(s) || !NUMBER(LTYPE(ARG1(s))) ||
      !NUMBER(LTYPE(ARG2(s))) || ARG3P(s)) {
    fprintf (stderr, "Usage: (%s num1 num2)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  if (LTYPE(ARG1(s)) == S_FLOAT || LTYPE(ARG2(s)) == S_FLOAT) {
    LTYPE(l) = S_FLOAT;
    d = LTYPE(ARG1(s)) == S_FLOAT ? LFLOAT(ARG1(s)) : LINTEGER(ARG1(s));
    d+= LTYPE(ARG2(s)) == S_FLOAT ? LFLOAT(ARG2(s)) : LINTEGER(ARG2(s));
    LFLOAT(l) = d;
  }
  else {
    LTYPE(l) = S_INT;
    LINTEGER(l) = LINTEGER(ARG1(s))+LINTEGER(ARG2(s));
  }
  return l;
}


/*-----------------------------------------------------------------------------
 *
 *  LispSub --
 *
 *      "-"
 *
 *  Results:
 *      Returns the difference of two arguments.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispSub (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  double d;

  if (!ARG1P(s) || !ARG2P(s) || !NUMBER(LTYPE(ARG1(s))) ||
      !NUMBER(LTYPE(ARG2(s))) || ARG3P(s)) {
    fprintf (stderr, "Usage: (%s num1 num2)\n", name);
    RETURN;
  }
  if (!NUMBER(LTYPE(ARG1(s))) || !NUMBER(LTYPE(ARG2(s)))) {
    fprintf (stderr, "%s: requires two numbers\n", name);
    RETURN;
  }
  l = LispNewObj ();
  if (LTYPE(ARG1(s)) == S_FLOAT || LTYPE(ARG2(s)) == S_FLOAT) {
    LTYPE(l) = S_FLOAT;
    d = LTYPE(ARG1(s)) == S_FLOAT ? LFLOAT(ARG1(s)) : LINTEGER(ARG1(s));
    d-= LTYPE(ARG2(s)) == S_FLOAT ? LFLOAT(ARG2(s)) : LINTEGER(ARG2(s));
    LFLOAT(l) = d;
  }
  else {
    LTYPE(l) = S_INT;
    LINTEGER(l) = LINTEGER(ARG1(s))-LINTEGER(ARG2(s));
  }
  return l;
}


/*-----------------------------------------------------------------------------
 *
 *  LispMult --
 *
 *      "*"
 *
 *  Results:
 *      Returns the product of two arguments.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispMult (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  double d;

  if (!ARG1P(s) || !ARG2P(s) || !NUMBER(LTYPE(ARG1(s))) ||
      !NUMBER(LTYPE(ARG2(s))) || ARG3P(s)) {
    fprintf (stderr, "Usage: (%s num1 num2)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  if (LTYPE(ARG1(s)) == S_FLOAT || LTYPE(ARG2(s)) == S_FLOAT) {
    d = LTYPE(ARG1(s)) == S_FLOAT ? LFLOAT(ARG1(s)) : LINTEGER(ARG1(s));
    d *= (LTYPE(ARG2(s)) == S_FLOAT ? LFLOAT(ARG2(s)) : LINTEGER(ARG2(s)));
    LTYPE(l) = S_FLOAT;
    LFLOAT(l) = d;
  }
  else {
    LTYPE(l) = S_INT;
    LINTEGER(l) = LINTEGER(ARG1(s))*LINTEGER(ARG2(s));
  }
  return l;
}

/*-----------------------------------------------------------------------------
 *
 *  LispDiv --
 *
 *      "/"
 *
 *  Results:
 *      Returns the quotient of two arguments.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispDiv (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  double d;

  if (!ARG1P(s) || !ARG2P(s) || !NUMBER(LTYPE(ARG1(s))) ||
      !NUMBER(LTYPE(ARG2(s))) || ARG3P(s)) {
    fprintf (stderr, "Usage: (%s num1 num2)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  if (LTYPE(ARG1(s)) == S_FLOAT || LTYPE(ARG2(s)) == S_FLOAT) {
    LTYPE(l) = S_FLOAT;
    d = LTYPE(ARG1(s)) == S_FLOAT ? LFLOAT(ARG1(s)) : LINTEGER(ARG1(s));
    if ((LTYPE(ARG2(s)) == S_FLOAT && LFLOAT(ARG2(s)) == 0) ||
	(LTYPE(ARG2(s)) == S_INT && LINTEGER(ARG2(s)) == 0)) {
      fprintf (stderr, "Division by zero\n");
      RETURN;
    }
    d /= LTYPE(ARG2(s)) == S_FLOAT ? LFLOAT(ARG2(s)) : LINTEGER(ARG2(s));
    LFLOAT(l) = d;
  }
  else {
    if (LINTEGER(ARG2(s)) == 0) {
      fprintf (stderr, "Division by zero\n");
      RETURN;
    }
    d = (double)LINTEGER(ARG1(s))/(double)LINTEGER(ARG2(s));
    if (d == ((int)d)) {
      LTYPE(l) = S_INT;
      LINTEGER(l) = (int)d;
    }
    else {
      LTYPE(l) = S_FLOAT;
      LFLOAT(l) = d;
    }
  }
  return l;
}


/*-----------------------------------------------------------------------------
 *
 *  LispTruncate --
 *
 *      Truncate a number.
 *
 *  Results:
 *      Returns an integer.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispTruncate (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  double d;

  if (!ARG1P(s) || !NUMBER(LTYPE(ARG1(s))) || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s num)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_INT;
  if (LTYPE(ARG1(s)) == S_FLOAT) 
    LINTEGER(l) = (int)LFLOAT(ARG1(s));
  else
    LBOOL(l) = LINTEGER(ARG1(s));
  return l;
}


/*-----------------------------------------------------------------------------
 *
 *  LispZeroQ --
 *
 *      Checks if argument is zero.
 *
 *  Results:
 *      Returns a boolean.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispZeroQ (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  double d;

  if (!ARG1P(s) || !NUMBER(LTYPE(ARG1(s))) || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s num)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_BOOL;
  if (LTYPE(ARG1(s)) == S_FLOAT) 
    LBOOL(l) = LFLOAT(ARG1(s)) == 0 ? 1 : 0;
  else
    LBOOL(l) = LINTEGER(ARG1(s)) == 0 ? 1 : 0;
  return l;
}


/*-----------------------------------------------------------------------------
 *
 *  LispPositiveQ --
 *
 *      Checks if argument is positive.
 *
 *  Results:
 *      Returns a boolean.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispPositiveQ (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  double d;

  if (!ARG1P(s) || !NUMBER(LTYPE(ARG1(s))) || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s num)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_BOOL;
  if (LTYPE(ARG1(s)) == S_FLOAT) 
    LBOOL(l) = LFLOAT(ARG1(s)) > 0 ? 1 : 0;
  else
    LBOOL(l) = LINTEGER(ARG1(s)) > 0 ? 1 : 0;
  return l;
}


/*-----------------------------------------------------------------------------
 *
 *  LispNegativeQ --
 *
 *      Checks if argument is negative.
 *
 *  Results:
 *      Returns a boolean.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispNegativeQ (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  double d;

  if (!ARG1P(s) || !NUMBER(LTYPE(ARG1(s))) || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s num)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_BOOL;
  if (LTYPE(ARG1(s)) == S_FLOAT) 
    LBOOL(l) = LFLOAT(ARG1(s)) < 0 ? 1 : 0;
  else
    LBOOL(l) = LINTEGER(ARG1(s)) < 0 ? 1 : 0;
  return l;
}
