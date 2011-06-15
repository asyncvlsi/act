/****************************************************************************
 *
 *  An implementation of LZW Compression/Decompression
 *
 *  Author: Rajit Manohar
 *  Date: Thu Jul 28 20:29:01 PDT 1994
 *
 *  Note:
 *    The purpose of the routines provided by this file is to provide a simple
 *  interface that enables the user to read and write compressed files using
 *  a lossless compression scheme.
 *
 *  The compression scheme used is a modified version of the scheme outlined
 *  in the article:
 *  Welsh, T. "A High Performance Algorithm for Data Compression", 
 *  Computer, July 1994. Pages 8-17.
 *
 *  The following functions are provided:
 *
 *  void c_openread (int *file_handle, char *filename, int *len) 
 *     This function tries to open the file specifed by "filename" 
 *  for reading. The file is assumed to be saved using the routines
 *  provided by in this file. "*len" is the length of the string in 
 *  "filename". "*file_handle" will contain the file handle which is to
 *  be used to refer to the file. If an error occurred while opening the
 *  file, "*file_handle" will be -1. 
 *
 *  void c_openwrite (int *file_handle, char *filename, int *len)
 *     Similar to c_openread, only opens the file for writing. It destroys
 *  any existing file with the same name.
 *
 *  void c_close (int *file_handle)
 *     Closes the file. THIS IS IMPORTANT! If the file is not closed, then
 *  the contents of the file may be lost. 
 *
 *  int c_read (int *file_handle, char *array, int *size)
 *     Tries to read in "*size" values into "array" from "file_handle". 
 *  Returns the number of values actually read in.
 *
 *  int c_write (int *file_handle, char *array, int *size)
 *     Tries to write out "*size" values from "array" into "file_handle".
 *  It returns the number of values actually written to the file.
 *
 *****************************************************************************/



#include <stdio.h>
#include "misc.h"
#include "lzw.h"
#include "avl.h"

#define MAX_TABLE_SIZE 4096	/* table size for LZW */


typedef unsigned char byte;	/* C defines sizeof(char) to be 1. */
				/* So, this should be the same no matter */
				/* no matter what the architechture of the */
				/* machine you are using is. */

typedef enum { False = 0, True = 1 } boolean;
				/* Booleans used all over the place */

/***** Compression Table *****/

typedef struct {
  FILE *fp;			/* FILE pointer for file. Use buffered I/O. */
  boolean read;			/* true if the file was opened for reading */
  byte tab[MAX_TABLE_SIZE];	/* string table used by LZW algorithm */
  byte iobuf[3];		/* input/output buffer */
  boolean empty;		/* true if the buffer is empty */
  boolean start;		/* true if started already */
  boolean eof;			/* true if eof */
  union {
    struct {
      int  link[MAX_TABLE_SIZE];    /* back links: used for decompression */
      byte buf[MAX_TABLE_SIZE];
      byte *stack;
      byte finchar;
      int oldcode, code, incode;
    } d;			/* decompression data structures */
    struct {
      avl_t *forw[MAX_TABLE_SIZE]; /* forward links: used for compression */
                                    /* avl_t is a search tree ADT */
      int location;		
    } c;			/* compression data structures */
  } u;
  int size;			/* sizeof used table */
} Table;

static Table *cbuf[MAX_OPEN_FILES]; /* compression table */

static boolean used[MAX_OPEN_FILES]; /* true if used, false otherwise */


/*------------------------------------------------------------------------
  Returns a new slot from the compression table.
------------------------------------------------------------------------*/
static int new_slot (void)
{
  int i;
  for (i=0; i < MAX_OPEN_FILES; i++)
    if (!used[i])
      return i;
  return -1;
}


