/*************************************************************************
 *
 *  Extension of lexer library, but with multiple files.
 * 
 *  Copyright (c) 2009, 2019 Rajit Manohar
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
#ifndef __FILE_H__
#define __FILE_H__

#include <stdio.h>
#include "lex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define f_err l_err
#define f_eof l_eof
#define f_integer l_integer
#define f_real l_real
#define f_string l_string
#define f_id l_id

#define FILE_FLAGS_NOREAL      LEX_FLAGS_NOREAL     
#define FILE_FLAGS_IDSLASH     LEX_FLAGS_IDSLASH    
#define FILE_FLAGS_PARSELINE   LEX_FLAGS_PARSELINE  
#define FILE_FLAGS_DIGITID     LEX_FLAGS_DIGITID    
#define FILE_FLAGS_NODOTS      LEX_FLAGS_NODOTS     
#define FILE_FLAGS_INTREALS    LEX_FLAGS_INTREALS   
#define FILE_FLAGS_NSTCOMMENT  LEX_FLAGS_NSTCOMMENT 
#define FILE_FLAGS_HEXINT      LEX_FLAGS_HEXINT     
#define FILE_FLAGS_BININT      LEX_FLAGS_BININT     
#define FILE_FLAGS_ESCAPEID    LEX_FLAGS_ESCAPEID

typedef struct _file_ LFILE;

LFILE *file_open (const char *);
void file_push (LFILE *, const char *);

int file_addtoken (LFILE *, const char *);
void file_deltoken (LFILE *, const char *);
int file_istoken (LFILE *, const char *);

unsigned int file_flags (LFILE *);
void file_setflags (LFILE *, unsigned int);

int file_have (LFILE *, int);
int file_have_keyw (LFILE *, const char *);
int file_is_keyw (LFILE *, const char *); 
void file_mustbe (LFILE *, int);

int file_getsym (LFILE *);

void file_push_position (LFILE *);
void file_pop_position (LFILE *);
void file_set_position (LFILE *);

void file_get_position (LFILE *, int *line, int *col, char **fname);

void file_set_error (LFILE *, const char *);
char *file_errstring (LFILE *);
int file_eof (LFILE *);

char *file_tokenstring (LFILE *);
const char *file_tokenname (LFILE *l, int tok);
int file_sym (LFILE *);
char *file_prev (LFILE *);
int file_integer (LFILE *);
double file_real (LFILE *);

#ifdef __cplusplus
}
#endif

#endif /* __FILE_H__ */
