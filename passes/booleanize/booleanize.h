
/*************************************************************************
 *
 *  Copyright (c) 2019 Rajit Manohar
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
#ifndef __ACT_BOOLEANIZE_H__
#define __ACT_BOOLEANIZE_H__

#include <map>
#include <act/act.h>

struct netlist_bool_port {
  act_connection *c;		/* port bool */
  unsigned int omit:1;		/* skipped due to aliasing */
  unsigned int input:1;		/* 1 if input, otherwise output */
  unsigned int netid;		/* if set, points to net; -1 if not set */
};

/*
  CHP programs can have complex dynamic accesses, with runtime array
  references.

  1. This must be a bool or an int type
  2. Indexing this type has to get mapped to some index into a bunch
     of bits
*/
typedef struct act_dynamic_var {
  act_connection *id;
  ActId *aid;
  unsigned int isint:1;
  Array *a;			// array info: dimensions
  int width;
} act_dynamic_var_t;
  

typedef struct act_booleanized_var {
  act_connection *id;		/* unique connection id */
  unsigned int input:1;		/* is a primary input variable */
  unsigned int output:1;	/* set to 1 to force it to be an
				   output */
  unsigned int used:1;		/* used flags */
  unsigned int ischan:1;	/* for built-in channel variables that
				   have not been turned into bools! */
  unsigned int isint:1;		/* for built-in int variables that
				   have not been turned into bools! */
  unsigned int usedchp:1;	/* also used in CHP mode */
  unsigned int isglobal:1;	/* global flag */
  unsigned int isport:1;	/* 1 if this is in the port list, and
				   isn't omitted; won't be set for
				   black box processes */
  unsigned int ischpport:1;	/* 1 if this is in the chp port list,
				   and isn't omitted; won't be set
				   for black box processes */

  unsigned int isfragmented:1;	/* 1 if this is a user-defined
				   data/channel type that is
				   fragmented */
  
  unsigned int width;		/* bit-width for chan/int */
  void *extra;			/* space for rent */
} act_booleanized_var_t;

typedef struct {
  ActId *inst;
  act_connection *pin;
} act_local_pin_t;
  

typedef struct {
  act_connection *net; // this could be a global
  unsigned int skip:1; // skip this net
  unsigned int port:1; // is a port
  A_DECL (act_local_pin_t, pins);
} act_local_net_t;


typedef struct {
  Process *p;
  Scope *cur;

  unsigned int visited:1;	/* flags */
  unsigned int isempty:1;	/* check if this is empty! */

  struct iHashtable *cH;   /* connection hash table (map to var)  */
  struct iHashtable *cdH;  /* connection hash table for dynamic vars */

  A_DECL (struct netlist_bool_port, chpports);
  A_DECL (struct netlist_bool_port, ports);
  A_DECL (act_connection *, instports);
  A_DECL (act_connection *, instchpports);
  A_DECL (act_connection *, used_globals);

  A_DECL (act_local_net_t, nets); // nets

} act_boolean_netlist_t;


void act_booleanize_netlist (Act *, Process *);

class ActBooleanizePass : public ActPass {
 public:
  ActBooleanizePass(Act *a);
  ~ActBooleanizePass();

  /**
   * Run the pass
   *  @param p is the top-level process. Must be an expanded process.
   *         (NULL means the global namespace scope)
   *  @return 1 on success, 0 on failure
   */
  int run (Process *p = NULL);

  /**
   *  @param p is a pointer to a process; NULL means the top-level
   *  global namespace (the global scope, external to any process)
   *
   *  @return the booleanized netlist pointer for the spceified
   * process 
   **/
  act_boolean_netlist_t *getBNL (Process *p);


  /**
   * Create net data structure that contains a netname and instance
   *  names + pins
   *
   * @param p is the top level process
   */
  void createNets (Process *p = NULL);


  static act_dynamic_var_t *isDynamicRef (act_boolean_netlist_t *,
					  act_connection *);
  
  /*-- internal data structures and functions --*/
 private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  int black_box_mode;

  /*-- internal functions: generate booleans for a process --*/
  act_boolean_netlist_t *_create_local_bools (Process *p);

  void flatten_ports_to_bools (act_boolean_netlist_t *,
			       ActId *prefix,
			       Scope *s,
			       UserDef *u, int nochp);

  void update_used_flags (act_boolean_netlist_t *n,
			  ValueIdx *vx, Process *p);
  void rec_update_used_flags (act_boolean_netlist_t *n,
			      act_boolean_netlist_t *subinst,
			      ActId *prefix,
			      Scope *s, UserDef *u, int *count, int *count2);
  void append_base_port (act_boolean_netlist_t *n, act_connection *c, Type *t,
			 int mode);

  /*--- create netlist helper functions ---*/
  void _createNets (Process *p);
  int addNet (act_boolean_netlist_t *n, act_connection *c);
  void addPin (act_boolean_netlist_t *n, int netid, const char *name, Array *a, act_connection *pin);
  void importPins (act_boolean_netlist_t *n, int netid, const char *name, Array *a, act_local_net_t *net);

};

#endif /* __ACT_BOOLEANIZE_H__ */
