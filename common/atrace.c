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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include "atrace.h"
#include "misc.h"

#define ENDIAN_SIGNATURE 0xffff0000
#define ENDIAN_SIGNATURE_SWIZZLED 0x0000ffff

/* only filter for analog signals */
#define DONT_FILTER_DELTAS(t)  (!((t) == 0))

#define ATRACE_IS_STREAM(a) ((a)->fd >= 0)


static atrace *_atrace_alloc (int read_mode)
{
  atrace *a;
  
  NEW (a, atrace);

  a->H = hash_new (32);
  a->N = NULL;
  a->file = NULL;
  a->tfile = NULL;
  a->tr = NULL;
  a->endianness = 0;
  a->fmt = 0;
  a->Nnodes = 0;
  a->Nsteps = 0;
  a->timestamp = 0;
  a->stop_time = 0;
  a->dt = -1;
  a->adv = 0;
  a->rdv = 0;
  a->Nvsteps = 0;
  a->vdt = -1;
  a->fnum = 0;
  a->fpos = 0;
  a->hd_chglist = NULL;
  a->set_members = 0;
  a->read_mode = read_mode;
  a->locked = 0;
  a->used = 0;
  a->bufsz = 0;
  a->bufpos = 0;
  a->buffer = NULL;
  a->fd = -1;
  a->sock = -1;
  a->nextt = -2;
  a->_last_ret_ts = 0;

  return a;
}

/* open an empty trace file */
atrace *atrace_create (const char *s, int fmt, float stop_time, float dt)
{
  atrace *a;
  int l;

  if (stop_time < 0 || fmt < ATRACE_FMT_MIN || ATRACE_FMT(fmt) > ATRACE_FMT_MAX) {
    return NULL;
  }

  a = _atrace_alloc (0 /* read mode = 0 */);

  l = strlen (s);
  MALLOC (a->file, char, l+1);
  strcpy (a->file, s);
  
  MALLOC (a->tfile, char, l + 7);
  sprintf (a->tfile, "%s.trace", a->file);

  a->tr = fopen (a->tfile, "w");
  if (!a->tr) {
    fprintf (stderr, "ERROR: Could not open trace file `%s.trace'\n", a->file);
    FREE (a->file);
    FREE (a->tfile);
    FREE (a);

    return NULL;
  }

  a->Nnodes = 1; /* time */
  a->dt = dt;
  a->fmt = fmt;
  a->stop_time = stop_time;
  a->Nsteps = 1 + (int)(stop_time / dt + 0.1);
  a->timestamp = time (NULL);
  a->curt = -1;

  /* used for signal recording in write mode */
  a->curtime = 0;
  a->nprev = NULL;

  a->bufsz = 10240;
  a->bufpos = 0;
  MALLOC (a->buffer, int, a->bufsz);
  a->fpos = 0;
  a->fnum = 0;
  a->used = 0;
  a->endianness = 0;

  a->adv = -1;
  a->rdv = -1;
  
  return a;
}

