/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
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
