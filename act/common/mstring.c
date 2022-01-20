/*************************************************************************
 *
 *  Managed string library
 *
 *  Copyright (c) 2011, 2019 Rajit Manohar
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
#include "mstring.h"
#include "misc.h"

struct strHashtable {
  int size;
  mstring_t **head;
  int n;
}; 

static struct strHashtable *sH = NULL;

/*-- note this is copied from hash.c --*/

static int T[] =
{
  103,152,181,  5,117,148,142, 43, 25, 16, 78, 58,114,155,197, 63,
  76,241,160, 74, 59,199,  2, 62,150,240,  3,191, 71,147,129, 11,
  70,  7, 51,221,109, 50,201,118,253, 36,115,213,116,226,234, 56,
  91, 79, 28,177, 95,124, 94,157, 39,  0,206,149, 69,  9, 44,231,
  8, 52,242,106,153,202, 88, 37, 54,193,168,167, 24,  6,247, 64,
  175,189,166,111, 19,169,209,204,138, 61,184,121,128,215,239,222,
  162,196,174,237,127,136,154,212,187,164, 68,248,145,214,176, 34,
  245, 80,188,137, 35,251,194, 66,144,190,163,110,108,252, 42, 81,
  133,233, 85, 99,130,244, 57, 97,119,250,183, 83, 87,125,141,140,
  113,210, 82, 22,200,223, 26, 18,180,192,216,134, 60,235, 32, 46,
  219, 41, 77,143, 86, 96, 72,151,159,179,230,126, 15, 45, 38,211,
  243,229, 10,207,208,132,255,104,123, 47, 49, 14,198,217, 98,  1,
  203,105,135,205, 21,139,156, 20,107,170,227,220,131, 75, 13, 73,
  89,249, 48,158, 30, 40,225, 93, 67, 55, 92,238,246,182,186, 90,
  236, 65,172,101,171,112,  4,195,173,120, 33,228, 53, 12,161,100,
  102,218, 23, 29,254,232, 31,224,122, 84,185,165,146,178, 17, 27
};

static int hash (struct strHashtable *h, const char *k)
{
  register unsigned int sum=0;
  int size = h->size;
  const char *s;
  unsigned char c;

  if (*k == 0) {
    /* empty string! */
    return 0;
  }

  if (size <= (1<<8)) {
    /* byte index */
    sum = 0;
    for (s=k; (c=*s++); ) {
      sum = T[sum^c];
    }
  } else if (size <= (1<<16)) {
    unsigned int sum1;
    sum = T[*k];
    sum1 = T[0xff & (1+*k)];
    for (s=k+1; (c=*s++); ) {
      sum = T[sum^c];
      sum1 = T[sum1^c];
    }
    sum |= sum1 << 8;
  } else if (size <= (1<<24)) {
    unsigned int sum1, sum2;
    sum = T[*k];
    sum1 = T[0xff & (1 + *k)];
    sum2 = T[0xff & (2 + *k)];
    for (s=k+1; (c=*s++); ) {
      sum = T[sum ^ c];
      sum1 = T[sum1 ^ c];
      sum2 = T[sum2 ^ c];
    }
    sum |= (sum1 << 8) | (sum2 << 16);
  } else {
    unsigned int sum1, sum2, sum3;
    sum = T[*k];
    sum1 = T[0xff & (1 + *k)];
    sum2 = T[0xff & (2 + *k)];
    sum3 = T[0xff & (3 + *k)];
    for (s=k+1; (c=*s++); ) {
      sum = T[sum ^ c];
      sum1 = T[sum1 ^ c];
      sum2 = T[sum2 ^ c];
      sum3 = T[sum3 ^ c];
    }
    sum |= (sum1 << 8) | (sum2 << 16) | (sum3 << 24);
  }
  /* assumes sum is a power of 2, so calculates MOD */
  return sum & (size-1);
}

static void check_table (struct strHashtable *H)
{
  int i;
  mstring_t *h;

  for(i=0; i < H->size; i++) {
    for (h = H->head[i]; h; h = h->next) {
      if (i != hash (H, &h->s[0])) {
	printf ("XXX: hash table messed up!\n");
	printf ("Entry: `%s' [len=%d]\n", &h->s[0], (int)strlen (&h->s[0]));
	exit (1);
      }
    }
  }
}


static void resize_table (struct strHashtable *H)
{
  struct strHashtable Hnew;
  mstring_t *h, *tmp;
  int i, k;

  /* double it */
  Hnew.size = (H->size << 1);

  MALLOC (Hnew.head, mstring_t *, Hnew.size);
  Hnew.n = H->n;

  /* initialize table */
  for(i=0; i < Hnew.size; i++) {
    Hnew.head[i] = NULL;
  }
  
  /* move buckets--don't need to allocate them */
  for(i=0; i < H->size; i++) {
    h = H->head[i];
    tmp = NULL;
    while (h) {
      tmp = h->next;
      Assert (i == hash (H, &h->s[0]), "Um what?");
      k = hash (&Hnew, &h->s[0]);
      h->next = Hnew.head[k];
      Hnew.head[k] = h;
      h = tmp;
    }
  }
  /* delete old bucket array */
  FREE (H->head);

  /* update table */
  *H = Hnew;
}

static void string_init (void)
{
  int i;

  if (sH) return;
  NEW (sH, struct strHashtable);

  sH->size = 16;
  sH->n = 0;
  MALLOC (sH->head, mstring_t *, sH->size);
  
  for (i=0; i < sH->size; i++)
    sH->head[i] = NULL;

  return;
}

mstring_t *string_create (const char *s)
{
  int i;
  mstring_t *b;

  string_init ();

  if (sH->n > (sH->size << 2)) {
    resize_table (sH);
  }

  i = hash (sH, s);

  /* check for duplicate keys */
  for (b = sH->head[i]; b; b = b->next) {
    if (strcmp (&b->s[0], s) == 0) {
      break;
    }
  }
  if (b) {
    b->ref++;
  }
  else {
    b = (mstring_t *)malloc (sizeof (mstring_t) + strlen (s)*sizeof (char));
    strcpy (&b->s[0], s);
    b->next = sH->head[i];
    b->ref = 1;
    sH->head[i] = b;
    sH->n++;
  }

  /*check_table (sH);*/

  return b;
}
		       
mstring_t *string_dup (mstring_t *s)
{
  s->ref++;
  return s;
}

void string_free (mstring_t *s)
{
  s->ref--;
  /* if s->ref == 0... */
}

const char *string_char (mstring_t *s)
{
  return &s->s[0];
}
