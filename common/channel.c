/*************************************************************************
 *
 *  Communication channel implementation that works with the thread
 *  library.
 *
 *  Copyright (c) 1998, 1999, 2019 Rajit Manohar
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
#include <string.h>
#include "thread.h"
#include "qops.h"
#include "channel.h"
#include "misc.h"

extern lthread_t *readyQh, *readyQt;

static void _ch_fsendul (ch_t *c, unsigned long *msg);
static void _ch_frecvul (ch_t *c, unsigned long *msg);

static void _ch_fsend (ch_t *c, void *msg);
static void _ch_frecv (ch_t *c, void *msg);

static void _ch_tsend (ch_t *c, void *msg);
static void _ch_trecv (ch_t *c, void *msg);

typedef void (*FUNC)(ch_t*,void*);

static int channel_timing = 0;

static FILE *timer_fp = NULL;


#ifdef CLASS_HACKERY_NONDET

/*
 *  Timed queue for non-deterministic selection statements.
 *
 *  Algorithm:
 *
 *     - every process at a non-deterministic selection statement
 *       is put onto the timed queue.
 *
 *     - once no other work remains, each process in the timed queue
 *       gets scheduled and updates its local time to be the time at
 *       which the first true guard (if any) became true, and then
 *       re-inserts itself into the timed queue sorted in time.
 *
 *     - once the above process is complete, the first process with
 *       a true guard is inserted into the ready queue, thereby
 *       scheduling it.
 *
 */

static lthread_t *timeQh = NULL;
static lthread_t *timeQt = NULL;

/* timeQ is ordered by time */

void ch_reset_color (void)
{
  lthread_t *t;
  for (t = timeQh; t; q_step (t)) {
    t->color = 0;
  }
  ((lthread_t*)current_process)->color = 0;
}


#if 0
void check_timeQ (char *s)
{
  lthread_t *t, *prev;
  Time_t x;
  int flag = 0;
  int len = 0;
  int flen = 0;

  if (timeQh) {
    prev = NULL;
#if 1
    printf ("[%s] Qstate:", s);
#endif
    for (t = timeQh; t; q_step (t)) {
#if 1
      printf (" %qu", t->time);
#endif
      len++;
      if (!flag && t->time < (prev ? prev->time : timeQh->time)) {
	flen = len;
	flag = 1;
      }
      prev = t;
    }
#if 1
    printf (" (len=%d)\n", len);
    fflush (stdout);
#endif
    if (flag) {
      printf ("[%s] ## %s, len %d (flen=%d)##\n", s, flag ? "FAIL" : "ok", len, flen);
      fflush (stdout);
    }
  }
}
#endif

void ch_ready_color (void)
{
  ((lthread_t*)current_process)->color = 2;
}

int ch_get_color (void)
{
  return ((lthread_t*)current_process)->color;
}

void ch_update_readyQ (void)
{
  lthread_t *t, *prev;

  prev = NULL;
  for (t = timeQh; t; q_step (t)) {
    if (t->color == 0)
      break;
    prev = t;
  }
  if (t) {
    t->color = 1;
    q_delete_item (timeQh, timeQt, prev, t);
    q_ins (readyQh, readyQt, t);
  }
  else {
    /* everyone has updated their current time; look for the earliest
       waiting ready thread */
    prev = NULL;
    for (t = timeQh; t; q_step (t)) {
      fflush (stdout);
      if (t->color == 2) {
	q_delete_item (timeQh, timeQt, prev, t);
	q_ins (readyQh, readyQt, t);
      }
      prev = t;
    }
  }
}

void ch_insert_timeQ (void)
{
  lthread_t *t, *cur, *prev;

  cur = (lthread_t *) current_process;
  cur->in_readyq = 0;

#if 0
  check_timeQ ("pre-insert ");
#endif

  prev = NULL;
  t = timeQh;
  while (t && t->time < cur->time) {
    prev = t;
    q_step (t);
  }

  if (!t) {
    /* insert at end */
    if (!prev) {
      q_ins (timeQh, timeQt, cur);
    }
    else {
      prev->next = cur;
      cur->next = NULL;
      timeQt = cur;
    }
  }
  else {
    if (t == timeQh) {
      if (timeQh) {
	cur->next = timeQh;
	timeQh = cur;
      }
      else {
	timeQh = timeQt = cur;
	cur->next = NULL;
      }
    }
    else {
      Assert (prev, "what?");
      prev->next = cur;
      cur->next = t;
    }
  }
#if 0
  check_timeQ("post-insert");
#endif

  context_switch (context_select ());
}
#endif

