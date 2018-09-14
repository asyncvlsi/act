/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

#include <stdio.h>
#include <string.h>

#include "hier.h"
#include "misc.h"


static int T[] =
{  1, 87, 49, 12,176,178,102,166,121,193,  6, 84,249,230, 44,163,
  14,197,213,181,161, 85,218, 80, 64,239, 24,226,236,142, 38,200,
 110,177,104,103,141,253,255, 50, 77,101, 81, 18, 45, 96, 31,222,
  25,107,190, 70, 86,237,240, 34, 72,242, 20,214,244,227,149,235,
  97,234, 57, 22, 60,250, 82,175,208,  5,127,199,111, 62,135,248,
 174,169,211, 58, 66,154,106,195,245,171, 17,187,182,179,  0,243,
 132, 56,148, 75,128,133,158,100,130,126, 91, 13,153,246,216,219,
 119, 68,223, 78, 83, 88,201, 99,122, 11, 92, 32,136,114, 52, 10,
 138, 30, 48,183,156, 35, 61, 26,143, 74,251, 94,129,162, 63,152,
 170,  7,115,167,241,206,  3,150, 55, 59,151,220, 90, 53, 23,131,
 125,173, 15,238, 79, 95, 89, 16,105,137,225,224,217,160, 37,123,
 118, 73,  2,157, 46,116,  9,145,134,228,207,212,202,215, 69,229,
  27,188, 67,124,168,252, 42,  4, 29,108, 21,247, 19,205, 39,203,
 233, 40,186,147,198,192,155, 33,164,191, 98,204,165,180,117, 76,
 140, 36,210,172, 41, 54,159,  8,185,232,113,196,231, 47,146,120,
  51, 65, 28,144,254,221, 93,189,194,139,112, 43, 71,109,184,209};



static int
hash( struct hier_table *h, const char *k)
{
   register unsigned int sum=0;
   int size=h->size;
#if 0
   while( *k) sum = sum<<1 ^ *k++;
#endif
/*
 * 8, 16, 24, or 32 bit hashing function--- from CACM June 1990, p.679
 */
   if ( *k) {
      const char *p;
      char c;
      if ( size <= (1<<8)) {
	for( p=k; (c=*p++); ) sum = T[ sum ^ c];
      } else if ( size <= (1<<16)) {
	 unsigned int sum1;
	 sum = T[ *k];
	 sum1 = T[ 0xff & ( 1+ *k)];
	 for( p=k+1; (c=*p++); ) {
	    sum = T[ sum ^ c];
	    sum1 = T[ sum1 ^ c];
	 }
	 sum |= sum1 << 8;
      } else if ( size <= (1<<24)) {
	 unsigned int sum1, sum2;
	 sum = T[ *k];
	 sum1 = T[ 0xff & ( 1+ *k)];
	 sum2 = T[ 0xff & ( 2+ *k)];
	 for( p=k+1; (c=*p++); ) {
	    sum = T[ sum ^ c];
	    sum1 = T[ sum1 ^ c];
	    sum2 = T[ sum2 ^ c];
	 }
	 sum |= (sum1 << 8) | (sum2 << 16);
      } else {
	 unsigned int sum1, sum2, sum3;
	 sum = T[ *k];
	 sum1 = T[ 0xff & ( 1+ *k)];
	 sum2 = T[ 0xff & ( 2+ *k)];
	 sum3 = T[ 0xff & ( 3+ *k)];
	 for( p=k+1; (c=*p++); ) {
	    sum = T[ sum ^ c];
	    sum1 = T[ sum1 ^ c];
	    sum2 = T[ sum2 ^ c];
	    sum3 = T[ sum3 ^ c];
	 }
	 sum |= (sum1 << 8) | (sum2 << 16) | (sum3 << 24);
      }
   }
/* return sum & (size-1);*/
   return sum % size;
}

