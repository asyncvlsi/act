/*************************************************************************
 *
 *  Heap data structure
 *
 *  Copyright (c) 1999, 2019 Rajit Manohar
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
int heap_update_key (Heap *h, heap_key_t key, void *v);
void heap_insert_random (Heap *H, void *value);
void *heap_peek_min (Heap *H);
heap_key_t heap_peek_minkey (Heap *H);
void heap_save (Heap *H, FILE *fp, void (*save_element)(FILE *, void *value));
Heap *heap_restore (FILE *fp, void *(*restore_element) (FILE *));

#define heap_size(h) ((h)->sz)

#ifdef __cplusplus
}
#endif

#endif /* __HEAP_H__ */