/*
 *  Turn on channel timing
 */
void ch_trace (FILE *fp)
{
  if (channel_timing) return;

  channel_timing = 1;
  timer_fp = fp;
}
 

/* allocate channel */
ch_t *ch_newl (int slack, int sz, int lat, int cycle)
{
  ch_t *c;
  int i;

  context_disable ();
  c = (ch_t *)malloc(sizeof(ch_t));
  if (!c) {
    printf ("FATAL ERROR: malloc failed, file %s, line %d\n",
	    __FILE__, __LINE__);
    exit (1);
  }
  c->sz = sz;
  c->sendid = 0;
  c->recvid = 0;
  c->slack = slack;
  c->fifodelay = c->slack*lat;
  c->overhead = cycle - lat;

  c->qs.hd = NULL;
  c->qr.hd = NULL;
  c->ss.hd = NULL;
  c->sender = NULL;
  c->receiver = NULL;

  if (channel_timing) {
    c->send = (FUNC)_ch_tsend;
    c->recv = (FUNC)_ch_trecv;
  }
  else {
    if (sz == sizeof(unsigned long)) {
      c->send = (FUNC)_ch_fsendul;
      c->recv = (FUNC)_ch_frecvul;
    }
    else {
      c->send = _ch_fsend;
      c->recv = _ch_frecv;
    }
  }

  c->tm = (Time_t*) malloc(sizeof(Time_t)*(1+slack));
  for (i=0; i < 1+slack; i++)
    time_init (c->tm[i]);

  c->msgbuf = (void*) malloc (sz*(1+slack));
  if (!c->msgbuf) {
    printf ("FATAL ERROR: malloc failed, file %s, line %d\n",
	    __FILE__, __LINE__);
    exit (1);
  }

  context_enable ();

  return c;
}

/* free channel */
void _ch_free (ch_t *c, char *file, int line)
{
  context_disable ();
  if (c->qs.hd || c->qr.hd) {
    lthread_t *t;
    thread_printf ("ERROR: ch_free(), file `%s', line %d\n", file, line);
    thread_printf ("\n");
    thread_printf ("Pending processes:\n");
    if (c->qs.hd) {
      thread_printf ("SEND queue:\n");
      for (t = c->qs.hd; t; t = t->next) {
	thread_printf ("");
	thread_print_name (t);
	printf ("\n");
      }
    }
    if (c->qr.hd) {
      thread_printf ("RECV queue:\n");
      for (t = c->qr.hd; t; t = t->next) {
	thread_printf ("");
	thread_print_name (t);
	printf ("\n");
      }
    }
  }
  free (c);
  context_enable ();
}

void ch_dump (ch_t *c, char *msg)
{
  int i, cs, cr;
  thread_print_name (NULL);
  printf ("%s (cS=%d, cR=%d, qS=%d, qR=%d)\n", msg, c->sendid, c->recvid, 
	  c->qs.hd ? 1 : 0, c->qr.hd ? 1 : 0);

  cs = c->sendid % (c->slack+1);
  cr = c->recvid % (c->slack+1);

  for (i=0; i < c->slack+1; i++) {
    printf ("\ttm[%d] = ", i);
    time_print (stdout, c->tm[i]);
    /* data present when? */
    if (cr <= cs) {
      if (i >= cr && i < cs) printf (" *");
    }
    else {
      if (i < cs || i >= cr) printf (" *");
    }
    printf ("\n");
  }
}

#ifdef CLASS_HACKERY
#define DO_SELECTION(ch) 					\
     while (ch->ss.hd) {					\
       selqueue_t *st;						\
       q_del(ch->ss.hd, ch->ss.tl, st);				\
       if (st->susp && !st->susp->in_readyq) {			\
	 q_ins(readyQh, readyQt, st->susp);			\
	 st->susp->in_readyq = 1;				\
       }							\
     }
#else
#define DO_SELECTION(ch)
#endif

