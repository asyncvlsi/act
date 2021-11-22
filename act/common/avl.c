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
#include <stdio.h>
#include <stdlib.h>

#include "act/common/avl.h"

/*------------------------------------------------------------------------*/
avl_t *avl_new (int val, void *I)
{
  avl_t *a;

  a = (avl_t*)malloc(sizeof(avl_t));
  if (!a) return NULL;
  a->key = val;
  a->info = I;
  a->left = a->right = NULL;
  a->l = a->r = 0;
  return a;
}

/*------------------------------------------------------------------------*/
void *avl_search (avl_t *a, int val)
{
  if (!a) return NULL;
  if (a->key == val) return a->info;
  if (a->key > val)
    return avl_search (a->left, val);
  else
    return avl_search (a->right, val);
}

/*
  RETURNS: OLD VALUE OF TAG
*/  
static int do_insert (avl_t *a, int val, void *I)
{
  int tag, ltag;
  
  tag = a->l - a->r;

  if (a->key == val) {
    a->info = I;
    return tag;
  }
  else if (a->key > val) {
    /* go left */
    if (!a->left) {
      /* nothing left */
      a->left = avl_new (val,I);
      if (a->r) {
	a->r = 0;
	a->l = 0;
      }
      else 
       a->l = 1;
      return tag;
    }
    ltag = do_insert (a->left, val, I);
    if (ltag != 0)
      /* left tag was previously non-zero. Don't make any change! */
      return tag;
    /* left tag was previously zero. */
    if (tag == 0) {
      /* tree was balanced to start with. 
	 left tag was previously zero.
       */
      if (a->left->l != a->left->r) {
	/* tree height on left has increased by one. */
	a->l = 1;
	a->r = 0;
      }
      return tag;
    }
    
    /* tree was unbalanced to start with.
       left tag was previously zero.
       */
    
    if (tag == -1) {
      /* tree had more stuff on the right to start with */
      if (a->left->l != a->left->r) {
	/* tree height on left has increased by one. */
	a->l = 0;
	a->r = 0;
      }
      return tag;
    }
    
    /* 
      tree was unbalanced to start with, height greater on the left. 
      left tag was previously zero.
      */
    if (a->left->l == a->left->r)
      /* left tag is zero now! No problem! */
      return tag;
    
    /* tree unbalanced top start with, height greater on the left.
       left tag was previously zero.
       
       Now it is non-zero!
       
       Have to rotate the tree. One of two possibilities.
             tag is now +1  : simple rotation.
	     tag is now -1  : two rotations.
       */
    
    if (a->left->l > a->left->r) {
      /* tag is now +1. 
	 Rotate. 
	 */
      avl_t *x, *y, *z, *b;
      void *info;
      int ival;

      x = a->left->left;
      y = a->left->right;
      z = a->right;

      ival = a->left->key;
      info = a->left->info;

      /* flip */
      a->right = a->left;
      a->right->info = a->info;
      a->right->key = a->key;
      a->info = info;
      a->key = ival;
    
      a->right->left = y;
      a->right->right = z;
      a->left = x;
      a->l = a->r = 0;
      a->right->l = a->right->r = 0;
      
      return tag;
    }

    /* tag is now -1. Rotate */
    
    {
      avl_t *x, *y, *z, *w, *b, *c;
      void *info;
      int ival;

      info = a->info;
      ival = a->key;

      x = a->left->left;
      y = a->left->right->left;
      z = a->left->right->right;
      w = a->right;
      b = a->left;
      c = a->left->right;

      a->right = c;
      a->info = c->info;
      a->key = c->key;
      c->info = info;
      c->key = ival;
      
      a->left = b;
      b->left = x;
      b->right = y;
      c->left = z;
      c->right = w;
      
      ival = c->l - c->r;

      a->l = a->r = 0;

      if (ival == 0) {
	b->l = b->r = 0;
	c->l = c->r = 0;
      }
      else if (ival == 1) {
	b->l = b->r = 0;
	c->l = 0; c->r = 1;
      }
      else {
	b->l = 0; b->r = 1;
	c->l = 0; c->r = 0;
      }
      return tag;
    }
  }
  else /*if (a->key < val)*/ {
    /* go right */
    if (!a->right) {
      /* nothing right */
      a->right = avl_new (val,I);
      if (a->l) {
	a->l = 0;
	a->r = 0;
      }
      else 
       a->r = 1;
      return tag;
    }

    ltag = do_insert (a->right, val, I);
    if (ltag != 0)
      /* right tag was previously non-zero. Don't make any change! */
      return tag;
    /* right tag was previously zero. */
    if (tag == 0) {
      /* tree was balanced to start with. 
	 right tag was previously zero.
       */
      if (a->right->l != a->right->r) {
	/* tree height on right has increased by one. */
	a->r = 1;
	a->l = 0;
      }
      return tag;
    }
    
    /* tree was unbalanced to start with.
       right tag was previously zero.
       */
    
    if (tag == 1) {
      /* tree had more stuff on the right to start with */
      if (a->right->l != a->right->r) {
	/* tree height on right has increased by one. */
	a->l = 0;
	a->r = 0;
      }
      return tag;
    }
    
    /* 
      tree was unbalanced to start with, height greater on the right. 
      right tag was previously zero.
      */
    if (a->right->l == a->right->r)
      /* right tag is zero now! No problem! */
      return tag;
    
    /* tree unbalanced top start with, height greater on the right.
       right tag was previously zero.
       
       Now it is non-zero!
       
       Have to rotate the tree. One of two possibilities.
             tag is now -1  : simple rotation.
	     tag is now +1  : two rotations.
       */
    
    if (a->right->r > a->right->l) {
      /* tag is now -1. 
	 Rotate. 
	 */
      avl_t *x, *y, *z, *b;
      void *info;
      int ival;

      x = a->right->right;
      y = a->right->left;
      z = a->left;

      ival = a->right->key;
      info = a->right->info;

      /* flip */
      a->left = a->right;
      a->left->info = a->info;
      a->left->key = a->key;
      a->info = info;
      a->key = ival;
    
      a->left->right = y;
      a->left->left = z;
      a->right = x;
      a->l = a->r = 0;
      a->left->l = a->left->r = 0;
      
      return tag;
    }

    /* tag is now +1. Rotate */
    
    {
      avl_t *x, *y, *z, *w, *b, *c;
      void *info;
      int ival;

      info = a->info;
      ival = a->key;

      x = a->right->right;
      y = a->right->left->right;
      z = a->right->left->left;
      w = a->left;
      b = a->right;
      c = a->right->left;

      a->left = c;
      a->info = c->info;
      a->key = c->key;
      c->info = info;
      c->key = ival;
      
      a->right = b;
      b->right = x;
      b->left = y;
      c->right = z;
      c->left = w;
      
      ival = c->l - c->r;

      a->l = a->r = 0;

      if (ival == 0) {
	b->l = b->r = 0;
	c->l = c->r = 0;
      }
      else if (ival == -1) {
	b->l = b->r = 0;
	c->r = 0; c->l = 1;
      }
      else {
	b->r = 0; b->l = 1;
	c->l = 0; c->r = 0;
      }
      return tag;
    }
  }
}


void avl_insert (avl_t *a, int val, void *I)
{
  if (!a) return;
  do_insert (a, val, I);
}


/*------------------------------------------------------------------------*/
void avl_free (avl_t *a)
{
  if (!a) return;
  avl_free (a->left);
  avl_free (a->right);
  free (a);
}

int avl_height (avl_t *a)
{
  int l, r;
  if (!a) return 0;
  l = avl_height (a->left);
  r = avl_height (a->right);
  if (l == r) return l+1;
  else if (l > r) {
	if (l != r+1) { printf ("hmm...\n"); }
	return l+1;
  }
  else /*if (r > l)*/ {
	if (r != l+1) { printf ("hmm...\n"); }
	return r+1;
  }
}
