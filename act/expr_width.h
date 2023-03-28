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


/**
   @file expr_width.h

   @brief This file contains a macro that is used to compute the
   bit-width of the result of two sub-expressions based on different
   combination modes. The bit-width rules for ACT expressions fall
   into one of the modes specified in this file.

   Modes:
      0 = max of lw. rw
      1 = max of lw, rw + 1
      2 = sum of lw, rw
      3 = 1 (bool result)
      4 = lw itself
      5 = rw itself
      6 = lw + (2^rw-1)
  */

#define WIDTH_MAX   0   ///< set width to the max of lw, rw
#define WIDTH_MAX1  1   ///< set width to max of lw, rw + 1
#define WIDTH_SUM   2   ///< set width to sum of lw, rw
#define WIDTH_BOOL  3   ///< set width to 1
#define WIDTH_LEFT  4   ///< set width to lw
#define WIDTH_RIGHT 5   ///< set width to rw
#define WIDTH_LSHIFT 6  ///< set width to lw + 2^rw-1

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
