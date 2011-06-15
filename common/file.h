/*************************************************************************
 *
 *  Copyright (c) 2009 Rajit Manohar
 *  All Rights Reserved
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

typedef struct _file_ LFILE;

LFILE *file_open (const char *);
void file_push (LFILE *, const char *);

int file_addtoken (LFILE *, const char *);
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
int file_sym (LFILE *);
char *file_prev (LFILE *);
int file_integer (LFILE *);
double file_real (LFILE *);

#ifdef __cplusplus
}
#endif

#endif /* __FILE_H__ */
