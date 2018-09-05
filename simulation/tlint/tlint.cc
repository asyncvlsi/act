/*************************************************************************
 *
 *  Copyright (c) 2012 Rajit Manohar
 *  All Rights Reserved
 *
 *  $Id: tlint.c,v 1.11 2010/07/12 18:27:05 rajit Exp $
 *
 **************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#include <sys/types.h>
#include <regex.h>

#include "misc.h"
#include "config.h"
#include "atrace.h"
#include "array.h"

#include <act/act.h>
#include "../prsim/prs.h"
#include "heap.h"

int verbose;			/* debugging info */

double Vdd;			/* supply voltage */

double V_high;			/* voltage limits for 0 | X | 1 boundaries */
double V_low;

double slewrate_fast_threshold; /* slewrate limits */
double slewrate_slow_threshold;

int max_print;			/* max # of errors to display for slew rates */

int show_frequency;		/* 1 (default) if it should display
				   frequency */

char *filter_results;		/* regexp for filtering results */

double hysteresis;		/* hysteresis in V */

double reset_sync_time;         /* time to verify reset state */

double skip_initial_time;	/* no errors reported before this time
				   in seconds */

double delta_t;			/* computed */


char *prs_file_name;		/* prs file name */

char *output_file_name;		/* output file name */

enum err_type {
  CHG_SHARING = 0,
  FAST_TRANSITION = 1,
  SLOW_TRANSITION = 2,
  INCOMPLETE_TRANSITION = 3
};

#define NUM_ERR_TYPES   4

struct err_log {
  int num;
  double tm;
  double val;			/* value */
  
  struct err_log *next;
};

static int num_errs[NUM_ERR_TYPES];
static int smaller_val_better[NUM_ERR_TYPES];

static const char *err_names[] = {
	"charge sharing",
	"fast transitions",
	"slow transitions",
        "incomplete transitions"
};

static struct err_log *signal_errs[NUM_ERR_TYPES];

static double min_cycle_time;
static int min_cycle_node;

static Act *act_global = NULL;

int is_init_prs_firing (PrsNode *n, int val);

static void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-v] [-m maxprint] [-V Vdd] [-C configfile] [-f] [-F str] [-h val] [-t tm] tracefile\n", s);
  fprintf (stderr, " -v     : verbosity; repeat for higher levels\n");
  fprintf (stderr, " -V val : specifies Vdd voltage; V_high is set to 0.1 less\n");
  fprintf (stderr, " -C conf: specifies configuration file name\n");
  fprintf (stderr, " -m max : max # of messages per error type\n");
  fprintf (stderr, " -f     : do not display frequency\n");
  fprintf (stderr, " -F str : ignore names that match regular expression <str>\n");
  fprintf (stderr, "-h val : set hysteresis for analog to digital conversion\n");
  fprintf (stderr, "-t time : only report errors after <time> seconds\n");
  fprintf (stderr, "-o <digital> : digital trace file output\n");
  fprintf (stderr, "-p <prs> : validate against prs file (requires -o)\n");
  fprintf (stderr, "-r time : synchronize initial state with prs at <time> seconds\n");
  exit (1);
}


static double round_double (double d)
{
  return ((int)(d*100.0+0.5))/100.0;
}

static int has_trailing_extension (const char *s, const char *ext)
{
  int l_s, l_ext;
  
  l_s = strlen (s);
  l_ext = strlen (ext);

  if (l_s < l_ext + 2) return 0;

  if (strcmp (s + l_s - l_ext, ext) == 0) {
    if (s[l_s-l_ext-1] == '.')
      return 1;
  }
  return 0;
}

static void display_options (void)
{
  if (verbose < 2) return;

  printf ("OPTIONS:\n");
  printf ("\tverbose = %d\n", verbose);
  printf ("\tVdd = %g\n", Vdd);
  printf ("\tV_high = %g\n", V_high);
  printf ("\tV_low = %g\n", V_low);
  printf ("\tslewrate_fast_threshold = %g\n", slewrate_fast_threshold);
  printf ("\tslewrate_slow_threshold = %g\n", slewrate_slow_threshold);
  printf ("\tmax_print = %d\n", max_print);
  printf ("\tdelta_t = %g\n", delta_t);
  printf ("\tshow_frequency = %d\n", show_frequency);
  printf ("\tfilter_results = `%s'\n", filter_results);
  printf ("\tskip_initial_time = `%g'\n", skip_initial_time);
  printf ("\treset_sync_time = `%g'\n", reset_sync_time);
  printf ("----\n");
}


static void process_node (atrace *a, int num, float *storage, 
			  int *storage1, int *storage2, int Nsteps);


