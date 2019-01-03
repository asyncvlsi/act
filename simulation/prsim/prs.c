/**************************************************************************
 *
 *  Copyright (c) 1999-2000 Cornell University
 *  Computer Systems Laboratory
 *  Ithaca, NY 14853
 *  All Rights Reserved
 *
 *  $Id: prs.c,v 1.38 2010/05/14 18:24:25 rajit Exp $
 *
 *************************************************************************/
 
// TODO: The interference / instability stuff should *say* what caused the interference / instability...
 
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "prs.h"
#include "misc.h"
#include "heap.h"

/* used for printing purposes */
char __prs_nodechstring[] = { '1', '0', 'X' };
char __prs_nodechboolv[] = { 'T', 'F', 'X' };

/* forward declarations */
static void propagate_up (Prs *p, PrsNode *n, PrsExpr *e, int prev, int val, int is_seu);
static void parse_file (LEX_T *l, Prs *p);
static void init_tables (void);
static void merge_or_up (PrsNode *n, PrsExpr *e, int weak);
static void merge_or_dn (PrsNode *n, PrsExpr *e, int weak);
static void mk_out_link (PrsExpr *e, PrsNode *n);
static PrsNode *raw_lookup (char *s, struct Hashtable *H);
static void canonicalize_hashtable (Prs *p);
static void canonicalize_excllist (Prs *p);
static unsigned long random_number (Prs *p, PrsNode *n, int dir);

static int lex_is_idx (Prs *p, LEX_T *L);

/*
 *  Token definitions
 */
static int TOK_AND, TOK_OR, TOK_NOT, TOK_LPAR, TOK_RPAR,
  TOK_ID, TOK_ARROW, TOK_UP, TOK_DN, TOK_EQUAL, TOK_COMMA;

/* 
 *  Access node data structure stored in expr
 */
#define NODE(expr) ((PrsNode *)(((hash_bucket_t *)(expr)->l)->v))

static void random_init (void);
static Prs *prs_lex_internal (LEX_T *L, char *names);
static void process_names_conns (Prs *p);

#define UNSTAB_NODE(p,n) ((n)->unstab || ((p)->flags & PRS_UNSTAB))


/*
 * Check if it is an idx: coudl be if we have a packed file!
 */
static int lex_is_id (Prs *p, LEX_T *l);

static int lex_is_idx (Prs *p, LEX_T *L)
{
  if (!p->N) {
    return lex_is_id (p,L);
  }
  else {
    if (((unsigned)L->token[0]) >= 0x80)
      return 1;
    else if (L->token[0] >= 0x1 && L->token[0] <= 0x5)
      return 1;
  }
  return 0;
}

static int lex_have_exclhi (Prs *p, LEX_T *l)
{
  if (!p->N) {
    return lex_have_keyw (l, "mk_exclhi") || lex_have_keyw (l, "mk_excl");
  }
  else {
    if (lex_sym (l) == l_err && l->token[0] == 0x5) {
      lex_getsym (l);
      return 1;
    }
    return 0;
  }
}

static int lex_have_excllo (Prs *p, LEX_T *l)
{
  if (!p->N) {
    return lex_have_keyw (l, "mk_excllo");
  }
  else {
    if (lex_sym (l) == l_err && l->token[0] == 0x4) {
      lex_getsym (l);
      return 1;
    }
    return 0;
  }
}


static void prs_trace_pairs (PrsNode *n, PrsNode *cause)
{
  int idx;
  struct tracing_info *t;
  int i;

  if (!cause) return;
  if (n->val == PRS_VAL_X) return;
  
  if (n->val == PRS_VAL_T) { 
    idx = 1;
  }
  else {
    idx = 0;
  }
  t = n->tracing;
  Assert (t, "What?");

  for (i=0; i < t->sz[idx]; i++) {
    if (cause == t->in[idx][i]) {
      t->count[idx][i]++;
      return;
    }
  }
  
  if (t->max[idx] == 0) {
    t->max[idx] = 3;
    t->sz[idx] = 0;
    MALLOC (t->in[idx], PrsNode *, t->max[idx]);
    MALLOC (t->count[idx], unsigned long, t->max[idx]);
  }
  else if (t->max[idx] == t->sz[idx]) {
    t->max[idx] += 4;
    REALLOC (t->in[idx], PrsNode *, t->max[idx]);
    REALLOC (t->count[idx], unsigned long, t->max[idx]);
  }
  t->in[idx][i] = cause;
  t->count[idx][i] = 1;
  t->sz[idx]++;
}

/*
 *
 *   Read prs from file "fp", return data structure
 *
 */
Prs *prs_file (FILE *fp)
{
  Prs *p;
  LEX_T *l;

  random_init ();
  l = lex_file (fp);
  p = prs_lex (l);
  lex_free (l);

  return p;
}

Prs *prs_packfile (FILE *fp, char *name)
{
  Prs *p;
  LEX_T *l;

  random_init ();
  l = lex_file (fp);
  p = prs_lex_internal (l, name);
  lex_free (l);

  return p;
}

/*
 *
 *   Read prs from file "s"
 *
 */
Prs *prs_fopen (char *s)
{
  Prs *p;
  LEX_T *l;

  random_init ();
  l = lex_fopen (s);
  p = prs_lex (l);
  lex_free (l);

  return p;
}


/*
 *
 *   Read prs from packed file "s"
 *
 */
Prs *prs_packfopen (char *s, char *names)
{
  Prs *p;
  LEX_T *l;

  random_init ();
  l = lex_fopen (s);
  p = prs_lex_internal (l, names);
  lex_free (l);

  return p;
}

/*
 *  1 if it is an identifier token, 0 otherwise
 */
static int lex_is_id (Prs *p, LEX_T *l)
{
  if (!p->N) {
    if (lex_sym (l) == l_id || lex_sym (l) == l_string)
      return 1;
  }
  else {
    if (lex_sym (l) == l_err && ((unsigned)l->token[0] >= 0x80))
      return 1;
  }
  return 0;
}

/*
 *  Return pointer to string (which will change if you read in the next 
 *  token) corresponding to the identifier
 */
static char *lex_mustbe_id (Prs *p, LEX_T *l)
{
  if (!lex_is_id (p,l)) {
    fatal_error ("Expecting `id'\n\t%s", lex_errstring (l));
  }
  if (!p->N) {
    if (lex_sym (l) == l_string) {
      lex_getsym (l);
      lex_prev(l)[strlen(lex_prev(l))-1] = '\0';
      return lex_prev(l)+1;
    }
    else {
      lex_getsym (l);
      return lex_prev(l);
    }
  }
  else {
    IDX_TYPE idx;
    int offset;
    idx = 0;
    offset = 0;
    while (lex_sym (l) == l_err && (unsigned)l->token[0] >= 0x80) {
      idx |= ((unsigned)l->token[0] & 0x7f) << offset;
      offset += 7;
      lex_getsym (l);
    }
    return names_num2name (p->N, idx);
  }
}

/*
 *  Parse prs file and return simulation data structure
 */
static Prs *prs_lex_internal (LEX_T *L, char *names)
{
  Prs *p;
  int numtoks = 0;

  init_tables ();

#ifdef __addtok
#error naming conflict in macro
#endif
#define __addtok(name,string)  do { numtoks+=!lex_istoken(L,string); name=lex_addtoken(L,string); } while(0)

  __addtok (TOK_AND, "&");
  __addtok (TOK_OR, "|");
  __addtok (TOK_NOT, "~");
  __addtok (TOK_LPAR, "(");
  __addtok (TOK_RPAR, ")");
  __addtok (TOK_ARROW, "->");
  __addtok (TOK_UP, "+");
  __addtok (TOK_DN, "-");
  __addtok (TOK_EQUAL, "=");
  __addtok (TOK_COMMA, ",");

  TOK_ID = l_id;

#undef __addtok

  NEW (p, Prs);
  p->H = hash_new (128);
  p->seed = 0;
  p->flags = 0;
  if (names) {
    p->N = names_open (names);
  }
  else {
    p->N = NULL;
  }
  p->eventQueue = heap_new (128);
  p->time = 0;
  p->ev_list = NULL;
  p->energy = 0;
  A_INIT (p->exhi);
  A_INIT (p->exlo);

  lex_getsym (L);

  parse_file (L,p);

  lex_deltokens (L, numtoks);

  canonicalize_hashtable (p);

  /* exclhi/lo lists point to buckets during parsing, at this point
     they are converted to nodes */
  canonicalize_excllist (p);

  return p;
}

Prs *prs_lex (LEX_T *L)
{
  return prs_lex_internal (L, NULL);
}

/*
 *  Return node pointer corresponding to name "s"
 */
PrsNode *prs_node (Prs *p, char *s)
{
  return raw_lookup (s, p->H);
}


/*
 *  Set a breakpoint on node n
 */
void prs_set_bp (Prs *p, PrsNode *n)
{
  n->bp = 1;
}

/*
 *  Clear breakpoint on node n
 */
void prs_clr_bp (Prs *p, PrsNode *n)
{
  n->bp = 0;
}


/* 
 *  Return fanout of node in array "l" (preallocated!)
 */
void prs_fanout (Prs *p, PrsNode *n, PrsNode **l)
{
  int i;
  PrsExpr *e;

  for (i=0; i < n->sz; i++) {
    e = n->out[i];
    while (e && e->type != PRS_NODE_UP && e->type != PRS_NODE_DN)
      e = e->u;
    Assert (e, "You've *got* to be kidding");
    l[i] = NODE(e);
  }
}

/* 
 *  Return fanout of node in array "l" (preallocated!)
 */
void prs_fanout_rule (Prs *p, PrsNode *n, PrsExpr **l)
{
  int i;
  PrsExpr *e;

  for (i=0; i < n->sz; i++) {
    e = n->out[i];
    while (e && (e->type != PRS_NODE_UP && e->type != PRS_NODE_DN &&
		 e->type != PRS_NODE_WEAK_UP && e->type != PRS_NODE_WEAK_DN))
      e = e->u;
    Assert (e, "You've *got* to be kidding");
    l[i] = e;
  }
}

int prs_num_fanout (PrsNode *n)
{
  return n->sz;
}


/* 
 *  Return fanin of node in array "l", with length "*len"
 *
 */
static void apply (PrsExpr *e, void (*f)(PrsExpr *, void *), void *cookie)
{
  if (!e) return;

  (*f)(e, cookie);

  switch (e->type) {
  case PRS_AND:
  case PRS_OR:
  case PRS_NOT:
    for (e = e->l; e; e = e->r)
      apply (e, f, cookie);
    break;
  case PRS_NODE_UP:
  case PRS_NODE_DN:
    apply (e->r, f, cookie);
    break;
  case PRS_VAR:
    break;
  default:
    Assert (0, "Corrupt data structure. Insert $$$ to continue.");
    break;
  }
}

static void countvars (PrsExpr *e, int *x)
{
  if (e->type == PRS_VAR && NODE(e)->flag == 0) {
    NODE(e)->flag = 1;
    (*x)++;
  }
}

static void clrflags (PrsExpr *e, void *x)
{
  if (e->type == PRS_VAR)
    NODE(e)->flag = 0;
}

static void add_to_varlist (PrsExpr *e, PrsNode ***l)
{
  if (e->type == PRS_VAR && NODE(e)->flag == 0) {
    NODE(e)->flag = 1;
    *(*l) = (PrsNode *)e->l;
    (*l)++;
  }
}

typedef void (*APPLYFN) (PrsExpr *, void *);

void prs_fanin  (Prs *p, PrsNode *n, PrsNode **l)
{
  apply (n->up[G_NORM], (APPLYFN) add_to_varlist, &l);
  apply (n->dn[G_NORM], (APPLYFN) add_to_varlist, &l);
  apply (n->up[G_NORM], clrflags, NULL);
  apply (n->dn[G_NORM], clrflags, NULL);
}


int prs_num_fanin (PrsNode *n)
{
  int num = 0;

  apply (n->up[G_NORM], (APPLYFN) countvars, &num);
  apply (n->dn[G_NORM], (APPLYFN) countvars, &num);
  apply (n->up[G_NORM], clrflags, NULL);
  apply (n->dn[G_NORM], clrflags, NULL);

  return num;
}

/*
 *
 *  Memory allocation/deallocation functions
 *
 *  WARNING: the newnode()/delnode(), newrawnode(), newexpr()
 *  functions are *NOT* thread-safe. They should only be called by the
 *  parsing functions (which are not thread-safe either).
 *
 */
#ifndef UNIT_MALLOC
static PrsNode *node_list = NULL;
#endif

