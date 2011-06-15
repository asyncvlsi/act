/*************************************************************************
 *
 *  Copyright (c) 2009 Rajit Manohar
 *  All Rights Reserved
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
    MALLOC (freelist, listitem_t, 1024);
    for (i=0; i < 1023; i++)
      freelist[i].next = &freelist[i+1];
    freelist[1023].next = NULL;
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
void list_append (list_t *l, void *item)
{
  listitem_t *li;

  Assert (l, "void pointer as list argument");

  li = allocitem ();
  li->data = item;

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
list_t *list_map (list_t *l, void *(*f)(void *))
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
			 void *(*f)(void *, void *))
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
void list_apply (list_t *l, void *cookie, void (*f)(void *, void *))
{
  listitem_t *x = l->hd;

  while (x) {
    (*f)(cookie, x->data);
    x = x->next;
  }
  return;
}


void stack_push (list_t *l, void *item)
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

void *stack_pop (list_t *l)
{
  void *item = l->hd->data;
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
void *list_delete_tail (list_t *l)
{
  listitem_t *li;
  listitem_t *prev;
  void *data;

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
