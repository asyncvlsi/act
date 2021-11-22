/*************************************************************************
 *
 *  Disk hash table for names and aliases
 *
 *  Copyright (c) 2008, 2019 Rajit Manohar
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
#include <unistd.h>
#include <string.h>
#include "act/common/names.h"
#include "act/common/misc.h"
#include "act/common/hash.h"

/*
  IDX table:
       ENDIANNESS-INFO  [1 IDX_TYPE]
       [list of <index into string table>]

  STRING table:
       <string>\0<string>\0 ....

  ALIAS TABLE
       <one entry per idx>; points to unique root IDX

  REVIDX table:
       USE +1 collision strategy; default size: 2 * max_size


*/

#define LAZY_WRITE

static void safe_fwrite (const void *obj, unsigned long size, unsigned long num, FILE *fp)
{
  unsigned long old;

  /* speed this up! */
  old = ftell (fp);
  while (fwrite (obj, size, num, fp) != num) {
    fprintf (stderr, "fwrite failed, retrying..\n");
    sleep (60);
    fseek (fp, old, SEEK_SET);
  }
}

static void safe_fread (void *obj, unsigned long size, unsigned long num, FILE *fp)
{
  unsigned long old;

  /* speed this up! */
  old = ftell (fp);
  while (fread (obj, size, num, fp) != num) {
    fprintf (stderr, "fread @ %ld failed.. %d\n", old, ferror (fp));
    exit (1);
  }
}

static void fread_idxtype (IDX_TYPE *i, NAMES_T *N, FILE *fp)
{
  unsigned char *b, *c;
  IDX_TYPE t;

  Assert (IDX_SIZE == 4, "eh?");

  if (fread (i, IDX_SIZE, 1, fp) != 1) {
    fatal_error ("fread failed!");
  }
  if (N->reverse_endian) {
    t = *i;
    b = (unsigned char *)&t;
    c = (unsigned char *)i;
    c[0] = b[3];
    c[1] = b[2];
    c[2] = b[1];
    c[3] = b[0];
  }
}
  
  

/*------------------------------------------------------------------------
 *
 *  names_init --
 *
 *   Initialize standard fields
 *
 *------------------------------------------------------------------------
 */
static NAMES_T *names_init (char *file)
{
  NAMES_T *N;
  char buf[10240];
  unsigned long sz;
  int i;

  NEW (N, NAMES_T);

  Assert (sizeof (IDX_TYPE) == IDX_SIZE, "eh what");
  
  sprintf (buf, "%s_str.dat", file);
  N->string_tab = Strdup (buf);
  N->sfp = NULL;

  sprintf (buf, "%s_idx.dat", file);
  N->idx_tab = Strdup (buf);
  N->ifp = NULL;

  sprintf (buf, "%s_rev.dat", file);
  N->idx_revtab = Strdup (buf);
  N->rfp = NULL;

  sprintf (buf, "%s_alias.dat", file);
  N->alias_tab = Strdup (buf);
  N->afp = NULL;

  N->fpos = 0;
  N->mode = -1;
  N->count = 0;
  N->update_hash = 0;
  N->reverse_endian = 0;
  N->hsize = 0;
  N->unique_names = 0;

  return N;
}

/*------------------------------------------------------------------------
 *
 *  names_create --
 *
 *   Create names database on disk
 *
 *------------------------------------------------------------------------
 */
