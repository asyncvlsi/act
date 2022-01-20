/*************************************************************************
 *
 *  Copyright (c) 1996, 2020 Rajit Manohar
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
/*************************************************************************
 *
 *  lispPrint.c -- 
 *
 *   Stuff that prints out the internals of lists.
 *
 **************************************************************************
 */

#include <stdio.h>

#include "lisp.h"
#include "lispInt.h"
#include "lispargs.h"
#include "act/common/hash.h"

#define IsSpace(c)        ((c) == ' ')

/*
  Print t into s, expanding control characters.
  flag = 1: put quotes around if spaces present
  flag = 0: don't
*/
static
void
LispBufPrintName (char *s, char *t, int flag)
{
  int i,j, spc;
  i=0;
  spc=0;
  for (j=0; t[j]; j++)
    if (IsSpace (t[j])) {
      spc=1;
      break;
    }
  if (spc && flag) s[i++] = '\"';
  for (j=0; t[j]; j++)
    if (t[j] >= 32)
      s[i++] = t[j];
    else {
      s[i++] = '^';
      s[i++] = t[j]+64;
    }
  if (spc && flag)
    s[i++] = '\"';
  s[i]='\0';
}

/*------------------------------------------------------------------------
 *
 *  LispPrint --
 *
 *      Print the object out to the text stream.
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      Output appears in text window.
 *
 *------------------------------------------------------------------------
 */

static char obuf[128];
//static HashTable PrintTable, GenTable;
struct pHashtable *PrintTable, *GenTable;
static int num_refs;

static void
_LispPrint (FILE *fp, LispObj *l)
{
  phash_bucket_t *b;
  int i;
  if (LispInterruptExecution) return;
  b = phash_lookup (PrintTable, l);
  if (b) {
    i = b->i;
    if (i) {
      /* i > 0 */
      fprintf (fp, "#%d", i);
      return;
    }
    else {
      /* not printed; set the print tag */
      b->i = ++num_refs;
      fprintf (fp, "#%d:", num_refs);
    }
  }
  switch (LTYPE(l)) {
  case S_INT:
    fprintf (fp,"%ld", LINTEGER(l));
    break;
  case S_FLOAT:
    fprintf (fp, "%lf", LFLOAT(l));
    break;
  case S_STRING:
    LispBufPrintName (obuf, LSTR(l),0);
    fprintf (fp, "\"%s\"", obuf);
    break;
  case S_BOOL:
    fprintf (fp, "#%c", LINTEGER(l) ? 't' : 'f');
    break;
  case S_SYM:
    LispBufPrintName (obuf, LSYM(l),1);
    fprintf (fp, "%s", obuf);
    break;
  case S_LIST:
    fprintf (fp, "(");
    if (LLIST(l)) {
      Sexp *s;
      s = LLIST(l);
      _LispPrint (fp,CAR(s));
      while ((LTYPE(CDR(s)) == S_LIST) && LLIST(CDR(s))) {
	fprintf (fp, " ");
	b = phash_lookup (PrintTable, CDR(s));
	if (b) {
	  i = b->i;
	  if (i) {
	    /* i > 0 */
	    fprintf (fp, "#%d", i);
	    goto done;
	  }
	  else {
	    /* not printed; set the print tag */
	    b->i = ++num_refs;
	    fprintf (fp, "#%d:", num_refs);
	  }
	}
	s = LLIST(CDR(s));
	_LispPrint (fp,CAR(s));
      }
      if (LTYPE(CDR(s)) != S_LIST) {
	fprintf (fp, " . ");
	_LispPrint (fp,CDR(s));
      }
    }
done:
    fprintf (fp, ")");
    break;
  case S_LAMBDA:
    fprintf (fp, "(lambda ");
    _LispPrint (fp,ARG2(LUSERDEF(l)));
    fprintf (fp, " ");
    _LispPrint (fp,ARG4(LUSERDEF(l)));
    fprintf (fp, ")");
    break;
  case S_LAMBDA_BUILTIN:
  case S_MAGIC_BUILTIN:
    fprintf (fp, "#proc");
    break;
  default:
    break;
  }
}


static void
_LispGenTable (LispObj *l)
{
  phash_bucket_t *b;
  int i;
  if (LispInterruptExecution) return;
  b = phash_lookup (GenTable, l);
  if (b) {
    b->i++;
    return;
  }
  b = phash_add (GenTable, l);
  b->i = 0;
  switch (LTYPE(l)) {
  case S_INT:
  case S_FLOAT:
  case S_STRING:
  case S_BOOL:
  case S_SYM:
  case S_LAMBDA_BUILTIN:
  case S_LAMBDA_BUILTIN_DYNAMIC:
  case S_MAGIC_BUILTIN:
    break;
  case S_LIST:
    if (LLIST(l)) {
      Sexp *s;
      s = LLIST(l);
      _LispGenTable (CAR(s));
      while ((LTYPE(CDR(s)) == S_LIST) && LLIST(CDR(s))) {
	b = phash_lookup (GenTable, CDR(s));
	if (b) {
	  b->i++;
	  return;
	}
	b = phash_add (GenTable, CDR(s));
	b->i = 0;
	s = LLIST(CDR(s));
	_LispGenTable (CAR(s));
      }
      if (LTYPE(CDR(s)) != S_LIST) {
	_LispGenTable (CDR(s));
      }
    }
    break;
  case S_LAMBDA:
    _LispGenTable (ARG2(LUSERDEF(l)));
    _LispGenTable (ARG4(LUSERDEF(l)));
    break;
  default:
    break;
  }
}


void
LispPrint (FILE *fp, LispObj *l)
{
  phash_bucket_t *b;
  int i;

  num_refs = 0;
  PrintTable = phash_new (128);
  GenTable = phash_new (128);
  
  _LispGenTable (l);

  for (i=0; i < GenTable->size; i++) {
    for (b = GenTable->head[i]; b; b = b->next) {
      if (b->i) {
	phash_bucket_t *u = phash_add (PrintTable, b->key);
	u->i = 0;
      }
    }
  }
  _LispPrint (fp,l);
  phash_free (PrintTable);
  phash_free (GenTable);
}


/*------------------------------------------------------------------------
 *
 *  LispPrintType --
 *
 *      Print the type of the object out to the text stream.
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      Output appears in text window.
 *
 *------------------------------------------------------------------------
 */

void
LispPrintType (FILE *fp, LispObj *l)
{
  switch (LTYPE(l)) {
  case S_INT:
    fprintf (fp, "#integer");
    break;
  case S_FLOAT:
    fprintf (fp, "#float");
    break;
  case S_STRING:
    fprintf (fp, "#string");
    break;
  case S_BOOL:
    fprintf (fp, "#boolean");
    break;
  case S_SYM:
    fprintf (fp, "#symbol");
    break;
  case S_LIST:
    fprintf (fp, "#list");
    break;
  case S_LAMBDA:
    fprintf (fp, "#proc-userdef");
    break;
  case S_LAMBDA_BUILTIN:
    fprintf (fp, "#proc-builtin");
    break;
  case S_LAMBDA_BUILTIN_DYNAMIC:
    fprintf (fp, "#proc-dynamic");
    break;
  case S_MAGIC_BUILTIN:
    fprintf (fp, "#proc-magic");
    break;
  default:
    break;
  }
}
