/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2007, 2018-2019 Rajit Manohar
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
#include <stdlib.h>
#include <string.h>
#include <common/names.h>
#include <common/hash.h>
#include <common/lex.h>
#include <common/misc.h>

static int syms[32];

enum {
  AND = 0,
  OR = 1,
  NOT = 2,
  PLUS = 3,
  MINUS = 4,
  ARROW = 5,
  EQUAL = 6,
  LPAR = 7,
  RPAR = 8,
  COMMA = 9
};

void emit_idx (IDX_TYPE idx)
{
  unsigned char c;
  /* emit bottom 7 bits at a time */
  while (idx != 0) {
    c = idx & 0x7f;
    putchar (c | 0x80);
    idx = idx >> 7;
  }
}

int main (int argc, char **argv)
{
  LEX_T *l;
  struct Hashtable *H;
  hash_bucket_t *b;
  NAMES_T *N;

  if (argc != 2) {
    fatal_error ("Usage: prspack <name>");
  }
  l = lex_file (stdin);

  N = names_create (argv[1], 1e7);
  H = hash_new (1024);

  syms[AND] = lex_addtoken (l, "&");
  syms[OR] = lex_addtoken (l, "|");
  syms[NOT] = lex_addtoken (l, "~");
  syms[PLUS] = lex_addtoken (l, "+");
  syms[MINUS] = lex_addtoken (l, "-");
  syms[ARROW] = lex_addtoken (l, "->");
  syms[EQUAL] = lex_addtoken (l, "=");
  syms[LPAR] = lex_addtoken (l, "(");
  syms[RPAR] = lex_addtoken (l, ")");
  syms[COMMA] = lex_addtoken (l, ",");

  lex_getsym (l);
  while (!lex_eof (l)) {
    if (lex_have (l, syms[EQUAL]) || lex_have_keyw (l, "connect")) {
      char *s1, *s2;
      /* connection, special! */
      lex_mustbe (l, l_string);
      s1 = Strdup (lex_prev (l)+1);
      s1[strlen (s1)-1] = '\0';
      lex_mustbe (l, l_string);
      s2 = Strdup (lex_prev (l)+1);
      s2[strlen (s2)-1] = '\0';
      if (strcmp (s1, s2) == 0) {
	/* nothing */
      }
      else {
	IDX_TYPE idx1, idx2;
	b = hash_lookup (H, s1);
	if (!b) {
	  b = hash_add (H, s1);
	  b->v = (void *)(long)names_newname (N, s1);
	}
	idx1 = (IDX_TYPE) b->v;
	b = hash_lookup (H, s2);
	if (!b) {
	  b = hash_add (H, s2);
	  b->v = (void *)(long)names_newname (N, s2);
	}
	idx2 = (IDX_TYPE) b->v;

	names_addalias (N, idx1, idx2);
      }
      FREE (s1);
      FREE (s2);
    }
    else if (lex_have_keyw (l, "mk_exclhi") ||
	     lex_have_keyw (l, "mk_excllo") ||
	     lex_have_keyw (l, "mk_excl") ||
	     lex_have_keyw (l, "rand_init") ||
	     lex_have_keyw (l, "hazard")) {
      int type = 0;
      if (strcmp (lex_prev (l), "mk_excllo") == 0) {
	putchar ('\04');
	type = '\04';
      }
      else if ((strcmp (lex_prev (l), "mk_exclhi") == 0) ||
	       (strcmp (lex_prev (l), "mk_excl") == 0)) {
	putchar ('\05');
	type = '\05';
      }
      else if (strcmp (lex_prev (l), "rand_init") == 0) {
	putchar ('\07');
	type = '\07';
      }
      else {
	putchar ('\010');
	type = '\010';
      }
      lex_mustbe (l, syms[LPAR]);
      putchar ('(');
      do {
	char *s;
	IDX_TYPE idx;
	lex_mustbe (l, l_string);
	s = Strdup (lex_prev (l)+1);
	s[strlen (s)-1] = '\0';
	b = hash_lookup (H, s);
	if (!b) {
	  b = hash_add (H, s);
	  b->v = (void *)(long)names_newname (N, s);
	}
	FREE (s);
	idx = (IDX_TYPE)b->v;
	emit_idx (idx);
	if (lex_sym (l) == syms[COMMA]) {
	  putchar (',');
	}
      } while (lex_have (l, syms[COMMA]));
      lex_mustbe (l, syms[RPAR]);
      putchar (')');
    }
    else if (lex_have_keyw (l, "weak")) {
      putchar ('\02');
    }
    else if (lex_have_keyw (l, "after")) {
      putchar ('\03');
      lex_mustbe (l, l_integer);
      printf ("%ld", lex_integer (l));
    }
    else if (lex_have_keyw (l, "unstab")) {
      putchar ('\01');
    }
    else if (lex_have (l, l_string)) {
      /* put string into disk table! */
      IDX_TYPE idx;
      char *s = Strdup (lex_prev(l)+1);
      s[strlen (s)-1] = '\0';

      b = hash_lookup (H, s);
      if (b) {
	/* already there */
      }
      else {
	b = hash_add (H, s);
	b->v = (void *)(long)names_newname (N, s);
      }
      FREE (s);
      idx = (IDX_TYPE)b->v;
      emit_idx (idx);
    }
    else {
      /* pass-thru */
      printf ("%s", lex_tokenstring (l));
      lex_getsym (l);
    }
  }
  names_close (N);
  lex_free (l);
  return 0;
}
