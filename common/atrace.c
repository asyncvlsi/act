/*************************************************************************
 *
 *  Copyright (c) 2004 Cornell University
 *  Computer Systems Laboratory
 *  Cornell University, Ithaca, NY 14853
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "atrace.h"
#include "misc.h"

#define ENDIAN_SIGNATURE 0xffff0000
#define ENDIAN_SIGNATURE_SWIZZLED 0x0000ffff

/* only filter for analog signals */
#define DONT_FILTER_DELTAS(t)  (!((t) == 0))

/* open an empty trace file */
atrace *atrace_create (const char *s, int fmt, float stop_time, float dt)
{
  atrace *a;
  int l;

  if (stop_time < 0 || fmt < ATRACE_FMT_MIN || ATRACE_FMT(fmt) > ATRACE_FMT_MAX) {
    return NULL;
  }
  NEW (a, atrace);

  a->H = hash_new (32);
  a->N = NULL;
  a->hd_chglist = NULL;

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
  a->read_mode = 0;
  a->fmt = fmt;
  a->stop_time = stop_time;
  a->Nsteps = 1 + (int)(stop_time / dt + 0.1);
  a->timestamp = time (NULL);
  a->locked = 0;
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
  
  return a;
}

/*
  Used to read the next int 
*/
static int safe_fread (atrace *a, void *x)
{
  long old;

 retry:
  if (a->fpos < a->fend) {
    a->fpos += sizeof (int);
    return fread (x, sizeof (int), 1, a->tr);
  }

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

static int fread_float (atrace *a, float *f)
{
  int ret = safe_fread (a, f);
  if (a->endianness) {
    *f = swap_endian_float (*f);
  }
  return ret;
}


static void seek_after_header (atrace *a, int offset)
{
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

  fseek (a->tr, 0, SEEK_END);
  a->fend = ftell (a->tr);
  a->fpos = 0;
  a->endianness = 0;

 retry:
  fseek (a->tr, 0, SEEK_SET);
  a->endianness = 0;
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
    a->Nsteps = n / (a->Nnodes * sizeof (float));

    /* get the stop-time from the file itself */
    fseek (a->tr, 4 * sizeof (int) + 
	   ((a->Nsteps-1)*(a->Nnodes))*sizeof (float), SEEK_SET);
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
  fseek (a->tr, offset, SEEK_SET);
  a->fpos = offset;
}


static void safe_fwrite (atrace *a, void *x)
{
  unsigned long old;

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

static void safe_fwrite_buf (atrace *a, void *x)
{
  if (!a->used) {
    a->used = 1;
    a->fpos = ftell (a->tr);
  }


  if (a->bufpos == a->bufsz || 
      (((a->bufpos*sizeof(int)) + a->fpos) == ATRACE_MAX_FILE_SIZE)) {
    Assert (a->fpos == ftell (a->tr), "Invariant violated");
    while (fwrite (a->buffer, sizeof (int), a->bufpos, a->tr) != a->bufpos) {
      fprintf (stderr, "fwrite failed, retrying..\n");
      sleep (60);
      fseek (a->tr, a->fpos, SEEK_SET);
    }
    a->fpos += sizeof (int) * a->bufpos;

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
    a->bufpos = 0;
  }
  a->buffer[a->bufpos++] = * ((int*) x);
}

static void safe_fwrite_bufdone (atrace *a)
{
  if (!a->used) return;

  if (a->bufpos > 0) {
    Assert (a->fpos == ftell (a->tr), "Invariant violated");
    while (fwrite (a->buffer, sizeof (int), a->bufpos, a->tr) != a->bufpos) {
      fprintf (stderr, "fwrite failed, retrying...\n");
      sleep (60);
      fseek (a->tr, a->fpos, SEEK_SET);
    }
    a->fpos += sizeof (int) * a->bufpos;
    a->bufpos = 0;     
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

/*
  Helper function: write atrace header
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
  else {
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
  write_header (a, 0);

  /* Allow early reads for some formats */
  if (ATRACE_FMT(a->fmt) == ATRACE_TIME_ORDER) {
    write_header (a, 1);
  }

  if (ATRACE_FMT(a->fmt) == ATRACE_DELTA) {
    /* can we update the stop time to time so far? */
  }

  /* Create and emit names file */
  MALLOC (t, char, strlen (a->file) + 7);
  sprintf (t, "%s.names", a->file);

  Assert (nfp = fopen (t, "w"), "Could not create names file!");
  FREE (t);

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
	fprintf (nfp, "d:");
      }
      else if (m->type == 2) {
	fprintf (nfp, "c:");
      }
      else {
	fprintf (nfp, "a:");
      }
      fprintf (nfp, "%s", b->key);

      /* ok, here we go */
      while (m->next != n) {
	m = m->next;
	m->idx = idx;
	fprintf (nfp, "=%s", m->b->key);
      }
      fprintf (nfp, "\n");
      idx++;
    }
  fclose (nfp);

  /* Canonicalize table */
  for (i=0; i < a->H->size; i++) 
    for (b = a->H->head[i]; b; b = b->next) {
      n = (name_t *) b->v;
      if (n->up && n->up->up) {
	m = n->up;
	while (m->up)
	  m = m->up;
	/* m is now the root */
	n->up = m;
      }
    }

  gen_index_table (a);
}


#define MAXBUFSZ 1024000
/* open a trace file */
atrace *atrace_open (char *s)
{
  atrace *a;
  FILE *nfp;
  int l;
  char buf[MAXBUFSZ];
  char *sbuf;
  char *t;
  name_t *n, *m;
  int idx;
  int count;
  int sigtype;

  NEW (a, atrace);

  a->H = hash_new (32);
  a->N = NULL;
  a->fnum = 0;
  a->fpos = 0;
  a->hd_chglist = NULL;

  l = strlen (s);
  MALLOC (a->file, char, l+1);
  strcpy (a->file, s);
  
  MALLOC (a->tfile, char, l + 7);
  sprintf (a->tfile, "%s.trace", a->file);

  a->tr = fopen (a->tfile, "rb");
  if (!a->tr) {
    fprintf (stderr, "ERROR: Could not open trace file `%s.trace'\n", a->file);
    FREE (a->file);
    FREE (a->tfile);
    FREE (a);
    return NULL;
  }

  a->read_mode = 1;
  a->locked = 0;
  a->vdt = -1;
  read_header (a);

  /* read in names */
  MALLOC (t, char, l + 7);
  sprintf (t, "%s.names", a->file);
  Assert (nfp = fopen (t, "r"), "Could not open names file for reading");
  FREE (t);

  count = a->Nnodes;
  idx = 0;
  a->Nnodes = 0;
  buf[MAXBUFSZ-1] = '\0';
  while (fgets (buf, MAXBUFSZ, nfp)) {
    Assert (buf[MAXBUFSZ-1] == '\0', "Line too long");

    l = strlen (buf);
    Assert (buf[l-1] == '\n', "Line almost too long");
    buf[l-1] = '\0';
    l--;

    n = NULL;
    m = NULL;

    if (l > 2 && buf[0] == 'a' && buf[1] == ':') {
      sigtype = 0; /* analog */
      sbuf = buf+2;
      l -= 2;
    }
    else if (l > 2 && buf[0] == 'd' && buf[1] == ':') {
      sigtype = 1; /* digital */
      sbuf = buf+2;
      l -= 2;
    }
    else if (l > 2 && buf[0] == 'c' && buf[1] == ':') {
      sigtype = 2; /* channel */
      sbuf = buf+2;
      l -= 2;
    }
    else {
      sigtype = 0; /* analog */
      sbuf = buf;
    }

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
    if (m) {
      atrace_alias (a, n, m);
    }
    idx++;
  }
  fclose (nfp);
  a->locked = 1;

  Assert (count == a->Nnodes, "Error: # of nodes in names file doesn't match trace file");

  gen_index_table (a);

  /* buffer isn't used for reading */
  a->bufsz = 0;
  a->bufpos = 0;
  a->buffer = NULL;
  a->fpos = 0;
  a->fnum = 0;
  a->used = 0;
  a->curt = -1;

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
    read_header (a);
  }

  *ts = a->timestamp;
  *Nnodes = a->Nnodes;
  *Nsteps = a->Nvsteps;
  *fmt = a->fmt;

  return 0;
}

