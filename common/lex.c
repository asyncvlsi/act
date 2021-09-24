/*************************************************************************
 *
 *  Lexical analysis engine
 *
 *  Copyright (c) 1996, 1999, 2016, 2019 Rajit Manohar
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
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include "lex.h"
#include "misc.h"
#include "lzw.h"

/*
  Predefined tokens
*/
const int l_err = 0;
const int l_eof = 1;
const int l_integer = 2;
const int l_real = 3;
const int l_string = 4;
const int l_id = 5;

const int l_offset = 6;		/* offset for all newly allocated tokens */

static const char *fixed_toks[] = {
  "-error-",
  "-end-of-file-",
  "-integer-",
  "-real-",
  "-string-",
  "-identifier-"
  };


#define contiguous_tok(c)  (isalnum(c) || (c) == '_')


/*-------------------------------------------------------------------------
 * get next character from input stream
 *-----------------------------------------------------------------------*/
static void getch (LEX_T *l)
{
  int pos;
  if (l->changed)
    l->colno = 0;
  if (l->bufptr < l->buflen && l->buf[l->bufptr])
    l->ch = l->buf[l->bufptr++];
  else {
    pos = 0;
    if (l->pos) {
      if (l->bufptr == l->buflen-1) {
	l->buflen *= 2;
	l->buf = (char *)realloc(l->buf, l->buflen);
	if (!l->buf)
	  fatal_error ("getch: realloc failed, size=%d", l->buflen);
      }
      pos = l->bufptr;
    }
    if (l->file) {
      l->buf[pos] = '\0';
      if (l->cfile) {
	c_fgets (l->buf+pos, l->buflen-pos, l->inp.fp);
      }
      else {
	fgets (l->buf+pos, l->buflen-pos, l->inp.fp);
      }
      l->bufptr = pos;
    }
    else {
      int foo;
      strncpy (l->buf+pos, l->inp.string, l->buflen-pos);
      l->bufptr = pos;
      foo = strlen (l->buf + pos);
      if (foo < l->buflen - pos) {
          l->inp.string += foo;
      }
      else {
          l->inp.string += l->buflen - pos;
      }
    }
    l->ch = l->buf[l->bufptr++];
  }
  l->colno++;
  if (l->ch == '\n') {
    l->lineno++;
    l->changed = 1;
  }
  else
    l->changed = 0;
}

/*-------------------------------------------------------------------------
 * go back one character in the input stream
 *-----------------------------------------------------------------------*/
static void ungetch (LEX_T *l)
{
  int i;

  if (l->bufptr > 0)
    l->bufptr--;
  else {
    l->buf = (char *) realloc (l->buf, sizeof(char)*(l->buflen+1));
    l->buflen ++;
    if (!l->buf)
      fatal_error ("ungetch:: realloc failed, size=%d", sizeof(char));
    for (i=l->buflen-1; i > 0; i--)
      l->buf[i] = l->buf[i-1];
    l->buf[0] = l->ch;
  }
  if (l->ch == '\n')
    l->lineno--;
}

/*------------------------------------------------------------------------
 * Unget one character from the saved string
 *-----------------------------------------------------------------------*/
static void unsave (LEX_T *l)
{
  if (l->saving && l->saved_loc > 0)
    l->saved[--l->saved_loc] = '\0';
}


/*------------------------------------------------------------------------
 * reallocate a string
 *------------------------------------------------------------------------
 */
static void realloc_string (char **sbuf, int *pos, int *len)
{
  if (*pos == *len-1) {
    *sbuf = (char*)realloc (*sbuf, *len*2);
    *len *= 2;
    if (!*sbuf)
      fatal_error ("realloc_string: realloc failed, size=%d", *len);
  }
}


/*-------------------------------------------------------------------------
 * add character to whitespace string
 *-----------------------------------------------------------------------*/
static void addws (LEX_T *l, char c)
{
  realloc_string (&l->whitespace, &l->whitespace_loc, &l->whitespace_len);
  l->whitespace[l->whitespace_loc++] = c;
  l->whitespace[l->whitespace_loc] = '\0';
  if (l->saving) {
    realloc_string (&l->saved, &l->saved_loc, &l->saved_len);
    l->saved[l->saved_loc++] = c;
    l->saved[l->saved_loc] = '\0';
  }
}


/*-------------------------------------------------------------------------
 * add character to token string
 *-----------------------------------------------------------------------*/
static void addtok (LEX_T *l, char c)
{
  int d;
  d = l->token_len;
  realloc_string (&l->token, &l->token_loc, &l->token_len);
  if (d != l->token_len) {
    REALLOC (l->tokprev, char, l->token_len);
  }
  l->token[l->token_loc++] = c;
  l->token[l->token_loc] = '\0';
  if (l->saving) {
    realloc_string (&l->saved, &l->saved_loc, &l->saved_len);
    l->saved[l->saved_loc++] = c;
    l->saved[l->saved_loc] = '\0';
  }
}