/* open an empty trace file */
atrace *atrace_create_stream (const char *host, int port,
			      int fmt, float stop_time, float dt)
{
  struct hostent *hp;
  atrace *a;

  if (stop_time < 0 || (fmt < ATRACE_FMT_MIN || ATRACE_FMT(fmt) > ATRACE_FMT_MAX || ATRACE_FMT(fmt) < ATRACE_DELTA)) {
    return NULL;
  }

  if ((hp = gethostbyname (host)) == NULL) {
    return NULL;
  }

  a = _atrace_alloc (0 /* read mode = 0 */);

  a->Nnodes = 1; /* time */
  a->dt = dt;
  a->fmt = fmt;
  a->stop_time = stop_time;
  a->Nsteps = 1 + (int)(stop_time / dt + 0.1);
  a->timestamp = time (NULL);
  a->curt = -1;

  /* used for signal recording in write mode */
  a->curtime = 0;
  a->nprev = NULL;

  a->bufsz = 10240;
  a->bufpos = 0;
  MALLOC (a->buffer, int, a->bufsz);
  a->fpos = 0;
  a->fnum = 0;
  a->used = 0;
  a->endianness = 0;

  a->adv = -1;
  a->rdv = -1;

  if ((a->fd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
    perror ("socket");
    hash_free (a->H);
    FREE (a->buffer);
    FREE (a);
    return NULL;
  }
  a->addr.sin_family = AF_INET;
  bcopy (hp->h_addr, &a->addr.sin_addr, sizeof (a->addr.sin_addr));
  a->addr.sin_port = htons (port);
  if (connect (a->fd, (struct sockaddr *)&a->addr, sizeof (a->addr)) < 0) {
    perror ("connect failed");
    close (a->fd);
    hash_free (a->H);
    FREE (a->buffer);
    FREE (a);
    return NULL;
  }
  a->tr = fdopen (a->fd, "w");
  
  return a;
}

void atrace_filter (atrace *a, float adv, float rdv)
{
  Assert (a->read_mode == 0, "Cannot set filter in read mode");

  if (rdv < 0 || rdv > 1) return;
  if (adv < 0) return;

  a->adv = adv;
  a->rdv = rdv;
}

/*
  Used to read the next int 
*/
static int safe_fread (atrace *a, void *x)
{
  long old;

 retry:
  if (ATRACE_IS_STREAM (a) || a->fpos < a->fend) {
    a->fpos += sizeof (int);
    return fread (x, sizeof (int), 1, a->tr);
  }
  Assert (!ATRACE_IS_STREAM (a), "What?");

  if (a->fend == ATRACE_MAX_FILE_SIZE) {
    char buf[10240];
    fclose (a->tr);

    if (ATRACE_FMT(a->fmt) == ATRACE_NODE_ORDER ||
	ATRACE_FMT(a->fmt) == ATRACE_TIME_ORDER) {
      fatal_error ("atrace format: time/node order cannot be larger than 2GB");
    }

    sprintf (buf, "%s_%d.trace", a->file, ++a->fnum);
    a->tr = fopen (buf, "rb");
    if (!a->tr) {
      fprintf (stderr, "fread failed for `%s'\n", buf);
      fclose (a->tr);
      exit (1);
    }
    a->fpos = 0;
    fseek (a->tr, 0, SEEK_END);
    a->fend = ftell (a->tr);
    fseek (a->tr, 0, SEEK_SET);
    goto retry;
  }

  old = ftell (a->tr);
  while (fread (x, sizeof (int), 1, a->tr) != 1) {
    fprintf (stderr, "fread @ %d failed for `%s', retrying in 60 seconds...\n", (int) old, a->file);
    fclose (a->tr);
    sleep (60);

    a->tr = fopen (a->tfile, "rb");
    if (!a->tr) {
      fprintf (stderr, "fread: could not reopen trace file...\n");
      exit  (1);
    }
    fseek (a->tr, 0, SEEK_END);
    a->fend = ftell (a->tr);
    fseek (a->tr, old, SEEK_SET);
  }
  a->fpos = old + sizeof (int);
  return 1;
}

static int swap_endian_int(int x)
  {
  int y=x;
  char *bx=(char *)(&x),*by=(char *)(&y);
  by[0]=bx[3];
  by[1]=bx[2];
  by[2]=bx[1];
  by[3]=bx[0];
  Assert(sizeof(int)==4, "Size error");
  return y;
  }

static float swap_endian_float(float x)
  {
  float y=x;
  char *bx=(char *)(&x),*by=(char *)(&y);
  by[0]=bx[3];
  by[1]=bx[2];
  by[2]=bx[1];
  by[3]=bx[0];
  Assert(sizeof(float)==4, "Size error");
  return y;
  }


static int fread_int (atrace *a, int *x)
{
  int ret = safe_fread (a, x);
  if (a->endianness) {
    *x = swap_endian_int (*x);
  }
  return ret;
}

static int fread_ulong (atrace *a, unsigned long *v)
{
  int ret;
  if (sizeof (unsigned long) == sizeof (int)) {
    ret = fread_int (a, (int*)v);
  }
  else if (sizeof (unsigned long) == 2*sizeof (int)) {
    unsigned int i0, i1;
    ret = fread_int (a, (int*)&i0);
    if (!ret) return ret;
    ret = fread_int (a, (int*)&i1);
    if (!ret) {
      if (a->fpos >= sizeof (int)) {
	a->fpos -= sizeof (int);
	fseek (a->tr, a->fpos, SEEK_SET);
	return ret;
      }
      else {
	fprintf (stderr, "fread_ulong: failed and split across files; trace file corrupted.");
	return 0;
      }
    }
    *v = (((unsigned long)i1) << 8*sizeof (unsigned int)) | i0;
    return ret;
  }
  else {
    fatal_error ("FIXME");
  }
}

static int fread_float (atrace *a, float *f)
{
  int ret = safe_fread (a, f);
  if (a->endianness) {
    *f = swap_endian_float (*f);
  }
  return ret;
}

#define ONE_WIDTH ATRACE_SHORT_WIDTH

static int fread_value (atrace *a, name_t *n, union atrace_value *v)
{
  int ret, i;
  v->v = 0;
  v->val = 0;
  v->valp = NULL;
  if (n->type == 0) {
    ret = fread_float (a, &v->v);
  }
  else if (n->width <= ONE_WIDTH) {
    if (n->width <= ONE_WIDTH/2) {
      ret = fread_int (a, &i);
      v->val = i;
    }
    else {
      ret = fread_ulong (a, &v->val);
    }
  }
  else {
    int count = (n->width + ONE_WIDTH - 1)/ONE_WIDTH;
    for (i=0; i < count; i++) {
      ret = fread_ulong (a, &v->valp[count]);
      if (ret == 0 && i == 0) {
	return ret;
      }
      else if (ret == 0 && !ATRACE_IS_STREAM (a)) {
	if (a->fpos >= sizeof (unsigned long)*i) {
	  a->fpos -= sizeof (unsigned long)*i;
	  fseek (a->tr, a->fpos, SEEK_SET);
	  return 0;
	}
	else {
	  fprintf (stderr, "fread_value: failed and split across files; trace file corrupted\n");
	  return 0;
	}
      }
    }
  }
  return ret;
}

/*
   ignoring time, amount of space needed for all the nodes
   upto but not including nodeval
*/
static int _space_for_one_entry (name_t *n)
{
  if (n->type == 0) {
    return sizeof (float);
  }
  else if (n->width <= ONE_WIDTH/2) {
    return sizeof (int);
  }
  else if (n->width <= ONE_WIDTH) {
    return sizeof (unsigned long);
  }
  else {
    int count = (n->width + ONE_WIDTH - 1)/ONE_WIDTH;
    return count * sizeof (unsigned long);
  }
}

static int _space_for_nodes_upto (atrace *a, int nodeval)
{
  int i;
  int space = 0;
  for (i=0; i < nodeval; i++) {
    space += _space_for_one_entry (a->N[i]);
  }
  return space;
}

static void seek_after_header (atrace *a, int offset)
{
  if (ATRACE_IS_STREAM (a)) {
    /* no seek operation for streams */
    return;
  }
  
  switch (ATRACE_FMT (a->fmt)) {
  case ATRACE_TIME_ORDER:
  case ATRACE_NODE_ORDER:
    fseek (a->tr, 4 * sizeof (int) + offset, SEEK_SET);
    a->fpos = 4*sizeof(int)+offset;
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    if (a->fnum != 0) {
      a->fnum = 0;
      fclose (a->tr);
      a->tr = fopen (a->tfile, "rb");
      if (!a->tr) {
	fatal_error ("Could not open trace file `%s'", a->tfile);
      }
      fseek (a->tr, 0, SEEK_END);
      a->fend = ftell (a->tr);
    }
    fseek (a->tr, 6 * sizeof (int) + offset, SEEK_SET);
    a->fpos = 6*sizeof(int)+offset;
    break;

  default:
    fatal_error ("Can't seek unless file is stable");
    break;
  }
}


/*
  Helper function: Read atrace header
*/
static void read_header (atrace *a)
{
  unsigned long n;
  int offset;
  int x;

  Assert (a->fnum == 0, "read_header: called on non-initial file");

  if (!ATRACE_IS_STREAM (a)) {
    fseek (a->tr, 0, SEEK_END);
    a->fend = ftell (a->tr);
    a->fpos = 0;
    a->endianness = 0;
  }

 retry:
  if (!ATRACE_IS_STREAM (a)) {
    fseek (a->tr, 0, SEEK_SET);
    a->endianness = 0;
  }
  if (fread_int (a, &x) != 1) {
    sleep (5);
    goto retry;
  }
  if (x == ENDIAN_SIGNATURE) {
    a->endianness = 0;
  }
  else {
    Assert (x == ENDIAN_SIGNATURE_SWIZZLED, "Weird endianness?!");
    a->endianness = 1;
  }
  a->fpos = 0;
  if (fread_int (a, &a->fmt) != 1) {
    sleep (5);
    goto retry;
  }

  if (ATRACE_FMT(a->fmt) == ATRACE_CHANGING) {
    /* going to block! */
    fprintf (stderr, "atrace: file `%s' in flux; retrying in 30 seconds...\n",
	     a->file);
    fclose (a->tr);

    sleep (30);

    a->tr = fopen (a->tfile, "rb");
    if (!a->tr) {
      fprintf (stderr, "atrace: could not reopen trace file!\n");
      exit (1);
    }
    goto retry;
  }

  if (fread_int (a, &a->timestamp) != 1) { sleep (5); goto retry; }
  if (fread_int (a, &a->Nnodes) != 1) { sleep (5); goto retry; }

  offset = 4 * sizeof (int);

  if (ATRACE_FMT(a->fmt) == ATRACE_TIME_ORDER || 
      ATRACE_FMT(a->fmt) == ATRACE_NODE_ORDER) {
    /* XXX: these formats cannot exceed 2GB */

    n = a->fend - offset;

    /* This is okay even if the file is truncated */
    a->Nsteps = n / _space_for_nodes_upto (a, a->Nnodes);

    /* get the stop-time from the file itself */
    if (ATRACE_FMT (a->fmt) == ATRACE_TIME_ORDER) {
      fseek (a->tr, 4 * sizeof (int) +
	     (a->Nsteps-1)*_space_for_nodes_upto (a, a->Nnodes), SEEK_SET);
    }
    else {
      fseek (a->tr, 4 * sizeof (int) +
	     (a->Nsteps-1)*_space_for_one_entry (a->N[0]), SEEK_SET);
    }
    fread_float (a, &a->stop_time);
    a->dt = a->stop_time/(a->Nsteps-1);

    a->Nvsteps = a->Nsteps;
    if (a->vdt > a->dt) {
      a->Nvsteps = 1 + (int) ((a->stop_time)/a->vdt);
    }
    else {
      a->vdt = a->dt;
    }
  }
  else {
    /* ALL other formats */
    if (fread_float (a, &a->stop_time) != 1) { sleep (5); goto retry; }
    if (fread_float (a, &a->dt) != 1) { sleep (5); goto retry; }

    a->Nsteps  = 1 + (a->stop_time/ a->dt + 0.01);

    a->Nvsteps = a->Nsteps;
    if (a->vdt > a->dt) {
      a->Nvsteps = 1 + (a->stop_time/a->vdt + 0.01);
    }
    else {
      a->vdt = a->dt;
    }
    offset += sizeof (float) * 2;
  }
  if (!ATRACE_IS_STREAM (a)) {
    fseek (a->tr, offset, SEEK_SET);
  }
  a->fpos = offset;
}


static void safe_fwrite (atrace *a, void *x)
{
  unsigned long old;

  if (ATRACE_IS_STREAM (a)) {
    if (fwrite (x, sizeof (int), 1, a->tr) != 1) {
      fprintf (stderr, "Write failed to stream...\n");
    }
    return;
  }

  /* speed this up! */
  old = ftell (a->tr);
  a->fpos = old;
  if (old == ATRACE_MAX_FILE_SIZE) {
    /* XXX: we need to fix this! */
    fatal_error ("FIXME");
  }
  while (fwrite (x, sizeof (int), 1, a->tr) != 1) {
    fprintf (stderr, "fwrite failed, retrying..\n");
    sleep (60);
    fseek (a->tr, old, SEEK_SET);
  }
  a->fpos += sizeof (int);
}

static void safe_fwrite_int (atrace *a, int x)
{
  if (a->endianness) {
    Assert (0, "What?!");
    x = swap_endian_int (x);
  }
  /* assume sizeof (int) == 4 */
  safe_fwrite (a, &x);
}

static void safe_fwrite_float (atrace *a, float f)
{
  if (a->endianness) {
    Assert (0, "What?!");
    f = swap_endian_float (f);
  }
  /* assume sizeof (float) == 4 */
  safe_fwrite (a, &f);
}

static void _write_out_buf (atrace *a)
{
  if (ATRACE_IS_STREAM (a)) {
    int amt = 0;
    int count = 0;
    while ((count = fwrite (a->buffer + amt, sizeof (int), a->bufpos - amt,
			    a->tr)) != (a->bufpos - amt)) {
      amt += count;
      sleep (1);
    }
  }
  else {
    while (fwrite (a->buffer, sizeof (int), a->bufpos, a->tr) != a->bufpos) {
      fprintf (stderr, "fwrite failed, retrying..\n");
      sleep (60);
      fseek (a->tr, a->fpos, SEEK_SET);
    }
  }
}

static void safe_fwrite_buf (atrace *a, void *x)
{
  if (!a->used) {
    a->used = 1;
    a->fpos = ftell (a->tr);
  }

  if (a->bufpos == a->bufsz || 
      (((a->bufpos*sizeof(int)) + a->fpos) == ATRACE_MAX_FILE_SIZE)) {
    Assert (a->fpos == ftell (a->tr), "Invariant violated");
    _write_out_buf (a);
    if (!ATRACE_IS_STREAM (a)) {
      a->fpos += sizeof (int) * a->bufpos;
      a->bufpos = 0;

      if (a->fpos == ATRACE_MAX_FILE_SIZE) {
	char buf[10240];
	fclose (a->tr);
	sprintf (buf, "%s_%d.trace", a->file, ++a->fnum);
	a->tr = fopen (buf, "w");
	if (!a->tr) {
	  fatal_error ("Could not open continuation trace file `%s'", buf);
	}
	a->fpos = 0;
      }
    }
    else {
      a->bufpos = 0;
    }
  }
  a->buffer[a->bufpos++] = * ((int*) x);
}

static void safe_fwrite_bufdone (atrace *a)
{
  if (!a->used) return;

  if (a->bufpos > 0) {
    if (!ATRACE_IS_STREAM (a)) {
      Assert (a->fpos == ftell (a->tr), "Invariant violated");
    }
    _write_out_buf (a);
    a->bufpos = 0;
    if (!ATRACE_IS_STREAM (a)) {
      a->fpos += sizeof (int) * a->bufpos;
    }
  }
  fflush (a->tr);
}
  

static void safe_fwrite_int_buf (atrace *a, int x)
{
  if (a->endianness) {
    x = swap_endian_int (x);
  }
  /* assume sizeof (int) == 4 */
  safe_fwrite_buf (a, &x);
}

static void safe_fwrite_float_buf (atrace *a, float f)
{
  if (a->endianness) {
    f = swap_endian_float (f);
  }
  /* assume sizeof (float) == 4 */
  safe_fwrite_buf (a, &f);
}

static void safe_fwrite_ulong_buf (atrace *a, unsigned long v)
{
  safe_fwrite_int_buf (a, (int)(v & 0xffffffff));
  safe_fwrite_int_buf (a, (int) (v >> 32));
}

static void safe_fwrite_value_buf (atrace *a, name_t *n)
{
  if (n->type == 0) {
    safe_fwrite_float_buf (a, n->vu.v);
  }
  else if (n->width <= ONE_WIDTH) {
    if (n->width <= ONE_WIDTH/2) {
      safe_fwrite_int_buf (a, (int)n->vu.val);
    }
    else {
      safe_fwrite_ulong_buf (a, n->vu.val);
    }
  }
  else {
    int i;
    int count = (n->width + ONE_WIDTH - 1)/ONE_WIDTH;
    for (i=0; i < count; i++) {
      safe_fwrite_ulong_buf (a, n->vu.valp[i]);
    }
  }
}

/*
  Helper function: write atrace header
     done = 0 followed by done = 1 for most modes
     done = 2 for delta mode
*/
static void write_header (atrace *a, int done) 
{
  int x;

  Assert (a->endianness == 0, "Reversing endianness while writing?");

  if (done == 0) {
    Assert (a->read_mode == 0 && a->locked == 0, "Sorry, cannot write header");
    Assert (a->fnum == 0, "Must be at file 0");

    x = ENDIAN_SIGNATURE;
    safe_fwrite_int (a, x);

    x = ATRACE_CHANGING;

    safe_fwrite_int (a, x);
    safe_fwrite_int (a, a->timestamp);
    safe_fwrite_int (a, a->Nnodes);

    if (ATRACE_FMT(a->fmt) != ATRACE_NODE_ORDER && 
	ATRACE_FMT(a->fmt) != ATRACE_TIME_ORDER) {
      safe_fwrite_float (a, a->stop_time);
      safe_fwrite_float (a, a->dt);
    }
    a->locked = 1;
  }
  else if (done == 1) {
    Assert ((a->read_mode == 0) && (a->locked == 1), "Sorry cannot update header");
    if (a->fnum != 0) {
      fclose (a->tr);
      a->tr = fopen (a->tfile, "r+");
      if (!a->tr) {
	fatal_error ("Missing trace file!");
      }
      a->fnum = 0;
      a->fpos = ftell (a->tr);
    }
    fseek (a->tr, 0, SEEK_SET);

    x = ENDIAN_SIGNATURE;
    safe_fwrite_int (a, x);
    safe_fwrite_int (a, a->fmt);

    fseek (a->tr, 2*sizeof (int), SEEK_CUR);
    if (ATRACE_FMT(a->fmt) != ATRACE_NODE_ORDER &&
	ATRACE_FMT(a->fmt) != ATRACE_TIME_ORDER) {
      safe_fwrite_float (a, a->stop_time);
      fseek (a->tr, sizeof (float), SEEK_CUR);
    }
  }
  else  {
    /* done = 2 */
    Assert (a->read_mode == 0 && a->locked == 0, "Sorry, cannot write header");
    Assert (a->fnum == 0, "Must be at file 0");

    Assert (a->fmt != ATRACE_NODE_ORDER && a->fmt != ATRACE_TIME_ORDER,
	    "write_header: done=2 only for delta formats");

    x = ENDIAN_SIGNATURE;
    safe_fwrite_int (a, x);

    safe_fwrite_int (a, a->fmt);
    safe_fwrite_int (a, a->timestamp);
    safe_fwrite_int (a, a->Nnodes);

    safe_fwrite_float (a, a->stop_time);
    safe_fwrite_float (a, a->dt);
    a->locked = 1;
  }
}


static void gen_index_table (atrace *a)
{
  int i;
  hash_bucket_t *b;
  name_t *n;

  MALLOC (a->N, name_t *, a->Nnodes);

  for (i=0; i < a->Nnodes; i++)
    a->N[i] = NULL;

  for (i=0; i < a->H->size; i++)
    for (b = a->H->head[i]; b; b = b->next) {
      n = (name_t *)b->v;
      if (n->up) continue;
      a->N[n->idx] = n;
    }

  for (i=1; i < a->Nnodes; i++)
    if (a->N[i] == NULL) {
      printf ("WARNING: node %d unknown?\n", i);
    }
}

static void emit_header_aux (atrace *a) 
{
  FILE *nfp;
  name_t *n, *m;
  hash_bucket_t *b;
  int i, idx;
  char *t;

  Assert (a->read_mode == 0, "Cannot emit header");
  if (a->locked) return;

  /* Write header and lock a */
  if (ATRACE_FMT (a->fmt) == ATRACE_TIME_ORDER ||
      ATRACE_FMT (a->fmt) == ATRACE_NODE_ORDER) {
    write_header (a, 0);

    /* Allow early reads for some formats */
    if (ATRACE_FMT(a->fmt) == ATRACE_TIME_ORDER) {
      write_header (a, 1);
    }
  }
  else {
    /* write the header out, not marked changing */
    write_header (a, 2);
  }

  if (ATRACE_IS_STREAM (a)) {
    nfp = a->tr;
  }
  else {
    /* Create and emit names file */
    MALLOC (t, char, strlen (a->file) + 7);
    sprintf (t, "%s.names", a->file);

    Assert (nfp = fopen (t, "w"), "Could not create names file!");
    FREE (t);
  }

  /* a: = analog signal
     d: = digital signal
  */
  fprintf (nfp, "a:time\n");

  idx = 1;
  for (i=0; i < a->H->size; i++) 
    for (b = a->H->head[i]; b; b = b->next) {
      n = (name_t *) b->v;
      if (n->up) continue;
      
      m = n;
      m->idx = idx;

      if (m->type == 1) {
	fprintf (nfp, "d%d:", m->width);
      }
      else if (m->type == 2) {
	fprintf (nfp, "c%d:", m->width);
      }
      else if (m->type == 3) {
	fprintf (nfp, "e%d:", m->width);
      }
      else {
	fprintf (nfp, "a:");
      }
      fprintf (nfp, "%s", b->key);

      /* ok, here we go */
      while (m->next != n) {
	m = m->next;
	fprintf (nfp, "=%s", m->b->key);
      }
      fprintf (nfp, "\n");
      idx++;
    }

  for (i=0; i < a->H->size; i++) 
    for (b = a->H->head[i]; b; b = b->next) {
      name_t *set_root;
      n = (name_t *) b->v;
      if (n->up) continue;
      if (!n->up_set) continue;
      set_root = n->up_set;
      while (set_root->up_set) {
	set_root = set_root->up_set;
      }
      fprintf (nfp, "s:%d %d %d\n", set_root->idx, n->idx, n->set_flags);
    }

  if (nfp != a->tr) {
    /* names file: close it */
    fclose (nfp);
  }
  else {
    fprintf (nfp, "eof\n");
    fflush (nfp);
  }
  gen_index_table (a);
}

#define MAXBUFSZ 102400

static void _atrace_open_helper_names (atrace *a, FILE *fp)
{
  int  count;
  int idx;
  char buf[MAXBUFSZ];
  int l;
  char *sbuf;
  name_t *n, *m;
  int sigtype;
  int width;
  int found_type;
  
  count = a->Nnodes;
  idx = 0;
  a->Nnodes = 0;
  buf[MAXBUFSZ-1] = '\0';
  while (fgets (buf, MAXBUFSZ, fp)) {
    Assert (buf[MAXBUFSZ-1] == '\0', "Line too long");

    l = strlen (buf);
    Assert (buf[l-1] == '\n', "Line almost too long");
    buf[l-1] = '\0';
    l--;

    n = NULL;
    m = NULL;
    found_type = 0;

    if (l > 2 && buf[0] == 'a' && buf[1] == ':') {
      width = 0;
      sigtype = 0; /* analog */
      found_type = 1;
    }
    else if (l > 2 && buf[0] == 'd' && buf[1] == ':') {
      width = 1;
      sigtype = 1; /* digital */
      found_type = 1;
    }
    else if (l > 2 && buf[0] == 'd' && (sscanf (buf+1,"%d:", &width) == 1)) {
      sigtype = 1;
      found_type = 1;
    }
    else if (l > 2 && buf[0] == 'c' && buf[1] == ':') {
      width = 1;
      sigtype = 2; /* channel */
      found_type = 1;
    }
    else if (l > 2 && buf[0] == 'c' && (sscanf (buf+1,"%d:", &width) == 1)) {
      sigtype = 2;
      found_type = 1;
    }
    else if (l > 2 && buf[0] == 'e' && buf[1] == ':') {
      width = 1;
      sigtype = 3; /* extra */
      found_type = 1;
    }
    else if (l > 2 && buf[0] == 'e' && (sscanf (buf+1,"%d:", &width) == 1)) {
      sigtype = 3;
      found_type = 1;
    }
    else if (l > 2 && buf[0] == 's' && buf[1] == ':') {
      int set_root, my_idx, my_flags;
      a->set_members = 1;
      gen_index_table (a);
      if (sscanf (buf+2, "%d %d %d", &set_root, &my_idx, &my_flags) != 3) {
	warning ("Corrupted set entry: %s\n", buf);
      }
      else {
	if (set_root >= a->Nnodes || set_root < 0 ||
	    my_idx >= a->Nnodes || my_idx < 0) {
	  warning ("Illegal set entry %d %d", set_root, my_idx);
	}
	else {
	  atrace_set_addname (a, a->N[set_root], a->N[my_idx], my_flags);
	}
      }
      found_type = 2;
    }
    else if (buf[0] == 'e' && buf[1] == 'o' && buf[2] == 'f') {
      /* -- eof -- */
      break;
    }
    else {
      width = 0;
      sigtype = 0; /* analog */
      found_type = 0;
    }

    if (found_type == 1) {
      sbuf = buf+1;
      l--;
      while (*sbuf && *sbuf != ':') {
	sbuf++;
	l--;
      }
      if (*sbuf == ':') {
	sbuf++;
	l--;
      }
    }
    else {
      sbuf = buf;
    }

    if (found_type != 2) {

      while (l > 0) {
	while (l > 0 && sbuf[l] != '=') {
	  l--;
	}
	/* l == 0 || buf[l] == '=' */
	if (sbuf[l] == '=') {
	  n = atrace_create_node (a, sbuf+l+1);
	  n->type = sigtype;
	  n->idx = idx;
	  if (m) {
	    atrace_alias (a, n, m);
	  }
	  m = n;
	
	  sbuf[l] = '\0';
	  l--;
	}
      }
      n = atrace_create_node (a, sbuf);
      n->type = sigtype;
      n->idx = idx;
      n->width = width;
      if (m) {
	atrace_alias (a, n, m);
      }
      idx++;
    }
  }
  a->locked = 1;
  Assert (count == a->Nnodes, "Error: # of nodes in names file doesn't match trace file");
  if (a->set_members == 0) {
    gen_index_table (a);
    a->set_members = 1;
  }
}


/* open a trace file */
atrace *atrace_open (const char *s)
{
  atrace *a;
  FILE *nfp;
  char *t;
  int len;

  a = _atrace_alloc (1 /* read mode */);

  len = strlen (s);
  MALLOC (a->file, char, len + 1);
  strcpy (a->file, s);
  
  MALLOC (a->tfile, char, len + 7);
  sprintf (a->tfile, "%s.trace", a->file);

  a->tr = fopen (a->tfile, "rb");
  if (!a->tr) {
    fprintf (stderr, "ERROR: Could not open trace file `%s.trace'\n", a->file);
    FREE (a->file);
    FREE (a->tfile);
    FREE (a);
    return NULL;
  }

  read_header (a);

  /* read in names */
  MALLOC (t, char, len + 7);
  sprintf (t, "%s.names", a->file);
  Assert (nfp = fopen (t, "r"), "Could not open names file for reading");
  FREE (t);

  _atrace_open_helper_names (a, nfp);
  fclose (nfp);

  /* buffer isn't used for reading */
  a->curt = -1;
  a->rec_type = -2;

  return a;
}

/* open a trace file */
atrace *atrace_listen_wait (int port)
{
  atrace *a;

  a = _atrace_alloc (1 /* read mode */);

  if ((a->sock = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
    perror ("socket");
    return NULL;
  }
  a->addr.sin_family = AF_INET;
  a->addr.sin_port = htons (port);
  a->addr.sin_addr.s_addr = htons (INADDR_ANY);

  if (bind (a->sock, (struct sockaddr *)&a->addr, sizeof (a->addr)) < 0) {
    perror ("bind failed");
    close (a->sock);
    hash_free (a->H);
    FREE (a);
    return NULL;
  }

  if (listen (a->sock, 1) < 0) {
    perror ("listen failed");
    close (a->sock);
    hash_free (a->H);
    FREE (a);
    return NULL;
  }

  if ((a->fd = accept (htons (port), NULL, NULL)) < 0) {
    perror ("accept failed");
    close (a->sock);
    hash_free (a->H);
    FREE (a);
    return NULL;
  }

  a->tr = fdopen (a->fd, "rb");
  read_header (a);
  _atrace_open_helper_names (a, a->tr);

  /* buffer isn't used for reading */
  a->curt = -1;
  a->rec_type = -2;

  return a;
}


/*
  Rescale trace file
*/
void atrace_rescale (atrace *a,  float vdt)
{
  Assert (a->read_mode, "Cannot rescale a file that is being created!");
  if (vdt < a->dt) {
    fprintf (stderr, "WARNING: Rescaling on file `%s' ignored; cannot increase resolution", a->file);
    return;
  }

  /* set vdt, nsteps */
  a->vdt = vdt;
  a->Nvsteps = 1 + (a->stop_time/a->vdt +0.01);
}
  
  
  

/*
  Return header
*/
int atrace_header (atrace *a, int *ts, int *Nnodes, int *Nsteps, int *fmt)
{
  Assert (a->read_mode, "atrace_header called in write mode!");
  Assert (a->locked, "atrace_header called, locked not set!");

  if (a->fnum == 0) {
    if (!ATRACE_IS_STREAM (a)) {
      read_header (a);
    }
  }

  *ts = a->timestamp;
  *Nnodes = a->Nnodes;
  *Nsteps = a->Nvsteps;
  *fmt = a->fmt;

  return 0;
}

#define ISTEP(a,tm)  ((int) (((tm) + (a)->dt*0.01)/(a)->dt))
#define VSTEP(a,tm)((int) (((tm) + (a)->vdt*0.01)/(a)->vdt))

static int atrace_node_equal (name_t *n, union atrace_value *v)
{
  if (n->type == 0) {
    return (n->vu.v == v->v);
  }
  else if (n->width <= ONE_WIDTH) {
    return (n->vu.val == v->val);
  }
  else {
    int i, count;
    count = (n->width + ONE_WIDTH-1)/ONE_WIDTH;
    for (i=0; i < count; i++) {
      if (n->vu.valp[i] != v->valp[i]) {
	return 0;
      }
    }
    return 1;
  }
}

static void _value_alloc (name_t *n, union atrace_value *v)
{
  if (n->type == 0) return;
  if (n->width > 64) {
    int count = (n->width + ONE_WIDTH-1)/ONE_WIDTH;
    MALLOC (v->valp, unsigned long, count);
    v->valp[0] = 0;
  }
  else {
    v->val = 0;
  }
}

void atrace_alloc_val_entry (name_t *n, atrace_val_t *v)
{
  _value_alloc (n, v);
}

static void _value_free (name_t *n, union atrace_value *v)
{
  if (n->type == 0) return;
  if (n->width > 64) {
    FREE (v->valp);
  }
}

void atrace_free_val_entry (name_t *n, atrace_val_t *v)
{
  _value_free (n, v);
}

static void _value_assign (name_t *n, atrace_val_t *lhs, atrace_val_t *rhs)
{
  if (n->type == 0) {
    lhs->v = rhs->v;
  }
  else if (n->width <= ONE_WIDTH) {
    lhs->val = rhs->val;
  }
  else {
    int i, count;
    count = (n->width + ONE_WIDTH - 1)/ONE_WIDTH;
    for (i=0; i < count; i++) {
      lhs->valp[i] = rhs->valp[i];
    }
  }
}

/*
  Read record, and the next time "t"
*/
static float _read_record (atrace *a, float t)
{
  int idx;
  union atrace_value v;
  int c;
  name_t *prev;

  if (feof (a->tr)) return -1;

  fread_int (a, &idx);
  a->hd_chglist = NULL;

  if (idx < 0) {
    /* EOF marker */
    return -1;
  }
  if (a->rec_type == -2) {
    idx = 0;
  }
  prev = NULL;
  if (a->rec_type == -2) {
    for (idx = 1; idx < a->Nnodes; idx++) {
      _value_alloc (a->N[idx], &v);
      fread_value (a, a->N[idx], &v);
      if (!atrace_node_equal (a->N[idx], &v)) {
	if (prev) {
	  prev->chg_next = a->N[idx];
	}
	else {
	  a->hd_chglist = a->N[idx];
	}
	a->N[idx]->chg_next = NULL;
	prev = a->N[idx];
	_value_assign (a->N[idx], &a->N[idx]->vu, &v);
      }
      _value_free (a->N[idx], &v);
      if (a->fmt == ATRACE_DELTA_CAUSE) {
	fread_int (a, &c);
	if (c < 0 || c >= a->Nnodes) {
	  fprintf (stderr, "ERROR: invalid index in trace file (%d)\n", c);
	  fprintf (stderr, "OFFSET: %d\n", (int)  ftell (a->tr));
	  exit (1);
	}
	a->N[idx]->cause = c;
      }
      else {
	a->N[idx]->cause = 0;
      }
    }
    fread_int (a, &idx);
  }
  else {
    while (idx != -1 && idx != -2) {
      if (prev) {
	prev->chg_next = a->N[idx];
      }
      else {
	a->hd_chglist = a->N[idx];
      }
      a->N[idx]->chg_next = NULL;
      if (idx < 0 || idx >= a->Nnodes) {
	fprintf (stderr, "ERROR: invalid index in trace file (%d)\n", idx);
	fprintf (stderr, "OFFSET: %d\n", (int)  ftell (a->tr));
	exit (1);
      }
      fread_value (a, a->N[idx], &a->N[idx]->vu);
      if (a->fmt == ATRACE_DELTA_CAUSE) {
	fread_int (a, &c);
	if (c < 0 || c >= a->Nnodes) {
	  fprintf (stderr, "ERROR: invalid index in trace file (%d)\n", c);
	  fprintf (stderr, "OFFSET: %d\n", (int)  ftell (a->tr));
	  exit (1);
	}
	a->N[idx]->cause = c;
      }
      else {
	a->N[idx]->cause = 0;
      }
      prev = a->N[idx];
      fread_int (a, &idx);
    }
  }
  if (idx == -1) {
    a->rec_type = -1;
  }
  else {
    a->rec_type = -2;
  }
  fread_float (a, &v.v);
  return v.v;
}


/* read all values into an array 
   node #i, step #j is at position M[Nvsteps*i + j]
*/
void atrace_readall (atrace *a, atrace_val_t *M)
{
  int i, j;
  float t;
  int step;
  int k;

  Assert (a->read_mode, "atrace_readall called in write mode");
  Assert (sizeof (int) == sizeof(float), "this is insane");

  seek_after_header (a, 0);

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_TIME_ORDER:
    for (j=0; j < a->Nsteps; j++) {
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP (a, j*a->dt);
      }
      for (i=0; i < a->Nnodes; i++) {
	fread_value (a, a->N[i], &M[a->Nvsteps*i + k]);
      }
    }
    break;

  case ATRACE_NODE_ORDER:
    for (i=0; i < a->Nnodes; i++) 
      for (j=0; j < a->Nsteps; j++) {
	if (a->vdt == a->dt) {
	  k = j;
	}
	else {
	  k = VSTEP(a, j*a->dt);
	}
	fread_value (a, a->N[i], &M[a->Nvsteps*i + k]);
      }
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    fread_float (a, &t);
    step = ISTEP (a, t);
    for (j = 0; j < a->Nsteps; j++) {
      while (j == step) {
	t = _read_record (a, t);
	if (t >= 0) {
	  step = ISTEP (a, t);
	}
	else {
	  step = -1;
	}
      }
      a->N[0]->vu.v = j*a->dt;
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP (a, j*a->dt);
      }
      for (i = 0; i < a->Nnodes; i++) {
	_value_assign (a->N[i], &M[a->Nvsteps*i + k], &a->N[i]->vu);
      }
    }
    break;

  default:
    Assert (0, "Unimplemented format");
  }
}

