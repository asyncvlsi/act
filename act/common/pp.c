/*************************************************************************
 *
 *  Pretty printer library.
 *
 *  Copyright (c) 1996-2000, 2019 Rajit Manohar
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

/*
   Generic pretty printer.
*/
#include <stdio.h>
#include <stdarg.h>
#include "misc.h"
#include "pp.h"


/*
 *------------------------------------------------------------------------
 *
 *  Initialize a pretty-printer
 *
 *------------------------------------------------------------------------
 */
pp_t *pp_init (FILE *fp, int margin)
{
  pp_t *pp;

  if (margin < 1)
    fatal_error ("pp_init: margin argument must be at least 1.");
  if (!fp) return NULL;
  MALLOC(pp,pp_t,1);
  pp->fp = fp;
  pp->margin = margin;
  MALLOC(pp->linebuf,char,margin+1);
  MALLOC(pp->inpbuf,char,4096+1);
  pp->inpbufsz = 4096;
  pp->inpbufst = 0;
  pp->inpbufend = 0;
  pp->indent = 0;
  pp->correction = 0;
  pp->stk = NULL;
  return pp;
}

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif

static void Break (pp_t *pp, int offset)
{
  int i;

  pp->linebuf[pp->indent] = '\0';
  fputs (pp->linebuf, pp->fp);
  fputs ("\n", pp->fp);
  if (pp->stk) {
    pp->stk->broken = 1;
    offset = MAX(0,pp->stk->indent+offset);
  }
  else
    offset = MAX(0,offset);
  pp->indent = offset % pp->margin;
  pp->correction = (offset / pp->margin ) * pp->margin;
  for (i=0; i < pp->indent; i++)
    pp->linebuf[i] = ' ';
}

#define isbp(c)  ((c) == PP_FORCED || (c) == PP_UNITED || (c) == PP_LAZY)

void stuff_into_buffer (pp_t *pp, const char *s, int flag)
{
  int i;
  int len;
  int sz;

  for (len=0; s[len]; len++)
    if (isbp(s[len])) len++;
  /* simple case */
  if (pp->inpbufend >= pp->inpbufst)
    i = pp->inpbufend - pp->inpbufst;
  else
    i = pp->inpbufend - pp->inpbufst + pp->inpbufsz;
  sz = pp->inpbufsz;
  while (sz - i < len) sz *= 2;
  if (sz != pp->inpbufsz) {
    /* resize buffer */
    REALLOC (pp->inpbuf, char, sz+1);
    if (pp->inpbufend < pp->inpbufst) {
      /* copy to end! */
      for (i=0; i < pp->inpbufend; i++)
	pp->inpbuf[(i+pp->inpbufsz)%sz] = pp->inpbuf[i];
      pp->inpbufend = (i+pp->inpbufsz)%sz;
    }
    pp->inpbufsz = sz;
  }
  /* stuff into input buffer */
  for (i=0; s[i]; i++) {
    pp->inpbuf[(i+pp->inpbufend)%pp->inpbufsz] = s[i];
    if (!flag && isbp(s[i])) {
      pp->inpbuf[(i+pp->inpbufend+1)%pp->inpbufsz] = s[i+1];
      i++;
    }
  }
  pp->inpbufend = (pp->inpbufend + i)%pp->inpbufsz;
}


/*
 *------------------------------------------------------------------------
 *
 *  Put a string out to the pretty-printer
 *
 *------------------------------------------------------------------------
 */
void pp_puts_generic (pp_t *pp, const char *s, int flag)
{
  int i, j, k;
  int tmp;
  int nlev;
  int offset;
  int pos;
  int bp;
  struct __pp_brstack *st;

  stuff_into_buffer (pp,s,flag);
  for (i=pp->inpbufst; i != pp->inpbufend;) {
    tmp = (i+1)%pp->inpbufsz;

    if (pp->inpbuf[i] == PP_SETB && !flag) {
      MALLOC (st,struct __pp_brstack, 1);
      st->indent = pp->indent + pp->correction;
      st->broken = 0;
      st->n = pp->stk;
      pp->stk = st;
    }

    else if (pp->inpbuf[i] == PP_ENDB && !flag) {
      st = pp->stk;
      pp->stk = pp->stk->n;
      if (pp->stk)
	pp->stk->broken |= st->broken; /* break from root */
      FREE (st);
    }

    else if (pp->inpbuf[i] == PP_FORCED) {
      if (!flag) {
	offset = (signed int)((signed char) pp->inpbuf[tmp]);
	tmp=(i+2)%pp->inpbufsz;
      }
      else
	offset = 0;
      Break (pp,offset);
    }

    else if (pp->inpbuf[i] == PP_LAZY && !flag) {
      offset = (signed int)((signed char) pp->inpbuf[tmp]);
      tmp=(i+2)%pp->inpbufsz;

      /* check for break */

      nlev = 0;
      j = tmp;
      pos = pp->indent + pp->correction;
      while (1) {
	if (j == pp->inpbufend) { bp = -1;  break; }
	if (pos >= pp->margin) {  bp = 1; break; }
	if (nlev == 0 && (pp->inpbuf[j] == PP_ENDB || isbp (pp->inpbuf[j]))) {
	  bp = 0;
	  break;
	}
	if (pp->inpbuf[j] == PP_SETB) nlev++;
	else if (pp->inpbuf[j] == PP_ENDB) nlev--;
	else pos++;
	j = (j+1)%pp->inpbufsz;
      }
      if (bp == -1)
	break;			/* don't know what to do now. */
      else if (bp)
	Break(pp,offset);
    }

    else if (pp->inpbuf[i] == PP_UNITED && !flag) {
      offset = (signed int)((signed char) pp->inpbuf[tmp]);
      tmp = (i+2)%pp->inpbufsz;

      nlev = 0;
      j = tmp;
      pos = pp->indent + pp->correction;
      if (pp->stk && pp->stk->broken)
	bp = 1;
      else {
	while (1) {
	  if (j == pp->inpbufend) { bp = -1; break; }
	  if (pos >= pp->margin) { bp = 1; break; }
	  if (nlev == 0 && pp->inpbuf[j] == PP_ENDB) {
	    bp = 0;
	    break;
	  }
	  if (pp->inpbuf[j] == PP_SETB) nlev++;
	  else if (pp->inpbuf[j] == PP_ENDB) nlev--;
	  else if (isbp(pp->inpbuf[j])) j = (j+1)%pp->inpbufsz;
	  else pos++;
	  j = (j+1)%pp->inpbufsz;
	}
      }
      if (bp == -1)
	break;
      if (bp)
	Break(pp,offset);
    }
    else {
      if (pp->indent == pp->margin) {
	pp->linebuf[pp->margin] = '\0';
	fputs (pp->linebuf, pp->fp);
	pp->correction += pp->margin;
	pp->indent = 0;
      }
      pp->linebuf[pp->indent] = pp->inpbuf[i];
      pp->indent++;
    }
    i = tmp;
  }
  pp->inpbufst = i;
}


