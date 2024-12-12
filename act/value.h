/*************************************************************************
 *
 *  This file is part of the ACT library
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
#ifndef __ACT_VALUE_H__
#define __ACT_VALUE_H__


class InstType;
class ValueIdx;
struct act_attr;

#include <act/basetype.h>

/**
 * @class act_connection
 *
 * @brief Connections
 *
 * This is the core data structure that maintains hierarchical
 * connectivity information. An act_connection pointer is associated
 * with a scope. Within the scope, the act_connection pointer can be
 * used to identify the unique instance (or part of an instance)
 * associated with an identifier within the scope.
 *
 * When there is a parameter, we don't use connection pointers since
 * we simply bind each parameter to its value. 
 *
 * For non-parameter types, we represent connections using a
 * combination of a union-find tree (to make it cheap to find the
 * canonical connection pointer), and a ring of connections (so that
 * we can find all other instances/instance fragments that are
 * connected together).
 *
 * For simple connections (e.g. x = y, where x and y are bool types),
 * the union-find + tree structure is easy to understand.
 *  
 * We also directly represent connections between user-defined types
 * as well as arrays. So for example in the case of
 * ```
 *   bool x[4], y[4];
 *   x = y
 * ```
 * we would represent this as a *single* connection between two
 * arrays, rather than four element-wise connections between bools.
 * We could also have situations such as:
 * ```
 *   bool x[4], y[4], z;
 *   x = y;
 *   x[3] = z;
 * ```
 * In this case, we have both an array connection, as well as a
 * connection to an individual element.
 *
 * To represent complex connections, each instance has a potential
 * connection *tree*. For an array like `x` above, that means we have:
 *   - a connection pointer for `x` itself 
 *   - sub-connections within `x`. These are represented as an array
 * of act_connection pointers within the connection structure for
 * `x`.
 * Each element of the sub-connection array corresponds to an element
 * of the array. The same idea is used for user-defined types. The
 * sub-connection array corresponds to the port parameters from left
 * to right. For an array of user-defined types, there are two levels
 * of hierarchy: the first is for the array, and then the next is for
 * the user-defined type.
 * 
 * In a connection tree, the parent pointer can be used to go back up in
 * the connection tree.
 *
 * Connections are mainted in a manner that the root of the union-find
 * tree is the _canonical_ connection pointer for a particular
 * connection. The rules for being canonical are as follows:
 *   - a global connection takes precedence over a non-global
 * connection
 *   - a port connection takes precedence over a non-port connection
 *   - an array connection takes precedence over a non-arrayed
 * connection (this is to keep arrays as "compact" as possible later
 * on in the design flow)
 *   - all other things being the same, the smallest (in terms of
 * string lexicographic ordering) name with the fewest hierarchy
 * separators is given precedence (so if a.b = c, then c would be used
 * as the canonical name).
 *
 * Subconnection pointers are allocated in a lazy fashion.
 *
 * Connections to globals are special. This is because we do not
 * replicate all the connection pointers on a per-instance
 * basis. Instead, if a global signal is connected to a local one
 * within a scope, then that is represented by a single
 * connection---the representative for all instances of the entire
 * user-defined type.
 */
class act_connection {
public:
  ValueIdx *vx;			///< identifier that has been
				///allocated within the scope of this
				///connection that corresponds to this
				///particular connection pointer. Note
				///that this could be NULL for
				///sub-connection elements. When a
				///user-defined type is traversed, the
				///vx pointer switches scope to the
				///user-defined type self. This is
				///done to speed up lookups.


  act_connection *parent;	///< parent in the subconnection tree
				///for id.id or id[val]
  
  act_connection *up;		///< up pointer in the union-find data
				///structure
  
  act_connection *next;	        ///< next pointer in the ring of
				///connections used to find all other
				///connections to this particular one.
  
  act_connection **a;	///< slots for root arrays and root userdefs
			///for sub-connections

