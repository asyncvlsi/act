/*************************************************************************
 *
 *  Locks
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
#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <common/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lthread_t *hd, *tl;
  int busy;
} mutex_t;

typedef struct {
  lthread_t *hd, *tl;
  mutex_t *lock;
} cond_t;

mutex_t *mutex_new (void);
void mutex_free (mutex_t *);
void mutex_lock (mutex_t *);
void mutex_unlock (mutex_t *);

cond_t *cond_new (mutex_t *);
void cond_free (cond_t *);
void cond_wait (cond_t *);
void cond_signal (cond_t *);
int cond_waiting (cond_t *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MUTEX_H__ */
