/*************************************************************************
 *
 *  Miscellaneous functions
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
#ifndef __MISC_H__
#define __MISC_H__

#include <stdlib.h> /* malloc.h is deprecated */
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MALLOC

#ifdef MEM_DEBUG
#define MEM_LOG(s) mem_log s

void mem_log (const char *s, ...);

#else
#define MEM_LOG(s)  
#endif

#define MALLOC(var,type,size)						\
  do {									\
  if (size == 0) {							\
    fprintf (stderr, "FATAL: allocating zero-length block.\n\tFile %s, line %d\n", __FILE__, __LINE__); \
    exit (2);								\
  } else {								\
    var = (type *) malloc (sizeof(type)*(size));			\
    if (!var) {								\
      fprintf (stderr, "FATAL: malloc of size %lu failed!\n\tFile %s, line %d\n", sizeof(type)*(size), __FILE__, __LINE__); \
      fflush(stderr);							\
      exit (1);								\
    } else {								\
      MEM_LOG (("alloc'ed %x (%lu bytes) @ %s:%d", var, sizeof (type)*(size), __FILE__, __LINE__)); \
    }									\
  }									\
 } while (0)

#define REALLOC(var,type,size)						\
  do {									\
    if (size == 0) {							\
      fprintf (stderr, "FATAL: allocating zero-length block.\n\tFile %s, line %d\n", __FILE__, __LINE__); \
      exit (2);								\
    } else {								\
      MEM_LOG (("dealloc'ed %x @ %s:%d", var, __FILE__, __LINE__)); \
      var = (type *) realloc (var, sizeof(type)*(size));		\
      if (!var) {							\
	fprintf (stderr, "FATAL: realloc of size %lu failed!\n\tFile %s, line %d\n", sizeof(type)*(size), __FILE__, __LINE__); \
	exit (1);							\
      } else {								\
	MEM_LOG (("alloc'ed %x (%lu bytes) @ %s:%d", var, sizeof (type)*(size), __FILE__, __LINE__)); \
      }									\
    }									\
  } while (0)

#ifdef NEW
#undef NEW
#endif
#define NEW(var,type) MALLOC(var,type,1)
#define FREE(var)   do { MEM_LOG (("dealloc'ed %x @ %s:%d", var, __FILE__, __LINE__)); free(var); } while (0)

#endif /* MALLOC */

#define Max(a,b) ( ((a) > (b)) ? (a) : (b) )

void fatal_error (const char *s, ...);
void warning (const char *s, ...);
char *Strdup (const char *);

#define Assert(a,b) do { if (!(a)) { fprintf (stderr, "Assertion failed, file %s, line %d\n", __FILE__, __LINE__); fprintf (stderr, "Assertion: " #a "\n"); fprintf (stderr, "ERR: " b "\n"); exit (4); } } while (0)

#define MEMCHK(x)  Assert (x, "out of memory")

#define REFCNT_TAG   unsigned long __ref_cnt
#define REFCNT_INIT(x)  ((x)->__ref_cnt = 1)
#define REFCNT_DUP(x)   ((x)->__ref_cnt++)
#define REFCNT_FREE(x)  			\
  do {						\
     (x)->__ref_cnt--;				\
     if ((x)->__ref_cnt == 0) {			\
       FREE (x);				\
     }						\
  } while (0)



void mymergesort (const void **a, int sz,
		  int (*cmpfn)(const void *, const void *));

/* p is an array of size sz. 
   aux = allocated array of size sz+1, with aux[0] = -1
   Call to return a permutation of p. Call repeatedly until you get 0
*/
int mypermutation (int *p, int *aux, int sz);
  

#ifdef __cplusplus
}
#endif


#endif /* __MISC_H__ */
