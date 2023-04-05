/*************************************************************************
 *
 *  Copyright (c) 2019-2021 Rajit Manohar
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
#include <act/extmacro.h>

/**
 * @class netlist_bool_port
 * 
 * @brief This holds information about a port in the Booleanized netlist data
 * structure. 
 */
struct netlist_bool_port {
  act_connection *c;		/**< port bool connection pointer */
  unsigned int omit:1;		/**< skipped due to aliasing */
  unsigned int input:1;		/**< 1 if input, otherwise output */
  unsigned int bidir:1;		/**< bidirectional? if this is set,
				   then it is an I/O signal;
				   otherwise the input flag specifies
				   input v/s output */
  unsigned int used:1;		/**< placeholder, not set by standard passes.
                                   used when checking what is actually used
                                   after user-specified languages are  selected
                                */
  unsigned int netid;		/**< if set, points to net; -1 if not set */
};

/**
 * @class netlist_global_port
 *
 * @brief This is used to hold information about a global signal used
 * in a process.
 */
struct netlist_global_port {
  act_connection *c;		/**< port bool connection pointer */
  unsigned int input:1;		/**< 1 if input, otherwise output */
  unsigned int bidir:1;		/**< bidirectional? if this is set,
				   then it is an I/O signal;
				   otherwise the input flag specifies
				   input v/s output */
};


/**
 * @class act_dynamic_var
 *
 * @brief CHP programs can have complex dynamic accesses, with runtime
 * array references. The Booleanize pass records these as "dynamic
 * variables" represented by this structure.
*/
typedef struct act_dynamic_var {
  act_connection *id;	   ///< The connection ID corresponding to the
			   ///array itself
  ActId *aid;		   ///< The array name as an ID
  unsigned int isint:1;	   ///< 1 if this is an int, 0 if it is a bool
  Data *isstruct;	   ///< non-NULL means it is a structure (the
			   ///width field is ignored in this case)
  Array *a;		   ///< array info: dimensions
  int width;		   ///< for integers, this is the bit-width
} act_dynamic_var_t;
  

/**
 * @class act_booleanized_var
 *
 * @brief The core data type for a variable accessed in an ACT
 * process. A   variable may be an act_booleanized_var or an
 * act_dynamic_var. The   latter is used for dynamic array references,
 * and the   act_booleanized_var structure is used for all other
 * variables. 
 *
 * Structures are expanded out as individual variables for normal
 * accesses. Only dynamic references are maintained as intact
 * structures.
 * 
 * A variable can be used in CHP, dataflow, HSE, and PRS bodies. The
 * true use case will be selected by the tool that picks which level
 * of  modeling is to be used. HSE/PRS are "Boolean" modeling levels,
 * whereas CHP/dataflow are "CHP" levels.
*/
typedef struct act_booleanized_var {
  act_connection *id;		/**< unique connection id
				   corresponding to the Booleanized variable */
  
  unsigned int input:1;		/**< set to 1 if this is an input variable */
  unsigned int output:1;	/**< set to 1 if this is an output variable */
  unsigned int localout:1;	/**< set to 1 if this is driven by a local circuit */
  unsigned int used:1;		/**< used flag for prs/hse */
  unsigned int ischan:1;	/**< for channel variables that
				   have not been turned into bools! */

  unsigned int chanflag:2;	/**< flag for channels:
				   0 = not determined by CHP or
				       channel definition
				   1 = passive receive, active send
				   2 = active receive, passive send
				*/
  
  unsigned int isint:1;		/**< for built-in int variables that
				   have not been turned into bools! */
  unsigned int usedchp:1;	/**< used flag for CHP mode */
  unsigned int isglobal:1;	/**< is this a global variable flag */
  unsigned int isport:1;	/**< 1 if this is in the port list, and
				   isn't omitted **/
  unsigned int ischpport:1;	/**< 1 if this is in the chp port list,
				   and isn't omitted; won't be set
				   for black box processes */

  unsigned int isfragmented:1;	/**< 1 if this is a user-defined
				   data/channel type that is
				   fragmented */

  unsigned int width;		/**< bit-width for chan/int */
  unsigned int w2;		/**< bit-width for bidirectional
				   channel ack */

  short proc_in, proc_out;	/**<
				  which of the top-level parallel blocks is
				  an in/out, when a channel is used
				  in both contexts within a chp block 
				*/
  
  void *extra;			/**< space for rent */
} act_booleanized_var_t;


