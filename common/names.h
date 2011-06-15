/*************************************************************************
 *
 *  Copyright (c) 2006 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __NAMES_H__
#define __NAMES_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 
   Names file database. Keeps the names on disk in tables, caching
   names as and when necessary.

   Disk format:
     <file>_str.dat  <- string table
                        * contains null-terminated strings
                        * a null string indicates `end of record'

     <file>_idx.dat  <- index table
*/

#define IDX_TYPE unsigned long
#define IDX_SIZE 4

#define NAMES_WRITE 1
#define NAMES_READ  2

typedef struct {
  char *string_tab;		/* string table file name */
  FILE *sfp;			/* file pointer */

  char *idx_tab;		/* index table file name */
  FILE *ifp;			/* file pointer */
  
  char *idx_revtab;		/* reverse index table */
  FILE *rfp;

  char *alias_tab;		/* alias table */
  FILE *afp;

  IDX_TYPE fpos;		/* position in string table */

  unsigned int mode;		/* read or write */

  IDX_TYPE count;
  IDX_TYPE hsize;		/* size of the disk inverse hash
				   table! */

  int reverse_endian;		/* 1 if endianness should be reversed
				   on reading from disk */

  IDX_TYPE unique_names;	/* actual # of strings */

  unsigned int update_hash;	/* 1 if hash has been updated! */

} NAMES_T;

/*
  Return # that corresponds to the string given
    -1 = not found
*/
IDX_TYPE names_str2name (NAMES_T *N, char *s);

IDX_TYPE names_parent (NAMES_T *N, IDX_TYPE num);

/*
  Return string corresponding to number, NULL if not found
  NOTE: this string will be DESTROYED if some other names_ function is
  called!
*/
char *names_num2name (NAMES_T *N, IDX_TYPE num);


NAMES_T *names_open (char *file);

/*
  1. Create names file.
  2. (names_newname; names_addalias*)*
  3. names_close ()
*/
NAMES_T *names_create (char *file, IDX_TYPE max_names);
IDX_TYPE names_newname (NAMES_T *, char *str);
void names_addalias (NAMES_T *, IDX_TYPE idx1, IDX_TYPE idx2);
void names_close (NAMES_T *);

#ifdef __cplusplus
}
#endif

#endif /* __NAMES_H__ */
