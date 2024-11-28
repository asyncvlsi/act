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
 *  lispParse.c -- 
 *
 *   This module contains the mini-scheme command-line parser (ugh).
 *
 *************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <common/mstring.h>

#include "lisp.h"
#include "lispInt.h"

#define IsSpace(c)        ((c) == ' ')

#define BeginIdChar(c)    (isalpha(c) || (c) == '+' || (c) == '-' || \
			   (c) == '.' || (c) == '*' || (c) == '/' || \
			   (c) == '<' || (c) == '=' || (c) == '>' || \
			   (c) == '!' || (c) == '?' || (c) == ':' || \
			   (c) == '$' || (c) == '%' || (c) == '_' || \
			   (c) == '&' || (c) == '~' || (c) == '^' || \
			   (c) == '#' || (c) == '@' || (c) == ',')

#define IsIdChar(c)   (BeginIdChar(c) || isdigit(c) || ((c) == '[') || \
		       ((c) == ']'))

#define ISEND(c)   ((c) == '\0' || isspace(c) || (c) == ')' || (c) == '(')

/*-----------------------------------------------------------------------------
 *
 * Various string munging functions
 *
 *-----------------------------------------------------------------------------
 */

/* 
   strip whitespace from left: returns new string pointer
*/
static char *
stripleft (char *s)
{
  while (*s && IsSpace (*s))
    s++;
  return s;
}

/*-----------------------------------------------------------------------------
 * 
 *  LispNewString --
 *
 *    Returns a unique string pointer corresponding to string "s"
 *
 *-----------------------------------------------------------------------------
 */
const char *LispNewString (char *s)
{
  return string_cache (s);
}

/*-----------------------------------------------------------------------------
 *
 *  LispAtomParse --
 *
 *      Parse an atom.
 *      If within a quote, 'quoted' is 1.
 *
 *  Results:
 *      Returns pointer to an object.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispAtomParse (char **pstr, int quoted)
{
  char *str = *pstr;
  char *q, c;
  LispObj *l;
  int r;
  char *t;

  l = LispNewObj ();

  if ((*str == '+' || *str == '-' || *str == '.') && ISEND(*(str+1))) {
    str++;
    *str = '\0';
    LTYPE(l) = S_SYM;
    LSYM(l) = (char *)LispNewString (str-1);
    *pstr = str+1;
    return l;
  }
  if ((isdigit(*str) || *str == '.' || *str == '-' || *str == '+') &&
      (isdigit(str[1]) || str[1] == '.' || str[1] == '-' || str[1] == '+' 
       || ISEND(str[1]))) {
    /* eat leading sign */
    q = str;
    r = 0;
    if (*str == '-' || *str == '+')
      str++;
    if (!*str) {
      fprintf (stderr, "Invalid number\n");
      *pstr = str;
      return NULL;
    }
    while (*str && isdigit (*str))
      str++;
    if (*str && *str == '.') {
      r = 1;
      str++;
    }
    while (*str && isdigit(*str))
      str++;
    c = *str;
    *str = '\0';
    if (r) {
      LTYPE(l) = S_FLOAT;
      sscanf (q, "%lf", &LFLOAT(l));
    }
    else {
      LTYPE(l) = S_INT;
      sscanf (q, "%ld", &LINTEGER(l));
    }
    *str = c;
  }
  else if ((*str == '0') && ((str[1] == 'b') || (str[1] == 'x'))) {
    /* eat 0[x,b] */
    q = str;
    str++;
    char base = *str;  // can only be 'b' or 'x'
    str++;
    if (!*str) {
      fprintf (stderr, "Invalid number\n");
      *pstr = str;
      return NULL;
    }
    if (base == 'b') {
      while (*str && (*str == '0' || *str == '1'))
        str++;
    }
    else {
      while (*str && isxdigit (*str))
        str++;
    }
    c = *str;
    *str = '\0';
    LTYPE(l) = S_INT;
    if (base == 'b') {
      q++;
      q++;
      LINTEGER(l) = strtol(q, NULL, 2);
    }
    else {
      sscanf (q, "%lx", &LINTEGER(l));
    }
    *str = c;
  }
  else if (*str == '\"') {
    str++;
    q = str;
    while (*str != '\"') {
      if (!*str) {
	fprintf (stderr, "Unterminated string\n");
	*pstr = str;
	return NULL;
      }
      if (*str == '\\') {
	if (*(str+1))
	  str++;
	else {
	  fprintf (stderr, "Trailing character constant\n");
	  *pstr = str;
	  return NULL;
	}
      }
      str++;
    }
    *str = '\0';
    LTYPE(l) = S_STRING;
    LSTR(l) = Strdup (q);
    *str = '\"';
    str++;
  }
  else if (!quoted && str[0] == '#' && (str[1] == 't'|| str[1] == 'f') && 
	   ISEND(str[2])) {
    LTYPE(l) = S_BOOL;
    LBOOL(l) = (str[1] == 't') ? 1 : 0;
    str+=2;
  }
  else if (BeginIdChar(*str) || (*str == '\\')) {
    int nest;

    LTYPE(l) = S_SYM;
    t = q = str;
    if (*str == '\\') {
      *t = *++str;
      t++;
      str++;
    }
    nest = 0;
    while (*str && (IsIdChar (*str) || *str == '(' || (*str == ')' && nest>0))) {
      *t = *str;
      if (*str == '(') nest++;
      if (*str == ')') nest--;
      str++;
      if (*str == '\\') {
	*t = *++str;
	str++;
      }
      t++;
    }
    c = *t;
    *t = '\0';
    LSYM(l) = (char *)LispNewString (q);
    *t = c;
  }
  else {
    fprintf (stderr, "Unparsable input character: %c\n", *str);
    *pstr = str;
    return NULL;
  }
  *pstr = str;
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispIParse --
 *
 *      Parse a string to a Sexp.
 *
 *  Results:
 *      Returns pointer to a Sexp.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Sexp *
