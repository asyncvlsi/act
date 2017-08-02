/*************************************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *
 *************************************************************************/
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
