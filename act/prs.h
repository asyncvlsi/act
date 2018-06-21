/*************************************************************************
 *
 *  Copyright (c) 2011-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __PRS_H__
#define __PRS_H__

#include <stdio.h>
#include <act/lang.h>
#include "file.h"
#include <act/treetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void act_init_prs_expr (LFILE *);
int act_is_a_prs_expr (LFILE *);
void act_free_a_prs_expr (void *);
void *act_parse_a_prs_expr (LFILE *);
void *act_walk_X_prs_expr (ActTree *, void *);

#ifdef __cplusplus
}
#endif

#endif /* __PRS_H__ */
