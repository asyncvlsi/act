/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __HIER_H__
#define __HIER_H__

#include "act/common/ext.h"
#include "var.h"

/*
 *  Data structures for hierarchical analysis
 */
#include <assert.h>

#define HIER_IS_INPUT 1
#define HIER_IS_SEEN  2

#define HVAL(cell) ((struct hier_cell_val *)((cell)->v))

#define ROOT(cell)  (HVAL(cell)->root ? HVAL(cell)->root : cell)

#define KEY(cell) ((cell)->key)
#define IS_INPUT(cell) (HVAL(cell)->flags & HIER_IS_INPUT)
#define IS_SEEN(cell) (HVAL(cell)->flags & HIER_IS_SEEN)
#define MK_INPUT(cell) (HVAL(cell)->flags |= HIER_IS_INPUT)
#define MK_SEEN(cell) (HVAL(cell)->flags |= HIER_IS_SEEN)
#define MK_UNSEEN(cell) (HVAL(cell)->flags &= ~HIER_IS_SEEN)
#define HIER_INIT_FLAGS(cell)  (HVAL(cell)->flags = 0)

char *hier_subcell_node (VAR_T *, char *, var_t **, char sep);
int hier_notinput_subcell_node (VAR_T *, char *, char sep);

#endif /* __HIER_H__ */
