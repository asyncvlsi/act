/*************************************************************************
 *
 *  Simple implementation of AVL trees.
 *
 *  Copyright (c) 1994 Rajit Manohar
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
