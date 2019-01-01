/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __EXT_FILE_H__
#define __EXT_FILE_H__

#include <stdio.h>
#include "hash.h"

#define EXT_ATTR_PCHG 0x01 /* @pchg */
#define EXT_ATTR_NUP  0x02 /* @nup */
#define EXT_ATTR_NDN  0x04 /* @ndn */
#define EXT_ATTR_PUP  0x08 /* @pup */
#define EXT_ATTR_PDN  0x10 /* @pdn */
#define EXT_ATTR_VC   0x20 /* @voltage_converter */

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

#define EXT_FET_PTYPE 1
#define EXT_FET_NTYPE 0

struct ext_fets {
  char *g, *t1, *t2;
  double length, width;
  int type;
  int isweak;
  struct ext_fets *next;
};

struct ext_attr {
  char *n;
  unsigned int attr;
  struct ext_attr *next;
};

#define HIER_IS_INPUT 1
#define HIER_IS_SEEN  2

struct hier_cell_val {
  unsigned int flags;		/* flags used */
  hash_bucket_t *root;
};
  
struct ext_file {
  int mark;
  unsigned long timestamp;
  struct Hashtable *h;		/* if hierarchical */
  struct ext_fets *fet;		/* fets */
  struct ext_list *subcells;	/* subcells */
  struct ext_alias *aliases;	/* aliases */
  struct ext_cap *cap;		/* capacitance */
  struct ext_attr *attr;	/* ext attributes */
  struct ext_ap *ap;		/* area/perim */
};

/* parse hierarchical extract file */
extern struct ext_file *ext_read (const char *name);
extern void ext_validate_timestamp (const char *name);

#endif /* __PARSE_EXT_H__ */
