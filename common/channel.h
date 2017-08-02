/*************************************************************************
 *
 *  Copyright (c) 1998 Rajit Manohar
 *  
 *************************************************************************/
#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#ifdef __cplusplus
extern "C" {
#endif

struct selection_stmt;

typedef struct chan {
  int sendid;
  int recvid;
  int sz;
  int slack;

  /* 
     delay = latency * holes + throughput * filled slots
           = latency * slack + (throughput-latency)* filled slots
	   = fifodelay + overhead * filled slots
  */
  int fifodelay, overhead;
  volatile void *msgptr;

  lthread_t *sender, *receiver;

  Time_t *tm;			/* time for each slot */
  void *msgbuf;			/* message buffer */
  void (*send)(struct chan *, void *);
  void (*recv)(struct chan *, void *);
  struct {
    lthread_t *hd, *tl;
  } qs, qr;			
  struct {
    struct selection_stmt *hd, *tl;
  } ss;				/* select suspension */
} ch_t;

typedef struct selection_stmt {
  lthread_t *susp;		/* suspended process, if any */
  ch_t **c;			/* channel dependencies */
  int nch;			/* num channels */
  int mch;			/* max alloc'ed channels */
  struct selection_stmt *next;
} selqueue_t;

ch_t *ch_newl (int slack, int sz, int lat, int cycle);
#define ch_new(slack,sz)  ch_newl (slack,sz,0,0)
void _ch_free (ch_t *c, char *file, int line);
int ch_sprobe (ch_t *c);
int ch_rprobe (ch_t *c);
#define ch_send(c,msg)  (c)->send(c,msg)
#define ch_recv(c,msg)  (c)->recv(c,msg)

#define ch_free(c) _ch_free(c,__FILE__,__LINE__)

void ch_trace (FILE *fp);

  /* selections */
void ch_clearsel (selqueue_t *q);
selqueue_t *ch_newselq (void);
void ch_linksel (ch_t *c, selqueue_t *s);
int ch_selectprobe (selqueue_t *s, ch_t *c, int dir); /* 1 == in, 0 == out */
void ch_freeselq (selqueue_t *q);

void ch_update_readyQ ();

void ch_reset_color (void);
void ch_update_readyQ (void);
void ch_insert_timeQ (void);
int ch_rawprobe (ch_t *c, int dir, Time_t *t);
void ch_ready_color (void);
int ch_get_color (void);

/* export dump function for selection statements */
void ch_dump_time (ch_t *c, char *type);

#define Abs(a) ((a) >= 0 ? (a) : -(a))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CHANNEL_H__ */
