/*************************************************************************
 *
 *  Copyright (c) 1999 Cornell University
 *  Computer Systems Laboratory
 *  Ithaca, NY 14853
 *  All Rights Reserved
 *
 *  Permission to use, copy, modify, and distribute this software
 *  and its documentation for any purpose and without fee is hereby
 *  granted, provided that the above copyright notice appear in all
 *  copies. Cornell University makes no representations
 *  about the suitability of this software for any purpose. It is
 *  provided "as is" without express or implied warranty. Export of this
 *  software outside of the United States of America may require an
 *  export license.
 *
 *  $Id: prs.h,v 1.17 2010/04/06 18:14:31 rajit Exp $
 *
 *************************************************************************/
#ifndef __PRS_H__
#define __PRS_H__

#include "hash.h"
#include "lex.h"
#include "heap.h"
#include "mytime.h"
#include "array.h"
#include "names.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
/*#define DBG(format, args...) printf(format, args)*/
#define DBG(format, ...) do { printf("DBG: "); printf(format, ## __VA_ARGS__); } while (0)
#else
#define DBG(format, ...) 
#endif

#ifdef HEAP_DEBUG
#define HDBG(format, ...) do { printf("DBG: "); printf(format, ## __VA_ARGS__); } while (0)
#else
#define HDBG(format, ...) 
#endif

struct expr_tree;
typedef struct expr_tree PrsExpr;

struct prs_event;
typedef struct prs_event PrsEvent;

/*
  Used for tracing 
*/
struct tracing_info;

#define G_NORM 0
#define G_WEAK 1

typedef struct prs_node {
  struct prs_node *alias;	/* aliases */
  struct prs_node *alias_ring;	/* alias ring! */
  hash_bucket_t *b;		/* bucket pointer */
  PrsEvent *queue;		/* non-NULL if on the event queue */
  unsigned int val:2;		/* 0,1,X */
  unsigned int bp:1;		/* breakpoint */
  unsigned int flag:1;		/* marker to avoid double-counting vars */
  unsigned int unstab:1;	/* don't report instability */
  unsigned int exclhi:1;	/* part of an exclhi/excllo directive */
  unsigned int excllo:1;
  unsigned int exq:1;		/* 1 if the queue pointer is not to
				   the normal event queue */

  unsigned int seu:1;	/* 1 if the node is undergoing an seu event */

  unsigned int after_range:1;	/* 1 if we are using after range */

  int delay_up[2];		/* after delay on the node (up) */
  int delay_dn[2];		/* after delay on node (down) */
  long sz, max;
  PrsExpr **out;		/* fanout */
  PrsExpr *up[2], *dn[2];	/* pull-up/pull-down
				   [0] = normal
				   [1] = weak
				 */
  unsigned long tc;		/* transition-count */

  void *chinfo;			/* for channels */
  void *space;			/* for rent */
  struct tracing_info *tracing;	/* for tracing */

} PrsNode;

typedef struct excl_ring {
  PrsNode *n;
  struct excl_ring *next;
} PrsExclRing;

typedef struct raw_prs_node {
  struct prs_node *alias;
  struct raw_prs_node *alias_ring; /* ring pointer */
  hash_bucket_t *b;		/* bucket pointer */
} RawPrsNode;

struct expr_tree {
  struct expr_tree *l, *r, *u;
  unsigned type:4;
  short val, valx;
};

struct tracing_info {
  int sz[2], max[2];
  PrsNode **in[2];   /* 0 = down, 1 = up */
  unsigned long *count[2];
};


  /* SPECIAL TIME EVENT:
        n = node
	cause is overloaded to mean delay
	seu = 1
	force = 1
	weak = 0
	val = PRS_VAL_T/F
  */
struct prs_event {
  PrsNode *n;
  unsigned int val:4;
  unsigned int weak:1;		/* 1 if this is a weak event */
  unsigned int force:1;		/* 1 if this was forced! */

  unsigned int seu:1;		/* 1 if this was caused by an SEU */

  unsigned int start_seu:1;	/* 1 for starting an seu */
  unsigned int stop_seu:1;	/* 1 stop seu */
				   
  unsigned int kill:1;		/* 1 if event is to be discarded */

  unsigned int interf:1;	/* 1 if this is a pending interference */

  PrsNode *cause;		/* cause! */
};

enum {
  PRS_AND, PRS_OR, PRS_NOT, PRS_VAR, PRS_NODE_UP, PRS_NODE_DN,
  PRS_NODE_WEAK_UP, PRS_NODE_WEAK_DN
};

enum {
  PRS_VAL_T = 0,
  PRS_VAL_F = 1,
  PRS_VAL_X = 2			/* note!!! we rely on these values!!! */
};
/* WARNING: if you change this, need to change propagate_up, and the tables
   just before it */