NAMES_T *names_create (char *file, IDX_TYPE max_names)
{
  NAMES_T *N;
  IDX_TYPE endian;
  unsigned long sz;
  int i;

  N = names_init (file);
  N->mode = NAMES_WRITE;
  N->reverse_endian = 0;

  sz = max_names*2;

  for (N->hsize = 1; sz > N->hsize; N->hsize <<= 1)
    ;

  N->sfp = fopen (N->string_tab, "wb+");
  if (!N->sfp) fatal_error ("Could not open file `%s' for writing", 
			    N->string_tab);
  
  N->ifp = fopen (N->idx_tab, "wb");
  if (!N->ifp) fatal_error ("Could not open file `%s' for writing", 
			    N->idx_tab);

#ifndef LAZY_WRITE
  N->rfp = fopen (N->idx_revtab, "wb+");
  if (!N->rfp) fatal_error ("Could not open file `%s' for writing", 
			    N->idx_revtab);
  fseek (N->rfp, (N->hsize-1)*IDX_SIZE, SEEK_SET);
  endian = 0;
  safe_fwrite (&endian, IDX_SIZE, 1, N->rfp);
  fseek (N->rfp, 0L, SEEK_SET);
#endif

  N->afp = fopen (N->alias_tab, "wb+");
  if (!N->afp) fatal_error ("Could not open file `%s' for writing",
			    N->alias_tab);

  endian = 0x12345678;
  safe_fwrite (&endian, IDX_SIZE, 1, N->ifp);

  return N;
}


/*------------------------------------------------------------------------
 *
 *  names_newname --
 *
 *   Create a new name
 *
 *------------------------------------------------------------------------
 */
IDX_TYPE names_newname (NAMES_T *N, char *str)
{
  int move = 0;
  IDX_TYPE idx;
  IDX_TYPE n;

  if (N->mode != NAMES_WRITE) {
    warning ("names_newname: ignored, since not in write mode");
    return 0;
  }

  /* 
     1. Write string table position to the forward index table 
  */
  safe_fwrite (&N->fpos, IDX_SIZE, 1, N->ifp);

  /*
    2. Write out string
  */
  safe_fwrite (str, sizeof (char), strlen (str)+1, N->sfp);
  N->fpos += strlen(str)+1;

  
  N->count++;

#ifndef LAZY_WRITE
  /*
    3. Write inverse table entry 
  */
 try_again:
  move = 0;
  idx = hash_function (N->hsize, str);
  while (move < 32) {
    fseek (N->rfp, ((idx+move)&(N->hsize-1))*IDX_SIZE, SEEK_SET);
    safe_fread (&n, IDX_SIZE, 1, N->rfp);
    if (n == 0) {
      /* found an empty slot! */
      fseek (N->rfp, ((idx+move) & (N->hsize-1))*IDX_SIZE, SEEK_SET);
      safe_fwrite (&N->count, IDX_SIZE, 1, N->rfp);
      break;
    }
    move++;
  }
  if (move == 32) {
    char buf[102400];
    int count, i = 0;

    /* re-create the hash table! */
    fclose (N->rfp);
    N->hsize <<= 1;
    N->rfp = fopen (N->idx_revtab, "wb+");
    fseek (N->rfp, (N->hsize-1)*IDX_SIZE, SEEK_SET);
    n = 0;
    safe_fwrite (&n, IDX_SIZE, 1, N->rfp);
    fseek (N->rfp, 0L, SEEK_SET);

    /* ok, walk through the strings! */
    fseek (N->sfp, 0L, SEEK_SET);
    count = 0;
    while (ftell (N->sfp) != N->fpos) {
      count++;
      i = 0;
      do {
	Assert (i < 102400, "What?");
	if (fread (buf+i, sizeof (char), 1, N->sfp) != 1) {
	  fatal_error ("EOF?!");
	}
	i++;
      } while (buf[i-1] != '\0');
      idx = hash_function (N->hsize, buf);
      i = 0;
      do {
	fseek (N->rfp, ((idx+i) & (N->hsize-1))*IDX_SIZE, SEEK_SET);
	safe_fread (&n, IDX_SIZE, 1, N->rfp);
	i++;
      } while (n != 0);
      i--;
      fseek (N->rfp, ((idx+i)&(N->hsize-1))*IDX_SIZE, SEEK_SET);
      safe_fwrite (&count, IDX_SIZE, 1, N->rfp);
    }
    fseek (N->sfp, N->fpos, SEEK_SET);
    N->update_hash = 1;
    goto try_again;
  }
#endif

  idx = 0;
  /*
    4. Write out empty alias entry
  */
  safe_fwrite (&idx, IDX_SIZE, 1, N->afp);

  return N->count;
}


