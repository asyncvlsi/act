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
#ifndef __PP_H__
#define __PP_H__

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct __pp_brstack {
  int indent;
  int broken;			/* true if broken */
  struct __pp_brstack *n;
};
  
typedef struct {
  FILE *fp;			/* output stream */

  char *inpbuf;                 /* input buffer */
  int inpbufsz;			/* input buffer size */
  int inpbufst;			/* start position */
  int inpbufend;		/* end position */
  /* inpbuf: a circular buffer.
             0     .  .  .     inpbufsz

     One position is not used.

     Includes inpbufst, not inpbufend.
     Empty: inpbufst == inpbufend
      Full: inpbufst = k, inpbufend = k-1 mod inpbufsz.
  */

  char *linebuf;		/* output buffer */
  int margin;			/* margin */

  int indent;			/* current indentation */
  int correction;		/* correction */
 
  struct __pp_brstack *stk;	/* stack of setb/endbs */
} pp_t;


enum {
  PP_FORCED  = '\n',
  PP_UNITED = '\001',
  PP_LAZY   = '\002',
  PP_SETB   = '\003',
  PP_ENDB   = '\004'
};

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                             FUNCTIONS 
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

extern pp_t *pp_init (FILE *, int margin);
  /*
     Return a new pretty-printer filter with "margin" set to be the margin.
  */

extern void pp_puts_generic (pp_t *, const char *, int flag);
  /*
     If "flag" != 0, prints out raw string without interpreting anything
     except '\n' as a newline.
     If "flag" == 0, understands normal pp formatting conventions.
  */

#define pp_puts(pp,s)  pp_puts_generic(pp,s,0)
   /*
     Pretty-print string
    */

#define pp_puts_raw(pp,s) pp_puts_generic(pp,s,1)
   /*  
     Dump raw string "s" on pretty-printer
    */

extern void pp_printf (pp_t *, const char *, ...);
  /*
     Pretty-printf a string. The resulting string should be < 4096
     characters.
  */

extern void pp_vprintf (pp_t *, const char *, va_list);
  /*
     Pretty-printf a string. The resulting string should be < 4096
     characters.
  */

extern void pp_printf_raw (pp_t *, const char *, ...);
  /*
     Dump raw string to pp. Same 4096 character restriction.
  */

extern void pp_vprintf_raw (pp_t *, const char *, va_list);
  /*
     Pretty-printf a string. The resulting string should be < 4096
     characters.
  */

extern void pp_printf_text (pp_t *, const char *, ...);
  /*  
      Pretty-printf a text string. This means that word/tab spacing is
      ignored, and there is an implicit lazy breakpoint after each
      word. Words are space/tab delimited.  Newlines are respected.
  */

extern void pp_lazy (pp_t *, int);
  /*
     Put a lazy breakpoint into the output
  */

extern void pp_united (pp_t *, int);
  /*
     Put a united breakpoint into the output
  */

extern void pp_forced (pp_t *, int);
  /*
     Put a forced breakpoint into the output
  */

#define pp_setb(x)  pp_puts(x,"\003")
  /*
     Put a "setb" into the output
   */

#define pp_endb(x)  pp_puts(x,"\004")
 /*
    Put an "endb" into the output
  */

extern void pp_flush (pp_t *);
  /*
     Flush output
  */

extern void pp_close (pp_t *);
  /*
     Close a pretty-printer
  */

extern void pp_stop (pp_t *);
  /*
    Flush and free a pretty-printer without closing the output file
  */

#ifdef __cplusplus
}
#endif

#endif /* __PP_H__ */