/* read all values into an array 
   node #i, step #j is at position M[Nnodes*j + i]
*/
void atrace_readall_xposed (atrace *a, atrace_val_t *M)
{
  int i, j;
  int step;
  float t;
  int k;

  Assert (a->read_mode, "atrace_readall_xposed called in write mode");

  seek_after_header (a, 0);

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_TIME_ORDER:
    for (j=0; j < a->Nsteps; j++) {
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP (a, j*a->dt);
      }
      for (i=0; i < a->Nnodes; i++) {
	fread_value (a, a->N[i], &M[a->Nnodes*k + i]);
      }
    }
    break;

  case ATRACE_NODE_ORDER:
    for (i=0; i < a->Nnodes; i++)
      for (j=0; j < a->Nsteps; j++) {
	if (a->vdt == a->dt) {
	  k = j;
	}
	else {
	  k = VSTEP (a, j*a->dt);
	}
	fread_value (a, a->N[i], &M[a->Nnodes*k + i]);
      }
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    fread_float (a, &t);
    step = ISTEP (a, t);
    for (j = 0; j < a->Nsteps; j++) {
      while (j == step) {
	t = _read_record (a, t);
	if (t >= 0) {
	  step = ISTEP (a, t);
	}
	else {
	  step = -1;
	}
      }
      a->N[0]->vu.v = j*a->dt;
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP (a, j*a->dt);
      }
      for (i = 0; i < a->Nnodes; i++) {
	_value_assign (a->N[i], &M[a->Nnodes*k + i], &a->N[i]->vu);
      }
    }
    break;

  default:
    Assert (0, "Unimplemented format");
    break;
  }
}