/*------------------------------------------------------------------------
 *
 *  names_addalias --
 *
 *   Add an alias to the current name: str = str2: both must already
 *   be present
 *
 *------------------------------------------------------------------------
 */
void names_addalias (NAMES_T *N, IDX_TYPE idx1, IDX_TYPE idx2)
{
  IDX_TYPE n1, n2;
  IDX_TYPE up;
  unsigned long pos;
  
  if (N->mode != NAMES_WRITE) {
    warning ("names_addalias: ignored, since not in write mode");
    return;
  }

  pos = ftell (N->afp);

  n1 = idx1;
  fseek (N->afp, (idx1-1)*IDX_SIZE, SEEK_SET);
  fread_idxtype (&up, N, N->afp);
  while (up != 0) {
    n1 = up;
    fseek (N->afp, (up-1)*IDX_SIZE, SEEK_SET);
    fread_idxtype (&up, N, N->afp);
  }

  n2 = idx2;
  fseek (N->afp, (idx2-1)*IDX_SIZE, SEEK_SET);
  fread_idxtype (&up, N, N->afp);
  while (up != 0) {
    n2 = up;
    fseek (N->afp, (up-1)*IDX_SIZE, SEEK_SET);
    fread_idxtype (&up, N, N->afp);
  }

  if (n1 != n2) {
    fseek (N->afp, (n2-1)*IDX_SIZE, SEEK_SET);
    safe_fwrite (&n1, IDX_SIZE, 1, N->afp);
  }

  fseek (N->afp, pos, SEEK_SET);
}


/*------------------------------------------------------------------------
 *
 *  Close names file table
 *
 *------------------------------------------------------------------------
 */
void names_close (NAMES_T *N)
{
  if (N->ifp) fclose (N->ifp);
  if (N->afp) fclose (N->afp);
  if (N->sfp) fclose (N->sfp);
  FREE (N->idx_tab);
  FREE (N->alias_tab);

#ifdef LAZY_WRITE
  if (N->mode == NAMES_WRITE) {
    IDX_TYPE idx, idx2;
    IDX_TYPE *idx_tmp;
    unsigned long i;
    char buf[102400];
    /* create inverse hash table */
    N->hsize = 1;
    while (N->count > N->hsize) {
      N->hsize <<= 1;
    }
    /* update N->hsize */

    N->rfp = fopen (N->idx_revtab, "wb+");
    if (!N->rfp) fatal_error ("Could not open file `%s' for writing", 
			      N->idx_revtab);
    fseek (N->rfp, (N->hsize-1)*IDX_SIZE, SEEK_SET);
    idx = 0;
    safe_fwrite (&idx, IDX_SIZE, 1, N->rfp);
    fseek (N->rfp, 0L, SEEK_SET);

    /* create the table in memory */
    MALLOC (idx_tmp, IDX_TYPE, N->hsize);

    /* initialize */
    for (i=0; i < N->hsize; i++) {
      idx_tmp[i] = 0;
    }

    /* sequentially read the string table, and create the appropriate
       hash table entry */
    N->sfp = fopen (N->string_tab, "rb");
    idx = 0;
    while (!feof (N->sfp)) {
      idx++;
      i = 0;
      do {
	Assert (i < 102400, "What?");
	if (fread (buf+i, sizeof (char), 1, N->sfp) != 1) {
	  if (i == 0) goto finished;
	  fatal_error ("EOF?!");
	}
	i++;
      } while (buf[i-1] != '\0');
      i = hash_function (N->hsize, buf);
      while (idx_tmp[i] != 0) {
	i = (i+1) & (N->hsize-1);
      }
      idx_tmp[i] = idx;
    }
  finished:
    safe_fwrite (idx_tmp, IDX_SIZE, N->hsize, N->rfp);
    FREE (idx_tmp);
  }
#endif

  if (N->sfp) fclose (N->sfp);
  FREE (N->string_tab);

  if (N->rfp) fclose (N->rfp);
  FREE (N->idx_revtab);
  
  FREE (N);
}



