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



/*------------------------------------------------------------------------
 *
 *  mergesort --
 *
 *   Sort an array: input is in a, output is in a if return value = 0, 
 *                                 output is in b if return value = 1
 *
 *------------------------------------------------------------------------
 */
static int  _mysort (const void **a, const void **b,
		     int sz, int (*cmpfn)(const void *, const void *))
{
  const void **x, **y, **z;
  int i, j;
  int p;
  
  if (sz == 1) {
    return 0;
  }
  if ((p = _mysort (a, b, sz/2, cmpfn))) {
    x = b;
    z = a;
  }
  else {
    x = a;
    z = b;
  }
  if (_mysort (a+sz/2, b+sz/2, sz-sz/2, cmpfn)) {
    if (p == 0) {
      /* copy */
      y = a+sz/2;
      for (i=0; i < sz-sz/2; i++) {
	y[i] = (b+sz/2)[i];
      }
    }
    else {
      y = b+sz/2;
    }
  }
  else {
    if (p == 1) {
      /* copy */
      y = b+sz/2;
      for (i=0; i < sz-sz/2; i++) {
	y[i] = (a+sz/2)[i];
      }
    }
    else {
      y = a+sz/2;
    }
  }

  /* -- merge -- */
  i = 0;
  j = 0;
  while (i < sz/2 || j < (sz-sz/2)) {
    if (i < sz/2) {
      if (j < (sz-sz/2)) {
	if ((*cmpfn) (x[i], y[j]) < 0) {
	  z[i+j] = x[i];
	  i++;
	}
	else {
	  z[i+j] = y[j];
	  j++;
	}
      }
      else {
	z[i+j] = x[i];
	i++;
      }
    }
    else {
      z[i+j] = y[j];
      j++;
    }
  }
  return (1-p);
}
		     
void mymergesort (const void **a, int sz,
		  int (*cmpfn)(const void *, const void *))
{
  const void **b;
  int i;
  
  Assert (sz > 1, "What");

  MALLOC (b, const void *, sz);

  if (_mysort (a, b, sz, cmpfn)) {
    for (i=0; i < sz; i++) {
      a[i] = b[i];
    }
  }
  FREE (b);
}


/*
  p has size sz
  aux has size sz+1
  Uses Heap's algorithm.
*/
int mypermutation (int *p, int *aux, int sz)
{
  int i;

  if (sz == 0) return -1;
  
  if (sz == 1) {
    return 0;
  }
  
  if (aux[0] == -1) {
    aux[0] = 0;
    /* initialize */
    for (i=0; i < sz; i++) {
      aux[i+1] = 0;
    }
    return 1;
  }
  i = aux[0];
  while (i < sz) {
    if (aux[i+1] < i) {
      int tmp;
      if (i % 2) {
	/* i sz */
	tmp = p[i];
	p[i] = p[aux[i+1]];
	p[aux[i+1]] = tmp;
      }
      else {
	/* 0 sz */
	tmp = p[0];
	p[0] = p[i];
	p[i] = tmp;
      }
      aux[i+1]++;
      i = 0;
      aux[0] = i;
      return 1;
    }
    else {
      aux[i+1] = 0;
      i++;
    }
  }
  aux[0] = -1;
  return 0;
}
