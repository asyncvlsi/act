/*************************************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *
 *************************************************************************/
#ifndef __MUTEX_H__
#define __MUTEX_H__

#include "thread.h"

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
