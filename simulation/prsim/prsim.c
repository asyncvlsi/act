/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 1999-2019 Rajit Manohar
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
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "prs.h"
#include "misc.h"
#include "array.h"
#include "atrace.h"
#include "channel.h"
#include "mytime.h"

#include <histedit.h>

#if !defined(LIBEDIT_MAJOR) && !defined(H_SETSIZE)
#define OLD_LIBEDIT
#endif

#define PROMPT "(Prsim) "

static History *el_hist;
static EditLine *el_ptr;
static char *prompt_val;
static int profile_cmd = 0;


enum vector_type {
  V_BOOL,
  V_DUALRAIL,
  V_1OFN
};

typedef struct {
  int vtype;
  int num;			/* for 1ofN */
  A_DECL (PrsNode *, n)
  char *key;
} Vector;

static void fprint_vector (FILE *fp, Vector *v)
{
  unsigned long val;
  int state;
  int i, j;

  val = 0;
  state = 0;
  /* 0000 = print
     0001 = undefined
     0010 = state violation
     0100 = neutral
     1000 = valid
  */
  fprintf (fp, "%s [", v->key);
  if (v->vtype == V_BOOL) {
    for (i=0; i < A_LEN (v->n); i++) {
      val <<= 1;
      fprintf (fp, "%c", prs_nodechar (prs_nodeval (v->n[i])));
      if (prs_nodeval (v->n[i]) == PRS_VAL_X) {
	state |= 1;
      }
      else if (prs_nodeval (v->n[i]) == PRS_VAL_T) {
	val |= 1;
      }
    }
    if (state == 1) {
      fprintf (fp, "]  -undefined-");
    }
    else {
      fprintf (fp, "] %lu", val);
    }
  }
  else if (v->vtype == V_DUALRAIL) {
    for (i=0; i < A_LEN (v->n)/2; i++) {
      val <<= 1;
      if (prs_nodeval (v->n[2*i]) == PRS_VAL_X ||
	  prs_nodeval (v->n[2*i+1]) == PRS_VAL_X) {
	fprintf (fp, "X");
	state |= 1;
      }
      else if (prs_nodeval (v->n[2*i]) == PRS_VAL_T) {
	if (prs_nodeval (v->n[2*i+1]) != PRS_VAL_F) {
	  fprintf (fp, "*");
	  state |= 2;		/* state violation */
	}
	else {
	  fprintf (fp, "0");
	}
	/* false value */
	state |= 8;
      }
      else if (prs_nodeval (v->n[2*i+1]) == PRS_VAL_T) {
	if (prs_nodeval (v->n[2*i]) != PRS_VAL_F) {
	  fprintf (fp, "*");
	  state |= 2;   
	}
	else {
	  fprintf (fp, "1");
	}
	val |= 1;
	state |= 8;
      }
      else { /* both must be false */
	fprintf (fp, "n");
	state |= 4;
      }
    }
    if (state & 1) {
      fprintf (fp, "]  -undefined-");
    }
    else if (state & 2) {
      fprintf (fp, "] **dualrail code violation**");
    }
    else if ((state & 4) && (state & 8)) {
      fprintf (fp, "] intermediate state");
    }
    else if (state & 4) {
      fprintf (fp, "] neutral");
    }
    else if (state & 8) {
      fprintf (fp, "] %lu", val);
    }
    else {
      fprintf (fp, "???????");
    }
  }
  else if (v->vtype == V_1OFN) {
    for (i=0; i < A_LEN (v->n)/v->num; i++) {
      val = 0;
      state = 0;
      /* state: 0 = looking
	        1 = found value
		2 = violation
		3 = X
      */
      for (j=0; j < v->num; j++) {
	if (prs_nodeval (v->n[i*v->num+j]) == PRS_VAL_T) {
	  if (state == 0) {
	    val = j;
	    state = 1; 
	  }
	  else {
	    /* anything else is an error */
	    state = 2;
	  }
	}
	else if (prs_nodeval (v->n[i*v->num+j]) == PRS_VAL_F) {
	  /* no problem */
	}
	else if (prs_nodeval (v->n[i*v->num+j]) == PRS_VAL_X) {
	  if (state == 0) {
	    state = 3;
	  }
	  else if (state == 3) {
	    /* no problem */
	  }
	  else {
	    state = 2;
	  }
	}
      }
      if (state == 0) {
	fprintf (fp, "n");
      }
      else if (state == 1) {
	fprintf (fp, "%lu", val);
      }
      else if (state == 2) {
	fprintf (fp, "E");
      }
      else {
	fprintf (fp, "X");
      }
      if (v->num > 9) {
	fprintf (fp, " ");
      }
    }
    fprintf (fp, "]");
  }
}

static char *promptfn (EditLine *el)
{
  return prompt_val;
}

static void read_line_init (void)
{
  HistEvent ev;

#ifdef OLD_LIBEDIT
  el_ptr = el_init ("editline", stdin, stdout);
#else
  el_ptr = el_init ("editline", stdin, stdout, stderr);
#endif
  el_hist = history_init ();
  el_set (el_ptr, EL_PROMPT, promptfn);
  el_set (el_ptr, EL_EDITOR, "emacs");

#ifdef OLD_LIBEDIT
  history (el_hist, &ev, H_EVENT, 1000);
#else
  history (el_hist, &ev, H_SETSIZE, 1000);
#endif
  el_set (el_ptr, EL_HIST, history, el_hist);
  el_set (el_ptr, EL_BIND, "^P", "ed-prev-history", NULL);
  el_set (el_ptr, EL_BIND, "^N", "ed-next-history", NULL);
}

static char *read_line (char *prompt)
{
  int count;
  HistEvent ev;
  const char *s;
  char *t;

  prompt_val = prompt;

  count = 0;
  while (count == 0) {
   s = el_gets (el_ptr, &count);
   if (!s) break;
  }   
  if (s && *s) {
    	history (el_hist, &ev, H_ENTER, s);
  }
  if (s) {
    t = Strdup (s);
    if (count > 0 && t[count-1] == '\n') {
	t[count-1] = '\0';
    }
  }
  else {
    t = NULL;
  }
  return t;
}

/* --- end editline --- */


static float prs_nodeanalogval[] = { 1.0, 0.0, 0.5 };

static Prs *P;				/* global prs stuff */
void handle_user_input (FILE *fp);
static struct Channel C;


static struct Hashtable *vH = NULL;	/* vector hash */

struct watchlist {
  PrsNode *n;
  int bp:1;			/* 1 if also a breakpoint */
  Vector *v;
  struct watchlist *next;
};

static struct watchlist *wlist = NULL;
static int interrupted = 0;
static int exit_on_warn = 0;

static int pairwise_transition_counts = 0;

static int no_readline;

void signal_handler (int sig)
{
  interrupted = 1;
  if (P) { P->flags |= PRS_STOP_SIMULATION; }
}

static
struct watchlist *in_watchlist (PrsNode *n)
{
  struct watchlist *l;
  int i;

  for (l = wlist; l; l = l->next) {
    if (l->n == n) return l;
    if (l->v) {
      for (i=0; i < A_LEN (l->v->n); i++)
	if (l->v->n[i] == n) return l;
    }
  }
  return NULL;
}