static void
expandHashtable(struct hier_table *ht)
{
   
   struct hier_table Ht;
   Ht.size = ( ht->size <<2);
   MALLOC (Ht.head, struct hash_cell *, Ht.size);
   Ht.nelements = ht->nelements;
   {
      int i;
      for( i=0; i<Ht.size; i++) {
	 Ht.head[i] = NULL;
      }
      for( i=0; i<ht->size; i++) {
	 struct hash_cell *h=ht->head[i], *hnext;
	 for( ; h; h=hnext) {
	    int bucket;
	    hnext = h->next;
	    bucket = hash( &Ht, KEY(h));
	    h->next = Ht.head[bucket];
	    Ht.head[bucket] = h;
	 }
      }
   }
   free( (char*) ht->head);
   *ht = Ht;
}

struct hier_table*
hier_create( int s)
{
   struct hier_table* h;

   NEW (h, struct hier_table);

   for( h->size=1; s > h->size; h->size <<= 1) ;

   MALLOC (h->head, struct hash_cell *, h->size);
   h->nelements = 0;

   for( s=0; s< h->size; s++) h->head[s] = (struct hash_cell*) 0;

   return h;

}

struct hash_cell*
hier_add (struct hier_table* h,
	  const char* k,
	  int isinput)
{

   int bucket;
   struct hash_cell* p;

   if ( h->nelements > ( h->size << 2)) {
      expandHashtable( h);
   }

   bucket = hash( h, k);

   for( p = h->head[bucket]; p; p=p->next) {
      if (0 == strcmp( KEY(p), k)) {
	 (void) fprintf( stderr,
			 "hier_add: Key '%s' already entered.\n", k);
	 exit( 1);
      }
   }

   NEW (p, struct hash_cell);
   MALLOC (p->key, char, strlen(k)+1);
   (void) strcpy( p->key, k);

   HIER_INIT_FLAGS (p);

   if (isinput)
     MK_INPUT (p);

   p->root = p;
   p->next = h->head[bucket];

   h->head[bucket] = p;

   h->nelements++;

   return p;

}

struct hash_cell*
hier_find(struct hier_table* h,
	  const char* k)
{
   int bucket;
   struct hash_cell* p;

   bucket = hash( h, k);

   for( p = h->head[bucket]; p; p=p->next) {
      if (0 == strcmp( KEY(p), k)) return p;
   }
   return (struct hash_cell*) 0;
}

void
hier_free(struct hier_table* h,
	  const char* k)
{
   int bucket;
   struct hash_cell *p, *pnext, **prev;

   bucket = hash( h, k);

   prev = &h->head[bucket];
   for( p = *prev; p; p=pnext) {
      pnext = p->next;
      if (0 == strcmp( KEY(p), k)) {
	 *prev = p->next;
	 free( (char *) KEY(p));
	 free( (char *) p);
	 h->nelements--;
	 return;
      } else {
	 prev = &p->next;
      }
   }
   (void) fprintf( stderr,
		   "disassociateHashtable: Key '%s' not found.\n", k);
   exit( 1);
}

void
hier_freeall(struct hier_table* h)
{

   while( h->size--) {
      while( h->head[h->size]) {
	 struct hash_cell* p;
	 p = h->head[h->size];
	 h->head[h->size] = h->head[h->size]->next;

	 free( (char*) KEY(p));
	 free( (char*) p);
      }
   }

   free( (char*) h->head);
   free( (char*) h);

}



/*------------------------------------------------------------------------*/


char *
hier_subcell_node (VAR_T *V, char *node, var_t **hcell, char sep)
{
  char *s;
  var_t *v;

  for (s = node; *s; s++) {
    if (*s == sep) { 
      *s = '\0';
      v = var_locate (V, node);
      if (v && v->hcell) {
	*s = sep;
	if (hcell)
	  *hcell = v;
	return s+1;
      }
      *s = sep;
    }
  }
  return NULL;
}


/*------------------------------------------------------------------------*/


int
hier_notinput_subcell_node (VAR_T *V, char *node, char sep)
{
  char *s;
  var_t *v;
  struct hash_cell *hc;

  for (s = node; *s; s++) {
    if (*s == sep) {
      *s = '\0';
      v = var_locate (V, node);
      if (v && v->hcell) {
	*s = sep;
	hc = hier_find ((struct hier_table*)v->hc, s+1);
	if (!hc) return 1;
	if (IS_INPUT(hc->root)) return 0;
	return 1;
      }
      *s = sep;
    }
  }
  return 0;
}
