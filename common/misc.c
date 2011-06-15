/*************************************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *  All Rights Reserved
 *
 *************************************************************************/
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