static void emit_err_log (atrace *a, int t, struct err_log *x)
{
  switch (t) {
  case CHG_SHARING:
    printf ("  `%s', %g ns: charge-sharing bump of %.3g V\n", 
	    ATRACE_GET_NAME(a,x->num), x->tm*1e9, x->val);
    break;

  case FAST_TRANSITION:
    printf ("  `%s', %g ns: fast transition, slew rate %.3g V/ns\n",
	    ATRACE_GET_NAME (a, x->num), x->tm*1e9, x->val);
    break;
    
  case SLOW_TRANSITION:
    printf ("  `%s', %g ns: slow transition, slew rate %.3g V/ns\n",
	    ATRACE_GET_NAME (a,x->num), x->tm*1e9, x->val);
    break;

  case INCOMPLETE_TRANSITION:
    printf ("  `%s', %g ns: incomplete transition, swing %.3g V\n", 
	    ATRACE_GET_NAME (a,x->num), x->tm*1e9, x->val);
    break;

  default:
    fatal_error ("Unknown error log!");
    break;
  }
}

static void reverse_list (int t)
{
  struct err_log *prev, *cur, *tmp;

  prev = NULL;
  cur = signal_errs[t];

  while (cur) {
	tmp = cur->next;
	cur->next = prev;
	prev = cur;
	cur = tmp;
  } 
  signal_errs[t] = prev;
}



static void compute_errs (atrace *a, int Nnodes, int Nsteps);

/*------------------------------------------------------------------------
 *  main program
 *------------------------------------------------------------------------
 */
int main (int argc, char **argv)
{
  char *config_name;
  extern int opterr, optind;
  extern char *optarg;
  extern int optreset;
  int ch;
  char **eargv;
  char buf[10240];
  int ts, Nnodes, Nsteps, fmt;
  int i;
  atrace *a;
  int tmp_optind;
  char *conf_mangle_string;

  Act::Init (&argc, &argv);
  config_read ("prs2net.conf");
  config_read ("lint.conf");

  for (i=0; i < NUM_ERR_TYPES; i++) {
    signal_errs[i] = NULL;
    num_errs[i] = 0;
  }
  smaller_val_better[CHG_SHARING] = 1;
  smaller_val_better[FAST_TRANSITION] = 1;
  smaller_val_better[SLOW_TRANSITION] = 0;
  smaller_val_better[INCOMPLETE_TRANSITION] = 1;

  config_name = NULL;

  MALLOC (eargv, char *, argc+1);
  for (ch = 0; ch < argc+1; ch++)
    eargv[ch] = argv[ch];

  /* two pass getopt... that way any config file specified will be
     overridden by command line arguments */

#define GETOPT_STRING "fvm:C:V:F:h:t:p:o:r:"

  while ((ch = getopt (argc, eargv, GETOPT_STRING)) != -1) {
    switch (ch) {
    case 'C':
      if (config_name) {
	FREE (config_name);
      }
      config_name = Strdup (optarg);
      break;
    default:
      break;
    }
  }
  
  if (config_name) {
    if (has_trailing_extension (config_name, "conf") == 0) {
      sprintf (buf, "%s.conf", config_name);
    }
    else {
      strcpy (buf, config_name);
    }
    config_read (buf);
    FREE (config_name);
  }
  FREE (eargv);
  tmp_optind = optind;

  act_global = new Act();

  verbose = config_get_int ("lint.verbose");
  Vdd = config_get_real ("lint.Vdd");
  slewrate_slow_threshold = config_get_real ("lint.slewrate_slow_threshold");
  slewrate_fast_threshold = config_get_real ("lint.slewrate_fast_threshold");
  V_high = config_get_real ("lint.V_high");
  V_low = config_get_real ("lint.V_low");
  max_print = config_get_int ("lint.max_print");
  show_frequency = config_get_int ("lint.show_frequency");
  filter_results = config_get_string ("lint.filter_results");
  hysteresis = config_get_real ("lint.hysteresis");
  skip_initial_time = config_get_real ("lint.skip_initial_time");
  reset_sync_time = config_get_real ("lint.reset_sync_time");

  if (config_exists ("net.mangle_string")) {
    act_global->mangle (config_get_string ("net.mangle_string"));
  }

#ifdef __APPLE__
  optreset = 1;
#endif
  optind = 1;

  prs_file_name = NULL;
  output_file_name = NULL;

  while ((ch = getopt (argc, argv, GETOPT_STRING)) != -1) {
    switch (ch) {
    case 'p':
      prs_file_name = Strdup (optarg);
      break;
    case 'o':
      output_file_name = Strdup (optarg);
      break;
    case 't':
      sscanf (optarg, "%lg", &skip_initial_time);
      break;
    case 'r':
      sscanf (optarg, "%lg", &reset_sync_time);
      break;
    case 'h':
      sscanf (optarg, "%lg", &hysteresis);
      break;
    case 'f':
      show_frequency = 0;
      break;
    case 'm':
      max_print = atoi (optarg);
      break;
    case 'v':
      verbose++;
      break;
    case 'V':
      sscanf (optarg, "%lg", &Vdd);
      V_high = Vdd - 0.1;
      break;
    case 'F':
      filter_results = Strdup (optarg);
      break;
    case 'C':
      break;
    case '?':
      usage (argv[0]);
      break;
    default:
      fatal_error ("Unknown argument!");
      break;
    }
  }
  if (tmp_optind != argc-1) {
    usage (argv[0]);
  }

  /* open a trace file */
  a = atrace_open (argv[tmp_optind]);
  if (!a) {
    fatal_error ("Terminated.");
  }
  
  atrace_header (a, &ts, &Nnodes, &Nsteps, &fmt);

  if (verbose > 1) {
    time_t time_value = ts;
    printf ("Trace file `%s' opened\n", argv[tmp_optind]);
    printf ("\tFormat = %d\n", fmt);
    printf ("\t# nodes = %d\n", Nnodes);
    printf ("\t# steps = %d\n", Nsteps);
    printf ("\ttimestamp = %d\n", ts);
    printf ("\ttimestamp = %s", ctime (&time_value));
  }

  /* node 0 is always time */
  delta_t = a->dt;
  min_cycle_time = delta_t*Nsteps;
  min_cycle_node = -1;

  display_options ();

  compute_errs (a, Nnodes, Nsteps);
  
  /* print error logs */
  for (i=0; i < NUM_ERR_TYPES; i++) {
    reverse_list (i);
    if (num_errs[i] > 0) {
	printf ("%d %s error%s\n", num_errs[i], err_names[i], num_errs[i] == 1 ? ":" : "s:");
        while (signal_errs[i]) {
          emit_err_log (a, i, signal_errs[i]);
          signal_errs[i] = signal_errs[i]->next;
        }
    }
  }
  if (show_frequency) {
    if (min_cycle_time == delta_t*Nsteps) {
      printf ("*** could not estimate frequency.\n");
    }
    else {
      printf ("Cycle time: %g ns (signal `%s')\n", 
	      round_double(min_cycle_time*1e9),
	      ATRACE_GET_NAME (a,min_cycle_node));
      printf ("Frequency: %g MHz\n", round_double(1e-6/min_cycle_time));
    }
  }
  return 0;
}