/*------------------------------------------------------------------------
 *
 *  names_open --
 *
 *   Open names file for reading.
 *
 *------------------------------------------------------------------------
 */
NAMES_T *names_open (char *file)
{
  NAMES_T *N;
  IDX_TYPE endian;
  unsigned long sz;
  int i;

  N = names_init (file);
  N->mode = NAMES_READ;

  /* ok find out endianness */
  N->reverse_endian = 0;

  N->sfp = fopen (N->string_tab, "rb");
  if (!N->sfp) 
    fatal_error ("Could not open file `%s' for reading", N->string_tab);
  
  N->ifp = fopen (N->idx_tab, "rb");
  if (!N->ifp) fatal_error ("Could not open file `%s' for reading", 
			    N->idx_tab);

  fseek (N->ifp, 0, SEEK_END);
  N->unique_names = (ftell (N->ifp)/IDX_SIZE)-1;
  fseek (N->ifp, 0, SEEK_SET);

  N->rfp = fopen (N->idx_revtab, "rb");
  if (!N->rfp) fatal_error ("Could not open file `%s' for reading", 
			    N->idx_revtab);
  fseek (N->rfp, 0, SEEK_END);
  N->hsize = (ftell (N->rfp)/IDX_SIZE);
  fseek (N->rfp, 0, SEEK_SET);

  safe_fread (&endian, IDX_SIZE, 1, N->ifp);
  if (endian != 0x12345678)
    N->reverse_endian = 1;

  N->afp = fopen (N->alias_tab, "rb");
  if (!N->afp) {
    fatal_error ("Could not open file `%s' for reading", N->alias_tab);
  }

  return N;
}


/*------------------------------------------------------------------------
 *
 *  names_num2name --
 *
 *   Convert an index to a name
 *
 *------------------------------------------------------------------------
 */
char *names_num2name (NAMES_T *N, IDX_TYPE num)
{
  IDX_TYPE idx;
  static char buf[102400];
  int i;

  if (N->mode != NAMES_READ) {
    return NULL;
  }

  if (num < 1 || num > N->unique_names) {
    return NULL;
  }

  /* position = num (since nums start from 1, and there are two
     IDX_TYPE's in the file */
  fseek (N->ifp, num*IDX_SIZE, SEEK_SET);
  fread_idxtype (&idx, N, N->ifp);
  
  /* ok! we now have the offset to the string table */
  fseek (N->sfp, idx, SEEK_SET);

  /* read in the string! */
  i = 0;
  do {
    Assert (i < 102400, "What?");
    if (fread (buf+i, sizeof (char), 1, N->sfp) != 1) {
      fatal_error ("End of file?!");
    }
    i++;
  } while (buf[i-1] != '\0');

  return buf;
}


/*------------------------------------------------------------------------
 *
 *  names_str2num --
 *
 *   Convert a string into a number
 *
 *------------------------------------------------------------------------
 */
IDX_TYPE names_str2name (NAMES_T *N, char *s)
{
  int i;
  IDX_TYPE idx;
  char *n;

  /* string -> hash
     look through rev table buckets sequentially
  */
  if (N->mode != NAMES_READ) {
    return 0;
  }

  idx = hash_function (N->hsize, s);

  i = 0;
  while (1) {
    fseek (N->rfp,((idx+i)& N->hsize-1)*IDX_SIZE, SEEK_SET);
    fread_idxtype (&idx, N, N->rfp);
    if (idx == 0)
      break;
    n = names_num2name (N, idx);
    if (strcmp (n, s) == 0) {
      return idx;
    }
  }
  return 0;
}


/*------------------------------------------------------------------------
 *
 *  names_parent --
 *
 *   Parent pointer in alias table
 *
 *------------------------------------------------------------------------
 */
IDX_TYPE names_parent (NAMES_T *N, IDX_TYPE idx)
{
  if (N->mode != NAMES_READ) return 0;

  fseek (N->afp, (idx-1)*IDX_SIZE, SEEK_SET);
  fread_idxtype (&idx, N, N->afp);
  return idx;
}