/*------------------------------------------------------------------------
  Initializes all data structures.
------------------------------------------------------------------------*/
static void initialize_func (void)
{
  static int init = 0;
  int i, j;
  if (init) return;
  if (!init) {
    init = 1;
    for (i=0; i < MAX_OPEN_FILES; i++)
      used[i] = False;
  }
}

 
/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
FILE *c_fopen_r (const char *s)
{
  int i, j;

  initialize_func ();
  
  if ((i = new_slot()) == -1) {
    printf ("Warning: c_fopen_r: Too many open files. Maximum allowed: %d\n",
	    MAX_OPEN_FILES);
    return NULL;
  }
  used[i] = True;
  cbuf[i] = (Table *)malloc(sizeof(Table));
  if (!cbuf[i]) {
	printf ("MALLOC FAILED. size=%lu\n", sizeof(Table));
	exit (1);
  }
  if (!(cbuf[i]->fp = fopen(s, "rb"))) {
    printf ("Warning: c_openread: Could not open file [%s] for reading.\n",
	    s);
    return NULL;
  }
  cbuf[i]->read = True;
  for (j=0; j < 256; j++) {
    cbuf[i]->tab[j] = j;
    cbuf[i]->u.d.link[j] = -1;
  }
  cbuf[i]->size = 256;
  cbuf[i]->empty = True;
  cbuf[i]->start = True;
  cbuf[i]->eof = False;
  if ((j = fread (cbuf[i]->iobuf, 1, 3, cbuf[i]->fp)) != 3)
    if (j != 0)
      cbuf[i]->empty = True;
    else
      cbuf[i]->eof = True;
  else
    cbuf[i]->empty = False;

  return (FILE *) (i+1);
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
FILE *c_fopen_w (const char *s)
{
  int i, j;

  initialize_func ();
  
  if ((i = new_slot()) == -1) {
    printf ("Warning: c_openwrite: Too many open files. Maximum allowed: %d\n",
	    MAX_OPEN_FILES);
    return NULL;
  }
  used[i] = True;
  cbuf[i] = (Table *)malloc(sizeof(Table));
  if (!cbuf[i]) {
	printf ("MALLOC FAILED. size=%lu\n", sizeof(Table));
	exit (1);
  }
  if (!(cbuf[i]->fp = fopen(s, "wb"))) {
    printf ("Warning: c_openwrite: Could not open file [%s] for writing.\n",
	    s);
    return NULL;
  }
  cbuf[i]->read = False;
  for (j=0; j < 256; j++) {
    cbuf[i]->tab[j] = j;
    cbuf[i]->u.c.forw[j] = NULL;
  }
  cbuf[i]->size = 256;
  cbuf[i]->empty = True;
  cbuf[i]->start = True;
  cbuf[i]->eof = False;
  cbuf[i]->u.c.location = -1;

  return (FILE*) (i+1);
}


/*------------------------------------------------------------------------
  Puts a 12-bit value into the output buffer/file.
  entry - pointer to the compression table.
  val   - value to be written out.
------------------------------------------------------------------------*/
static void put12 (int entry, int val)
{
  if (cbuf[entry]->empty) {
    /* empty */
    cbuf[entry]->iobuf[0] = val%256;
    cbuf[entry]->iobuf[1] = val/256;
    cbuf[entry]->empty = False;
  }
  else {
    int i, x;

    /* half-full */
    cbuf[entry]->iobuf[1] |= ((val/256) << 4);
    cbuf[entry]->iobuf[2] = val%256;

    i = 0;
    do {
      x = fwrite (cbuf[entry]->iobuf + i, 1, 3-i, cbuf[entry]->fp);
      if (x+i != 3) {
	printf ("lzw.c:put12: fwrite failed, retrying...\n");
	sleep (5);
	i += x;
	x = 0;
      }
    } while (x+i != 3);
    cbuf[entry]->empty = True;
  }
}


/*------------------------------------------------------------------------
  Reads in a 12-bit value into *val.
  entry - pointer to the compression table.
  *val   - value to be read in.
------------------------------------------------------------------------*/
static void get12 (int entry, int *val)
{
  int i;
  if (cbuf[entry]->eof && cbuf[entry]->empty) {
    *val = -1;
  }
  else {
    if (!cbuf[entry]->empty) {
      /* full */
      *val = cbuf[entry]->iobuf[0];
      *val += 256*(cbuf[entry]->iobuf[1] & 0x0F);
      cbuf[entry]->empty = True;
    }
    else {
      /* half-full */
      *val = cbuf[entry]->iobuf[2];
      *val += 256*(cbuf[entry]->iobuf[1] >> 4);
      if ((i = fread (cbuf[entry]->iobuf, 1, 3, cbuf[entry]->fp)) != 3)
	if (i != 0) {
	  cbuf[entry]->eof = True;
	  cbuf[entry]->empty = False;
	}
	else {
	  cbuf[entry]->eof = True;
	  cbuf[entry]->empty = True;
	}
      else
	cbuf[entry]->empty = False;
    }
  }
}  
 

/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
void c_fclose (FILE *fp)
{
  int i, f;
  
  f = ((int) fp) - 1;

  Assert ((f >= 0) && (f < MAX_OPEN_FILES), "c_fclose: Invalid file handle.");
  Assert (used[f], "c_close: Invalid file handle.");
  if (!cbuf[f]->read && (cbuf[f]->u.c.location != -1)) {
    put12 (f, cbuf[f]->u.c.location);
    if (!cbuf[f]->empty)
      fwrite (cbuf[f]->iobuf, 1, 2, cbuf[f]->fp);
  }
  fclose (cbuf[f]->fp);
  used[f] = False;
  if (!cbuf[f]->read)
    for (i=0; i < cbuf[f]->size; i++)
      avl_free (cbuf[f]->u.c.forw[i]);
  free (cbuf[f]);
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
int c_fwrite (char *buf, int sz, int n, FILE *fp)
{
  int i;
  int st;
  int pos;
  int f;
  Table *t;

  f = ((int)fp) - 1;

  Assert ((f >= 0) && (f < MAX_OPEN_FILES), 
	  "c_fwrite: Invalid file handle.");
  Assert (used[f], "c_fwrite: Invalid file handle.");
  Assert (cbuf[f]->read == False, 
	  "c_fwrite: Specified file was opened for reading.");
  /* hmm... check special case for startup */
  t = cbuf[f];
  st = 0;
  if (t->start) {
    t->start = False;
    t->u.c.location = (unsigned char) buf[0];
    st = 1;
  }
  /* hmm... */
  /* do compression stuff */
  for (i=st; i < sz*n; i++) {
    if (pos = (int)avl_search (t->u.c.forw[t->u.c.location],(int)buf[i])) 
      t->u.c.location = pos-1;
    else {
      if (t->size < (MAX_TABLE_SIZE-1)) {
	t->tab[t->size] = buf[i];
	if (!t->u.c.forw[t->u.c.location])
	  t->u.c.forw[t->u.c.location] = 
	    avl_new ((int)buf[i],(void*)(t->size+1));
	else
	  avl_insert (t->u.c.forw[t->u.c.location], (int)buf[i],
		       (void*)(t->size+1));
	t->u.c.forw[t->size] = NULL;
	t->size++;
	
	put12 (f, t->u.c.location);

	if (t->size == (MAX_TABLE_SIZE-1)) {
	  int j;
	  put12 (f, MAX_TABLE_SIZE-1);
	  for (j=0; j < t->size; j++) {
	    avl_free (t->u.c.forw[j]);
	    t->u.c.forw[j] = NULL;
	  }
	  t->size = 256;
	}
	t->u.c.location = (unsigned char)buf[i];
      }
      else
	Assert (0, "Huh?");
    }
  }
  return n;
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
int c_fread (char *buf, int sz, int n, FILE *fp)
{
  int i, st;
  int bufpos;
  int count;
  Table *t;
  int f;

  f = ((int)fp)-1;

  Assert ((f >= 0) && (f < MAX_OPEN_FILES), 
	  "c_read: Invalid file handle.");
  Assert (used[f], "c_read: Invalid file handle.");
  Assert (cbuf[f]->read == True, 
	  "c_read: Specified file was opened for writing.");

  t = cbuf[f];
  st = 0;
  bufpos = 0;
  count = 0;
  if (t->start) {
    t->start = False;

    get12 (f, &t->u.d.code);
    if (t->u.d.code == -1) return 0;
    t->u.d.oldcode = t->u.d.code;
    buf[0] = t->tab[t->u.d.code];
    bufpos = 1;
    count = 1;
    st = 0;
    if (count == sz) {
      st++;
      count = 0;
    }
    t->u.d.finchar = t->tab[t->u.d.code];
    t->u.d.stack = t->u.d.buf;
  }
  while (st != n) {
    /* number of elements */
    Assert (bufpos == count + st*sz, "What?");
    while (t->u.d.stack != t->u.d.buf && (st != n)) {
      buf[bufpos++] = *--t->u.d.stack;
      count++;
      if (count == sz) {
	count = 0;
	st++;
      }
    }
    if (st == n) return st;

    /* do one step */
    get12 (f, &t->u.d.code);

#define PUTBACK					\
    do {					\
      while (count > 0) {			\
	*t->u.d.stack++ = buf[--bufpos];	\
	count--;				\
      }						\
    } while (0)

    /* could not read any more */
    if (t->u.d.code == -1) {
      PUTBACK;
      return st;
    }

    if (t->u.d.code == (MAX_TABLE_SIZE-1)) {
      t->size = 256;
      get12 (f, &t->u.d.code);
      t->u.d.oldcode = t->u.d.code;
      if (t->u.d.code == -1) {
	PUTBACK;
	return st;
      }
      
      buf[bufpos++] = t->tab[t->u.d.code];
      count++;
      if (count == sz) {
	count = 0;
	st++;
      }
      t->u.d.finchar = t->tab[t->u.d.code];
    }
    else {
      t->u.d.incode = t->u.d.code;
      if (t->u.d.code >= t->size) {
	*t->u.d.stack++ = t->u.d.finchar;
	t->u.d.code = t->u.d.oldcode;
      }
      while (t->u.d.link[t->u.d.code] != -1) {
	*t->u.d.stack++ = t->tab[t->u.d.code];
	t->u.d.code = t->u.d.link[t->u.d.code];
      }
      t->u.d.finchar = *t->u.d.stack++ = t->tab[t->u.d.code];
      if (t->size < (MAX_TABLE_SIZE-1)) {
	t->tab[t->size] = t->u.d.finchar;
	t->u.d.link[t->size] = t->u.d.oldcode;
	t->size++;
      }
      t->u.d.oldcode = t->u.d.incode;
    }
  }
  return st;
}



/*------------------------------------------------------------------------
 * Compressed cgets
 *------------------------------------------------------------------------
 */
char *c_fgets (char *buf, int len, FILE *fp)
{
  char *s = buf;
  int flag = 0;

  buf[len-1] = '\0';
  len--;
  while (len--) {
    if (c_fread (buf, 1, 1, fp) == 1) {
      flag = 1;
      if (*buf == '\n' && len > 0) {
	buf[1] = '\0';
	return s;
      }
    }
    else {
      buf[0] = '\0';
      if (flag == 0) {
	return NULL;
      }
      else {
	return s;
      }
    }
    buf++;
  }
}
