/*************************************************************************
 *
 *  Bit-vector implementation of sets.
 *
 *  Copyright (c) 2005, 2019 Rajit Manohar
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
#ifndef __BITSET_H__
#define __BITSET_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long ulong_t;

typedef struct bitset {
  ulong_t *x;
  unsigned int sz;
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
  int bitset_andclear (bitset_t *b, bitset_t *c); /* 1 if the AND of
						     the two is empty */
bitset_t *bitset_copy (bitset_t *b);
int bitset_equal (bitset_t *a, bitset_t *b);

void bitset_expand (bitset_t *b, unsigned int w2);
unsigned int bitset_size (bitset_t *b);

#ifdef __cplusplus
}
#endif

#endif /* __BITSET_H__ */
