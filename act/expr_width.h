/*************************************************************************
 *
 *  This file is part of the ACT library
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
#ifndef __ACT_EXPR_WIDTH_H__
#define __ACT_EXPR_WIDTH_H__


  /*
    Modes:
       0 = max of lw. rw
       1 = max of lw, rw + 1
       2 = sum of lw, rw
       3 = 1 (bool result)
  */
#define WIDTH_MAX   0
#define WIDTH_MAX1  1
#define WIDTH_SUM   2
#define WIDTH_BOOL  3
#define WIDTH_LEFT  4
#define WIDTH_RIGHT 5
#define WIDTH_LSHIFT 6

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif  

#define WIDTH_UPDATE(mode)			\
  if (width) {					\
    if (mode == WIDTH_MAX) {			\
      *width = MAX(lw,rw);			\
    }						\
    else if (mode == WIDTH_MAX1) {		\
      *width = MAX(lw,rw)+1;			\
    }						\
    else if (mode == WIDTH_SUM) {		\
      *width = lw+rw;				\
    }						\
    else if (mode == WIDTH_BOOL) {		\
      *width = 1;				\
    }						\
    else if (mode == WIDTH_LEFT) {		\
      *width = lw;				\
    }						\
    else if (mode == WIDTH_RIGHT) {		\
      *width = rw;				\
    }						\
    else if (mode == WIDTH_LSHIFT) {		\
      *width = lw + ((1 << rw)-1);		\
    }						\
  }

#endif /* __ACT_EXPR_WIDTH_H__ */
