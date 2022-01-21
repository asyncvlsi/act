/*************************************************************************
 *
 *  Copyright (c) 2011, 2019 Rajit Manohar
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

/*
  VPI interface to prsim
*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "vpi_user.h"
#include "prs.h"

void prsim_cpp_dump_tc (char *filename, int handle);
void prsim_cpp_dump_tcall (char *filename, int handle);
static void _int_prsim_dump_pending (Prs *p);

#if 0
#define DEBUG_TO_PRSIM
#define DEBUG_FROM_PRSIM
#endif

struct prs_nodeinfo {
  Prs *p;
  vpiHandle net;
};

#define PNI(x) ((struct prs_nodeinfo *)(x)->space)

/*
  Production-rule simulator!
*/

#define MAX_PRSIM 8192

/* 32 different simulators available at any time */
static Prs *P[MAX_PRSIM];
static int used_prsim = 0;
static unsigned int scale_prsim_time = 1;

static int debug_level = 0;

static Prs *curP = NULL;

#if 0
#define DEBUGFN(tm)   range_debug (tm, __FUNCTION__)
#else
#define DEBUGFN(tm)
#endif

static void range_debug (Time_t curtime, const char *nm)
{
  if (curtime >= 382252089300ULL && curtime <= 382252093700ULL) {
	vpi_printf ("--> [%llu] %s\n", curtime, nm);
  }
}

static int new_prs (void)
{
  if (used_prsim == MAX_PRSIM) {
    fatal_error ("Sorry, too many open prs files (max=%d)!", MAX_PRSIM);
  }
  used_prsim++;
  P[used_prsim-1] = NULL;

  return used_prsim-1;
}

static Prs *handle_to_prs (int handle)
{
  if (handle < 0) {
    return curP;
  }
  else {
    if (handle < used_prsim) {
      return P[handle];
    }
    else {
      return NULL;
    }
  }
}

static int prs_to_handle (Prs *p)
{
  int i;
 
  for (i=0; i < used_prsim; i++) {
	if (p == P[i]) return i;
  }
  return -1;
}



static void delete_last_prs (void)
{
  used_prsim--;
}

static void _run_prsim (Prs *p, Time_t vcstime);

static char *skip_white (char *s)
{
  while (*s && isspace (*s)) {
    s++;
  }
  return s;
}

static void vcs_to_prstime (s_vpi_time *p, Time_t *tm)
{
#ifdef TIME_64
  *tm = (((unsigned long long)p->high) << 32) | ((unsigned long long)p->low);
  *tm = (*tm) * (unsigned long long)scale_prsim_time;
#else
  *tm = p->low;
  *tm = (*tm) * scale_prsim_time;
#endif
}

static void prs_to_vcstime (s_vpi_time *p, Time_t *tm)
{
  Time_t ntm;
#ifdef TIME_64
  ntm = (*tm + (Time_t)(scale_prsim_time-1)) / (unsigned long long) scale_prsim_time;
  p->high = ((unsigned long)((ntm) >> 32) & 0xffffffffUL);
  p->low = (unsigned long) (ntm & 0xffffffffUL);
#else
  ntm = (*tm + scale_prsim_time-1) / scale_prsim_time;
  p->high = 0;
  p->low = (unsigned long) ntm & 0xffffffffUL;
#endif
}

unsigned long long my_vcs_current_time (void)
{
 s_vpi_time tm;
 unsigned long long t;

 tm.type = vpiSimTime;
 vpi_get_time (NULL, &tm);
 vcs_to_prstime (&tm, &t);
 return t; 
}

/*
 *  Flush all pending events in the prsim simulation(s)
 *  until time "cur" (including all events at time "cur")
 *
 *  If p != NULL, the specified simulation is run first.
 */
static void _align_prs_simulation_times (Time_t cur, Prs *p)
{
  int i;
  if (p) {
    _run_prsim (p, cur);
  }
  for (i=0; i < used_prsim; i++) {
    if (p != P[i]) {
      _run_prsim (P[i], cur);
    }
  }
}


static int scheduled = 0;
static Time_t scheduled_time;


/*
  last scheduled _run_prsim_callback. We need to remove it!!! Insane! 
*/
static vpiHandle last_registered_callback;

static void register_self_callback (Time_t vcstime);

/*
 *  Step all prs simulators to the current time
 */
static void _run_prsim_callback (p_cb_data p)
{
  Time_t curtime;
  int i;

  scheduled = 0;
  vcs_to_prstime (p->time, &curtime);

  DEBUGFN(curtime);

  if (debug_level > 1) {
    vpi_printf ("running prsim callback @ %llu (scheduled=%llu)\n", curtime, scheduled_time);
  }

  vpi_remove_cb (last_registered_callback);
  _align_prs_simulation_times (curtime, NULL);
  register_self_callback (curtime);
}


static void register_self_callback (Time_t vcstime)
{
  static s_cb_data cb_data;
  static s_vpi_time tm; /* at most one callback pending, let's not malloc! */
  int i;
  Time_t mytime;
  int has_time;
  int prs_id;
  PrsEvent *pe;

  if (debug_level > 1) {
    vpi_printf ("register_self_callback\n");
  }
  DEBUGFN(vcstime);
  
  has_time = 0;
  for (i=0; i < used_prsim; i++) {
    if (heap_peek_min (P[i]->eventQueue) != NULL) {
      if (!has_time) {
	mytime = heap_peek_minkey (P[i]->eventQueue);
        pe = (PrsEvent *) heap_peek_min (P[i]->eventQueue);
	has_time = 1;
	prs_id = i;
      }
      else {
	mytime = time_min (mytime, heap_peek_minkey (P[i]->eventQueue));
        pe = (PrsEvent *) heap_peek_min (P[i]->eventQueue);
	prs_id = i;
      }
    }
  }

  if (debug_level > 1) {
    vpi_printf ("***\n");
    vpi_printf ("vcstime = %llu\n", vcstime);
    vpi_printf ("has_time = %d, mytime = %llu\n", has_time, mytime);
    if (has_time) {
      vpi_printf ("   EVENT: node %s set to %c\n", prs_nodename (P[prs_id], pe->n), prs_nodechar (pe->val));
    }
    vpi_printf ("scheduled = %d, scheduled_time = %llu\n", scheduled, scheduled_time);
    vpi_printf ("***\n");
  }

  if (!has_time) {
    if (scheduled) {
      scheduled = 0;
      if (debug_level > 1) {
	vpi_printf ("** removing callback at time %llu\n", vcstime);
      }
      vpi_remove_cb (last_registered_callback);
    }
    if (debug_level > 1) {
      vpi_printf ("no future pending events!\n");
    }
    /* I have nothing to do, go away */
    return;
  }
  
  if (scheduled == 1) {
    if (mytime < scheduled_time) {
      /* we have an earlier event! */
      scheduled = 0;
      vpi_remove_cb (last_registered_callback);
    }
    else {
      if (debug_level > 1) {
	vpi_printf ("***\n");
	vpi_printf ("existing callback at earlier time %llu v/s %llu\n", scheduled_time, mytime);
	vpi_printf ("current time is %llu\n", vcstime);
	vpi_printf ("***\n");
      }
      if ((vcstime > scheduled_time) && (mytime > vcstime)) {
	scheduled = 0;
	if (debug_level > 1) {
	 vpi_printf ("*** this should not happen? at time = %llu\n", vcstime);
        }
        /* XXXX: this should not happen, but it does; need to investigate 
	vpi_remove_cb (last_registered_callback);*/
      }
      else {
	return;
      }
    }
  }

  cb_data.reason = cbAfterDelay;
  cb_data.cb_rtn = _run_prsim_callback;
  cb_data.obj = NULL;

  cb_data.time = &tm;

  if (debug_level > 1) {
    vpi_printf ("***\n");
    vpi_printf ("vcs time is %llu\n", vcstime);
    vpi_printf ("earliest prsim event time is %llu\n", mytime);
    vpi_printf ("***\n");
  }

  if (mytime > vcstime) {
    scheduled_time = mytime;
    vcstime = mytime - vcstime;
  }
  else {
    if (debug_level > 1) {
      vpi_printf ("**vpi-prsim-WARNING** Potential negative time future event!\n");
    }
    scheduled_time = vcstime + 1;
    vcstime = 1;
  }

  cb_data.time->type = vpiSimTime;
  prs_to_vcstime (cb_data.time, &vcstime);

  cb_data.value = NULL;
  cb_data.user_data = NULL;
  if (debug_level > 1) {
    vpi_printf ("callback time set to: %llu\n", scheduled_time);
  }
  last_registered_callback = vpi_register_cb (&cb_data);
  scheduled = 1;
}