/*
  0 = low
  1 = high
  -1 = X

  last_val is used for hysteresis.
*/
static int raw_analog2digital (double v)
{
  if (v <= V_low)
    return 0;
  if (v >= V_high)
    return 1;
  /* else it is X */
  return -1;
}

static int analog2digital (double v, int last_val)
{
  int ret;

  ret = raw_analog2digital (v);
  if (ret == last_val) return ret;

  if (ret != last_val) {

    if (raw_analog2digital (v - hysteresis) == raw_analog2digital (v + hysteresis)) {
      return ret;
    }
    return last_val;
#if 0
    switch (last_val) {
    case 0: /* low */
      if (raw_analog2digital (v - hysteresis) == 0) {
	return 0;
      }
      break;
    case 1: /* high */
      if (raw_analog2digital (v + hysteresis) == 1) {
	return 1;
      }
      break;
    case -1: /* X */
      if (ret == 0 && raw_analog2digital (v + hysteresis) == -1) {
	return -1;
      }
      if (ret == 1 && raw_analog2digital (v - hysteresis) == -1) {
	return -1;
      }
      break;
    default:
      fatal_error ("Should not be here\n");
      break;
    }
#endif
  }
  return ret;
}


static void add_err_log (enum err_type t, int num, double tm, double val)
{
  struct err_log *entry;

  if (tm < skip_initial_time) return;

  if (num_errs[t] == max_print) {
    if (smaller_val_better[t]) {
      if (signal_errs[t] && (fabs (val) < fabs (signal_errs[t]->val))) {
	return;
      }
    }
    else {
      if (signal_errs[t] && (fabs(signal_errs[t]->val) < fabs(val))) return;
    }
    entry = signal_errs[t];
    signal_errs[t] = signal_errs[t]->next;
  }
  else {
    NEW (entry, struct err_log);
    num_errs[t]++;
  }
  entry->num = num;
  entry->tm = tm;
  entry->val = val;
  entry->next = NULL;
  if (!signal_errs[t]) {
    signal_errs[t] = entry;
  }
  else {
    struct err_log *cur, *prev;
    prev = NULL;
    cur = signal_errs[t];
    if (smaller_val_better[t]) {
      while (cur) {
	if (fabs (val) < fabs(cur->val)) {
	  if (!prev) {
	    entry->next = signal_errs[t];
	    signal_errs[t] = entry;
	  }
	  else {
	    prev->next = entry;
	    entry->next = cur;
	  }
	  return;
	}
	prev = cur;
	cur = cur->next;
      }
    }
    else {
      while (cur) {
	if (fabs (val) > fabs(cur->val)) {
	  if (!prev) {
	    entry->next = signal_errs[t];
	    signal_errs[t] = entry;
	    return;
	  }
	  else {
	    prev->next = entry;
	    entry->next = cur;
	    return;
	  }
	}
	prev = cur;
	cur = cur->next;
      }
    }
    prev->next = entry;
  }
}

