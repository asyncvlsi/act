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

#include <act/act.h>

struct netlist_bool_port {
  act_connection *c;		/* port bool */
  unsigned int omit:1;		/* skipped due to aliasing */
  unsigned int input:1;		/* 1 if input, otherwise output */
};

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
  unsigned int width;		/* bit-width for chan/int */
  void *extra;			/* space for rent */
} act_booleanized_var_t;


typedef struct {
  Process *p;
  Scope *cur;

  unsigned int visited:1;	/* flags */
  unsigned int isempty:1;	/* check if this is empty! */

  struct iHashtable *cH;   /* connection hash table (map to var)  */
  struct iHashtable *uH;   /* used act_connection *'s in subckts */

  A_DECL (struct netlist_bool_port, ports);
  A_DECL (act_connection *, instports);

} act_boolean_netlist_t;


void act_booleanize_netlist (Act *, Process *);

#endif /* __ACT_BOOLEANIZE_H__ */
