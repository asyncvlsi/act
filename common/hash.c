/*************************************************************************
 *
 *  Copyright (c) 2003-2010 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>

#include "hash.h"
#include "misc.h"

static int T[] =
{  1, 87, 49, 12,176,178,102,166,121,193,  6, 84,249,230, 44,163,
  14,197,213,181,161, 85,218, 80, 64,239, 24,226,236,142, 38,200,
 110,177,104,103,141,253,255, 50, 77,101, 81, 18, 45, 96, 31,222,
  25,107,190, 70, 86,237,240, 34, 72,242, 20,214,244,227,149,235,
  97,234, 57, 22, 60,250, 82,175,208,  5,127,199,111, 62,135,248,
 174,169,211, 58, 66,154,106,195,245,171, 17,187,182,179,  0,243,
 132, 56,148, 75,128,133,158,100,130,126, 91, 13,153,246,216,219,
 119, 68,223, 78, 83, 88,201, 99,122, 11, 92, 32,136,114, 52, 10,
 138, 30, 48,183,156, 35, 61, 26,143, 74,251, 94,129,162, 63,152,
 170,  7,115,167,241,206,  3,150, 55, 59,151,220, 90, 53, 23,131,
 125,173, 15,238, 79, 95, 89, 16,105,137,225,224,217,160, 37,123,
 118, 73,  2,157, 46,116,  9,145,134,228,207,212,202,215, 69,229,
  27,188, 67,124,168,252, 42,  4, 29,108, 21,247, 19,205, 39,203,
 233, 40,186,147,198,192,155, 33,164,191, 98,204,165,180,117, 76,
 140, 36,210,172, 41, 54,159,  8,185,232,113,196,231, 47,146,120,
  51, 65, 28,144,254,221, 93,189,194,139,112, 43, 71,109,184,209};

int hash_function (int size, const char *k)
{
  register unsigned int sum=0;
  const char *s;
  unsigned char c;

/*
 * 8, 16, 24, or 32 bit hashing function--- from CACM June 1990, p.679
 */
  if (*k == 0) {
    /* empty string! */
    return 0;
  }

  if (size <= (1<<8)) {
    /* byte index */
    sum = 0;
    for (s=k; c=*s++; ) {
      sum = T[sum^c];
    }
  } else if (size <= (1<<16)) {
    unsigned int sum1;
    sum = T[*k];
    sum1 = T[0xff & (1+*k)];
    for (s=k+1; c=*s++; ) {
      sum = T[sum^c];
      sum1 = T[sum1^c];
    }
    sum |= sum1 << 8;
  } else if (size <= (1<<24)) {
    unsigned int sum1, sum2;
    sum = T[*k];
    sum1 = T[0xff & (1 + *k)];
    sum2 = T[0xff & (2 + *k)];
    for (s=k+1; c=*s++; ) {
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
    for (s=k+1; c=*s++; ) {
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

/*
 * 8, 16, 24, or 32 bit hashing function--- from CACM June 1990, p.679
 */
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
      c=*s++;
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
      c=*s++;
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
      c=*s++;
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
  register unsigned int sum=0;
  int size = h->size;
  const char *s;
  unsigned char c;

/*
 * 8, 16, 24, or 32 bit hashing function--- from CACM June 1990, p.679
 */
  if (*k == 0) {
    /* empty string! */
    return 0;
  }

  if (size <= (1<<8)) {
    /* byte index */
    sum = 0;
    for (s=k; c=*s++; ) {
      sum = T[sum^c];
    }
  } else if (size <= (1<<16)) {
    unsigned int sum1;
    sum = T[*k];
    sum1 = T[0xff & (1+*k)];
    for (s=k+1; c=*s++; ) {
      sum = T[sum^c];
      sum1 = T[sum1^c];
    }
    sum |= sum1 << 8;
  } else if (size <= (1<<24)) {
    unsigned int sum1, sum2;
    sum = T[*k];
    sum1 = T[0xff & (1 + *k)];
    sum2 = T[0xff & (2 + *k)];
    for (s=k+1; c=*s++; ) {
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
    for (s=k+1; c=*s++; ) {
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

/*
 * 8, 16, 24, or 32 bit hashing function--- from CACM June 1990, p.679
 */
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
	printf ("Entry: `%s' [len=%ld]\n", b->key, strlen (b->key));
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