/* 
   read all node values into an array 
*/
void atrace_readall_nodenum (atrace *a, int node, atrace_val_t *M)
{
  int i;
  int step;
  int k;

  Assert (a->read_mode, "atrace_readall_node called in write mode");
  Assert (node >= 0 && node < a->Nnodes, "atrace_readall_node: bad node");

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_TIME_ORDER:
    seek_after_header (a, _space_for_nodes_upto (a, node));
    step = _space_for_nodes_upto (a, a->Nnodes);

    for (i=0; i < a->Nsteps; i++) {
      if (a->vdt == a->dt) {
	k = i;
      }
      else {
	k = VSTEP (a, i*a->dt);
      }
      fread_value (a, a->N[node], &M[k]);
      fseek (a->tr, step, SEEK_CUR);
    }
    break;

  case ATRACE_NODE_ORDER:
    seek_after_header (a, _space_for_nodes_upto (a, node)*a->Nsteps);
    for (i=0; i < a->Nsteps; i++) {
      if (a->vdt == a->dt) {
	k = i;
      }
      else {
	k = VSTEP (a, i*a->dt);
      }
      fread_value (a, a->N[node], &M[k]);
    }
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    atrace_readall_node (a, a->N[node], M);
    break;
  default:
    Assert (0, "Unimplemented format");
    break;
  }
}