static PrsNode *newnode (void)
{
  PrsNode *n;
  int i;

#ifdef UNIT_MALLOC

  NEW (n, PrsNode);

#else
  if (!node_list) {
    MALLOC (node_list, PrsNode, 1024);
    for (i=0; i < 1023; i++) 
      node_list[i].alias = node_list+i+1;
    node_list[i].alias = NULL;
  }
  n = node_list;
  node_list = node_list->alias;
#endif

  n->queue = 0;
  n->bp = 0;
  n->val = PRS_VAL_X;
  n->sz = 0;
  n->max = 0;
  n->out = NULL;
  n->alias = NULL;
  n->alias_ring = n;
  n->up[0] = NULL;
  n->dn[0] = NULL;
  n->up[1] = NULL;
  n->dn[1] = NULL;
  n->unstab = 0;
  n->exclhi = 0;
  n->excllo = 0;
  n->exq = 0;
  n->flag = 0;
  n->tc = 0;
  n->seu = 0;
  n->after_range = 0;
  n->delay_up[0] = -1;
  n->delay_dn[0] = -1;
  n->delay_up[1] = -1;
  n->delay_dn[1] = -1;
  n->chinfo = NULL;
  n->space = NULL;
  n->tracing = NULL;
  
  return n;
}

static void delnode (PrsNode *n)
{
#if UNIT_MALLOC
  FREE (n);
#else
    n->alias = node_list;
    node_list = n;
#endif
}

static RawPrsNode *newrawnode (void)
{
  static int max_nodes = 0;
  static int num_nodes = 0;
  static RawPrsNode *n = NULL;
  RawPrsNode *p;

  if (max_nodes == num_nodes) {
    max_nodes = 1024;
    num_nodes = 0;
    MALLOC (n, RawPrsNode, max_nodes);
  }
  p = &n[num_nodes++];
  p->alias = NULL;
  p->alias_ring = p;
  p->b = NULL;
  return p;
}
 
static PrsExpr *expr_list = NULL;

static PrsExpr *newexpr (void)
{
  PrsExpr *e;
  int i;

  if (!expr_list) {
    MALLOC (expr_list, PrsExpr, 1024);
    for (i=0; i < 1023; i++)
      expr_list[i].r = expr_list+i+1;
    expr_list[i].r = NULL;
  }
  e = expr_list;
  expr_list = expr_list->r;

  e->l = NULL;
  e->r = NULL;
  e->u = NULL;
  e->type = 0;
  e->val = 0;
  e->valx = 0;

  return e;
}
 
static void delexpr (PrsExpr *e)
{
  e->r = expr_list;
  expr_list = e;
}

/*
 *  Allocate a new event (thread-safe, unlike other allocs :) )
 *
 */
static PrsEvent *rawnewevent (Prs *p)
{
  PrsEvent *e;
  int i;

  if (!p->ev_list) {
    MALLOC (p->ev_list, PrsEvent, 1024);
    for (i=0; i < 1023; i++) {
      p->ev_list[i].n = (PrsNode *)(p->ev_list+i+1);
    }
    p->ev_list[i].n = NULL;
  }
  e = p->ev_list;
  p->ev_list = (PrsEvent *)p->ev_list->n;

  e->n = NULL;
  e->val = 0;
  e->weak = 0;
  e->seu = 0;
  e->force = 0;
  e->start_seu = 0;
  e->stop_seu = 0;
  e->kill = 0;
  e->interf = 0;
  e->cause = NULL;
  return e;
}

/*
 *  Just allocate event, no checks, no updates
 *
 */
static PrsEvent *newevent (Prs *p, PrsNode *n, int val)
{
  PrsEvent *e;

#if 0
  if (strcmp (n->b->key, "t_y.d[0]") == 0) {
    printf ("queue ptr = %x [val=%c]\n", n->queue, __prs_nodechboolv[val]);
  }
#endif

  e = rawnewevent (p);

  Assert (n->queue == 0, "newevent() on a node already on the event queue");

  e->n = n;
  e->val = val;
  n->queue = e;
  n->exq = 0;
  return e;
}

static void deleteevent (Prs *p, PrsEvent *e)
{
  e->n = (PrsNode *)p->ev_list;
  p->ev_list = e;
}


/*
 *  Execute "set" function: puts (n:=value) on the pending event queue.
 */
void prs_set_node (Prs *p, PrsNode *n, int value)
{
  prs_set_nodetime (p, n, value, p->time);
}


/*
 *  Put (n := value) on queue @ time t
 */
void prs_set_nodetime (Prs *p, PrsNode *n, int value, Time_t time)
{
  PrsEvent *pe;

  if (n->exq) {
    printf ("WARNING: ignoring set_node on `%s' [in-excl-queue]\n", n->b->key);
    return;
  }
  if (value == n->val) return;
  if (n->queue) {
    n->queue->kill = 1;
    n->queue = NULL;
  }
  pe = newevent (p, n, value);
  pe->force = 1;
  HDBG("7. Inserting event for node %s -> %c\n", prs_nodename (p, pe->n), prs_nodechar(pe->val));
  heap_insert (p->eventQueue, time, pe);
}

/*
 *  Put (n := value) on queue @ time t
 */
void prs_set_seu (Prs *p, PrsNode *n, int value, Time_t time, int dur)
{
  PrsEvent *pe;

  if (n->up[G_NORM] == NULL ||
      n->dn[G_NORM] == NULL) {
    printf ("SEU events only supported on nodes that have a pull-up and pull-down\n");
    return;
  }
  if (n->exclhi || n->excllo) {
    printf ("SEU events only supported on non exclhi/excllo nodes\n");
    return;
  }
    

  pe = rawnewevent (p);
  pe->n = n;
  pe->start_seu = 1;
  pe->seu = 1;
  pe->val = value;
  
  heap_insert (p->eventQueue, time, pe);

  pe = rawnewevent (p);
  pe->n = n;
  pe->stop_seu = 1;
  pe->force = 1;
  pe->val = value;
  
  heap_insert (p->eventQueue, time + dur, pe);
}



/*
 *  Run until either a breakpoint is encountered, or no more events
 *  are pending.
 *
 *  Return value: NULL => no more events; non-NULL => bp on that node
 */
PrsNode *prs_cycle (Prs *p)
{
  PrsNode *n;

  while (n = prs_step (p)) {
    if (n->bp || (p->flags & PRS_STOP_SIMULATION))
      break;
  }
  return n;
}

PrsNode *prs_cycle_cause (Prs *p, PrsNode **m, int *seu)
{
  PrsNode *n;

  while (n = prs_step_cause (p, m, seu)) {
    if (n->bp || (p->flags & PRS_STOP_SIMULATION))
      break;
  }
  return n;
}

L_A_DECL(PrsEvent *, pendingQ);

typedef struct prs_eventlist {
  struct prs_event *p;
  Time_t t;
} PrsEventArray;

L_A_DECL(PrsEventArray,prsexclhi);
L_A_DECL(PrsEventArray,prsexcllo);
L_A_DECL(int,exclshuffle);

static void insert_exclhiQ (PrsEvent *pe, Time_t t)
{
  int i, j;

  A_NEW (prsexclhi, PrsEventArray);

  for (i=0; i < A_LEN (prsexclhi); i++) {
    if (prsexclhi[i].t > t) break;
  }
  if (i != A_LEN (prsexclhi)) {
    /* make space! */
    for (j = A_LEN (prsexclhi)-1; j >= i; j--) {
      prsexclhi[j+1] = prsexclhi[j];
    }
  }
  prsexclhi[i].p = pe;
  prsexclhi[i].t = t;
  A_INC (prsexclhi);
  pe->n->exq = 1;
}

static void insert_exclloQ (PrsEvent *pe, Time_t t)
{
  int i, j;

  A_NEW (prsexcllo, PrsEventArray);

  for (i=0; i < A_LEN (prsexcllo); i++) {
    if (prsexcllo[i].t > t) break;
  }
  if (i != A_LEN (prsexcllo)) {
    /* make space! */
    for (j = A_LEN (prsexcllo)-1; j >= i; j--) {
      prsexcllo[j+1] = prsexcllo[j];
    }
  }

  prsexcllo[i].p = pe;
  prsexcllo[i].t = t;
  A_INC (prsexcllo);

  pe->n->exq = 1;
}

static void insert_pendingQ (PrsEvent *pe)
{
  A_NEW (pendingQ, PrsEvent *);
  A_NEXT (pendingQ) = pe;
  A_INC (pendingQ);
}

	
#ifdef OLD
static int pending_weak[3][3] = { 
  { 0, 0, 1 },
  { 0, 0, 1 },
  { 1, 1, 0 }
};
#endif
static int pending_weak[3][3] = { 
  { 0, 0, 1 },
  { 0, 0, 0 },
  { 1, 0, 1 }
};

#define NEWTIMEUP(p,ev,type)					\
  time_add (p->time,						\
	    (ev)->n->delay_up[(type)] ?				\
	    (p->flags & PRS_RANDOM_TIMING ?			\
	     (Time_t)random_number(p,(ev)->n, 1) : ((Time_t)(ev)->n->delay_up[(type)])) : 0)

#define NEWTIMEDN(p,ev,type)					\
  time_add (p->time,						\
	    (ev)->n->delay_dn[(type)] ?				\
	    (p->flags & PRS_RANDOM_TIMING ?			\
	     (Time_t)random_number(p,(ev)->n,0) : ((Time_t)(ev)->n->delay_dn[(type)])) : 0)

extern double drand48(void);

#define MAX_VALUE (1<<16)	/* maximum value of random numbers */
static double LN_MAX_VAL;

/*------------------------------------------------------------------------
 *
 *  Initialize random number generator
 *
 *------------------------------------------------------------------------
 */
static
void random_init (void)
{
  LN_MAX_VAL = log(MAX_VALUE);
}

/*------------------------------------------------------------------------
 *
 * Random number generator, distribution 1/(1+x) from 0..MAX_VALUE so that
 * the std. dev of the distribution is large  (approx MAX_VALUE/sqrt(2))
 * compared to the mean ( O (MAX_VALUE/ln(MAX_VALUE+1)) )
 *
 *------------------------------------------------------------------------
 */
static unsigned long 
random_number (Prs *p, PrsNode *n, int dir)
{
  double d;
  unsigned long val;

  if (n->after_range) {
    d = (0.0 + rand_r (&p->seed))/RAND_MAX;
    /*d = drand48();*/
    if (dir) {
      val = n->delay_up[0] /* min */ + d*(n->delay_up[1]-n->delay_up[0]);
    }
    else {
      val = n->delay_dn[0] /* min */ + d*(n->delay_dn[1]-n->delay_dn[0]);
    }
    /* val cannot be 0, since 0 delay rules are removed */
    return val;
  }
  if (p->flags & PRS_RANDOM_TIMING_RANGE) {
    d = (0.0 + rand_r (&p->seed))/RAND_MAX;
    /*d = drand48();*/
    val = p->min_delay + d*(p->max_delay - p->min_delay);
  }
  else {
    /*d = drand48();*/
    d = (0.0 + rand_r (&p->seed))/RAND_MAX;
    val = exp(d*LN_MAX_VAL)-1;
  }
  if (val == 0) { val = 1; }
  return val;
}

static
int
expr_value (PrsExpr *e)
{
  Assert (e, "NULL pointer?!");
  switch (e->type) {
  case PRS_AND:
    if (e->val > 0) {
      return PRS_VAL_F;
    }
    else if (e->valx > 0) {  
      return PRS_VAL_X;
    }
    else {
      return PRS_VAL_T;
    }
    break;

  case PRS_OR:
    if (e->val > 0) {
      return PRS_VAL_T;
    } 
    else if (e->valx > 0) {
      return PRS_VAL_X;
    }
    else {
      return PRS_VAL_F;
    }
    break;

  case PRS_NOT:
    return e->val;
    break;
   
  case PRS_VAR:
    return NODE(e)->val;
    break;
 
  case PRS_NODE_UP:
  case PRS_NODE_DN:  
    return expr_value (e->r);
    break;
  
  default:
    printf ("FATAL ERROR!\n");
    exit (1);
  }
}

