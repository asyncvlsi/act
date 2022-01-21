/*************************************************************************
 *
 *  Copyright (c) 2020 Rajit Manohar
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
 *  lispPath.c --
 *
 *    This handles path functions for file I/O
 *
 *************************************************************************
 */
#include <stdio.h>
#include "lispInt.h"

FILE *
LispPathOpen (char *name, char *mode, char *pathspec)
{
  FILE *fp;
  char *t, *u, *s;
  char *tmp;

  if ((fp = fopen (name, mode))) {
    return fp;
  }
  if (pathspec) {
    int done = 0;
    t = Strdup (pathspec);
    u = t;
    s = u;
    while (!done) {
      if (*u == ':' || *u == '\0') {
	if (*u == '\0') {
	  done = 1;
	}
	*u = '\0';
	MALLOC (tmp, char, strlen (s) + strlen (name) + 2);
	sprintf (tmp, "%s/%s", s, name);
	fp = fopen (tmp, mode);
	FREE (tmp);
	if (fp) {
	  FREE (t);
	  return fp;
	}
	s = u+1;
      }
      u++;
    }
    FREE (t);
  }
  return NULL;
}

char *
LispPathFile (char *name, char *pathspec)
{
  FILE *fp;
  char *t, *u, *s;
  char *tmp;

  if ((fp = fopen (name, "r"))) {
    fclose (fp);
    return Strdup (name);
  }
  if (pathspec) {
    int done = 0;
    t = Strdup (pathspec);
    u = t;
    s = u;
    while (!done) {
      if (*u == ':' || *u == '\0') {
	if (*u == '\0') {
	  done = 1;
	}
	*u = '\0';
	MALLOC (tmp, char, strlen (s) + strlen (name) + 2);
	sprintf (tmp, "%s/%s", s, name);
	fp = fopen (tmp, "r");
	if (fp) {
	  FREE (t);
	  fclose (fp);
	  return tmp;
	}
	FREE (tmp);
	s = u+1;
      }
      u++;
    }
    FREE (t);
  }
  return NULL;
}