#define PRS_NO_WEAK_INTERFERENCE  0x1 /* use during reset phase;
					 prevents warnings related to
					 weak interference (but the node
					 becomes X) during the reset
					 phase. */

#define PRS_STOP_SIMULATION       0x2 /* use to stop simulation,
					 eg. by an external interrupt
				      */

#define PRS_ESTIMATE_ENERGY       0x4 /* estimate energy dissipated */

#define PRS_RANDOM_TIMING	  0x8 /* random timings */

#define PRS_UNSTAB                0x10

#define PRS_RANDOM_TIMING_RANGE   0x20 /* random timing is within a range */

#define PRS_STOP_ON_WARNING       0x40 /* causes simulation to stop on a warning */
#define PRS_STOPPED_ON_WARNING	  0x80 /* specifies a warning caused the simulation to stop */

#define PRS_TRACE_PAIRS           0x100 /* specifies that we should record (cause,transition) pairs */

#define PRS_RANDOM_EXCL           0x200 /* specifies that if multiple
					   firings are possible for an
					   exclhi or excllo rule, pick
					   one at random */

typedef struct {
  struct Hashtable *H;		/* prs hash table */
  Heap *eventQueue;		/* event queue */

  A_DECL(PrsExclRing *, exhi);	/* exclusive high ring */
  A_DECL(PrsExclRing *, exlo);	/* exclusive low ring */

  Time_t time;			/* current time */
  unsigned long energy;		/* energy estimate */
  PrsEvent *ev_list;		/* memory allocation--avail list */
  unsigned int flags;		/* simulation control flags */
  unsigned int min_delay, max_delay; /* random timing range */

  NAMES_T *N;			/* names file, if packed
				   if NULL => not packed
				 */

  unsigned seed;		/* random number seed */
	
  /* global time expressions.
     This list is sorted by stop_time!
  */
} Prs;



/*
 * WARNING: these two functions are *not* thread-safe. They should be
 * called before the process is initialized. Typically one could
 * create a process by reading in production rules from a file,
 * attaching channels to the production rules, and then starting up
 * the multi-threaded part of the simulation.
 */
Prs *prs_file (FILE *fp);
Prs *prs_lex (LEX_T *L);
Prs *prs_fopen (char *filename);
Prs *prs_packfopen (char *file, char *names);
Prs *prs_packfile (FILE *fp, char *names);

/* get node corresponding to node name */
PrsNode *prs_node (Prs *, char *s);

/* set/clear breakpoints */
void prs_set_bp (Prs *, PrsNode *n);
void prs_clr_bp (Prs *, PrsNode *n);

/* return fanout in _preallocated_ array. use the num function below
   to find out the # of elements in the array 

   fanout_rule returns the up/dn expression, whereas fanout returns
   the nodes.
*/
void prs_fanout (Prs *, PrsNode *n, PrsNode **l);
void prs_fanout_rule (Prs *, PrsNode *n, PrsExpr **l);
int prs_num_fanout (PrsNode *n);

/* return fanin in _preallocated_ array. use the num function below
   to find out the # of elements in the array */
void prs_fanin  (Prs *, PrsNode *n, PrsNode **l);
int prs_num_fanin (PrsNode *n);

/* set node to value */
void prs_set_node (Prs *, PrsNode *n, int value);

/* set node to value at specified time */
void prs_set_nodetime (Prs *, PrsNode *n, int value, Time_t time);

/* seu event */
void prs_set_seu (Prs *p, PrsNode *n, int value, Time_t time, int dur);

/* run until either a breakpoint is reached or no more nodes left
   if there are no more nodes, ret val == NULL; otherwise return value 
   is the last node flipped
*/
PrsNode *prs_cycle (Prs *);
PrsNode *prs_cycle_cause (Prs *, PrsNode **, int *seu);

/* fire next event */
PrsNode *prs_step  (Prs *);

/* fire next event, returning cause */
PrsNode *prs_step_cause  (Prs *, PrsNode **cause, int *seu);

/* initialize circuit to all X */
void prs_initialize (Prs *);

extern char __prs_nodechstring[];
#define prs_nodechar(v) __prs_nodechstring[v]

#define prs_nodeval(n)  ((n)->val)

#define prs_reset_time(p)  ((p)->time = 0)

const char *prs_nodename (Prs *, PrsNode *);
const char *prs_rawnodename (Prs *, RawPrsNode *);
  
/* free storage */
void prs_free (Prs *p);

/* apply function at each node */
void prs_apply (Prs *p, void *cookie, void (*f)(PrsNode *, void *));

/* dump node to stdout */
void prs_dump_node (Prs *,PrsNode *n);
void prs_printrule (Prs *, PrsNode *n, int vals);
void prs_print_expr (Prs *, PrsExpr *n);

/* checkpoint and restore */
void prs_checkpoint (Prs *, FILE *);
void prs_restore (Prs *, FILE *);

#ifdef __cplusplus
}
#endif

#endif /* __PRS_H__ */
