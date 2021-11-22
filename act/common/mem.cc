/*************************************************************************
 *
 *  Standard interface to ASCII file that represents memory contents
 *
 *  Copyright (c) 1999, 2019 Rajit Manohar
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
#include "act/common/mem.h"
#include "act/common/misc.h"
#include "act/common/sim.h"
#include "act/common/thread.h"

#define CHUNK_SIZE 16384
#define CHUNK_THRESHOLD 16 /* gc threshold */
static int adj_merge_size = 16384;
#define ADJ_LIMIT 1024*32

#define DOUBLE_THRESHOLD (1024*1024*256)

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/*
 * free memory image
 */
void Mem::freemem (void)
{
  int i;

  context_disable ();

  if (chunks > 0) {
    for (i=0; i < chunks; i++)
      FREE (mem[i]);
    FREE (mem);
    FREE (mem_len);
    FREE (mem_addr);
    chunks = 0;
    numchunks = 0;
    last_chunk_collect = 0;
  }

  context_enable ();
}

Mem::~Mem (void)
{
  freemem();
}

Mem::Mem (void)
{
  chunks = 0;
  numchunks = 0;
  last_chunk_collect = 0;
}

/*
 * add one more slot to memory array
 */
void Mem::get_more (int ch, long long num, int merging)
{
  long long newnum;
  unsigned long long t;
  if (num <= 0) return;
  
  if (!merging) {
    Assert ((ch == chunks - 1) ||
	    (mem_len[ch] + num + mem_addr[ch] - 1 < mem_addr[ch+1]),
	    "Overlap violation");

    if (mem_len[ch] < DOUBLE_THRESHOLD)
      newnum = mem_len[ch];
    else 
      newnum = CHUNK_SIZE;

    if (newnum < num)
      newnum = num;

    if (ch < chunks - 1) {
      /* double if possible */
      if (mem_addr[ch] + mem_len[ch] + newnum - 1 < mem_addr[ch+1]) {
	num = newnum;
      }
      else {
	num = mem_addr[ch+1] - mem_addr[ch] - mem_len[ch];
      }
    }
    else {
      num = newnum;
    }
    mem_len[ch] += num;
    REALLOC (mem[ch], LL, mem_len[ch]);
    for (t=mem_len[ch]-1;t>=mem_len[ch]-num;t--) {
       mem[ch][t] = MEM_BAD;
    }
    return;
  }
  /* merging */
  if (mem_len[ch] == 0) {
    mem_len[ch] = num;
    MALLOC (mem[ch], LL, mem_len[ch]);
    for (t=0;t<=mem_len[ch]-1;t++) {
       mem[ch][t] = MEM_BAD;
    }
  }
  else {
    mem_len[ch] += num;
    REALLOC (mem[ch], LL, mem_len[ch]);
    for (t=mem_len[ch]-1;t>=mem_len[ch]-num;t--) {
       mem[ch][t] = MEM_BAD;
    }
  }
}

/*
 * create new chunk, at position "chunks-1"
 *
 * Return values:
 *
 *   1  => created a new chunk at array position "chunks-1"
 *   0  => resized array at position ch
 */
int Mem::create_new_chunk (LL addr, int ch)
{
  int i, gap;
  static int last_create = -1;

  if (chunks == numchunks) {
    if (numchunks == 0) {
      numchunks = 4;
      MALLOC (mem, LL *, numchunks);
      MALLOC (mem_len, unsigned long, numchunks);
      MALLOC (mem_addr, LL, numchunks);
    }
    else {
      numchunks *= 2;
      REALLOC (mem, LL *, numchunks);
      REALLOC (mem_len, unsigned long, numchunks);
      REALLOC (mem_addr, LL, numchunks);
    }
  }
  if (0 <= ch && ch < chunks) {
    /* the new chunk is right next to "ch" */
    if (mem_addr[ch] - addr >= CHUNK_SIZE)
      mem_len[chunks] = CHUNK_SIZE;
    else {
      /* next chunk is close by, we need to just adjust the size of
	 the next chunk by CHUNK_SIZE *backward*
      */
      if (ch > 0) {
	if (last_create == ch) {
	  adj_merge_size *= 2;
	  if (adj_merge_size > ADJ_LIMIT)
	    adj_merge_size = ADJ_LIMIT;
	}
	last_create = ch;
	
	gap = mem_addr[ch] - mem_len[ch-1] - mem_addr[ch-1];
	gap = MIN (gap, adj_merge_size);
	REALLOC (mem[ch], LL, mem_len[ch] + gap);
	for (i=mem_len[ch]-1; i >= 0; i--) {
	  mem[ch][i+gap] = mem[ch][i];
	}
	for (i=0; i < gap; i++) {
	  mem[ch][i] = MEM_BAD;
	}
	mem_addr[ch] = mem_addr[ch] - gap;
	mem_len[ch] = mem_len[ch] + gap;
	return 0;
      }
      mem_len[chunks] = mem_addr[ch] - addr;
    }
    Assert (mem_len[chunks] > 0, "What?");
  }
  else {
    mem_len[chunks] = CHUNK_SIZE;
  }
  MALLOC (mem[chunks], LL, mem_len[chunks]);
  mem_addr[chunks] = 0;
  for (i=0;i<=mem_len[chunks]-1;i++) {
     mem[chunks][i] = MEM_BAD;
  }
  chunks++;
  return 1;
}

