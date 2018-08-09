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

extern output_formats export_format;

void aflat_prs (Act *a);

#endif /* __AFLAT_H__ */
