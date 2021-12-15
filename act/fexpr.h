#ifndef __ACT_FEXPR_H__
#define __ACT_FEXPR_H__

#include <stdio.h>
#include <act/lang.h>
#include <common/file.h>
#include "treetypes.h"
#include "expr.h"

extern Expr *(*expr_parse_basecase_extra)(LFILE *l);

void act_init_fexpr (LFILE *);
int act_is_a_fexpr (LFILE *);
void act_free_a_fexpr (void *);
void *act_parse_a_fexpr (LFILE *);
void *act_walk_X_fexpr (ActTree *, void *);


#endif /* __ACT_FEXPR_H__ */