inline int Mem::_binsearch (LL addr)
{
  int i, j, m;
  
  if (chunks == 0) return -1;
  
  i = 0; j = chunks;

  while ((i+1) != j) {
    m = (i+j)/2;
    if (mem_addr[m] > addr)
      j = m;
    else
      i = m;
  }
  if (mem_addr[i] <= addr && addr < mem_addr[i] + mem_len[i])
    return i;
  else {
    return -1;
  }
}



inline int Mem::_binsearchw (LL addr)
{
  int i, j, m;
  
  if (chunks == 0) return 0;
  
  i = 0; j = chunks;

  while ((i+1) != j) {
    m = (i+j)/2;
    if (addr < mem_addr[m])
      j = m;
    else
      i = m;
  }
  if (mem_addr[i] <= addr && addr <= mem_addr[i] + mem_len[i]) {
    if (i > 0 && mem_addr[i-1] + mem_len[i-1] == addr)
      return i-1;
    return i;
  }
  else {
    /* not found */
    if (addr < mem_addr[0])
      return 0;
    else if (addr > mem_addr[chunks-1] + mem_len[chunks-1])
      return chunks;
    else
      return i+1;
  }
}

/*
 * get value @ address addr
 */
LL 
Mem::Read (LL addr)
{
  int i, w;

  addr >>= MEM_ALIGN;
  w = _binsearch (addr);
  if (w == -1) {
    return MEM_BAD;
  }
  else {
    return mem[w][addr-mem_addr[w]];
  }
}

/* merge two chunks */
void Mem::merge_chunks (int c1, int c2)
{
  unsigned long int i;

  Assert (mem_addr[c1]+mem_len[c1] == mem_addr[c2], "What?");
  Assert (c1 < c2, "Hmm...");

  get_more (c1, mem_len[c2], 1);
  for (i=0; i < mem_len[c2]; i++) {
    mem[c1][i+mem_addr[c2]-mem_addr[c1]] = mem[c2][i];
  }
  FREE (mem[c2]);
  for (i=c2; i < chunks-1; i++) {
    mem[i] = mem[i+1];
    mem_len[i] = mem_len[i+1];
    mem_addr[i] = mem_addr[i+1];
  }
  mem_len[i] = 0;
  chunks--;

  return;
}

/*
 *  garbage collect chunks periodically
 */
void Mem::chunk_gc (int force)
{
  int i;

  if (force || (chunks - last_chunk_collect > CHUNK_THRESHOLD)) {
    i = 0;
    while (i < chunks-1) {
      if (mem_addr[i]+mem_len[i] == mem_addr[i+1])
	merge_chunks (i, i+1);
      else
	i++;
    }
    last_chunk_collect = chunks;
  }
}


/*
 * store value at addr
 */
