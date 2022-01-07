/*************************************************************************
 *  
 *  Trace file library for circuit simulation.
 *
 *  Copyright (c) 2004, 2016-2019 Rajit Manohar
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
#ifndef __ATRACE_H__
#define __ATRACE_H__

#include <common/hash.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * "aspice" trace format
 *
 *   Item 0: 0xffff0000   [used to detect endianness issues]
 *   Item 1: int ["order"]
 *       0 = original
 *       1 = reordered
 *       2 = changing
 *   Item 2: int timestamp
 *   Item 3: int # nodes
 *   Item 4: end_time [for any format other than 0, 1]
 *   Item 5: dt       [for any format other than 0, 1]
 *
 *   Values written out as follows:
 *
 *      - floats for analog signals and time
 *
 *      - digital/enum signals have a specified bit-width and are
 *      written out in integer format described next.
 *
 *      - channel values have a specified bit-width, and the internal
 *      width is one more than the data width (two more if the
 *      original channel had zero bitwidth). The value 0 is used to
 *      indicate a sender blocked, 1 for receiver blocked, and
 *      successful communication records 2 + data sent/received on the
 *      channel.
 *
 *   Integer format given a bit-width:
 *      If the bit-width is <= 32, the integer is written out as an
 *      int; if <= 64, it is written out as the lower 32 bits followed
 *      by the upper 32 bits. For widths > 64, the representation is
 *      multiple 64-bit values that are written out least-significant
 *      64-bit chunk first.
 *
 *
 *  FORMATS:
 *
 *  Original : [array of nodes dumped] per timestep
 *  Reordered: [dump of node for all time] per node
 *  Value Nsteps inferred from file size
 *
 *  For formats 0, 1 :  node 0 is special and is the time
 *
 *  Delta format:
 *     t <nodeid> <val> .... <nodek> <valk> -1
 *     t <nodeid> <val> .... <nodek> <valk> -1
 *     t -1
 *
 *  Delta format with cause:
 *     t <nodeid> <val> <cause-nodeid> ..... -1
 *     t -1
 *
 *  If the end marker is -2, then that means the next record is a full
 *  dump:
 *     t 0 <val> <val> .... <val> -1 or -2
 *
 *
 *  <file>.trace : contains the trace
 *  <file>.names : contains the names of all signals
 *
 */

#define ATRACE_FMT_MIN 0
#define ATRACE_TIME_ORDER  0
#define ATRACE_NODE_ORDER  1
#define ATRACE_CHANGING    2
#define ATRACE_DELTA       3
#define ATRACE_DELTA_CAUSE 4
#define ATRACE_FMT_MAX 4

#define ATRACE_FMT(x)   ((x) & 0xf)
#define ATRACE_ATTRIB(x)  (((x) >> 4) & 0xf)

#define ATRACE_MAX_FILE_SIZE 2140000000UL

    /* less than 2GB. Must be a multiple of (sizeof(int)) */

typedef union atrace_value {
    float v;			/* value */
    unsigned long val;		/* <= 63 bits for channels
				   lsb is 0 for data
				   lsb is 1 for other channel events
				*/
    unsigned long *valp;	/* > 63 bits */
} atrace_val_t;

typedef struct name_struct {
  struct name_struct *up;
  struct name_struct *next;
  hash_bucket_t *b;		/* name */
  union atrace_value vu;	/* value */
  int cause;			/* cause--read in for delta-cause fmt */
  int idx;			/* index */
  unsigned int chg:1;		/* changed? */
  unsigned int type:2;		/* 0 for analog, non-zero for other
				   1 = digital
				   2 = bit-channel
				   3 = enumeration (used for selects)
				*/
  unsigned int width;	        /* for digital/channel/sel: bitwidth */

  struct name_struct *chg_next;	/* change-list for reading */
} name_t;

typedef struct atrace_struct {
  struct Hashtable *H;		/* hash table of names */
  name_t **N;			/* indexed lookup */

  char *file;			/* file name (without suffix) */
  char *tfile;			/* trace file name */
  FILE *tr;			/* trace file */

  int endianness;		/* 1 if you have to swap endianness on
				   reads */

  unsigned int locked:1;	/* if locked, can't add nodes/aliases! */
  unsigned int read_mode:1;	/* 1 = read, 0 = write */

  int fmt;			/* format of trace */

  int Nnodes;			/* # of nodes 
				   (at least 1, since time is a node) */
  int Nsteps;			/* # of steps */

  int timestamp;		/* no idea what this is for... */

  float stop_time;		/* max time in the trace */
  float dt;			/* delta t */

  float adv;			/* min change for recording */
  float rdv;			/* a=absolute, r=relative */

  /* only used for reading */
  int Nvsteps;			/* real step count */
  float vdt;			/* virtual delta t:
				   used for rescaling the
				   trace. Assumes that
				   vdt >= dt 
				*/

  /* internal state for new atrace API */
  int rec_type;			/* -1 or -2 depending on read type? */
  float curt;			/* record's current time */
  float nextt;			/* next time */
  int curstep;			/* current time step for API */

  /* internal */
  unsigned long curtime;  /* in ticks; this is the current update
			     cycle...
			     
			     when emitting in node order, this is the
			     time of the next update that is expected.
			     
			     when emitting in time order, this is the
			     time of the current update.
			  */

  name_t *nprev;

  unsigned long fpos;		/*  Current file position, consistent
				    after the header has been 
				    written/read. */
  unsigned long fend;		/* used for reading */
  int fnum;			/* file number! 
				   Position is really (fnum,fpos)
				*/

  unsigned int used:1;		/* used this ever? */

  int bufsz;			/* size */
  int bufpos;			/* current utilization */
  int *buffer;			/* i/o buffer */


  /* for reading incremental files */
  name_t *hd_chglist;

} atrace;


