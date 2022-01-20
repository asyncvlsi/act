/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __TABLE_H__
#define __TABLE_H__

typedef struct {
  int n;
  int max;
  double *idx;
  double *val;
} Table;

Table *table_create (void);
void table_add (Table *, double idx, double val);
double table_lookup (Table *, double idx);

#endif /* __TABLE_H__ */