  /**
   * Constuctor
   * @param _parent is the parent pointer, if any
   */
  act_connection(act_connection *_parent = NULL);

  
  ActId *toid();		///< convert the connection pointer
				///into a freshly allocated ActId name
				///corresponding to this connection element.
  
  bool isglobal();		///< returns true if this is a global
				/// signal

  ActNamespace *getnsifglobal(); ///< get namespace for a global
				 ///signal, if any

  /**
   * Check if this is a primary connection pointer: the canonical one
   * for this connection. This is done by checking the up pointer in
   * the union-find tree.
   * @return true if this is a primary connection, false otherwise
   */
  bool isPrimary() { return (up == NULL) ? 1 : 0; }

  /**
   * Check if the i-th sub-connection for this type is a primary
   * connection. Note that the sub-connections may not exist so a
   * non-NULL test is part of this function.
   *
   * This should only be called if hasSubconnections() is true.
   *
   * @param i is the index of the sub-connection
   * @return true if the i-th sub-connection is a primary connection,
   * false otherwise.
   */
  bool isPrimary(int i)  { return a[i] && (a[i]->isPrimary()); }

  
  /**
   *  Return a subconnection pointer, allocating slots if necessary. 
   *
   *  @param sz is the # of subconnections possible for this object
   *  @param idx is index of subconnection
   *  @return the sub-connection slot
   */
  act_connection *getsubconn(int idx, int sz);

  /**
   * Given a sub-connection slot, find the index in the sub-connection
   * array that matches this one. NOTE: this is a slow call, since it
   * does a linear search through the sub-connection pointer
   * array. The act.suboffset_limit parameter is used to truncate the
   * search, and print a warning.
   *
   * @param c is the sub-connection pointer to lookup
   * @return offset of subconnection c within current connection
   * object 
   */
  int suboffset (act_connection *c);

  /**
   * For sub-connection pointers (note that the parent must be
   * non-NULL!), return my offset within my parent.
   * @return my sub-connection offset within my parent
   */
  int myoffset () { return parent->suboffset (this); }
  
  /**
   * Check to see if there are direct connections to this particular
   * connection pointer. This is done by looking at the connection
   * ring.
   * @return true if there are direct connections, false otherwise
   */
  bool hasDirectconnections() { return next != this; }

  /**
   * Check if there are any connections to this particular instance
   * @return true if there are either direct connections or
   * connections to an part of the instance
   */
  bool hasAnyConnection();

  /**
   * This checks to see if there are any direct connections to the
   * i-th subconnection for this instance
   * @param i is the sub-connection index to inspect
   * @return true if there are any direct connections to the i-th
   * subconnection, false otherwise.
   */
  bool hasDirectconnections(int i) { return a && a[i] && a[i]->hasDirectconnections(); }
  
  /**
   * @return true if this is a complex object (array or user-defined)
   * with potential subconnections to it, false otherwise
   */
  bool hasSubconnections() { return a != NULL; }

  /**
   * Check to see if there are sub-connections registered for the i-th
   * sub-connection within this object
   * @param i is the sub-connection index to inspect
   * @return true if there are sub-connetions at position i, false otherwise
   */
  bool hasSubconnections(int i) { return a && a[i]; }


  /**
   * Return the number of potential sub-connection slots for this
   * instance. This corresponds to the size of the array, or the
   * number of port parameters for a user-defined type. If the lazily
   * allocated array pointer is NULL, this returns 0. Use this to
   * iterate over potential sub-connection slots.
   *
   * @return the number of potential sub-connections
   */
  int numSubconnections();

  /**
   * This returns the number of subconnection slots that could exist
   * for this particular instance. This is called by
   * numSubconnections() when the array pointer is non-NULL
   * @return the number of potential sub-connection slots
   */
  int numTotSubconnections();