LispIParse (char **pstr)
{
  char *str = *pstr;
  Sexp *s;
  Sexp *ret = NULL;
  Sexp **sptr;

  str = stripleft (str);
  if (!*str) {
    fprintf (stderr, "Input malformed\n");
    return NULL;
  }
  while (*str != ')') {
    if (!*str) {
      fprintf (stderr, "Input malformed: missing )\n");
      return NULL;
    }
    s = LispNewSexp ();
    if (*str == '(') {
      CAR(s) = LispNewObj ();
      LTYPE(CAR(s)) = S_LIST;
      str++;
      LLIST(CAR(s)) = LispIParse (&str);
      if (*str != ')') {
	*pstr = str;
	return NULL;
      }
      str++;
    }
    else if (*str == '\'') {
      LispObj *l;
      Sexp *t;
      t = s;
      CAR(s) = LispNewObj ();
      LTYPE(CAR(s)) = S_SYM;
      LSYM(CAR(s)) = (char *)LispNewString ("quote");
      CDR(s) = LispNewObj ();
      LTYPE(CDR(s)) = S_LIST;
      LLIST(CDR(s)) = LispNewSexp ();
      s = LLIST(CDR(s));
      CDR(s) = LispNewObj ();
      LTYPE(CDR(s)) = S_LIST;
      LLIST(CDR(s)) = NULL;
      str++;
      str = stripleft (str);
      if (*str == '(') {
	str++;
	CAR(s) = LispNewObj ();
	LTYPE(CAR(s)) = S_LIST;
	LLIST(CAR(s)) = LispIParse (&str);
	if (*str != ')') {
	  *pstr = str;
	  return NULL;
	}
	str++;
      }
      else {
	if (!(CAR(s) = LispAtomParse (&str,1))) {
	  *pstr = str;
	  return NULL;
	}
      }
      *pstr = str;
      l = LispNewObj ();
      LTYPE(l) = S_LIST;
      LLIST(l) = t;
      t = LispNewSexp ();
      CAR(t) = l;
      s = t;
    }
    else {
      if (!(CAR(s) = LispAtomParse (&str,0))) {
	*pstr = str;
	return NULL;
      }
    }
    CDR(s) = LispNewObj ();
    LTYPE(CDR(s)) = S_LIST;
    if (ret == NULL)
      ret = s;
    else
      *sptr = s;
    sptr = &LLIST(CDR(s));
    str = stripleft (str);
  }
  if (ret)
    *sptr = NULL;
  *pstr = str;
  return ret;
}


/*-----------------------------------------------------------------------------
 *
 *  LispParseString --
 *
 *      Parse string to a lisp object.
 *
 *  Results:
 *      Returns pointer to the object.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispParseString (char *str)
{
  LispObj *l;

  str = stripleft (str);
  if (*str != '(')
    l = LispAtomParse (&str,0);
  else {
    str++;
    l = LispNewObj ();
    LTYPE(l) = S_LIST;
    LLIST(l) = LispIParse (&str);
    if (LLIST(l) && *str != ')')
	l = NULL;
  }
  return l;
}
