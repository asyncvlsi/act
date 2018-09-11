/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __DOTS_H__
#define __DOTS_H__

#define MAXDOTS 512

typedef struct {
  int ndots;		/* 0 = no dots, -1 = don't know, -2 = error */
  int globalnode;       /* 1 = trailing bang stripped, 0 = no, -1 = don't know */
  int *pos;
} dots_t;

void init_dots (dots_t *t);
void get_dots (char *s, dots_t *t);
void merge_dots (dots_t *t1, dots_t *t2);
void create_dots (dots_t *, int ndots, int *pos);

#endif /* __DOTS_H__ */