static int is_digital_node (atrace *a, int node_id)
{
  Assert (0 <= node_id && node_id < a->Nnodes, "Invalid node id!");
  if (a->N[node_id]->type != 0) {
    return 1;
  }
  return 0;
}

static int is_internal_node (char *s)
{
  int i;

  i = strlen (s);
  i--;

  if (i >= 0 && s[i] == '#') {
    /* ending in # is an internal node */
    return 1;
  }

  while (i >= 0 && isdigit (s[i])) {
    i--;
  }
  if (i >= 0 && s[i] == '#') {
    if (i == 0 || s[i-1] == '.')
      /* a node name #<digits> is an internal node */
      return 1;
  }
  return 0;
}

static int ignore_node (char *s)
{
  static int first = 1;
  static regex_t match;

  if (filter_results[0] == '\0') return 0;

  if (first) {
    if (regcomp (&match, filter_results, REG_EXTENDED) != 0) {
      fatal_error ("Could not compile regular expression `%s' for filter pattern.", filter_results);
    }
  }
  first = 0;
  if (regexec (&match, s, 0, NULL, 0) == 0) {
    return 1;
  }
  return 0;
}

typedef struct node_info {
  /* [0] = prev value, [1] = the one before that */
  float hist_val[2];
  float hist_tm[2];
  int  hist_dig[2];

  float max_val;		/* max value in the X regime */
  float min_val;		/* min value in the X regime */

  float tm_first_nonX;		/* computing cycle time */
  int first_nonX;
  float tm_last_nonX;
  int count;			/* # of transitions that match first_nonX */

  int skip;

  name_t *digital_link;		/* digital node? */
  PrsNode *pn;			/* prs node? */
  int is_prs_input;		/* prs input? */

} node_info_t;
  
double cputime_msec ();

static void spice_to_act_name (char *s, char *t, int sz, int xconv)
{
  char buf[10240];
  int i = 0;
  int possible_x = xconv;
  char *tmp;
  int countdots = 0;

  tmp = s;
  while (*tmp) {
    if (*tmp == '.') countdots++;
    tmp++;
  }

  while (*s) {
    if (possible_x && *s == 'x') {
      possible_x = 0;
    }
    else {
      buf[i] = *s;
      i++;
      if (i == 10240) fatal_error ("Resize the buffer");
      buf[i] = '\0';
      if (*s == '.') {
	countdots--;
	if (countdots == 0) {
	  possible_x = 0;
	}
	else {
	  possible_x = xconv;
	}
      }
      else {
	possible_x = 0;
      }
    }
    s++;
  }
  act_global->unmangle_string (buf, t, sz);
}

void dump_pending (Prs *P)
{
  int i;

  if (!P->eventQueue || (heap_peek_min (P->eventQueue) == NULL)) {
    printf ("No pending events!\n");
    return;
  }

  for (i=0; i < P->eventQueue->sz; i++) {
    PrsEvent *ev = (PrsEvent *)P->eventQueue->value[i];
    Time_t t = P->eventQueue->key[i];
    printf ("  %s := %c  @ ", prs_nodename (P, ev->n), prs_nodechar (ev->val));
    printf ("%llu\n", t);
  }
}

static Prs *P_global;

static void check_nodeval (PrsNode *n, void *val)
{
  long v = (long)val;

  if (n->flag) return;

  n->flag = 1;

  if (n->val == v) {
      printf ("%s ", prs_nodename (P_global,n));
  }
}

static void clear_nodeflag (PrsNode *n, void *val)
{
  n->flag = 0;
}

static void dump_undefined (Prs *P)
{
  int i;
  hash_bucket_t *b;


  if (!P) return;

  P_global = P;

  prs_apply (P, (void*)PRS_VAL_X, check_nodeval);
  prs_apply (P, (void*)NULL, clear_nodeflag);
  printf ("\n");
}



