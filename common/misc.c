/*************************************************************************
 *
 *  Miscellaneous functions
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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "misc.h"


#ifdef MEM_DEBUG

static FILE *mem_logfp = NULL;

static void mem_log_init (void)
{
  if (mem_logfp == NULL) {
    mem_logfp = fopen ("mem.log", "w");
  }
}

static void mem_log_done (void)
{
  mem_log_init ();
  fclose (mem_logfp);
  mem_logfp = NULL;
}
			 
void mem_log (const char *s, ...)
{
  va_list ap;

  mem_log_init ();

  va_start (ap, s);
  vfprintf (mem_logfp, s, ap);
  va_end (ap);
  fprintf (mem_logfp, "\n");
  fflush (mem_logfp);
}

#endif


/*-------------------------------------------------------------------------
 * print error message and die.
 *-----------------------------------------------------------------------*/
void fatal_error (const char *s, ...)
{
  va_list ap;

  fprintf (stderr, "FATAL: ");
  va_start (ap, s);
  vfprintf (stderr, s, ap);
  va_end (ap);
  fprintf (stderr, "\n");
  exit (1);
}

/*-------------------------------------------------------------------------
 * warning
 *-----------------------------------------------------------------------*/
void warning (const char *s, ...)
{
  va_list ap;

  fprintf (stderr, "WARNING: ");
  va_start (ap, s);
  vfprintf (stderr, s, ap);
  va_end (ap);
  fprintf (stderr, "\n");
}

char *my_Strdup (const char *s)
{
  char *t;


  MALLOC (t, char, strlen(s)+1);
  strcpy (t, s);

  return t;
}

char *Strdup (const char *s)
{
  char *t;

  MALLOC (t, char, strlen(s)+1);
  strcpy (t, s);

  return t;
}

