/*************************************************************************
 *
 *  Basic timing functions
 *
 *  Copyright (c) 1997, 2019 Rajit Manohar
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
#ifndef __MY_TIME_H__
#define __MY_TIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#define TIME_64

#ifndef TIME_RETURN_TYPE
#define TIME_RETURN_TYPE double
#endif

#ifdef F77_USE_UNDERSCORES
TIME_RETURN_TYPE cputime_msec_ ();
TIME_RETURN_TYPE realtime_msec_ ();
#else
TIME_RETURN_TYPE realtime_msec ();
TIME_RETURN_TYPE cputime_msec ();
#endif


/* Ticks */
#ifdef TIME_64
typedef unsigned long long Time_t;
#else
typedef unsigned long Time_t;
#endif

#define time_init(t)   do { t = 0; } while (0)
#define time_inc(t,v)  do { t += v; } while (0)
#define time_max(a,b)  ((a) > (b) ? (a) : (b))
#define time_min(a,b)  ((a) < (b) ? (a) : (b))
#define time_print(fp,t)  fprintf(fp,"%lu",(unsigned long)(t))
#define time_sprint(fp,t)  do { context_disable(); time_print (fp,t); context_enable (); } while (0)
#define time_add(t1,t2) ((t1)+(t2))

#ifdef __cplusplus
}
#endif

#endif /* __MY_TIME_H__ */
