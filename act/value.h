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


class ActNamespace;
class InstType;
class ActId;
struct ValueIdx;
struct act_attr;

#include <act/basetype.h>

/*
  Connections

  For non-parameter types, we represent connections.
   - ring of connections
   - union-find tree
*/
struct act_connection {
  ValueIdx *vx;			// identifier that has been allocated

  act_connection *parent;	// parent for id.id or id[val]
  
  act_connection *up;
  act_connection *next;
  act_connection **a;	// slots for root arrays and root userdefs


  act_connection(act_connection *_parent = NULL);

  

  ActId *toid();		// the ActId name for this connection entity
  
  bool isglobal();		// returns true if this is a global
				// signal

  bool isPrimary() { return (up == NULL) ? 1 : 0; }
  bool isPrimary(int i)  { return a[i] && (a[i]->isPrimary()); }

  
  /* return subconnection pointer; allocate slots if necessary. 
        sz = # of subconnections possible for this object
       idx = index of subconnection
   */
  act_connection *getsubconn(int idx, int sz);

  /* return offset of subconnection c within current connection object */
  int suboffset (act_connection *c);

  /* return my offset within my parent */
  int myoffset () { return parent->suboffset (this); }
  
  // returns true when there are other things connected to
  // the object directly
  bool hasDirectconnections() { return next != this; }

  // returns true when there is a subconnection at index i
  bool hasDirectconnections(int i) { return a && a[i] && a[i]->hasDirectconnections(); }
  
  // returns true if this is a complex object (array or user-defined)
  // with potential subconnections to it
  bool hasSubconnections() { return a != NULL; }
  bool hasSubconnections(int i) { return a && a[i]; }
  int numSubconnections();
  
  ValueIdx *getvx();		/* this returns the root ValueIdx for
				   this particular connection */
  
  unsigned int getctype();
  // 0 = standard  "x"
  // 1 = array element "x[i]"
  // 2 = port "x.y" 
  // 3 = array element + port "x[i].y"

  act_connection *primary(); // return primary designee for this connection

  Type::direction getDir(); // get direction flags: none, in, or out.
};


/*
  Values: this is the core expanded data structure
*/
struct ValueIdx {
  InstType *t;
  struct act_attr *a;			// attributes for the value
  struct act_attr **array_spec;	// array deref-specific value
  
  unsigned int init:1;	   /**< Has this been allocated? 
			         0 = no allocation
				 1 = allocated
			    */
  unsigned int immutable:1;	/**< for parameter types: immutable if
				   it is in a namespace or a template
				   parameter */

  ActNamespace *global;	     /**< set for a namespace global; NULL
				otherwise. Note that global =>
				immutable, but not the other way
				around. */
  
  union {
    long idx;		   /**< Base index for allocated storage for
			      ptypes */
    struct {
      act_connection *c;	/**< For non-parameter types */
      const char *name;		/**< the base name, from the hash
				   table lookup: DO NOT FREE */
    } obj;
  } u;

  /* assumes object is not a parameter type */
  bool hasConnection()  { return init && (u.obj.c != NULL); }
  bool hasConnection(int i) { return init && (u.obj.c != NULL) && (u.obj.c->a) && (u.obj.c->a[i]); }

  bool hasSubconnections() { return hasConnection() && connection()->hasSubconnections(); }
  
  bool isPrimary() { return !hasConnection() || (u.obj.c->isPrimary()); }
  bool isPrimary(int i) { return !hasConnection(i) || (u.obj.c->a[i]->isPrimary()); }
  act_connection *connection() { return init ? u.obj.c : NULL; }
  const char *getName() { return u.obj.name; }
};

void act_mk_connection (UserDef *ux, const char *s1, act_connection *c1,
		    const char *s2, act_connection *c2);

void act_merge_attributes (struct act_attr **x, act_attr *a);
void _act_mk_raw_connection (act_connection *c1, act_connection *c2);

#endif /* __ACT_VALUE_H__ */