/* 
   read all node values into an array 
*/
void atrace_readall_block (atrace *a, int node, int num, atrace_val_t *M)
{
  int i;
  int step;
  int k;
  int j;
  float t;

  Assert (a->read_mode, "atrace_readall_block called in write mode");
  Assert (node >= 0 && node < a->Nnodes, "atrace_readall_block: bad node");
  Assert (node + num - 1 < a->Nnodes, "atrace_readall_block: bad node");

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_TIME_ORDER:
    step = _space_for_nodes_upto (a, node);
    seek_after_header (a, step);
    step = _space_for_nodes_upto (a, a->Nnodes) -
      _space_for_nodes_upto  (a, node+num) + step;

    for (i=0; i < a->Nsteps; i++) {
      if (a->vdt == a->dt) {
	k = i;
      }
      else {
	k = VSTEP (a, i*a->dt);
      }
      for (j=0; j < num; j++) {
	/* node # (node+j), step k */
	fread_value (a, a->N[node+j], &M[j*a->Nvsteps+k]);
      }
      fseek (a->tr, step, SEEK_CUR);
    }
    break;

  case ATRACE_NODE_ORDER:
    for (j=0; j < num; j++) {
      seek_after_header (a, _space_for_nodes_upto (a, j+node)*a->Nsteps);
      for (i=0; i < a->Nsteps; i++) {
	if (a->vdt == a->dt) {
	  k = i;
	}
	else {
	  k = VSTEP (a, i*a->dt);
	}
	fread_value (a, a->N[j], &M[j*a->Nvsteps+ k]);
      }
    }
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    if (node == 0) {
      /* special case, index 0 is time */
      for (j=0; j < a->Nsteps; j++) {
	if (a->vdt == a->dt) {
	  k = j;
	}
	else {
	  k = VSTEP (a, j*a->dt);
	}
	M[k].v = j*a->dt;
      }
      node = 1;
      num--;
    }
    if (num == 0) return;
    seek_after_header (a, 0);
    fread_float (a, &t);
    step = ISTEP (a, t);
    for (j=0; j < a->Nsteps; j++) {
      while (j == step) {
	t = _read_record (a, t);
	if (t >= 0) {
	  step = ISTEP (a, t);
	}
	else {
	  step = -1;
	}
      }
      a->N[0]->vu.v = j*a->dt;
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP (a, j*a->dt);
      }
      for (i=0; i < num; i++) {
	_value_assign (a->N[i+node], &M[a->Nvsteps*i + k], &a->N[i+node]->vu);
      }
    }
    break;
  default:
    Assert (0, "Unimplemented format");
    break;
  }
}