/**
 * @class act_local_pin_t
 * 
 * @brief This is used for flat netlist generation to represent an I/O
 * pin for a leaf cell.
 */
typedef struct {
  ActId *inst;			/**< path to instance */
  act_connection *pin;		/**< pin name */
  Process *cell;		/**< leaf cell corresponding to the pin */
} act_local_pin_t;
  

/**
 * @class act_local_net_t
 *
 * @brief This is used for flat netlist generation to represent a net
 * that consists of a net name and a list of pins.
 */
typedef struct {
  act_connection *net; ///< the name of the net; note that this could
		       ///be a global
  unsigned int skip:1; ///< set to skip this net
  unsigned int port:1; ///< 1 if this is a port
  A_DECL (act_local_pin_t, pins); ///< array of pins for the net
} act_local_net_t;


/**
 *
 * @class act_boolean_netlist_t
 *
 * @brief This structure is computed for each process by the
 * Booleanize pass. It summarizes the information about all the
 * variables in the design, capturing basic information for all the
 * language bodies.
 */
typedef struct {
  Process *p;			///< The process this structure
				///corresponds to
  Scope *cur;			///< The local scope of the process

  unsigned int visited:1;	/**< flags used to check if visited */
  unsigned int isempty:1;	/**< set if this process is empty! */

  struct pHashtable *cH;   /**< connection hash table (map to
			      var). This table maps a unique
			      connection pointer within the local
			      scope to an act_booleanized_var_t pointer. */
  
  struct pHashtable *cdH;  /** connection hash table for dynamic
			       vars. This maps a unique connection
			       pointer in the local scope
			       corresponding that corresponds to a
			       dynamic array to an act_dynamic_var_t
			       pointer. */

  A_DECL (struct netlist_bool_port, chpports); ///< the list of CHP ports
  A_DECL (struct netlist_bool_port, ports);    ///< the list of
					       ///Boolean ports
  
  A_DECL (act_connection *, instports);	       ///< helper list that
					       ///contains the
					       ///connection pointers
					       ///corresponding to the
					       ///instances within the
					       ///process.
  
  A_DECL (act_connection *, instchpports);     ///< [CHP mode] helper list
					       ///that contains the
					       ///connection pointers
					       ///corresponding to the
					       ///instances within the
					       ///process.
  
  A_DECL (struct netlist_global_port, used_globals); ///< the list of
						     ///used global
						     ///signals within
						     ///this process

  A_DECL (act_local_net_t, nets); ///< the list of nets
  struct pHashtable *nH;	  ///< hash to map the unique net
				  ///connection pointer to net index

  ExternMacro *macro;		///< external macro for black boxes,
				///if this process is an external macro.

} act_boolean_netlist_t;



/**
 * @class ActBooleanizePass
 *
 * @brief This pass is used to pre-process information about languages
 * and variables within the design. The pass computes the
 * act_boolean_netlist_t data structure, which contains information
 * about all the local variables used by the process.
 *
 * The reason this is called "Booleanize" is that channels/integers
 * are expanded out into collections of Booleans as specified by the
 * user. For example, imagine the following program:
 *
 * ```
 * defchan e1of2 <: chan(bool) (bool t, f, e) { ... }
 *
 * defproc test (e1of2? l; e1of2 r) { ... }
 * ```
 *
 * Running the Booleanize pass will expand out the ports for `test`
 * into `l.t`, `l.f`, `l.e`, `r.t`, `r.f`, `r.e`. These will be part
 * of the Boolean port list (the ports array). The CHP level of
 * abstraction would have ports `l` and `r`, since we treat those as
 * channels for CHP level modeling, and so those two variables will
 * appear in the chpports array but not the ports array.
 *
 * If in the above example `l.e` and `r.e` are connected to each other
 * internally within `test`, then one of the two ports will be marked
 * as omitted using the `omit` flag (see the netlist_bool_port).
 * 
 */
class ActBooleanizePass : public ActPass {
 public:
  ActBooleanizePass(Act *a); ///< constructor
  ~ActBooleanizePass();  ///< release storage

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
   * names + pins
   *
   * @param p is the top level process
   */
  void createNets (Process *p = NULL);

