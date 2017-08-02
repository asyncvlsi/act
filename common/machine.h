/*-*-mode:c++-*-**********************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *
 *************************************************************************/
#ifndef __MYMACHINE_H__
#define __MYMACHINE_H__

#if defined(__FreeBSD__)
#include <machine/endian.h>
#endif

#if !defined (BIG_ENDIAN) && !defined(LITTLE_ENDIAN)

/* need lots of crap here */
#define BIG_ENDIAN 0
#define LITTLE_ENDIAN 1

#if defined (__i386__)

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