/* 
   read all node values into an array 
*/
void atrace_readall_node (atrace *a, name_t *n, atrace_val_t *M)
{
  int j;
  int step;
  float t;
  int k;

  Assert (a->read_mode, "atrace_readall_node called in write mode");
  Assert (n, "atrace_readall_node: bad node name");

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_TIME_ORDER:
  case ATRACE_NODE_ORDER:
    atrace_readall_nodenum (a, n->idx, M);
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    if (n->idx == 0) {
      /* special case: index 0 is time */
      for (j = 0; j < a->Nsteps; j++) {
	if (a->vdt == a->dt) {
	  k = j;
	}
	else {
	  k = VSTEP (a, j*a->dt);
	}
	M[k].v = j*a->dt;
      }
      return;
    }

    /* Ugly ugly... */
    seek_after_header (a, 0);
    fread_float (a, &t);
    step = ISTEP (a, t);
    for (j = 0; j < a->Nsteps; j++) {
      while (j == step) {
	t = _read_record (a, t);
	if (t >= 0) {
	  step = ISTEP (a, t);
	}
	else {
	  step = -1;
	}
      }
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP(a, j*a->dt);
      }
      _value_assign (a->N[n->idx], &M[k], &a->N[n->idx]->vu);
    }
    break;
  default:
    Assert (0, "Unimplemented format");
    break;
  }
}

/* 
   read all node values into an array 
*/
void atrace_readall_nodenum_c (atrace *a, int node, atrace_val_t *M, int *C)
{
  Assert (a->read_mode, "atrace_readall_nodenum_c called in write mode");
  Assert (node >= 0 && node < a->Nnodes, "atrace_readall_nodenum_c: bad node");

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_TIME_ORDER:
  case ATRACE_NODE_ORDER:
  case ATRACE_DELTA:
    fatal_error ("Trace format does not contain cause values!");
    break;

  case ATRACE_DELTA_CAUSE:
    atrace_readall_node_c (a, a->N[node], M, C);
    break;
  default:
    Assert (0, "Unimplemented format");
    break;
  }
}

/* 
   read all node values into an array 
*/
void atrace_readall_node_c (atrace *a, name_t *n, atrace_val_t *M, int *C)
{
  int j;
  int step;
  float t;
  int k;

  Assert (a->read_mode, "atrace_readall_node_c called in write mode");
  Assert (n, "atrace_readall_node_c: bad node name");

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_TIME_ORDER:
  case ATRACE_NODE_ORDER:
  case ATRACE_DELTA:
    fatal_error ("Trace format does not contain cause values!");
    break;

  case ATRACE_DELTA_CAUSE:
    if (n->idx == 0) {
      /* special case: index 0 is time */
      for (j = 0; j < a->Nsteps; j++) {
	if (a->vdt == a->dt) {
	  k = j;
	}
	else {
	  k = VSTEP (a, j*a->dt);
	}
	M[k].v = j*a->dt;
	C[k] = 0;
      }
      return;
    }

    /* Ugly ugly... */
    seek_after_header (a, 0);
    fread_float (a, &t);
    step = ISTEP (a, t);
    for (j = 0; j < a->Nsteps; j++) {
      while (j == step) {
	t = _read_record (a, t);
	if (t >= 0) {
	  step = ISTEP (a, t);
	}
	else {
	  step = -1;
	}
      }
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP(a, j*a->dt);
      }
      _value_assign (a->N[n->idx], &M[k], &a->N[n->idx]->vu);
      C[k] = a->N[n->idx]->cause;
    }
    break;
  default:
    Assert (0, "Unimplemented format");
    break;
  }
}

/*------------------------------------------------------------------------
 *
 *  atrace_init_time --
 *
 *   Initialize new API. Read first value for step 0
 *
 *------------------------------------------------------------------------
 */
void atrace_init_time (atrace *a)
{
  int n;

  Assert (a->curt == -1, "Only call this once!");
  Assert (a->read_mode, "atrace_init_time called in write mode");

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_NODE_ORDER:
    fatal_error ("New atrace API does not work with node order format");
    break;

  case ATRACE_TIME_ORDER:
    seek_after_header (a, 0);
    for (n=0; n < a->Nnodes; n++) {
      fread_value (a, a->N[n], &a->N[n]->vu);
    }
    a->curt = 0;
    a->curstep = 0;
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    a->N[0]->vu.v = 0;
    fread_float (a, &a->curt);
    a->nextt = _read_record (a, 0);
    a->curstep = 0;
    break;
  default:
    Assert (0, "Unimplemented format");
    break;
  }
}

void atrace_advance_time (atrace *a, int nsteps)
{
  int n;
  int i, k;
  int nv;

  Assert (a->curt >= 0, "Missing init_time()");
  Assert (a->read_mode, "atrace_advance_time called in write mode");
  if (nsteps <= 0) return;

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_NODE_ORDER:
    fatal_error ("New atrace API does not work with node order format");
    break;

  case ATRACE_TIME_ORDER:
    fatal_error ("Not tested");
    nv = VSTEP(a,a->curt);
    for (i=ISTEP(a,a->curt); i < a->Nsteps; i++) {
      if (a->vdt == a->dt) {
	k = i;
      }
      else {
	k = VSTEP (a, i*a->dt);
      }
      if (k > nsteps + nv)
	break;
      for (n=0; n < a->Nnodes; n++) {
	fread_value (a, a->N[n], &a->N[n]->vu);
      }
    }
    a->curt = (i-1)*a->dt;
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    a->curstep += nsteps;
    a->N[0]->vu.v += nsteps*a->vdt;
    while (a->nextt >= 0 && a->curt >= 0 && (a->nextt < a->curstep*a->vdt)) {
      a->curt = a->nextt;
      //a->N[0]->v = a->curt;
      a->nextt = _read_record (a, a->curt);
    }
#if 0    
    nv = VSTEP (a, a->curt);
    for (i=ISTEP (a, a->curt); i < a->Nsteps; i++) {
      if (a->vdt == a->dt) {
	k = i;
      }
      else {
	k = VSTEP (a, i*a->dt);
      }
      if (k > nsteps + nv) 
	break;
      a->N[0]->vu.v = i*a->dt;
    }
    while ((a->curt >= 0 && a->nextt >= 0) && (a->curt < (nv+nsteps)*a->vdt)) {
      a->curt = a->nextt;
      a->nextt = _read_record (a, a->curt);
      printf ("Got record @ %g, next is %g\n", a->curt*1e9, a->nextt*1e9);
    }
#endif
    break;
  default:
    Assert (0, "Unimplemented format");
    break;
  }
}

void atrace_advance_time_to (atrace *a, int nsteps)
{
  int nv;

  Assert (a->curt >= 0, "Missing init_time()");
  Assert (a->read_mode, "atrace_advance_time called in write mode");
  if (nsteps <= 0) return;

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_NODE_ORDER:
    fatal_error ("New atrace API does not work with node order format");
    break;

  case ATRACE_TIME_ORDER:
    nv = VSTEP(a,a->curt);
    atrace_advance_time (a, nsteps - nv);
    break;
  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    atrace_advance_time (a, nsteps - a->curstep);
    break;
  default:
    Assert (0, "Unimplemented format");
    break;
  }
}


