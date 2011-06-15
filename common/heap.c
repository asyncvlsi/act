/*-*-mode:c++-*-**********************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *  All Rights Reserved
 *
 *************************************************************************/
#include "heap.h"
#include "misc.h"

/*
 * Some invariants for heap h:
 *
 *  h->sz > 0 => h->key[0] = (MIN k: 0 <= k < h->sz : h->key[k])
 *  
 *  h->sz > 0 => (ALL k :: (h->key[k],h->value[k]) correspond to an insert)
 *
 *  0 <= h->sz && h->sz <= h->max
 *
 *  (0 <= i < h->sz && 2i+1 < h->sz && 2i+2 < h->sz) =>
 *       h->key[i] = MIN (h->key[i], h->key[2*i+1], h->key[2*i+2])
 */  


Heap *heap_new (int sz)
{
  unsigned int i;
  Heap *h;

  for (i=1; i < sz; i <<= 1) {
    if (i == 0) {
      i = 1;
      break;
    }
  }
  sz = i;

  Assert (sz > 0, "yikes!");

  NEW (h, Heap);
  
  h->sz = 0;
  h->max = sz;
  MALLOC (h->value, void *, h->max);
  MALLOC (h->key, heap_key_t, h->max);

  return h;
}

void heap_insert (Heap *h, heap_key_t key, void *v)
{
  int i, j, k;

  if (h->sz == h->max) {
    h->max *= 2;
    REALLOC (h->value, void *, h->max);
    REALLOC (h->key, heap_key_t, h->max);
  }
  h->key[h->sz] = key;
  h->value[h->sz] = v;
  
  k = h->sz;
  i = (k-1)/2;

  while (k > 0) {
    if (k & 1)
      j = k+1;
    else
      j = k-1;

    Assert (i <= h->sz, "Hmm");
    Assert (k <= h->sz, "HMM");

#define APPLY_SWAP(p,q)     do {       \
			         key = h->key[q];                          \
			         h->key[q] = h->key[p];                    \
			         h->key[p] = key;                          \
			                                                   \
			         v = h->value[q];                          \
			         h->value[q] = h->value[p];                \
			         h->value[p] = v;                          \
			    } while (0)

    if (h->key[k] < h->key[i]) {
      APPLY_SWAP (k,i);
    }
    if (j <= h->sz && h->key[j] < h->key[i]) {
      APPLY_SWAP (i,j);
    }
    k = i;
    i = (k-1)/2;
  }
  h->sz++;
}

void *heap_peek_min (Heap *h)
{
  if (h->sz == 0) return NULL;
  return h->value[0];
}

heap_key_t heap_peek_minkey (Heap *h)
{
  if (h->sz == 0) return 0;
  return h->key[0];
}

void *heap_remove_min (Heap *h)
{
  void *retval, *v;
  heap_key_t key;
  int i, j, k;
  int res1, res2;

  if (h->sz == 0) return NULL;

  retval = h->value[0];
  h->sz--;
  
  h->value[0] = h->value[h->sz];
  h->key[0] = h->key[h->sz];

  i = 0;
  j = 2*i+1;
  k = j+1;

  while (j < h->sz) {
    if (h->key[j] < h->key[i]) {
      if (k >= h->sz || h->key[j] < h->key[k]) {
	APPLY_SWAP (i,j);
	i = j;
      }
      else {
	APPLY_SWAP (i,k);
	i = k;
      }
    }
    else if (k < h->sz && (h->key[k] < h->key[i])) {
      APPLY_SWAP (i,k);
      i = k;
    }
    else {
      break;
    }
    j = 2*i+1;
    k = j+1;
  }
  return retval;
}

void *heap_remove_min_key (Heap *h, heap_key_t *keyp)
{
  void *retval, *v;
  heap_key_t key;
  int i, j, k;
  int res1, res2;

  if (h->sz == 0) return NULL;

  retval = h->value[0];
  *keyp = h->key[0];
  h->sz--;
  
  h->value[0] = h->value[h->sz];
  h->key[0] = h->key[h->sz];

  i = 0;
  j = 2*i+1;
  k = j+1;

  while (j < h->sz) {
    if (h->key[j] < h->key[i]) {
      if (k >= h->sz || h->key[j] < h->key[k]) {
	APPLY_SWAP (i,j);
	i = j;
      }
      else {
	APPLY_SWAP (i,k);
	i = k;
      }
    }
    else if (k < h->sz && (h->key[k] < h->key[i])) {
      APPLY_SWAP (i,k);
      i = k;
    }
    else {
      break;
    }
    j = 2*i+1;
    k = j+1;
  }
  return retval;
}



void heap_insert_random (Heap *h, void *v)
{
  /* insert into a random spot in the heap */
  if (h->sz == 0)
    heap_insert (h, 0, v);
  else
    heap_insert (h, h->key[(random()%h->sz)]+1, v);
}

