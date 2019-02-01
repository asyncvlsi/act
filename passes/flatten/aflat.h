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
#ifndef __AFLAT_H__
#define __AFLAT_H__

#include <act/act.h>

enum output_formats {
  PRSIM_FMT,
  LVS_FMT
};


void act_expand (Act *a);
void aflat_prs (Act *a, output_formats fmt);


void act_flat_apply_conn_pairs (Act *a, void *cookie,
				void (*f)(void *c, ActId *one, ActId *two));
void act_flat_apply_processes (Act *a, void *cookie,
			       void (*f)(void *c, ActId *name, Process *p));


#endif /* __AFLAT_H__ */
