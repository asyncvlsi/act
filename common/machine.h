/*************************************************************************
 *
 *  Figure out something about the machine...
 *
 *  Copyright (c) 1999, 2019 Rajit Manohar
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
#ifndef __MYMACHINE_H__
#define __MYMACHINE_H__

#if defined(__FreeBSD__)
#include <machine/endian.h>
#endif

#if !defined (BIG_ENDIAN) && !defined(LITTLE_ENDIAN)

/* need lots of crap here */
#define BIG_ENDIAN 0
#define LITTLE_ENDIAN 1

#if defined (__i386__) || defined(__x86_64__)

#define BYTE_ORDER LITTLE_ENDIAN

#elif defined (__sparc__)

#define BYTE_ORDER BIG_ENDIAN

#elif defined(__sgi)

#define BYTE_ORDER BIG_ENDIAN

#elif defined(__alpha__)

#define BYTE_ORDER LITTLE_ENDIAN

#else

#error What on earth?

#endif

#endif

#ifndef BYTE_ORDER

#if defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)

#define BYTE_ORDER    LITTLE_ENDIAN
#define BIG_ENDIAN !LITTLE_ENDIAN

#if BYTE_ORDER == BIG_ENDIAN
#error Wowee
#endif

#elif defined (BIG_ENDIAN) && !defined(LITTLE_ENDIAN)

#define BYTE_ORDER    BIG_ENDIAN
#define LITTLE_ENDIAN !BIG_ENDIAN

#if BYTE_ORDER == LITTLE_ENDIAN
#error Wowee
#endif

#else

#error Fix byte order file!

#endif

#endif


#endif /* __MYMACHINE_H__ */
