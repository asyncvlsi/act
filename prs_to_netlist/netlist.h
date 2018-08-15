/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 *  Some parts of this code were written when the author was at Cornell
 *
 **************************************************************************
 */
#ifndef __NETLIST_H__
#define __NETLIST_H__

#include <stdio.h>
#include <act/act.h>
#include "bool.h"
#include "list.h"
#include "bitset.h"
#include "array.h"

#define EDGE_NFET 0
#define EDGE_PFET 1

struct edge;
struct node;
typedef struct edge edge_t;

typedef struct var {
  act_connection *id;		/* unique connection id */
  act_prs_expr_t *e_up, *e_dn;	/* parsed expression */

  bool_t *b;			/* the bdd for the variable */

  bool_t *up, *dn;		/* pull-up, pull-down */
  
  struct node *n;

  struct node *vdd, *gnd;	/* power supply for the gate: used to
				   check if two gates are compatible */

  unsigned int unstaticized:1;	/* unstaticized! */
  unsigned int stateholding:1;	/* state-holding variable */
  unsigned int usecf:1;		/* combinational feedback */
  unsigned int input:1;		/* is a primary input variable */
  unsigned int output:1;	/* set to 1 to force it to be an output */
  unsigned int used:1;		/* 1 if it is used */

  struct node *inv;		/* var is an input to an inverter
				   whose output is inv */

} var_t;


/* transistor-level netlist graph */
typedef struct node {
  int i;			/* node id# */
  var_t *v;			/* some have names (could be NULL) */

  list_t *e;			/* edges */

  bool_t *b;			/* bool expr for this node */

  /* flags */
  unsigned int contact:1;	/* 1 if it needs a contact */
  unsigned int supply:1;	/* is a power supply */
  unsigned int inv:1;		/* 1 if it is a generated inverter
				   for staticizers */


  /* arrays are EDGE_NFET/EDGE_PFET indexed */
  list_t *wl;		      /* max reff calculation: worklist */
  double reff[2];		/* actual reff value */

  unsigned char reff_set[2];	/* 1 if set by attr, 0 otherwise */

  double cap;			/* cap to GND on the node */
  double resis;			/* output resistance of the node */

  struct node *next;		/* global list of nodes for the
				   Netlist */
} node_t;

struct edge {
  node_t *g;			/* gate on the edge */
  node_t *bulk;			/* body */
  node_t *a, *b;		/* two nodes */

  int w, l;			/* w, l for the gate */
  int flavor;			/* lvt,svt,hvt,od18,... */
  int subflavor;		/* subflavor for the fet */

  unsigned int type:1;		/* 0 = nfet, 1 = pfet */
  
  unsigned int pchg:1;		/* internal precharge expression */
  unsigned int keeper:1;	/* weak keeper */
  unsigned int combf:1;		/* combinational feedback */

  unsigned int raw:1;		/* explicitly specified fet */

  unsigned int visited:1;	/* visited this edge? */
  unsigned int shared:1;	/* 1 = shared, omit it! */
  unsigned int tree:1;		/* is part of the current tree! */

};

struct netlist_bool_port {
  act_connection *c;		/* port bool */
  unsigned int omit:1;		/* skipped due to aliasing */
  unsigned int input:1;		/* 1 if input, otherwise output */
};
  

typedef struct {
  Process *p;
  BOOL_T *B;

  node_t *hd, *tl;
  int idnum;			/* used to number the nodes */

  unsigned int visited:1;	/* flags */

  struct iHashtable *cH;   /* connection hash table (map to var_t)  */

  struct Hashtable *atH[2];	/* hash table for @-labels to node mapping */

  list_t *vdd_list, *gnd_list;	/* list of Vdd/GND node_t pointers */
  list_t *psc_list, *nsc_list;

  node_t *Vdd, *GND;
  node_t *psc, *nsc;		/* substrate contacts */

  struct {
    int w, l;			/* current size */
    int sw, sl;			/* staticizer sizes */
  } sz[2];    			/* sizes */


  A_DECL (struct netlist_bool_port, ports);
  struct iHashtable *uH;      /* used act_connection *'s in subckts */

  A_DECL (act_connection *, instports);

} netlist_t;

void act_prs_to_netlist (Act *, Process *);
void act_create_bool_ports (Act *, Process *);
void act_emit_netlist (Act *, Process *, FILE *);
void emit_verilog_pins (Act *, FILE *, FILE *, Process *);

#endif /* __NETLIST_H__ */