atrace *atrace_create (const char *s, int fmt, float stop_time, float dt);
  /* open an empty trace file
     fmt = trace format
     stop_time = time of last output
     dt = time resolution
  */

void atrace_filter (atrace *, float adv, float rdv);
  /* include a trace file filter. only works in create mode for DELTA format.
     adv = absolute delta v before a change is recorded
     rdv = relative delta v before a change is recorded
  */

atrace *atrace_open (const char *s);
  /* open a trace file for reading */

void atrace_rescale (atrace *, float vdt);
  /* rescale trace file with a new virtual time */

int atrace_header (atrace *, int *ts, int *Nnodes, int *Nsteps, int *fmt);
  /* returns the trace header.
     *Nsteps is computed, so if you have rescaled time, Nsteps will
     not correspond to the number of steps in the trace file.
   */


  /* this function must be called for each slot based on the name
     prior to passing the array to readall functions */
void atrace_alloc_val_entry (name_t *n, atrace_val_t *v);

  /* this must be called to release any storage allocated for a slot */
void atrace_free_val_entry (name_t *n, atrace_val_t *v);

void atrace_readall (atrace *, atrace_val_t *M);
  /* read all values into an array 
     node #i, step #j is at position M[Nsteps*i + j]
  */
void atrace_readall_free (atrace *, atrace_val_t *M);

void atrace_readall_xposed (atrace *, atrace_val_t *M);
  /* read all values into an array 
     node #i, step #j is at position M[Nnodes*j + i]
  */

void atrace_readall_block (atrace *, int start, int num, atrace_val_t *M);
  /* read "num" values into an array, starting at node number "start"
     node #i, step #j is at position M[Nsteps*i + j]
     i starts from 0 (corresponding to "start")
  */

void atrace_readall_nodenum (atrace *, int, atrace_val_t *M);
void atrace_readall_node (atrace *, name_t *, atrace_val_t *M);
  /* read node value over all time into array */

void atrace_readall_nodenum_c (atrace *, int, atrace_val_t *M, int *C);
void atrace_readall_node_c (atrace *, name_t *, atrace_val_t *M, int *C);
  /* Same as above, except C = cause array */


/* New atrace API:
     Advance current time by `nstep' steps
*/
void atrace_init_time (atrace *);
void atrace_advance_time (atrace *, int nstep);

#define ATRACE_NAME(a,n) ((a)->N[n])
#define ATRACE_GET_NAME(a,n) ATRACE_NAME(a,n)->b->key
#define ATRACE_GET_VAL(a,n)  ATRACE_NAME(a,n)->vu
#define ATRACE_GET_STEPSIZE(a)  ((a)->vdt)
#define ATRACE_NODE_FLOATVAL(a,n) (n)->vu.v
#define ATRACE_NODE_SMALLVAL(a,n) (n)->vu.val

/*
  Macros for channel values
*/
#define ATRACE_CHAN_SEND_BLOCKED 0
#define ATRACE_CHAN_RECV_BLOCKED 1
#define ATRACE_CHAN_VAL_TO_TRACE(v) (2+(v))
#define ATRACE_CHAN_TRACE_TO_VAL(v) ((v)-2)

void  atrace_signal_change_cause (atrace *, name_t *, float t, float v, name_t *);
void  atrace_general_change_cause (atrace *, name_t *, float t, atrace_val_t *v, name_t *);
  /* Record a signal change, with a cause for the change as well. We
     require that the "t" argument calls for this function are non-decreasing.
   */


#define atrace_signal_change(a,n,t,v) atrace_signal_change_cause ((a), (n), (t), (v), ((a)->N ? (a)->N[0] : NULL))
#define atrace_general_change(a,n,t,v) atrace_general_change_cause ((a), (n), (t), (v), ((a)->N ? (a)->N[0] : NULL))

name_t *atrace_create_node (atrace *, const char *);
  /* create a node; if exists, return old value */
void atrace_mk_digital (name_t *n);
void atrace_mk_analog (name_t *n);
void atrace_mk_channel (name_t *n);
void atrace_mk_enum (name_t *n);
void atrace_mk_width (name_t *n, int w);

name_t *atrace_lookup (atrace *, const char *);
  /* lookup a node, return NULL if not present */

void atrace_alias (atrace *, name_t *, name_t *);
  /* alias two nodes */

void atrace_flush (atrace *);

void atrace_close (atrace *);

#ifdef __cplusplus
}
#endif

#endif /* __ATRACE_H__ */
