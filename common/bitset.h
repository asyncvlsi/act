/*************************************************************************
 *
 *  Copyright (c) 2009 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __BITSET_H__
#define __BITSET_H__

typedef unsigned long ulong_t;

typedef struct bitset {
  ulong_t *x;
  int sz;
} bitset_t;

void bitset_clear (bitset_t *b);
void bitset_free (bitset_t *b);
bitset_t *bitset_new (unsigned int w);
void bitset_print (bitset_t *f);
void bitset_printw (bitset_t *f, int w);
void bitset_set (bitset_t *f, unsigned int v);
void bitset_clr (bitset_t *f, unsigned int v);
int bitset_tst (bitset_t *f, unsigned int v);

void bitset_or (bitset_t *l, bitset_t *r);
void bitset_and (bitset_t *l, bitset_t *r);
void bitset_xor (bitset_t *l, bitset_t *r);
int bitset_isclear (bitset_t *b);
bitset_t *bitset_copy (bitset_t *b);
int bitset_equal (bitset_t *a, bitset_t *b);

void bitset_expand (bitset_t *b, unsigned int w2);

#endif /* __BITSET_H__ */