static name_t *_union_find (name_t *n)
{
  name_t *tmp;
  if (!n || !n->up) return n;

  tmp = n;
  while (tmp->up) {
    tmp = tmp->up;
  }

  while (n->up) {
    name_t *x = n->up;
    n->up = tmp;
    n = x;
  }
  return tmp;
}

/* lookup a node */
name_t *atrace_lookup (atrace *a, const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (a->H, s);
  if (b) {
    return _union_find ((name_t *) b->v);
  }
  return NULL;
}

/* create a node */
name_t *atrace_create_node (atrace *a, const char *s)
{
  name_t *n;
  hash_bucket_t *b;

  Assert (!a->locked, "atrace_create_node: Trace file locked!");
  Assert (!a->set_members, "atrace_create_node: Sets being created; no more nodes/aliases!");

  b = hash_lookup (a->H, s);
  if (b) {
    return _union_find ((name_t *) b->v);
  }
  b = hash_add (a->H, s);

  NEW (n, name_t);
  n->vu.v = 0.0;
  n->up = NULL;
  n->up_set = NULL;
  n->set_flags = 0;
  n->next = n;
  n->b = b;
  n->idx = -1;
  n->chg = 0;
  n->type = 0;
  n->chg_next = NULL;
  n->width = 1;
  n->wadj = 0;

  b->v = (void *) n;

  a->Nnodes++;
  
  return n;
}

struct raw_name_struct {
  struct name_struct *up;
  struct name_struct *next;
  hash_bucket_t *b;		/* name */
};

void atrace_alias (atrace *a, name_t *n, name_t *m)
{
  struct raw_name_struct *r = NULL;
  name_t *stmp;
  struct raw_name_struct *tmp;
  Assert (!a->locked, "atrace_alias: Trace file locked!");
  Assert (!a->set_members, "atrace_alias: Sets being created; no more nodes/aliases!");

  /* ok, set m to n */
  m = _union_find (m);
  n = _union_find (n);
  if (m == n) {
    return;
  }

  NEW (r, struct raw_name_struct);
  tmp = (struct raw_name_struct *) m;

  while (1) {
    if (tmp->up == m) {
      tmp->up = (name_t *)r;
    }
    if (tmp->next == m) {
      break;
    }
    tmp = (struct raw_name_struct *) tmp->next;
  }
  if (tmp == (struct raw_name_struct *)m) {
    /* nothing in the ring */
    r->next = (name_t *)r;
  }
  else {
    tmp->next = (name_t *)r;
    r->next = m->next;
  }
  r->up = n;
  r->b = m->b;
  r->b->v = r;
  FREE (m);

  stmp = n->next;
  n->next = r->next;
  r->next = stmp;

  a->Nnodes--;
}


void atrace_set_addname (atrace *a, name_t *n, name_t *m, int flags)
{
  a->set_members = 1;

  /* ok, set m to n */
  m = _union_find (m);
  n = _union_find (n);
  if (m == n) {
    return;
  }

  m->set_flags = flags;

  while (n->up_set) {
    n = n->up_set;
  }
  
  while (m->up_set) {
    m = m->up_set;
  }

  if (m == n) {
    return;
  }
 
  m->up_set = n;
}

name_t *atrace_get_setname (name_t *n)
{
  name_t *m, *tmp;
  n = _union_find (n);

  m = n;
  while (m->up_set) {
    m = m->up_set;
  }
  while (n->up_set) {
    tmp = n->up_set;
    n->up_set = m;
    n = tmp;
  }
  return m;
}

int atrace_get_setflags (name_t *n)
{
  n = _union_find (n);
  return n->set_flags;
}

static void _emit_record (atrace *a)
{
  int i;
  hash_bucket_t *b;
  name_t *n;

  if (a->curtime == 0) {
    /* special case, initial condition */
    safe_fwrite_float_buf (a, a->curtime*a->dt);
    safe_fwrite_int_buf (a, 0); /* dummy */
    for (i=0; i < a->H->size; i++)
      for (b = a->H->head[i]; b; b = b->next) {
	n = (name_t *) b->v;
	if (n->up) continue;
	/*safe_fwrite_int_buf (a, n->idx);*/
	safe_fwrite_value_buf (a, n);
	if (a->fmt == ATRACE_DELTA_CAUSE) {
	  safe_fwrite_int_buf (a, 0);
	}
	n->chg = 0;
      }
    /*safe_fwrite_int_buf (a, -1);*/
  }
  else {
    int count = 0;
    for (i=0; i < a->H->size; i++) 
      for (b = a->H->head[i]; b; b = b->next) {
	n = (name_t *) b->v;
	if (n->up) continue;
	count += n->chg;
      }
    if (count > 0) {
      if (count > a->Nnodes/2) {
	safe_fwrite_int_buf (a, -2); /* for prev record */
	safe_fwrite_float_buf (a, a->curtime*a->dt);
	safe_fwrite_int_buf (a, 0);
	for (i=0; i < a->H->size; i++)
	  for (b = a->H->head[i]; b; b = b->next) {
	    n = (name_t *) b->v;
	    if (n->up) continue;
	    safe_fwrite_value_buf  (a, n);
	    if (a->fmt == ATRACE_DELTA_CAUSE) {
	      safe_fwrite_int_buf (a, n->cause);
	    }
	    n->chg = 0;
	  }
      }
      else {
	safe_fwrite_int_buf (a, -1); /* for prev record */
	safe_fwrite_float_buf (a, a->curtime*a->dt);
	for (i=0; i < a->H->size; i++) 
	  for (b = a->H->head[i]; b; b = b->next) {
	    n = (name_t *) b->v;
	    if (n->up) continue;
	    if (n->chg) {
	      safe_fwrite_int_buf (a, n->idx);
	      safe_fwrite_value_buf (a, n);
	      if (a->fmt == ATRACE_DELTA_CAUSE) {
		safe_fwrite_int_buf (a, n->cause);
	      }
	      n->chg = 0;
	    }
	  }
      }
    }
    else {
      /* else nothing to do! no changes */
    }
  }
}

static int large_change (atrace *a, float oldv, float newv)
{
  float chg;

  if (oldv == newv) return 0;
  if (a->adv < 0 && a->rdv < 0) {
    return (oldv != newv);
  }

  chg = newv-oldv;

  /* absolute */
  if ((a->adv > 0) && (fabs (chg) >= a->adv)) {
    return 1;
  }
  /* small according to absolute check */

  /* relative */
  if ((a->rdv > 0) && (oldv == 0 || (fabs (chg/oldv) >= a->rdv))) {
    return 1;
  }
  
  return 0;
}

static void _sig_change_delta (atrace *a, name_t *m, float t, atrace_val_t *v)
{
  int step;

  step = ISTEP (a, t);

  if (step >= a->Nsteps) {
    /* Nsteps changed! */
    a->Nsteps = step + 1;
    a->stop_time = step * a->dt;
  }

  Assert (step < a->Nsteps, "Invalid stop time!");
  Assert (step >= a->curtime, "Going backward in time?");

  if (step == a->curtime) {
    if (DONT_FILTER_DELTAS(m->type) || large_change (a, m->vu.v, v->v)) {
      _value_assign (m, &m->vu, v);
      m->chg = 1;
    }
    return;
  }

  _emit_record (a);		/* emit record */
  
  a->curtime = step;
  if (large_change (a, m->vu.v, v->v) || DONT_FILTER_DELTAS(m->type)) {
    _value_assign (m, &m->vu, v);
    m->chg = 1;
  }
}

static void _sig_change_delta_cause (atrace *a, name_t *m, float t,
				     atrace_val_t *v, int idx)
{
  int step;

  step = ISTEP (a, t);

  if (step >= a->Nsteps) {
    /* Nsteps changed! */
    a->Nsteps = step + 1;
    a->stop_time = step * a->dt;
  }

  Assert (step < a->Nsteps, "Invalid stop time!");
  Assert (step >= a->curtime, "Going backward in time?");

  if (step == a->curtime) {
    if (large_change (a, m->vu.v, v->v) || DONT_FILTER_DELTAS(m->type)) {
      _value_assign (m, &m->vu, v);
      m->cause = idx;
      m->chg = 1;
    }
    return;
  }

  _emit_record (a);		/* emit record */
  
  a->curtime = step;
  if (large_change (a, m->vu.v, v->v) || DONT_FILTER_DELTAS(m->type)) {
    _value_assign (m, &m->vu, v);
    m->cause = idx;
    m->chg = 1;
  }
}


static void _sig_delta_end (atrace *a)
{
  _emit_record (a);
  safe_fwrite_int_buf (a, -1);

  /* repeat empty sequence */
  safe_fwrite_float_buf (a, a->curtime*a->dt);
  safe_fwrite_int_buf (a, -1);
}


