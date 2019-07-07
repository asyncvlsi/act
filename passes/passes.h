/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#ifndef __ACT_PASSES_H__
#define __ACT_PASSES_H__

#include <act/act.h>

void act_expand (Act *a);

/* emit flattened production rule set */
void act_prsflat_prsim (Act *a);
void act_prsflat_lvs (Act *a);

/* flat connection pair pass */
void act_flat_apply_conn_pairs (Act *a, void *cookie,
				void (*f)(void *c, ActId *one, ActId *two));
void act_flat_apply_conn_pairs (Act *a, void *cookie, Process *top,
				void (*f)(void *c, ActId *one, ActId *two));


/* flat process pass */
void act_flat_apply_processes (Act *a, void *cookie,
			       void (*f)(void *c, ActId *name, Process *p));
void act_flat_apply_processes (Act *a, void *cookie, Process *top,
			       void (*f)(void *c, ActId *name, Process *p));

/* create netlist */  
void act_prs_to_netlist (Act *, Process *);

/* booleanize */
void act_booleanize_netlist (Act *, Process *);

/* create unique boolean port list */
void act_create_bool_ports (Act *, Process *);

/* create cells from processes */
void act_prs_to_cells (Act *a, Process *p, int add_cells = -1);
void act_emit_celltable (FILE *fp, Act *a);



#endif /* __ACT_PASSES_H__ */