#define ISTEP(a,tm)  ((int) (((tm) + (a)->dt*0.01)/(a)->dt))
#define VSTEP(a,tm)((int) (((tm) + (a)->vdt*0.01)/(a)->vdt))

/*
  Read record, and the next time "t"
*/
static float _read_record (atrace *a, float t)
{
  int idx;
  float v;
  int c;
  name_t *prev;

  if (feof (a->tr)) return -1;

  fread_int (a, &idx);
  a->hd_chglist = NULL;

  if (idx < 0) {
    /* EOF marker */
    return -1;
  }
  prev = NULL;
  while (idx != -1) {
    if (prev) {
      prev->chg_next = a->N[idx];
    }
    else {
      a->hd_chglist = a->N[idx];
    }
    a->N[idx]->chg_next = NULL;
    fread_float (a, &v);
    if (idx < 0 || idx >= a->Nnodes) {
      fprintf (stderr, "ERROR: invalid index in trace file (%d)\n", idx);
      fprintf (stderr, "OFFSET: %d\n", (int)  ftell (a->tr));
      exit (1);
    }
    a->N[idx]->v = v;
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
  fread_float (a, &v);
  return v;
}
    


/* read all values into an array 
   node #i, step #j is at position M[Nvsteps*i + j]
*/
void atrace_readall (atrace *a, float *M)
{
  int i, j;
  float t;
  int step, idx;
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
      for (i=0; i < a->Nnodes; i++)
	fread_float (a, &M[a->Nvsteps*i + k]);
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
	fread_float (a, &M[a->Nvsteps*i + k]);
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
      a->N[0]->v = j*a->dt;
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP (a, j*a->dt);
      }
      for (i = 0; i < a->Nnodes; i++) {
	M[a->Nvsteps*i + k] = a->N[i]->v;
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
void atrace_readall_xposed (atrace *a, float *M)
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
      for (i=0; i < a->Nnodes; i++)
	fread_float (a, &M[a->Nnodes*k + i]);
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
	fread_float (a, &M[a->Nnodes*k + i]);
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
      a->N[0]->v = j*a->dt;
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP (a, j*a->dt);
      }
      for (i = 0; i < a->Nnodes; i++) {
	M[a->Nnodes*k + i] = a->N[i]->v;
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
void atrace_readall_nodenum (atrace *a, int node, float *M)
{
  int i;
  int step;
  name_t *m;
  int k;

  Assert (a->read_mode, "atrace_readall_node called in write mode");
  Assert (node >= 0 && node < a->Nnodes, "atrace_readall_node: bad node");

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_TIME_ORDER:
    seek_after_header (a, node*sizeof (float));
    step = (a->Nnodes-1)*sizeof (float);

    for (i=0; i < a->Nsteps; i++) {
      if (a->vdt == a->dt) {
	k = i;
      }
      else {
	k = VSTEP (a, i*a->dt);
      }
      fread_float (a, &M[k]);
      fseek (a->tr, step, SEEK_CUR);
    }
    break;

  case ATRACE_NODE_ORDER:
    seek_after_header (a, node*sizeof(float)*a->Nsteps);
    for (i=0; i < a->Nsteps; i++) {
      if (a->vdt == a->dt) {
	k = i;
      }
      else {
	k = VSTEP (a, i*a->dt);
      }
      fread_float (a, &M[k]);
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
void atrace_readall_block (atrace *a, int node, int num, float *M)
{
  int i;
  int step;
  name_t *m;
  int k;
  int j;
  float t;

  Assert (a->read_mode, "atrace_readall_block called in write mode");
  Assert (node >= 0 && node < a->Nnodes, "atrace_readall_block: bad node");
  Assert (node + num - 1 < a->Nnodes, "atrace_readall_block: bad node");

  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_TIME_ORDER:
    seek_after_header (a, node*sizeof (float));
    step = (a->Nnodes-num)*sizeof (float);

    for (i=0; i < a->Nsteps; i++) {
      if (a->vdt == a->dt) {
	k = i;
      }
      else {
	k = VSTEP (a, i*a->dt);
      }
      for (j=0; j < num; j++) {
	/* node # (node+j), step k */
	fread_float (a, &M[j*a->Nvsteps+k]);
      }
      fseek (a->tr, step, SEEK_CUR);
    }
    break;

  case ATRACE_NODE_ORDER:
    for (j=0; j < num; j++) {
      seek_after_header (a, (j+node)*sizeof(float)*a->Nsteps);
      for (i=0; i < a->Nsteps; i++) {
	if (a->vdt == a->dt) {
	  k = i;
	}
	else {
	  k = VSTEP (a, i*a->dt);
	}
	fread_float (a, &M[j*a->Nvsteps+ k]);
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
	M[k] = j*a->dt;
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
      a->N[0]->v = j*a->dt;
      if (a->vdt == a->dt) {
	k = j;
      }
      else {
	k = VSTEP (a, j*a->dt);
      }
      for (i=0; i < num; i++) {
	M[a->Nvsteps*i + k] = a->N[i+node]->v;
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
void atrace_readall_node (atrace *a, name_t *n, float *M)
{
  int i, j;
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
	M[k] = j*a->dt;
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
      M[k] = a->N[n->idx]->v;
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
void atrace_readall_nodenum_c (atrace *a, int node, float *M, int *C)
{
  int i;
  int step;
  name_t *m;
  int k;

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
void atrace_readall_node_c (atrace *a, name_t *n, float *M, int *C)
{
  int i, j;
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
	M[k] = j*a->dt;
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
      M[k] = a->N[n->idx]->v;
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
      fread_float (a, &a->N[n]->v);
    }
    a->curt = 0;
    a->curstep = 0;
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    a->N[0]->v = 0;
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
	fread_float (a, &a->N[n]->v);
      }
    }
    a->curt = (i-1)*a->dt;
    break;

  case ATRACE_DELTA:
  case ATRACE_DELTA_CAUSE:
    a->curstep += nsteps;
    while (a->nextt >= 0 && a->curt >= 0 && (a->nextt < a->curstep*a->vdt)) {
      a->curt = a->nextt;
      a->N[0]->v = a->curt;
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
      a->N[0]->v = i*a->dt;
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



/* lookup a node */
name_t *atrace_lookup (atrace *a, char *s)
{
  name_t *n;
  hash_bucket_t *b;

  b = hash_lookup (a->H, s);
  if (b) {
    return (name_t *) b->v;
  }
  return NULL;
}

/* create a node */
name_t *atrace_create_node (atrace *a, const char *s)
{
  name_t *n;
  hash_bucket_t *b;

  Assert (!a->locked, "atrace_create_node: Trace file locked!");

  b = hash_lookup (a->H, s);
  if (b) {
    return (name_t *) b->v;
  }
  b = hash_add (a->H, s);

  NEW (n, name_t);
  n->v = 0.0;
  n->up = NULL;
  n->next = n;
  n->b = b;
  n->idx = -1;
  n->chg = 0;
  n->type = 0;
  n->chg_next = NULL;

  b->v = (void *) n;

  a->Nnodes++;
  
  return n;
}

/* alias two nodes */
static void swap (name_t **x, name_t **y)
{
  name_t *t;
  t = *x;  *x = *y;  *y = t;
}

void atrace_alias (atrace *a, name_t *n, name_t *m)
{
  Assert (!a->locked, "atrace_alias: Trace file locked!");

  /* ok, set m to n */
  while (m->up) {
    m = m->up;
  }
  while (n->up) {
    n = n->up;
  }
  if (m == n) 
    return;

  m->up = n;
  swap (&m->next, &n->next);

  a->Nnodes--;
}

static void _emit_record (atrace *a)
{
  int i;
  int flag;
  hash_bucket_t *b;
  name_t *n;

  if (a->curtime == 0) {
    /* special case, initial condition */
    safe_fwrite_float_buf (a, a->curtime*a->dt);

    for (i=0; i < a->H->size; i++)
      for (b = a->H->head[i]; b; b = b->next) {
	n = (name_t *) b->v;
	if (n->up) continue;
	safe_fwrite_int_buf (a, n->idx);
	safe_fwrite_float_buf (a, n->v);
	if (a->fmt == ATRACE_DELTA_CAUSE) {
	  safe_fwrite_int_buf (a, 0);
	}
	n->chg = 0;
      }
    safe_fwrite_int_buf (a, -1);
  }
  else {
    flag = 0;
    for (i=0; i < a->H->size; i++) 
      for (b = a->H->head[i]; b; b = b->next) {
	n = (name_t *) b->v;

	if (n->chg) {
	  if (flag == 0) {
	    safe_fwrite_float_buf (a, a->curtime*a->dt);
	    flag = 1;
	  }
	  safe_fwrite_int_buf (a, n->idx);
	  safe_fwrite_float_buf (a, n->v);
	  if (a->fmt == ATRACE_DELTA_CAUSE) {
	    safe_fwrite_int_buf (a, n->cause);
	  }
	  n->chg = 0;
	}
      }
    if (flag) {
      safe_fwrite_int_buf (a, -1);
    }
  }
}


static void _sig_change_delta (atrace *a, name_t *m, float t, float v)
{
  int step;
  int flag = 0;

  step = ISTEP (a, t);

  if (step >= a->Nsteps) {
    /* Nsteps changed! */
    a->Nsteps = step + 1;
    a->stop_time = step * a->dt;
  }

  Assert (step < a->Nsteps, "Invalid stop time!");
  Assert (step >= a->curtime, "Going backward in time?");

  if (step == a->curtime) {
    if (m->v != v || DONT_FILTER_DELTAS(m->type)) {
      m->v = v;
      m->chg = 1;
    }
    return;
  }

  _emit_record (a);		/* emit record */
  
  a->curtime = step;
  if (m->v != v || DONT_FILTER_DELTAS(m->type)) {
    m->v = v;
    m->chg = 1;
  }
}

static void _sig_change_delta_cause (atrace *a, name_t *m, float t, float v, int idx)
{
  int step;
  int flag = 0;

  step = ISTEP (a, t);

  if (step >= a->Nsteps) {
    /* Nsteps changed! */
    a->Nsteps = step + 1;
    a->stop_time = step * a->dt;
  }

  Assert (step < a->Nsteps, "Invalid stop time!");
  Assert (step >= a->curtime, "Going backward in time?");

  if (step == a->curtime) {
    if (m->v != v || DONT_FILTER_DELTAS(m->type)) {
      m->v = v;
      m->cause = idx;
      m->chg = 1;
    }
    return;
  }

  _emit_record (a);		/* emit record */
  
  a->curtime = step;
  if (m->v != v || DONT_FILTER_DELTAS(m->type)) {
    m->v = v;
    m->cause = idx;
    m->chg = 1;
  }
}


static void _sig_delta_end (atrace *a)
{
  _emit_record (a);

  /* repeat empty sequence */
  safe_fwrite_float_buf (a, a->curtime*a->dt);
  safe_fwrite_int_buf (a, -1);
}


static void _sig_change_timeorder (atrace *a, name_t *n, float t, float v)
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
	safe_fwrite_float_buf (a, n->v);
      }
    a->curtime++;
  }
  Assert (a->curtime == step, "What?");
  m->v = v;
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
	safe_fwrite_float_buf (a, n->v);
      }
    a->curtime++;
  }
}

static void _sig_change_nodeorder (atrace *a, name_t *n, float t, float v)
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
	safe_fwrite_float_buf (a, a->nprev->v);
	a->curtime++;
      }
    }
    a->nprev = n;
    /* Ok, we're in good shape */
    safe_fwrite_float_buf (a, v);
    a->curtime = 1; /* next expected at time 1 * dt */
    n->v = v;
  }
  else {
    while (a->curtime < step) {
      safe_fwrite_float_buf (a, n->v);
      a->curtime++;
    }
    safe_fwrite_float_buf (a, v);
    n->v = v;
    a->curtime = step + 1;
  }
}