static
void add_watchpoint (PrsNode *n)
{
  struct watchlist *l;

  if ((l = in_watchlist (n)))
    return;
  
  MALLOC (l, struct watchlist, 1);
  l->v = NULL;
  l->n = n;
  l->next = wlist;
  wlist = l;
  l->bp = n->bp;
  n->bp = 1;
}

static 
void re_implement_bp (void)
{
  struct watchlist *l;

  for (l = wlist; l; l = l->next) {
    l->n->bp = 1;
  }
}

static
void add_watchpoint_wrapper (PrsNode *n, void *v) 
{
  add_watchpoint(n);
}


static
void del_watchpoint (PrsNode *n)
{
  struct watchlist *l, *prev;

  prev = NULL;
  l = wlist;
  while (l && l->n != n) {
    prev = l;
    l = l->next;
  }
  if (l) {
    n->bp = l->bp;
    if (prev == NULL) {
      FREE (wlist);
      wlist = NULL;
    }
    else {
      prev->next = l->next;
      FREE (l);
    }
  }
}


#define GET_ARG(msg)  do { s = strtok (NULL, " \t"); if (!s) { printf ("%s", msg); return; } } while(0)

#define GET_ARGCOLON(msg)  do { s = strtok (NULL, " \t:"); if (!s) { printf ("%s",msg); return; } } while(0)

#define GET_OPTARG  do { s = strtok (NULL, " \t"); } while (0)

#define CHECK_TRAILING(msg) do { s = strtok (NULL, " \t"); if (s && s[0] != '#') { printf ("%s",msg); return; } } while (0)


static char *match_string;
static int match_len;
void check_nodeval (PrsNode *n, void *val)
{
  long v = (long)val;

  if (n->flag) return;

  n->flag = 1;

  if (n->val == v) {
    if (match_string) {
      if (match_string[0] == '^') {
	if (strncmp (prs_nodename (P,n), match_string+1, match_len-1) == 0) {
	  printf ("%s ", prs_nodename (P,n));
	}
      }
      else {
	if (strstr (prs_nodename (P,n), match_string)) {
	  printf ("%s ", prs_nodename (P,n));
	}
      }
    }
    else {
      printf ("%s ", prs_nodename (P,n));
    }
  }
}

static void clear_nodeflag (PrsNode *n, void *val)
{
  n->flag = 0;
}


/* 
 * vector name node list
 */
void process_vector (void)
{
  char *s;
  PrsNode *n;
  char *usage = "Usage: vector name [:type] node1 node2...\n";
  hash_bucket_t *b;
  Vector *v;
  int count;
  char *t;

  GET_ARG(usage);
  if (!vH) {
    vH = hash_new (32);
  }
  b = hash_lookup (vH, s);
  if (b) {
    printf ("Vector `%s' already exists\n", s);
    return;
  }
  b = hash_add (vH, s);
  NEW (v, Vector);
  v->vtype = V_BOOL;
  v->key = b->key;
  A_INIT (v->n);
  GET_OPTARG;
  if (!s) {
    printf ("%s", usage);
    hash_delete (vH, b->key);
    A_FREE (v->n);
    FREE (v);
    return;
  }
  if ((strcmp (s, ":dualrail") == 0)) {
    v->vtype = V_DUALRAIL;
    GET_OPTARG;
    if (!s) {
      printf ("%s", usage);
      hash_delete (vH, b->key);
      A_FREE (v->n);
      FREE (v);
      return;
    }
  }
  else if ((strcmp (s, ":1ofN") == 0)) {
    v->vtype = V_1OFN;
    GET_OPTARG;
    if (!s) {
      v->num = 0;
    }
    else {
      v->num = atoi (s);
    }
    if (v->num < 1) {
      printf ("%s", usage);
      hash_delete (vH, b->key);
      A_FREE (v->n);
      FREE (v);
      return;
    }
    GET_OPTARG;
    if (!s) {
      printf ("%s", usage);
      hash_delete (vH, b->key);
      A_FREE (v->n);
      FREE (v);
      return;
    }
  }
  count = 0;
  do {
    /* add s to the vector */
    count++;
    n = prs_node (P, s);
    if (!n) {
      printf ("Node `%s' not found\n", s);
      goto err;
    }
    if (CHINFO(n)->inVector) {
      printf ("Node `%s' already in a vector\n", s);
      goto err;
    }
    A_NEW (v->n, PrsNode *);
    A_NEXT (v->n) = n;
    CHINFO(n)->inVector = v;
    A_INC (v->n);
    GET_OPTARG;
  } while (s);
  if (v->vtype == V_DUALRAIL && ((count & 1) == 1)) {
    printf ("Dualrail vector must have an even number of signals\n");
    goto err;
  }
  if (v->vtype == V_1OFN && ((count % v->num) != 0)) {
    printf ("1ofN (N=%d) vector must have a number of signals that are a multiple of N\n", v->num);
    goto err;
  }
  b->v = (void *)v;
  return;
 err:
  for (count = 0; count < A_LEN (v->n); count++) {
    CHINFO(v->n[count])->inVector = NULL;
  }
  A_FREE (v->n);
  hash_delete (vH, b->key);
  FREE (v);
  return;
}

static atrace *tracing = NULL;
static Time_t tracing_stop_time = 0;
static Time_t tracing_start_time = 0;
static float prs_timescale = 100e-12;  // 100ps default

static
void clear_trace_wrap (PrsNode *n, void *v) {
  n->bp = 0;
}

static
void add_trace_wrap (PrsNode *n, void *v) {
  name_t *m, *o;
  PrsNode *nn;

  n->bp = 1;
  
  m = atrace_create_node (tracing, prs_nodename (P,n));
  atrace_mk_digital (m);
  
  SPACE(n) = m;

#if 0
  for (nn = n->alias; nn != n; nn = nn->alias) {
    o = atrace_create_node (tracing, prs_nodename (nn));
    atrace_mk_digital (o);
    atrace_alias (tracing, m, o);
  }
#endif

}

void process_trace (void)
{
  char *s;
  char *usage = "Usage: trace <file> <time>\n";
  char *f;
  float tm;
  
  if (tracing) {
    printf ("Still tracing! Skipped\n");
    return;
  }

  GET_ARG (usage);
  
  f = Strdup (s);

  GET_ARG (usage);

  if (sscanf (s, "%f", &tm) != 1) {
    printf ("%s", usage);
    return;
  }
  /* transition is 20ps */
  printf ("Creating trace file, %.2fns in duration (~ %d transition delays)\n",
	  tm, (int)(tm*1e-9/prs_timescale));

  if ((int)(tm*1e-9/prs_timescale) <= 0) {
    printf ("Invalid duration!\n");
    return;
  }
  tracing = atrace_create (f, ATRACE_DELTA_CAUSE, tm*1e-9, 
			   prs_timescale/10.0);

  if (!tracing) {
    printf ("Could not create trace file!\n");
    return;
  }

  prs_apply (P, NULL, add_trace_wrap);
  tracing_start_time = P->time;
  tracing_stop_time = P->time + (int)(tm*1e-9/prs_timescale);
}

/*
 * vset vector value
 */