static void process_signal_change (atrace *aout, Prs *p, node_info_t *ni, float tm, int v)
{
  PrsNode *pn, *cause;
  /* there is a signal change, v=new value */
  cause = NULL;

#if 0
  if (!ni->pn) {
    printf (" ** no corresponding node\n");
  }
#endif

  if (ni->pn) {
    if (ni->is_prs_input) {
      if (verbose > 2) {
	printf (" ** input change\n");
      }
      prs_set_node (p, ni->pn, v ? PRS_VAL_T : PRS_VAL_F);
      pn = prs_step_cause (p, &cause, NULL);
      Assert (pn == ni->pn, "This is strange");
    }
    else {
      /* check there is a pending event! */
      if (!ni->pn->exq && ni->pn->queue) {
	cause = ni->pn->queue->cause;
	if ((ni->pn->queue->val == PRS_VAL_T && v == 1) ||
	    (ni->pn->queue->val == PRS_VAL_F && v == 0)) {
	  if (verbose > 1) {
	    printf ("t=%.3gns : validated %s%c", tm*1e9, prs_nodename (p, ni->pn), v ? '+' : '-');
	    if (cause && cause->space) {
	      printf (" (cause: %s)\n", prs_nodename (p, cause));
	    }
	    else {
	      if (cause) {
		printf (" (no cause, %s)\n", prs_nodename (p, cause));
	      }
	      else {
		printf (" (no cause)\n");
	      }
	    }
	  }
#if 0
	  {
	    int ii;
	    printf ("HEAP-IN:\n ");
	    for (ii=0; ii < p->eventQueue->sz; ii++) {
	      printf ("%d ", (int)p->eventQueue->key[ii]);
	    }
	    printf ("\n");
	  }
#endif
	  Assert (heap_update_key (p->eventQueue, p->time, ni->pn->queue) == 1, "Failed to update event queue");
#if 0
	  {
	    int ii;
	    printf ("HEAP-OUT:\n ");
	    for (ii=0; ii < p->eventQueue->sz; ii++) {
	      printf ("%d ", (int)p->eventQueue->key[ii]);
	    }
	    printf ("\n");
	  }
#endif
	  pn = prs_step_cause (p, &cause, NULL);
	  Assert (pn == ni->pn, "Event queue management error");
	}
	else {
	  printf ("ERR t=%.3gns : transition %s%c, prs has %c\n", tm*1e9, prs_nodename (p, ni->pn), v ? '+' : '-', prs_nodechar(ni->pn->queue->val));

	  if (verbose) {
	    prs_printrule (p, ni->pn, 1);
	  }
	}


      }
      else {
	if (is_init_prs_firing (ni->pn, v ? PRS_VAL_T : PRS_VAL_F)) {
	  /* ok, we're good! */
	}
	else {
	  printf ("ERR t=%.3gns : missing %s%c in prs\n", tm*1e9, prs_nodename (p, ni->pn), v ? '+' : '-');
	  if (verbose) {
	    prs_printrule (p, ni->pn, 1);
	  }
#if 0
	  printf (" --- ERR DEBUG ---\n");
	  dump_pending (p);
	  printf (" --- END ---\n");
#endif
	}
      }
    }
  }
  if (aout && ni->digital_link) {
    if (cause && cause->space) {
      atrace_signal_change_cause (aout, ni->digital_link, tm, v, ((name_t *)cause->space));
    }
    else {
      atrace_signal_change (aout, ni->digital_link, tm, v);
    }
  }
}


static PrsNode *find_prs_node (Prs *p, char *buf)
{
  PrsNode *n;
  int i;
  hash_bucket_t *b;

  n = prs_node (p, buf);
  if (n) return n;

  /* ok, now we have to match this without case sensitivity */
  for (i=0; i < p->H->size; i++) 
    for (b = p->H->head[i]; b; b = b->next) {
      n = (PrsNode *)b->v;
      if (strcasecmp (buf, b->key) == 0) {
	return n;
      }
    }
  return NULL;
}


struct ev_buf {
  PrsNode *n;
  int val;
  double tm;
};

/* initial event buffer */
L_A_DECL(struct ev_buf, initbuf);

/* reorder event buffer */
L_A_DECL(struct ev_buf, reorder_buf);


void add_init_firing (PrsNode *n, int val)
{
  A_NEW(initbuf, struct ev_buf);
  A_NEXT(initbuf).n = n;
  A_NEXT(initbuf).val = val;
  A_INC(initbuf);
}

void add_reordered_firing(PrsNode *n, int val, double tm)
{
  A_NEW (reorder_buf, struct ev_buf);
  A_NEXT (reorder_buf).n = n;
  A_NEXT (reorder_buf).val = val;
  A_NEXT (reorder_buf).tm = tm;
  A_INC (reorder_buf);
}

int is_init_prs_firing (PrsNode *n, int val)
{
  int i;

  for (i=0; i < A_LEN (initbuf); i++) {
    if (initbuf[i].n == n && initbuf[i].val == val) {
      /* found! */
      for (; i < A_LEN(initbuf)-1; i++) {
	initbuf[i] = initbuf[i+1];
      }
      A_LEN(initbuf)--;
      return 1;
    }
  }
  return 0;
}


int is_reordered_buf (PrsNode *n, int val, double tm)
{
  int i;
  int found;
  int count;

  found = 0;
  for (i=0; i < A_LEN (reorder_buf); i++) {
    if (reorder_buf[i].n == n) {
      if (reorder_buf[i].val == val) {
	/* found! */
	for (; i < A_LEN(reorder_buf)-1; i++) {
	  reorder_buf[i] = reorder_buf[i+1];
	}
	A_LEN(reorder_buf)--;
	found = 1;
	break;
      }
    }
  }
  count = 0;
  for (i=0; i < A_LEN (reorder_buf); i++) {
    if (reorder_buf[i].tm + 5 < tm) {
      count = i;
      for (i=0; i < A_LEN(reorder_buf)-count; i++) {
	reorder_buf[i] = reorder_buf[i+count];
      }
      A_LEN(reorder_buf) -= count;
      break;
    }
  }
  return found;
}