static void _run_prsim (Prs *p, Time_t vcstime)
{
  int gap;
  PrsNode *n;
  PrsNode *m;
  int seu;
  Time_t prsdiff;
  int ran_once;
  s_vpi_error_info info;

#if 0
  vpi_printf ("Running prsim %x @ time %lu\n", p, (unsigned long)(vcstime & 0xffffffff));
#endif
  DEBUGFN(vcstime);
  ran_once = 0;
  /* run for at most 1ps */
  while ((heap_peek_min (p->eventQueue) && heap_peek_minkey (p->eventQueue) <= vcstime /*&& (!scheduled || (heap_peek_minkey (p->eventQueue) <= scheduled_time))*/)
	 && (n = prs_step_cause (p, &m, &seu))) {
    ran_once = 1;
    if (n->bp && PNI(n) && PNI(n)->net) {
      vpiHandle net;
      s_vpi_value v;
      s_vpi_time tm;

      if (vcstime > p->time) {
	prsdiff = 0;
	vpi_printf ("**vpi-prsim-WARNING** Suppressed negative time event on prsim signal %s\n", prs_nodename (p, n));
	vpi_printf ("**vpi-prsim-WARNING** Total negative time: %llu\n", vcstime - p->time);
      }
      else {
	prsdiff = p->time - vcstime;
      }
      tm.type = vpiSimTime;
      prs_to_vcstime (&tm, &prsdiff);
      /* aha, schedule an event into the vcs queue */
      net = PNI(n)->net;
      v.format = vpiScalarVal;
      Assert (net, "What?");
      switch (n->val) {
      case PRS_VAL_T:
#if 0
	vpi_printf ("Set net %s (%x) to TRUE\n", prs_nodename (n), net);
#endif
	v.value.scalar = vpi1;
	break;
      case PRS_VAL_F:
#if 0
	vpi_printf ("Set net %s (%x) to FALSE\n", prs_nodename (n), net);
#endif
	v.value.scalar = vpi0;
	break;
      case PRS_VAL_X:
#if 0
	vpi_printf ("Set net %s (%x) to X\n", prs_nodename (n), net);
#endif
	v.value.scalar = vpiX;
	break;
      default:
	fatal_error ("Unknown value");
	break;
      }
      if (debug_level > 2) { 
      vpi_printf ("[tovcs] signal %s changed @ time %llu, val = %d\n",
		  prs_nodename (p, n),
		  p->time,
		  v.value.scalar);
      }
      {
	vpiHandle retval;
	retval = vpi_put_value (net, &v, &tm, vpiPureTransportDelay);
	if (vpi_chk_error (&info)) {
	  vpi_printf ("**vpi-prsim** vpi_put_value error. prsim name: %s!\n", prs_nodename (p, n));
	  vpi_printf ("**vpi-prsim** Message: %s\n", info.message);
	}
	vpi_free_object (retval);
      }
      if (debug_level > 2) { 
      vpi_printf ("[aftertovcs] signal %s changed @ time %llu, twtime = hi:%lu lo:%lu val = %d\n",
		  prs_nodename (p, n),
		  p->time,
		  tm.low,
		  tm.high,
		  v.value.scalar);
      }
    }
    else if (n->bp && (!PNI(n) || !PNI(n)->net)) {
      vpi_printf ("\t%10llu %s : %c", 
		  p->time,
		  prs_nodename (p,n),
		  prs_nodechar(prs_nodeval(n)));
      if (m) {
	vpi_printf ("  [by %s:=%c%s]", prs_nodename (p,m), 
		prs_nodechar (prs_nodeval (m)),
		seu ? " *seu*" : "");
      }
      vpi_printf ("\n");
      vpi_flush ();
    }
    if (debug_level > 2) { 
      vpi_printf ("\t%10llu %s : %c", 
		  p->time,
		  prs_nodename (p,n),
		  prs_nodechar(prs_nodeval(n)));
      if (m) {
	vpi_printf ("  [by %s:=%c%s]", prs_nodename (p,m), 
		prs_nodechar (prs_nodeval (m)),
		seu ? " *seu*" : "");
      }
      vpi_printf ("\n");
      vpi_flush ();
   }
  }
  if (debug_level > 1) {
    vpi_printf ("=== _run_prsim called @ %llu, prsid %d, run status = %d\n", vcstime, prs_to_handle (p), ran_once);
  }
}


PLI_INT32 prsim_callback (s_cb_data *p)
{
  PrsNode *n;
  Time_t vcstime, prsdiff;
  int bpcount = 0;
  int gap, i;
  Prs *prs;

  n = (PrsNode *)p->user_data;
  prs = PNI(n)->p;
  
  Assert (prs, "What?");

#if 0
  vpi_printf ("signal %s changed @ time %d, val = %d\n",
	      prs_nodename (prs, n),
	      p->time->low,
	      p->value->value.scalar);
#endif

  /* convert my time to prsim time */
  vcs_to_prstime (p->time, &vcstime);
  DEBUGFN(vcstime);

  if (debug_level > 1) {
     vpi_printf ("prsim_callback @ %llu. %s changed to %d\n",
                 vcstime, prs_nodename (prs, n), p->value->value.scalar);
  }

#if 0
  vpi_printf ("TIME: %d\n", (unsigned long)vcstime);
#endif
  switch (p->value->value.scalar) {
  case vpi0:
    prs_set_nodetime (prs, n, PRS_VAL_F, vcstime);
    break;

  case vpi1:
    prs_set_nodetime (prs, n, PRS_VAL_T, vcstime);
    break;

  case vpiZ:
    /* nothing */
    break;
    
  case vpiX:
    prs_set_nodetime (prs, n, PRS_VAL_X, vcstime);
    break;
    
  default:
    fatal_error ("Unknown value!");
    break;
  }
#if 0
  vpi_printf ("*** [?] vcstime: %llu, running prsim.\n", vcstime);
  vpi_flush ();
#endif

  vcstime = my_vcs_current_time ();
  _align_prs_simulation_times (vcstime, prs);
  if (debug_level > 2) {
    vpi_printf ("*** DONE. Registering callback, current time = %llu\n", vcstime);
    vpi_flush ();
  }
  register_self_callback (vcstime);
}    

