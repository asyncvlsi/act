/*************************************************************************
 *
 *  Compressed I/O library
 *
 *  Copyright (c) 1994, 1995, 2019 Rajit Manohar
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

FILE *c_fopen_r (const char *s);
FILE *c_fopen_w (const char *s);
void c_fclose (FILE *fp);

int c_fwrite (char *buf, int sz, int n, FILE *fp);
int c_fread (char *buf, int sz, int n, FILE *fp);
char *c_fgets (char *buf, int len, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* __LZW_H__ */
