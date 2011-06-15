/*************************************************************************
 *
 *  Copyright (c) 1994 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __AVL_H__
#define __AVL_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
  AVL Trees
*/

typedef struct __avl_t {
  unsigned int l:1, r:1;
  int key;
  void *info;
  struct __avl_t *left, *right;
} avl_t;

avl_t *avl_new (int, void *);
void avl_insert (avl_t *, int, void *);
void *avl_search (avl_t *, int);
void avl_free (avl_t *);
int avl_height (avl_t *);

#ifdef __cplusplus
}
#endif

#endif