/*
  Register a VCS-driven node with VCS and prsim
*/
void register_to_prsim (char *vcs_name, vpiHandle scope, 
			char *prsim_name, Prs *p)
{
  s_cb_data cb_data;
  vpiHandle net;
  PrsNode *n;
  s_vpi_value val;
  Time_t vcstime;
  
  Assert (p, "No prs file loaded!");

  vcs_name = skip_white (vcs_name);
  prsim_name = skip_white (prsim_name);

#ifdef DEBUG_TO_PRSIM
  vpi_printf ("[Handle %x] Register to prsim: vcs %s -> prs %s\n", p, vcs_name, prsim_name);
#endif

  cb_data.reason = cbValueChange;
  cb_data.cb_rtn = prsim_callback;

  if (vcs_name[0] == '.') {
    vcs_name++;
  }
  else {
    scope = NULL;
  }

  net = vpi_handle_by_name (vcs_name, scope);
  if (!net) {
    fatal_error ("Net name `%s' not found in .v file", vcs_name);
  }
  cb_data.obj = net;

  cb_data.time = (p_vpi_time) malloc (sizeof (s_vpi_time));
  cb_data.time->type = vpiSimTime;

  cb_data.value = (p_vpi_value) malloc (sizeof(s_vpi_value));
  cb_data.value->format = vpiScalarVal;

  n = prs_node (p, prsim_name);
  if (!n) {
    fatal_error ("Node `%s' not found in .prs file!", prsim_name);
  }
  /* ick: workaround; use this pointer */
  if (PNI(n) && PNI(n)->p) {
    fatal_error ("Node `%s' already has an association with a vcs name",
		 prsim_name);
  }
  if (!PNI(n)) {
    NEW (n->space, struct prs_nodeinfo);
  }
  PNI(n)->p = p;
#if 0
  n->inVector = p;
#endif

  /* take current vcs value and propagate it to prs */
  val.format = vpiScalarVal;
  vpi_get_value (net, &val);

  if (val.format == vpiScalarVal) {
    switch (val.value.scalar) {
    case vpi0:
      prs_set_node (p, n, PRS_VAL_F);
    break;
    
  case vpi1:
    prs_set_node (p, n, PRS_VAL_T);
    break;

  case vpiZ:
    /* nothing */
    break;

  case vpiX:
    prs_set_node (p, n, PRS_VAL_X);
    break;

  default:
    vpi_printf ("$to_prsim: vcs %s -> prs %s, init scalar value unknown (%d)\n", p, vcs_name, prsim_name, val.value.scalar);
    /* nothing: node stays at X */
    break;
    }
  }
  else {
    vpi_printf ("$to_prsim: vcs %s -> prs %s, init value of unknown format (%d)\n", p, vcs_name, prsim_name, val.format);
  }
  /* prsim net name */
  cb_data.user_data = (void *)n;
  vpi_register_cb (&cb_data);

  vcstime = my_vcs_current_time ();
  _align_prs_simulation_times (vcstime, p);
  if (debug_level > 2) {
    vpi_printf ("*** DONE. Registering callback, current time = %llu\n", vcstime);
    vpi_flush ();
  }
  register_self_callback (vcstime);
}


static void register_to_prsim_vec (char *vcs_name, vpiHandle scope, int v1, int v2,
				   char *prsim_name, Prs *p, int p1, int p2)
{
  int idx1, idx2;
  int d1, d2;
  char *buf1, *buf2;

  if (v1 > v2) { 
    idx1 = -1;
    d1 = v1-v2+1;
  }
  else {
    idx1 = 1;
    d1 = v2-v1+1;
  }
  if (p1 > p2) {
    idx2 = -1;
    d2 = p1-p2+1;
  }
  else {
    idx2 = 1;
    d2 = p2-p1+1;
  }
  if (d1 != d2) {
    vpi_printf ("$to_prsim_vec: number of elements in VCS and PRSIM vectors must match\n");
    return;
  }
  
  buf1 = (char *)malloc (strlen (vcs_name) + 32);
  buf2 = (char *)malloc (strlen (prsim_name) + 32);

  if (!buf1 || !buf2) {
    vpi_printf ("Could not allocate memory!\n");
    return;
  }
  
  do {
    sprintf (buf1, "%s[%d]", vcs_name, v1);
    sprintf (buf2, "%s[%d]", prsim_name, p1);
    register_to_prsim (buf1, scope, buf2, p);
    v1 = v1 + idx1;
    p1 = p1 + idx2;
  } while (v1 != v2+idx1);

  free (buf1);
  free (buf2);
  return;
}


/*
  Register a prsim-driven node with VCS and prsim
*/
void register_from_prsim (char *vcs_name, vpiHandle scope,
			  char *prsim_name, Prs *p)
{
  vpiHandle net;
  PrsNode *n;
  s_vpi_value val;
  struct prs_nodeinfo *pn;

  Assert (p, "No prs file loaded!");

  vcs_name = skip_white (vcs_name);
  prsim_name = skip_white (prsim_name);

#ifdef DEBUG_FROM_PRSIM
  vpi_printf ("[Handle %x] Register from prsim: vcs %s <- prs %s\n", p, vcs_name, prsim_name);
#endif

  if (vcs_name[0] == '.') {
    vcs_name++;
  }
  else {
    scope = NULL;
  }

  /* this is the handle to the verilog net name */
  net = vpi_handle_by_name (vcs_name, scope);
  if (!net) {
    fatal_error ("Net name `%s' not found in .v file", vcs_name);
  }

  n = prs_node (p, prsim_name);
  if (!n) {
    fatal_error ("Node `%s' not found in .prs file!", prsim_name);
  }
  if (PNI(n) && PNI(n)->p) {
    fatal_error ("Node `%s' already has an association with a vcs name",
		 prsim_name);
  }

  /* propagate prsim value to vcs */
  val.format = vpiScalarVal;
  switch (n->val) {
  case PRS_VAL_T:
    val.value.scalar = vpi1;
    break;
    
  case PRS_VAL_F:
    val.value.scalar = vpi0;
    break;

  case PRS_VAL_X:
    val.value.scalar = vpiX;
    break;

  default:
    fatal_error ("Unknown value");
    break;
  }
  vpi_free_object (vpi_put_value (net, &val, NULL, vpiNoDelay));

  /* set a breakpoint! */
  prs_set_bp (p, n);

#if 0
  vpi_printf ("Net %s is %x\n", prs_nodename (n), net);
#endif

  /* set up correspondence */
  if (!n->space) {
    NEW (n->space, struct prs_nodeinfo);
  }
  pn = PNI(n);

  pn->p = p;
  pn->net = net;

#if 0
  n->inVector = p;
  n->space = (void *)net;
#endif
}


static void register_from_prsim_vec (char *vcs_name, vpiHandle scope, int v1, int v2,
				     char *prsim_name, Prs *p, int p1, int p2)
{
  int idx1, idx2;
  int d1, d2;
  char *buf1, *buf2;

  if (v1 > v2) { 
    idx1 = -1;
    d1 = v1-v2+1;
  }
  else {
    idx1 = 1;
    d1 = v2-v1+1;
  }
  if (p1 > p2) {
    idx2 = -1;
    d2 = p1-p2+1;
  }
  else {
    idx2 = 1;
    d2 = p2-p1+1;
  }
  if (d1 != d2) {
    vpi_printf ("$to_prsim_vec: number of elements in VCS and PRSIM vectors must match\n");
    return;
  }
  
  buf1 = (char *)malloc (strlen (vcs_name) + 32);
  buf2 = (char *)malloc (strlen (prsim_name) + 32);

  if (!buf1 || !buf2) {
    vpi_printf ("Could not allocate memory!\n");
    return;
  }
  
  do {
    sprintf (buf1, "%s[%d]", vcs_name, v1);
    sprintf (buf2, "%s[%d]", prsim_name, p1);
    register_from_prsim (buf1, scope, buf2, p);
    v1 = v1 + idx1;
    p1 = p1 + idx2;
  } while (v1 != v2+idx1);

  free (buf1);
  free (buf2);
  return;
}