  /**
   * The ValueIdx pointer for a connection is the primary instance
   * corresponding to this particular connection pointer. For a simple
   * instance, it is clear what it is. But for an array instance, this
   * would be the ValueIdx for the parent. If this connection pointer
   * corresponds to part of a user-defined type, it would be the
   * ValueIdx for the parent as well. For arrays of user-defined
   * types, it would be the ValueIdx  in the parent's parent.
   *
   * @return the ValueIdx pointer for this connection
   */
  ValueIdx *getvx();

  /**
   * A connection type can be identified as one of four possibilies
   *   - standard  "x"
   *   - array element "x[i]"
   *   - port "x.y"
   *   - array element + port "x[i].y"
   *
   * The connection pointer has a "vx" that corresponds to the
   * "closest" ValueIdx that it corresponds to. That vx could be at a
   * distance of 0 (e.g. a standard instance), 1 (array or port), or 2
   * (array element + port) away.
   *
   * This function returns the distance of the vx pointer from the
   * connection pointer. If it returns 3, then that means it is
   * further away.
   *
   * @return the connection type for this pointer
   */
  unsigned int getctype();

  /**
   * Return the primary connection pointer for this one. This does the
   * self-flattening operation for the union-find tree
   * @return primary connection pointer
   */
  act_connection *primary();

  /**
   * This computes the direction flags for this connection pointer (in
   * or out). This translates INOUT/OUTIN flags to the correct version
   * given the context. It returns either Type::NONE, Type::IN, or
   * Type::OUT.
   * @return the direction flag for this connection
   */
  Type::direction getDir();

  /**
   * Disconnect: only works for non-primary connections.
   * @return true on success, false otherwise
   */
  bool disconnect ();

  /**
   * Check to see if this connection is disconnectable
   * @return true if this can be disconnected, false otherwise
   */
  bool disconnectable ();


  /**
   * Check connection invariant in terms of vx positions
   * @return true if validated, false otherwise
   */
  bool vxValidate ();

  /**
   * Print the ActId corresponding to this connection pointer
   * @param fp is the output file
   */
  void Print (FILE *fp);

  /**
   * Debug print parents and vx values to root.
   * @param fp is the output file
   */
  void printUpTree (FILE *fp);
};


/**
 * @class ValueIdx
 *
 * @brief This class is used to create an instance in a scope. The
 * name comes from the fact that this is used to keep track of the
 * index of a value within a particular type (rather than a flat data
 * structure, one per instance) and so is not defined as a
 * value-per-instance, but rather a value-per-type.
 *
 * When a value is "allocated", it means it contains information about
 * its value. 
 * 
 * A value can also have a list of instance attributes associated with
 * it. These are also held in this data structure.
 *
 */
class ValueIdx {
public:
  InstType *t;			///< the type corresponding to this
				///particular instance
  
  struct act_attr *a;		///< instance attributes for the value
  struct act_attr **array_spec;	///< array deref-specific attributes,
				///if any
  
  unsigned int init:1;	   ///< Has this been allocated? 0 = no
			   /// allocation, 1 = allocated

  unsigned int immutable:1;	///< for parameter types: immutable if
				///it is in a namespace or a template
				///parameter

  ActNamespace *global;	     ///< set for a namespace global; NULL
			     ///otherwise. Note that global =>
			     ///immutable, but not the other way around

  union {
    long idx;		   ///< Base index for allocated storage for
			   ///parameterized types
    struct {
      act_connection *c;	///< For non-parameter types, the
				///connection pointer. This is
				///allocated in a lazy fashion.
      
      const char *name;		///<  the base name, from the hash
				///table lookup: DO NOT FREE

    } obj;			///< information about the object for
				///non-paramter types
    
  } u;				///< the value associated with this instance


  /**
   * ONLY FOR NON-PARAM TYPES.
   * For non-parameter types, this checks if there is a connection
   * pointer associated with this object.
   * @return true if there is a connection pointer, false otherwise
   */
  bool hasConnection()  { return init && (u.obj.c != NULL); }

