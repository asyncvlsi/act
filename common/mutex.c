/*************************************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *
 *************************************************************************/
#include <stdio.h>
#include "mutex.h"
#include "qops.h"

extern lthread_t *readyQh;
extern lthread_t *readyQt;


/*------------------------------------------------------------------------
 *------------------------------------------------------------------------
 *
 * Mutex
 *
 *------------------------------------------------------------------------
 *------------------------------------------------------------------------
 */
mutex_t *mutex_new (void)
{
  mutex_t *t;

  context_disable ();
  t = (mutex_t*)malloc (sizeof(mutex_t));
  if (!t) {
    printf ("FATAL ERROR: malloc failed, file %s, line %d\n",
	    __FILE__, __LINE__);
    exit (1);
  }
  t->busy = 0;
  t->hd = NULL;
  context_enable ();
  return t;
}

void mutex_free (mutex_t *m)
{
  context_disable ();
  free (m);
  context_enable ();
}

static void mutex_lock_safe (mutex_t *m)
{
  if (m->busy) {
    q_ins (m->hd, m->tl, current_process);
    context_switch (context_select ());
    context_disable ();
    if (m->busy)
      printf ("HMMMMM!\n");
  }
  m->busy = 1;
}

void mutex_lock (mutex_t *m)
{
  context_disable ();
  mutex_lock_safe (m);
  context_enable ();
}
    
static void mutex_unlock_safe (mutex_t *m)
{
  lthread_t *t;
  if (m->busy)
    m->busy = 0;
  else {
    printf ("ERROR: unlock of an unlocked mutex!\n");
    exit (1);
  }
  if (m->hd) {
    q_del (m->hd, m->tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
  }
}

void mutex_unlock (mutex_t *m)
{
  context_disable ();
  mutex_unlock_safe (m);
  context_enable ();
}
    

/*------------------------------------------------------------------------
 *------------------------------------------------------------------------
 *
 * Condition Variables
 *
 *------------------------------------------------------------------------
 *------------------------------------------------------------------------
 */
cond_t *cond_new (mutex_t *m)
{
  cond_t *c;

  context_disable ();
  c = (cond_t *)malloc(sizeof(cond_t));
  if (!c) {
    printf ("FATAL ERROR: malloc failed, file %s, line %d\n",
	    __FILE__, __LINE__);
    exit (1);
  }
  c->hd = NULL;
  c->tl = NULL;
  c->lock = m;
  context_enable ();
  return c;
}

void cond_free (cond_t *c)
{
  context_disable ();
  free (c);
  context_enable ();
}

void cond_wait (cond_t *c)
{
  context_disable ();
  mutex_unlock_safe (c->lock);
  q_ins (c->hd, c->tl, current_process);
  context_switch (context_select ());
}

void cond_signal (cond_t *c)
{
  lthread_t *t;

  context_disable ();
  if (c->hd) {
    q_del (c->hd, c->tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
  }
  else
    mutex_unlock_safe (c->lock);
  context_enable ();
}

int cond_waiting (cond_t *c)
{
  return (c->hd == NULL) ? 0 : 1;
}