static void watchall_helper (PrsNode *n, void *cookie)
{
  Prs *p = (Prs *)cookie;
  struct prs_nodeinfo *pn;

  if (n->bp) {
    /* node already exists in vcs OR already watched; skip */
    return;
  }
  prs_set_bp (p, n);

  if (!n->space) {
    NEW (n->space, struct prs_nodeinfo);
  }
  pn = (struct prs_nodeinfo *)n->space;

  pn->p = p;
  pn->net = NULL;

#if 0
  n->inVector = p;
  n->space = NULL;
#endif
}



/*
  Register a prsim-driven node watch-point
*/
void register_prsim_watchpoint (char *prsim_name, Prs *p)
{
  vpiHandle net;
  PrsNode *n;
  struct prs_nodeinfo *pn;

  Assert (p, "No prs file loaded!");

  prsim_name = skip_white (prsim_name);

  n = prs_node (p, prsim_name);
  if (!n) {
    fatal_error ("Node `%s' not found in .prs file!", prsim_name);
  }

  if (n->bp && PNI(n) && PNI(n)->net) {
    fatal_error ("Cannot watch node `%s'; it already exists in vcs\n");
  }

  /* set a breakpoint! */
  prs_set_bp (p, n);

#if 0
  vpi_printf ("Net %s is %x\n", prs_nodename (n), net);
#endif
  if (!n->space) {
    NEW (n->space, struct prs_nodeinfo);
  }
  pn = (struct prs_nodeinfo *)n->space;

  pn->p = p;
  pn->net = NULL;
  
#if 0
  n->inVector = p;
  n->space = NULL;
#endif
}


/*
  The handle should be a vpi_iterate handle 
  Returns 0 on fail, 1 on success
*/
static int pli_get_intarg (vpiHandle h, int *ival)
{
  vpiHandle ih;
  s_vpi_value arg;

  ih = vpi_scan (h);
  if (!ih) {
    return 0;
  }
  arg.format = vpiIntVal;
  vpi_get_value (ih, &arg);
  *ival = arg.value.integer;
  return 1;
}

static Prs *pli_get_optprs (vpiHandle h)
{
  vpiHandle ih;
  s_vpi_value arg;

  ih = vpi_scan (h);
  if (!ih) {
    return curP;
  }
  else {
    arg.format = vpiIntVal;
    vpi_get_value (ih, &arg);
    if (arg.value.integer >= 0 && arg.value.integer < used_prsim) {
      return P[arg.value.integer];
    }
    else {
      return NULL;
    }
  }
}

static int pli_get_strarg (vpiHandle h, char **s)
{
  vpiHandle ih;
  s_vpi_value arg;

  *s = NULL;
  ih = vpi_scan (h);
  if (!ih) {
    return 0;
  }
  arg.format = vpiStringVal;
  vpi_get_value (ih, &arg);
  *s = strdup (arg.value.str);
  return 1;
}


PLI_INT32 to_prsim (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle call_scope;
  vpiHandle h;
  char *arg1, *arg2;
  Prs *p;

  task_call = vpi_handle (vpiSysTfCall, NULL);

  call_scope = vpi_handle (vpiScope, task_call);
  if (!call_scope) {
    vpi_printf ("Could not find current scope?\n");
    return 0;
  }

  h = vpi_iterate (vpiArgument, task_call);

  if (!pli_get_strarg (h, &arg1)) {
    vpi_printf ("Usage: $to_prsim(vcs-name, prsim-name, handle)\n");
    return 0;
  }

  if (!pli_get_strarg (h, &arg2)) {
    vpi_printf ("Usage: $to_prsim(vcs-name, prsim-name, handle)\n");
    return 0;
  }

#if 0
  vpi_printf ("setup %s (vcs) -> %s (prsim)\n", arg1, arg2);
#endif

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$to_prsim: invalid handle\n");
    return 0;
  }

  register_to_prsim (arg1, call_scope, arg2, p);

  return 1;
}


/*
  Vector
*/
PLI_INT32 to_prsim_vec (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle call_scope;
  vpiHandle h;
  char *arg1, *arg2;
  Prs *p;
  int v1, v2;
  int p1, p2;
  s_vpi_value argnum;

#define USAGE								\
  do {									\
    vpi_printf ("Usage: $to_prsim_vec(vcs-name, idx-start, idx-end, prsim-name, idx-start, idx-end, handle)\n"); \
    return 0;								\
} while (0)

  task_call = vpi_handle (vpiSysTfCall, NULL);

  call_scope = vpi_handle (vpiScope, task_call);
  if (!call_scope) {
    vpi_printf ("Could not find current scope?\n");
    return 0;
  }

  h = vpi_iterate (vpiArgument, task_call);


  if (!pli_get_strarg (h, &arg1)) {
    USAGE;
  }

  if (!pli_get_intarg (h, &v1)) {
    USAGE;
  }

  if (!pli_get_intarg (h, &v2)) {
    USAGE;
  }

  if (!pli_get_strarg (h, &arg2)) {
    USAGE;
  }

  if (!pli_get_intarg (h, &p1)) {
    USAGE;
  }

  if (!pli_get_intarg (h, &p2)) {
    USAGE;
  }

#if 0
  vpi_printf ("setup %s (vcs) -> %s (prsim)\n", arg1, arg2);
#endif

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$to_prsim_vec: invalid handle\n");
    return 0;
  }

  register_to_prsim_vec (arg1, call_scope, v1, v2,
			 arg2, p, p1, p2);

  return 1;
#undef USAGE
}


PLI_INT32 from_prsim (PLI_BYTE8 *args)
{
  vpiHandle task_call, call_scope;
  vpiHandle h;
  char *arg1, *arg2;
  Prs *p;

  task_call = vpi_handle (vpiSysTfCall, NULL);

  call_scope = vpi_handle (vpiScope, task_call);
  if (!call_scope) {
    vpi_printf ("Could not find current scope?\n");
    return 0;
  }

  h = vpi_iterate (vpiArgument, task_call);

  if (!pli_get_strarg (h, &arg1)) {
    vpi_printf ("Usage: $from_prsim(prsim-name,vcs-name,handle)\n");
    return 0;
  }

  if (!pli_get_strarg (h, &arg2)) {
    vpi_printf ("Usage: $from_prsim(prsim-name,vcs-name,handle)\n");
    return 0;
  }

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$from_prsim: invalid handle\n");
    return 0;
  }

  register_from_prsim (arg2, call_scope, arg1, p);

  return 1;
}

