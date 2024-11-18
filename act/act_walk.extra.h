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
#ifndef __ACT_EXTRA_H__
#define __ACT_EXTRA_H__

#include <stdio.h>
#include <string.h>
#include <common/list.h>
#include <common/except.h>
#include <common/qops.h>
#include <common/misc.h>
#include <common/config.h>
#include <act/lang.h>
#include <act/path.h>
#include <act/namespaces.h>
#include <act/types.h>
#include <act/inst.h>
#include <act/body.h>
#include <act/treetypes.h>

#define OPT_EXISTS(x)    !list_isempty (x)
#define OPT_EMPTY(x)     list_isempty (x)
#define LIST_VALUE(x)     ((ActRet *)list_value (x))
#define OPT_VALUE(x)     LIST_VALUE (list_first (x))
#define OPT_FREE(x)      list_free(x)
#define OPT_VALUE2(x)    LIST_VALUE (list_next (list_first (x)))
#define OPT_VALUE3(x)    LIST_VALUE (list_next (list_next (list_first (x))))
#define OPT_VALUE4(x)    LIST_VALUE (list_next (list_next (list_next (list_first (x)))))


/* for dataflow repetition checks */
#define NO_LOOP(x)  (((act_dataflow_loop *)(x))->id == NULL)

Expr *const_expr (long val);
Expr *act_walk_X_expr (ActTree *cookie, Expr *e);
void print_ns_string (FILE *fp, list_t *l);
int _act_id_is_true_false (ActId *id);
int _act_id_is_enum_const (ActOpen *os, ActNamespace *ns, ActId *id);
int _act_shadow_warning (void);
int _act_is_reserved_id (const char *s);

#endif /* __ACT_EXTRA_H__ */
