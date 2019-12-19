/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#include <act/act.h>
#include "misc.h"

/*
  Code for mangling/unmangling special characters to sanitize output
*/

static char mangle_result[] = 
  { '_', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f', 'z', 'h', 'i', 'j', 'k',
    'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z' };

static int mangle_invidx[256];

/*------------------------------------------------------------------------
 *
 *  Act::mangle --
 *
 *   Install string mangling. Different mangling gets used for body
 *   items and types.
 *
 *------------------------------------------------------------------------
 */
void Act::mangle (char *str)
{
  int i;
  int max_len = sizeof (mangle_result)/sizeof (mangle_result[0]);

  for (i=0; i < 256; i++) {
    mangle_characters[i] = -1;
    inv_map[i] = -1;
    mangle_invidx[i] = -1;
  }

  if (str == NULL || *str == '\0') {
    /* no mangling! */
    any_mangling = 0;
    return;
  }
  any_mangling = 1;
  //mangle_langle_idx = -1;
  //mangle_min_idx = -1;
  //mangle_mode = 0;

  mangle_characters['_'] = mangle_result[0];
  inv_map[mangle_result[0]] = '_';
  mangle_invidx['_'] = 0;

  for (i=0; (i+1) < max_len && str[i]; i++) {
    if (mangle_characters[str[i]] >= 0) {
      fatal_error ("Cannot install mangle string `%s': dup char `%c'",
		   str, str[i]);
    }
    mangle_invidx[mangle_result[i+1]] = i+1;
    mangle_characters[str[i]] = mangle_result[i+1];
    inv_map[mangle_result[i+1]] = str[i];

#if 0
    if (str[i] == '<') {
      mangle_langle_idx = i;
    }
    if (mangle_min_idx == -1 && (str[i] == '.' || str[i] == ',' || str[i] == '{' || str[i] == '}')) {
      mangle_min_idx = i;
    }
#endif    
  }
  if (str[i]) {
    fatal_error ("Cannot install mangle string `%s'---too many characters",
		 str);
  }
#if 0  
  if (mangle_min_idx == -1) {
    mangle_min_idx = strlen (str);
  }
#endif  
}

/*------------------------------------------------------------------------
 *
 *  Act::mangle_string --
 *
 *   String mangling code. Returns 0 on success, -1 on error
 *
 *------------------------------------------------------------------------
 */
int Act::mangle_string (const char *src, char *dst, int sz)
{
  if (!any_mangling) {
    if (strlen (src) > sz) return -1;
    strcpy (dst, src);
    return 0;
  }

  while (*src && sz > 0) {
    if (mangle_characters[*src] >= 0) {
      //&&
      //((*src != '_') || inv_map[*(src+1)] == -1)) {
      /* modify _ mangling; special case.
	 so mangle if
	 
	 *src != '_' || *src == '_' AND
	    next character cannot be the result of mangling!
	    (inv_map[next char] == -1)
      */
      *dst++ = '_';
      sz--;
      if (sz == 0) return -1;
      *dst++ = mangle_characters[*src];
      sz--;
    }
    else {
      *dst++ = *src;
      sz--;
    }
    if (sz == 0) return -1;
#if 0    
    if (*src == '<') {
      mangle_mode++;
    }
    else if (*src == '>') {
      mangle_mode--;
    }
#endif
    src++;
  }
  *dst = '\0';
  return 0;
}


/*------------------------------------------------------------------------
 *
 *  Act::unmangle_string --
 *
 *   String unmangling code. Returns 0 on success, -1 on error
 *
 *------------------------------------------------------------------------
 */
int Act::unmangle_string (const char *src, char *dst, int sz)
{
  if (!any_mangling) {
    if (strlen (src) > sz) return -1;
    strcpy (dst, src);
    return 0;
  }

  while (*src && sz > 0) {
    if (*src == mangle_result[0]) {
    //    if ((*src == mangle_result[0]) &&
    //	(mangle_invidx[*(src+1)] != -1)) {
      src++;
      *dst++ = inv_map[*src];
#if 0      
      if (inv_map[*src] == '<') {
	mangle_mode++;
      }
      else if (inv_map[*src] == '>') {
	mangle_mode--;
      }
#endif      
      sz--;
      if (sz == 0) return -1;
    }
    else {
      *dst++ = *src;
      sz--;
    }
    if (sz == 0) return -1;
    src++;
  }
  *dst = '\0';
  return 0;
}

/*------------------------------------------------------------------------
 *
 *  Act::mfprintf --
 *
 *   Internal fprintf that automatically mangles names before emitting
 *   them.
 *
 *------------------------------------------------------------------------
 */
void Act::mfprintf (FILE *fp, const char *s, ...)
{
  va_list ap;
  static char buf[10240];
  static char buf2[20480];
  static char chk[10240];

  va_start (ap, s);
  vsprintf (buf, s, ap);
  va_end (ap);
  
  Assert (mangle_string (buf, buf2, 20480) == 0, "Long name");
  Assert (unmangle_string (buf2, chk, 10240) == 0, "Ick");
  if (strcmp (chk, buf) != 0) {
#if 1
    /* XXX: This is a problem.
       In particular, foo<8>
       Will get mangled to foo_38_7
       Mangling foo_38_7 will result in foo_38_7
       So this is not a unique invertible mapping!
    */
    fprintf (stderr, "Mangled: %s; unmangled: %s\n", buf2, chk);
    fatal_error ("Mangle/unmangle pair failure: [[ %s ]]\n", buf);
#endif    
  }
  
  fprintf (fp, "%s", buf2);
}