PLI_INT32 from_prsim_vec (PLI_BYTE8 *args)
{
  vpiHandle task_call, call_scope;
  vpiHandle h;
  char *arg1, *arg2;
  Prs *p;
  int v1, v2;
  int p1, p2;

#define USAGE								\
  do {									\
    vpi_printf ("Usage: $from_prsim_vec(prsim-name, idx-start, idx-end, vcs-name, idx-start, idx-end, handle)\n"); \
    return 0;								\
} while (0)

  task_call = vpi_handle (vpiSysTfCall, NULL);

  call_scope = vpi_handle (vpiScope, task_call);
  if (!call_scope) {
    vpi_printf ("Could not find current scope?\n");
    return 0;
  }

  h = vpi_iterate (vpiArgument, task_call);

  if (!pli_get_strarg (h, &arg1)) {
    USAGE;
  }

  if (!pli_get_intarg (h, &p1)) {
    USAGE;
  }

  if (!pli_get_intarg (h, &p2)) {
    USAGE;
  }

  if (!pli_get_strarg (h, &arg2)) {
    USAGE;
  }

  if (!pli_get_intarg (h, &v1)) {
    USAGE;
  }

  if (!pli_get_intarg (h, &v2)) {
    USAGE;
  }

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$from_prsim_vec: invalid handle\n");
    return 0;
  }

  register_from_prsim_vec (arg2, call_scope, v1, v2, arg1, p, p1, p2);

  return 1;
#undef USAGE
}

PLI_INT32 prsim_watchall (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  char *arg;
  Prs *p;

  task_call = vpi_handle (vpiSysTfCall, NULL);

  h = vpi_iterate (vpiArgument, task_call);

#define USAGE						\
  do {							\
    vpi_printf ("Usage: $prsim_watch(handle)\n");	\
    return 0;						\
} while (0)

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$prsim_watch: invalid handle\n");
  }

  prs_apply (p, p, watchall_helper);

  return 1;
#undef USAGE
}


PLI_INT32 prsim_watch (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  char *arg;
  Prs *p;

  task_call = vpi_handle (vpiSysTfCall, NULL);

  h = vpi_iterate (vpiArgument, task_call);

#define USAGE								\
  do {									\
    vpi_printf ("Usage: $prsim_watch(prsim-name,handle)\n"); \
    return 0;								\
} while (0)

  if (!pli_get_strarg (h, &arg)) {
    USAGE;
  }

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$prsim_watch: invalid handle\n");
  }

  register_prsim_watchpoint (arg, p);

  return 1;
#undef USAGE
}

PLI_INT32 prsim_file (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  vpiHandle fname;
  s_vpi_value arg, arg2;
  int prs;

  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);
  fname = vpi_scan (h);
  if (!fname) {
    vpi_printf ("Usage: $prsim(filename)\n");
    return 0;
  }
  arg.format = vpiStringVal;
  vpi_get_value (fname, &arg);

  prs = new_prs ();

  if (fname = vpi_scan (h)) {
    /* return the handle */
    arg2.format = vpiIntVal;
    arg2.value.integer = prs;
  }

  if (fname && vpi_scan (h)) {
    vpi_printf ("Usage: $prsim(filename,handle)\n");
    delete_last_prs ();
    return 0;
  }

  /*
   * If there is an argument, then set it
   */
  if (fname) {
    vpi_free_object (vpi_put_value (fname, &arg2, NULL, vpiNoDelay));
  }
  
  P[prs] = prs_fopen (arg.value.str);
  curP = P[prs];
  curP->time = my_vcs_current_time ();

  return 0;
}

PLI_INT32 prsim_packfile (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  vpiHandle fname;
  s_vpi_value arg, arg2;
  char *s;
  int prs;

  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);
  fname = vpi_scan (h);
  if (!fname) {
    vpi_printf ("Usage: $packprsim(filename,names,handle)\n");
    return 0;
  }
  arg.format = vpiStringVal;
  vpi_get_value (fname, &arg);
  s = strdup (arg.value.str);

  fname = vpi_scan (h);
  if (!fname) {
    vpi_printf ("Usage: $packprsim(filename,names,handle)\n");
    return 0;
  }
  arg.format = vpiStringVal;
  vpi_get_value (fname, &arg);

  prs = new_prs ();

  if (fname = vpi_scan (h)) {
    /* return the handle */
    arg2.format = vpiIntVal;
    arg2.value.integer = prs;
  }

  if (fname && vpi_scan (h)) {
    vpi_printf ("Usage: $packprsim(filename,names,handle)\n");
    return 0;
  }

  if (fname) {
    vpi_free_object (vpi_put_value (fname, &arg2, NULL, vpiNoDelay));
  }
  
  P[prs] = prs_packfopen (s,arg.value.str);
  curP = P[prs];
  curP->time = my_vcs_current_time ();

  return 0;
}

PLI_INT32 prsim_random (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;

  int arg;
  Prs *p;

  if (!curP) {
    fatal_error ("Call prsim_mkrandom only after a prs file has been loaded");
  }
    
  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);

#define USAGE							\
   do {								\
    vpi_printf ("Usage: $prsim_mkrandom(1 or 0, handle)\n");	\
    return 0;							\
  } while (0)

  if (!pli_get_intarg (h, &arg)) {
    USAGE;
  }

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$prsim_mkrandom: invalid handle\n");
    return 0;
  }

  if (arg == 0) {
    p->flags &= ~(PRS_RANDOM_TIMING|PRS_RANDOM_TIMING_RANGE);
  }
  else {
    p->flags |= PRS_RANDOM_TIMING;
  }
  return 0;
#undef USAGE
}

PLI_INT32 prsim_random_seed (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  int arg;
  Prs *p;
    
  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);

#define USAGE							\
   do {							\
    vpi_printf ("Usage: $prsim_random_seed(seed,handle)\n");	\
    return 0;						\
  } while (0)

  if (!pli_get_intarg (h, &arg)) {
    USAGE;
  }
  
  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$prsim_random_seed: invalid handle\n");
    return 0;
  }
  p->seed = arg;
  return 0;
#undef USAGE
}


PLI_INT32 prsim_random_setrange (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  int min_d, max_d;
  Prs *p;

  if (!curP) {
    fatal_error ("Call prsim_random_setrange only after a prs file has been loaded");
  }
    
  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);

  if (!pli_get_intarg (h, &min_d)) {
    vpi_printf ("Usage: $prsim_random_setrange(min, max, handle)\n");
    return 0;
  }

  if (!pli_get_intarg (h, &max_d)) {
    vpi_printf ("Usage: $prsim_random_setrange(min, max, handle)\n");
    return 0;
  }

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$prsim_random_setrange: invalid handle\n");
    return 0;
  }

  if (min_d < 1) { 
    min_d = 1;
    vpi_printf ("Min-delay less than one; resetting.\n");
  }
  if (min_d > max_d) {
    min_d = max_d;
    vpi_printf ("Min-delay larger than max delay; resetting.\n");
  }
    
  p->flags |= PRS_RANDOM_TIMING | PRS_RANDOM_TIMING_RANGE;
  p->min_delay = min_d;
  p->max_delay = max_d;

  return 0;
}


PLI_INT32 prsim_resetmode (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  vpiHandle fname;
  s_vpi_value arg, arg2;
  Prs *p;

  if (!curP) {
    fatal_error ("Call prsim_resetmode only after a prs file has been loaded");
  }
    
  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);
  fname = vpi_scan (h);
  if (!fname) {
    vpi_printf ("Usage: $prsim_resetmode(1 or 0, handle)\n");
    return 0;
  }
  arg.format = vpiIntVal;
  vpi_get_value (fname, &arg);

  if (fname = vpi_scan (h)) {
    arg2.format = vpiIntVal;
    vpi_get_value (fname, &arg2);
    if (arg2.value.integer >= 0 && arg2.value.integer < used_prsim) {
      p = P[arg2.value.integer];
    }
    else {
      p = NULL;
      vpi_printf ("$prsim_resetmode: invalid handle\n");
      return 0;
    }
  }
  else {
    p = curP;
  }

  if (arg.value.integer == 0) {
    p->flags &= ~PRS_NO_WEAK_INTERFERENCE;
  }
  else {
    p->flags |= PRS_NO_WEAK_INTERFERENCE;
  }
  return 0;
}

