/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2007, 2018-2019 Rajit Manohar
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
#ifndef __V2ACT_H__
#define __V2ACT_H__

#include <stdio.h>
#include <stdlib.h>
#include <act/act.h>
#include <common/misc.h>
#include "vnet.h"


enum V2ACT_mode {
  V_ASYNC,
  V_SYNC
};

extern int mode;		/* mode */

void label_clocks (VNet *w, const char *clk);

void emit_types (VNet *);
void free_typetable (struct Hashtable *);
void update_id_info (id_info_t *);
void update_conn_info (id_info_t *);

void emit_id_deref (FILE *fp, id_deref_t *id);
int array_length (conn_info_t *c); /* crazy */
void emit_conn_rhs (FILE *fp, conn_rhs_t *r, list_t *l);

void compute_fanout (VNet *v, module_t *m);
void emit_verilog (VNet *);

extern char *channame;

#endif /* __V2ACT_H__ */
