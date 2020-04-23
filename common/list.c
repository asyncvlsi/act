/*************************************************************************
 *
 *  Yet another linked list library
 *
 *  Copyright (c) 2009, 2019 Rajit Manohar
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
#include "misc.h"
#include "list.h"
#include "qops.h"


static listitem_t *freelist = NULL;


static listitem_t *allocitem (void)
{
  listitem_t *l;

  if (!freelist) {
    int i;
    
    l = NULL;
    for (i=0; i < 1024; i++) {
      NEW (freelist, listitem_t);
      freelist->next = l;
      l = freelist;
    }
  }
  l = freelist;
  freelist = freelist->next;
  l->next = NULL;
  l->data = NULL;
  return l;
}

static void freeitem (listitem_t *l)
{
  l->next = freelist;
  freelist = l;
}

/*------------------------------------------------------------------------
 *
 *  list_new --
 *
 *   Allocate new list
 *
 *------------------------------------------------------------------------
 */
list_t *
list_new (void)
{
  list_t *l;

  NEW (l, list_t);
  l->hd = l->tl = NULL;
  return l;
}

/*------------------------------------------------------------------------
 *
 *  list_append --
 *
 *   append an item to the list
 *
 *------------------------------------------------------------------------
 */
void list_append (list_t *l, const void *item)
{
  listitem_t *li;

  Assert (l, "void pointer as list argument");

  li = allocitem ();
  li->data = item;

  q_ins (l->hd, l->tl, li);
}

void list_iappend (list_t *l, int item)
{
  listitem_t *li;

  Assert (l, "void pointer as list argument");

  li = allocitem ();
  li->idata = item;

  q_ins (l->hd, l->tl, li);
}


/*------------------------------------------------------------------------
 *
 *  list_length --
 *
 *   Count length of the list
 *
 *------------------------------------------------------------------------
 */
int list_length (list_t *l)
{
  int i = 0;
  listitem_t *li = l->hd;

  while (li) {
    i++;
    li = li->next;
  }
  return i;
}

/*------------------------------------------------------------------------
 *
 *  list_free --
 *
 *   Free entire list
 *
 *------------------------------------------------------------------------
 */
void list_free (list_t *l)
{
  if (!l) return;
  if (!l->hd) {
    FREE (l);
    return;
  }
  l->tl->next = freelist;
  freelist = l->hd;
  l->hd = l->tl = NULL;
  FREE (l);
}


/*------------------------------------------------------------------------
 *
 *  list_map --
 *
 *   
 *
 *------------------------------------------------------------------------
 */
list_t *list_map (list_t *l, void *(*f)(const void *))
{
  listitem_t *x = l->hd;
  list_t *m = list_new ();

  while (x) {
    list_append (m,  (*f)(x->data));
    x = x->next;
  }
  return m;
}

list_t *list_map_cookie (list_t *l, void *cookie, 
			 void *(*f)(void *, const void *))
{
  listitem_t *x = l->hd;
  list_t *m = list_new ();

  while (x) {
    list_append (m,  (*f)(cookie, x->data));
    x = x->next;
  }
  return m;
}

/*------------------------------------------------------------------------
 *
 *  list_apply --
 *
 *------------------------------------------------------------------------
 */
void list_apply (list_t *l, void *cookie, void (*f)(void *, const void *))
{
  listitem_t *x = l->hd;

  while (x) {
    (*f)(cookie, x->data);
    x = x->next;
  }
  return;
}


void stack_push (list_t *l, const void *item)
{
  listitem_t *li;

  li = allocitem ();
  li->data = item;

  if (l->hd) {
    li->next = l->hd;
    l->hd = li;
  }
  else {
    l->hd = li;
    l->tl = li;
  }
}

void stack_ipush (list_t *l, int item)
{
  listitem_t *li;

  li = allocitem ();
  li->idata = item;

  if (l->hd) {
    li->next = l->hd;
    l->hd = li;
  }
  else {
    l->hd = li;
    l->tl = li;
  }
}