PLI_INT32 prsim_scale (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  vpiHandle fname;
  s_vpi_value arg, arg2;
  int handle;

  if (!curP) {
    fatal_error ("Call prsim_scale only after a prs file has been loaded");
  }
    
  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);
  fname = vpi_scan (h);
  if (!fname) {
    vpi_printf ("Usage: $prsim_scale(integer)\n");
    return 0;
  }
  arg.format = vpiIntVal;
  vpi_get_value (fname, &arg);
  if (arg.value.integer < 1) {
      vpi_printf ("$prsim_scale: invalid scale factor\n");
  }
  scale_prsim_time = arg.value.integer;

  return 0;
}

static int count_nodes = 0;
static Prs *nodeP = NULL;

static void check_nodeval (PrsNode *n, void *val)
{
  int v = (int)val;

  if (n->flag) return;

  n->flag = 1;

  if (n->val == v) {
    vpi_printf ("%s ", prs_nodename (nodeP,n));
    count_nodes++;
  }
}

static void clear_nodeflag (PrsNode *n, void *val)
{
  n->flag = 0;
}



PLI_INT32 prsim_status (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  vpiHandle fname;
  s_vpi_value arg, arg2;
  Prs *p;
  int val;

  if (!curP) {
    fatal_error ("Call prsim_status only after a prs file has been loaded");
  }
    
  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);
  fname = vpi_scan (h);
  if (!fname) {
    vpi_printf ("Usage: $prsim_status(integer,handle)\n");
    return 0;
  }
  arg.format = vpiIntVal;
  vpi_get_value (fname, &arg);

  switch (arg.value.integer) {
  case 0:
    val = PRS_VAL_F;
    break;
  case 1:
    val = PRS_VAL_T;
    break;
  case 2:
    val = PRS_VAL_X;
    break;
  default:
    vpi_printf ("$prsim_status: 0=false, 1=true, 2=X; provide valid integer\n");
    return 0;
    break;
  }

  if (fname = vpi_scan (h)) {
    arg.format = vpiIntVal;
    vpi_get_value (fname, &arg);
    if (arg.value.integer >= 0 && arg.value.integer < used_prsim) {
      p = P[arg.value.integer];
    }
    else {
      vpi_printf ("$prsim_status: invalid handle\n");
      return 0;
    }
  }
  else {
    p = curP;
  }
  
  count_nodes = 0;
  nodeP = p;
  vpi_printf ("Nodes with value %c: ", prs_nodechar (val));
  prs_apply (p, (void *)val, check_nodeval);
  prs_apply (p, (void *)NULL, clear_nodeflag);
  vpi_flush ();
  nodeP = NULL;
  if (count_nodes > 0) {
    vpi_printf ("\n");
    vpi_printf ("%d node%s found.\n", count_nodes, count_nodes > 1 ? "s" : "");
  }
  else {
    vpi_printf ("none.\n");
  }

  return 0;
}


PLI_INT32 prsim_num_events (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  vpiHandle fname, fname2;
  s_vpi_value arg, arg2;
  Prs *p;
  int val;

  if (!curP) {
    fatal_error ("Call prsim_num_event only after a prs file has been loaded");
  }
    
  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);
  fname = vpi_scan (h);
  if (!fname) {
    vpi_printf ("Usage: $prsim_num_events (integer-var,handle)\n");
    return 0;
  }
  arg.format = vpiIntVal;

  if (fname2 = vpi_scan (h)) {
    arg.format = vpiIntVal;
    vpi_get_value (fname2, &arg);
    if (arg.value.integer >= 0 && arg.value.integer < used_prsim) {
      p = P[arg.value.integer];
    }
    else {
      vpi_printf ("$prsim_num_events: invalid handle\n");
      return 0;
    }
  }
  else {
    p = curP;
  }

  if (!p->eventQueue) {
    val = 0;
  }
  else {
    val = p->eventQueue->sz;
  }
  arg.format = vpiIntVal;
  arg.value.integer = val;
  vpi_free_object (vpi_put_value (fname, &arg, NULL, vpiNoDelay));

  return 0;
}

PLI_INT32 prsim_dump_tc (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  char *filename;
  Prs *p;

#define USAGE							\
  do {								\
    vpi_printf ("Usage: $prsim_dump_tc(filename,handle)\n");	\
    return 0;							\
} while (0)

  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);

  if (!pli_get_strarg (h, &filename)) {
    USAGE;
  }

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$prsim_dump_tc: invalid handle\n");
    return 0;
  }
  prsim_cpp_dump_tc (filename, prs_to_handle (p));
  return 1;
#undef USAGE
}

PLI_INT32 prsim_dump_tcall (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  char *filename;
  Prs *p;

#define USAGE							\
  do {								\
    vpi_printf ("Usage: $prsim_dump_tcall(filename,handle)\n");	\
    return 0;							\
} while (0)

  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);

  if (!pli_get_strarg (h, &filename)) {
    USAGE;
  }

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$prsim_dump_tc: invalid handle\n");
    return 0;
  }
  prsim_cpp_dump_tcall (filename, prs_to_handle (p));
  return 1;
#undef USAGE
}

static void _int_prsim_dump_pending (Prs *p)
{
  if (!p->eventQueue || (heap_peek_min (p->eventQueue) == NULL)) {
    vpi_printf ("No pending events!\n");
  }
  else {
    PrsEvent *ev = (PrsEvent *)heap_peek_min (p->eventQueue);
    Time_t t = (heap_key_t)heap_peek_minkey (p->eventQueue);
    s_vpi_time tm;

    vpi_printf ("Next event: ", ev);
    vpi_printf ("%s := %c  @ ", prs_nodename (p, ev->n),
		prs_nodechar (ev->val));
    prs_to_vcstime (&tm, &t);
    if (tm.high) {
      vpi_printf ("(%d, %d)\n", tm.high, tm.low);
    }
    else {
      vpi_printf ("%d\n", tm.low);
    }
  }
}

PLI_INT32 prsim_dump_pending (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  vpiHandle fname, fname2;
  s_vpi_value arg, arg2;
  Prs *p;
  int val;

  if (!curP) {
    fatal_error ("Call prsim_dump_pending only after a prs file has been loaded");
  }
    
  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);
  if (fname2 = vpi_scan (h)) {
    arg.format = vpiIntVal;
    vpi_get_value (fname2, &arg);
    if (arg.value.integer >= 0 && arg.value.integer < used_prsim) {
      p = P[arg.value.integer];
    }
    else {
      vpi_printf ("$prsim_dump_pending: invalid handle\n");
      return 0;
    }
  }
  else {
    p = curP;
  }

  _int_prsim_dump_pending (p);

  return 0;
}


void prsim_cpp_save (char *file)
{
  char buf[10240];
  int i;
  FILE *fp;

  for (i=0; i < used_prsim; i++) {
    sprintf (buf, "%s_%d.pch", file, i);
    fp = fopen (buf, "w");
    if (!fp) {
      vpi_printf ("Could not open file `%s' for writing--aborting\n", buf);
      return;
    }
    prs_checkpoint (P[i], fp);
    fclose (fp);
  }
}