/* selection stuff, call with contexts disabled! */
void ch_linksel (ch_t *c, selqueue_t *s)
{
  selqueue_t *t;
  int i;

  for (i=0; i < s->nch; i++)
    if (s->c[i] == c)
      return;
  q_ins (c->ss.hd, c->ss.tl, s);
  if (s->nch == s->mch) {
    if (s->mch == 0) {
      s->c = (ch_t **)malloc (sizeof(ch_t*)*8);
      s->mch = 8;
      s->nch = 0;
    }
    else {
      s->mch *= 2;
      s->c = (ch_t **)realloc (s->c, sizeof (ch_t*)*s->mch);
    }
  }
  s->c[s->nch++] = c;
}

selqueue_t *ch_newselq (void)
{
  selqueue_t *q;

  q = (selqueue_t *)malloc (sizeof (selqueue_t));
  q->mch = 0;
  q->nch = 0;
  return q;
}

void ch_freeselq (selqueue_t *q)
{
  ch_clearsel (q);
  if (q->mch > 0)
    free (q->c);
  free (q);
}

/* interrupts disabled */
void ch_clearsel (selqueue_t *q)
{
  int i;
  selqueue_t *t, *p;

  for (i=0; i < q->nch; i++) {
    p = q->c[i]->ss.hd;
    t = NULL;
    while (p) {
      if (p == q) {
	if (!t) {
	  /* delete head */
	  q->c[i]->ss.hd = q->c[i]->ss.hd->next;
	  break;
	}
	else {
	  t->next = p->next;
	  break;
	}
      }
      t = p;
      p = p->next;
    }
  }
  q->nch = 0;
  q->susp = NULL;
}


int ch_rawprobe (ch_t *c, int dir, Time_t *tm)
{
  int cond;
  lthread_t *t;

  t = (lthread_t*)current_process;

  if (dir) {
    cond = ch_rprobe (c);
    if (cond) {
      if (c->sendid == c->recvid) {
	*tm = time_max (*tm, c->qs.hd->time);
      }
      else {
	*tm = time_max (*tm, c->tm[c->recvid % (1+c->slack)]);
      }
    }
  }
  else {
    cond = ch_sprobe (c);
    if (cond) {
      if (c->sendid == c->recvid + c->slack) {
	*tm = time_max (*tm, c->qr.hd->time);
      }
      else {
	*tm = time_max (*tm, c->tm[(c->sendid+1) % (c->slack+1)]);
      }
    }
  }
  return cond;
}


/*
  deterministic selection probe -- no negated probes possible!
*/
int ch_selectprobe (selqueue_t *s, ch_t *c, int dir)
{
  int cond;
  lthread_t *t;

  ch_linksel (c, s);
  t = (lthread_t*)current_process;

  if (dir) {
    cond = ch_rprobe (c);
    if (cond) {
      if (c->sendid == c->recvid) {
	t->time = time_max (t->time, c->qs.hd->time);
      }
      else {
	t->time = time_max (t->time, c->tm[c->recvid % (1+c->slack)]);
      }
    }
  }
  else {
    cond = ch_sprobe (c);
    if (cond) {
      if (c->sendid == c->recvid + c->slack) {
	t->time = time_max (t->time, c->qr.hd->time);
      }
      else {
	t->time = time_max (t->time, c->tm[(c->sendid+1) % (c->slack+1)]);
      }
    }
  }
  return cond;
}

int ch_sprobe (ch_t *c)
{
  return (c->sendid != c->recvid + c->slack ? 1 : (c->qr.hd == NULL ? 0 : 1));
}

int ch_rprobe (ch_t *c)
{
  return (c->sendid != c->recvid ? 1 : (c->qs.hd == NULL ? 0 : 1));
}



static void _ch_frecvul (ch_t *c, unsigned long *msg)
{
  lthread_t *t, *s;
  int id, id2;

  context_disable ();
  id = c->recvid % (1+c->slack);
  c->receiver = (lthread_t*)current_process;

  if (c->qs.hd) {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    if (c->slack == 0) {
      t->time = c->qs.hd->time = time_max (c->qs.hd->time, t->time);
    }
    q_del (c->qs.hd, c->qs.tl, t);
    s = t;
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
    *msg = ((unsigned long*)c->msgbuf)[id];
    t = (lthread_t*)current_process;
    c->recvid++;
    id2 = (c->sendid+1) % (c->slack+1);
    s->time = time_max (s->time, c->tm[id2]);
    c->tm[c->sendid % (c->slack+1)] = s->time +
      (Abs(c->sendid - c->recvid)*c->overhead + c->fifodelay);

    c->sendid++;
    context_enable ();
  }
  else if (c->sendid == c->recvid) {
    c->msgptr = msg;
    q_ins (c->qr.hd, c->qr.tl, current_process);
    DO_SELECTION (c);
    context_switch (context_select ());
  }
  else {
    *msg = ((unsigned long*)c->msgbuf)[id];
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    c->recvid++;
    context_enable ();
  }
}

