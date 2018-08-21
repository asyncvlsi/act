/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __ACT_PASSES_H__
#define __ACT_PASSES_H__

#include <act/act.h>

void act_expand (Act *a);

void act_prsflat_prsim (Act *a);
void act_prsflat_lvs (Act *a);

void act_flat_apply_conn_pairs (Act *a, void *cookie,
				void (*f)(void *c, ActId *one, ActId *two));

void act_flat_apply_processes (Act *a, void *cookie,
			       void (*f)(void *c, ActId *name, Process *p));

#endif /* __ACT_PASSES_H__ */
