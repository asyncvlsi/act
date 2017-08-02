/*-*-mode:c++-*-**********************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *
 *************************************************************************/
#ifndef __MEM_H__
#define __MEM_H__

#include <stdio.h>
#include <stdlib.h>
#include "machine.h"

/*
 * Physical memory: Assumes big-endian format!
 */

typedef unsigned long long LL;
typedef unsigned char Byte;
typedef unsigned long Word;
  
#define MEM_BAD  0x0ULL
#define MEM_ALIGN 3 /* bottom three bits zero */

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

class Mem {
public:
  Mem();
  ~Mem();

  int ReadImage (char *s);
  int ReadImage (FILE *fp);	// load fresh memory image
  int MergeImage (char *s);
  int MergeImage (FILE *fp);	// merge image from file with current mem
  void DumpImage (FILE *fp);	// write current mem to file
  
  LL Read (LL addr);
  void Write (LL addr, LL val);
  
  Word BEReadWord (LL addr) {
    return BEGetWord (addr, Read (addr));
  }
    
  void BEWriteWord (LL addr, Word val) {
    Write (addr, BESetWord (addr, Read (addr), val));
  }

  Word LEReadWord (LL addr) {
    return LEGetWord (addr, Read (addr));
  }
    
  void LEWriteWord (LL addr, Word val) {
    Write (addr, LESetWord (addr, Read (addr), val));
  }

  inline Byte BEGetByte (LL addr, LL data) {
    return (data >> ((~addr & 7)<<3)) & 0xff;
  };
  inline Byte LEGetByte (LL addr, LL data) {
    return (data >> ((addr & 7)<<3)) & 0xff;
  };

  inline Word BEGetHalfWord (LL addr, LL data) {
    return (data >> ((~addr & 6)<<3)) & 0xffff;
  };
  inline Word LEGetHalfWord (LL addr, LL data) {
    return (data >> ((addr & 6)<<3)) & 0xffff;
  };

  inline Word BEGetWord (LL addr, LL data) {
    return (data >> ((~addr & 4)<<3)) & 0xffffffff;
  };
  inline Word LEGetWord (LL addr, LL data) {
    return (data >> ((addr & 4)<<3)) & 0xffffffff;
  };

  inline LL BESetByte (LL addr, LL data, Byte val) {
    return ((LL)val<<((~addr & 7)<<3))|(data&~((LL)0xff<<((~addr & 7)<<3)));
  };
  inline LL LESetByte (LL addr, LL data, Byte val) {
    return ((LL)val<<((addr & 7)<<3))|(data&~((LL)0xff<<((addr & 7)<<3)));
  };


  inline LL BESetHalfWord (LL addr, LL data, Word val) {
    return ((LL)val<<((~addr & 6)<<3))|(data&~((LL)0xffff<<((~addr & 6)<<3)));
  };
  inline LL LESetHalfWord (LL addr, LL data, Word val) {
    return ((LL)val<<((addr & 6)<<3))|(data&~((LL)0xffff<<((addr & 6)<<3)));
  };

  inline LL BESetWord (LL addr, LL data, Word val) {
    return ((LL)val<<((~addr & 4)<<3))|(data&~((LL)0xffffffff<<((~addr & 4)<<3)));
  };
  inline LL LESetWord (LL addr, LL data, Word val) {
    return ((LL)val<<((addr & 4)<<3))|(data&~((LL)0xffffffff<<((addr & 4)<<3)));
  };

  void Clear (void) { freemem (); }

  int Compare (Mem *m, int verbose = 1) {
    int i, j;
    int count = 10;

    chunk_gc (1);
    m->chunk_gc (1);

    if (m->chunks != chunks) {
      goto err;
    }
    for (i=0; i < chunks; i++) {
      if (mem_addr[i] != m->mem_addr[i]) goto err;
      if (mem_len[i] == m->mem_len[i]) continue;
      if (mem_len[i] < m->mem_len[i]) {
	for (j=mem_len[i]; j < m->mem_len[i]; j++)
	  if (m->mem[i][j] != MEM_BAD)
	    goto err;
      }
      else {
	for (j=m->mem_len[i]; j < mem_len[i]; j++)
	  if (mem[i][j] != MEM_BAD)
	    goto err;
      }
    }
    for (i=0; i < chunks; i++) {
      for (j=0; j < MIN(mem_len[i],m->mem_len[i]); j++)
	if (mem[i][j] != m->mem[i][j]) {
	  count--;
	  if (count == 0) goto err;
	  if (verbose) {
	    printf ("[0x%08x%08x]  0x%08x%08x  != 0x%08x%08x\n",
		    (unsigned long)(((j+mem_addr[i])<<MEM_ALIGN) >> 32),
		    (unsigned long)(((j+mem_addr[i])<<MEM_ALIGN) & 0xffffffff),
		    (unsigned long) (mem[i][j] >> 32),
		    (unsigned long) (mem[i][j] & 0xffffffff),
		    (unsigned long) (m->mem[i][j] >> 32),
		    (unsigned long) (m->mem[i][j] & 0xffffffff));
	  }
	}
    }
    if (count != 10) goto err;

    return 1;

  err:
    if (verbose) {
      printf ("First mem:\n");
      for (i=0; i < chunks; i++) {
	printf ("\t*0x%08x%08x %lu\n",
		(unsigned long)((mem_addr[i]<<MEM_ALIGN) >> 32),
		(unsigned long)((mem_addr[i]<<MEM_ALIGN) & 0xffffffff),
		mem_len[i]);
      }
      printf ("Second mem:\n");
      for (i=0; i < m->chunks; i++) {
	printf ("\t*0x%08x%08x %lu\n",
		(unsigned long)((m->mem_addr[i]<<MEM_ALIGN) >> 32),
		(unsigned long)((m->mem_addr[i]<<MEM_ALIGN) & 0xffffffff),
		m->mem_len[i]);
      }
    }
    return 0;
  }

private:
  int chunks;
  int numchunks;
  int last_chunk_collect;

  // array of blocks of memory
  LL **mem;

  // length of each block
  LL *mem_len;

  // size of each block
  // LL *mem_sz;

  // start address for each block
  LL *mem_addr;

  void freemem (void);
  void get_more (int chunkid, long long num, int merging = 0);
  int  create_new_chunk (LL addr, int ch);
  void merge_chunks (int chunk1, int chunk2);
  void chunk_gc (int force);

  inline int _binsearch (LL addr);
  inline int _binsearchw (LL addr);
};
  

#endif /* __MEM_H__ */