static void _ch_fsendul (ch_t *c, unsigned long *msg)
{
  lthread_t *t;
  int id, id2;
  int delay;

  context_disable ();

  t = (lthread_t*)current_process;
  c->sender = t;
  id = c->sendid % (c->slack+1);
  /*bcopy (msg, c->msgbuf+id*c->sz, c->sz);*/
  ((unsigned long*)c->msgbuf)[id] = *msg;
  id2 = (c->sendid+1) % (c->slack+1);

  delay = Abs(c->sendid - c->recvid)*c->overhead + c->fifodelay;

  if (c->qr.hd) {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time + delay;

    if (c->slack == 0) {
      t->time = c->qr.hd->time = time_max (c->qr.hd->time, t->time);
    }
    q_del (c->qr.hd, c->qr.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
    *((int*)c->msgptr) = *msg;
    id = c->recvid % (1+c->slack);
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    c->recvid++;
    c->sendid++;
    context_enable ();
  }
  else if (Abs (c->sendid - c->recvid) == c->slack) {
    q_ins (c->qs.hd, c->qs.tl, current_process);
    DO_SELECTION (c);
    context_switch (context_select ());
  }
  else {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time + delay;
    c->sendid++;
    context_enable ();
  }
}


static void _ch_frecv (ch_t *c, void *msg)
{
  lthread_t *t, *s;
  int id, id2;
  
  context_disable ();
  id = c->recvid % (1+c->slack);
  c->receiver = (lthread_t*)current_process;

  if (c->qs.hd) {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    if (c->slack == 0) {
      t->time = c->qs.hd->time = time_max (c->qs.hd->time, t->time);
    }
    q_del (c->qs.hd, c->qs.tl, t);
    s = t;
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
    bcopy ((char *)c->msgbuf+c->sz*id, msg, c->sz);
    t = (lthread_t*)current_process;
    c->recvid++;
    id2 = (c->sendid+1) % (c->slack+1);
    s->time = time_max (s->time, c->tm[id2]);
    c->tm[c->sendid % (c->slack+1)] = s->time + 
      (Abs(c->sendid - c->recvid)*c->overhead + c->fifodelay);
    c->sendid++;
    context_enable ();
  }
  else if (c->sendid == c->recvid) {
    c->msgptr = msg;
    q_ins (c->qr.hd, c->qr.tl, current_process);
    DO_SELECTION (c);
    context_switch (context_select ());
  }
  else {
    bcopy ((char *)c->msgbuf + id*c->sz, msg, c->sz);
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    c->recvid++;
    context_enable ();
  }
}

static void _ch_fsend (ch_t *c, void *msg)
{
  lthread_t *t;
  int id, id2;
  int delay;

  context_disable ();
  
  delay = Abs(c->sendid - c->recvid)*c->overhead + c->fifodelay;

  t = (lthread_t*)current_process;
  c->sender = t;
  id = c->sendid % (c->slack+1);
  bcopy (msg, (char *)c->msgbuf+id*c->sz, c->sz);
  id2 = (c->sendid+1) % (c->slack+1);

  if (c->qr.hd) {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time + delay;
    if (c->slack == 0) {
      t->time = c->qr.hd->time = time_max (c->qr.hd->time, t->time);
    }
    q_del (c->qr.hd, c->qr.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
    bcopy (msg, (void *)c->msgptr, c->sz);
    id = c->recvid % (1+c->slack);
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    c->recvid++;
    c->sendid++;
    context_enable ();
  }
  else if (Abs (c->sendid - c->recvid) == c->slack) {
    q_ins (c->qs.hd, c->qs.tl, current_process);
    DO_SELECTION (c);
    context_switch (context_select ());
  }
  else {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time + delay;
    c->sendid++;
    context_enable ();
  }
}  


void ch_dump_time (ch_t *c, char *type)
{
  lthread_t *t = (lthread_t*)current_process;

  fprintf (timer_fp, "%s %d %8lx %s ", thread_name (), t->tid, (unsigned long)c, type);
  time_print (timer_fp, t->time);
  fputc ('\n', timer_fp);
}

/*
 *  Channels that save time traces to a file
 */
static void dump_time (ch_t *c, char *type)
{
  lthread_t *t = (lthread_t*)current_process;

  fprintf (timer_fp, "%s %d %8lx %s ", thread_name (), t->tid, (unsigned long)c, type);
  time_print (timer_fp, t->time);
  fputc ('\n', timer_fp);
}


static void _ch_tsend (ch_t *c, void *msg)
{
  lthread_t *t;
  int id, id2;
  int delay;
  int dumped = 0;

  context_disable ();

  delay = Abs(c->sendid - c->recvid)*c->overhead + c->fifodelay;

  dump_time (c, "ES");

  t = (lthread_t*)current_process;
  c->sender = t;
  id = c->sendid % (c->slack+1);
  bcopy (msg, (char *)c->msgbuf+id*c->sz, c->sz);

  id2 = (c->sendid+1) % (c->slack+1);

  if (c->qr.hd) {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time + delay;
    if (c->slack == 0) {
      t->time = c->qr.hd->time = time_max (c->qr.hd->time, t->time);
    }
    q_ins (c->qs.hd, c->qs.tl, current_process);
    q_del (c->qr.hd, c->qr.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;

    context_switch (context_select ());
    context_disable ();
    c->sendid++;
  }
  else if (Abs (c->sendid - c->recvid) == c->slack) {
    q_ins (c->qs.hd, c->qs.tl, current_process);
    DO_SELECTION (c);

    context_switch (context_select ());
    context_disable ();

    delay = Abs (c->sendid - c->recvid)*c->overhead + c->fifodelay;

    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time + delay;
    c->sendid++;
    dump_time (c, "XS");
    dumped = 1;

    if (c->qr.hd) {
      q_del (c->qr.hd, c->qr.tl, t);
      q_ins (readyQh, readyQt, t);
      t->in_readyq = 1;
      q_ins (c->qs.hd, c->qs.tl, current_process);
      context_switch (context_select ());
      return;
    }
  }
  else {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time + delay;
    c->sendid++;
  }
  if (!dumped) dump_time (c, "XS");
  if (c->qr.hd) {
    q_del (c->qr.hd, c->qr.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
  }
  context_enable ();
}  

static void _ch_trecv (ch_t *c, void *msg)
{
  lthread_t *t, *s;
  int id, ind2;
  int dumped = 0;
  
  context_disable ();

  dump_time (c, "ER");

  id = c->recvid % (1+c->slack);
  c->receiver = (lthread_t*)current_process;

  if (c->qs.hd) {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    if (c->slack == 0) {
      t->time = c->qs.hd->time = time_max (c->qs.hd->time, t->time);
    }
    q_ins (c->qr.hd, c->qr.tl, current_process);
    q_del (c->qs.hd, c->qs.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;

    context_switch (context_select ());
    context_disable ();

    bcopy ((char *)c->msgbuf + id*c->sz, msg, c->sz);
    /**msg = ((unsigned long*)c->msgbuf)[id];*/
    t = (lthread_t*)current_process;
    c->recvid++;
  }
  else if (c->sendid == c->recvid) {
    q_ins (c->qr.hd, c->qr.tl, current_process);
    DO_SELECTION (c);
    context_switch (context_select ());
    context_disable ();

    bcopy ((char *)c->msgbuf + id*c->sz, msg, c->sz);
    /**msg = ((unsigned long*)c->msgbuf)[id];*/
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    c->recvid++;
    dump_time (c, "XR");

    dumped = 1;

    if (c->qs.hd) {
      q_del (c->qs.hd, c->qs.tl, t);
      q_ins (readyQh, readyQt, t);
      t->in_readyq = 1;
      q_ins (c->qr.hd, c->qr.tl, current_process);

      context_switch (context_select ());
      return;
    }
  }
  else {
    bcopy ((char *)c->msgbuf + id*c->sz, msg, c->sz);
    /**msg = ((unsigned long*)c->msgbuf)[id];*/
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    c->recvid++;
  }
  if (!dumped) dump_time (c, "XR");

  if (c->qs.hd) {
    q_del (c->qs.hd, c->qs.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
  }

  context_enable ();
}


#if 0
/*-------------------------------------------------------------------------
 *
 *  The code below is provided as a reference for why the implementations
 *  shown above work. The code shown above optimized with the
 *  assumption that there was only a single sender and a single receiver.
 *  The code shown below was derived from the code for a split-binary
 *  semaphore implementation of send and receive, optimized to assume
 *  that there was only one processor executing the whole system.
 *  In addition, the "m" (a.k.a. "z" or zero) semaphore was eliminated 
 *  by re-using the queues qs and qr, since we're assuming a single
 *  sender and single receiver.
 *
 *  Program transformation is neat, but it can obfuscate code. :)
 *
 *-------------------------------------------------------------------------
 */
static void _ch_sendul (ch_t *c, unsigned long *msg)
{
  lthread_t *t;
  int id, id2;

  context_disable ();
  
  t = (lthread_t*)current_process;
  id = c->sendid % (c->slack+1);
  /*bcopy (msg, c->msgbuf+id*c->sz, c->sz);*/
  ((unsigned long*)c->msgbuf)[id] = *msg;

  id2 = (c->sendid+1) % (c->slack+1);

  if (c->qr.hd) {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time;
    if (c->slack == 0) {
      t->time = c->qr.hd->time = time_max (c->qr.hd->time, t->time);
    }
    q_ins (c->qs.hd, c->qs.tl, current_process);
    q_del (c->qr.hd, c->qr.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
    context_switch (context_select ());
    context_disable ();
    c->sendid++;
  }
  else if (Abs (c->sendid - c->recvid) == c->slack) {
    q_ins (c->qs.hd, c->qs.tl, current_process);
    context_switch (context_select ());
    context_disable ();
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time;
    c->sendid++;
    if (c->qr.hd) {
      q_del (c->qr.hd, c->qr.tl, t);
      q_ins (readyQh, readyQt, t);
      t->in_readyq = 1;
      q_ins (c->qs.hd, c->qs.tl, current_process);
      context_switch (context_select ());
      return;
    }
  }
  else {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id2]);
    c->tm[id] = t->time;
    c->sendid++;
  }
  if (c->qr.hd) {
    q_del (c->qr.hd, c->qr.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
  }
  context_enable ();
}  

static void _ch_recvul (ch_t *c, unsigned long *msg)
{
  lthread_t *t, *s;
  int id, ind2;
  
  context_disable ();
  id = c->recvid % (1+c->slack);

  if (c->qs.hd) {
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    if (c->slack == 0) {
      t->time = c->qs.hd->time = time_max (c->qs.hd->time, t->time);
    }
    q_ins (c->qr.hd, c->qr.tl, current_process);
    q_del (c->qs.hd, c->qs.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
    context_switch (context_select ());
    context_disable ();
    /*bcopy (c->msgbuf + id*c->sz, msg, c->sz);*/
    *msg = ((unsigned long*)c->msgbuf)[id];
    t = (lthread_t*)current_process;
    c->recvid++;
  }
  else if (c->sendid == c->recvid) {
    q_ins (c->qr.hd, c->qr.tl, current_process);
    context_switch (context_select ());
    context_disable ();
    /*bcopy (c->msgbuf + id*c->sz, msg, c->sz);*/
    *msg = ((unsigned long*)c->msgbuf)[id];
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    c->recvid++;
    if (c->qs.hd) {
      q_del (c->qs.hd, c->qs.tl, t);
      q_ins (readyQh, readyQt, t);
      t->in_readyq = 1;
      q_ins (c->qr.hd, c->qr.tl, current_process);
      context_switch (context_select ());
      return;
    }
  }
  else {
    /*bcopy (c->msgbuf + id*c->sz, msg, c->sz);*/
    *msg = ((unsigned long*)c->msgbuf)[id];
    t = (lthread_t*)current_process;
    t->time = time_max (t->time, c->tm[id]);
    c->tm[id] = t->time;
    c->recvid++;
  }
  if (c->qs.hd) {
    q_del (c->qs.hd, c->qs.tl, t);
    q_ins (readyQh, readyQt, t);
    t->in_readyq = 1;
  }
  context_enable ();
}
#endif
