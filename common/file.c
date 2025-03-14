/*************************************************************************
 *
 *  Extension of lexer library, but with multiple files.
 * 
 *  Copyright (c) 2009, 2019 Rajit Manohar
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
#include <string.h>
#include "file.h"
#include "lex.h"
#include "misc.h"
#include "except.h"
#include "array.h"
#include "hash.h"

typedef struct __fl__ {
  LEX_T *l;
  struct __fl__ *next;
} flist;

typedef struct __el__ {
  int l, c;			/* line, column */
  char *file;			/* file */
  struct __el__ *next;
} elist;

L_A_DECL(char *, string_tab);

static struct Hashtable *ERRMSG = NULL;

struct _file_ {
  char **toks;
  int ntoks;
  int maxtoks;

  const char *errstring;
  elist *el;

  flist *l;
};


/* Cache file names! */
static char *string_to_string (const char *s)
{
  int i;

  for (i=0; i < A_LEN (string_tab); i++) {
    if (strcmp (s, string_tab[i]) == 0)
      return string_tab[i];
  }
  A_NEW (string_tab, char *);
  A_NEXT (string_tab) = Strdup (s);
  A_INC (string_tab);
  return string_tab[A_LEN(string_tab)-1];
}

/*
  Free error list
*/
static void free_el (elist *e)
{
  elist *f;
  while (e) {
    f = e->next;
    FREE (e);
    e = f;
  }
  return;
}


LFILE *file_open (const char *s)
{
  LFILE *l;

  NEW (l, LFILE);
  l->toks = NULL;
  l->ntoks = 0;
  l->maxtoks = 0;
  l->errstring = NULL;
  l->el = NULL;
  NEW (l->l, flist);
  l->l->l =lex_fopen (s);
  l->l->next = NULL;

  return l;
}

void file_close (LFILE *l)
{
  int i;
  if (l->l) {
    if (l->l->l) {
       lex_free (l->l->l);
    }
    FREE (l->l);
  }
  if (l->ntoks > 0) {
    for (i=0; i < l->ntoks; i++) {
      FREE (l->toks[i]);
    }
    FREE (l->toks);
  }
  free_el (l->el);
  FREE (l);
}

unsigned int file_flags (LFILE *l)
{
  return lex_flags (l->l->l);
}

void file_setflags (LFILE *l, unsigned int flags)
{
  lex_setflags (l->l->l, flags);
}


void file_push (LFILE *l, const char *s)
{
  int i;
  LEX_T *m;
  flist *x;

  m = lex_fopen (s);

  for (i=0; i < l->ntoks; i++)
    lex_addtoken (m, l->toks[i]);
  lex_getsym (m);

  NEW (x, flist);
  x->next = l->l;
  x->l = m;
  l->l = x;
  return;
}


int file_pop (LFILE *l)
{
  flist *x, *y;

  if (!lex_eof (l->l->l)) {
    except_throw (EXC_NULL_EXCEPTION, NULL);
  }
  if (l->l->l->pos) {
    return 0;
  }

  if (!l->l->next) {
    return 0;
  }

  /* XXX: why isn't this a constant time operation? Are we exporting
     flist pointers? 
  */
  x = l->l;
  lex_free (x->l);
  y = x->next;
  while (y->next) {
    x->l = y->l;
    x = y;
    y = x->next;
  }
  x->next = NULL;
  Assert (y, "Hmm...");
  FREE (y);
  return 1;
}


int file_sym (LFILE *l)
{
  while (lex_eof (l->l->l)) {
    if (!file_pop (l)) {
      return l_eof;
    }
  }
  return lex_sym (l->l->l);
}

int file_addtoken (LFILE *l, const char *s)
{
  int k, old_k;
  flist *m;

  if (lex_istoken (l->l->l, s)) {
    return lex_addtoken (l->l->l, s);
  }

  if (l->maxtoks < l->ntoks + 1) {
    if (l->maxtoks == 0) {
      l->maxtoks = 32;
      MALLOC (l->toks, char *, l->maxtoks);
    }
    else {
      l->maxtoks = 2*l->maxtoks;
      REALLOC (l->toks, char *, l->maxtoks);
    }
  }
  l->toks[l->ntoks++] = Strdup (s);
  old_k = lex_addtoken (l->l->l, s);

  for (m=l->l; m; m = m->next) {
    k = lex_addtoken (m->l, s);
    if (k != old_k) {
      fprintf (stderr, "This sucks!\n");
      except_throw (EXC_NULL_EXCEPTION, NULL);
    }
  }
  return k;
}