static void compute_errs (atrace *a, int Nnodes, int Nsteps)
{
  int i, j;
  node_info_t *n;
  int idx = 0;
  double tm;
  char buf[10240];
  atrace *new_a = NULL;
  Prs *p = NULL;

  MALLOC (n, node_info_t, Nnodes);

  for (i=0; i < Nnodes; i++) {
    n[i].digital_link = NULL;
    n[i].pn = NULL;
    n[i].is_prs_input = 0;
  }

  atrace_init_time (a);

  if (output_file_name) {
    if (prs_file_name) {
      p = prs_fopen (prs_file_name);
      new_a = atrace_create (output_file_name, ATRACE_DELTA_CAUSE, 
			     a->stop_time, a->dt);
    }
    else {
      new_a = atrace_create (output_file_name, ATRACE_DELTA,
			     a->stop_time, a->dt);
    }
    /* i=0 is the special "time" node */
    for (i=1; i < Nnodes; i++) {
      if (is_internal_node (ATRACE_GET_NAME (a,i)))
	continue;

      if (ATRACE_GET_NAME (a,i)[0] == 'x') {
	spice_to_act_name (ATRACE_GET_NAME (a, i), buf, 10240, 1);
      }
      else {
	buf[10239] = '\0';
	snprintf (buf, 10239, "%s", ATRACE_GET_NAME (a,i));
      }
      n[i].digital_link = atrace_create_node (new_a, buf);
      atrace_mk_digital (n[i].digital_link);
      if (p) {
	n[i].pn = find_prs_node (p, buf);
	if (n[i].pn) {
	  PrsNode *pn_a;
	  /* add aliases based on aliases in the prs file */
	  pn_a = n[i].pn->alias_ring;
	  while (pn_a != n[i].pn) {
	    atrace_alias (new_a, n[i].digital_link, atrace_create_node (new_a, pn_a->b->key));
	    pn_a = pn_a->alias_ring;
	  }

	  /* now check if it is an input */
	  if (prs_num_fanin (n[i].pn) == 0) {
	    n[i].is_prs_input = 1;
	  }
	  /* unit delay 1 step = 1 unit */
	  n[i].pn->delay_up[0] = n[i].pn->delay_up[1] = 1;
	  n[i].pn->delay_dn[0] = n[i].pn->delay_dn[1] = 1;

	  /* back link */
	  n[i].pn->space = n[i].digital_link;

	  if (verbose > 2) {
	    printf ("MAP %s  to PRS %s\n", ATRACE_GET_NAME (a,i), prs_nodename (p, n[i].pn));
	  }
	}
	else {
	  if (verbose > 1) { 
	    printf ("WARNING: found no corresponding node: %s -> %s\n",
		    ATRACE_GET_NAME(a,i), buf);
	  }
	}
      }
    }
  }


#define PREV 0
#define EARLIER 1

  for (i=0; i < Nnodes; i++) {
    n[i].hist_val[PREV] = a->N[i]->v;
    n[i].hist_tm[PREV] = 0;
    n[i].hist_tm[EARLIER] = -1;
    n[i].hist_dig[PREV] = raw_analog2digital (n[i].hist_val[PREV]);
    n[i].hist_dig[EARLIER] = -2;
    n[i].max_val = n[i].hist_val[PREV];
    n[i].min_val = n[i].hist_val[PREV];
    n[i].skip = (is_internal_node (ATRACE_GET_NAME(a,i)) ||
		 ignore_node (ATRACE_GET_NAME(a,i)) ||
		 is_digital_node (a,i)) ? 1 : 0;
    n[i].tm_first_nonX = -2;

    if (n[i].pn && n[i].is_prs_input) {
      if (n[i].hist_dig[PREV] == 0) {
	prs_set_node (p, n[i].pn, PRS_VAL_F);
	if (verbose > 2) {
	  printf (" set %s := 0\n", prs_nodename(p, n[i].pn));
	}
      }
      else if (n[i].hist_dig[PREV] == 1) {
	prs_set_node (p, n[i].pn, PRS_VAL_T);
	if (verbose > 2) {
	  printf (" set %s := 1\n", prs_nodename(p, n[i].pn));
	}
      }
    }
  }

  tm = cputime_msec ();

  if (p) {
    PrsNode *pn;
    int cnt = 0;

    p->flags |= PRS_NO_WEAK_INTERFERENCE;

    while (pn = prs_step_cause (p, NULL, NULL)) {
      cnt++;

      add_init_firing (pn, pn->val);

      if (verbose > 2) {
	printf ("Init firing: %s := %c\n", prs_nodename (p, pn), prs_nodechar (pn->val));
      }

      if (cnt > 100000) {
	printf ("Exceeded 100,000 firings on reset. Aborting!\n");
	exit (1);
      }
    }
    p->time = 0;
  }

  buf[0] = '\0';
  for (i=1; i < Nsteps; i++) {
    name_t *nm;
    int special_shift;

    special_shift = 0;

    if (verbose > 2) {
    if ((i % 5000) == 0) {
      int kk;
      for (kk=0; kk < strlen (buf); kk++) {
	printf ("\b \b");
      }
      fflush (stdout);
      tm += cputime_msec();
      sprintf (buf, "Est. total time: %6.3g mins. [%4.2g%% done]..", tm/1000.0/i/60.0*Nsteps, (i+0.0)/Nsteps*100.0);
      printf ("%s", buf);
      fflush (stdout);
    }
    }

    if (i == (int) (reset_sync_time/a->dt)) {
      /* verify that all nodes match */
      for (j=0; j < Nnodes; j++) {
	int v;
	if (n[j].skip) continue;
	if (n[j].pn) {
	  if (n[j].pn->val == PRS_VAL_X) {
	    v = raw_analog2digital (a->N[j]->v);
	    if (v == 0) {
	      prs_set_node (p, n[j].pn, PRS_VAL_F);
	      prs_step_cause (p, NULL, NULL);
	    }
	    else if (v == 1) {
	      prs_set_node (p, n[j].pn, PRS_VAL_T);
	      prs_step_cause (p, NULL, NULL);
	    }
	  }
	  if (raw_analog2digital (a->N[j]->v) == 1) {
	    if (n[j].pn->val != PRS_VAL_T) {
	      printf (" *** initialization error: %s should be 1 (is %c)\n", 
		      prs_nodename (p, n[j].pn), prs_nodechar (n[j].pn->val));
	    }
	  }
	  else if (raw_analog2digital (a->N[j]->v) == 0) {
	    if (n[j].pn->val != PRS_VAL_F) {
		printf (" *** initialization error: %s should be 0 (is %c)\n", 
			prs_nodename (p, n[j].pn), prs_nodechar (n[j].pn->val));
	    }
	  }
	  else {
	    if (n[j].pn->val != PRS_VAL_X) {
	      printf (" *** initialization error: %s should be X (is %c)\n", 
		      prs_nodename (p, n[j].pn), prs_nodechar (n[j].pn->val));
	    }
	  }
	}
      }
    }

    if (a->curt < 0) break;
    atrace_advance_time (a, 1);

    if (p) {
      if (p->time != i-1) {
	printf ("time not consistent?!\n");
      }

      /* push everything on the prsim heap forward one time unit! */
      {
	int ii;
	for (ii=0; ii < p->eventQueue->sz; ii++) {
	  p->eventQueue->key[ii]++;
	}
      }
    }

    nm = a->hd_chglist;
    while (nm) {
      int v;

      j = nm->idx;

      /*if (n[j].skip) {
	goto next;
	}
      */

      if (n[j].hist_val[PREV] == nm->v) {
	goto next;
      }

      v = analog2digital (nm->v, n[j].hist_dig[PREV]);
      
      if (v != n[j].hist_dig[PREV]) {
	/* if v is X now, or if we don't have enough history, nothing
	   to do */
	if (n[j].hist_dig[EARLIER] != -2 && v != -1) {
	  if (v == n[j].hist_dig[EARLIER]) {
	    float bump;
	    /* charge sharing! */
	    if (v == 0) {
	      bump = n[j].max_val;
	    }
	    else {
	      Assert (v == 1, "Eh?");
	      bump = n[j].min_val-Vdd;
	    }
	    if (fabs (bump) >= (Vdd/2)) {
	      if (!n[j].skip)
		add_err_log (INCOMPLETE_TRANSITION, j, a->curt, bump);
	    }
	    else {
	      if (!n[j].skip) {
		add_err_log (CHG_SHARING, j, a->curt, bump);
	      }
	    }
	  }
	  else {
	    double slew, aslew;


	    /* slew rate check */
	    if ((v == 1 && n[j].hist_dig[PREV] == 0) ||
		(v == 0 && n[j].hist_dig[PREV] == 1)) {
	      /* transition so fast we didn't go through X */
	      slew = (nm->v - n[j].hist_val[PREV])/((a->curt-n[j].hist_tm[PREV])*1e9);

	      if (!n[j].skip) {
		add_err_log (FAST_TRANSITION, j, a->curt, slew);
	      }
	      special_shift = 1;
	    }
	    else if (v == 0 || v == 1) {
	      Assert (n[j].hist_dig[PREV] == -1, "Hmm...");
	      Assert (n[j].hist_dig[EARLIER] != v, "Hmmmm.");
	      slew = (nm->v - n[j].hist_val[PREV])/(1e9*(a->curt-n[j].hist_tm[PREV]));
	      aslew = fabs (slew);
	      if (aslew <= slewrate_slow_threshold) {
		if (!n[j].skip) {
		  add_err_log (SLOW_TRANSITION, j, n[j].hist_tm[PREV], slew);
		}
	      }
	      if (aslew >= slewrate_fast_threshold) {
		if (!n[j].skip) {
		  add_err_log (FAST_TRANSITION, j, n[j].hist_tm[PREV], slew);
		}
	      }
	    }
	  }
	}

	if (n[j].hist_dig[PREV] != v) {
	  if (n[j].hist_dig[PREV] == 0 && v == -1) {
	    if (verbose > 2) {
	      printf ("[%.4g] change from 0, signal %s\n", i*a->dt*1e9, ATRACE_GET_NAME(a, j));
	    }
	    /* 0 -> X */
	    process_signal_change (new_a, p, &n[j], i*a->dt, 1);
	  }
	  else if (n[j].hist_dig[PREV] == 1 && v == -1) {
	    if (verbose > 2) {
	      printf ("[%.4g] change from 1, signal %s\n", i*a->dt*1e9, ATRACE_GET_NAME(a, j));
	    }
	    /* 1 -> X */
	    process_signal_change (new_a, p, &n[j], i*a->dt, 0);
	  }
	}

	/* shift it over */
	n[j].hist_val[EARLIER] = n[j].hist_val[PREV];
	n[j].hist_tm[EARLIER] = n[j].hist_tm[PREV];
	n[j].hist_dig[EARLIER] = n[j].hist_dig[PREV];

	n[j].hist_val[PREV] = nm->v;
	n[j].hist_tm[PREV] = a->curt;
	n[j].hist_dig[PREV] = v;

	if (special_shift) {
	  n[j].hist_dig[EARLIER] = -2;
	}
	
	if (v == -1) { /* it's X now---reset max/min */
	  n[j].max_val = n[j].min_val = n[j].hist_val[PREV];
	}
	else {
	  if (n[j].tm_first_nonX == -2) {
	    n[j].tm_first_nonX = -1;
	  }
	  else {
	    if (n[j].tm_first_nonX == -1) {
	      n[j].first_nonX = v;
	      n[j].tm_first_nonX = a->curt;
	      n[j].tm_last_nonX = a->curt;
	      n[j].count = 0;
	    }
	    else if (n[j].first_nonX == v) {
	      n[j].tm_last_nonX = a->curt;
	      n[j].count++;
	    }
	  }
	}
      }
      else {
	/* same old, nothing to do */
	if (v == -1) /* X */ {
	  if (nm->v > n[j].max_val)
	    n[j].max_val = nm->v;
	  if (nm->v < n[j].min_val)
	    n[j].min_val = nm->v;
	}
      }
    next:
      nm = nm->chg_next;
    }
    if (p) {
      int ii;
      p->time++;
      for (ii=0; ii < p->eventQueue->sz; ii++) {
	if (p->eventQueue->key[ii] <= (1+p->time)) {
	  break;
	}
      }
      if (ii != p->eventQueue->sz) {
	for (ii=0; ii < p->eventQueue->sz; ii++) {
	  p->eventQueue->key[ii]++;
	}
      }
    }
  }
  if (verbose) {
    int kk;
    for (kk=0; kk < strlen (buf); kk++) {
      printf ("\b \b");
    }
    fflush (stdout);
  }
  for (i=0; i < Nnodes; i++) {
    double my_cycle_time;
    if (n[i].skip) continue;
    if (n[i].tm_first_nonX >= 0 && n[i].count > 1) {
      my_cycle_time = (n[i].tm_last_nonX-n[i].tm_first_nonX)/n[i].count;
      if (my_cycle_time < min_cycle_time) {
	min_cycle_time = my_cycle_time;
	min_cycle_node = i;
      }
    }
  }
  FREE (n);

  if (new_a) {
    atrace_close (new_a);
    new_a = NULL;
  }
  if (p) {
    prs_free (p);
  }
}