  /**
   * Print ports/flags computed.
   */
  void Print (FILE *fp, Process *p);

  /**
   * @return the act_dynamic_var_t structure (if it exists) given an
   * act_boolean_netlist_t pointer and a connection pointer
   */
  static act_dynamic_var_t *isDynamicRef (act_boolean_netlist_t *,
					  act_connection *);
  
  /**
   * @return the act_dynamic_var_t structure (if it exists) given an
   * act_boolean_netlist_t pointer and an ActId from the process
   * corresponding to the act_boolean_netlist_t pointer.
   */
  static act_dynamic_var_t *isDynamicRef (act_boolean_netlist_t *,
					  ActId *);
  
  /*-- internal data structures and functions --*/
 private:
  void *local_op (Process *p, int mode = 0); ///< implementation of
					     ///the ActPass operations
  void free_local (void *);		     ///< impementation of the
					     ///ActPass operations

  int black_box_mode;  ///< if set to 1, this pass operates in "black
		       ///box" mode. In this mode, an empty process is
		       ///assumed to correspond to an external black
		       ///box macro and hence all the ports are
		       ///assumed to exist even though they are not
		       ///referenced within the process.

  int _create_nets_run;  ///< set to 1 if the creatNets() operation
			 ///has already been run.

  FILE *_fp;		///< used internally as the FILE pointer for
			///printing.

  /*-- internal functions: generate booleans for a process --*/

  /**
   * This is the local call to create the act_boolean_netlist_t data
   * structure for the process. This assumes that all child processes
   * have been processed already.
   * @param p is the Process of interest
   */
  act_boolean_netlist_t *_create_local_bools (Process *p);

  /**
   * Helper function to print local process info
   * @param p is the local process
   * Implicitly uses _fp as the output file.
   */
  void _print (Process *p);

  /**
   * Recursively expand out ports into their consitituent leaf types
   * @param prefix is used to construct the full ActId name for the
   * expanded port component. This is the prefix to the current
   * instance. 
   * @param n is the boolean netlist data structure
   * @param s is the parent scope of the current user defined object
   * @param u is the current user-defined object
   * @param nochp is a flag saying we've broken down a channel so skip for chp
   */
  void flatten_ports_to_bools (act_boolean_netlist_t *n,
			       ActId *prefix,
			       Scope *s,
			       UserDef *u, int nochp);

  /**
   * Update the used flags in the current act_boolean_netlist_t,
   * based on connections to the ports of a specific instance of a
   * process type
   * @param p is the process type
   * @param vx is a process instance of type Process p.
   * @param n is the netlist where the flags should be updated.
   */
  void update_used_flags (act_boolean_netlist_t *n,
			  ValueIdx *vx, Process *p);

  /**
   * Helper function for update_used_flags().
   * @param n is the netlist where the update should be done
   * @param subinst is the netlist of current instance
   * @param prefix is the prefix to current instance
   * @param s is the scope in which the instance exists
   * @param u is the user-defined type corresponding to the prefix
   * @param count is the in/out used to track instports to make things simpler
   *            for other tools
   * @param count2 is the same as count, but for chp ports
   */
  void rec_update_used_flags (act_boolean_netlist_t *n,
			      act_boolean_netlist_t *subinst,
			      ActId *prefix,
			      Scope *s, UserDef *u, int *count, int *count2);

  /**
   * Helper function for update_used_flags()
   * @param n is the netlist where the port is to be appended
   * @param c is the new port 
   * @param t is the Type for the port
   * @param mode is 0 if this is both a CHP and Boolean port, 1 if
   * this is Boolean-only, and 2 if it is CHP-only
   */
  void append_base_port (act_boolean_netlist_t *n, act_connection *c, Type *t,
			 int mode);

  /*--- create netlist helper functions ---*/

  /** Helper function for createNets() */
  void _createNets (Process *p);

  /** Helper function for createNets() */
  int addNet (act_boolean_netlist_t *n, act_connection *c);

  /** Helper function for createNets() */
  void addPin (act_boolean_netlist_t *n, int netid,
	       const char *name, Array *a,
	       act_connection *pin);

  /** Helper function for createNets() */
  void importPins (act_boolean_netlist_t *n, int netid, const char *name, Array *a, act_local_net_t *net);

};





#endif /* __ACT_BOOLEANIZE_H__ */
