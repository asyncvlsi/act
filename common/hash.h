/*************************************************************************
 *
 *  Copyright (c) 2008 Rajit Manohar
 *  All Rights Reserved
 *
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
  void *v;
  struct hash_bucket *next;
} hash_bucket_t;

struct Hashtable {
  int size;
  hash_bucket_t **head;
  int n;
}; 

typedef struct ihash_bucket {
  unsigned long key;
  void *v;
  struct ihash_bucket *next;
} ihash_bucket_t;

struct iHashtable {
  int size;
  ihash_bucket_t **head;
  int n;
};

typedef struct chash_bucket {
  void *key;			/* key */
  void *v;
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

/* You can access all elements in struct Hashtable *h as follows:

int i;
hash_bucket_t *b;

for (i = 0; i < h->size; i++)
  for (b = h->head[i]; b; b = b->next) {
    use(b);
  }	     

*/

struct Hashtable  *hash_new (int sz);
int hash_function (int size, const char *key);
struct iHashtable  *ihash_new (int sz);

struct cHashtable  *chash_new (int sz);

hash_bucket_t *hash_add (struct Hashtable *, const char *key);
ihash_bucket_t *ihash_add (struct iHashtable *, long key);
chash_bucket_t *chash_add (struct cHashtable *, void *key);

hash_bucket_t *hash_lookup (struct Hashtable *, const char *key);
ihash_bucket_t *ihash_lookup (struct iHashtable *, long key);
chash_bucket_t *chash_lookup (struct cHashtable *, void *key);

void hash_delete (struct Hashtable *, const char *key);
void ihash_delete (struct iHashtable *, long key);
void chash_delete (struct cHashtable *, void *key);

void hash_clear (struct Hashtable *); /* clears the hash table */
void ihash_clear (struct iHashtable *);
void chash_clear (struct cHashtable *);

void hash_free (struct Hashtable *);
void ihash_free (struct iHashtable *);
void chash_free (struct cHashtable *);

  /* you can use this to build custom hash functions

     size = hash table size (must be a power of 2)
     k    = ptr to an array of bytes
     len  = size of array

     iscont = 1 if you want to continue updating the hash 
     iscont = 0 if this is the first time it is being called
     prev   = previous return value if you want to chain calls to this
     function
   */
int hash_function_continue (unsigned int size, 
			    const unsigned char *k, int len, unsigned int prev,
			    int iscont);

#ifdef __cplusplus
}
#endif

#endif