static void _sig_nodeorder_end (atrace *a)
{
  int step;
  int last_idx;

  Assert (a->nprev != NULL && a->nprev->idx == a->Nnodes-1, "What on earth?");
    
  /* finish writing out last node */
  while (a->curtime < a->Nsteps) {
    safe_fwrite_float_buf (a, a->nprev->v);
    a->curtime++;
  }
}


/* signal change */
void atrace_signal_change (atrace *a, name_t *n, float t, float v)
{
  emit_header_aux (a);

  while (n->up)
    n = n->up;
  
  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_NODE_ORDER:
    _sig_change_nodeorder (a, n, t, v);
    break;
  case ATRACE_TIME_ORDER:
    _sig_change_timeorder (a, n, t, v);
    break;
  case ATRACE_DELTA_CAUSE:
    _sig_change_delta_cause (a, n, t, v, 0);
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
void atrace_signal_change_cause (atrace *a, name_t *n, float t, float v, name_t *c)
{
  emit_header_aux (a);

  while (n->up)
    n = n->up;
  
  switch (ATRACE_FMT(a->fmt)) {
  case ATRACE_NODE_ORDER:
    _sig_change_nodeorder (a, n, t, v);
    break;
  case ATRACE_TIME_ORDER:
    _sig_change_timeorder (a, n, t, v);
    break;
  case ATRACE_DELTA_CAUSE:
    _sig_change_delta_cause (a, n, t, v, c->idx);
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
  /* free stuff; emit offset table if required */
  if (a->read_mode == 0) {
    emit_header_aux (a);
    atrace_signal_done (a);
    write_header (a, 1);
  }
  fclose (a->tr);
  hash_free (a->H);
  FREE (a->file);
  FREE (a->tfile);
  if (a->N)
    FREE (a->N);
  if (a->buffer)
    FREE (a->buffer);
  FREE (a);
}
