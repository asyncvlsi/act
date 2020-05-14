/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2015, 2017, 2018-2019 Rajit Manohar
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
#ifndef __NETLIST_H__
#define __NETLIST_H__

#include <stdio.h>
#include <act/act.h>
#include <act/passes/booleanize.h>
#include "bool.h"
#include "list.h"
#include "bitset.h"
#include "array.h"

#define EDGE_NFET 0
#define EDGE_PFET 1

struct edge;
struct node;
typedef struct edge edge_t;

/* actual edge width is:

   EDGE_WIDTH(edge, i, fold, minw)
     edge = edge pointer
     i = 0..nfolds-1 for the # of folds of this edge
     fold = folding threshold for this type of edge
     minw = min width of a transistor
*/
#define _STD_WIDTH_PER_FOLD(x)  ((x)->w/(x)->nfolds)
#define _RESIDUAL_WIDTH(x)   ((x)->w % (x)->nfolds)
#define EDGE_WIDTH(x,i)  (_STD_WIDTH_PER_FOLD(x) + (((i) == (x)->nfolds-1) ? _RESIDUAL_WIDTH(x) : 0))

struct act_nl_varinfo {
  act_booleanized_var_t *v;	/* var pointer */
  
  act_prs_expr_t *e_up, *e_dn;	/* parsed expression */

  bool_t *b;			/* the bdd for the variable */

  bool_t *up, *dn;		/* pull-up, pull-down */
  
  struct node *n;

  struct node *vdd, *gnd;	/* power supply for the gate: used to
				   check if two gates are compatible */

  unsigned int unstaticized:1;	/* unstaticized! */
  unsigned int stateholding:1;	/* state-holding variable */
  unsigned int usecf:1;		/* combinational feedback */

  struct node *inv;		/* var is an input to an inverter
				   whose output is inv */

};

/* transistor-level netlist graph */
typedef struct node {
  int i;			/* node id# */
  struct act_nl_varinfo *v;	/* some have names (could be NULL) */

  list_t *e;			/* edges */

  bool_t *b;			/* bool expr for this node */

  /* flags */
  unsigned int contact:1;	/* 1 if it needs a contact */
  unsigned int supply:1;	/* is a power supply */
  unsigned int inv:1;		/* 1 if it is a generated inverter
				   for staticizers */
  unsigned int visited:1;	/* visited flag for nodes */


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

  /* nfolds: # of folds */
  int nfolds;

  /* # of repeated lengths */
  int nlen;

  unsigned int type:1;		/* 0 = nfet, 1 = pfet */
  
  unsigned int pchg:1;		/* internal precharge expression */
  unsigned int keeper:1;	/* weak keeper */
  unsigned int combf:1;		/* combinational feedback */

  unsigned int raw:1;		/* explicitly specified fet */

  unsigned int pruned:1;	/* 1 = pruned during sharing */
  unsigned int tree:1;		/* is part of the current tree! */

  unsigned int visited;		/* visited this edge?; count if the
				   edge is folded */
};

typedef struct {
  act_boolean_netlist_t *bN;

  BOOL_T *B;
  node_t *hd, *tl;
  int idnum;			/* used to number the nodes */

  struct Hashtable *atH[2];	/* hash table for @-labels to node mapping */

  list_t *vdd_list, *gnd_list;	/* list of Vdd/GND node_t pointers */
  list_t *psc_list, *nsc_list;

  node_t *Vdd, *GND;
  node_t *psc, *nsc;		/* substrate contacts */

  int weak_supply_vdd, weak_supply_gnd;
  /* if > 0, this block has weak supply ports. It includes the count of the
     # of gates that share the weak inv within the block */
  int vdd_len, gnd_len;		/* sizing info for the weak supply
				   exported */
  int nid_wvdd, nid_wgnd;	/* node ids! */

  A_DECL (int, instport_weak);	/* node # for instance ports for weak
				   supplies */

  struct {
    int w, l;			/* current size */
    int nf;			/* current fold */
    int flavor;			/* current flavor */
    int sw, sl;			/* staticizer sizes */
  } sz[2];    			/* sizes */

} netlist_t;


/* -- netlist pass -- */

class ActNetlistPass : public ActPass {
 public:
  ActNetlistPass (Act *a);
  ~ActNetlistPass ();

  int run (Process *p = NULL);

  netlist_t *getNL (Process *p);

  void enableSharedStat();

  void Print (FILE *fp, Process *p);

  static node_t *connection_to_node (netlist_t *n, act_connection *c);
  static node_t *string_to_node (netlist_t *, char *s);
  static void sprint_node (char *buf, int sz, netlist_t *N, node_t *n);
  static void emit_node (netlist_t *N, FILE *fp, node_t *n, int mangle = 0);

 private:
  int init ();
  
  std::map<Process *, netlist_t *> *netmap;
  ActBooleanizePass *bools;

  /* lambda value */
  double lambda;
  
  /* minimum transistor size */
  int min_w_in_lambda;
  int min_l_in_lambda;

  /* maximum transistor size */
  int max_n_w_in_lambda;
  int max_p_w_in_lambda;

  /* strength ratios */
  double p_n_ratio;
  double weak_to_strong_ratio;

  /* load cap */
  double default_load_cap;

  /* netlist generation mode */
  int black_box_mode;
  
  /* folding parameters */
  int n_fold;
  int p_fold;
  int discrete_len;
  
  /* local and global Vdd/GND */
  static const char *local_vdd, *local_gnd, *global_vdd, *global_gnd;
  static Act *current_act;

  /* printing flags */
  int ignore_loadcap;
  int emit_parasitics;
  
  int fet_spacing_diffonly;
  int fet_spacing_diffcontact;
  int fet_diff_overhang;
  
  int use_subckt_models;
  int swap_source_drain;
  const char *extra_fet_string;
  
  int top_level_only;

  int weak_share_min, weak_share_max;

  netlist_t *generate_netlist (Process *p, int is_toplevel = 0);
  void generate_netgraph (netlist_t *N, int is_toplevel,
			  int num_vdd_share,
			  int num_gnd_share,
			  int vdd_len,
			  int gnd_len,
			  node_t *weak_vdd, node_t *weak_gnd);

  void generate_prs_graph (netlist_t *N, act_prs_lang_t *p, int istree = 0);
  void generate_staticizers (netlist_t *N, int is_toplevel,
			     int num_vdd_share,
			     int num_gnd_share,
			     int vdd_len, int gnd_len,
			     node_t *weak_vdd, node_t *weak_gnd);

  void fold_transistors (netlist_t *N);
  void set_fet_params (netlist_t *n, edge_t *f, unsigned int type,
		       act_size_spec_t *sz);
  void create_expr_edges (netlist_t *N, int type, node_t *left,
			  act_prs_expr_t *e, node_t *right, int sense);
  
  void emit_netlist (Process *p, FILE *fp);
};


#endif /* __NETLIST_H__ */