void file_deltoken (LFILE *l, const char *s)
{
  flist *m;

  Assert (l->ntoks > 0, "What?");
  if (strcmp (l->toks[l->ntoks-1], s) != 0) {
    fprintf (stderr, "deltoken error!");
    except_throw (EXC_NULL_EXCEPTION, NULL);
  }
  FREE (l->toks[l->ntoks-1]);
  l->ntoks--;
  for (m=l->l; m; m = m->next) {
    lex_deltokens (m->l, 1);
  }
}

void file_deltokens (LFILE *l, int n)
{
  flist *m;
  
  if (n <= 0) return;
  Assert (l->ntoks >= n, "What?");
  l->ntoks -= n;
  for (int i=l->ntoks; i < l->ntoks + n; i++) {
    FREE (l->toks[i]);
  }
  for (m=l->l; m; m = m->next) {
    lex_deltokens (m->l, n);
  }
}

int file_istoken (LFILE *l, const char *s)
{
  return lex_istoken (l->l->l, s);
}

int file_have (LFILE *l, int tok)
{
  file_sym (l);
  return lex_have (l->l->l, tok);
}

int file_have_keyw (LFILE *l, const char *s)
{
  file_sym (l);
  return lex_have_keyw (l->l->l, s);
}


int file_is_keyw (LFILE *l, const char *s)
{
  file_sym (l);
  return lex_is_keyw (l->l->l, s);
}

void file_mustbe (LFILE *l, int k)
{
  file_sym (l);
  lex_mustbe (l->l->l, k);
}

int file_getsym (LFILE *l)
{
  file_sym (l);
  return lex_getsym (l->l->l);
}

void file_push_position (LFILE *l)
{
  lex_push_position (l->l->l);
}

void file_pop_position (LFILE *l)
{
  lex_pop_position (l->l->l);
}

void file_set_position (LFILE *l)
{
  lex_set_position (l->l->l);
}

void file_get_position (LFILE *l, int *line, int *col, char **fname)
{
  *line = lex_linenumber (l->l->l);
  *col = lex_colnumber (l->l->l);
  *fname = string_to_string (l->l->l->filename ? l->l->l->filename : "-unknown-");
}


char *file_errstring (LFILE *l)
{
  char *s;
  elist *el;
  int len;
  
  if (!l->errstring) {
    return lex_errstring (l->l->l);
  }
  
  MALLOC (s, char, 4096);
  strncpy (s, l->errstring, 4095);
  len = strlen (s);

  for (el = l->el; el; el = el->next) {
    snprintf (s+len, 4096-len, "\n\tFile `%s', line: %d, col: %d", 
	      el->file, el->l, el->c);
    while (len < 4096 && s[len])
      len++;
  }
  return (char *)s;
}

int file_eof (LFILE *l)
{
  file_sym (l);
  return lex_eof (l->l->l);
}

char *file_tokenstring (LFILE *l)
{
  return lex_tokenstring (l->l->l);
}

const char *file_tokenname (LFILE *l, int tok)
{
  return lex_tokenname (l->l->l, tok);
}

char *file_prev (LFILE *l)
{
  return lex_prev (l->l->l);
}

long file_integer (LFILE *l)
{
  return lex_integer (l->l->l);
}

double file_real (LFILE *l)
{
  return lex_real (l->l->l);
}

static elist *mk_position (LFILE *l)
{
  elist *t, *u;
  flist *f;
  elist *m;

  f = l->l;
  t = NULL;
  u = NULL;
  while (f) {
    NEW (m, elist);
    m->next = NULL;
    m->file = string_to_string (f->l->filename);
    m->l = lex_linenumber (f->l);
    m->c = lex_colnumber (f->l);
    if (!t) {
      u = m;
    }
    else {
      t->next = m;
    }
    t = m;
    f  = f->next;
  }
  return u;
}

static int cmp_position (LFILE *l)
{
  elist *t;
  flist *f;

  t = l->el;
  f = l->l;

  if (strcmp (t->file, f->l->filename) != 0) {
    return 1;
  }
  if ((t->l < lex_linenumber (f->l)) ||
      ((t->l == lex_linenumber (f->l)) && t->c < lex_colnumber (f->l)))
    return 1;
  return 0;
}
  
void file_set_error (LFILE *l, const char *s)
{
  hash_bucket_t *b;
  if (!ERRMSG) {
    ERRMSG = hash_new (128);
  }
  if (!l->errstring) {
    free_el (l->el);
    l->el = mk_position (l);
    b = hash_lookup (ERRMSG, s);
    if (!b) {
      b = hash_add (ERRMSG, s);
    }
    l->errstring = b->key;
  }
  else {
    if (cmp_position (l)) {
      free_el (l->el);
      l->el = mk_position (l);
      b = hash_lookup (ERRMSG, s);
      if (!b) {
	b = hash_add (ERRMSG, s);
      }
      l->errstring = b->key;
    }
  }
}