void process_vset (void)
{
  char *s;
  PrsNode *n;
  char *usage = "Usage: vset name value\n";
  hash_bucket_t *b;
  Vector *v;
  unsigned long val;
  int i;

  GET_ARG(usage);
  if (!vH) {
    printf ("No vectors defined\n");
    return;
  }
  b = hash_lookup (vH, s);
  if (!b) {
    printf ("Vector `%s' not found\n", s);
    return;
  }
  v = (Vector *)b->v;
  GET_ARG(usage);
  if (v->vtype == V_DUALRAIL || v->vtype == V_1OFN) {
    if (strcmp (s, "neutral") == 0) {
      /* this has meaning! */
      CHECK_TRAILING(usage);
      for (i=0; i < A_LEN (v->n); i++) 
	prs_set_node (P, v->n[i], PRS_VAL_F);
      return;
    }
  }
  sscanf (s, "%lu", &val);

  if (v->vtype == V_BOOL || v->vtype == V_DUALRAIL) {
    CHECK_TRAILING(usage);
  }
  if (v->vtype == V_BOOL) {
    for (i=A_LEN(v->n)-1; i >= 0; i--) {
      prs_set_node (P, v->n[i], (val & 1) ? PRS_VAL_T : PRS_VAL_F);
      val = val >> 1;
    }
  }
  else if (v->vtype == V_DUALRAIL) {
    for (i=A_LEN(v->n)/2-1; i >= 0; i--) {
      if (val & 1) {
	/* true */
	prs_set_node (P, v->n[2*i], PRS_VAL_F);
	prs_set_node (P, v->n[2*i+1], PRS_VAL_T);
      }
      else {
	prs_set_node (P, v->n[2*i], PRS_VAL_T);
	prs_set_node (P, v->n[2*i+1], PRS_VAL_F);
      }
      val = val >> 1;
    }
  }
  else if (v->vtype == V_1OFN) {
    int j;
    for (i=0; i < A_LEN (v->n)/v->num; i++) {
      if (val < 0 || val >= v->num) {
	printf ("Value %lu exceeds bounds for 1ofN (N=%d)", val, v->num);
	return;
      }
      for (j=0; j < v->num; j++) {
	if (j != val) {
	  prs_set_node (P, v->n[i*v->num+j], PRS_VAL_F);
	}
	else {
	  prs_set_node (P, v->n[i*v->num+j], PRS_VAL_T);
	}
      }
      if (i+1 == A_LEN(v->n)/v->num) {
	CHECK_TRAILING(usage);
      }
      else {
	GET_ARG(usage);
	sscanf (s, "%lu", &val);
      }
    }
  }
}

void process_vget (void)
{
  char *s;
  PrsNode *n;
  char *usage = "Usage: vget name\n";
  hash_bucket_t *b;
  Vector *v;

  GET_ARG(usage);
  if (!vH) {
    printf ("No vectors defined\n");
    return;
  }
  b = hash_lookup (vH, s);
  if (!b) {
    printf ("Vector `%s' not found\n", s);
    return;
  }
  v = (Vector *)b->v;
  CHECK_TRAILING(usage);
  fprint_vector (stdout, v);
  printf ("\n");
}

void process_vclear (void)
{
  char *s;
  PrsNode *n;
  char *usage = "Usage: vclear name\n";
  hash_bucket_t *b;
  Vector *v;
  int i;

  GET_ARG(usage);
  if (!vH) {
    printf ("No vectors defined\n");
    return;
  }
  b = hash_lookup (vH, s);
  if (!b) {
    printf ("Vector `%s' not found\n", s);
    return;
  }
  v = (Vector *)b->v;
  CHECK_TRAILING(usage);
  for (i=0; i < A_LEN (v->n); i++) {
    CHINFO(v->n[i])->inVector = NULL;
  }
  A_FREE (v->n);
  hash_delete (vH, b->key);
  FREE (v);
  return;
}

void process_vwatch (void)
{
  char *s;
  PrsNode *n;
  char *usage = "Usage: vwatch name\n";
  hash_bucket_t *b;
  Vector *v;
  int i;

  GET_ARG(usage);
  if (!vH) {
    printf ("No vectors defined\n");
    return;
  }
  b = hash_lookup (vH, s);
  if (!b) {
    printf ("Vector `%s' not found\n", s);
    return;
  }
  v = (Vector *)b->v;
  CHECK_TRAILING(usage);
  for (i=0; i < A_LEN (v->n); i++) {
    add_watchpoint (v->n[i]);
  }
  return;
}

void process_vunwatch (void)
{
  char *s;
  PrsNode *n;
  char *usage = "Usage: vunwatch name\n";
  hash_bucket_t *b;
  Vector *v;
  int i;

  GET_ARG(usage);
  if (!vH) {
    printf ("No vectors defined\n");
    return;
  }
  b = hash_lookup (vH, s);
  if (!b) {
    printf ("Vector `%s' not found\n", s);
    return;
  }
  v = (Vector *)b->v;
  CHECK_TRAILING(usage);
  for (i=0; i < A_LEN (v->n); i++) {
    del_watchpoint (v->n[i]);
  }
  return;
}

void process_watchall (void)
{
  char *s;
  char *usage = "Usage: watchall\n";
  CHECK_TRAILING(usage);

  prs_apply (P, NULL, add_watchpoint_wrapper);

}


/*
 * status <value>
 */
void process_status (void)
{
  char *s;
  char *t;
  PrsNode *n;
  char *usage = "Usage: status T|1|F|0|X|U [[^]str]\n";
  int v;
  int type;
  
  GET_ARG(usage);
  if (s[0] == 'T' || s[0] == '1')
    v = PRS_VAL_T;
  else if (s[0] == 'F' || s[0] == '0')
    v = PRS_VAL_F;
  else if (s[0] == 'X' || s[0] == 'U')
    v = PRS_VAL_X;
  else {
    printf ("%s", usage);
    return;
  }
  if (s[1]) {
    printf ("%s", usage);
    return;
  }
  GET_OPTARG;
  match_string = s;
  if (s) {
    match_len = strlen (s);
  }
  prs_apply (P, (void*)(long)v, check_nodeval);
  prs_apply (P, (void*)NULL, clear_nodeflag);
  printf ("\n");
}
 
/*
 *   set n v
 */