static
void
print_expr (Prs *P, PrsExpr *e, char * prefix)
{
  PrsExpr *x;
  char * p;

  p = (char*)malloc(sizeof(char)*100);
  p = strcpy(p,prefix);
  p = strcat(p,"\t");
         
  if(e == NULL) {
    printf("NULL\n");
    return;
  }
  
  switch (e->type) {
  case PRS_AND:
    printf ("%s AND: val: %d, valx: %d\n",prefix,e->val, e->valx);
    x = e->l;
    while (x) {
      Assert (x->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
      print_expr (P,x,p);
      x = x->r;
      if (x) printf (" ");
    }
    //printf ("\n");
    break;
  
  case PRS_OR:
    printf ("%s OR: val: %d, valx: %d\n",prefix,e->val, e->valx);
    x = e->l;
    while (x) {
      Assert (x->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
      print_expr (P, x,p);
      x = x->r;
      if (x) printf (" ");
    }
    //printf ("\n");
    break;
  
    break;
  case PRS_NOT:
    printf ("%s NOT: val: ",prefix);
/*    printf ("%s %s val: ", prefix,prs_nodename (NODE(e))); */
    if(e->val == PRS_VAL_T){printf("PRS_VAL_T\n");}
    else if(e->val == PRS_VAL_F){printf("PRS_VAL_F\n");}
    else if(e->val == PRS_VAL_X){printf("PRS_VAL_X\n");}
    else {printf("what the?\n");}

    Assert (e->l->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
    print_expr (P,e->l,p);
    //printf ("\n");
    break;
  case PRS_NODE_UP: 
  case PRS_NODE_WEAK_UP:
    printf ("PULL-UP:[%s]:%s\n ", prs_nodename(P,NODE(e)),
	    e->type == PRS_NODE_WEAK_DN ? "-weak" : "");
    Assert (e->r->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
    print_expr (P,e->r,p);
    printf ("\n");

    break;
  case PRS_NODE_DN:
  case PRS_NODE_WEAK_DN:
    printf ("PULL-DN:[%s]:%s\n ", prs_nodename(P,NODE(e)), 
	    e->type == PRS_NODE_WEAK_DN ? "-weak" : "");
    Assert (e->r->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
    print_expr (P,e->r,p);
    printf ("\n");
    break;
  case PRS_VAR:
    printf ("%s %s val: ", prefix,prs_nodename (P,NODE(e)));
    if(NODE(e)->val == PRS_VAL_T){printf("PRS_VAL_T\n");}
    else if(NODE(e)->val == PRS_VAL_F){printf("PRS_VAL_F\n");}
    else if(NODE(e)->val == PRS_VAL_X){printf("PRS_VAL_X\n");}
    else {printf("what the?\n");}
    break;
  default:
    fatal_error ("prop_up: unknown type %d\n", e->type);
    break;
  }
}

static
int
validate_expr (PrsExpr *e)
{
  PrsExpr *x;
  char * p;
  int value;   
  int myval, myvalx;
    
  if (!e) return 1;

  switch (e->type) {
  case PRS_AND:
    x = e->l;
    myval = 0;
    myvalx = 0;
    while (x) {
      if (!validate_expr (x))
        return 0;
      value = expr_value (x);
      if (value == PRS_VAL_F) {
        myval++;
      } 
      else if (value == PRS_VAL_X) {
        myvalx++;
      }
      x = x->r;
    }
    if (myval != e->val || myvalx != e->valx) return 0;
    return 1;
    break;

  case PRS_OR:
    x = e->l;
    myval = 0; 
    myvalx = 0;
    while (x) {
      if (!validate_expr (x))
        return 0;
      value = expr_value (x);
      if (value == PRS_VAL_T) {
        myval++;
      }
      else if (value == PRS_VAL_X) {
        myvalx++;
      }
      x = x->r;
    }
    if (myval != e->val || myvalx != e->valx) return 0;
    return 1;
    break;

  case PRS_NOT:
    if (!validate_expr (e->l))
      return 0;
    if (expr_value (e->l) == PRS_VAL_T) {
      if (e->val != PRS_VAL_F)
        return 0;
    }
    if (expr_value (e->l) == PRS_VAL_F) {
      if (e->val != PRS_VAL_T)
        return 0;
    }
    if (expr_value (e->l) == PRS_VAL_X) {
      if (e->val != PRS_VAL_X)
        return 0;
    }
    return 1;
    break;
  case PRS_NODE_UP:
  case PRS_NODE_WEAK_UP:
    return validate_expr (e->r);
    break;
  case PRS_NODE_DN:
  case PRS_NODE_WEAK_DN:
    return validate_expr (e->r);
    break;
  case PRS_VAR:
    return 1;
    break;

  default:
    fatal_error ("prop_up: unknown type %d\n", e->type);
  }
  assert(0);    // should never be reached
  return -1;
}


static
void
paranoid_check (Prs *P)
{
  int i;
  hash_bucket_t *b;

  for (i=0; i < P->H->size; i++)
    for (b = P->H->head[i]; b; b = b->next) {
      if (((PrsNode *)b->v)->seu) continue;
      if (((PrsNode *)b->v)->alias) continue;

      if (validate_expr (((PrsNode *)b->v)->up[G_NORM]) == 0) {
	printf ("Failed!!!\n");
	printf ("NODE up: %s\n", ((PrsNode *)b->v)->b->key);
	print_expr (P, ((PrsNode *)b->v)->up[G_NORM], "");
	exit (1);
      }
      if (validate_expr (((PrsNode *)b->v)->dn[G_NORM]) == 0) {
	printf ("Failed!!!\n");
	printf ("NODE dn: %s\n", ((PrsNode *)b->v)->b->key);
	print_expr (P, ((PrsNode *)b->v)->dn[G_NORM], "");
	exit (1);
      }
    }
}

/*
 *  Execute next event. Returns node corresponding to the event.
 */
PrsNode *prs_step  (Prs *p)
{
  return prs_step_cause (p, NULL, NULL);
}


static void process_exclhi (Prs *p, PrsNode *n)
{
  int j;
  PrsExclRing *er;
  PrsEvent *ne;
  int flag;

  /* if n->exclhi and n is now low
     check if any of the nodes in n's exclhi rings
     are enabled; if so, insert them into the exclQ.
  */
  for (j=0; j < A_LEN(p->exhi); j++) {
    er = p->exhi[j];
    flag = 0; /* flag for matches */
    do {
      if (er->n == n) {
	flag = 1;
	break;
      }
      er = er->next;
    } while (er != p->exhi[j]);
    if (flag) {
      er = p->exhi[j];
      do {
	if (!er->n->queue && er->n->up[G_NORM] && 
	    er->n->up[G_NORM]->val == PRS_VAL_T && !er->n->exq) {
	  ne = newevent (p, er->n, PRS_VAL_T);
	  ne->cause = n;
	  insert_exclhiQ (ne, NEWTIMEUP (p, ne, G_NORM));
	}
	er = er->next;
      } while (er != p->exhi[j]);
    }
  }
}

static void process_excllo (Prs *p, PrsNode *n)
{
  int j;
  PrsExclRing *er;
  PrsEvent *ne;
  int flag;

  for (j=0; j < A_LEN(p->exlo); j++) {
    er = p->exlo[j];
    flag = 0;
    do {
      if (er->n == n) {
	flag = 1;
	break;
      }
      er = er->next;
    } while (er != p->exlo[j]);
    if (flag) {
      er = p->exlo[j];
      do {
	if (!er->n->queue && er->n->dn[G_NORM] && 
	    er->n->dn[G_NORM]->val == PRS_VAL_T && !er->n->exq) {
	  ne = newevent (p, er->n, PRS_VAL_F);
	  ne->cause = n;
	  insert_exclloQ (ne, NEWTIMEDN (p, ne, G_NORM));
	}
	er = er->next;
      } while (er != p->exlo[j]);
    }
  }
}

static void add_seu_expr (Prs *P, PrsEvent *pe)
{
  PrsExpr *and, *or;
  PrsExpr *t;
  /* Add AND and OR links to the expression tree */

  if (pe->n->seu) {
    printf ("-- warning, node %s already undergoing an SEU; ignored\n", 
	    prs_nodename (P,pe->n));
    return;
  }

  if (pe->val == PRS_VAL_T) {
    /* up is an OR, dn is an AND */
    or = pe->n->up[G_NORM]->r;
    and = pe->n->dn[G_NORM]->r;
  }
  else {
    or = pe->n->dn[G_NORM]->r;
    and = pe->n->up[G_NORM]->r;
  }

  if (or->type != PRS_OR) {
    /* create a PRS_OR type */
    t = newexpr ();
    t->type = PRS_OR;
    t->l = or;
    t->u = or->u;
    or->u->r = t;
    or->u = t;
    if (t->u->val == PRS_VAL_T) {
      t->val = 2;
      t->valx = 0;
    }
    else if (t->u->val == PRS_VAL_X) {
      t->valx = 1;
      t->val = 1;
    }
    or = t;
  }
  else {
    or->val++;
  }
  if (and->type != PRS_AND) {
    t = newexpr ();
    t->type = PRS_AND;
    t->l = and;
    t->u = and->u;
    and->u->r = t;
    and->u = t;
    if (t->u->val == PRS_VAL_F) {
      t->val = 2;
      t->valx = 0;
    }
    else if (t->u->val == PRS_VAL_X) {
      t->valx = 1;
      t->val = 1;
    }
    and = t;
  }
  else {
    and->val++;
  }
  Assert (or->type == PRS_OR, "HMM");
  Assert (and->type == PRS_AND, "HMM2");
  /* fix guards */
  or->u->val = PRS_VAL_T;
  and->u->val = PRS_VAL_F;
  pe->n->seu = 1;
}

static int unlink_seu_expr (PrsEvent *pe)
{
  PrsExpr *and, *or;
  int free_or, free_and;
  PrsExpr *t;
  /* Add AND and OR links to the expression tree */

  if (!pe->n->seu) {
    return 0;
  }

  free_or = 0;
  free_and = 0;
  if (pe->val == PRS_VAL_T) {
    /* up is an OR, dn is an AND */
    or = pe->n->up[G_NORM]->r;
    and = pe->n->dn[G_NORM]->r;
  }
  else {
    or = pe->n->dn[G_NORM]->r;
    and = pe->n->up[G_NORM]->r;
  }
  Assert (or->type == PRS_OR, "What?");
  Assert (and->type == PRS_AND, "Eh?");

  if (or->l->r == NULL) {
    /* this is a dummy or! */
    or->l->u = or->u;
    or->u->r = or->l;
    free_or = 1;
    /* calculate value of OR: it should be used to set the value of
       the guard.
    */
  }
  or->val--;
  if (or->val > 0) {
    or->u->val = PRS_VAL_T;
  }
  else if (or->valx > 0) {
    or->u->val = PRS_VAL_X;
  }
  else {
    or->u->val = PRS_VAL_F;
  }
  if (and->l->r == NULL) {
    /* this is a dummy and! */
    and->l->u = and->u;
    and->u->r = and->l;
    free_and = 1;
    /* calculate value of OR: it should be used to set the value of
       the guard.
    */
  }
  and->val--;
  if (and->val > 0) {
    and->u->val = PRS_VAL_F;
  }
  else if (and->valx > 0) {
    and->u->val = PRS_VAL_X;
  }
  else {
    and->u->val = PRS_VAL_T;
  }
  if (free_or) {
    delexpr (or);
  }
  if (free_and) {
    delexpr (and);
  }
  pe->n->seu = 0;
  return 1;
}

static void print_event (Prs *P, PrsEvent *e)
{
  printf ("--- event ---\n", e);
  /*printf ("  node: %s;", prs_nodename (P,e->n));*/
  printf ("  node: %x;", e->n);
  printf (" val: %c\n", __prs_nodechboolv[e->val]);
  printf ("  weak: %d; ", e->weak);
  printf ("force: %d; ", e->force);
  printf ("seu: %d; ", e->seu);
  printf ("st-seu: %d; ", e->start_seu);
  printf ("en-seu: %d; ", e->stop_seu);
  printf ("kill: %d; ", e->kill);
  printf ("interf: %d\n", e->interf);
  printf( "---\n");
}

#define CH(x)   ((x) ? __prs_nodechboolv[(x)->val] : 'n')
static void print_node (Prs *P, PrsNode *n)
{
  printf (">> up=%c, w-up=%c, dn=%c, w-dn=%c\n", CH(n->up[G_NORM]), CH(n->up[G_WEAK]),
	  CH(n->dn[G_NORM]), CH(n->dn[G_WEAK]));
  if (n->queue) {
    print_event (P, n->queue);
  }
  else {
    printf (" -- no pending event\n");
  }
}


PrsNode *prs_step_cause  (Prs *p, PrsNode **cause,  int *pseu)
{
  PrsEventArray *ea;
  PrsExclRing *er;
  PrsEvent *pe, *ne;
  PrsNode *n;
  int i, j, force;
  int prev, flag;
  heap_key_t t;
  int seu;
  PrsNode *saved_cause;

#if 0
  paranoid_check (p);
#endif

  if (pseu) *pseu = 0;
 start:
  do {
    pe = (PrsEvent *) heap_remove_min_key (p->eventQueue,(heap_key_t*)&p->time);
  } while (pe && pe->kill == 1);

  if (!pe) return NULL;

  n = pe->n;

  if (pe->start_seu) {
    add_seu_expr (p,pe);
    if (cause) *cause = NULL;
  }
  else if (pe->stop_seu) {
    if (!unlink_seu_expr (pe)) {
      goto start;
    }
    pe->val = n->val;
    if (cause) *cause = NULL;
  }
  else {
    n->queue = NULL;
    if (cause) *cause = pe->cause;

    /* node being set to X, but is already X. This could occur because a
       node can get set to X due to things other than guards becoming X */
    if (pe->val == PRS_VAL_X && n->val == PRS_VAL_X) return n;

    if (!(n->seu || UNSTAB_NODE (p,n) || n->val != pe->val)) {
      print_event (p,pe);
      printf ("Curtime: %10llu\n", p->time);
      fatal_error ("Vacuous firings on the event queue");
    }
#if 0
    Assert (n->seu || UNSTAB_NODE(p,n) || n->val != pe->val,"Vacuous firings on the event queue");
#endif

    /* apply n := value; and now propagate the effect of this change  */
  }
  
  prev = n->val;
  n->val = pe->val;
  seu = pe->seu;
  if (pseu) *pseu = seu;
  force = pe->force;

  if (p->flags & PRS_TRACE_PAIRS) {
    if (pe->start_seu || pe->stop_seu) {
      prs_trace_pairs (n, NULL);
    }
    else {
      prs_trace_pairs (n, pe->cause);
    }
  }

  if (pe->start_seu || pe->stop_seu) {
     saved_cause = NULL;
  } 
  else {
     saved_cause = pe->cause;
  }
  deleteevent (p, pe);

  /* Propagate the changes */
  for (i=0; i < n->sz; i++) {
    propagate_up (p, n, n->out[i], prev, n->val, seu);
  }

  /* If it is a forced event, check its own up/dn guards to see if we
     need to add a new event for this node! */
  if (force && n->queue == NULL) {
    /* check set to 1 */
    if (n->up[G_NORM] && n->up[G_NORM]->val == PRS_VAL_T && n->val != PRS_VAL_T) {
      ne = newevent (p, n, PRS_VAL_T);
      insert_pendingQ (ne);
    }
    else if (n->dn[G_NORM] && n->dn[G_NORM]->val == PRS_VAL_T && n->val != PRS_VAL_F) {
      ne = newevent (p, n, PRS_VAL_F);
      insert_pendingQ (ne);
    }
    else if (n->up[G_WEAK] && n->up[G_WEAK]->val == PRS_VAL_T && n->val != PRS_VAL_T && (!n->dn[G_NORM] || n->dn[G_NORM]->val == PRS_VAL_F)) {
      ne = newevent (p, n, PRS_VAL_T);
      ne->weak = 1;
      insert_pendingQ (ne);
    }
    else if (n->dn[G_WEAK] && n->dn[G_WEAK]->val == PRS_VAL_T && n->val != PRS_VAL_F && (!n->up[G_NORM] || n->up[G_NORM]->val == PRS_VAL_F)) {
      ne = newevent (p, n, PRS_VAL_F);
      ne->weak = 1;
      insert_pendingQ (ne);
    }
  }
  /* If this is an X, check to see if its guards are in a state to
     clean up the X */
  if (n->val == PRS_VAL_X && n->queue == NULL) {
    /* check set to 1 */
    if (n->up[G_NORM] && n->up[G_NORM]->val == PRS_VAL_T && (!n->dn[G_NORM] || n->dn[G_NORM]->val == PRS_VAL_F)) {
      ne = newevent (p, n, PRS_VAL_T);
      ne->cause = saved_cause;
      heap_insert (p->eventQueue, NEWTIMEUP (p, ne, G_NORM), ne);
    }
    else if (n->dn[G_NORM] && n->dn[G_NORM]->val == PRS_VAL_T && (!n->up[G_NORM] || n->up[G_NORM]->val == PRS_VAL_F)) {
      ne = newevent (p, n, PRS_VAL_F);
      ne->cause = saved_cause;
      heap_insert (p->eventQueue, NEWTIMEDN (p, ne, G_NORM), ne);
    }
  }

  if (n->exclhi && n->val == PRS_VAL_F) {
    process_exclhi (p, n);
  }
  if (n->excllo && n->val == PRS_VAL_T) {
    process_excllo (p, n);
  }

  /* energy estimate: approximately the fanout of the node */
  if (p->flags & PRS_ESTIMATE_ENERGY) p->energy += n->sz;
  n->tc++;
  

  /* process the newly created pending events */
  for (i=0; i < A_LEN(pendingQ); i++) {
    ne = pendingQ[i];

    if (((ne->n->up[G_NORM] && ne->n->up[G_NORM]->val != PRS_VAL_F) &&
	 (ne->n->dn[G_NORM] && ne->n->dn[G_NORM]->val != PRS_VAL_F))) {
      /* there is interference. if there is weak interference,
	 don't report it unless we're supposed to.
	 weak = (X & T) or (T & X)
      */
      if (!(p->flags & PRS_NO_WEAK_INTERFERENCE) ||
	  !pending_weak[ne->n->up[G_NORM]->val][ne->n->dn[G_NORM]->val]) {
	if (!ne->interf) {
	  if (p->flags & PRS_STOP_ON_WARNING) {
	    p->flags |= PRS_STOP_SIMULATION|PRS_STOPPED_ON_WARNING;
	    
	  }
	  printf ("WARNING: %sinterference `%s'\n",
		  pending_weak[ne->n->up[G_NORM]->val][ne->n->dn[G_NORM]->val]?
		  "weak-" : "", prs_nodename (p,ne->n));
	  if (ne->cause) {
	    printf (">> cause: %s (val: %c)\n", 
		    prs_nodename (p,ne->cause), prs_nodechar (ne->cause->val));
	  }
	  printf (">> time: %10llu\n", p->time);
	}
      }
      if (ne->interf) {
	if (ne->n->queue) {
	  /*Assert (ne->n->queue, "What?");
	    This could happen because the pendingQ could have
	    duplicates for the node (interference)
	   */
	  ne->n->queue->cause = ne->cause;
	  ne->n->queue->val = PRS_VAL_X;
	  deleteevent (p, ne);
	}
      }
      else {
      /* turn node to "X" */
	ne->val = PRS_VAL_X;
	if (ne->n->val != PRS_VAL_X) {
	  HDBG("1. Inserting event for node %s -> %c\n", prs_nodename (p, ne->n), prs_nodechar(ne->val));
	  if (ne->n->val == PRS_VAL_T)
	    heap_insert (p->eventQueue, NEWTIMEUP (p, ne, G_NORM), ne);
	  else
	    heap_insert (p->eventQueue, NEWTIMEDN (p, ne, G_NORM), ne);
	}
	else {
	  ne->n->queue = NULL;
	  ne->n->exq = 0;
	  deleteevent (p, ne);
	}
      }
    }
    else {
      Time_t tmpt;
      /* insert pending event into event heap */
      if (!ne->interf && ne->n->val != ne->val && 
	  (ne->weak == 0 ||
	   ((ne->val == PRS_VAL_T && (!ne->n->dn[G_NORM] || ne->n->dn[G_NORM]->val == PRS_VAL_F)) ||
	    (ne->val == PRS_VAL_F && (!ne->n->up[G_NORM] || ne->n->up[G_NORM]->val == PRS_VAL_F))))) {
	/* if the event is weak, then we drop it on the floor unless
	   the opposing guard is false */

	if (ne->n->queue) {
	  if (ne->n->queue != ne) {
	    print_event (p,ne);
	    print_event (p,ne->n->queue);
	  }
	  Assert (ne->n->queue == ne, "What?!");
	}
	else {
	  ne->n->queue = ne;
	}
	if (ne) {
	  if (ne->val == PRS_VAL_T) {
	    heap_insert (p->eventQueue, tmpt = NEWTIMEUP (p, ne, ne->weak ? G_WEAK : G_NORM), ne);
	  }
	  else {
	    heap_insert (p->eventQueue, tmpt = NEWTIMEDN (p, ne, ne->weak ? G_WEAK : G_NORM), ne);
	  }
	}
      }
      else {
	/* if ne->interf is set, this is not linked to the event queue
	 */
	if (!ne->interf) {
	  /* we need to clear the event queue */
	  ne->n->queue = NULL;
	}
	ne->n->exq = 0;
	deleteevent (p, ne);
      }
    }
  }
  A_LEN(pendingQ) = 0;

  /* process created excl events
     - put the event onto the heap if it does not violate
     any existing excl hi/lo directive

     - drop the event otherwise!
  */
  if (p->flags & PRS_RANDOM_EXCL) {
    A_LEN(exclshuffle) = 0;
    for (i=0; i < A_LEN(prsexclhi); i++) {
      A_NEW (exclshuffle, int);
      A_NEXT (exclshuffle) = i;
      A_INC (exclshuffle);
    }
    for (i=A_LEN(exclshuffle)-1; i >= 0; i--) {
      int tmp;
      j = rand_r (&p->seed) % A_LEN (exclshuffle);
      tmp = exclshuffle[j];
      exclshuffle[j] = exclshuffle[i];
      exclshuffle[i] = tmp;
    }
  }
  for (i=0; i < A_LEN(prsexclhi); i++) {
    if (p->flags & PRS_RANDOM_EXCL) {
      ea = &prsexclhi[exclshuffle[i]];
    }
    else {
      ea = &prsexclhi[i];
    }
    /* look through the events. if any of them have a pending
       queue entry, then we're done
    */
    for (j=0; j < A_LEN(p->exhi); j++) {
      er = p->exhi[j];

#define VALMATCH(v)							\
      (er->n->val==(v)||((er->n->queue&&er->n->queue->val==(v) && !er->n->exq)))

      prev = 0; /* count # of true/pending true nodes */
      flag = 0; /* flag for matches */

      do {
	if (VALMATCH(PRS_VAL_T)) prev++;
	if (er->n == ea->p->n)  flag = 1;
	er = er->next;
      } while (er != p->exhi[j]);

      /* if flag is true and prev > 0: we have a fail */

#if 0
      if (flag) {
	if (!prev) {
	  /* insert event into real queue */
	  HDBG("3. Inserting event for node %s -> %c\n", prs_nodename (p, ea->p->n), prs_nodechar(ea->p->val));
	  heap_insert (p->eventQueue, ea->t, ea->p);
	  ea->p->n->exq = 0;
	}
      }
#endif

      if (flag && prev > 0) {
	/* drop this event */
	break;
      }
    }

    if (j == A_LEN(p->exhi)) {
      HDBG("3. Inserting event for node %s -> %c\n", prs_nodename (p, ea->p->n), prs_nodechar(ea->p->val));
      heap_insert (p->eventQueue, ea->t, ea->p);
      ea->p->n->exq = 0;
    }

    if (ea->p->n->exq) {
      ea->p->n->queue = NULL;
      ea->p->n->exq = 0;
      deleteevent (p, ea->p);
    }
  }
  A_LEN(prsexclhi) = 0;

  if (p->flags & PRS_RANDOM_EXCL) {
    A_LEN (exclshuffle) = 0;
    for (i=0; i < A_LEN(prsexcllo); i++) {
      A_NEW (exclshuffle, int);
      A_NEXT (exclshuffle) = i;
      A_INC (exclshuffle);
    }
    for (i=A_LEN(exclshuffle)-1; i >= 0; i--) {
      int tmp;
      j = rand_r (&p->seed) % A_LEN (exclshuffle);
      tmp = exclshuffle[j];
      exclshuffle[j] = exclshuffle[i];
      exclshuffle[i] = tmp;
    }
  }
  for (i=0; i < A_LEN(prsexcllo); i++) {
    if (p->flags & PRS_RANDOM_EXCL) {
      ea = &prsexcllo[exclshuffle[i]];
    }
    else {
      ea = &prsexcllo[i];
    }
    for (j=0; j < A_LEN(p->exlo); j++) {
      er = p->exlo[j];

      prev = 0; /* count # of true/pending true nodes */
      flag = 0; /* flag for matches */
      do {
	if (VALMATCH(PRS_VAL_F)) prev++;
	if (er->n == ea->p->n)  flag = 1;
	er = er->next;
      } while (er != p->exlo[j]);

#if 0
      if (flag) {
	if (!prev) {
	  /* insert event into real queue */
	  HDBG("4. Inserting event for node %s -> %c\n", prs_nodename (p, ea->p->n), prs_nodechar(ea->p->val));
	  heap_insert (p->eventQueue, ea->t, ea->p);
	  ea->p->n->exq = 0;
	}
      }
#endif

      if (flag && prev > 0) {
	/* drop this event */
	break;
      }
    }

    if (j == A_LEN(p->exlo)) {
      HDBG("4. Inserting event for node %s -> %c\n", prs_nodename (p, ea->p->n), prs_nodechar(ea->p->val));
      heap_insert (p->eventQueue, ea->t, ea->p);
      ea->p->n->exq = 0;
    }

    if (ea->p->n->exq) {
      ea->p->n->queue = NULL;
      ea->p->n->exq = 0;
      deleteevent (p, ea->p);
    }
  }
  A_LEN(prsexcllo) = 0;

  return n;
}


static struct event_update {
  unsigned vacuous:1;		/* 1 if vacuous firing */
  unsigned unstab:1;		/* 1 if unstable */
  unsigned interf:1;		/* 1 if interference */
  unsigned weak:1;		/* 1 if weak-inter/weak-unstab */
} prs_upguard[3][3], prs_dnguard[3][3];
    
static int not_table[3];

/* first index: guard state; second index: pending event state */

#define SET(x,a,b,c,e) do { x.vacuous=a; x.unstab=b; x.interf=c; x.weak=e; } while(0)

static void init_tables (void)
{
  /* UP-rules */

  /* guard = T, event = T or X: vacuous firing */
  SET(prs_upguard[PRS_VAL_T][PRS_VAL_T],1,0,0,0);
  SET(prs_upguard[PRS_VAL_T][PRS_VAL_X],1,0,0,0);
  /* guard = T, event = F: inteference */
  SET(prs_upguard[PRS_VAL_T][PRS_VAL_F],0,0,1,0);

  /* guard = F, event = T: unstable node */
  SET(prs_upguard[PRS_VAL_F][PRS_VAL_T],0,1,0,0);
  /* guard = F, event = X: vacuous */
  SET(prs_upguard[PRS_VAL_F][PRS_VAL_X],1,0,0,0);
  SET(prs_upguard[PRS_VAL_F][PRS_VAL_F],1,0,0,0);
  
  /* guard = X, event = T: weak unstab */
  SET(prs_upguard[PRS_VAL_X][PRS_VAL_T],0,1,0,1);
  /* guard = X, event = F: weak interf */
  SET(prs_upguard[PRS_VAL_X][PRS_VAL_F],0,0,1,1);
  /* guard = X, event = X: nothing */
  SET(prs_upguard[PRS_VAL_X][PRS_VAL_X],1,0,0,0);

  /* DN-rules */

  /* guard = T, event = F or X: vacuous firing */
  SET(prs_dnguard[PRS_VAL_T][PRS_VAL_F],1,0,0,0);
  SET(prs_dnguard[PRS_VAL_T][PRS_VAL_X],1,0,0,0);
  /* guard = T, event = F: inteference */
  SET(prs_dnguard[PRS_VAL_T][PRS_VAL_T],0,0,1,0);

  /* guard = F, event = F: unstable node */
  SET(prs_dnguard[PRS_VAL_F][PRS_VAL_F],0,1,0,0);
  /* guard = F, event = F/X: vacuous */
  SET(prs_dnguard[PRS_VAL_F][PRS_VAL_X],1,0,0,0);
  SET(prs_dnguard[PRS_VAL_F][PRS_VAL_T],1,0,0,0);
  
  /* guard = X, event = T: weak interf */
  SET(prs_dnguard[PRS_VAL_X][PRS_VAL_T],0,0,1,1);
  /* guard = X, event = F: weak unstab */
  SET(prs_dnguard[PRS_VAL_X][PRS_VAL_F],0,1,0,1);
  /* guard = X, event = X: nothing */
  SET(prs_dnguard[PRS_VAL_X][PRS_VAL_X],1,0,0,0);

  not_table[PRS_VAL_T] = PRS_VAL_F;
  not_table[PRS_VAL_F] = PRS_VAL_T;
  not_table[PRS_VAL_X] = PRS_VAL_X;
}
#undef SET

/*
 *  main expression evaluation engine.
 */
static void propagate_up (Prs *p, PrsNode *root, PrsExpr *e, int prev, int val,
			  int is_seu)
{
  PrsNode *n;
  PrsEvent *pe;
  struct event_update *eu;
  int weak = 0;

  register int old_val, new_val;
  register PrsExpr *u;


  int trace = 0;

 start:
  u = e->u;
  Assert (e, "propagate_up: NULL argument expression");
  Assert (u, "propagate_up: no up-link for expression");

  switch (u->type) {
  case PRS_AND:
    old_val = (u->val > 0) ? PRS_VAL_F : ((u->valx == 0) ? PRS_VAL_T : 
					  PRS_VAL_X);
    /* remove old value information */

    /* 1. reduce X count */
    u->valx -= (prev >> 1);
    /* 2. for "AND", we count the number of zeros. If the # of zeros ==
       0, the node is a 1 */
    u->val -= (prev&1);

    /* add new value information */
    u->valx += (val >> 1);
    u->val += (val&1);
    
    new_val = (u->val > 0) ? PRS_VAL_F : ((u->valx == 0) ? PRS_VAL_T : 
					  PRS_VAL_X);
    if (old_val != new_val) {
      prev = old_val;
      val = new_val;
      e = u;
      goto start;
    }
    break;

  case PRS_OR:
    old_val = (u->val > 0) ? PRS_VAL_T : ((u->valx == 0) ? PRS_VAL_F : 
					  PRS_VAL_X);
    
    /* remove old value information */

    /* 1. reduce X count */
    u->valx -= (prev >> 1);
    /* 2. for "OR", we count the number of 1's */
    u->val -= !prev;
    /*u->val -= (1^(prev&1)^(prev>>1));*/

    /* add new value information */
    u->valx += (val >> 1);
    u->val += !val;
    /*u->val += (1^(val&1)^(val>>1));*/
    
    new_val = (u->val > 0) ? PRS_VAL_T : ((u->valx == 0) ? PRS_VAL_F : 
					  PRS_VAL_X);
    if (old_val != new_val) {
      prev = old_val;
      val = new_val;
      e = u;
      goto start;
    }
    break;
  case PRS_NOT:
    /* for NOT, we simply store the value of the inverted node */
    old_val = u->val;
    u->val = not_table[val];
    if (old_val != u->val) {
      prev = old_val;
      val = u->val;
      e = u;
      goto start;
    }
    break;
  case PRS_NODE_UP:
  case PRS_NODE_WEAK_UP:
    if (u->type == PRS_NODE_WEAK_UP) weak = 1;
    else weak = 0;

    u->val = val;
    n = NODE (u);

    /* the node is either T, F, or X. Either way, it's a change.
       If the node is T or X, insert into pending Q.

       If the guard becomes false, this could be an instability. It is
       an instability IF the output would have been turned on by the
       guard.
    */

#if 0
    if (strcmp (n->b->key, "dut.dut.bank.array._bl[0][3][0]") == 0) {
      trace = 1;
    }
#endif

#if 0
    if (trace) {
      printf ("BEFORE-UP: ");
      print_node (p, n);
    }
#endif

    if (!n->queue) {
      /* no pending event */
      if ((val == PRS_VAL_T && n->val != PRS_VAL_T) ||
	  (val == PRS_VAL_X && n->val == PRS_VAL_F)) {
	pe = newevent (p, n, val);
	pe->weak = weak;
	pe->cause = root;
	pe->seu = is_seu;
	if (n->exclhi) {
	  insert_exclhiQ (pe, NEWTIMEUP (p, pe, G_NORM));
	}
	else {
	  if (n->dn[G_NORM])
	    insert_pendingQ (pe);
	  else {
	    heap_insert (p->eventQueue, NEWTIMEUP (p, pe, G_NORM), pe);
	  }
	}
      }
      else if (val == PRS_VAL_F) {
	/* This case is to allow the simulator to recover from
	   interference.
	*/
	if (n->dn[G_NORM] && n->dn[G_NORM]->val == PRS_VAL_T) {
	  pe = newevent (p, n, PRS_VAL_F);
	  pe->cause = root;
	  pe->seu = is_seu;
	  if (n->excllo) {
	    insert_exclloQ (pe, NEWTIMEUP (p, pe, G_NORM));
	  }
	  else {
	    insert_pendingQ (pe);
	  }
	}
	else if ((!n->dn[G_NORM] || n->dn[G_NORM]->val == PRS_VAL_F) && 
		 (n->dn[G_WEAK] && n->dn[G_WEAK]->val == PRS_VAL_T)) {
	  /* weak recovery */
	  pe = newevent (p, n, PRS_VAL_F);
	  pe->cause = root;
	  pe->weak = 1;
	  pe->seu = is_seu;
	  if (n->excllo) {
	    insert_exclloQ (pe, NEWTIMEUP (p, pe, G_WEAK));
	  }
	  else {
	    insert_pendingQ (pe);
	  }
	}
      }
    }
    else if (!n->exq) {
#if 0
      if (trace) {
	printf ("Here\n");
      }
#endif

      /* pending event is in the queue already */
      if (val == PRS_VAL_F && n->dn[G_NORM] && n->dn[G_NORM]->val == PRS_VAL_T &&
	  n->queue->val == PRS_VAL_X && n->val != PRS_VAL_F) {
	/* there was a pending "X" in the queue; convert it */
	n->queue->val = PRS_VAL_F;
	n->queue->cause = root;

#if 0
	if (trace) {
	  printf ("AFTER-UP: ");
	  print_node (p, n);
	}
#endif

	break;
      }
      eu = &prs_upguard[val][n->queue->val];

#if 0
      if (trace) {
	printf ("eu: vac=%d, unstab=%d, interf=%d, weak=%d\n",
		eu->vacuous, eu->unstab, eu->interf, eu->weak);
      }
#endif

      if (!eu->vacuous) {
	if (eu->unstab && !UNSTAB_NODE (p,n) || eu->interf) {
	  if (eu->interf) {
	    /* now the question is: is this real? It should be checked
	       by the pending queue...
	    */
	    if (!weak && n->queue->weak) {
	      /* pending weak event is squashed */
	      n->queue->kill = 1;
	      n->queue = NULL;
	    }
	    else {
	      pe = rawnewevent (p);
	      pe->n = n;
	      pe->interf = 1;
	      pe->cause = root;
	      pe->val = PRS_VAL_T;
	      insert_pendingQ (pe);
	    }
	  }
	  if (eu->unstab && !UNSTAB_NODE(p,n)) {
	    if (!is_seu) {
	      if (!weak || !(n->up[G_NORM] && (n->up[G_NORM]->val == PRS_VAL_T))) {
		n->queue->cause = root;
		n->queue->val = PRS_VAL_X;
		printf ("WARNING: %sunstable `%s'+\n",
			eu->weak ? "weak-" : "", prs_nodename (p,n));
		printf (">> cause: %s (val: %c)\n", 
			prs_nodename (p,root), prs_nodechar (root->val));
		printf (">> time: %10llu\n", p->time);
		if (p->flags & PRS_STOP_ON_WARNING) {
		  p->flags |= PRS_STOP_SIMULATION|PRS_STOPPED_ON_WARNING;
		}
	      }
	    }
	    else {
	      n->queue->kill = 1;
	      n->queue = NULL;
	    }
	  }
	}
	if (eu->unstab && UNSTAB_NODE (p,n)) {
	  n->queue->kill = 1;
	  n->queue = NULL;
	}
      }
      else {
	/* a vacuous change might disable a pending weak firing! */
	if (n->queue->weak) {
	  n->queue->kill = 1;
	  n->queue = NULL;
	}
      }
    }
#if 0
    if (trace) {
      printf ("AFTER-UP: ");
      print_node (p, n);
    }
#endif

    break;
  case PRS_NODE_DN:
  case PRS_NODE_WEAK_DN:
    if (u->type == PRS_NODE_WEAK_DN) weak = 1;
    else weak = 0;

    u->val = val;
    n = NODE (u);

#if 0
    if (strcmp (n->b->key, "dut.dut.bank.array._bl[0][3][0]") == 0) {
      trace = 1;
    }
#endif

#if 0
    if (trace) {
      printf ("BEFORE-DN: ");
      print_node (p, n);
    }
#endif

    if (!n->queue) {
      if ((val == PRS_VAL_T && n->val != PRS_VAL_F) ||
	  (val == PRS_VAL_X && n->val == PRS_VAL_T)) {
	pe = newevent (p, n, not_table[val]);
	pe->weak = weak;
	pe->cause = root;
	pe->seu = is_seu;
	if (n->excllo) {
	  insert_exclloQ (pe, NEWTIMEDN (p, pe, G_NORM));
	}
	else {
	  if (n->up[G_NORM]) 
	    insert_pendingQ (pe);
	  else  {
	    HDBG("6. Inserting event for node %s -> %c\n", prs_nodename (p, pe->n), prs_nodechar(pe->val));
	    heap_insert (p->eventQueue, NEWTIMEDN (p, pe, G_NORM), pe);
	  }
	}
      }
      else if (val == PRS_VAL_F) {
	if (n->up[G_NORM] && n->up[G_NORM]->val == PRS_VAL_T) {
	  pe = newevent (p, n, PRS_VAL_T);
	  pe->cause = root;
	  pe->seu = is_seu;
	  if (n->exclhi) {
	    insert_exclhiQ (pe, NEWTIMEDN (p, pe, G_NORM));
	  }
	  else {
	    insert_pendingQ (pe);
	  }
	}
	else if ((!n->up[G_NORM] || n->up[G_NORM]->val == PRS_VAL_F) &&
		 (n->up[G_WEAK] && n->up[G_WEAK]->val == PRS_VAL_T)) {
	  pe = newevent (p, n, PRS_VAL_T);
	  pe->cause = root;
	  pe->weak = 1;
	  pe->seu = is_seu;
	  if (n->exclhi) {
	    insert_exclhiQ (pe, NEWTIMEDN (p, pe, G_WEAK));
	  }
	  else {
	    insert_pendingQ (pe);
	  }
	}
      }
    }
    else if (!n->exq) {
      if (val == PRS_VAL_F && n->up[G_NORM] && n->up[G_NORM]->val == PRS_VAL_T &&
	  n->queue->val == PRS_VAL_X && n->val != PRS_VAL_T) {
	/* there is a pending "X" in the queue */
	n->queue->cause = root;
	n->queue->val = PRS_VAL_T;

#if 0
	if (trace) {
	  printf ("AFTER-DN: ");
	  print_node (p, n);
	}
#endif

	break;
      }
      eu = &prs_dnguard[val][n->queue->val];
      if (!eu->vacuous) {
	if (eu->unstab && !UNSTAB_NODE(p,n) || eu->interf) {
	  if (eu->interf) {
	    /* insert into pending queue! */
	    if (!weak && n->queue->weak) {
	      /* pending weak event is squashed */
	      n->queue->kill = 1;
	      n->queue = NULL;
	    }
	    else {
	      pe = rawnewevent (p);
	      pe->n = n;
	      pe->interf = 1;
	      pe->cause = root;
	      pe->val = PRS_VAL_F;
	      insert_pendingQ (pe);
	    }
	  }
	  if (eu->unstab && !UNSTAB_NODE (p,n)) {
	    if (!is_seu) {
	      if (!weak || !(n->dn[G_NORM] && (n->dn[G_NORM]->val == PRS_VAL_T))) {
	      n->queue->cause = root;
	      n->queue->val = PRS_VAL_X;
	      printf ("WARNING: %sunstable `%s'-\n",
		      eu->weak ? "weak-" : "", prs_nodename (p,n));
	      printf (">> cause: %s (val: %c)\n", 
		      prs_nodename (p,root), prs_nodechar (root->val));
	      printf (">> time: %10llu\n", p->time);
	      if (p->flags & PRS_STOP_ON_WARNING) {
		p->flags |= PRS_STOP_SIMULATION|PRS_STOPPED_ON_WARNING;
	      }
	      }
	    }
	    else {
	      n->queue->kill = 1;
	      n->queue = NULL;
	    }
	  }
	}
	if (eu->unstab && UNSTAB_NODE (p,n)) {
	  n->queue->kill = 1;
	  n->queue = NULL;
	}
      }
      else {
	/* a pending weak firing can be disabled by a vacuous firing */
	if (n->queue->weak) {
	  n->queue->kill = 1;
	  n->queue = NULL;
	}
      }
    }

#if 0
    if (trace) {
      printf ("AFTER-DN: ");
      print_node (p, n);
    }
#endif

    break;
  case PRS_VAR:
    Assert (0, "This is insane");
    break;

  default:
    fatal_error ("prop_up: unknown type %d\n", u->type);
    break;
  }
}

static void parse_prs (Prs *p, LEX_T *l);
static void parse_connection (Prs *p, LEX_T *l);
static void parse_excl (LEX_T *l, int ishi, Prs *p);


static void _check_delays (PrsNode *n, void *v)
{
  int i;
  Prs *P = (Prs *)v;

  for (i=0; i < 2; i++) {
    if (n->up[i]) {
      if (n->delay_up[i] == -1) {
	printf ("NODE %s: rule with negative delay\n", prs_nodename(P,n));
      }
    }
    if (n->dn[i]) {
      if (n->delay_dn[i] == -1) {
	printf ("NODE %s: rule with negative delay\n", prs_nodename(P,n));
      }
    }
  }
}

static void parse_file (LEX_T *l, Prs *p)
{
  PrsExpr *e, *ret;
#ifdef VERBOSE
  int nrules = 0;
#endif

  if (p->N) {
    /* read connections! */
    process_names_conns (p);
  }

  while (!lex_eof (l) && !(!lex_is_idx (p,l) && (lex_sym (l) == l_err))) {
#ifdef VERBOSE
    if (((nrules + 1) % 10000) == 0) {
      printf ("%dK...", (nrules+1)/1000);
      fflush (stdout);
    }
#endif
    if (lex_have_keyw (l, "connect") || lex_have (l, TOK_EQUAL))
      parse_connection (p,l);
    else if (lex_have_exclhi (p,l)) {
      parse_excl (l, 1, p);
    }
    else if (lex_have_excllo (p,l)) {
      parse_excl (l, 0, p);
    }
    else {
      parse_prs (p,l);
    }
#ifdef VERBOSE
    nrules++;
#endif
  }

#ifdef VERBOSE
  printf ("Read %d rules\n\n", nrules);
#endif

  /* sanity check delays */
  prs_apply (p, p, _check_delays);
}


/* self-flattening alias tree */
static PrsNode *canonical_name (PrsNode *n)
{
  PrsNode *r, *tmp;

  Assert (!n->alias || !n->alias->alias, "What on earth");
  
  if (n->alias) {
    Assert (n->alias->alias == NULL, "What?");
    return n->alias;
  }
  else {
    return n;
  }

#if 0
  r = n;
  while (n->alias)
    n = n->alias;
  if (n != r) {
    while (r->alias) {
      tmp = r->alias;
      r->alias = n;
      r = tmp;
    }
  }
  return n;
#endif
}

static void canonicalize_hashtable (Prs *p)
{
  int i;
  hash_bucket_t *b;

  for (i=0; i < p->H->size; i++)
    for (b = p->H->head[i]; b; b = b->next) {
      b->v = (void*) canonical_name ((PrsNode *)b->v);
    }
}

static void canonicalize_excllist (Prs *p)
{
  int i;
  PrsExclRing *r;

  for (i=0; i < A_LEN(p->exhi); i++) {
    r = p->exhi[i];
    do {
      r->n = canonical_name ((PrsNode *)(((hash_bucket_t *)r->n)->v));
      r = r->next;
    } while (r != p->exhi[i]);
  }
  for (i=0; i < A_LEN(p->exlo); i++) {
    r = p->exlo[i];
    do {
      r->n = canonical_name ((PrsNode *)(((hash_bucket_t *)r->n)->v));
      r = r->next;
    } while (r != p->exlo[i]);
  }
}

static PrsNode *raw_lookup (char *s, struct Hashtable *H)
{
  hash_bucket_t *b;
  PrsNode *n, *r;

  b = hash_lookup (H, s);
  if (!b) {
    return NULL;
  }
  else {
    n = (PrsNode *)b->v;
    return canonical_name (n);
  }
}

static PrsNode *raw_insert (char *s, struct Hashtable *H)
{
  hash_bucket_t *b;
  PrsNode *n;

  b = hash_add (H, s);
  n = newnode ();
  /* link from node <-> bucket */
  n->b = b;
  b->v = (void*)n;

  return n;
}

/*
 *  Returns PrsNode * corresponding to node name "s"
 */
static PrsNode *lookup (char *s, struct Hashtable *H)
{
  PrsNode *n;

  n = raw_lookup (s, H);
  if (!n) {
    return raw_insert (s, H);
  }
  else
    return n;
}


/* 
 *  Merge data structures (except alias stuff and hash bucket). 
 *  n1 gets stuff that used to be in n2.
 *
 *  [ n1 := n1 UNION n2 ]
*/
static void merge_nodes (PrsNode *n1, PrsNode *n2)
{
  PrsExpr *e;
  int i;

  Assert (n1->val == n2->val, "Oh my god. You aren't parsing???");
  Assert (n1->queue == NULL, "Oh my god. You aren't parsing???");
  Assert (n2->queue == NULL, "Oh my god. You aren't parsing???");
  Assert (n1->bp == 0, "Oh my god. You aren't parsing???");
  Assert (n2->bp == 0, "Oh my god. You aren't parsing???");

  for (i=0; i < 2; i++) {
    if (n2->up[i]) {
      Assert (n2->up[i]->r, "Hmm...");
      merge_or_up (n1, n2->up[i]->r, i);
      delexpr (n2->up[i]);
    }
    if (n2->dn[i]) {
      Assert (n2->dn[i]->r, "Hmm...");
      merge_or_dn (n1, n2->dn[i]->r, i);
      delexpr (n2->dn[i]);
    }
  }

  /* move fanout information */
  for (i=0; i < n2->sz; i++) {
    e = n2->out[i];
    Assert (e, "yow!");
    Assert (e->type == PRS_VAR, "woweee!");
    Assert (NODE(e) == n2, "Oh my god");
    mk_out_link (e, n1);
    Assert (NODE(e) == n1, "You're hosed.");
  }

  if (n2->max > 0) {
    FREE (n2->out);
  }
  n2->max = 0;
  n2->sz = 0;

  /* merge flags */
  if (n2->unstab) {
    n1->unstab = 1;
  }
  if (n2->exclhi) {
    n1->exclhi = 1;
  }
  if (n2->excllo) {
    n1->excllo = 1;
  }
  for (i=0; i < 2; i++) {
    if (n2->delay_up[i] >= 0) {
      n1->delay_up[i] = n2->delay_up[i];
    }
    if (n2->delay_dn[i] >= 0) {
      n1->delay_dn[i] = n2->delay_dn[i];
    }
  }
}

static int _count_dots (char *s)
{
  int n = 0;
  while (*s) {
    if (*s == '.') n++;
    s++;
  }
  return n;
}

/*
  Return 1 if a has a shorter name than b
*/
static int _shorter_name (PrsNode *a, PrsNode *b)
{
  int dots1, dots2;

  dots1 = _count_dots (a->b->key);
  dots2 = _count_dots (b->b->key);
  if (dots1 < dots2) return 1;
  if (dots1 > dots2) return 0;
  dots1 = strlen (a->b->key);
  dots2 = strlen (b->b->key);
  if (dots1 < dots2) return 1;
  if (dots1 > dots2) return 0;
  dots1 = strcmp (a->b->key, b->b->key);
  if (dots1 < 0) return 1;
  if (dots1 > 0) return 0;

  /* XXX: should never get here */
  return 0;
}

/*
 *
 *  Implement "connect" operation
 *
 */
static void do_connection (Prs *p, PrsNode *n1, PrsNode *n2)
{
  RawPrsNode *n, *r;

  if (n1 == n2) return;

  if (_shorter_name (n2, n1)) {
    PrsNode *tmp;
    tmp = n1;
    n1 = n2;
    n2 = tmp;
  }

  /* make the connection:
       make the alias traversal *through* the buckets, so changing
       a PrsNode -> RawPrsNode is a constant time operation
  */
  merge_nodes (n1, n2);  	/* n1 := n1 UNION n2 */

  Assert ((PrsNode *)n2->b->v == n2, "Oh my god. You're dead.");

  /* walk the n2 ring, moving all "alias" pointers to n1 */
  n = (RawPrsNode *) n2;
  do {
    Assert (((n->alias == NULL && n == (RawPrsNode *)n2) || n->alias == n2),
	    "Invariant failed");
    n->alias = n1;
    if (n->alias_ring == (RawPrsNode *)n2) 
      break;
    n = n->alias_ring;
  } while (1);

  /* n->alias_ring should be changed to a new rawnode, only if n != n2 */

  if (n2->alias_ring == n2) {
    /* ring has one item, namely n2, so we don't need it any more */
    n = newrawnode ();
  }
  else {
    /* create a new raw node, replace n2 in the ring! */
    n->alias_ring = newrawnode ();
    n = n->alias_ring;
    n->alias_ring = (RawPrsNode *) n2->alias_ring;
  }

  n->alias = n1;
  n2->b->v = (void *) n;
  n->b = n2->b;

  /* I have one alias ring with n and another one with n1;
     fuse them 
  */
  
  r = (RawPrsNode *) n1->alias_ring;
  n1->alias_ring = (PrsNode *) n->alias_ring;
  n->alias_ring = r;

  delnode (n2);
}


static void parse_connection (Prs *p, LEX_T *l)
{
  char *s;
  PrsNode *n1, *n2;

  s = lex_mustbe_id (p,l);
  n1 = lookup (s, p->H);

  s = lex_mustbe_id (p,l);
  n2 = lookup (s, p->H);

  do_connection (p, n1, n2);
}

static void process_names_conns (Prs *p)
{
  IDX_TYPE idx1, idx2;
  PrsNode *n1, *n2;
  char *s;

  idx1 = 1;
  while (idx1 <= p->N->unique_names) {
    idx2 = names_parent (p->N, idx1);
    if (idx2 != 0) {
      n1 = lookup (names_num2name (p->N, idx1), p->H);
      n2 = lookup (names_num2name (p->N, idx2), p->H);
      do_connection (p, n1, n2);
    }
    idx1++;
  }
}
  


static PrsExpr *expr (Prs *p, LEX_T *l);


/*
 *
 *  Make links: e --> n's hash list element
 *              e <-- n's out[] array
 */
static void mk_out_link (PrsExpr *e, PrsNode *n)
{
  e->l = (PrsExpr *)n->b;
  /*e->r = NULL;*/
  if (n->max == n->sz) {
    if (n->max == 0) {
      n->max = 4;
      MALLOC (n->out, PrsExpr *, n->max);
    }
    else {
      n->max *= 2;
      REALLOC (n->out, PrsExpr *, n->max);
    }
  }
  n->out[n->sz++] = e;
}


/*
 * Parse excl directives
 */
static void parse_excl (LEX_T *l, int ishi, Prs *p)
{
  PrsNode *n;
  PrsExclRing *r, *s;

  lex_mustbe (l, TOK_LPAR);

  r = NULL;

  do {
    n = lookup (lex_mustbe_id (p,l), p->H);
    NEW (s, PrsExclRing);
    s->n = (PrsNode *)n->b;
    if (!r) {
      r = s;
    }
    else {
      s->next = r->next;
    }
    r->next = s;
    if (ishi)
      n->exclhi = 1;
    else
      n->excllo = 1;
  } while (lex_have (l, TOK_COMMA));

  if (ishi) {
    A_NEW (p->exhi, PrsExclRing *);
    A_NEXT (p->exhi) = r;
    A_INC (p->exhi);
  }
  else {
    A_NEW (p->exlo, PrsExclRing *);
    A_NEXT (p->exlo) = r;
    A_INC (p->exlo);
  }
  lex_mustbe (l, TOK_RPAR);
}

/*
 *
 * Welcome to yet-another-expression parser...
 *
 */
static PrsExpr *unary (Prs *p, LEX_T *l)
{
  PrsExpr *e;
  PrsNode *n;
  char *s;

  if (lex_is_id (p,l)) {
    s = lex_mustbe_id (p,l);
    n = lookup (s, p->H);
    e = newexpr();
    e->type = PRS_VAR;
    e->val = PRS_VAL_X;
    e->valx = 1;
    mk_out_link (e,n);
    Assert (NODE(e) == n, "Invariant: NODE(e) == n violated!");
  }
  else if (lex_have (l, TOK_NOT)) {
    e = newexpr();
    e->type = PRS_NOT;
    e->val = PRS_VAL_X;
    e->valx = 1;
    e->l = unary (p,l);
    e->l->u = e;
  }
  else if (lex_have (l, TOK_LPAR)) {
    e = expr (p,l);
    lex_mustbe (l, TOK_RPAR);
  }
  else {
    fatal_error ("unary: Expected `(', `id', `~'\n\t%s", lex_errstring 
		 (l));
  }
  return e;
}

static PrsExpr *term (Prs *p,LEX_T *l)
{
  PrsExpr *e, *ret;

  ret = unary (p,l);
  if (ret && lex_have (l, TOK_AND)) {
    e = ret;
    ret = newexpr ();
    e->u = ret;
    ret->l = e;
    ret->type = PRS_AND;
    ret->val = 0;
    ret->valx = 1;
    do {
      e->r = unary (p,l);
      e = e->r;
      if (e) {
	e->u = ret;
	ret->valx++;
      }
    } while (e && lex_have (l, TOK_AND));
  }
  return ret;
}

static PrsExpr *expr (Prs *p, LEX_T *l)
{
  PrsExpr *e, *ret;

  ret = term (p,l);
  if (ret && lex_have (l, TOK_OR)) {
    e = ret;
    ret = newexpr ();
    e->u = ret;
    ret->l = e;
    ret->type = PRS_OR;
    ret->val = 0;
    ret->valx = 1;
    do {
      e->r = term (p,l);
      e = e->r;
      if (e) {
	e->u = ret;
	ret->valx++;
      }
    } while (e && lex_have (l, TOK_OR));
  }
  return ret;
}


/*
 *
 *  Take two expressions e1 and e2, and construct expression
 *  (e1 | e2). If the root of either is |, it shares the | node.
 *
 */
static PrsExpr *or_merge (PrsExpr *e1, PrsExpr *e2)
{
  PrsExpr *e;

  if (e2->type == PRS_OR) {
    e = e1;
    e1 = e2;
    e2 = e;
  }
  if (e1->type == PRS_OR) {
    e2->r = e1->l;
    e2->u = e1;
    e1->valx ++;
    e1->l = e2;			// bug fixed.
    e = e1;
  }
  else {
    e = newexpr();
    e->val = 0;
    e->valx = 2;
    e->type = PRS_OR;
    e->l = e1;
    e->l->r = e2;
    e1->u = e;
    e2->u = e;
  }
  return e;
}

/*
 *  add a disjunct to a pull-up
 */
static void merge_or_up (PrsNode *n, PrsExpr *e, int weak)
{
  if (!n->up[weak]) {
    n->up[weak] = newexpr();
    n->up[weak]->r = e;
    e->u = n->up[weak];
    n->up[weak]->type = (weak ? PRS_NODE_WEAK_UP : PRS_NODE_UP);
    n->up[weak]->val = PRS_VAL_X;
    n->up[weak]->valx = 1;
    n->up[weak]->l = (PrsExpr *)n->b;
    return;
  }
  Assert (n->up[weak]->r, 
	  "merge_or_up: no ->r field for expression tree root");

  e = or_merge (n->up[weak]->r, e);
  n->up[weak]->r = e;
  e->u = n->up[weak];
}

/*
 *  add a disjunct to a pull-down
 */
static void merge_or_dn (PrsNode *n, PrsExpr *e, int weak)
{
  if (!n->dn[weak]) {
    n->dn[weak] = newexpr();
    n->dn[weak]->r = e;
    e->u = n->dn[weak];
    n->dn[weak]->type = (weak ? PRS_NODE_WEAK_DN : PRS_NODE_DN);
    n->dn[weak]->val = PRS_VAL_X;
    n->dn[weak]->valx = 1;
    n->dn[weak]->l = (PrsExpr *)n->b;
    return;
  }

  Assert (n->dn[weak]->r, 
	  "merge_or_dn: no ->r field for expression tree root");

  e = or_merge (n->dn[weak]->r, e);
  n->dn[weak]->r = e;
  e->u = n->dn[weak];
}
  
static int lex_have_weak (Prs *p, LEX_T *l)
{
  if (!p->N) {
    return lex_have_keyw (l, "weak");
  }
  else {
    if (lex_sym (l) == l_err && l->token[0] == 0x2) {
      lex_getsym (l);
      return 1;
    }
    return 0;
  }
}

static int lex_have_after (Prs *p, LEX_T *l)
{
  if (!p->N) {
    return lex_have_keyw (l, "after");
  }
  else {
    if (lex_sym (l) == l_err && l->token[0] == 0x3) {
      lex_getsym (l);
      return 1;
    }
    return 0;
  }
}

static int lex_have_unstab (Prs *p, LEX_T *l)
{
  if (!p->N) {
    return lex_have_keyw (l, "unstab");
  }
  else {
    if (lex_sym (l) == l_err && l->token[0] == 0x1) {
      lex_getsym (l);
      return 1;
    }
    return 0;
  }
}


/*
 *
 *  Parse single production rule
 *
 */
static void parse_prs (Prs *p,LEX_T *l)
{
  PrsExpr *e, *ne;
  PrsNode *n;
  int v, delay, unstab;
  int weak = G_NORM;

  delay = 10;
  if (lex_have_weak (p,l)) {
    weak = G_WEAK;
    delay = 20;
  }
  if (lex_have_unstab (p,l)) {
    unstab = 1;
  }
  else {
    unstab = 0;
  }
  if (lex_have_after (p,l)) {
    lex_mustbe (l, l_integer);
    delay = lex_integer (l);
  }

  e = expr (p,l);
  if (!e) return;
  lex_mustbe (l, TOK_ARROW);
  n = lookup(lex_mustbe_id (p,l), p->H);
  if (unstab) {
    n->unstab = 1;
  }
  if (lex_have (l, TOK_UP)) {
    n->delay_up[weak] = delay;
    v = 1;
  }
  else if (lex_have (l, TOK_DN)) {
    n->delay_dn[weak] = delay;
    v = 0;
  }
  else {
    fatal_error ("Expected `+' or `-'\n\t%s", lex_errstring (l));
  }

  if (v) {
    if (n->up[weak]) {
      merge_or_up (n, e, weak);
      return;
    }
  }
  else {
    if (n->dn[weak]) {
      merge_or_dn (n, e, weak);
      return;
    }
  }
  ne = newexpr ();
  ne->r = e;
  e->u = ne;
  ne->type = v ? (weak ? PRS_NODE_WEAK_UP : PRS_NODE_UP) : (weak ? PRS_NODE_WEAK_DN : PRS_NODE_DN);
  ne->val = PRS_VAL_X;
  ne->valx = 1;
  ne->l = (PrsExpr *)n->b;
  if (v) {
    n->up[weak] = ne;
  }
  else {
    n->dn[weak] = ne;
  }
}


/*
 *
 *  Debugging
 *
 */
static void print_expr_tree (Prs *P,PrsExpr *e)
{
  PrsExpr *x;

  Assert (e, "print_expr_tree: NULL expression argument");

  switch (e->type) {
  case PRS_AND:
    printf ("(& :[#0=%d, #X=%d] ", e->val, e->valx);
    x = e->l;
    while (x) {
      Assert (x->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
      print_expr_tree (P,x);
      x = x->r;
      if (x) printf (" ");
    }
    printf (")");
    break;

  case PRS_OR:
    printf ("(| :[#1=%d, #X=%d] ", e->val, e->valx);
    x = e->l;
    while (x) {
      Assert (x->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
      print_expr_tree (P,x);
      x = x->r;
      if (x) printf (" ");
    }
    printf (")");
    break;

    break;
  case PRS_NOT:
    printf ("(~ ");
    Assert (e->l->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
    print_expr_tree (P,e->l);
    printf (")");
    break;
  case PRS_NODE_UP:
  case PRS_NODE_WEAK_UP:
    printf ("(UP-%c%s :[%s] ", prs_nodechar(e->val), 
	    e->type == PRS_NODE_WEAK_UP ? " weak" : "",
	    prs_nodename(P,NODE(e)));
    Assert (e->r->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
    print_expr_tree (P,e->r);
    printf (")");
    
    break;
  case PRS_NODE_DN:
  case PRS_NODE_WEAK_DN:
    printf ("(DN-%c%s :[%s] ", prs_nodechar(e->val), 
	    e->type == PRS_NODE_WEAK_DN ? " weak" : "",
	    prs_nodename(P,NODE(e)));
    Assert (e->r->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
    print_expr_tree (P,e->r);
    printf (")");
    break;
  case PRS_VAR:
    printf ("%s", prs_nodename (P,canonical_name (NODE(e))));
    break;
    break;
  default:
    fatal_error ("prop_up: unknown type %d\n", e->type);
    break;
  }
}

/*  prec:
 *        1 = ~
 *        2 = &
 *        3 = |
 *        4 = top-level
 */
static void print_rule_tree (Prs *P,PrsExpr *e, int prec, int vals)
{
  PrsExpr *x;

  Assert (e, "print_rule_tree: NULL expression argument");

  switch (e->type) {
  case PRS_AND:
    if (prec < 2 || vals) printf ("(");
    x = e->l;
    while (x) {
      Assert (x->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
      print_rule_tree (P, x, 2, vals);
      x = x->r;
      if (x) printf (" & ");
    }
    if (prec < 2 || vals) {
      printf (")");
      if (vals) {
	if (e->val > 0)
	  printf (":0");
	else if (e->valx > 0) {
	  printf (":X");
	}
	else {
	  printf (":1");
	}
      }
    }
    break;

  case PRS_OR:
    if (prec < 3 || vals) printf ("(");
    x = e->l;
    while (x) {
      Assert (x->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
      print_rule_tree (P, x, 3, vals);
      x = x->r;
      if (x) printf (" | ");
    }
    if (prec < 3 || vals) {
      printf (")");
      if (vals) {
	if (e->val > 0)
	  printf (":1");
	else if (e->valx > 0) {
	  printf (":X");
	}
	else {
	  printf (":0");
	}
      }
    }
    break;

    break;
  case PRS_NOT:
    if (vals) {
      printf ("(");
    }
    printf ("~");
    Assert (e->l->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
    print_rule_tree (P, e->l, 1, vals);
    if (vals) {
      printf ("):%c", prs_nodechar (e->val));
    }
    break;
  case PRS_NODE_UP:
  case PRS_NODE_WEAK_UP:
    Assert (e->r->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
    print_rule_tree (P, e->r, 4, vals);
    printf (" -> %s+%s", prs_nodename (P,canonical_name (NODE(e))),
	    e->type == PRS_NODE_WEAK_UP ? " (weak)" : "");
    break;
  case PRS_NODE_DN:
  case PRS_NODE_WEAK_DN:
    Assert (e->r->u == e, "print: Invariant violation: uplink(downlink(expr)) != expr");
    print_rule_tree (P, e->r, 4, vals);
    printf (" -> %s-%s", prs_nodename (P,canonical_name (NODE(e))),
	    e->type == PRS_NODE_WEAK_DN ? " (weak)" : "");
    break;
  case PRS_VAR:
    printf ("%s", prs_nodename (P,canonical_name (NODE(e))));
    if (vals) {
      printf (":%c", prs_nodechar (prs_nodeval (NODE(e))));
    }
    break;
  default:
    fatal_error ("prop_up: unknown type %d\n", e->type);
    break;
  }
}


/*
 *  Free storage
 */
void prs_free (Prs *p)
{
  /* XXX: fixme */
}

void prs_apply (Prs *p, void *cookie, void (*f)(PrsNode *, void *))
{
  int i; 
  hash_bucket_t *b;
  PrsNode *n;

  for (i=0; i < p->H->size; i++)
    for (b = p->H->head[i]; b; b = b->next) {
      n = (PrsNode *)b->v;
      if (n->alias) continue;
      (*f)(n,cookie);
    }
}

void prs_dump_node (Prs *P, PrsNode *n)
{
  if (n->up[G_NORM]) {
    print_expr_tree (P, n->up[G_NORM]);
    printf ("\n");
  }
  if (n->dn[G_NORM]) {
    print_expr_tree (P, n->dn[G_NORM]);
    printf ("\n");
  }
}

void prs_printrule (Prs *P, PrsNode *n, int vals)
{
  if (n->up[G_NORM]) {
    if (n->delay_up[G_NORM] != 10) {
      printf ("after %d ", n->delay_up[G_NORM]);
    }
    if (n->unstab) {
      printf ("unstab ");
    }
    print_rule_tree (P, n->up[G_NORM], 0, vals);
    printf ("\n");
  }
  if (n->up[G_WEAK]) {
    printf ("weak ");
    if (n->delay_up[G_WEAK] != 10) {
      printf ("after %d ", n->delay_up[G_WEAK]);
    }
    if (n->unstab) {
      printf ("unstab ");
    }
    print_rule_tree (P, n->up[G_WEAK], 0, vals);
    printf ("\n");
  }
  if (n->dn[G_NORM]) {
    if (n->delay_dn[G_NORM] != 10) {
      printf ("after %d ", n->delay_dn[G_NORM]);
    }
    if (n->unstab) {
      printf ("unstab ");
    }
    print_rule_tree (P, n->dn[G_NORM], 0, vals);
    printf ("\n");
  }
  if (n->dn[G_WEAK]) {
    printf ("weak ");
    if (n->delay_dn[G_WEAK] != 10) {
      printf ("after %d ", n->delay_dn[G_WEAK]);
    }
    if (n->unstab) {
      printf ("unstab ");
    }
    print_rule_tree (P, n->dn[G_WEAK], 0, vals);
    printf ("\n");
  }
}

void prs_print_expr (Prs *P, PrsExpr *n)
{
  print_rule_tree (P, n, 0, 0);
  printf ("\n");
}

const char *prs_nodename (Prs *P, PrsNode *n)
{
  if (!P->N) {
    return n->b->key;
  }
  else {
    /* hah! */
    return n->b->key;
  }
}

const char *prs_rawnodename (Prs *P, RawPrsNode *n)
{
  if (!P->N) {
    return n->b->key;
  }
  else {
    /* hah! */
    return n->b->key;
  }
}



static void dump_node (PrsNode *n, void *v)
{
  Prs *P = (Prs *)v;
  prs_dump_node (P, n);
}

void dump_prs (Prs *p)
{
  prs_apply (p, p, dump_node);
}

static void setX (PrsNode *n, void *cookie)
{
  if (n->queue && !n->exq) {
    n->queue->val = PRS_VAL_X;
    n->queue->cause = NULL;
  }
  else
    prs_set_node ((Prs*)cookie, n, PRS_VAL_X);
}

void prs_initialize (Prs *p)
{
  PrsNode *n;

  prs_apply (p, (void*)p, setX);

  while (n = prs_step (p)) {
    ;
  }
}

static Prs *extra_arg = NULL;
static void save_prs_event (FILE *fp, void *v)
{
  PrsEvent *ev = (PrsEvent *)v;

  fprintf (fp, "%s ", prs_nodename (extra_arg, ev->n));
  fprintf (fp, "%d %d %d %d %d %d %d %d %s\n",
	   ev->val, ev->weak, ev->force, ev->seu, ev->start_seu,
	   ev->stop_seu, ev->kill, ev->interf, ev->cause ? prs_nodename (extra_arg, ev->cause) : "-");
}

static void *restore_prs_event (FILE *fp)
{
  PrsEvent *ev;
  char buf[10240];
  int a0, a1, a2, a3, a4, a5, a6, a7;

  Assert (extra_arg, "Hmm");
  
  ev = rawnewevent (extra_arg);
  if (fscanf (fp, "%s", buf) != 1) Assert (0, "Checkpoint read error");
  ev->n = prs_node (extra_arg, buf);
  if (!ev->n) {
    fatal_error ("Unknown node name in restoring from checkpoint! (%s)\n", buf);
  }

  if (fscanf (fp, "%d%d%d%d%d%d%d%d%s", &a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7, buf) != 9) Assert (0, "Checkpoint read error");
  ev->val = a0;
  ev->weak = a1;
  ev->force = a2;
  ev->seu = a3;
  ev->start_seu = a4;
  ev->stop_seu = a5;
  ev->kill = a6;
  ev->interf = a7;

  if (strcmp (buf, "-") == 0) {
    ev->cause = NULL;
  }
  else {
    ev->cause = prs_node (extra_arg, buf);
    if (!ev->cause) {
      fatal_error ("Unknown node name in restoring from checkpoint! (%s)\n", buf);
    }
  }
  return ev;
}

static void save_prs_node (PrsNode *n, void *x)
{
  FILE *fp = (FILE *)x;

  Assert (n->exq == 0, "Hmm");
  
  fprintf (fp, "%s ", prs_nodename (extra_arg, n));
  fprintf (fp, "%d %d %d %lu\n", n->val, n->bp, n->seu, n->tc);
}

void prs_checkpoint (Prs *p, FILE *fp)
{
  int i, count;
  hash_bucket_t *b;

  /* sanity check: first entry is # of nodes in hash table */
  fprintf (fp, "%d\n", p->H->n);

  count = 0;
  for (i=0; i < p->H->size; i++) {
    for (b = p->H->head[i]; b; b = b->next) {
      if (((PrsNode *)b->v)->alias) continue;
      count++;
    }
  }
  fprintf (fp, "%d\n", count);

  /* event queue */
  extra_arg = p;
  heap_save (p->eventQueue, fp, save_prs_event);
  extra_arg = NULL;

  /* current time */
  fprintf (fp, "%llu\n", p->time);

  /* current energy */
  fprintf (fp, "%lu\n", p->energy);

  /* flags */
  fprintf (fp, "%u\n", p->flags);

  /* random timing range */
  fprintf (fp, "%u %u\n", p->min_delay, p->max_delay);

  /* random number seed */
  fprintf (fp, "%u\n", p->seed);

  /* save all the node state */
  extra_arg = p;
  prs_apply (p, fp, save_prs_node);
  extra_arg = NULL;
}

static void delete_event_heap (void *v)
{
  deleteevent (extra_arg, (PrsEvent *)v);
}


static void _update_expr (PrsExpr *e)
{
  PrsExpr *x;
  char * p;
  int value;   
  int myval, myvalx;
    
  if (!e) return;

  switch (e->type) {
  case PRS_AND:
    x = e->l;
    myval = 0;
    myvalx = 0;
    while (x) {
      _update_expr (x);
      value = expr_value (x);
      if (value == PRS_VAL_F) {
        myval++;
      } 
      else if (value == PRS_VAL_X) {
        myvalx++;
      }
      x = x->r;
    }
    e->val = myval;
    e->valx = myvalx;
    break;

  case PRS_OR:
    x = e->l;
    myval = 0; 
    myvalx = 0;
    while (x) {
      _update_expr (x);
      value = expr_value (x);
      if (value == PRS_VAL_T) {
        myval++;
      }
      else if (value == PRS_VAL_X) {
        myvalx++;
      }
      x = x->r;
    }
    e->val = myval;
    e->valx = myvalx;
    break;

  case PRS_NOT:
    _update_expr (e->l);
    if (expr_value (e->l) == PRS_VAL_T) {
      e->val = PRS_VAL_F;
    }
    if (expr_value (e->l) == PRS_VAL_F) {
      e->val = PRS_VAL_T;
    }
    if (expr_value (e->l) == PRS_VAL_X) {
      e->val = PRS_VAL_X;
    }
    break;
  case PRS_NODE_UP:
  case PRS_NODE_WEAK_UP:
    _update_expr (e->r);
    e->val = expr_value (e->r);
    break;
  case PRS_NODE_DN:
  case PRS_NODE_WEAK_DN:
    _update_expr (e->r);
    e->val = expr_value (e->r);
    break;
  case PRS_VAR:
    break;

  default:
    fatal_error ("_update_expr: unknown type %d\n", e->type);
  }
  return;
}

static void _update_guards (PrsNode *n, void *cookie)
{
  n->queue = NULL;
  _update_expr (n->up[0]);
  _update_expr (n->up[1]);
  _update_expr (n->dn[0]);
  _update_expr (n->dn[1]);
}

void prs_restore (Prs *p, FILE *fp)
{
  int i;
  int count;
  hash_bucket_t *b;
  char buf[10240];

  /* check node count */
  if (fscanf (fp, "%d", &i) != 1) Assert (0, "Checkpoint read error");
  if (i != p->H->n) {
    fatal_error ("Different number of nodes in the checkpoint!");
  }

  count = 0;
  for (i=0; i < p->H->size; i++) {
    for (b = p->H->head[i]; b; b = b->next) {
      if (((PrsNode *)b->v)->alias) continue;
      count++;
    }
  }
  if (fscanf (fp, "%d", &i) != 1) Assert (0, "Checkpoint read error");
  if (i != count) {
    fatal_error ("Different number of unique nodes in the checkpoint!");
  }

  /* restore event queue */
  extra_arg = p;
  heap_free (p->eventQueue, delete_event_heap);
  p->eventQueue = heap_restore (fp, restore_prs_event);
  extra_arg = NULL;
  
  /* restore current time */
  if (fscanf (fp, "%llu", &p->time) != 1) Assert (0, "Checkpoint read error");

  /* restore current energy */
  if (fscanf (fp, "%lu", &p->energy) != 1) Assert (0, "Checkpoint read error");

  /* flags */
  if (fscanf (fp, "%u", &p->flags) != 1) Assert (0, "Checkpoint read error");
  
  /* random timing range */
  if (fscanf (fp, "%u%u", &p->min_delay, &p->max_delay) != 2) Assert (0, "Checkpoint read error");

  /* random number seed */
  if (fscanf (fp, "%u", &p->seed) != 1) Assert (0, "Checkpoint read error");

  /* restore all nodes */
  for (i=0; i < count; i++) {
    PrsNode *n;
    int a0, a1, a2;

    if (fscanf (fp, "%s", buf) != 1) Assert (0, "Checkpoint read error");
    n = prs_node (p, buf);
    if (!n) {
      fatal_error ("Unknown node name in checkpoint! (%s)", buf);
    }
    if (fscanf (fp, "%d%d%d%lu", &a0, &a1, &a2, &n->tc) != 4) Assert (0, "Checkpoint read error");
    n->val = a0;
    n->bp = a1;
    n->seu = a2;
  }
  prs_apply (p, NULL, _update_guards);

  for (i=0; i < p->eventQueue->sz; i++) {
    PrsEvent *pe;
    pe = (PrsEvent *) p->eventQueue->value[i];
    pe->n->queue = pe;
  }
}