void prsim_cpp_restore (char *file)
{
  char buf[10240];
  int i;
  FILE *fp;

  for (i=0; i < used_prsim; i++) {
    sprintf (buf, "%s_%d.pch", file, i);
    fp = fopen (buf, "r");
    if (!fp) {
      vpi_printf ("Could not open file `%s' for reading--aborting\n", buf);
      return;
    }
    prs_restore (P[i], fp);
    fclose (fp);
  }
}



PLI_INT32 prsim_save (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  char *filename;

#define USAGE							\
  do {								\
    vpi_printf ("Usage: $prsim_save(filename)\n");		\
    return 0;							\
} while (0)

  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);

  if (!pli_get_strarg (h, &filename)) {
    USAGE;
  }

  prsim_cpp_save (filename);

  return 1;
#undef USAGE
}

PLI_INT32 prsim_restore (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  char *filename;

#define USAGE							\
  do {								\
    vpi_printf ("Usage: $prsim_restore(filename)\n");		\
    return 0;							\
} while (0)

  task_call = vpi_handle (vpiSysTfCall, NULL);
  h = vpi_iterate (vpiArgument, task_call);

  if (!pli_get_strarg (h, &filename)) {
    USAGE;
  }

  prsim_cpp_restore (filename);

  return 1;
#undef USAGE
}

static void _init_tracing (PrsNode *n, void *cookie)
{
  NEW (n->tracing, struct tracing_info);
  n->tracing->sz[0] = 0;
  n->tracing->sz[1] = 0;
  n->tracing->max[0] = 0;
  n->tracing->max[1] = 0;
}

void prsim_cpp_pairtc (int handle)
{
  Prs *p;

  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_watchall(): invalid handle\n");
  }
  Assert (p, "No prs file loaded!");

  prs_apply (p, NULL, _init_tracing);
}

PLI_INT32 prsim_pairtc (PLI_BYTE8 *args)
{
  vpiHandle task_call;
  vpiHandle h;
  char *arg;
  Prs *p;

  task_call = vpi_handle (vpiSysTfCall, NULL);

  h = vpi_iterate (vpiArgument, task_call);

#define USAGE						\
  do {							\
    vpi_printf ("Usage: $prsim_pairtc(handle)\n");	\
    return 0;						\
} while (0)

  if (!(p = pli_get_optprs (h))) {
    vpi_printf ("$prsim_pairtc: invalid handle\n");
  }

  prsim_cpp_pairtc (prs_to_handle (p));

  return 1;
#undef USAGE
}


struct funcs {
  char *name;
  PLI_INT32 (*f) (PLI_BYTE8 *);
  int ret;
};

static struct funcs f[] = {
  { "$to_prsim", to_prsim, 0 },
  { "$to_prsim_vec", to_prsim_vec, 0 },
  { "$from_prsim", from_prsim, 0 },
  { "$from_prsim_vec", from_prsim_vec, 0 },
  { "$prsim", prsim_file, 0 },
  { "$packprsim", prsim_packfile, 0 },
  { "$prsim_mkrandom", prsim_random, 0 },
  { "$prsim_random_seed", prsim_random_seed, 0 },
  { "$prsim_random_setrange", prsim_random_setrange, 0 },
  { "$prsim_resetmode", prsim_resetmode, 0 },
  { "$prsim_watch", prsim_watch, 0 },
  { "$prsim_scale", prsim_scale, 0 },
  { "$prsim_status", prsim_status, 0 },
  { "$prsim_num_events", prsim_num_events, 0 },
  { "$prsim_dump_pending", prsim_dump_pending, 0 },
  { "$prsim_watchall", prsim_watchall, 0 },
  { "$prsim_dump_tc", prsim_dump_tc, 0 },
  { "$prsim_dump_tcall", prsim_dump_tcall, 0 },
  { "$prsim_save", prsim_save, 0 },
  { "$prsim_restore", prsim_restore, 0 }
};
  
/*
  Register prsim tasks
*/
void register_prsim (void)
{
  s_vpi_systf_data s;
  int i;

#if 0
  vpi_printf ("Here!\n");
  return;
#endif
  /* register tasks */
  for (i=0; i < sizeof (f)/sizeof (f[0]); i++) {
    if (f[i].ret) {
      s.type = vpiSysFunc;
      s.sysfunctype = vpiIntFunc;
      s.tfname = f[i].name;
      s.calltf = f[i].f;
      s.compiletf = NULL;
      s.sizetf = NULL;
      s.user_data = NULL;
    }
    else {
      s.type = vpiSysTask;
      s.tfname = f[i].name;
      s.calltf = f[i].f;
      s.compiletf = NULL;
      s.sizetf = NULL;
      s.user_data = NULL;
    }
    vpi_register_systf (&s);
  }
}

#ifndef NO_VPI_OBJ
void (*vlog_startup_routines[]) () =
{
  register_prsim,
  NULL
};
#endif


void prsim_cpp_resetmode (int mode, int handle)
{
  Prs *p;
  
  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_resetmode(): invalid handle\n");
  }
  Assert (p, "No prs file loaded!");
  if (mode == 0) {
    p->flags &= ~PRS_NO_WEAK_INTERFERENCE;
  }
  else {
    p->flags |= PRS_NO_WEAK_INTERFERENCE;
  }
}

void prsim_cpp_random (int mode, int handle)
{
  Prs *p;
  
  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_resetmode(): invalid handle\n");
  }
  Assert (p, "No prs file loaded!");
  if (mode == 0) {
    p->flags &= ~(PRS_RANDOM_TIMING|PRS_RANDOM_TIMING_RANGE);
  }
  else {
    p->flags |= PRS_RANDOM_TIMING;
  }
}

void prsim_cpp_random_seed (int arg, int handle)
{
  Prs *p;

  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_random_seed(): invalid handle\n");
  }
  Assert (p, "No prs file loaded!");
  p->seed = arg;
}

void prsim_cpp_random_setrange (int min_d, int max_d, int handle)
{
  Prs *p;
  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_random_setrange(): invalid handle\n");
  }
  if (min_d < 1) { 
    min_d = 1;
    vpi_printf ("Min-delay less than one; resetting.\n");
  }
  if (min_d > max_d) {
    min_d = max_d;
    vpi_printf ("Min-delay larger than max delay; resetting.\n");
  }
  p->flags |= PRS_RANDOM_TIMING | PRS_RANDOM_TIMING_RANGE;
  p->min_delay = min_d;
  p->max_delay = max_d;
}



void prsim_cpp_watch (char *signame, int handle)
{
  Prs *p;

  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_resetmode(): invalid handle\n");
  }
  Assert (p, "No prs file loaded!");

  register_prsim_watchpoint (signame, p);
}

void prsim_cpp_watchall (int handle)
{
  Prs *p;

  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_watchall(): invalid handle\n");
  }
  Assert (p, "No prs file loaded!");

  prs_apply (p, p, watchall_helper);
}

