/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
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

static int hash (struct strHashtable *h, const char *k)
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
