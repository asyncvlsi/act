/*************************************************************************
 *
 *  Hash functions: from CACM, p. 679, (June 1990)
 *
 *  Copyright (c) 2003-2010, 2019 Rajit Manohar
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
#include <strings.h>
#include <stdlib.h>

#include "hash.h"
#include "misc.h"

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

int hash_function (int size, const char *k)
{
  unsigned int sum;
  const char *s;
  unsigned char c;

  sum = 0;
  if (*k == 0) {
    /* empty string! */
    return 0;
  }
  if (size <= (1<<8)) {
    /* byte index */
    sum = T[*k];
    for (s=k+1; *s; s++) {
      c = *s;
      sum = T[sum^c];
    }
  } 
  else if (size <= (1<<16)) {
    unsigned int sum1;
    sum = T[*k];
    sum1 = T[0xff & (1+*k)];
    for (s=k+1; *s; s++) {
      c = *s;
      sum = T[sum^c];
      sum1 = T[sum1^c];
    }
    sum |= sum1 << 8;
  } else if (size <= (1<<24)) {
    unsigned int sum1, sum2;
    sum = T[*k];
    sum1 = T[0xff & (1 + *k)];
    sum2 = T[0xff & (2 + *k)];
    for (s=k+1; *s; s++) {
      c = *s;
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
    for (s=k+1; *s; s++) {
      c = *s;
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

int
hash_function_continue (unsigned int size, const unsigned char *k, int len, 
			unsigned int sumprev,
			int iscont)
{
  register unsigned int sum=0;
  const unsigned char *s;
  unsigned char c;

  if (len == 0) {
    /* empty string! */
    return (iscont ? sumprev : 0);
  }
  if (size <= (1<<8)) {
    /* byte index */
    sum = iscont ? sumprev : 0;
    for (s=k; len > 0; len--) {
      c = *s++;
      sum = T[sum^c];
    }
  } else if (size <= (1<<16)) {
    unsigned int sum1;
    if (iscont) {
      sum = sumprev & 0xff;
      sum1 = (sumprev >> 8) & 0xff;
    }
    else {
      sum = T[*k];
      sum1 = T[0xff & (1+*k)];
      len--;
      k++;
    }
    for (s=k; len > 0; len--) {
      (c=*s++);
      sum = T[sum^c];
      sum1 = T[sum1^c];
    }
    sum |= sum1 << 8;
  } else if (size <= (1<<24)) {
    unsigned int sum1, sum2;
    if (iscont) {
      sum = sumprev & 0xff;
      sum1 = (sumprev >> 8) & 0xff;
      sum2 = (sumprev >> 16) & 0xff;
    }
    else {
      sum = T[*k];
      sum1 = T[0xff & (1 + *k)];
      sum2 = T[0xff & (2 + *k)];
      len--;
      k++;
    }
    for (s=k; len > 0; len--) {
      (c=*s++);
      sum = T[sum ^ c];
      sum1 = T[sum1 ^ c];
      sum2 = T[sum2 ^ c];
    }
    sum |= (sum1 << 8) | (sum2 << 16);
  } else {
    unsigned int sum1, sum2, sum3;
    if (iscont) {
      sum = sumprev & 0xff;
      sum1 = (sumprev >> 8) & 0xff;
      sum2 = (sumprev >> 16) & 0xff;
      sum3 = (sumprev >> 24) & 0xff;
    }
    else {
      sum = T[*k];
      sum1 = T[0xff & (1 + *k)];
      sum2 = T[0xff & (2 + *k)];
      sum3 = T[0xff & (3 + *k)];
      len--;
      k++;
    }
    for (s=k; len > 0; len--) {
      (c=*s++);
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



static int hash (struct Hashtable *h, const char *k)
{
  if (*k == 0) {
    return 0;
  }
  return hash_function (h->size, k);
}

static int ihash (struct iHashtable *h, long k)
{
  register unsigned int sum=0;
  int size = h->size;
  unsigned char c0, c1, c2, c3;
  
  c0 = k & 0xff;
  c1 = (k >> 8) & 0xff;
  c2 = (k >> 16) & 0xff;
  c3 = (k >> 24) & 0xff;

#define DO_HASH(var)				\
  do {						\
    (var) = T[(var)^c1];			\
    (var) = T[(var)^c2];			\
    (var) = T[(var)^c3];			\
  } while (0)

  if (size <= (1<<8)) {
    /* byte index */
    sum = T[c0];
    DO_HASH(sum);
  } else if (size <= (1<<16)) {
    unsigned int sum1;
    sum = T[c0];
    sum1 = T[0xff & (1+c0)];
    DO_HASH(sum);
    DO_HASH(sum1);

    sum |= sum1 << 8;
  } else if (size <= (1<<24)) {
    unsigned int sum1, sum2;
    sum = T[c0];
    sum1 = T[0xff & (1 + c0)];
    sum2 = T[0xff & (2 + c0)];
    DO_HASH(sum);
    DO_HASH(sum1);
    DO_HASH(sum2);
    sum |= (sum1 << 8) | (sum2 << 16);
  } else {
    unsigned int sum1, sum2, sum3;
    sum = T[c0];
    sum1 = T[0xff & (1 + c0)];
    sum2 = T[0xff & (2 + c0)];
    sum3 = T[0xff & (3 + c0)];
    DO_HASH(sum);
    DO_HASH(sum1);
    DO_HASH(sum2);
    DO_HASH(sum3);
    sum |= (sum1 << 8) | (sum2 << 16) | (sum3 << 24);
  }
  /* assumes sum is a power of 2, so calculates MOD */
  return sum & (size-1);
#undef DO_HASH
}


static void check_table (struct Hashtable *H)
{
  int i;
  hash_bucket_t *b;

  for(i=0; i < H->size; i++) {
    for (b = H->head[i]; b; b = b->next) {
      if (i != hash (H, b->key)) {
	printf ("XXX: hash table messed up!\n");
	printf ("Entry: `%s' [len=%d]\n", b->key, (int)strlen (b->key));
	exit (1);
      }
    }
  }
}

static void icheck_table (struct iHashtable *H)
{
  int i;
  ihash_bucket_t *b;

  for(i=0; i < H->size; i++) {
    for (b = H->head[i]; b; b = b->next) {
      if (i != ihash (H, b->key)) {
	printf ("XXX: hash table messed up!\n");
	printf ("Entry: `%d'\n", (unsigned int)b->key);
	exit (1);
      }
    }
  }
}

#if 0
static void ccheck_table (struct cHashtable *H)
{
  int i;
  chash_bucket_t *b;

  for(i=0; i < H->size; i++) {
    for (b = H->head[i]; b; b = b->next) {
      if (i != (*H->hash) (H->size, b->key)) {
	printf ("XXX: hash table messed up!\n");
	if (H->print) {
	  printf ("Entry: `");
	  (*H->print)(stdout, b->key);
	  printf ("'\n");
	}
	else {
	  printf ("Entry: `%x'\n", b->key);
	}
	exit (1);
      }
    }
  }
}
#endif


static void resize_table (struct Hashtable *H)
{
  struct Hashtable Hnew;
  hash_bucket_t *h, *tmp;
  int i, k;

  /* double it */
  Hnew.size = (H->size << 1);

  MALLOC (Hnew.head, hash_bucket_t *, Hnew.size);
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
      Assert (i == hash (H, h->key), "Um what?");
      k = hash (&Hnew, h->key);
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

static void iresize_table (struct iHashtable *H)
{
  struct iHashtable Hnew;
  ihash_bucket_t *h, *tmp;
  int i, k;

  /* double it */
  Hnew.size = (H->size << 1);

  MALLOC (Hnew.head, ihash_bucket_t *, Hnew.size);
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
      Assert (i == ihash (H, h->key), "Um what?");
      k = ihash (&Hnew, h->key);
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

static void cresize_table (struct cHashtable *H)
{
  struct cHashtable Hnew;
  chash_bucket_t *h, *tmp;
  int i, k;

  /* double it */
  Hnew.size = (H->size << 1);

  MALLOC (Hnew.head, chash_bucket_t *, Hnew.size);
  Hnew.n = H->n;
  Hnew.hash = H->hash;
  Hnew.match = H->match;
  Hnew.dup = H->dup;
  Hnew.free = H->free;
  Hnew.print = H->print;

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
      Assert (i == (*H->hash) (H->size, h->key), "Um what?");
      k = (*H->hash) (Hnew.size, h->key);
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

struct Hashtable *hash_new (int sz)
{
  struct Hashtable *h;
  int i;

  NEW (h, struct Hashtable);

  /* allocate 2^sz s.t. it is large enough */
  for(h->size=1; sz > h->size; h->size <<= 1) 
    ;

  MALLOC (h->head, hash_bucket_t *, h->size);
  h->n = 0;

  for (i=0; i < h->size; i++)
    h->head[i] = NULL;

  return h;
}

struct iHashtable *ihash_new (int sz)
{
  struct iHashtable *h;
  int i;

  NEW (h, struct iHashtable);

  /* allocate 2^sz s.t. it is large enough */
  for(h->size=1; sz > h->size; h->size <<= 1) 
    ;

  MALLOC (h->head, ihash_bucket_t *, h->size);
  h->n = 0;

  for (i=0; i < h->size; i++)
    h->head[i] = NULL;

  return h;
}

struct cHashtable *chash_new (int sz)
{
  struct cHashtable *h;
  int i;

  NEW (h, struct cHashtable);

  /* allocate 2^sz s.t. it is large enough */
  for(h->size=1; sz > h->size; h->size <<= 1) 
    ;

  MALLOC (h->head, chash_bucket_t *, h->size);
  h->n = 0;

  for (i=0; i < h->size; i++)
    h->head[i] = NULL;

  h->hash = NULL;
  h->match = NULL;
  h->dup = NULL;
  h->free = NULL;
  h->print = NULL;

  return h;
}

hash_bucket_t *hash_add (struct Hashtable *h, const char *k)
{
  int i;
  hash_bucket_t *b;

  if (h->n > (h->size << 2)) {
    resize_table (h);
  }

  i = hash (h, k);

  /* check for duplicate keys */
  for (b = h->head[i]; b; b = b->next) {
    if (strcmp (b->key, k) == 0) {
      fatal_error ("hash_add: key `%s' already present!\n", k);
    }
  }

  NEW (b, hash_bucket_t);
  b->key = Strdup (k);

  b->next = h->head[i];
  h->head[i] = b;

  h->n++;

  return b;
}

ihash_bucket_t *ihash_add (struct iHashtable *h, long k)
{
  int i;
  ihash_bucket_t *b;

  if (h->n > (h->size << 2)) {
    iresize_table (h);
  }

  i = ihash (h, k);

  /* check for duplicate keys */
  for (b = h->head[i]; b; b = b->next) {
    if (b->key == k) {
      fatal_error ("hash_add: key `%ld' already present!\n", k);
    }
  }

  NEW (b, ihash_bucket_t);
  b->key = k;

  b->next = h->head[i];
  h->head[i] = b;

  h->n++;

  return b;
}


chash_bucket_t *chash_add (struct cHashtable *h, void *k)
{
  int i;
  chash_bucket_t *b;

  if (h->n > (h->size << 2)) {
    cresize_table (h);
  }

  i = (*h->hash) (h->size, k);

  /* check for duplicate keys */
  for (b = h->head[i]; b; b = b->next) {
    if ((*h->match) (b->key, k)) {
      fatal_error ("chash_add: key already present!\n");
    }
  }

  NEW (b, chash_bucket_t);
  b->key = (*h->dup) (k);

  b->next = h->head[i];
  h->head[i] = b;

  h->n++;

  return b;
}

hash_bucket_t *hash_lookup (struct Hashtable *h, const char *k)
{
  int i;
  hash_bucket_t *b;

  /*check_table (h);*/

  i = hash (h, k);

  for(b = h->head[i]; b; b = b->next) {
    if (strcmp (b->key, k) == 0)
      return b;
  }

  return NULL;
}

ihash_bucket_t *ihash_lookup (struct iHashtable *h, long k)
{
  int i;
  ihash_bucket_t *b;

  /*icheck_table (h);*/

  i = ihash (h, k);

  for(b = h->head[i]; b; b = b->next) {
    if (b->key == k)
      return b;
  }

  return NULL;
}

chash_bucket_t *chash_lookup (struct cHashtable *h, void *k)
{
  int i;
  chash_bucket_t *b;

  /*ccheck_table (h);*/

  i = (*h->hash) (h->size, k);

  for(b = h->head[i]; b; b = b->next) {
    if ((*h->match) (b->key, k))
      return b;
  }
  return NULL;
}

void hash_delete (struct Hashtable *h, const char *k)
{
  int i;
  hash_bucket_t *b, *prev;

  i = hash (h, k);

  prev = NULL;

  for (b = h->head[i]; b; ) {
    if (strcmp (b->key, k) == 0) {
      if (!prev) {
	h->head[i] = h->head[i]->next;
      }
      else {
	prev->next = b->next;
      }
      FREE (b->key);
      FREE (b);
      h->n--;
      return;
    }
    else {
      prev = b;
      b = b->next;
    }
  }
  fatal_error ("hash_delete: key `%s' not found!", k);
}

void ihash_delete (struct iHashtable *h, long k)
{
  int i;
  ihash_bucket_t *b, *prev;

  i = ihash (h, k);

  prev = NULL;

  for (b = h->head[i]; b; ) {
    if (b->key == k) {
      if (!prev) {
	h->head[i] = h->head[i]->next;
      }
      else {
	prev->next = b->next;
      }
      FREE (b);
      h->n--;
      return;
    }
    else {
      prev = b;
      b = b->next;
    }
  }
  fatal_error ("hash_delete: key `%ld' not found!", k);
}

void chash_delete (struct cHashtable *h, void *k)
{
  int i;
  chash_bucket_t *b, *prev;

  i = (*h->hash) (h->size, k);

  prev = NULL;

  for (b = h->head[i]; b; ) {
    if ((*h->match) (b->key, k)) {
      if (!prev) {
	h->head[i] = h->head[i]->next;
      }
      else {
	prev->next = b->next;
      }
      (*h->free) (b->key);
      FREE (b);
      h->n--;
      return;
    }
    else {
      prev = b;
      b = b->next;
    }
  }
  fatal_error ("hash_delete: key not found!");
}

void hash_free (struct Hashtable *h)
{
  int i;
  hash_bucket_t *b, *t;

  for (i=0; i < h->size; i++) {
    for (b = h->head[i]; b; ) {
      t = b;
      b = b->next;
      FREE (t->key);
      FREE (t);
    }
  }
  FREE (h->head);
  FREE (h);
}

void ihash_free (struct iHashtable *h)
{
  int i;
  ihash_bucket_t *b, *t;

  for (i=0; i < h->size; i++) {
    for (b = h->head[i]; b; ) {
      t = b;
      b = b->next;
      FREE (t);
    }
  }
  FREE (h->head);
  FREE (h);
}

void chash_free (struct cHashtable *h)
{
  int i;
  chash_bucket_t *b, *t;

  for (i=0; i < h->size; i++) {
    for (b = h->head[i]; b; ) {
      t = b;
      b = b->next;
      (*h->free) (t->key);
      FREE (t);
    }
  }
  FREE (h->head);
  FREE (h);
}

void hash_clear (struct Hashtable *h)
{
  int i;
  hash_bucket_t *b, *t;

  for (i=0; i < h->size; i++) {
    for (b = h->head[i]; b; ) {
      t = b;
      b = b->next;
      FREE (t->key);
      FREE (t);
    }
    h->head[i] = NULL;
  }
  h->n = 0;
}

void ihash_clear (struct iHashtable *h)
{
  int i;
  ihash_bucket_t *b, *t;

  for (i=0; i < h->size; i++) {
    for (b = h->head[i]; b; ) {
      t = b;
      b = b->next;
      FREE (t);
    }
    h->head[i] = NULL;
  }
  h->n = 0;
}

void chash_clear (struct cHashtable *h)
{
  int i;
  chash_bucket_t *b, *t;

  for (i=0; i < h->size; i++) {
    for (b = h->head[i]; b; ) {
      t = b;
      b = b->next;
      (*h->free) (t->key);
      FREE (t);
    }
    h->head[i] = NULL;
  }
  h->n = 0;
}
