/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __HIER_H__
#define __HIER_H__

#include "var.h"

/*
 *  Data structures for hierarchical analysis
 */

#ifndef NO_POINTER_FLAGS

#include <assert.h>

#define KEY(cell) ((char *)(((unsigned long)(cell)->key) & ~3))
#define IS_INPUT(cell) ((unsigned long)((cell)->key) & 1)
#define IS_SEEN(cell) ((unsigned long)((cell)->key) & 2)
#define MK_INPUT(cell) ((cell)->key = (char*)(((unsigned long)(cell)->key & ~1)|1))
#define MK_SEEN(cell) ((cell)->key = (char*)(((unsigned long)(cell)->key & ~2)|2))
#define MK_UNSEEN(cell) ((cell)->key = (char*)((unsigned long)(cell)->key & ~2))
#define HIER_INIT_FLAGS(cell)  assert (IS_INPUT(cell) == 0)

#else

#define HIER_IS_INPUT 1
#define HIER_IS_SEEN  2

#define KEY(cell) ((cell)->key)
#define IS_INPUT(cell) ((cell)->flags & HIER_IS_INPUT)
#define IS_SEEN(cell) ((cell)->flags & HIER_IS_SEEN)
#define MK_INPUT(cell) ((cell)->flags |= HIER_IS_INPUT)
#define MK_SEEN(cell) ((cell)->flags |= HIER_IS_SEEN)
#define MK_UNSEEN(cell) ((cell)->flags &= ~HIER_IS_SEEN)
#define HIER_INIT_FLAGS(cell)  ((cell)->flags = 0)

#endif


struct hash_cell {
  char *key;
#ifdef NO_POINTER_FLAGS
  unsigned char flags;
#endif
  struct hash_cell *root;
  struct hash_cell *next;
};

struct hier_table {
  int size;
  struct hash_cell **head;
  int nelements;
};

struct hier_table *hier_create (int sz);
struct hash_cell *hier_add (struct hier_table *, const char *, int);
struct hash_cell *hier_find (struct hier_table *, const char *);
void hier_free (struct hier_table *, const char *);
void hier_freeall (struct hier_table *);

char *hier_subcell_node (VAR_T *, char *, var_t **, char sep);
int hier_notinput_subcell_node (VAR_T *, char *, char sep);


#endif /* __HIER_H__ */
