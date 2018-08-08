/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
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
  }

  if (str == NULL || *str == '\0') {
    /* no mangling! */
    any_mangling = 0;
    return;
  }
  any_mangling = 1;

  mangle_characters['_'] = mangle_result[0];
  inv_map[mangle_result[0]] = '_';

  for (i=0; (i+1) < max_len && str[i]; i++) {
    if (mangle_characters[str[i]] >= 0) {
      fatal_error ("Cannot install mangle string `%s': dup char `%c'",
		   str, str[i]);
    }
    mangle_characters[str[i]] = mangle_result[i+1];
    inv_map[mangle_result[i+1]] = str[i];
  }
  if (str[i]) {
    fatal_error ("Cannot install mangle string `%s'---too many characters",
		 str);
  }
}

/*------------------------------------------------------------------------
 *
 *  Act::mangle_string --
 *
 *   String mangling code. Returns 0 on success, -1 on error
 *
 *------------------------------------------------------------------------
 */
int Act::mangle_string (char *src, char *dst, int sz)
{
  if (!any_mangling) {
    if (strlen (src) > sz) return -1;
    strcpy (dst, src);
    return 0;
  }

  while (*src && sz > 0) {
    if (mangle_characters[*src] >= 0) {
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
int Act::unmangle_string (char *src, char *dst, int sz)
{
  if (!any_mangling) {
    if (strlen (src) > sz) return -1;
    strcpy (dst, src);
    return 0;
  }

  while (*src && sz > 0) {
    if (*src == mangle_result[0]) {
      src++;
      *dst++ = inv_map[*src];
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
    fatal_error ("Mangle/unmangle pair failure: [[ %s ]]\n", buf);
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
    fatal_error ("Mangle/unmangle pair failure: [[ %s ]]\n", buf);
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
    fatal_error ("Mangle/unmangle pair failure: [[ %s ]]\n", buf);
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
    fatal_error ("Mangle/unmangle pair failure: [[ %s ]]\n", buf);
  }
  
  return snprintf (fp, len, "%s", buf2);
}