/*-------------------------------------------------------------------------
 * Print a complex message
 *-----------------------------------------------------------------------*/
void pp_printf (pp_t *pp, const char *s, ...)
{
  va_list ap;
  static char buffer[4096];

  va_start (ap, s);
  vsprintf (buffer, s, ap);
  va_end (ap);

  pp_puts (pp,buffer);
}

/*------------------------------------------------------------------------
 * Print a complex message
 *------------------------------------------------------------------------
 */
void pp_vprintf (pp_t *pp, const char *s, va_list ap)
{
  static char buffer[4096];

  vsprintf (buffer, s, ap);
  pp_puts (pp, buffer);
}


/*-------------------------------------------------------------------------
 * Print a complex message
 *-----------------------------------------------------------------------*/
void pp_printf_raw (pp_t *pp, const char *s, ...)
{
  va_list ap;
  static char buffer[4096];

  va_start (ap, s);
  vsprintf (buffer, s, ap);
  va_end (ap);

  pp_puts_raw (pp,buffer);
}

/*------------------------------------------------------------------------
 * Print a complex message
 *------------------------------------------------------------------------
 */
void pp_vprintf_raw (pp_t *pp, const char *s, va_list ap)
{
  static char buffer[4096];

  vsprintf (buffer, s, ap);
  pp_puts_raw (pp, buffer);
}


/*-------------------------------------------------------------------------
 * Print a complex message, word-split
 *-----------------------------------------------------------------------*/
void pp_printf_text (pp_t *pp, const char *s, ...)
{
  va_list ap;
  static char buffer[4096];
  int i, j;

  va_start (ap, s);
  vsprintf (buffer, s, ap);
  va_end (ap);

  j = 0; i = 0;

  while (buffer[i]) {
    if (buffer[i] == ' ' || buffer[i] == '\t') {
      buffer[i] = '\0';
      pp_puts (pp, buffer+j);
      pp_lazy (pp, 0);
      pp_puts (pp, " ");
      j = i+1;
      while (buffer[j] && (buffer[j] == ' ' || buffer[j] == '\t'))
	j++;
      i = j;
    }
    else if (buffer[i] == '\n') {
      buffer[i] = '\0';
      pp_puts (pp, buffer+j);
      pp_forced (pp, 0);
      i++;
      j = i;
    }
    else {
      i++;
    }
  }
  pp_puts (pp,buffer+j);
}


/*
 *------------------------------------------------------------------------
 *
 *  Put a "flush" out to the pretty-printer
 *
 *------------------------------------------------------------------------
 */
void pp_flush (pp_t *pp)
{
  if (pp->inpbufst != pp->inpbufend || pp->indent != 0)
    pp_forced (pp,0);
  fflush (pp->fp);
}


/*
 *------------------------------------------------------------------------
 *
 *  Close pp stream
 *
 *------------------------------------------------------------------------
 */
void pp_close (pp_t *pp)
{
  struct __pp_brstack *st;

  pp_flush (pp);
  fclose (pp->fp);
  while (pp->stk) {
    st = pp->stk;
    pp->stk = pp->stk->n;
    FREE (st);
  }
  FREE (pp->linebuf);
  FREE (pp->inpbuf);
  FREE (pp);
}


/*
 *------------------------------------------------------------------------
 *
 *  Stop pp stream
 *
 *------------------------------------------------------------------------
 */
void pp_stop (pp_t *pp)
{
  struct __pp_brstack *st;

  pp_flush (pp);
  while (pp->stk) {
    st = pp->stk;
    pp->stk = pp->stk->n;
    FREE (st);
  }
  FREE (pp->linebuf);
  FREE (pp->inpbuf);
  FREE (pp);
}


/*
 *------------------------------------------------------------------------
 *
 *  Put various special chars into the output
 *
 *------------------------------------------------------------------------
 */
static char obuf[3];

void pp_lazy (pp_t *pp, int val)
{
  obuf[0] = PP_LAZY;
  obuf[1] = val;
  obuf[2] = '\0';
  pp_puts (pp, obuf);
}

void pp_united (pp_t *pp, int val)
{
  obuf[0] = PP_UNITED;
  obuf[1] = val;
  obuf[2] = '\0';
  pp_puts (pp, obuf);
}

void pp_forced (pp_t *pp, int val)
{
  obuf[0] = PP_FORCED;
  obuf[1] = val;
  obuf[2] = '\0';
  pp_puts (pp, obuf);
}