const void *stack_pop (list_t *l)
{
  const void *item = l->hd->data;
  listitem_t *li = l->hd;

  l->hd = l->hd->next;
  freeitem (li);
  
  return item;
}

int stack_ipop (list_t *l)
{
  int item = l->hd->idata;
  listitem_t *li = l->hd;

  l->hd = l->hd->next;
  freeitem (li);
  
  return item;
}


/*------------------------------------------------------------------------
 *
 *  list_delete_tail --
 *
 *   Delete the last element in the list
 *
 *------------------------------------------------------------------------
 */
const void *list_delete_tail (list_t *l)
{
  listitem_t *li;
  listitem_t *prev;
  const void *data;

  if (list_isempty (l)) return NULL;
  prev = NULL;
  li = list_first (l);
  while (list_next (li)) {
    prev = li;
    li = list_next (li);
  }
  /* li is the last element */
  Assert (li, "list_delete_tail: list appears to be empty!");

  if (prev) {
    /* non-empty list remains */
    prev->next = NULL;
    l->tl = prev;
  }
  else {
    /* the list is now empty */
    l->hd = NULL;
    l->tl = NULL;
  }
  data = li->data;
  freeitem (li);
  return data;
}

int list_idelete_tail (list_t *l)
{
  listitem_t *li;
  listitem_t *prev;
  int data;

  if (list_isempty (l)) return 0;
  prev = NULL;
  li = list_first (l);
  while (list_next (li)) {
    prev = li;
    li = list_next (li);
  }
  /* li is the last element */
  Assert (li, "list_delete_tail: list appears to be empty!");

  if (prev) {
    /* non-empty list remains */
    prev->next = NULL;
    l->tl = prev;
  }
  else {
    /* the list is now empty */
    l->hd = NULL;
    l->tl = NULL;
  }
  data = li->idata;
  freeitem (li);
  return data;
}


/*------------------------------------------------------------------------
 *
 *  list_reverse --
 *
 *   In-place list reversal
 *
 *------------------------------------------------------------------------
 */
void list_reverse (list_t *l)
{
  listitem_t *prev, *li, *tmp;

  if (list_isempty (l)) return;

  prev = NULL;
  li = l->hd;
  while (li) {
    tmp = li;
    li = li->next;
    tmp->next = prev;
    prev = tmp;
  }
  tmp = l->hd;
  l->hd = l->tl;
  l->tl = tmp;
}


/*------------------------------------------------------------------------
 *
 *  list_cleanup --
 *
 *  Release ancillary storage
 *
 *------------------------------------------------------------------------
 */
void list_cleanup (void)
{
  listitem_t *l;
  while (freelist) {
    l = freelist;
    freelist = freelist->next;
    FREE (l);
  }
}



void list_concat (list_t *l, list_t *x)
{
  if (!x->hd) return;
  if (!l->hd) {
    l->hd = x->hd;
  }
  else {
    l->tl->next = x->hd;
  }
  l->tl = x->tl;
  x->hd = NULL;
  x->tl = NULL;
}
  

list_t *list_dup (list_t *l)
{
  list_t *tmp;
  listitem_t *li;
  if (!l) return NULL;
  tmp = list_new ();
  for (li = list_first (l); li; li = list_next (li)) {
    list_append (tmp, list_value (li));
  }
  return tmp;
}
  
void list_delete_next (list_t *l, listitem_t *li)
{
  if (!li) {
    if (l->hd == l->tl) {
      freeitem (l->hd);
      l->hd = l->tl = NULL;
    }
    else {
      li = l->hd->next;
      freeitem (l->hd);
      l->hd = li;
    }
  }
  else {
    listitem_t *tmp = li->next;
    Assert (tmp, "What?");
    li->next = tmp->next;
    if (li->next == l->tl) {
      l->tl = li;
    }
    freeitem (tmp);
  }
}