void process_set (void)
{
  char *s;
  PrsNode *n;
  int val;
  char *usage = "Usage: set <var> <value>\n";

  GET_ARGCOLON(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  GET_ARG(usage);
  if (strcmp (s, "0") == 0)
    val = PRS_VAL_F;
  else if (strcmp (s, "1") == 0)
    val = PRS_VAL_T;
  else if (strcmp (s, "X") == 0)
    val = PRS_VAL_X;
  else {
    printf ("Value must be `0', `1', or `X'\n");
    return;
  }
  CHECK_TRAILING(usage);
  prs_set_node (P, n, val);
}


/*
 *   set n v
 */
void process_seu (void)
{
  char *s;
  PrsNode *n;
  int val;
  int start, duration;
  char *usage = "Usage: set <var> <value> <start-delay> <dur>\n";

  GET_ARGCOLON(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  GET_ARG(usage);
  if (strcmp (s, "0") == 0)
    val = PRS_VAL_F;
  else if (strcmp (s, "1") == 0)
    val = PRS_VAL_T;
  else if (strcmp (s, "X") == 0)
    val = PRS_VAL_X;
  else {
    printf ("Value must be `0', `1', or `X'\n");
    return;
  }
  GET_ARG (usage);
  start = atoi (s);
  GET_ARG (usage);
  duration = atoi (s);
  CHECK_TRAILING(usage);
  prs_set_seu (P, n, val, P->time + start, duration);
}

/*
 *   alias n
 */
void process_alias (void)
{
  char *s;
  PrsNode *n;
  RawPrsNode *r;
  int val;
  char *usage = "Usage: alias <var>\n";

  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  CHECK_TRAILING(usage);
  r = (RawPrsNode *)n;
  printf ("Aliases:");
  do {
    printf (" %s", r->b->key);
    r = r->alias_ring;
  } while (r != (RawPrsNode *)n);
  printf ("\n");
}

/*
 *   set_principal n
 */
void process_set_principal (void)
{
  char *s;
  PrsNode *n;
  RawPrsNode *r;
  int val;
  char *usage = "Usage: set_principal <var>\n";

  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  /* make this the primary name for the node */

  if (strcmp (n->b->key, s) != 0) {
    RawPrsNode *r;
    hash_bucket_t *b;
    /* 
       All the hash table entries have been made to point to this
       PrsNode. So we need to switch this bucket to point to the
       bucket for the node s 
    */
    r = (RawPrsNode *)n;
    do {
      if (strcmp (r->b->key, s) == 0) {
	break;
      }
      r = r->alias_ring;
    } while (r != (RawPrsNode *)n);

    if (r == (RawPrsNode *)n) {
      printf ("Warning: this should not have happened!\n");
      return;
    }
    /* swap bucket pointers! */
    
    b = r->b;
    r->b = n->b;
    n->b = b;
  }
  CHECK_TRAILING(usage);
}


/*
 *   get n
 */
void process_get (void)
{
  char *s;
  PrsNode *n;
  int val;
  char *usage = "Usage: get <var>\n";

  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  printf ("%s: %c\n", s, prs_nodechar (prs_nodeval (n)));
  CHECK_TRAILING(usage);
}

/*
 *   uget n
 */
void process_uget (void)
{
  char *s;
  PrsNode *n;
  int val;
  char *usage = "Usage: uget <var>\n";

  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  printf ("%s: %c\n", prs_nodename (P,n), prs_nodechar (prs_nodeval (n)));
  CHECK_TRAILING(usage);
}

/**
	Asserts the value of a certain node.
	Added by Fang (2005-03-19).  
 */
void
process_assert(void) {
  static const char* usage = "Usage: assert <var> <value>\n";
  char *s;
  char *node_name;
  PrsNode *n;
  int val, expect;
  GET_ARG(usage);
  node_name = s;
  n = prs_node (P, node_name);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  GET_ARG(usage);
  if (strcmp (s, "0") == 0)
    expect = PRS_VAL_F;
  else if (strcmp (s, "1") == 0)
    expect = PRS_VAL_T;
  else if (strcmp (s, "X") == 0)
    expect = PRS_VAL_X;
  else {
    printf ("Value must be `0', `1', or `X'\n");
    return;
  }
  val = prs_nodeval(n);
  if (val != expect) {
	printf("WRONG ASSERT:\t\"%s\" has value %c and not %c.\n",
		node_name, prs_nodechar(val), prs_nodechar(expect));
	// how does error handling work in this???
	// abort(), exit(), throw?
  }
  CHECK_TRAILING(usage);
}

/*
 *   fanin n
 */
void process_fanin (void)
{
  char *s;
  PrsNode *n;
  int val;
  char *usage = "Usage: fanin <var>\n";

  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  CHECK_TRAILING(usage);
  prs_printrule (P,n,0);
}

void process_fanin2 (void)
{
  char *s;
  PrsNode *n;
  int val;
  char *usage = "Usage: fanin-get <var>\n";

  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  CHECK_TRAILING(usage);
  /* prs_dump_node (P,n);*/
  prs_printrule (P,n,1);
}

/*
 *   fanout n
 */
void process_fanout (void)
{
  char *s;
  PrsNode *n;
  int i, num;
  char *usage = "Usage: fanout <var>\n";
  PrsExpr **l;

  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  CHECK_TRAILING(usage);

  num = prs_num_fanout (n);
  if (num == 0) return;
  MALLOC (l, PrsExpr *, num);
  prs_fanout_rule (P, n, l);
  for (i=0; i < num; i++)
    prs_print_expr (P,l[i]);
  FREE (l);
}

static void stop_trace (void)
{
  if (tracing) {
    atrace_close (tracing);
    tracing = NULL;
  }
}

static void check_trace_stop (void)
{
  if (P->time >= tracing_stop_time) {
    atrace_close (tracing);
    prs_apply (P, NULL, clear_trace_wrap);
    re_implement_bp ();
    tracing = NULL;
  }
}


static void add_transition (PrsNode *n, PrsNode *m)
{
  if (m) {
    atrace_signal_change_cause (tracing,
				(name_t *)SPACE(n),
				(P->time - tracing_start_time)*prs_timescale,
				prs_nodeanalogval[prs_nodeval (n)],
				(name_t *)SPACE(m));
  }
  else {
    atrace_signal_change (tracing,
			  (name_t *)SPACE(n),
			  (P->time - tracing_start_time)*prs_timescale,
			  prs_nodeanalogval[prs_nodeval (n)]);
  }
}

/*
 *  cycle
 */
void process_cycle (void)
{
  PrsNode *n;
  PrsNode *m;
  char *s;
  char *usage = "Usage: cycle [signal]\n";
  int flag;
  int seu;
  PrsNode *stop;

  GET_OPTARG;
  if (s == NULL) {
    stop = NULL;
  }
  else {
    stop = prs_node (P, s);
  }
  CHECK_TRAILING(usage);

#if 0
  interrupted = 0;
#endif
  while (!interrupted) {
    n = prs_cycle_cause_stop (P, &m, &seu, stop);

    if (!n) return;

    flag = 0;
    if (tracing) { flag = 1; check_trace_stop (); }
    if (tracing) add_transition (n, m);

    if (n->bp) {
      if (in_watchlist (n)) {
	printf ("\t%10llu %s : %c", P->time, prs_nodename (P,n),
		prs_nodechar(prs_nodeval(n)));
	if (m) {
	  printf ("  [by %s:=%c%s]", prs_nodename (P,m), 
		  prs_nodechar (prs_nodeval (m)),
		  seu ? " *seu*" : "");
	}
        if (!flag && CHINFO(n)->inVector) {
	  printf (" vec ");
	  fprint_vector (stdout, (Vector *)CHINFO(n)->inVector);
        }
	printf ("\n");
      }
      // This is a channel's enable
      if (CHINFO(n)->hasChans) {
	channel_enableSwitched (P, &C, n);
      } else if (n == C.reset) {
	channel_resetSwitched (P, &C);
      }
      if (!in_watchlist(n) && !CHINFO(n)->hasChans && !(n==C.reset) && !tracing) {
	printf ("\t*** break: `%s' became %c",
		prs_nodename (P,n),
		prs_nodechar(prs_nodeval(n)));
	if (m) {
	  printf ("  [by %s:=%c%s]", prs_nodename (P,m), 
		  prs_nodechar (prs_nodeval (m)),
		  seu ? " *seu*" : "");
	}
	printf ("\n");
	return;
      }
    } 
    if (interrupted) { 
      printf ("\t*** interrupted cycle\n"); 
    }
    else {
      if (P->flags & PRS_STOPPED_ON_WARNING) {
	break;
      }
    }
    if (n == stop) return;
  }
}


/*
 *  step [n]
 */
void process_step (void)
{
  PrsNode *n;
  PrsNode *m;
  char *s;
  char *usage = "Usage: step [n]\n";
  unsigned long i;
  unsigned long long tm;
  int seu;

  GET_OPTARG;
  if (s == NULL) 
    i = 1;
  else
    i = atoi (s);
  CHECK_TRAILING(usage);

#if 0
  interrupted = 0;
#endif
  tm = P->time;
  while (!interrupted && i && (n = prs_step_cause (P, &m, &seu))) {
    // Check whether simulated time advanced?
    if (tm != P->time) {
      i--;
      tm = P->time;
    }
    if (tracing) check_trace_stop ();
    if (tracing) add_transition (n, m);

    if (n->bp) {
      if (in_watchlist (n)) {
	printf ("\t%10llu %s : %c", 
		tm,
		prs_nodename (P,n),
		prs_nodechar(prs_nodeval(n)));
	if (m) {
	  printf ("  [by %s:=%c%s]", prs_nodename (P,m), 
		  prs_nodechar (prs_nodeval (m)),
		  seu ? " *seu*" : "");
	}
        if (CHINFO(n)->inVector) {
	  printf (" vec ");
	  fprint_vector (stdout, (Vector *)CHINFO(n)->inVector);
        }
	printf ("\n");
      }
      // This is a channel's enable
      if (CHINFO(n)->hasChans) {
	channel_enableSwitched (P, &C, n);
      } else if (n == C.reset) {
	channel_resetSwitched (P, &C);
      }
      if (!in_watchlist(n) && !CHINFO(n)->hasChans && !tracing) {
	printf ("\t*** break, %lu steps left: `%s' became %c",
		i,
		prs_nodename (P,n),
		prs_nodechar(prs_nodeval(n)));
	if (m) {
	  printf ("  [by %s:=%c%s]", prs_nodename (P,m), 
		  prs_nodechar (prs_nodeval (m)),
		  seu ? " *seu*" : "");
	}
	printf ("\n");
	return;
      }
    }
    if (interrupted) {
      printf ("\t*** interrupted step\n");
    }
    else {
      if (P->flags & PRS_STOPPED_ON_WARNING) {
	break;
      }
    }
  }
}

/*
 *  advance [n time units]
 */
void process_advance (void)
{
  PrsNode *n;
  PrsNode *m;
  char *s;
  char *usage = "Usage: advance [n]\n";
  unsigned long i;
  unsigned long long tm;
  unsigned long long end_tm;
  int seu;

  GET_OPTARG;
  if (s == NULL) 
    i = 1;
  else
    i = atoi (s);
  CHECK_TRAILING(usage);

#if 0
  interrupted = 0;
#endif
  tm = P->time;
  end_tm = tm + i;

  while (!interrupted && (heap_peek_minkey (P->eventQueue) < end_tm) && (n = prs_step_cause (P, &m, &seu))) {
    // Check whether simulated time advanced?
    if (tracing) check_trace_stop ();
    if (tracing) add_transition (n, m);

    tm = P->time;

    if (n->bp) {
      if (in_watchlist (n)) {
	printf ("\t%10llu %s : %c", 
		tm,
		prs_nodename (P,n),
		prs_nodechar(prs_nodeval(n)));
	if (m) {
	  printf ("  [by %s:=%c%s]", prs_nodename (P,m), 
		  prs_nodechar (prs_nodeval (m)),
		  seu ? " *seu*" : "");
	}
        if (CHINFO(n)->inVector) {
	  printf (" vec ");
	  fprint_vector (stdout, (Vector *)CHINFO(n)->inVector);
        }
	printf ("\n");
      }
      // This is a channel's enable
      if (CHINFO(n)->hasChans) {
	channel_enableSwitched (P, &C, n);
      } else if (n == C.reset) {
	channel_resetSwitched (P,  &C);
      }
      if (!in_watchlist(n) && !CHINFO(n)->hasChans && !tracing) {
	printf ("\t*** break, %lu steps left: `%s' became %c",
		i,
		prs_nodename (P,n),
		prs_nodechar(prs_nodeval(n)));
	if (m) {
	  printf ("  [by %s:=%c%s]", prs_nodename (P,m), 
		  prs_nodechar (prs_nodeval (m)),
		  seu ? " *seu*" : "");
	}
	printf ("\n");
	return;
      }
    }
    if (interrupted) {
      printf ("\t*** interrupted advance\n");
    }
    else {
      if (P->flags & PRS_STOPPED_ON_WARNING) {
	break;
      }
    }
  }
}

void process_watch (void)
{
  PrsNode *n;
  char *s;
  char *usage = "Usage: watch node\n";
  
  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  if (n->bp) {
    printf ("Node `%s' already in a breakpoint/watchpoint\n", s);
    return;
  }
  CHECK_TRAILING(usage);
  add_watchpoint (n);
}

void process_break (void)
{
  PrsNode *n;
  char *s;
  char *usage = "Usage: breakpt node\n";
  GET_ARG(usage);
  
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  if (in_watchlist (n)) {
    printf ("Node `%s' already in watch list\n", s);
    return;
  }
  CHECK_TRAILING(usage);
  n->bp = 1;
}
  

void process_unwatch (void)
{
  PrsNode *n;
  char *s;
  char *usage = "Usage: unwatch node\n";
  
  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  CHECK_TRAILING(usage);
  del_watchpoint (n);
}


void process_mode (void)
{
  char *s;
  char *usage = "Usage: mode reset|run|unstab|nounstab\n";
  int v;

  GET_ARG(usage);
  if (strcmp (s, "run") == 0)
    v = 0;
  else if (strcmp (s, "reset") == 0)
    v = 1;
  else if (strcmp (s, "unstab") == 0)
    v = 2;
  else if (strcmp (s, "nounstab") == 0)
    v = 3;
  else {
    printf ("%s", usage);
    return;
  }
  CHECK_TRAILING(usage);

  if (v == 0)
    P->flags &= ~PRS_NO_WEAK_INTERFERENCE;
  else if (v == 1)
    P->flags |= PRS_NO_WEAK_INTERFERENCE;
  else if (v == 2)
    P->flags |= PRS_UNSTAB;
  else
    P->flags &= ~PRS_UNSTAB;
}

void process_timescale (void)
{
  char *s;
  char *usage = "Usage: timescale <picoseconds>\n";
  float f;
  int v;

  GET_ARG(usage);

  sscanf (s, "%f", &f);

  CHECK_TRAILING(usage);

  prs_timescale = f * 1e-12;
  
  printf ("Set timescale to %f picoseconds.\n", f);
}




static void dump_tc (PrsNode *n, void *v)
{
  FILE *fp = (FILE*) v;

  fprintf (fp, "%s %lu\n", prs_nodename (P,n), n->tc);

  if (pairwise_transition_counts) {
    struct tracing_info *t;
    int i;

    t = n->tracing;
    
#define DUMPTRACE(idx,prefix)							\
    for (i=0; i < t->sz[idx]; i++) {					\
      fprintf (fp, "  " prefix " %s %lu\n", prs_nodename (P, t->in[idx][i]), t->count[idx][i]); \
    }

    DUMPTRACE(1,"[up]");
    DUMPTRACE(0,"[dn]");
  }
}

static void _init_tracing (PrsNode *n, void *cookie)
{
  NEW (n->tracing, struct tracing_info);
  n->tracing->sz[0] = 0;
  n->tracing->sz[1] = 0;
  n->tracing->max[0] = 0;
  n->tracing->max[1] = 0;
}

void process_pairtc (void)
{
  char *s;
  char *usage = "Usage: pairtc\n";

  if (pairwise_transition_counts) {
    printf ("Already set.\n");
    return;
  }
  CHECK_TRAILING(usage);
  pairwise_transition_counts = 1;
  prs_apply (P, NULL, _init_tracing);
  P->flags |= PRS_TRACE_PAIRS;
}

void process_dumptc (void)
{
  char *s;
  char *usage = "Usage: dumptc file\n";
  int v;
  FILE *fp;
  char *t;

  GET_ARG(usage);
  t = Strdup (s);
  CHECK_TRAILING(usage);

  if (!(fp = fopen (t, "w"))) {
    fprintf (stderr, "Error: could not open file `%s' for dump; dump aborted\n", t);
    FREE (t);
    return;
  }
  FREE (t);
  prs_apply (P, (void *)fp, dump_tc);
  fclose (fp);
}



struct file_stack {
  char *name;
  struct file_stack *next;
};

static struct file_stack *flist = NULL;

static
int in_stack (char *s)
{
  struct file_stack *fstk;

  for (fstk = flist; fstk; fstk = fstk->next)
    if (strcmp (fstk->name, s) == 0)
      return 1;
  return 0;
}

static
void push_file (char *s)
{
  struct file_stack *fstk;

  NEW (fstk, struct file_stack);
  fstk->next = flist;
  flist = fstk;
  fstk->name = Strdup (s);
  return;
}

static
void pop_file (void)
{
  struct file_stack *fstk;

  Assert (flist, "Yow");
  fstk = flist;
  flist = flist->next;
  free (fstk->name);
  free (fstk);
  return;
}

void process_checkpoint (void)
{
  FILE *fp;
  char *s;
  char *fname;
  char *usage = "Usage: chk-save filename\n";
  int count;

  GET_ARG(usage);
  fname = Strdup (s);
  CHECK_TRAILING(usage);

  fp = fopen (fname, "w");
  if (!fp) {
    fprintf (fp, "Could not open file `%s' for writing\n", fname);
    return;
  }
  prs_checkpoint (P, fp);
  channel_checkpoint (&C, fp);
  fclose (fp);
}

void process_restore (void)
{
  FILE *fp;
  char *s;
  char *fname;
  char *usage = "Usage: chk-restore filename\n";
  int count;

  GET_ARG(usage);
  fname = Strdup (s);
  CHECK_TRAILING(usage);

  fp = fopen (fname, "r");
  if (!fp) {
    fprintf (fp, "Could not open file `%s' for reading\n", fname);
    return;
  }
  prs_restore (P, fp);
  channel_restore (&C, fp);
  fclose (fp);
}


void process_source (void)
{
  FILE *fp;
  char *s;
  char *fname;
  char *usage = "Usage: source filename [<repeat-count>]\n";
  int count;

  GET_ARG(usage);
  fname = Strdup (s);
  
  GET_OPTARG;
  if (s == NULL) {
    count = 1;
  }
  else {
    count = atoi (s);
  }
#if 0
  interrupted = 0;
#endif
  while (!interrupted && !(P->flags & PRS_STOPPED_ON_WARNING) && (count-- > 0)) {
    if (in_stack (fname)) {
      printf ("Recursive source command for file `%s'\n", fname);
      return;
    }
    push_file (fname);
    CHECK_TRAILING(usage);
    Assert (flist, "yow");
    fp = fopen (flist->name, "r");
    if (!fp) {
      printf ("Could not open file `%s' for reading\n", flist->name);
      return;
    }
    handle_user_input (fp);
    fclose (fp);
    pop_file ();
  }
  if (interrupted) {
    printf ("\t*** interrupted source %s\n", fname);
  }
  FREE (fname);
}

void process_initialize (void)
{
  char *s;
  char *usage = "Usage: initialize\n";

  CHECK_TRAILING(usage);
  prs_initialize (P);
  prs_reset_time (P);
}

// Function called to create a channel
// Usage is channel <channeltype> <size> <channelname>
void process_channel (void) 
{
  char *s, *sChannelType, *sName;
  ChannelType *ctype;
  char *usage = "Usage: channel channeltype size channelname\n";
  int size;

  GET_ARG(usage);
  sChannelType = s;

  GET_ARG(usage);
  size = atoi(s);

  GET_ARG(usage);
  sName = s;

  CHECK_TRAILING(usage);

  //printf("Trying to create channel %s (%d) %s \n", sChannelType, size, sName);

  // Need to make sure that this is a legal channel type
  create_channel(P, &C, sChannelType, size, sName);
}

void process_injectfile (int isLoop) {
  char *s;
  char *usage = "Usage: [loop-]injectfile channelname file\n";
  char *sChanName, *sFileName;

  GET_ARG(usage);
  sChanName = s;

  GET_ARG(usage);
  sFileName = s;

  CHECK_TRAILING(usage);

  channel_injectfile(P, &C, sChanName, sFileName, isLoop);
}

void process_expectfile (int isLoop) 
{
  char *s;
  char *usage = "Usage: [loop-]expectfile channelname file\n";
  char *sChanName, *sFileName;

  GET_ARG(usage);
  sChanName = s;

  GET_ARG(usage);
  sFileName = s;

  CHECK_TRAILING(usage);

  channel_expectfile(P, &C, sChanName, sFileName, isLoop);
}

void process_dumpfile (void)
{
  char *s;
  char *usage = "Usage: dumpfile channelname file\n";
  char *sChanName, *sFileName;

  GET_ARG(usage);
  sChanName = s;

  GET_ARG(usage);
  sFileName = s;

  CHECK_TRAILING(usage);

  channel_dumpfile(P, &C, sChanName, sFileName);
}

void process_pending (void)
{
  char *s;
  char *usage = "Usage: pending [signal]\n";

  if (!P->eventQueue || (heap_peek_min (P->eventQueue) == NULL)) {
    printf ("No pending events!\n");
    return;
  }

  GET_OPTARG;
  if (s == NULL) {
    CHECK_TRAILING(usage);

    PrsEvent *ev = (PrsEvent *)heap_peek_min (P->eventQueue);
    Time_t t = (heap_key_t)heap_peek_minkey (P->eventQueue);

    printf ("Next event: ");
    printf ("  %s := %c  @ ", prs_nodename (P, ev->n), prs_nodechar (ev->val));
    printf ("%llu\n", t);
  }
  else {
    PrsNode *n = prs_node (P, s);
    if (!n) {
      printf ("Node `%s' not found\n", s);
      return;
    }
    if (n->queue) {
      PrsEvent *e = n->queue;
      printf ("Pending event: ");
      printf (" %s := %c\n", prs_nodename (P, e->n), prs_nodechar (e->val));
      printf (" Flags: weak=%d; ", e->weak);
      printf ("force=%d; ", e->force);
      printf ("kill=%d; ", e->kill);
      printf ("interf=%d\n", e->interf);
    }
    else {
      printf ("No pending event.\n");
    }
  }
}




/*
 *************************************************************************
 
 END OF COMMANDS

 *************************************************************************
 */

/*
 *  read in an input line
 */
static char *read_input_line (FILE *fp, char *prompt, char *buf, int len)
{
  char *s;

  if (fp == stdin && isatty (0)) {
    if (no_readline) {
      printf ("%s", prompt);
      fflush (stdout);
      if (fgets (buf, len, fp)) {
	len = strlen (buf);
	if (buf[len-1] == '\n') {
	  buf[len-1] = '\0';
	}
	return buf;
      }
      else
	return NULL;
    }
    else {
      s = read_line (prompt);
      return s;
    }
  }
  else {
    if (fgets (buf, len, fp)) {
      len = strlen (buf);
      if (buf[len-1] == '\n') {
	buf[len-1] = '\0';
      }
      return buf;
    }
    else
      return NULL;
  }
}

static void process_help (void);


static void process_injectfile0 (void)
{
  process_injectfile (0);
}

static void process_injectfile1 (void)
{
  process_injectfile (1);
}

static void process_expectfile0 (void)
{
  process_expectfile (0);
}

static void process_expectfile1 (void)
{
  process_expectfile (1);
}

static void process_after (void)
{
  char *s;
  char *usage = "Usage: after v min_u max_u min_d max_d\n";
  PrsNode *n;
  unsigned int min_u, max_u, min_d, max_d;

  GET_ARG(usage);
  n = prs_node (P, s);
  if (!n) {
    printf ("Node `%s' not found\n", s);
    return;
  }
  if (n->up[1] || n->dn[1]) {
    printf ("Node `%s' has weak rules; cannot use this feature\n", s);
  }
  GET_ARG(usage);
  min_u = atoi(s);
  GET_ARG(usage);
  max_u = atoi(s);
  if (min_u > max_u) {
    printf ("min up delay should be <= max up delay\n");
    return;
  }
  if (min_u < 1) {
    printf ("min up delay cannot be less than 1\n");
    return;
  }
  GET_ARG(usage);
  min_d = atoi(s);
  GET_ARG(usage);
  max_d = atoi(s);
  if (min_d > max_d) {
    printf ("min dn delay should be <= max dn delay\n");
    return;
  }
  if (min_d < 1) {
    printf ("min dn delay cannot be less than 1\n");
    return;
  }
  n->after_range = 1;
  n->delay_up[0] = min_u;
  n->delay_up[1] = max_u;
  n->delay_dn[0] = min_d;
  n->delay_dn[1] = max_d;
}
  


static void process_random (void)
{
  char *s;
  char *usage = "Usage: random [min max]\n";
  int v;
  unsigned int min_d, max_d;

  GET_OPTARG;
  if (s == NULL) {
    P->flags |= PRS_RANDOM_TIMING;
    P->flags &= ~PRS_RANDOM_TIMING_RANGE;
  }
  else {
    min_d = atoi (s);
    GET_ARG (usage);
    max_d = atoi (s);
    if (min_d > max_d) {
      printf ("min delay should be <= max delay\n");
      return;
    }
    P->flags |= PRS_RANDOM_TIMING;
    P->flags |= PRS_RANDOM_TIMING_RANGE;
    P->min_delay = min_d;
    P->max_delay = max_d;
  }
}

static void process_random_seed (void)
{
  char *s;
  char *usage = "Usage: random_seed seed\n";
  int v;

  GET_ARG (usage);
  v = atoi (s);
  P->seed = v;
}

static void process_random_excl (void)
{
  char *s;
  char *usage = "Usage: random_excl on|off\n";
  int v;

  GET_ARG (usage);
  if (strcmp (s, "on") == 0) {
    P->flags |= PRS_RANDOM_EXCL;
  }
  else if (strcmp (s, "off") == 0) {
    P->flags &= ~PRS_RANDOM_EXCL;
  }
  else {
    printf ("%s", usage);
    return;
  }
}


static void process_norandom (void)
{
  P->flags &= ~PRS_RANDOM_TIMING;
  P->flags &= ~PRS_RANDOM_TIMING_RANGE;
}

static void process_break_on_warn (void)
{
  char *s;
  char *usage = "Usage: break-on-warn\n";

  CHECK_TRAILING(usage);
  
  if (P->flags & PRS_STOP_ON_WARNING) {
    P->flags &= ~PRS_STOP_ON_WARNING;
  }
  else {
    P->flags |= PRS_STOP_ON_WARNING;
  }
  printf ("Stop on instability/interference: %s\n", (P->flags & PRS_STOP_ON_WARNING ? "On" : "Off"));
  exit_on_warn = 0;
}

static void process_exit_on_warn (void)
{
  char *s;
  char *usage = "Usage: exit-on-warn\n";

  CHECK_TRAILING(usage);
  
  if (P->flags & PRS_STOP_ON_WARNING) {
    P->flags &= ~PRS_STOP_ON_WARNING;
  }
  else {
    P->flags |= PRS_STOP_ON_WARNING;
  }
  printf ("Exit on instability/interference: %s\n", (P->flags & PRS_STOP_ON_WARNING ? "On" : "Off"));
  exit_on_warn = 1;
}

/* --- Standard command processing --- */
struct Command {
    char *name;
    char *help;
    void (*f) (void);
} Cmds[] = {

  { NULL, "General", NULL },

  { "help", "help - display this message", process_help },
  { "exit", "exit - terminate", NULL },
  { "initialize", "initialize - initialize the simulation", process_initialize },
  { "source", "source <file> - read in a script file", process_source },
  { "mode", "mode reset|run - set running mode", process_mode },
  { "random", "random [min max] - random timings", process_random },
  { "random_seed", "random_seed seed - set random number seed", process_random_seed },
  { "norandom", "norandom - deterministic timings", process_norandom },
  { "random_excl", "random_excl on|off - turn on/off random exclhi/lo firings", process_random_excl },
  { "after", "after <n> <minu> <maxu> <mind> <maxd> - node set to random times within range", process_after },
  { "dumptc", "dumptc <file> - dump transition counts for nodes to <file>", process_dumptc },
  { "pairtc", "pairtc - turns on <input/output> pair transition counts", process_pairtc },

  { NULL, "Running Simulation", NULL },

  { "step", "step <n> - run for <n> simulation steps", process_step },
  { "advance", "advance <n> - run for <n> units of simulation time", process_advance },
  { "cycle", "cycle [<n>] - run simulation, and stop if <n> changes", process_cycle },
  { "watch", "watch <n> - add watchpoint for <n>", process_watch },
  { "unwatch", "unwatch <n> - delete watchpoint for <n>", process_unwatch },
  { "watchall", "watchall - watch all nodes", process_watchall },
  { "breakpt", "breakpt <n> - set a breakpoint on <n>", process_break },
  { "trace", "trace <file> <time> - Create atrace file for <time> duration", process_trace },
  { "timescale", "timescale <t> - set time scale to <t> picoseconds for tracing", process_timescale },
  { "break-on-warn", "break-on-warn - stops/doesn't stop simulation on instability/inteference", process_break_on_warn },
  { "exit-on-warn", "exit-on-warn - like break-on-warn, but exits prsim", process_exit_on_warn },
  { "chk-save", "chk-save <file> - save a simulation checkpoint to the specified file", process_checkpoint },
  { "chk-restore", "chk-restore <file> - restore simulation from a checkpoint", process_restore },
  { "pending", "pending - dump pending events", process_pending },

  { NULL, "Setting/Viewing Nodes and Rules", NULL },

  { "set", "set <n> 0|1|X - set <n> to specified value", process_set },
  { "get", "get <n> - get value of node <n>", process_get },
  { "assert", "assert <n> <v> - assert that <n> is <v>", process_assert },
  { "uget", "uget <n> - get value of node <n> but report its canonical name", process_uget },
  { "alias", "alias <n> - list aliases for <n>", process_alias },
  { "set_principal", "set_principal <n> - make <n> the primary name in the alias listing for node <n>", process_set_principal },
  { "status", "status 0|1|X [[^]str] - list all nodes with specified value, optional prefix/string match", process_status },
  { "fanin", "fanin <n> - list fanin for <n>", process_fanin },
  { "fanin-get", "fanin-get <n> - list fanin with values for <n>", process_fanin2 },
  { "fanout", "fanout <n> - list fanout for <n>", process_fanout },
  { "seu", "seu <n> 0|1|X <start-delay> <dur> - Delayed SEU event on node lasting for <dur> units", process_seu },


  { NULL, "Vector and Channel Commands", NULL },

  { "vector", "vector <name> [:dualrail|:1ofN <N>] <n1> <n2> ... - define vector", process_vector },
  { "vset", "vset <name> <val> - set vector", process_vset },
  { "vget", "vget <name> - get value of vector", process_vget },
  { "vclear", "vclear <name> - deletes a vector", process_vclear },
  { "vwatch", "vwatch <name> - watch a vector", process_vwatch },
  { "vunwatch", "vunwatch <name> - stop watching a vector", process_vunwatch },
  { "channel", "channel <type> <size> <name> - create channel\n\tExample: channel e1ofN 2 A declares an e1of2 channel called A", process_channel },
  { "injectfile", "injectfile <name> <file> - inject values in <file> into channel <name>", process_injectfile0 },
  { "loop-injectfile", "loop-injectfile <name> <file> - loop injectfile command", process_injectfile1 },
  { "expectfile", "expectfile <name> <file> - check channel outputs against values in file", process_expectfile0 },
  { "loop-expectfile", "loop-expectfile <name> <file> - loop expectfile command", process_expectfile1 },
  { "dumpfile", "dumpfile <name> <file> - dump channel output to file", process_dumpfile }
};

static void process_help (void)
{
  char *s;
  char *usage = "Usage: help\n";
  int i;
  
  CHECK_TRAILING(usage);
  for (i=0; i < sizeof (Cmds)/sizeof (Cmds[0]); i++) {
    if (!Cmds[i].name) {
      printf ("\n== %s ==\n", Cmds[i].help);
    }
    else {
      printf ("   %s\n", Cmds[i].help);
    }
  }
}

void do_command (char *s)
{
  char *t;
  int i;

  t = strtok (s, " \t");
  if (!t) return;
  if (strcmp (t, "#") == 0) return;
    
  if (strcmp (t, "quit") == 0 || strcmp (t, "exit") == 0) {
    t = strtok (NULL, " \t");
    if (t && (strcmp (t, "#") != 0)) {
      printf ("Trailing garbage ignored, terminating anyway\n");
    }
    stop_trace ();
    exit (0);
  }
  for (i=0; i < sizeof (Cmds)/sizeof (Cmds[0]); i++) {
    if (!Cmds[i].name) continue;
    if (strcmp (Cmds[i].name, s) == 0) {
      (*Cmds[i].f)();
      return;
    }
  }
  printf ("Unknown command name `%s'\n", s);
}


/*
 *  Main parsing loop
 */
void handle_user_input (FILE *fp)
{
  char buf[10240];
  char *s, *t;
  double tm;

  while ((s = read_input_line (fp, PROMPT, buf, 10240))) {
    if (interrupted) {
      if (flist) {
	return;
      }
      else {
	interrupted = 0;
	P->flags &= ~(PRS_STOP_SIMULATION|PRS_STOPPED_ON_WARNING);
      }
    }
    else {
      if (P->flags & PRS_STOPPED_ON_WARNING) {
	if (exit_on_warn) {
	  printf ("*** Exited on warning.\n");
	  stop_trace ();
	  exit (2);
	}
	if (flist) {
	  return;
	}
	else {
	  interrupted = 0;
	  P->flags &= ~(PRS_STOP_SIMULATION|PRS_STOPPED_ON_WARNING);
	}
      }
    }
    if (!*s) goto done;
    if (profile_cmd) {
      tm = cputime_msec ();
    }
    do_command (s);
    if (profile_cmd) {
      tm = cputime_msec ();
      printf ("time = %g ms\n", tm);
    }
  done:
    if (s != buf) {
      free (s);
    }
    fflush (stdout);
    fflush (stderr);
  }
}


static void _init_chaninfo (PrsNode *n, void *cookie)
{
  n->chinfo = prs_node_extra_init ();
}

static void prsim_init_channels (void)
{
  C.hChannels = hash_new (128);
  C.reset = NULL;
  prs_apply (P, NULL, _init_chaninfo);
}

int main (int argc, char **argv)
{
  FILE *fp;
  extern int opterr, optind;
  extern char *optarg;
  char *names;
  int ch;

  signal (SIGINT, signal_handler);

  read_line_init ();

  names = NULL;
  opterr = 0;
  no_readline = 0;
  profile_cmd = 0;
  while ((ch = getopt (argc, argv, "prn:")) != -1) {
    switch (ch) {
    case 'r':
      no_readline = 1;
      break;
    case 'n':
      names = Strdup (optarg);
      break;
    case 'p':
      profile_cmd = 1;
      break;
    default:
      fatal_error ("getopt() is broken");
      break;
    }
  }
  if (optind == argc-1) {
    if (names) {
      P = prs_packfopen (argv[optind],names);
    }
    else {
      P = prs_fopen (argv[optind]);
    }
    fp = stdin;
  }
  else if (optind == argc) {
    if (names) {
      P = prs_packfile (stdin, names);
    }
    else {
      P = prs_file (stdin);
    }
    fp = fopen ("/dev/tty", "r");
  }
  else {
    fprintf (stderr, "Usage: %s [options] [prsfile]\n", argv[0]);
    fprintf (stderr, "  -r : no readline\n");
    fprintf (stderr, "  -n names: packed file with names file\n");
    fprintf (stderr, "  -p : profile each prsim command\n");
    exit (1);
  }

  /* add channel info and tracing to all the nodes*/
  prsim_init_channels ();

  handle_user_input (fp);
  fclose (fp);

  stop_trace ();

  return 0;
}
