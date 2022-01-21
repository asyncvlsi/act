/*************************************************************************
 *
 *  Monotonic counters
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
#ifndef __COUNT_H__
#define __COUNT_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned int cnt;
  lthread_t *hd, *tl;
} countw_t;

countw_t *count_new (int init_val);
void _count_free (countw_t *, char *file, int line);
void count_await (countw_t *, unsigned int val);
void count_increment (countw_t *, unsigned int amount);
#define count_advance(c) count_increment(c,1)

#define count_free(c) _count_free(c,__FILE__,__LINE__)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __COUNT_H__ */
