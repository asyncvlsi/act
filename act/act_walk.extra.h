/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __ACT_EXTRA_H__
#define __ACT_EXTRA_H__

#include <stdio.h>
#include <string.h>
#include "list.h"
#include "except.h"
#include "qops.h"
#include "misc.h"
#include <act/lang.h>
#include <act/path.h>
#include <act/namespaces.h>
#include <act/treetypes.h>
#include <act/types.h>
#include <act/inst.h>

#define OPT_EXISTS(x)    !list_isempty (x)
#define OPT_EMPTY(x)     list_isempty (x)
#define LIST_VALUE(x)     ((ActRet *)list_value (x))
#define OPT_VALUE(x)     LIST_VALUE (list_first (x))
#define OPT_FREE(x)      list_free(x)
#define OPT_VALUE2(x)    LIST_VALUE (list_next (list_first (x)))

Expr *const_expr (int val);
Expr *act_walk_X_expr (ActTree *cookie, Expr *e);
void print_ns_string (FILE *fp, list_t *l);

#endif /* __ACT_EXTRA_H__ */