#define isidstart(l,x) (((x) == '_') || isalpha(x) || (((x) == '\\') && ((l)->flags & LEX_FLAGS_ESCAPEID)))
#define isid(x) (((x) == '_') || isalpha(x))


/*-------------------------------------------------------------------------
 * allocate LEX_T structure and initialize fields
 *-----------------------------------------------------------------------*/
static void linit (LEX_T *l)
{
  l->bufptr = 0;
  l->whitespace_loc = 0;
  l->token_loc = 0;
  l->saved_loc = 0;

  l->colno = 0;
  l->lineno = 1;
  l->changed = 0;
  l->file = 0;
  l->ch = '\0';
  
  l->buf[0] = '\0';
  l->whitespace[0] = '\0';
  l->token[0] = '\0';
  l->tokprev[0] = '\0';
  l->saved[0] = '\0';

  l->flags = 0;
  l->saving = 0;

  l->cfile = 0;
  
  l->pos = NULL;
}

static LEX_T *lmalloc (void)
{
  LEX_T *l;

  MALLOC (l,LEX_T,1);

  l->buflen = 128;
  l->whitespace_len = 128;
  l->token_len = 128;
  l->toksize = 128;
  l->saved_len = 128;

  MALLOC (l->buf,char,l->buflen);

  MALLOC (l->whitespace,char,l->whitespace_len);

  MALLOC (l->token,char,l->token_len);
  MALLOC (l->tokprev,char,l->token_len);
  MALLOC (l->saved, char, l->saved_len);

  MALLOC (l->tokens,char*,l->toksize);
  MALLOC (l->tokenvals,int,l->toksize);

  linit (l);

  l->ntokens = 0;

  return l;
}

/*------------------------------------------------------------------------
 * Skip #line directives
 *------------------------------------------------------------------------
 */
static void skipline (LEX_T *l)
{
  int ln;
  static char file[1024];
  int i;
  int found = 0;
  
  while ((l->flags & LEX_FLAGS_PARSELINE) && l->file
	 && (l->colno == 1) && l->ch == '#') {
    addws (l, l->ch);
    getch (l);
    while (!lex_eof (l) && (l->ch == ' ' || l->ch == '\t')) {
      addws (l, l->ch);
      getch (l);
    }
    if (l->ch == 'l') {
      found = 1;
      addws (l, l->ch);
      getch(l);
      if (l->ch != 'i') 
	fatal_error ("Unknown # directive in input\n%s", lex_errstring (l));
      addws (l, l->ch);
      getch(l);
      if (l->ch != 'n') 
	fatal_error ("Unknown # directive in input\n%s", lex_errstring (l));
      addws (l, l->ch);
      getch(l);
      if (l->ch != 'e') 
	fatal_error ("Unknown # directive in input\n%s", lex_errstring (l));
      addws (l, l->ch);
      getch(l);
      while (!lex_eof (l) && (l->ch == ' ' || l->ch == '\t')) {
	addws (l, l->ch);
	getch (l);
      }
    }
    ln = 0;
    while (isdigit (l->ch)) {
      found = 1;
      addws (l, l->ch);
      ln = 10*ln + l->ch - '0';
      getch (l);
    }
    if (found == 0) {
      fatal_error ("Unknown # directive in input\n%s", lex_errstring (l));
    }
    while (!lex_eof (l) && (l->ch == ' ' || l->ch == '\t')) {
      addws (l, l->ch);
      getch (l);
    }
    if (l->ch == '"') {		/* change file name */
      i = 0;
      addws (l, l->ch);
      getch (l);
      while (l->ch != '"') {
	addws (l, l->ch);
	file[i] = l->ch;
	if (l->ch == '\\') {
	  getch (l);
	  addws (l, l->ch);
	  file[i] = l->ch;
	}
	i++;
	if (i == 1024) fatal_error ("FIX THIS!");
	getch (l);
      }
      addws (l, l->ch);
      file[i] = '\0';
      FREE (l->filename);
      MALLOC(l->filename,char,strlen(file)+1);
      strcpy (l->filename, file);
      getch (l);
    }
    addws (l, l->ch);
    while (!lex_eof (l) && l->ch != '\n') {
      getch (l);
      addws (l, l->ch);
    }
    l->lineno = ln;
  }
}


/*-------------------------------------------------------------------------
 * skip whitespace
 *-----------------------------------------------------------------------*/
static void skipwhite (LEX_T *l)
{
  while (!lex_eof (l) && (isspace (l->ch) || l->ch == '#')) {
    if (l->ch == '#') {
      skipline (l);
      if (l->ch == '#') break;
    }
    else
      addws (l, l->ch);
    getch (l);
  }
}


