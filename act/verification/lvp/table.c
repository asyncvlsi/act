/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#include <stdio.h>
#include "act/common/misc.h"
#include "table.h"
#include "lvs.h"

/*
 *------------------------------------------------------------------------
 *
 * create an empty table
 *
 *------------------------------------------------------------------------
 */
Table *table_create (void)
{
  Table *t;

  MALLOC (t, Table, 1);
  t->max = 0;
  t->n = 0;
  return t;
}


/*
 *------------------------------------------------------------------------
 *
 *  Add entry to table
 *
 *------------------------------------------------------------------------
 */
void table_add (Table *t, double idx, double val)
{
  int i;
  int pos;

  if (t->n == t->max) {
    if (t->max == 0) {
      t->max = 16;
      MALLOC (t->idx, double, t->max);
      MALLOC (t->val, double, t->max);
    }
    else {
      t->max *= 2;
      REALLOC (t->idx, double, t->max);
      REALLOC (t->val, double, t->max);
    }
  }
  for (i=0; i < t->n; i++)
    if (t->idx[i] >= idx)
      break;
  pos = i;
  if (i < t->n && t->idx[i] == idx) { /* replacement entry... hmm */
    t->val[i] = val;
    return;
  }
  for (i=t->n-1; i >= pos; i--) {
    t->val[i+1] = t->val[i];
    t->idx[i+1] = t->idx[i];
  }
  t->idx[pos] = idx;
  t->val[pos] = val;
  t->n++;
}


/*
 *------------------------------------------------------------------------
 *
 *  Lookup value
 *
 *------------------------------------------------------------------------
 */
double table_lookup (Table *t, double idx)
{
  int i;
  double ret;
  for (i=0; i < t->n; i++)
    if (t->idx[i] >= idx) break;

  /* flat below lower and above upper idx range */
  if (i == 0)
    ret = t->val[i];
  else if (i == t->n)
    ret = t->val[i-1];
  else
    ret = t->val[i-1] + 
      (t->val[i]-t->val[i-1])*(idx-t->idx[i-1])/(t->idx[i]-t->idx[i-1]);
  
  return ret;
}