/*------------------------------------------------------------------------
 *
 *  Act::msnprintf --
 *
 *   Internal snprintf that automatically mangles names before emitting
 *   them.
 *
 *------------------------------------------------------------------------
 */
int Act::msnprintf (char *fp, int len, const char *s, ...)
{
  va_list ap;
  static char buf[10240];
  static char buf2[20480];
  static char chk[10240];

  va_start (ap, s);
  vsprintf (buf, s, ap);
  va_end (ap);
  
  Assert (mangle_string (buf, buf2, 20480) == 0, "Long name");
  Assert (unmangle_string (buf2, chk, 10240) == 0, "Ick");
  if (strcmp (chk, buf) != 0) {
#if 1
    fprintf (stderr, "Mangled: %s; unmangled: %s\n", buf2, chk);
    fatal_error ("Mangle/unmangle pair failure: [[ %s ]]\n", buf);
#endif
  }
  
  return snprintf (fp, len, "%s", buf2);
}



/*------------------------------------------------------------------------
 *
 *  Act::ufprintf --
 *
 *   Internal fprintf that automatically mangles names before emitting
 *   them.
 *
 *------------------------------------------------------------------------
 */
void Act::ufprintf (FILE *fp, const char *s, ...)
{
  va_list ap;
  static char buf[10240];
  static char buf2[10240];
  static char chk[10240];

  va_start (ap, s);
  vsprintf (buf, s, ap);
  va_end (ap);
  
  Assert (unmangle_string (buf, buf2, 10240) == 0, "Long name");
  Assert (mangle_string (buf2, chk, 10240) == 0, "Ick");
  if (strcmp (chk, buf) != 0) {
#if 0
    /* XXX: This is a problem.
       In particular, foo<8>
       Will get mangled to foo_38_7
       Mangling foo_38_7 will result in foo_38_7
       So this is not a unique invertible mapping!
    */
    fprintf (stderr, "Mangled: %s; unmangled: %s\n", buf2, chk);
    fatal_error ("Mangle/unmangle pair failure: [[ %s ]]\n", buf);
#endif    
  }
  
  fprintf (fp, "%s", buf2);
}

/*------------------------------------------------------------------------
 *
 *  Act::usnprintf --
 *
 *   Internal fprintf that automatically mangles names before emitting
 *   them.
 *
 *------------------------------------------------------------------------
 */
int Act::usnprintf (char *fp, int len, const char *s, ...)
{
  va_list ap;
  static char buf[10240];
  static char buf2[10240];
  static char chk[10240];

  va_start (ap, s);
  vsprintf (buf, s, ap);
  va_end (ap);
  
  Assert (unmangle_string (buf, buf2, 10240) == 0, "Long name");
  Assert (mangle_string (buf2, chk, 10240) == 0, "Ick");
  if (strcmp (chk, buf) != 0) {
#if 0    
    fprintf (stderr, "Mangled: %s; unmangled: %s\n", buf2, chk);
    fatal_error ("Mangle/unmangle pair failure: [[ %s ]]\n", buf);
#endif    
  }
  
  return snprintf (fp, len, "%s", buf2);
}

/*------------------------------------------------------------------------
 *
 *  Act::msnprintfproc --
 *
 *   Special mangling for process name
 *
 *  WARNING: keep consistent with InstType::sPrint 
 *
 *------------------------------------------------------------------------
 */
void Act::msnprintfproc (char *fp, int len, UserDef *p, int omit_ns)
{
  int pos = 0;

  fp[0] = '\0';
  if (!omit_ns) {
    if (p->getns() && p->getns() != ActNamespace::Global()) {
      char *s = p->getns()->Name();
      msnprintf (fp, len, "%s::", s);
      pos = strlen (fp);
      len -= pos;
      fp += pos;
      FREE (s);
    }
  }

  const char *proc_name = p->getName ();
  pos = strlen (proc_name);
  if (pos > 2 && proc_name[pos-2] == '<' &&  proc_name[pos-1] == '>') {
    pos = 0;
    while (proc_name[pos] != '<') {
      if (len == 1) {
	fp[pos] = '\0';
	return;
      }
      fp[pos] = proc_name[pos];
      pos++;
      len--;
    }
    fp[pos] = '\0';
  }
  else {
    msnprintf (fp, len, "%s", proc_name);
  }
}


/*------------------------------------------------------------------------
 *
 *  Act::mfprintfproc --
 *
 *   Special mangling for process name
 *
 *------------------------------------------------------------------------
 */
void Act::mfprintfproc (FILE *fp, UserDef *p, int omit_ns)
{
  int pos = 0;

  /*--- process gp ---*/
  if (!omit_ns) {
    if (p->getns() && p->getns() != ActNamespace::Global()) {
      char *s = p->getns()->Name();
      mfprintf (fp, "%s::", s);
      FREE (s);
    }
  }

  const char *proc_name = p->getName ();
  pos = strlen (proc_name);
  if (pos > 2 && proc_name[pos-2] == '<' &&  proc_name[pos-1] == '>') {
    pos = 0;
    while (proc_name[pos] != '<') {
      fputc (proc_name[pos], fp);
      pos++;
    }
  }
  else {
    mfprintf (fp, "%s", proc_name);
  }
}
