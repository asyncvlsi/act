/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011, 2018-2019 Rajit Manohar
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
#ifndef __PRS_H__
#define __PRS_H__

#include <stdio.h>
#include "act/lang/lang.h"
#include "act/common/file.h"
#include "treetypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void act_init_prs_expr (LFILE *);
int act_is_a_prs_expr (LFILE *);
void act_free_a_prs_expr (void *);
void act_free_a_prs_exexpr (void *);
void *act_parse_a_prs_expr (LFILE *);
void *act_walk_X_prs_expr (ActTree *, void *);

#ifdef __cplusplus
}
#endif

#endif /* __PRS_H__ */
