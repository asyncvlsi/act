/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
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
#ifndef __ACT_INLINE_H__
#define __ACT_INLINE_H__

#include "act/lang/expr.h"
#include <act/act_id.h>

struct act_inline_table;

act_inline_table *act_inline_new (Scope *sc, act_inline_table *parent);
void act_inline_free (act_inline_table *);

void act_inline_setval (act_inline_table *, ActId *id, Expr **e);
Expr **act_inline_getval (act_inline_table *, const char *s);
int act_inline_isbound (act_inline_table *, const char *s);

Expr **act_inline_expr (act_inline_table *, Expr *, int recurse_fn = 1);
Expr *act_inline_expr_simple (act_inline_table *, Expr *, int recurse_fn = 1);

act_inline_table *act_inline_merge_tables (int nT, act_inline_table **T, Expr **cond);

#endif /*  __ACT_INLINE_H__ */