void 
Mem::Write (LL addr, LL val)
{
  int i, j, w;
  int ch, ret;
  LL len, sz;
  LL *data;

  addr >>= MEM_ALIGN;

  context_disable ();

  w = _binsearchw (addr);
  if (w < chunks &&
      (mem_addr[w] <= addr && addr < mem_addr[w]+mem_len[w])) {
    mem[w][addr-mem_addr[w]] = val;
    context_enable ();
    return;
  }
  i = w;

  if (i < chunks && chunks > 0 && (addr == mem_addr[i] + mem_len[i])) {
    /* check merging chunks! */
    if (i < chunks-1 && mem_addr[i+1] == addr) {
      /* merge two chunks! */
      merge_chunks (i,i+1);
      mem[i][addr-mem_addr[i]] = val;
      context_enable();
      return;
    }
    /* extend chunk! */
    get_more (i, addr-mem_addr[i]-mem_len[i]+1);
    mem[i][addr-mem_addr[i]] = val;
    context_enable();
    return;
  }
  /* we need to move other chunks to the end */
  ch = i;
  ret = create_new_chunk (addr, ch);
    
  if (ret == 1) {
    /* save data and len */
    data = mem[chunks-1];
    len = mem_len[chunks-1];

    /* move other chunks up */
    for (j = chunks-1; j >= ch+1; j--) {
      mem_addr[j] = mem_addr[j-1];
      mem_len[j] = mem_len[j-1];
      mem[j] = mem[j-1];
    }

    /* set chunk info */
    mem[ch] = data;
    mem_len[ch] = len;
    mem_addr[ch] = addr;

    get_more (ch, addr-mem_addr[ch]-mem_len[ch]+1);
    mem[ch][0] = val;
  }
  else if (ret == 0) {
    mem[ch][addr - mem_addr[ch]] = val;
  }
  else {
    fatal_error ("Unknown ret value");
  }

  chunk_gc (0);
  context_enable(); 
  return;
}

/*
 * read memory image from a file
 */
int 
Mem::ReadImage (FILE *fp)
{
  freemem ();
  return MergeImage (fp);
}

/*
 * read memory image from a file
 */
int 
Mem::ReadImage (const char *s)
{
  FILE *fp;
  int i;

  if (!s) return 0;
  if (!(fp = fopen (s, "r")))
    return 0;
  freemem ();
  i = MergeImage (fp);
  fclose (fp);

  return i;
}

/*
 * read memory image from a file
 */
int 
Mem::MergeImage (const char *s)
{
  FILE *fp;
  int i;

  if (!s) return 0;
  if (!(fp = fopen (s, "r")))
    return 0;
  i = MergeImage (fp);
  fclose (fp);

  return i;
}


/*
 * read memory image from a file
 */
