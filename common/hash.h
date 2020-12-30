/*************************************************************************
 *
 *  Hash functions: from CACM, p. 679, (June 1990)
 *
 *  Copyright (c) 2003-2010, 2019 Rajit Manohar
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
#ifndef __HASH_H__
#define __HASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

typedef struct hash_bucket {
  char *key;
  union {
    void *v;
    int i;
    long l;
  };
  struct hash_bucket *next;
} hash_bucket_t;

struct Hashtable {
  int size;
  hash_bucket_t **head;
  int n;
};

typedef struct {
  hash_bucket_t *b;
  int i;
} hash_iter_t;

typedef struct ihash_bucket {
  unsigned long key;
  union {
    void *v;
    int i;
    long l;
  };
  struct ihash_bucket *next;
} ihash_bucket_t;

struct iHashtable {
  int size;
  ihash_bucket_t **head;
  int n;
};

typedef struct {
  ihash_bucket_t *b;
  int i;
} ihash_iter_t;
    
typedef struct chash_bucket {
  void *key;			/* key */
  union {
    void *v;
    int i;
    long l;
  };
  struct chash_bucket *next;
} chash_bucket_t;


  /*
    After creating a custom hash table, you must assign the function
    pointers 
       hash -- the hash function
       match -- the compare function (return 1 if keys are equal, 0 otherwise)
       dup   -- create a copy of the key
                    if keys do not need replication, make this the
                    identity function.
       free -- release key storage of a key replicated by the dup
                function
  */
struct cHashtable {
  int size;
  chash_bucket_t **head;
  int (*hash) (int sz, void *key);
  int (*match) (void *key1, void *key2);
  void *(*dup) (void *key);
  void (*free) (void *key);
  void (*print) (FILE *fp, void *key);
  int n;
};

typedef int (*CHASH_HASHFN) (int, void *);
typedef int (*CHASH_MATCHFN) (void *, void *);
typedef void * (*CHASH_DUPFN) (void *);
typedef void (*CHASH_FREEFN) (void *);
typedef void (*CHASH_PRINTFN) (FILE *, void *);

typedef struct {
  chash_bucket_t *b;
  int i;
} chash_iter_t;
    

/* You can access all elements in struct Hashtable *h as follows:

int i;
hash_bucket_t *b;

for (i = 0; i < h->size; i++)
  for (b = h->head[i]; b; b = b->next) {
    use(b);
  }	     

*/

#define phash_bucket_t ihash_bucket_t
#define pHashtable  iHashtable
#define phash_iter_t ihash_iter_t

struct Hashtable  *hash_new (int sz);
struct iHashtable  *ihash_new (int sz);
struct cHashtable  *chash_new (int sz);
#define phash_new ihash_new

hash_bucket_t *hash_add (struct Hashtable *, const char *key);
ihash_bucket_t *ihash_add (struct iHashtable *, long key);
chash_bucket_t *chash_add (struct cHashtable *, void *key);
#define phash_add(a,b) ihash_add((a),(unsigned long)(b))

hash_bucket_t *hash_lookup (struct Hashtable *, const char *key);
ihash_bucket_t *ihash_lookup (struct iHashtable *, long key);
chash_bucket_t *chash_lookup (struct cHashtable *, void *key);
#define phash_lookup(a,b) ihash_lookup((a),(unsigned long)(b))

void hash_delete (struct Hashtable *, const char *key);
void ihash_delete (struct iHashtable *, long key);
void chash_delete (struct cHashtable *, void *key);
#define phash_delete(a,b) ihash_delete((a),(unsigned long)(b))

void hash_clear (struct Hashtable *); /* clears the hash table */
void ihash_clear (struct iHashtable *);
void chash_clear (struct cHashtable *);
#define phash_clear ihash_clear  

void hash_free (struct Hashtable *);
void ihash_free (struct iHashtable *);
void chash_free (struct cHashtable *);
#define phash_free ihash_free

void hash_iter_init (struct Hashtable *, hash_iter_t *i);
void ihash_iter_init (struct iHashtable *, ihash_iter_t *i);
void chash_iter_init (struct cHashtable *, chash_iter_t *i);
#define phash_iter_init ihash_iter_init

hash_bucket_t *hash_iter_next (struct Hashtable *, hash_iter_t *i);
ihash_bucket_t *ihash_iter_next (struct iHashtable *, ihash_iter_t *i);
chash_bucket_t *chash_iter_next (struct cHashtable *, chash_iter_t *i);
#define phash_iter_next ihash_iter_next

  /* you can use this to build custom hash functions

     size = hash table size (must be a power of 2)
     k    = ptr to an array of bytes
     len  = size of array

     iscont = 1 if you want to continue updating the hash 
     iscont = 0 if this is the first time it is being called
     prev   = previous return value if you want to chain calls to this
     function
   */
int hash_function (int size, const char *key);
int hash_function_continue (unsigned int size, 
			    const unsigned char *k, int len, unsigned int prev,
			    int iscont);

#ifdef __cplusplus
}
#endif

#endif
