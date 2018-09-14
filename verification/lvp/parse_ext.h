/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __PARSE_EXT_H__
#define __PARSE_EXT_H__

#include <stdio.h>
#include "var.h"
#include "hier.h"

struct ext_file;

struct ext_list {
  char *file;
  char *id;
  int xlo, xhi, ylo, yhi;
  struct ext_file *ext;
  struct ext_list *next;
};

/*
 * three types of capacitances:
 *   1. cap to ground (n1 = node, n2 = NULL)
 *   2. cap correction (n1, n2 nodes being corrected)
 *   3. inter-node capacitance (n1,n2 nodes)
 */
enum cap_type {
  CAP_GND,
  CAP_CORRECT,
  CAP_INTERNODE
};

struct ext_cap {
  int type;
  char *n1, *n2;
  double cap;
  struct ext_cap *next;
};

struct ext_ap {
  char *node;
  double p_perim, p_area;
  double n_perim, n_area;
  struct ext_ap *next;
};
  

struct ext_alias {
  char *n1, *n2;
  struct ext_alias *next;
};

struct ext_fets {
  char *g, *t1, *t2;
  int length, width;
  int type;
  int isweak;
  struct ext_fets *next;
};

struct ext_attr {
  char *n;
  unsigned int attr;
  struct ext_attr *next;
};

struct ext_file {
  int mark;
  unsigned long timestamp;
  struct hier_table *h;		/* if hierarchical */
  struct ext_fets *fet;		/* fets */
  struct ext_list *subcells;	/* subcells */
  struct ext_alias *aliases;	/* aliases */
  struct ext_cap *cap;		/* capacitance */
  struct ext_attr *attr;	/* ext attributes */
  struct ext_ap *ap;		/* area/perim */
};

extern struct ext_file *parse_ext_file (FILE *, FILE *dumpfile, char *name);
extern FILE *mag_path_open (char *, FILE **);
extern void flatten_ext_file (struct ext_file *, VAR_T *);
extern void check_ext_timestamp (FILE *fp);

#endif /* __PARSE_EXT_H__ */
