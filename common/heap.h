/*-*-mode:c++-*-**********************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *  All Rights Reserved
 *
 *************************************************************************/
#ifndef __HEAP_H__
#define __HEAP_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long long heap_key_t;

typedef struct {
  int sz;
  int max;
  void **value;
  heap_key_t *key;
} Heap;

Heap *heap_new (int sz);
void heap_free (Heap *h, void (*free_element)(void *));
void heap_insert (Heap *H, heap_key_t key, void *value);
void *heap_remove_min (Heap *H);
void *heap_remove_min_key (Heap *H, heap_key_t *keyp);
void heap_insert_random (Heap *H, void *value);
void *heap_peek_min (Heap *H);
heap_key_t heap_peek_minkey (Heap *H);
void heap_save (Heap *H, FILE *fp, void (*save_element)(FILE *, void *value));
Heap *heap_restore (FILE *fp, void *(*restore_element) (FILE *));

#ifdef __cplusplus
}
#endif

#endif /* __HEAP_H__ */
