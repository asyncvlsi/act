/*************************************************************************
 *
 *  Copyright (c) 2020 Rajit Manohar
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
#ifndef __ACT_VNET_H__
#define __ACT_VNET_H__

#include <act/act.h>
#include "misc.h"
#include "array.h"
#include "hash.h"
#include "list.h"
#include "qops.h"

struct array_idx {
  int lo, hi;
};

/*
  The driver is either "id" or "id.port"
*/
struct driver_info {
  char *id;
  char *port;
  unsigned int isderef:1;
  int deref;

  int lo, hi;
};

struct idinfo {
  char *myname;
  //  hash_bucket_t *b;		/* name */

  unsigned int isinput:1;	/* input? */
  unsigned int isoutput:1;	/* output? */
  unsigned int isport:1;	/* is this a port? */

  unsigned int isclk:1;		/* is a clock? */

  unsigned int isinst:1;	/* inst type */
  unsigned int ismodname:1;	/* module name */

  char *nm;			/* module name for instance */
  int nused;			/* # of used bits */
  int *used;			/* used[i] = 1 if the ith port is
				   used, 0 otherwise */
  Process  *p;			/* act port name */
  struct moduletype *m;		/* module name */

  A_DECL (struct array_idx, a);	/* array information */

  /* the driver for this id: either one value, or an array */
  struct driver_info *d;

  /* fanout info */
  int fanout;
  int curnum;
  /* Element-by-element fanout information, in the case of an array;
     size is determined by array info
  */
  int *fa;
  int *fcur;

  /** this is used for missing modules to be filled in later **/
  struct idinfo *nxt;
  int conn_start, conn_end;
  struct moduletype  *mod;

  /** linear list **/
  struct idinfo *next;
};

typedef struct idinfo id_info_t;

typedef struct id_deref {
  id_info_t *id;
  unsigned int isderef:1;
  int deref;
} id_deref_t;

typedef struct conn_rhs {
  id_deref_t id;		/* identifier */
  unsigned int issubrange:1;	/* is this is subrange? */
  int lo, hi;			/* subrange params */
} conn_rhs_t;

typedef struct connectinfo {
  id_info_t *prefix;		/* prefix . id is the LHS */
  id_deref_t id;
  list_t *l;			/* a list of rhs connections */
  conn_rhs_t *r;		/* or a conn rhs */

  int isclk;			/* 1 if this is a clock connection */


  int dir;			/* direction: 0, 1, 2
				   0, 2 := lhs <= rhs
				   1 := rhs <= lhs
				*/

} conn_info_t;


#define M_FLAG_CLKDONE  0x1

struct connect_lhs {
  id_info_t *prefix;
  id_deref_t id;
};

typedef struct moduletype {
  hash_bucket_t *b;		/* module type name */

  /* ports */
  A_DECL (struct idinfo *, port_list);

  /* connections */
  A_DECL (struct connectinfo *, conn);

  /* instances */
  struct Hashtable *H;
  id_info_t *hd, *tl;
  

  /* has this been instantiated? */
  int inst_exists;

  /* flags */
  unsigned int flags;

  /* dangling ports (have to be bucketed for async) */
  A_DECL (struct connect_lhs, dangling_list);
  
  /* next pointer for global module list:
     modules are emitted in global order
  */
  struct moduletype *next;

} module_t;
  

typedef struct {
  FILE *out;			/* output file */
  Act *a;			/* library */

  struct Hashtable *M;		/* module hash table */

  /* module list */
  module_t *hd, *tl;

  /* missing module list: temporary */
  struct Hashtable *missing;


  /* temporary used during reading the netlist */
  id_info_t *prefix;
  int flag;			/* temp flag */
  id_info_t *tmpid;		/* temp */
  
} VNet;

#define CURMOD(vw)  ((vw)->tl)


enum VRet_type {
  V_INT,
  V_ID
};

typedef struct {
  int type;
  union {
    int i;
    id_info_t *id;
  } u;
} VRet;

VNet *verilog_read (const char *file, const char *lib);
void verilog_graph (VNet *n);


#endif /* __ACT_VNET_H__ */