int 
Mem::MergeImage (FILE *fp)
{
  char buf[1024];
  unsigned long a0,a1,v0,v1;
  unsigned long len;
  unsigned long a, v, tmp;
  int endian;

  while (fgets (buf, 1024, fp)) {
    if (buf[0] == '!') {
      break;
    }
    if (buf[0] == '\n') continue;
    if (buf[0] == '#') continue;
    if (buf[0] == '@') {
      /*
	Format: @address data length:

	 "length" data items starting at "address" all contain
	 "data"
      */
#if MEM_ALIGN == 3
      sscanf (buf+1, "0x%8lx%8lx 0x%8lx%8lx %lu", &a0, &a1, &v0, &v1, &len);
      a = ((LL)a0 << 32) | (LL)a1;
      v = ((LL)v0 << 32) | (LL)v1;
#elif MEM_ALIGN == 2
      sscanf (buf+1, "0x%8lx 0x%8lx %lu", &a, &v, &len);
#else
#error Unknown memory format
#endif
      while (len > 0) {
	Write (a, v);
	a += (1 << MEM_ALIGN);
	len--;
      }
      continue;
    }
    else if (buf[0] == '*') {
      /*
	Format: *address length
	        <list of data>

	 "length" locations starting at "address" contain the data
	 specified in the following "length" lines.
      */
#if MEM_ALIGN == 3
      sscanf (buf+1, "0x%8lx%8lx %lu", &a0, &a1, &len);
      a = ((LL)a0 << 32) | (LL)a1;
#elif MEM_ALIGN == 2
      sscanf (buf+1, "0x%8lx %lu", &a, &len);
#else
#error Unknown memory format
#endif
      while (len > 0 && fgets (buf, 1024, fp)) {
#if MEM_ALIGN == 3
	sscanf (buf, "0x%8lx%8lx", &v0, &v1);
	v = ((LL)v0 << 32) | (LL)v1;
#elif MEM_ALIGN == 2
	sscanf (buf, "0x%8lx", &v);
#else
#error Unknown memory format
#endif
	Write (a, v);
	a += (1 << MEM_ALIGN);
	len--;
      }
      if (len != 0) {
	fatal_error ("Memory format error, *0xaddr len format ends prematurely");
      }
      continue;
    }
    else if (buf[0] == '+' || buf[0] == '-') {
      /* 
	 Handle non-aligned data

	 + or - followed by
	 Format: address length
	         <list of data>

         Starting at address, the following length *bytes* are
	 specified below.

	 Assumptions:
	      length < (1 << MEM_ALIGN)	 
	      +  means big-endian
	      -  means little-endian
      */
      if (buf[0] == '+')
	endian = 1; /* big-endian */
      else
	endian = 0; /* little-endian */
#if MEM_ALIGN == 3
      sscanf (buf+1, "0x%8lx%8lx %lu", &a0, &a1, &len);
      a = ((LL)a0 << 32) | (LL)a1;
#elif MEM_ALIGN == 2
      sscanf (buf+1, "0x%8lx %lu", &a, &len);
#else
#error Unknown memory format
#endif
      while (len > 0 && fgets (buf, 1024, fp)) {
#if MEM_ALIGN == 3
	sscanf (buf, "0x%2lx", &v0);
	v = v0;
#elif MEM_ALIGN == 2
	sscanf (buf, "0x%2lx", &v);
#else
#error Unknown memory format
#endif
	tmp = Read (a);
	/* write the byte! */
	if (endian) {
	  tmp = (tmp & ~(0xffULL << ((~a & 3)<<3))) | (v << ((~a&3)<<3));
	}
	else {
	  tmp = (tmp & ~(0xffULL << ((a & 3)<<3))) | (v << ((a&3)<<3));
	}
	Write (a, tmp);
	a += 1;
	len--;
      }
      if (len != 0) {
	fatal_error ("Memory format error, *0xaddr len format ends prematurely");
      }
      continue;
    }
#if MEM_ALIGN == 3
    sscanf (buf, "0x%8lx%8lx 0x%8lx%8lx", &a0, &a1, &v0, &v1);
    a = ((LL)a0 << 32) | (LL)a1;
    v = ((LL)v0 << 32) | (LL)v1;
#elif MEM_ALIGN == 2
    sscanf (buf, "0x%8lx 0x%8lx", &a, &v);
#else
#error Unknown memory format
#endif
    Write (a, v);
  }

  a = 0;
  for (a1=0; a1 < chunks; a1++)
    a += mem_len[a1];

  return a;
}

/* Dump memory image */
void 
Mem::DumpImage (FILE *fp)
{
  int i, j, k;
  LL addr, data;
  unsigned long len;

  chunk_gc (1);

  for (i=0; i < chunks; i++) {
    fprintf (fp, "# Chunk %d of %d\n", i, chunks);
    addr = mem_addr[i] << MEM_ALIGN;

    for (j=0; j < mem_len[i]; j++)
      if (mem[i][j] != mem[i][0])
	break;
    if (j == mem_len[i]) {
      data = mem[i][0];
      len = mem_len[i];
#if MEM_ALIGN == 3
      fprintf (fp, "@0x%08lx%08lx 0x%08lx%08lx %lu\n",
	       (unsigned long)(addr >> 32),
	       (unsigned long)(addr & 0xffffffff),
	       (unsigned long)(data >> 32),
	       (unsigned long)(data & 0xffffffff), len);
#elif MEM_ALIGN == 2
      fprintf (fp, "@0x%08lx 0x%08lx %lu\n", (unsigned long)addr,
	       (unsigned long) data, len);
#else
#error What?
#endif
    }
    else {
#if MEM_ALIGN == 3
	fprintf (fp, "*0x%08lx%08lx %lu\n",
		 (unsigned long)(addr >> 32),
		 (unsigned long)(addr & 0xffffffff),
		 (unsigned long) mem_len[i]);
#elif MEM_ALIGN == 2
	fprintf (fp, "*0x%08lx 0x%08lx\n", (unsigned long)addr,
		 (unsigned long) mem_len[i]);
#else
#error Unknown mem format
#endif
	for (j=0; j < mem_len[i]; j++) {
	  data = mem[i][j];
#if MEM_ALIGN == 3
	fprintf (fp, "0x%08lx%08lx\n",
		 (unsigned long)(data >> 32),
		 (unsigned long)(data & 0xffffffff));
#elif MEM_ALIGN == 2
	fprintf (fp, "0x%08lx\n", (unsigned long) data);
#else
#error Unknown mem format
#endif
	}
    }
  }
}
