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
#include <stdio.h>
#include <stdlib.h>
#include "act/common/bitset.h"
#include "act/common/misc.h"


void bitset_clear (bitset_t *b)
{
  int i;
  for (i=0; i < b->sz; i++)
    b->x[i] = 0;
}

bitset_t *bitset_alloc (unsigned int w)
{
  bitset_t *b;
  
  if (sizeof (ulong_t) < 4) {
    printf ("err... help\n");
    exit (1);
  }

  NEW (b, bitset_t);
  b->sz = (w+31)/32;
  MALLOC (b->x, ulong_t, b->sz);

  return b;
}

bitset_t *bitset_new (unsigned int w)
{
  bitset_t *b = bitset_alloc (w);
  
  bitset_clear (b);
  return b;
}

void bitset_expand (bitset_t *b, unsigned int w)
{
  int o;

  if ((w+31)/32 < b->sz) {
    /* nothing to do */
    return;
  }
  Assert ((w+31)/32 >= b->sz, "Shrinking instead of expanding?");
  if ((w+31)/32 == b->sz) return;
  o = b->sz;
  b->sz = (w+31)/32;
  REALLOC (b->x, ulong_t, b->sz);
  for (; o < b->sz; o++) {
    b->x[o] = 0;
  }
}

void bitset_free (bitset_t *b)
{
  FREE (b->x);
  FREE (b);
}

void bitset_print (bitset_t *f)
{
  int i, val;

  for (i=0; i < f->sz*32; i++) {
    val = (f->x[i/32] >> (i % 32)) & 0x1;
    printf ("%d", val);
  }
}

void bitset_printw (bitset_t *f, int w)
{
  int i, val;

  for (i=0; i < w; i++) {
    val = (f->x[i/32] >> (i % 32)) & 0x1;
    putchar (val + '0');
  }
}


void bitset_set (bitset_t *f, unsigned int v)
{
  f->x[v >> 5] |= (1 << (v & 31));
}

void bitset_clr (bitset_t *f, unsigned int v)
{
  f->x[v >> 5] &= ~(1UL << (v & 31));
}

int bitset_tst (bitset_t *f, unsigned int v)
{
  return (f->x[v >> 5] & (1 << (v & 31)));
}

void bitset_or (bitset_t *l, bitset_t *r)
{
  int i;

  if (l->sz != r->sz) 
    fatal_error ("Sizes must match!");

  for (i=0; i < l->sz; i++)
    l->x[i] |= r->x[i];
}

void bitset_and (bitset_t *l, bitset_t *r)
{
  int i;

  if (l->sz != r->sz) 
    fatal_error ("Sizes must match!");

  for (i=0; i < l->sz; i++)
    l->x[i] &= r->x[i];
}

int bitset_andclear (bitset_t *b, bitset_t *c)
{
  int i;

  if (b->sz != c->sz) {
    fatal_error ("Sizes must match!");
  }

  for (i=0; i < b->sz; i++) {
    if (b->x[i] & c->x[i]) return 0;
  }
  return 1;
}


int bitset_equal (bitset_t *l, bitset_t *r)
{
  int i;

  if (l->sz != r->sz) 
    return 0;
  
  for (i=0; i < l->sz; i++)
    if (l->x[i] != r->x[i])
      return 0;

  return 1;
}

int bitset_subset (bitset_t *a, bitset_t *b)
{
  int i;
  
  if (a->sz != b->sz) return 0;

  for (i=0; i < a->sz; i++) {
    if ((a->x[i] & b->x[i]) != a->x[i])
      return 0;
  }
  return 1;
}

void bitset_xor (bitset_t *l, bitset_t *r)
{
  int i;

  if (l->sz != r->sz) 
    fatal_error ("Sizes must match!");

  for (i=0; i < l->sz; i++)
    l->x[i] ^= r->x[i];
}

int bitset_isclear (bitset_t *b)
{
  int i;

  for (i=0; i < b->sz; i++) {
    if (b->x[i] != 0) return 0;
  }
  return 1;
}

bitset_t *bitset_copy (bitset_t *b)
{
  bitset_t *r;
  int i;

  r = bitset_alloc (b->sz*32);

  for (i=0; i < b->sz; i++) {
    r->x[i] = b->x[i];
  }
  return r;
}

unsigned int bitset_size (bitset_t *b)
{
  return (unsigned)b->sz*32;
}