  /**
   * ONLY FOR NON-PARAM TYPES.
   * Checks if there is any connection associated with this object
   * @return true if there is any connection/subconnection to this
   * object, false otherwise
   */
  bool hasAnyConnection() { if (!hasConnection()) return false; else return u.obj.c->hasAnyConnection(); }

  /**
   * ONLY FOR NON-PARAM TYPES.
   * Checks if there is a connection pointer associated with a
   * sub-connection at index i for this object
   * @return true if there is a sub-connection at index i, false
   * otherwise
   */
  bool hasConnection(int i) { return init && (u.obj.c != NULL) && (u.obj.c->a) && (u.obj.c->a[i]); }

  /**
   * ONLY FOR NON-PARAM TYPES.
   * Check if there are any sub-connections to this object
   * @return true if there are any sub-connections, false otherwise
   */
  bool hasSubconnections() { return hasConnection() && connection()->hasSubconnections(); }

  /**
   * ONLY FOR NON-PARAM TYPES.
   * Check if this is a primary instance
   * @return true if this is the primary (canonical) instance, false otherwise
   */
  bool isPrimary() { return !hasConnection() || (u.obj.c->isPrimary()); }

  /**
   * ONLY FOR NON-PARAM TYPES.
   * Check if the sub-connection at index i is the primary instance,
   * @return true if the i-th sub-connection is the primary instance,
   * false otherwise.
   */
  bool isPrimary(int i) { return !hasConnection(i) || (u.obj.c->a[i]->isPrimary()); }

  /**
   * ONLY FOR NON-PARAM TYPES.
   * @return the connection pointer associated with this instance,
   * NULL if none yet
   */
  act_connection *connection() { return init ? u.obj.c : NULL; }

  /**
   * ONLY FOR NON-PARAM TYPES.
   * @return the string name for the instance
   */
  const char *getName() { return u.obj.name; }

  /**
   * @return the attributes associated with this instance
   */
  act_attr *getAttr() { return a; }

  /**
   * For attributes associated with array instances
   * @param i is the index into the array
   * @return the attributes associated with the sub-array at index i
   * for this instance
   */
  act_attr *getAttrIdx(int i) { return array_spec ? array_spec[i] : NULL; }

  /**
   * Check if there is an array-specific attribute specifier
   * @return true if an array specifier exists, false otherwise
   */
  bool haveAttrIdx() { return array_spec ? true : false; }

  /**
   * @return the number of indices that are valid for the attribute
   * index check
   */
  int numAttrIdx();
};

/**
 * Function used to actually make a connection. This also determines
 * which of the two connections should be  the canonical one according
 * to the rules for an act_connection.
 *
 * @param id1 is the id for the first connection element
 * @param c1 is the connection pointer for id1
 * @param id2 is the id for the second connection element
 * @param c2 is the connection pointer for id2
 * @param ux is the user-defined type, if any for the connection
 * context. This is needed to determine which of the two is
 * canonical. 
 */
void act_mk_connection (UserDef *ux, ActId *id1, act_connection *c1,
			ActId *id2, act_connection *c2);

/**
 * Merge the attributes of two instances. The vx1 and vx2 pointers are
 * used for error reporting only. The real merge happens between the
 * attribute lists provided.
 *
 * @param vx1 is the name of one of the instances
 * @param vx2 is the name of the other instance
 * @param a is the attribute list that should be merged
 * @param x is the attribute list into which the attributes should be
 * merged
 *
 */
void act_merge_attributes (ValueIdx *vx1, ValueIdx *vx2,
			   struct act_attr **x, struct act_attr *a);

/**
 * The union-find connection function. Make a simple connection,
 * combining c1 and c2. c1 has the canonical connection pointer.
 *
 * @param c1 is the canonical one
 * @param c2 is the one to connect to c1
 */
void _act_mk_raw_connection (act_connection *c1, act_connection *c2);

/**
 * Print out the attribute list
 * @param fp is the output file
 * @param a is the attribute list
 */
void act_print_attributes (FILE *fp, act_attr *a);

#endif /* __ACT_VALUE_H__ */