/*------------------------------------------------------------------------
   System-dependent CPU time
   Returns CPU time since last measurement in milliseconds.
------------------------------------------------------------------------*/
#include <sys/time.h>
#include <sys/resource.h>

/* set default return type */

double cputime_msec ()
{
  static int first_time = 1;
  static struct rusage last_measurement;
  struct rusage current_measurement;
  double tm;

  getrusage (RUSAGE_SELF, &current_measurement);
  
  if (first_time) {
    last_measurement.ru_utime.tv_sec = 0.0;
    last_measurement.ru_utime.tv_usec = 0.0;
    last_measurement.ru_stime.tv_sec = 0.0;
    last_measurement.ru_stime.tv_usec = 0.0;
    first_time = 0;
  }
  
  tm = (current_measurement.ru_utime.tv_sec
	-last_measurement.ru_utime.tv_sec)*1000.0;
  tm += (current_measurement.ru_stime.tv_sec
	-last_measurement.ru_stime.tv_sec)*1000.0;

  tm += (current_measurement.ru_utime.tv_usec
	-last_measurement.ru_utime.tv_usec)*0.001;
  tm += (current_measurement.ru_stime.tv_usec
	-last_measurement.ru_stime.tv_usec)*0.001;

  getrusage (RUSAGE_SELF, &last_measurement);
  
  return tm;
}
