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
 *  lispString.c --
 *
 *   This module contains the builtin mini-scheme string functions.
 *
 *************************************************************************
 */

#include <stdio.h>
#include <ctype.h>

#include "lisp.h"
#include "lispInt.h"
#include "lispargs.h"


/*-----------------------------------------------------------------------------
 *
 *  LispStrCat --
 *
 *      Concatenate two strings.
 *
 *  Results:
 *      Returns the concatenated string.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispStrCat (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;

  if (!ARG1P(s) || !ARG2P(s) || LTYPE(ARG1(s)) != S_STRING ||
      LTYPE(ARG2(s)) != S_STRING || ARG3P(s)) {
    fprintf (stderr, "Usage: (%s str1 str2)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_STRING;
  MALLOC (LSTR(l), char, strlen(LSTR(ARG1(s)))+strlen(LSTR(ARG2(s)))+1);
  strcpy (LSTR(l),LSTR(ARG1(s)));
  strcat (LSTR(l),LSTR(ARG2(s)));
  return l;
}


/*-----------------------------------------------------------------------------
 *
 *  LispSymbolToString --
 *
 *      Returns the string name for a symbol.
 *
 *  Results:
 *      New string.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispSymbolToString (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  if (!ARG1P(s) || LTYPE(ARG1(s)) != S_SYM || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s symbol)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_STRING;
  LSTR(l) = Strdup (LSYM(ARG1(s)));
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispStringToSymbol --
 *
 *      Symbol named "string"
 *
 *  Results:
 *      The symbol.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispStringToSymbol (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  if (!ARG1P(s) || LTYPE(ARG1(s)) != S_STRING || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s string)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_SYM;
  LSYM(l) = (char *)LispNewString (LSTR(ARG1(s)));
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispNumberToString --
 *
 *      Convert number to string.
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispNumberToString (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  char buf[128];

  if (!ARG1P(s) || !NUMBER(LTYPE(ARG1(s))) || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s num)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_STRING;
  if (LTYPE(ARG1(s)) == S_FLOAT)
    sprintf (buf, "%lf", LFLOAT(ARG1(s)));
  else
    sprintf (buf, "%ld", LINTEGER(ARG1(s)));
  LSTR(l) = Strdup (buf);
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispStringToNumber --
 *
 *      Number named "string"
 *
 *  Results:
 *      The number.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispStringToNumber (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  char *str;
  int r;

  if (!ARG1P(s) || LTYPE(ARG1(s)) != S_STRING || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s string)\n", name);
    RETURN;
  }
  str = LSTR(ARG1(s));
  l = LispNewObj ();
  if (isdigit(*str) || *str == '.' || *str == '-' || *str == '+') {
    r = 0;
    if (*str == '-' || *str == '+')
      str++;
    if (!*str) {
      fprintf (stderr, "String is not a number.\n");
      RETURN;
    }
    while (*str && isdigit (*str))
      str++;
    if (*str && *str == '.') {
      r = 1;
      str++;
    }
    while (*str && isdigit(*str))
      str++;
    *str = '\0';
    if (r) {
      LTYPE(l) = S_FLOAT;
      sscanf (LSTR(ARG1(s)), "%lf", &LFLOAT(l));
    }
    else {
      LTYPE(l) = S_INT;
      sscanf (LSTR(ARG1(s)), "%ld", &LINTEGER(l));
    }
  }
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispStringLength --
 *
 *      Compute length of string.
 *
 *  Results:
 *      Returns length.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispStringLength (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  if (!ARG1P(s) || LTYPE(ARG1(s)) != S_STRING || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s string)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_INT;
  LINTEGER(l) = strlen (LSTR(ARG1(s)));
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispStringCompare --
 *
 *      Compare two strings.
 *
 *  Results:
 *      An integer.
 *      0    => str1 == str2
 *      (>0) => str1 > str2
 *      (<0) => str1 < str2
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispStringCompare (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  if (!ARG1P(s) || !ARG2P(s) || LTYPE(ARG1(s)) != S_STRING ||
      LTYPE(ARG2(s)) != S_STRING || ARG3P(s)) {
    fprintf (stderr, "Usage: (%s str1 str2)\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_INT;
  LINTEGER(l) = strcmp (LSTR(ARG1(s)),LSTR(ARG2(s)));
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispStringRef --
 *
 *      Return character k from a string.
 *
 *  Results:
 *      An integer.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispStringRef (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  if (!ARG1P(s) || !ARG2P(s) || LTYPE(ARG1(s)) != S_STRING ||
      LTYPE(ARG2(s)) != S_INT || ARG3P(s)) {
    fprintf (stderr, "Usage: (%s str int)\n", name);
    RETURN;
  }
  if (strlen (LSTR(ARG1(s))) <= LINTEGER(ARG2(s)) || LINTEGER(ARG2(s)) < 0) {
    fprintf (stderr, "%s: integer argument out of range.\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_INT;
  LINTEGER(l) = LSTR(ARG1(s))[LINTEGER(ARG2(s))];
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispStringSet --
 *
 *      Set kth string character to the appropriate integer.
 *
 *  Results:
 *      boolean.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispStringSet (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;

  if (!ARG1P(s) || !ARG2P(s) || !ARG3P(s) ||
      LTYPE(ARG1(s)) != S_STRING || LTYPE(ARG2(s)) != S_INT ||
      LTYPE(ARG3(s)) != S_INT || ARG4P(s)) {
    fprintf (stderr, "Usage: (%s str int int)\n", name);
    RETURN;
  }
  if (strlen (LSTR(ARG1(s))) <= LINTEGER(ARG2(s)) || LINTEGER(ARG2(s)) < 0) {
    fprintf (stderr, "%s: integer argument out of range.\n", name);
    RETURN;
  }
  l = LispNewObj();
  LSTR(ARG1(s))[LINTEGER(ARG2(s))] = LINTEGER(ARG3(s));
  LTYPE(l) = S_BOOL;
  LBOOL(l) = 1;
  return l;
}


/*-----------------------------------------------------------------------------
 *
 *  LispSubString --
 *
 *      Return a substring from a string.
 *
 *  Results:
 *      String.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispSubString (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;

  if (!ARG1P(s) || !ARG2P(s) || !ARG3P(s) ||
      LTYPE(ARG1(s)) != S_STRING || LTYPE(ARG2(s)) != S_INT ||
      LTYPE(ARG3(s)) != S_INT || ARG4P(s)) {
    fprintf (stderr, "Usage: (%s str int int)\n", name);
    RETURN;
  }
  if (!(0 <= LINTEGER(ARG2(s)) && LINTEGER(ARG2(s)) <= LINTEGER(ARG3(s)) &&
	LINTEGER(ARG3(s)) <= strlen(LSTR(ARG1(s))))) {
    fprintf (stderr, "%s: integer argument out of range.\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_STRING;
  MALLOC (LSTR (l), char, (LINTEGER(ARG3(s))-LINTEGER(ARG2(s))+1));
  strncpy (LSTR(l), LSTR(ARG1(s))+LINTEGER(ARG2(s)), 
	   LINTEGER(ARG3(s))-LINTEGER(ARG2(s)));
  LSTR(l)[LINTEGER(ARG3(s))-LINTEGER(ARG2(s))] = '\0';
  return l;
}
