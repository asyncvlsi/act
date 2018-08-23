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

void heap_free (Heap *h, void (*free_element)(void *))
{
  int i;

  for (i=0; i < h->sz; i++) {
    (*free_element) (h->value[i]);
  }
  FREE (h->value);
  FREE (h->key);
  FREE (h);
  return;
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


void heap_save (Heap *h, FILE *fp, void (*save_element)(FILE *, void *))
{
  int i;
  fprintf (fp, "%d ", h->sz);
  for (i=0; i < h->sz; i++) {
    fprintf (fp, "%llu ", h->key[i]);
    (*save_element) (fp, h->value[i]);
    fprintf (fp, "\n");
  }
}

Heap *heap_restore (FILE *fp, void *(restore_element)(FILE *))
{
  int i;
  int sz;
  Heap *h;
  heap_key_t key;
  void *value;

  if (fscanf (fp, "%d", &sz) != 1) Assert (0, "Checkpoint read error");
  Assert (sz >= 0, "Hmm");

  h = heap_new (sz);

  for (i=0; i < sz; i++) {
    if (fscanf (fp, "%llu", &key) != 1) Assert (0, "Checkpoint read error");
    value = (*restore_element) (fp);
    heap_insert (h, key, value);
  }
  return h;
}

int heap_update_key (Heap *h, heap_key_t key, void *v)
{
  int ii, i, j, k;

  for (i=0; i < h->sz; i++) {
    if (h->value[i] == v) break;
  }
  if (i == h->sz) return 0;

  /* update the key, and move the node into place */
  h->key[i] = key;

  /* now check propagate down */
  j = 2*i+1;
  k = j+1;

  ii = i;

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
      APPLY_SWAP(i,k);
      i = k;
    }
    else {
      break;
    }
    j = 2*i+1;
    k = j+1;
  }

  if (i != ii) {
    /*check_heap ("step1", h);*/
    return 1;
  }
  
  k = ii;
  i = (k-1)/2;

  while (k > 0) {
    if (k & 1)
      j = k+1;
    else 
      j = k-1;
    if (h->key[k] <= h->key[i]) {
      APPLY_SWAP (k,i);
    }
    if (j < h->sz && h->key[j] < h->key[i]) {
      APPLY_SWAP(i,j);
    }
    k = i;
    i = (k-1)/2;
  }
  /*check_heap ("step2", h);*/
  return 1;
}  
