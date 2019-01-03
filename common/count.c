/*************************************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *
 *************************************************************************/
#include <stdio.h>
#include "thread.h"
#include "count.h"
#include "qops.h"

/* allocate initialized counter */
countw_t *count_new (int init_val)
{
  countw_t *c;
  
  context_disable ();
  c = (countw_t*)malloc(sizeof(countw_t));
  if (!c) {
    printf ("FATAL ERROR: malloc failed, file %s, line %d\n",
	    __FILE__, __LINE__);
    exit (1);
  }
  c->cnt = init_val;
  c->hd = NULL;
  c->tl = NULL;
  
  context_enable ();
  return c;
}

/* free counter */
void _count_free (countw_t *c, char *file, int line)
{
  context_disable ();
  if (c->hd) {
    lthread_t *t;
    thread_printf ("ERROR: count_free(), file `%s', line %d\n", file, line);
    thread_printf ("\n");
    thread_printf ("Pending processes:\n");
    for (t = c->hd; t; t = t->next) {
      thread_printf ("");
      thread_print_name (t);
      printf ("\n");
    }
  }
  free (c);
  context_enable ();
}

/*
 * wait for count c to exceed value
 */
void count_await (countw_t *c, unsigned int val)
{
  context_disable ();
  if (c->cnt < val) {
    current_process->cdata1 = (void*)(long)val;
    q_ins (c->hd, c->tl, current_process);
    context_switch (context_select ());
  }
  else {
    context_enable ();
  }
}

/* increment counter */
void count_increment (countw_t *c, unsigned int amount)
{
  extern lthread_t *readyQh;
  extern lthread_t *readyQt;

  context_disable ();
  c->cnt += amount;
  if (amount > 0 && c->hd) {
    lthread_t *t, *prev;
    prev = NULL;
    for (t = c->hd; t; t = t->next) {
      if (((unsigned int)t->cdata1) >= c->cnt) {
	q_ins (readyQh, readyQt, t);
	t->in_readyq = 1;
	if (prev) {
	  prev->next = t->next;
	}
	else {
	  c->hd = t->next;
	}
      }
      else {
	prev = t;
      }
    }
  }
  context_enable ();
}