/*-------------------------------------------------------------------------
 * skip comments and whitespace 
 *-----------------------------------------------------------------------*/
static void skipspace (LEX_T *l)
{
  skipwhite (l);
  while (l->ch == '/') {
    getch (l);
    if (l->ch == '/') {	/* C++ style comment */
      addws (l,'/');
      while (!lex_eof(l) && l->ch != '\n') {
	addws (l, l->ch);
	getch (l);
      }
    }
    else if (l->ch == '*') { /* C style comment */
      int done = 0;
      int nest = 0;
      addws (l, '/');
      addws (l, '*');
      getch (l);
      nest = 1;
      while (!lex_eof (l) && !done) {
	addws (l,l->ch);
	if (l->ch == '*') {
	  getch (l);
	  if (l->ch == '/') {
	    addws (l, '/');
	    nest--;
	    if (nest == 0 || !(lex_flags (l)&LEX_FLAGS_NSTCOMMENT))
	      done = 1;
	  }
	  else {
	    ungetch (l);
	    l->ch = '*';
	  }
	}
	else if (l->ch == '/') {
	  getch (l);
	  if (l->ch == '*') {
	    addws (l, '*');
	    nest++;
	  }
	  else {
	    ungetch (l);
	    l->ch = '/';
	  }
	}
	getch (l);
      }
    }
    else {
      ungetch (l);
      l->ch = '/';
      return;
    }
    skipwhite (l);
  }
  if (l->flags & LEX_FLAGS_PARENCOM) {
    while (l->ch == '(') {
      getch (l);
      if (l->ch == '*') {
	int done = 0;
	int nest = 0;
	addws (l, '(');
	addws (l, '*');
	getch (l);
	nest = 1;
	while (!lex_eof (l) && !done) {
	  addws (l,l->ch);
	  if (l->ch == '*') {
	    getch (l);
	    if (l->ch == ')') {
	      addws (l, ')');
	      nest--;
	      if (nest == 0 || !(lex_flags (l)&LEX_FLAGS_NSTCOMMENT))
		done = 1;
	    }
	    else {
	      ungetch (l);
	      l->ch = '*';
	    }
	  }
	  else if (l->ch == '(') {
	    getch (l);
	    if (l->ch == '*') {
	      addws (l, '*');
	      nest++;
	    }
	    else {
	      ungetch (l);
	      l->ch = '(';
	    }
	  }
	  getch (l);
	}
      }
      else {
	ungetch (l);
	l->ch = '(';
	return;
      }
      skipwhite (l);
    }
  }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                          EXPORTED FUNCTIONS

 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*-------------------------------------------------------------------------
 *  start lexical analysis on file
 *-----------------------------------------------------------------------*/
extern LEX_T *lex_file (FILE *fp)
{
  LEX_T *l;
  
  l = lmalloc ();
  l->file = 1;
  l->inp.fp = fp;
  if (fp == stdin) {
    MALLOC(l->filename,char,8);
    strcpy (l->filename,"-stdin-");
  }
  else
    l->filename = NULL;
  getch (l);
  return l;
}

/*-------------------------------------------------------------------------
 *  start lexical analysis on file
 *-----------------------------------------------------------------------*/
extern LEX_T *lex_fopen (const char *name)
{
  LEX_T *l;
  FILE *fp;

  fp = fopen (name, "r");
  if (!fp)
    fatal_error ("lex_fopen: could not open file `%s' for reading.", name);
  l = lmalloc ();
  l->file = 1;
  l->inp.fp = fp;
  MALLOC(l->filename,char,strlen(name)+1);
  strcpy (l->filename,name);
  getch (l);
  return l;
}

/*-------------------------------------------------------------------------
 *  start lexical analysis on file
 *-----------------------------------------------------------------------*/
extern LEX_T *lex_zfopen (const char *name)
{
  LEX_T *l;
  FILE *fp;

  fp = c_fopen_r (name);
  if (fp == NULL) {
    fatal_error ("lex_zfopen: could not open file `%s' for reading.", name);
  }
  l = lmalloc ();
  l->file = 1;
  l->cfile = 1;
  l->inp.fp = fp;
  MALLOC(l->filename,char,strlen(name)+1);
  strcpy (l->filename,name);
  getch (l);
  return l;
}


/*-------------------------------------------------------------------------
 * start lexical analysis on string
 *-----------------------------------------------------------------------*/
extern LEX_T *lex_string (char *s)
{
  LEX_T *l;

  l = lmalloc ();
  l->file = 0;
  l->inp.string = s;
  MALLOC(l->filename,char,9);
  strcpy (l->filename,"-string-");
  getch (l);
  return l;
}

/*-------------------------------------------------------------------------
 * restart lexical analysis on string
 *-----------------------------------------------------------------------*/
extern LEX_T *lex_restring (LEX_T *l, char *s)
{
  linit (l);
  l->file = 0;
  l->inp.string = s;
  getch (l);
  return l;
}


/*-------------------------------------------------------------------------
 * add a token to the list of currently recognized tokens
 *-----------------------------------------------------------------------*/
extern int lex_addtoken (LEX_T *l, const char *s)
{
  int i, j;

  if (*s == '\0')
    fatal_error ("lex_addtoken: received empty token!");
  if (l->ntokens == l->toksize) {
    l->tokens = (char **) realloc (l->tokens, sizeof(char*)*l->toksize*2);
    if (!l->tokens)
      fatal_error ("lex_addtoken: realloc failed, size=%d", 
		   sizeof(char*)*l->toksize);
    l->tokenvals = (int *) realloc (l->tokenvals, sizeof(int)*l->toksize*2);
    if (!l->tokenvals)
      fatal_error ("lex_addtoken: realloc failed, size=%d", 
		   sizeof(int)*l->toksize);
    l->toksize *= 2;
  }
  
  for (i=0; i < l->ntokens && (strcmp (l->tokens[i], s) < 0); i++) 
    /* empty */
    ;
  
  if (i < l->ntokens && (strcmp (l->tokens[i], s) == 0))
    return l->tokenvals[i];
  
  for (j = l->ntokens; j > i; j--) {
    l->tokens[j] = l->tokens[j-1];
    l->tokenvals[j] = l->tokenvals[j-1];
  }

  l->tokens[i] = (char *) malloc (sizeof(char)*(strlen(s)+1));
  l->tokenvals[i] = l->ntokens+l_offset;
  strcpy (l->tokens[i], s);
  l->ntokens++;
  return l->ntokens-1+l_offset;
}

/*-------------------------------------------------------------------------
 * remove the last recognized token
 *-----------------------------------------------------------------------*/
extern void lex_deltoken (LEX_T *l, int tok)
{
  int i, j;

  if (tok != l->ntokens-1+l_offset)
    fatal_error ("lex_deltoken: token is not the last one!");
  if (l->ntokens == 0)
    fatal_error ("lex_deltoken: can't delete builtins!");

  for (i=0; i < l->ntokens; i++)
    if (l->tokenvals[i] == tok)
      break;
  if (i == l->ntokens)
    fatal_error ("lex_deltoken: What on earth is going on here . . .");
  FREE (l->tokens[i]);
  for (j=i; j < l->ntokens-1; j++) {
    l->tokens[j] = l->tokens[j+1];
    l->tokenvals[j] = l->tokenvals[j+1];
  }
  l->ntokens--;
}

/*-------------------------------------------------------------------------
 * remove the last "m" tokens
 *-----------------------------------------------------------------------*/
extern void lex_deltokens (LEX_T *l, int m)
{
  int i;

  if (m > l->ntokens)
    fatal_error ("lex_deltokens: haven't defined that many tokens!");

  for (i=0; i < m; i++)
    lex_deltoken (l, l->ntokens+l_offset-1);
}

/*------------------------------------------------------------------------
 * Is this token already defined? 
 *  FIXME: binary search
 *------------------------------------------------------------------------
 */
extern int lex_istoken (LEX_T *l, const char *s)
{
  int i;
  for (i=0; i < l->ntokens; i++)
    if (strcmp (l->tokens[i], s) == 0) return 1;
  return 0;
}

/*-------------------------------------------------------------------------
 * get symbol from file
 *-----------------------------------------------------------------------*/
extern int lex_getsym (LEX_T *l)
{
  int i, j, m, oldi, ilimit, jlimit;
  int found = 0;
  int depth = 0;
  int flag;

  strcpy (l->tokprev, l->token);
  l->whitespace_loc = 0;
  l->token_loc = 0;
  l->token[0] = '\0';
  l->whitespace[0] = '\0';
  skipspace (l);

  if (lex_eof (l))
    return l->sym = l_eof;

  ilimit = 0; jlimit = l->ntokens-1;
  i = 0; j = l->ntokens-1;
  oldi = -1;
  if (j >= 0) 
    /* there are some registered tokens */
    while (!found) {

      /* I haven't found a match yet */

      if (lex_eof (l)) {
	if (oldi >= 0) {
	  found = 1;
	  break;
	}
	else {
	  /* no match */
	  return l->sym = l_eof;
	}
      }
      if (i != j) {
	/* do a binary search for character at this depth */
	while ((i+1) != j) {
	  m = (i+j)/2;
	  if (l->tokens[m][depth] <= l->ch)
	    i = m;
	  else
	    j = m;
	}
      }
      /* check if character was found */
      if (l->tokens[i][depth] != l->ch)
	if (l->tokens[j][depth] != l->ch) {
	  /* can't match at this depth, check previous depth for 
	     an exact match.
	     "xy" can be self-delimiting iff both x and y are not
	     alphanumeric.
	  */
	  found = (oldi >= 0) ? 1 : 0;
	  break;
	}
	else
	  /* found character at position j */
	  m = j;
      else
	/* found character at position i */
	m = i;
      addtok (l, l->ch);
      i = j = m;

      /* move i, j to boundary of matching range */

      while (i > ilimit && (l->tokens[i-1][depth] == l->ch)) i--;
      while (j < jlimit && (l->tokens[j+1][depth] == l->ch)) j++;

      /* increase depth by one */
      ilimit = i;
      jlimit = j;
      depth++;

      getch (l);
      flag = 0;
      /* whitespace separates tokens. Check if the next character
	 begins whitespace */
      if (isspace (l->ch))
	flag = 1;
      if (l->ch == '/') {
	getch (l);
	if (l->ch == '/' || l->ch == '*')
	  flag = 1;
	ungetch (l);
	l->ch = '/';
      }
      if (flag) {
	/* delimited token, check if there is a match at previous depth */
	if (l->tokens[i][depth] == '\0') {
	  found = 1;
	  oldi = i;
	  break;
	}
	else {
	  found = (oldi == -1) ? 0 : 1;
	  break;
	}
      }
      /* check if this could be a legal end-of-token */
      if (!(contiguous_tok(l->tokens[i][depth-1]) && contiguous_tok(l->ch)) &&
	  l->tokens[i][depth] == '\0')
	oldi = i;
    }
  if (found && (strcmp (l->tokens[oldi], ".") == 0) &&
      (lex_flags (l) & LEX_FLAGS_NODOTS) &&
      !(lex_flags (l) & LEX_FLAGS_NOREAL)) {
    found = 0;
  }
  if (!found) {
    /* no match for any known token. Try to match with id, integer, real,
       or string */

    /* put back input string */
    for (i=l->token_loc-1; i >=0; i--) {
      unsave (l);
      ungetch (l);
      l->ch = l->token[i];
    }
    l->token_loc = 0;
    l->token[0] = '\0';

    /* check for integer/real */

    if (isdigit (l->ch) || l->ch == '-' || l->ch == '.') {
      int found = 0;
      int tsign = 1;
      /* integer or real */
      m = 0;
      l->integer = 0;
      l->real = 0;
      if (l->ch == '-') {
	tsign = -1;
	addtok (l, l->ch);
	getch (l);
      }
      else if ((lex_flags(l) & (LEX_FLAGS_HEXINT|LEX_FLAGS_BININT)) && 
	       !(lex_flags(l) & LEX_FLAGS_DIGITID)) {
	if (l->ch == '0') {
	  found = 1;
	  addtok (l, l->ch);
	  getch (l);
	  if (l->ch == 'x' && (lex_flags (l) & LEX_FLAGS_HEXINT)) {
	    /* parse hex int */
	    addtok (l, l->ch);
	    getch (l);
	    while (isdigit (l->ch) || (l->ch >= 'a' && l->ch <= 'f')
		   || (l->ch >= 'A' && l->ch <= 'F')) {
	      found = 1;
	      l->integer <<= 4;
	      if (isdigit (l->ch))
		l->integer += l->ch - '0';
	      else if (l->ch >= 'a' && l->ch <= 'f')
		l->integer += 10 + l->ch - 'a';
	      else
		l->integer += 10 + l->ch - 'A';
	      addtok (l, l->ch);
	      getch (l);
	    }
	    /* return stuff here */
	    if (found) {
	      return l->sym = l_integer;
	    }
	    else {
	      return l->sym = l_err;
	    }
	  }
	  else if (l->ch == 'b' && (lex_flags (l) & LEX_FLAGS_BININT)) {
	    /* parse binary int */
	    addtok (l, l->ch);
	    getch (l);
	    while ((l->ch == '0') || (l->ch == '1')) {
	      found = 1;
	      l->integer <<= 1;
	      l->integer += l->ch - '0';
	      addtok (l, l->ch);
	      getch (l);
	    }
	    /* return stuff here */
	    if (found) {
	      return l->sym = l_integer;
	    }
	    else {
	      return l->sym = l_err;
	    }
	  }
	}
      }
      while (isdigit (l->ch)) {
	found = 1;
	l->integer *= 10;
	l->integer += l->ch - '0';
	addtok (l, l->ch);
	getch (l);
      }
      l->integer *= tsign;
      if (tsign == 1 && isidstart (l,l->ch) && (lex_flags (l) & LEX_FLAGS_DIGITID)) {
	if (l->ch == '\\') {
	  do {
	    addtok (l, l->ch);
	    getch (l);
	  } while (!lex_eof (l) && !isspace (l->ch));
	}
	else {
	  while (isid (l->ch) || isdigit (l->ch) ||
		 ((lex_flags (l)&LEX_FLAGS_IDSLASH) && l->ch == '/')) {
	    addtok (l, l->ch);
	    getch (l);
	  }
	}
	return l->sym = l_id;
      }
      if (l->ch == '.' && !(lex_flags(l) & LEX_FLAGS_NOREAL)) {
	/* must be a real */
	found = 2;
	m = 1;
	addtok (l, l->ch);
	getch (l);
	while (isdigit (l->ch)) {
	  m *= 10;
	  l->real *= 10;
	  l->real += l->ch - '0';
	  addtok (l, l->ch);
	  getch (l);
	}
	l->real /= (m+0.0);
	l->real *= tsign;
	l->real = l->real + (signed)l->integer;
      }
      if ((found || (lex_flags (l) & LEX_FLAGS_INTREALS)) && 
	  (l->ch == 'e' || l->ch == 'E') &&
	  !(lex_flags(l) & LEX_FLAGS_NOREAL)) {
	int putback, sign;
	/* exponent form. be careful. keep track of what to put back. */
	putback = l->token_loc;
	addtok (l, l->ch);
	getch (l);
	if (l->ch != '+' && l->ch != '-' && !isdigit(l->ch)) {
	  /* error in what we got */
	  for (i=l->token_loc-1; i >= putback; i--) {
	    unsave (l);
	    ungetch (l);
	    l->ch = l->token[i];
	  }
	  l->token_loc = putback;
	  l->token[putback] = '\0';
	}
	else {
	  int exp = 0;
	  sign = l->ch == '-' ? -1 : 1;
          if (l->ch == '+' || l->ch == '-') {
	     addtok (l, l->ch);
	     getch (l);
          }
	  if (!isdigit (l->ch)) {
	    for (i=l->token_loc-1; i >= putback; i--) {
	      unsave (l);
	      ungetch (l);
	      l->ch = l->token[i];
	    }
	    l->token_loc = putback;
	    l->token[putback] = '\0';
	  }
	  else {
	    m = 1;
	    exp = l->ch - '0';
	    addtok (l, l->ch);
	    getch (l);
	    while (isdigit (l->ch)) {
	      exp *= 10;
	      exp += l->ch - '0';
	      addtok (l, l->ch);
	      getch (l);
	    }
	    if (found == 1) l->real = l->integer;
	    if (sign > 0) {
	      for (i=0; i < exp; i++)
		l->real *= 10.0;
	    }
	    else {
	      for (i=0; i < exp; i++)
		l->real /= 10.0;
	    }
	  }
	}
      }
      if (found == 0) {
	addtok (l, l->ch);
	getch (l);
	return l->sym = l_err;
      }
      if (m) 
	return l->sym = l_real;
      else
	return l->sym = l_integer;
    }
    else if (isidstart (l,l->ch)) {
      /* identifier */
      if (l->ch == '\\') {
	do {
	  addtok (l, l->ch);
	  getch (l);
	} while (!lex_eof (l) && !isspace (l->ch));
      }
      else {
	do {
	  addtok (l, l->ch);
	  getch (l);
	} while (isid (l->ch) || isdigit (l->ch) ||
		 ((lex_flags (l)&LEX_FLAGS_IDSLASH) && l->ch == '/'));
      }
      return l->sym = l_id;
    }
    else if (l->ch == '"') {
      /* string */
      addtok (l, l->ch);
      do {
	if (lex_eof (l)) { /* unterminated string constant */ 
	  break;
	}
	getch (l);
	addtok (l, l->ch);
      } while (l->ch != '"');
      getch (l);
      return l->sym = l_string;
    }
    else {
      /* who knows? keep the character so that someone can parse it. */
      addtok (l, l->ch);
      getch (l);
      return l->sym = l_err;
    }
  }
  else {
    /* put back any characters that were not matched */
    i = strlen (l->tokens[oldi]);
    j = l->token_loc;
    while (j > i) {
      unsave (l);
      ungetch (l);
      l->ch = l->token[j-1];
      l->token_loc--;
      j--;
    }
    l->token[l->token_loc] = '\0';
    return l->sym = l->tokenvals[oldi];
  }
}


/*-------------------------------------------------------------------------
 * return "1" if end of file, "0" otherwise
 *-----------------------------------------------------------------------*/
extern int lex_eof (LEX_T *l)
{
  return (l->ch == '\0') ? 1 : 0;
}


/*-------------------------------------------------------------------------
 * free allocated space
 *-----------------------------------------------------------------------*/
extern void lex_free (LEX_T *l)
{
  int i;
  free (l->buf);
  for (i=0; i < l->ntokens; i++)
    free (l->tokens[i]);
  if (l->filename)
    free (l->filename);
  if (l->file) {
    if (!l->cfile) {
      fclose (l->inp.fp);
    }
    else {
      c_fclose (l->inp.fp);
    }
  }
  free (l->tokens);
  free (l->whitespace);
  free (l->token);
  free (l);
}


/*-------------------------------------------------------------------------
 * adds array of tokens
 *-----------------------------------------------------------------------*/
extern void lex_addtokenarray (LEX_T *l, const char **s, int *tok)
{
  int i;

  for (i=0; s[i]; i++)
    tok[i] = lex_addtoken (l, s[i]);
}

/*--------------------------------------------------------------------------
 * Checks to see if next token is "tok"
 *------------------------------------------------------------------------*/
int lex_have (LEX_T *l, int tok)
{
  if (lex_sym (l) == tok) {
    lex_getsym (l);
    return 1;
  }
  else
    return 0;
}

/*--------------------------------------------------------------------------
 * Checks to see if next token is "s"
 *------------------------------------------------------------------------*/
int lex_have_keyw (LEX_T *l, const char *s)
{
  if (strcmp (s, lex_tokenstring(l)) == 0) {
    lex_getsym (l);
    return 1;
  }
  else
    return 0;
}


/*--------------------------------------------------------------------------
 * Checks to see if next token is "s"
 *------------------------------------------------------------------------*/
int lex_is_keyw (LEX_T *l, const char *s)
{
  if (strcmp (s, lex_tokenstring(l)) == 0) {
    return 1;
  }
  else
    return 0;
}

/*------------------------------------------------------------------------
 * token name
 *------------------------------------------------------------------------
 */
extern const char *lex_tokenname (LEX_T *l, int tok)
{
  int i;
  if (tok < 0) return "-error-in-input-";
  if (tok < l_offset)
    return fixed_toks[tok];
  for (i=0; i < l->ntokens; i++)
    if (tok == l->tokenvals[i])
      return l->tokens[i];
  return "-error-in-input-";
}


/*--------------------------------------------------------------------------
 *  mustbe function
 *------------------------------------------------------------------------*/
extern void lex_mustbe (LEX_T *l, int tok)
{
  if (lex_have (l, tok))
    return; 
  else {
    int i;
    const char *nm1, *nm2;

    if (tok < l_offset)
      nm1 = fixed_toks[tok];
    else {
      for (i=0; i < l->ntokens; i++)
	if (tok == l->tokenvals[i]) {
	  nm1 = l->tokens[i];
	  break;
	}
      if (i == l->ntokens)
	fatal_error ("lex_mustbe: unknown token #%d", tok);
    }
    if (lex_sym(l) < l_offset)
      nm2 = fixed_toks[lex_sym(l)];
    else {
      for (i=0; i < l->ntokens; i++)
	if (lex_sym(l) == l->tokenvals[i])
	  nm2 = l->tokens[i];
    }
    fatal_error ("lex_mustbe: expected token: %s, received: %s\n"
		 "\tFile: %s, line: %d, col: %d\n", nm1, nm2,
		 l->filename ? l->filename : "-unknown-", lex_linenumber(l),
		 lex_colnumber (l));
  }
}


/*--------------------------------------------------------------------------
 * like mustbe, but with a string "s"
 *------------------------------------------------------------------------*/
void lex_mustbe_keyw (LEX_T *l, const char *s)
{
  if (lex_have_keyw (l,s)) 
    return;
  else {
    int i;
    const char *nm1, *nm2;

    nm1 = s;
    if (lex_sym(l) < l_offset)
      nm2 = fixed_toks[lex_sym(l)];
    else {
      for (i=0; i < l->ntokens; i++)
	if (lex_sym(l) == l->tokenvals[i])
	  nm2 = l->tokens[i];
    }
    fatal_error ("lex_mustbe_keyw: expected token: %s, received: %s [%s]\n"
		 "\tFile: %s, line: %d, col: %d\n", nm1, nm2, 
		 lex_tokenstring (l),
		 l->filename ? l->filename : "-unknown-", lex_linenumber(l),
		 lex_colnumber(l));
  }
}



/*--------------------------------------------------------------------------
 *  Return file, line, column string.
 *------------------------------------------------------------------------*/
extern char *lex_errstring (LEX_T *l)
{
  char *s;
  
  MALLOC (s,char,50 + strlen(l->filename ? l->filename : "-unknown-"));
  sprintf (s, "\tFile: %s, line: %d, col: %d",
	   l->filename ? l->filename : "-unknown-",
	   lex_linenumber (l), lex_colnumber (l));
  return s;
}


/*------------------------------------------------------------------------
 * Saved mode
 *------------------------------------------------------------------------
 */
extern void lex_begin_save (LEX_T *l)
{
  l->saving = 1;
  l->saved_loc = 0;
  l->saved[0] = '\0';
}


/*------------------------------------------------------------------------
 * end mode
 *------------------------------------------------------------------------
 */
extern void lex_end_save (LEX_T *l)
{
  l->saving = 0;
}


/*========================================================================*/
/*   
  Character mode parsing 
 */

static
int is_mem (char c, const char *s)
{
  while (*s && c != *s)
    s++;
  return c == *s;
}

/*------------------------------------------------------------------------
 * Skip characters in "s"
 *------------------------------------------------------------------------
 */
extern void lex_skip (LEX_T *l, const char *s)
{
  strcpy (l->tokprev, l->token);
  l->token_loc = 0;
  l->token[0] = '\0';
  while (!lex_eof (l) && is_mem (l->ch, s)) {
    addtok (l, l->ch);
    getch (l);
  }
}

/*------------------------------------------------------------------------
 * Skip characters in "s" and pretend it is whitespace
 *------------------------------------------------------------------------
 */
extern void lex_skipws (LEX_T *l, const char *s)
{
  l->whitespace_loc = 0;
  l->whitespace[0] = '\0';
  while (!lex_eof (l) && is_mem (l->ch, s)) {
    addws (l, l->ch);
    getch (l);
  }
}

/*------------------------------------------------------------------------
 * Skip whitespace
 *------------------------------------------------------------------------
 */
extern void lex_skipwhite (LEX_T *l)
{
  l->whitespace_loc = 0;
  l->whitespace[0] = '\0';
  skipspace (l);
}


/*------------------------------------------------------------------------
 * Check that the next characters exactly match the string "s"
 *------------------------------------------------------------------------
 */
extern void lex_mustbe_ckeyw (LEX_T *l, const char *s)
{
  strcpy (l->tokprev, l->token);
  l->token_loc = 0;
  l->token[0] = '\0';
  while (!lex_eof (l) && l->ch == *s) {
    s++;
    addtok (l, l->ch);
    getch (l);
  }
  if (*s)
    fatal_error ("lex_mustbe_ckeyw: expected token: %s, received: %s%c \n"
		 "\tFile: %s, line: %d, col: %d\n", s,
		 lex_tokenstring (l), l->ch,
		 l->filename ? l->filename : "-unknown-", lex_linenumber(l),
		 lex_colnumber(l));
}


/*------------------------------------------------------------------------
 * save position
 *------------------------------------------------------------------------
 */
#define ASSIGN(a,b,field) a->field = b->field

extern void lex_push_position (LEX_T *l)
{
  lex_position_t *cur;
  cur = (lex_position_t*)malloc (sizeof(lex_position_t));
  if (!cur)
    fatal_error ("lex_save_position: malloc failed, size=%d\n",
		 sizeof(lex_position_t));
  ASSIGN(cur,l,bufptr);
  ASSIGN(cur,l,lineno);
  ASSIGN(cur,l,colno);
  ASSIGN(cur,l,changed);
  ASSIGN(cur,l,ch);
  ASSIGN(cur,l,sym);
  ASSIGN(cur,l,integer);
  ASSIGN(cur,l,real);
  cur->ws = (char *)malloc(l->whitespace_loc+1);
  strcpy (cur->ws, l->whitespace);
  cur->tok = (char*)malloc (l->token_loc+1);
  strcpy (cur->tok, l->token);
  cur->prev = (char*)malloc(strlen(l->tokprev)+1);
  strcpy (cur->prev, l->tokprev);
  if (l->saving) {
    cur->save = (char*)malloc (l->saved_loc+1);
    strcpy (cur->save, l->saved);
  }
  else
    cur->save = NULL;
  cur->next = l->pos;
  l->pos = cur;
}


/*------------------------------------------------------------------------
 * pop position
 *------------------------------------------------------------------------
 */
extern void lex_pop_position (LEX_T *l)
{
  lex_position_t *tos;
  if (!l->pos)
    fatal_error ("lex_pop_position: no positions to pop!");
  tos = l->pos;
  l->pos = l->pos->next;
  free (tos->tok);
  free (tos->ws);
  free (tos->prev);
  if (tos->save)
    free (tos->save);
  free (tos);
}

/*------------------------------------------------------------------------
 * set position
 *------------------------------------------------------------------------
 */
extern void lex_set_position (LEX_T *l)
{
  lex_position_t *tos;
  if (!l->pos)
    fatal_error ("lex_set_position: no position on stack!");
  tos = l->pos;
  ASSIGN(l,tos,bufptr);
  ASSIGN(l,tos,lineno);
  ASSIGN(l,tos,colno);
  ASSIGN(l,tos,changed);
  ASSIGN(l,tos,ch);
  ASSIGN(l,tos,sym);
  ASSIGN(l,tos,integer);
  ASSIGN(l,tos,real);
  strcpy (l->whitespace, tos->ws); l->whitespace_loc = strlen (tos->ws);
  strcpy (l->token, tos->tok); l->token_loc = strlen (tos->tok);
  strcpy (l->tokprev, tos->prev);
  if (tos->save) {
    strcpy (l->saved, tos->save);
    l->saved_loc = strlen (tos->save);
  }
  else 
    l->saved_loc = 0;
}