static void _sig_change_timeorder (atrace *a, name_t *n, float t, atrace_val_t *v)
{
  int step;
  int i;
  hash_bucket_t *b;
  name_t *m = n;

  step = ISTEP (a, t);

  if (step >= a->Nsteps) {
    /* Nsteps changed! */
    a->Nsteps = step + 1;
    a->stop_time = step * a->dt;
  }

  Assert (step < a->Nsteps, "Invalid stop time!");
  Assert (step >= a->curtime, "Going backward in time?");

  while (a->curtime < step) {
    /* dump the filler */
    safe_fwrite_float_buf (a, a->curtime*a->dt);
    for (i=0; i < a->H->size; i++) 
      for (b = a->H->head[i]; b; b = b->next) {
	n = (name_t *) b->v;
	if (n->up) continue;
	safe_fwrite_value_buf (a, n);
      }
    a->curtime++;
  }
  Assert (a->curtime == step, "What?");
  _value_assign (m, &m->vu, v);
}

static void _sig_timeorder_end (atrace *a)
{
  int i;
  hash_bucket_t *b;
  name_t *n;

  while (a->curtime < a->Nsteps) {
     safe_fwrite_float_buf (a, a->curtime*a->dt);
    for (i=0; i < a->H->size; i++) 
      for (b = a->H->head[i]; b; b = b->next) {
	n = (name_t *) b->v;
	if (n->up) continue;
	safe_fwrite_value_buf (a, n);
      }
    a->curtime++;
  }
}

static void _sig_change_nodeorder (atrace *a, name_t *n, float t,
				   atrace_val_t *v)
{
  int step;
  int last_idx;
  
  step = ISTEP (a, t);

  Assert (step < a->Nsteps, "invalid stop time!");
  Assert (t == 0 || (n == a->nprev && step >=  a->curtime), "Invariant failed");

  if (a->nprev == NULL) {
    int i;
    /* step 0: write the time array out! */
    for (i=0; i < a->Nsteps; i++) {
      safe_fwrite_float_buf (a, i*a->dt);
    }
    last_idx = 0;
  }
  else {
    last_idx = a->nprev->idx;
  }

  if (step == 0) {
    if (last_idx + 1 != n->idx) {
      fprintf (stderr, "Not emitting nodes in node order!\n");
      exit (1);
    }
    if (last_idx != 0) {
      /* finish writing out previous node */
      while (a->curtime < a->Nsteps) {
	safe_fwrite_value_buf (a, a->nprev);
	a->curtime++;
      }
    }
    a->nprev = n;
    _value_assign (n, &n->vu, v);
    /* Ok, we're in good shape */
    safe_fwrite_value_buf (a, n);
    a->curtime = 1; /* next expected at time 1 * dt */
  }
  else {
    while (a->curtime < step) {
      safe_fwrite_value_buf (a, n);
      a->curtime++;
    }
    _value_assign (n, &n->vu, v);
    safe_fwrite_value_buf (a, n);
    a->curtime = step + 1;
  }
}

static void _sig_nodeorder_end (atrace *a)
{
  Assert (a->nprev != NULL && a->nprev->idx == a->Nnodes-1, "What on earth?");
    
  /* finish writing out last node */
  while (a->curtime < a->Nsteps) {
    safe_fwrite_value_buf (a, a->nprev);
    a->curtime++;
  }
}


/* signal change */
void atrace_signal_change_cause (atrace *a, name_t *n, float t, float v, name_t *c)
{
  atrace_val_t x;
  x.v = v;

  if (n->type != 0) {
    warning ("atrace_signal_change: %s is not an analog node!", ATRACE_GET_NAME (n));
  }
  atrace_general_change_cause (a, n, t, &x, c);
}

/* signal change */
void atrace_general_change_cause (atrace *a, name_t *n, float t, atrace_val_t *v, name_t *c)
{
  emit_header_aux (a);

  n = _union_find (n);
  
  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_NODE_ORDER:
    _sig_change_nodeorder (a, n, t, v);
    break;
  case ATRACE_TIME_ORDER:
    _sig_change_timeorder (a, n, t, v);
    break;
  case ATRACE_DELTA_CAUSE:
    _sig_change_delta_cause (a, n, t, v, c ? c->idx : 0);
    break;
  case ATRACE_DELTA:
    _sig_change_delta (a, n, t, v);
    break;
  default:
    Assert (0, "unsupported format");
    break;
  }
}


/* signal change */
static void atrace_signal_done (atrace *a)
{
  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_NODE_ORDER:
    _sig_nodeorder_end (a);
    break;
  case ATRACE_TIME_ORDER:
    _sig_timeorder_end (a);
    break;
  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    _sig_delta_end (a);
    break;
  default:
    Assert (0, "unsupported format");
    break;
  }
  safe_fwrite_bufdone (a);
}

/* flush output */
void atrace_flush (atrace *a)
{
  safe_fwrite_bufdone (a);
}

/* close file */
void atrace_close (atrace *a)
{
  hash_iter_t it;
  hash_bucket_t *b;

  /* free stuff; emit offset table if required */
  if (a->read_mode == 0) {
    emit_header_aux (a);
    atrace_signal_done (a);
    if (ATRACE_FMT (a->fmt) == ATRACE_NODE_ORDER) {
      write_header (a, 1);
    }
  }
  fclose (a->tr);

  hash_iter_init (a->H, &it);
  while ((b = hash_iter_next (a->H, &it))) {
    name_t *n = (name_t *) b->v;
    if (!n->up) {
      _value_free (n, &n->vu);
    }
    FREE (n);
  }
  hash_free (a->H);
  FREE (a->file);
  FREE (a->tfile);
  if (a->N)
    FREE (a->N);
  if (a->buffer)
    FREE (a->buffer);
  FREE (a);
}


void atrace_mk_digital(name_t *n)
{
  _value_free (n, &n->vu);
  n->type = 1;
  _value_alloc (n, &n->vu);
}

void atrace_mk_analog(name_t *n)
{
  _value_free (n, &n->vu);
  n->type = 0;
}

void atrace_mk_channel(name_t *n)
{
  _value_free (n, &n->vu);
  n->type = 2;
  _value_alloc (n, &n->vu);
}

void atrace_mk_extra(name_t *n)
{
  _value_free (n, &n->vu);
  n->type = 3;
  _value_alloc (n, &n->vu);
}

void atrace_mk_width(name_t *n, int w)
{
  _value_free (n, &n->vu);
  n->width = w;
  n->wadj = 0;
  if (n->type == 2) {
    /* channel
       need (2^w + 3) states
       w = 0 => 2 bits
       w = 1 => 3 bits
       w = 2 => 3 bits

       So w + 1 for w >= 2, w + 2 otherwise
    */
    if (n->width < 2) {
      n->width++;
      n->wadj++;
    }
    n->width++;
    n->wadj++;
  }
  _value_alloc (n, &n->vu);
}

int atrace_channel_state (name_t *n, atrace_val_t *v)
{
  int i, c;
  if (n->width <= ATRACE_SHORT_WIDTH) {
    if (v->val == ATRACE_CHAN_SEND_BLOCKED ||
	v->val == ATRACE_CHAN_RECV_BLOCKED ||
	v->val == ATRACE_CHAN_IDLE) {
      return v->val;
    }
    else {
      return -1;
    }
  }
  c = (n->width + ONE_WIDTH - 1)/ONE_WIDTH;
  if (v->valp[0] == ATRACE_CHAN_SEND_BLOCKED ||
      v->valp[0] == ATRACE_CHAN_RECV_BLOCKED ||
      v->valp[0] == ATRACE_CHAN_IDLE) {
    for (i=1; i < c; i++) {
      if (v->valp[i] != 0) {
	return -1;
      }
    }
    return v->valp[0];
  }
  else {
    return -1;
  }
}


int atrace_more_data (atrace *a)
{
  if (a->read_mode == 0) {
    return 0;
  }
  if (ATRACE_FMT (a->fmt) == ATRACE_NODE_ORDER ||
      ATRACE_FMT (a->fmt) == ATRACE_CHANGING) {
    return 0;
  }
  if ((a->tr == NULL) || feof (a->tr) || (a->nextt == -1)) {
    return 0;
  }
  return 1;
}

int atrace_next_timestep (atrace *a)
{
  if (a->read_mode == 0) {
    return -1;
  }
  if (ATRACE_FMT (a->fmt) == ATRACE_TIME_ORDER) {
    return VSTEP (a, a->curt) + 1;
  }
  else if (ATRACE_FMT (a->fmt) == ATRACE_DELTA_CAUSE ||
	   ATRACE_FMT (a->fmt) == ATRACE_DELTA) {
    if (a->nextt == -1) {
      return -1;
    }
    else {
      int rval = VSTEP (a, a->nextt);
      if (rval == a->_last_ret_ts) {
	rval++;
      }
      a->_last_ret_ts = rval;
      return rval;
    }
  }
  else {
    return -1;
  }
}
