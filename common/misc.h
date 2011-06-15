/*************************************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *  All Rights Reserved
 *
 *************************************************************************/
#ifndef __MISC_H__
#define __MISC_H__

#include <stdlib.h> /* malloc.h is deprecated */
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MALLOC

#define MALLOC(var,type,size) do { if (size == 0) { fprintf (stderr, "FATAL: allocating zero-length block.\n\tFile %s, line %d\n", __FILE__, __LINE__); exit (2); } else { var = (type *) malloc (sizeof(type)*(size)); if (!var) { fprintf (stderr, "FATAL: malloc of size %ld failed!\n\tFile %s, line %d\n", sizeof(type)*(size), __FILE__, __LINE__); fflush(stderr); exit (1); } } } while (0)

#define REALLOC(var,type,size) do { if (size == 0) { fprintf (stderr, "FATAL: allocating zero-length block.\n\tFile %s, line %d\n", __FILE__, __LINE__); exit (2); } else { var = (type *) realloc (var, sizeof(type)*(size)); if (!var) { fprintf (stderr, "FATAL: realloc of size %ld failed!\n\tFile %s, line %d\n", sizeof(type)*(size), __FILE__, __LINE__); exit (1); } } } while (0)

#ifdef NEW
#undef NEW
#endif
#define NEW(var,type) MALLOC(var,type,1)
#define FREE(var)   free(var)

#endif /* MALLOC */

#define Max(a,b) ( ((a) > (b)) ? (a) : (b) )

void fatal_error (char *s, ...);
void warning (char *s, ...);
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

#ifdef __cplusplus
}
#endif


#endif /* __MISC_H__ */
