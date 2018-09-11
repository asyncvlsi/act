/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#include <stdio.h>
#include "dots.h"
#include "misc.h"

void init_dots (dots_t *t)
{
  t->ndots = -1;
  t->globalnode = -1;
}

void merge_dots (dots_t *tgt, dots_t *src)
{
  int i;

  if (tgt->ndots == -2) {
    if (src->ndots > 0) FREE (src->pos);
    src->ndots = -1;
    return;
  }
  if (tgt->ndots == -1) {
    tgt->ndots = src->ndots;
    tgt->pos = src->pos;
    src->pos = NULL;
    src->ndots = -1;
  }
  else {
    if (tgt->ndots != src->ndots) {
      if (tgt->ndots > 0) FREE (tgt->pos);
      if (src->ndots > 0) FREE (src->pos);
      src->ndots = -1;
      tgt->ndots = -2;
      return;
    }
    for (i=0; i < tgt->ndots; i++) 
      if (tgt->pos[i] != src->pos[i]) {
	FREE (tgt->pos);
	FREE (src->pos);
	tgt->ndots = -2;
	return;
      }
    if (src->ndots > 0) FREE (src->pos);
    src->ndots = -1;
  }
}


void create_dots (dots_t *d, int ndots, int *pos)
{
  d->ndots = ndots;
  if (ndots > 0) {
    int i;
    MALLOC (d->pos, int, ndots);
    for (i=0; i < ndots; i++)
      d->pos[i] = pos[i];
  }
}