void prsim_cpp_status (int mode, int handle)
{
  Prs *p;
  int val;

  switch (mode) {
  case 0:
    val = PRS_VAL_F;
    break;
  case 1:
    val = PRS_VAL_T;
    break;
  case 2:
    val = PRS_VAL_X;
    break;
  default:
    vpi_printf ("prsim_cpp_status(): 0=false, 1=true, 2=X; provide valid integer\n");
    return;
    break;
  }
  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_resetmode(): invalid handle\n");
  }
  Assert (p, "No prs file loaded!");

  count_nodes = 0;
  nodeP = p;
  vpi_printf ("Nodes with value %c: ", prs_nodechar (val));
  prs_apply (p, (void *)val, check_nodeval);
  prs_apply (p, (void *)NULL, clear_nodeflag);
  vpi_flush ();
  nodeP = NULL;
  if (count_nodes > 0) {
    vpi_printf ("\n");
    vpi_printf ("%d node%s found.\n", count_nodes, count_nodes > 1 ? "s" : "");
  }
  else {
    vpi_printf ("none.\n");
  }
}

int prsim_cpp_num_events (int handle)
{
  Prs *p;

  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_num_events(): invalid handle\n");
  }
  Assert (p, "No prs file loaded");
  if (!p->eventQueue) {
    return 0;
  }
  else {
    return (int)p->eventQueue->sz;
  }
}

void prsim_cpp_debug_level (int lev)
{
  debug_level = lev;
}

void prsim_cpp_dump_pending (int handle)
{
  Prs *p;
  
  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_cpp_dump_pending(): invalid handle\n");
  }
  Assert (p, "No prs file loaded");
  
  if (!p->eventQueue || (heap_peek_min (p->eventQueue) == NULL)) {
    vpi_printf ("No pending events!\n");
  }
  else {
    PrsEvent *ev = (PrsEvent *)heap_peek_min (p->eventQueue);
    Time_t t = (heap_key_t)heap_peek_minkey (p->eventQueue);
    s_vpi_time tm;

    vpi_printf ("Next event: ", ev);
    vpi_printf ("%s := %c  @ ", prs_nodename (p, ev->n),
		prs_nodechar (ev->val));
    prs_to_vcstime (&tm, &t);
    if (tm.high) {
      vpi_printf ("(%d, %d)\n", tm.high, tm.low);
    }
    else {
      vpi_printf ("%d\n", tm.low);
    }
  }
}

struct counting_struct {
  int total;
  int num_zero;
  int num_one;
  int num_two;
  int num_more;
};

static void count_node_tc (PrsNode *n, void *v)
{
  struct counting_struct *ct = (struct counting_struct *)v;
  ct->total++;

  if (n->tc == 0)  {
    ct->num_zero++;
  }
  else if (n->tc == 1) {
    ct->num_one++;
  }
  else if (n->tc == 2) {
    ct->num_two++;
  }
  else {
    ct->num_more++;
  }
}

static Prs *dump_prs = NULL;

static void dump_zero_one_two (PrsNode *n, void *v)
{
  FILE *fp = (FILE *)v;
  
  if (n->tc == 0) {
    fprintf (fp, "0: %s\n", prs_nodename (dump_prs, n));
  }
  else if (n->tc == 1) {
    fprintf (fp, "1: %s\n", prs_nodename (dump_prs, n));
  }
  else if (n->tc == 2) {
    fprintf (fp, "2: %s\n", prs_nodename (dump_prs, n));
  }
}

static void dump_all (PrsNode *n, void *v)
{
  FILE *fp = (FILE *)v;
  
  fprintf (fp, "%d: %s\n", n->tc, prs_nodename (dump_prs, n));
  if (n->tracing) {
    struct tracing_info *t;
    int i;

    t = n->tracing;

#define DUMPTRACE(idx,prefix)						\
    for (i=0; i < t->sz[idx]; i++) {					\
      fprintf (fp, "  " prefix " %s %lu\n", prs_nodename (dump_prs, t->in[idx][i]), t->count[idx][i]); \
    }
    DUMPTRACE(1,"[up]");
    DUMPTRACE(0,"[dn]");
  }
#undef DUMPTRACE
}

    
void prsim_cpp_dump_tc (char *filename, int handle)
{
  Prs *p;
  FILE *fp;
  struct counting_struct ct;

  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_dump_tc: invalid handle\n");
  }
  Assert (p, "No prs file loaded!");

  fp = fopen (filename, "a");
  if (!fp) {
    vpi_printf ("Could not open file `%s' for writing\n", filename);
    return;
  }
  fprintf (fp, "**\n");
  fprintf (fp, "** Log for handle %d @ time %llu\n", handle, p->time);
  fprintf (fp, "**\n");
  
  ct.total = 0;
  ct.num_zero = 0;
  ct.num_one = 0;
  ct.num_two = 0;
  ct.num_more = 0;

  prs_apply (p, &ct, count_node_tc);

  fprintf (fp, "Total nodes = %d\n", ct.total);
  if (ct.total > 0) {
    fprintf (fp, "Transition count breakdown:\n");
    fprintf (fp, " zero : %d (%5.2f%%)\n", ct.num_zero,
	     (0.0 + ct.num_zero)/(ct.total + 0.0)*100.0);
    fprintf (fp, "  one : %d (%5.2f%%)\n", ct.num_one,
	     (0.0 + ct.num_one)/(ct.total + 0.0)*100.0);
    fprintf (fp, "  two : %d (%5.2f%%)\n", ct.num_two,
	     (0.0 + ct.num_two)/(ct.total + 0.0)*100.0);
    fprintf (fp, " more : %d (%5.2f%%)\n", ct.num_more,
	     (0.0 + ct.num_more)/(ct.total + 0.0)*100.0);

    dump_prs = p;
    prs_apply (p, fp, dump_zero_one_two);
    dump_prs = NULL;
  }

  fprintf (fp, "**\n");
  fprintf (fp, "** end log for handle %d **\n", handle);
  fprintf (fp, "**\n\n");
  fclose (fp);
}

void prsim_cpp_dump_tcall (char *filename, int handle)
{
  Prs *p;
  FILE *fp;
  struct counting_struct ct;

  p = handle_to_prs (handle);
  if (!p) {
    vpi_printf ("prsim_dump_tc: invalid handle\n");
  }
  Assert (p, "No prs file loaded!");

  fp = fopen (filename, "a");
  if (!fp) {
    vpi_printf ("Could not open file `%s' for writing\n", filename);
    return;
  }
  fprintf (fp, "**\n");
  fprintf (fp, "** Log for handle %d @ time %llu\n", handle, p->time);
  fprintf (fp, "**\n");
  
  ct.total = 0;
  ct.num_zero = 0;
  ct.num_one = 0;
  ct.num_two = 0;
  ct.num_more = 0;

  prs_apply (p, &ct, count_node_tc);

  fprintf (fp, "Total nodes = %d\n", ct.total);
  if (ct.total > 0) {
    fprintf (fp, "Transition count breakdown:\n");
    fprintf (fp, " zero : %d (%5.2f%%)\n", ct.num_zero,
	     (0.0 + ct.num_zero)/(ct.total + 0.0)*100.0);
    fprintf (fp, "  one : %d (%5.2f%%)\n", ct.num_one,
	     (0.0 + ct.num_one)/(ct.total + 0.0)*100.0);
    fprintf (fp, "  two : %d (%5.2f%%)\n", ct.num_two,
	     (0.0 + ct.num_two)/(ct.total + 0.0)*100.0);
    fprintf (fp, " more : %d (%5.2f%%)\n", ct.num_more,
	     (0.0 + ct.num_more)/(ct.total + 0.0)*100.0);

    dump_prs = p;
    prs_apply (p, fp, dump_all);
    dump_prs = NULL;
  }

  fprintf (fp, "**\n");
  fprintf (fp, "** end log for handle %d **\n", handle);
  fprintf (fp, "**\n\n");
  fclose (fp);
}
