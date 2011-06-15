/****************************************************************************
 *
 *  Copyright (c) 1995 Rajit Manohar
 *  All Rights Reserved
 *
 *    Compressed I/O
 *
 ***************************************************************************/
#ifndef __LZW_H__
#define __LZW_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * #defines that you might want to modify.
 *
 *  MAX_OPEN_FILES:
 *       The maximum number of files that can be open at one time.
 *
 *****************************************************************************/
#define MAX_OPEN_FILES 256	/* maximum number of open files */

/**** Compressed I/O Routine Declarations ****/

FILE *c_fopen_r (char *s);
FILE *c_fopen_w (char *s);
void c_fclose (FILE *fp);

int c_fwrite (char *buf, int sz, int n, FILE *fp);
int c_fread (char *buf, int sz, int n, FILE *fp);
char *c_fgets (char *buf, int len, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* __LZW_H__ */
