/*************************************************************************
 *
 *  Info: package for manipulating boolean functions (BDDs)
 *
 *  Copyright (c) 1995 Rajit Manohar
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
#ifndef __BOOL_H__
#define __BOOL_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long bool_var_t;

typedef struct bool_t {
  unsigned int ref:14;		/* refcount */
  unsigned char mark:2;		/* mark for Schorr-Waite */
  bool_var_t id;		/* variable, if non-leaf, value if leaf. */
  struct bool_t *l, *r;		/* left, right links */
  struct bool_t *next;		/* next ptr, for hashtable */
} bool_t;

#ifdef BOOL_INTERNAL_H

#define HIBIT_OFFSET (sizeof(bool_var_t)*8-1)

#define BOOL_MAXVAR  (1UL<<HIBIT_OFFSET)

#define SET_HIBIT(v) (v |= 1UL << HIBIT_OFFSET)

#define REFMAX  ((1<<14)-1)
 /* max value of "ref" */

#define ISLEAF(b)      (((b)->id & (1UL << HIBIT_OFFSET)) ? 1 : 0)
 /* true if "b" is a leaf */

#define ASSIGN_LEAF(b,n) ((b)->id = ((b)->id & ~(1UL<<HIBIT_OFFSET))|(((unsigned long)n) << HIBIT_OFFSET))
 /* assign to the "leaf" bit. */

#define MARK(b)        ((b)->mark)
 /* get value of "mark" */

#define INC_MARK(b)   ((b)->mark++)
 /* increment value of "mark" */

#define CLEAR_MARK(b)  ((b)->mark = 0)
 /* decrement value of "mark" */

#define REF(b)         ((b)->ref)
 /* value of the "ref" field */

#define INC_REF(b)     do { if ((b)->ref < REFMAX) (b)->ref++; }while(0)
 /* increment value of "ref" field */

#define DEC_REF(b)     do { if ((b)->ref < REFMAX) (b)->ref--; }while(0)
 /* decrement value of "ref" field */

#define CLEAR(b) ((b)->ref = 0, (b)->mark = 0, (b)->id = 0)
 /* clear all fields */

#define VAR_BLOCK 32
 /* number of variables added at a time */

#define HASH_BLOCK 32
 /* initial size of the hastable for each variable */

enum triple_operations_t {
  BOOL_AND = 0, BOOL_OR = 1, BOOL_XOR = 2, BOOL_IMPLIES = 3, BOOL_NOT = 4,
  BOOL_MKTRUE = 5, BOOL_MKFALSE = 6, BOOL_NEGATE_VAR = 7
  } ;

#endif

#define BOOL_MAXOP 8

typedef struct {
  unsigned long nelements;	/* number of elements in the hashtable */
  unsigned long nbuckets;	/* number of buckets */
  bool_t **bucket;		/* the buckets */
} pairhash_t;

struct triple {			/* triple for hashing */
  bool_t *v1, *v2, *v3;
  struct triple *next;
};

typedef struct {
  unsigned long nbuckets;
  unsigned long nelements;
  struct triple **bucket;
} triplehash_t;			/* triple hash table */

struct rootlist {
  bool_t *b;
  struct rootlist *next;
}; 

typedef struct {
  unsigned long nvar;		/* number of variables */
  unsigned long totvar;		/* total number of variables */
  pairhash_t **H;		/* hashtable for each variable */
  triplehash_t *TH[BOOL_MAXOP];	/* hashtable for (a,b)->c values */
  bool_t *btrue, *bfalse;
  struct rootlist *roots;	/* roots */
} BOOL_T;

typedef struct {
  bool_var_t *v;
  unsigned long n;
} bool_list_t;			/* sorted list of variables */

extern BOOL_T *bool_init (void);

extern bool_t *bool_true (BOOL_T *);
extern bool_t *bool_false (BOOL_T *);
extern bool_t *bool_newvar (BOOL_T *);
extern bool_t *bool_var (BOOL_T *, bool_var_t);

extern bool_t *bool_and (BOOL_T *, bool_t *, bool_t *);
extern bool_t *bool_or (BOOL_T *, bool_t *, bool_t *);
extern bool_t *bool_xor (BOOL_T *, bool_t *, bool_t *);
extern bool_t *bool_not (BOOL_T *, bool_t *);
extern bool_t *bool_copy  (BOOL_T *, bool_t *);
extern bool_t *bool_implies (BOOL_T *, bool_t *, bool_t *);

extern bool_t *bool_maketrue (BOOL_T *, bool_t *, bool_t *);
extern bool_t *bool_makefalse (BOOL_T *, bool_t *, bool_t *);

extern bool_t *bool_negate_var (BOOL_T *, bool_t *, bool_t *);
extern bool_t *bool_substitute (BOOL_T *, bool_list_t *, bool_list_t *, 
				bool_t *);
extern bool_t *bool_exists (BOOL_T *, bool_list_t *, bool_t *);
extern bool_t *bool_exists2 (BOOL_T *, bool_list_t *, bool_t *, bool_t *);
extern bool_list_t *bool_qlist (BOOL_T *, unsigned long, bool_var_t *);

extern void bool_free (BOOL_T *, bool_t *);
extern void bool_gc (BOOL_T *);

extern void bool_print (bool_t *);
extern void bool_info (BOOL_T *B);

extern int bool_isleaf (bool_t *b);

#define bool_topvar(b) ((b)->id)

#ifdef __cplusplus
}
#endif

#endif
