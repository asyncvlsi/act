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

#endif /* __AFLAT_H__ */
