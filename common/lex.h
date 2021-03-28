/*************************************************************************
 *
 *  Lexical analysis engine
 *
 *  Copyright (c) 1996, 1999, 2016, 2019 Rajit Manohar
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
#ifndef __LEX_H__
#define __LEX_H__

#include <stdio.h>

#define LEX_FLAGS_NOREAL     0x01 /* don't parse real numbers */
#define LEX_FLAGS_IDSLASH    0x02 /* treat "/" as an id character */
#define LEX_FLAGS_PARSELINE  0x04 /* parse cpp #line directives */
#define LEX_FLAGS_DIGITID    0x08 /* treat digits as part of ids */
#define LEX_FLAGS_NODOTS     0x10 /* don't accept "." as a token */
#define LEX_FLAGS_INTREALS   0x20 /* integerE-4 works as a real */
#define LEX_FLAGS_NSTCOMMENT 0x40 /* nested C-style comments */
#define LEX_FLAGS_HEXINT     0x80 /* hex consts: exclusive with
				     DIGITID */
#define LEX_FLAGS_BININT    0x100 /* binary consts: exclusive with
				     DIGITID */
#define LEX_FLAGS_ESCAPEID  0x200 /* \[chars][spc] is an ID */
#define LEX_FLAGS_PARENCOM  0x400 /* Allow (* *) comments */

#ifdef __cplusplus
extern "C" {
#endif

/*
  Lexical analysis structure.
  Stores all temporary information required for lexical analysis.
*/

typedef struct lex_position {
  char *ws, *tok, *save, *prev;
  int bufptr;
  int colno,lineno;
  int changed;
  char ch;
  int sym;
  unsigned long integer;
  double real;
  struct lex_position *next;
} lex_position_t;

typedef struct {

  char *filename;		/* file name */
  union {
    FILE *fp;			/* input file */
    char *string;		/* input string */
  } inp;

  char *buf;			/* input buffer, if necessary */
  int bufptr;			/* buffer pointer */
  int buflen;			/* buffer size */

  lex_position_t *pos;		/* position stack */

  unsigned int flags;		/* lexer flags */

  int lineno;			/* line number in input */
  int colno;			/* column number in input */
  unsigned int changed:1;	/* "1" if lineno was changed */
  unsigned int file:1;		/* "1" if input is a file */
  unsigned int cfile:1;		/* "1" if input is a compressed file */

  char ch;			/* next input character */
  int sym;			/* next input token */

  unsigned long integer;	/* integer */
  double real;			/* real number */

  char **tokens;		/* token array, sorted */
  int *tokenvals;		/* token array values */
  int ntokens;			/* number of tokens */
  int toksize;			/* size of array */

  char *whitespace;		/* whitespace preceding this token */
  int whitespace_loc;		/* where I am */
  int whitespace_len;		/* currently allocated whitespace length */
  char *tokprev;		/* previous token */
  char *token;			/* actual text that was the token */
  int token_loc;		/* where I am */
  int token_len;		/* currently allocated token length */

  int saving;			/* mode */
  char *saved;			/* saved string */
  int saved_loc;		/* saved location */
  int saved_len;

} LEX_T;


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                             FUNCTIONS 
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/*------------------------------------------------------------------------
  C and C++ style comments are skipped.
  All whitespace is skipped.

  Strings:
      "[^"]*"

  Identifiers:
      [a-zA-Z_][a-zA-Z_0-9]*

  Integers:
      [0-9]+

  Reals:
      [0-9]+.[0-9]* | [0-9]*.[0-9]+
------------------------------------------------------------------------*/

extern const int l_err;
extern const int l_eof;
extern const int l_integer;
extern const int l_real;
extern const int l_string;
extern const int l_id;

extern LEX_T *lex_file (FILE *fp);
   /*
     Used to start lexical analysis on file "fp" from the current location.
     Returns a LEX_T structure that should be used to refer to this file.
   */

extern LEX_T *lex_fopen (const char *name);
extern LEX_T *lex_zfopen (const char *name);
   /*
     Used to start lexical analysis on file "name".
     Returns a LEX_T structure that should be used to refer to this file.
   */

extern LEX_T *lex_string (char *s);
   /*
     Used to start lexical analysis on string "s". Returns a LEX_T structure 
     that should be used to refer to this string.
   */

extern LEX_T *lex_restring (LEX_T *, char *s);
   /*
     Used to restart lexical analysis on string "s". Returns a LEX_T structure 
     that should be used to refer to this string.
   */

extern int lex_addtoken (LEX_T *l, const char *s);
   /*
     Adds string "s" as a reserved word to the lexical analyzer.
   */

extern void lex_addtokenarray (LEX_T *l, const char **s, int *i);
   /*
     Adds strings from array s[], and assigns token values to corresponding
     elements of array i[]. The array s[] must be NULL terminated.
   */

extern void lex_deltoken (LEX_T *l, int tok);
   /*
     Deletes "tok" if it is the last token
   */

extern void lex_deltokens (LEX_T *l, int n);
   /*
     Deletes the last "n" tokens.
   */

extern int lex_istoken (LEX_T *l, const char *s);
   /*
     Returns 1 if s is a token that has been defined already, 0 otherwise
    */

#define lex_setflags(l,f)  do { (l)->flags = (f); } while(0)
   /*
     Set flags to "flags"
    */

#define lex_flags(l)  (l)->flags
   /*
     Value of the symbol recognizer flags
    */

extern int lex_getsym (LEX_T *l);
   /*
     Returns the next lexical token in the file/string associated with 
     the LEX_T structure "l".
   */

extern void lex_mustbe (LEX_T *l, int tok);
   /*
     Asserts "tok" to be the current symbol, and reads in the next
     symbol
   */

extern void lex_mustbe_keyw (LEX_T *l, const char *s);
   /*
     Asserts "s" to be the current keyword, and reads in the next symbol
   */

extern int lex_have (LEX_T *l, int tok);
   /*
     Returns "1" if the next token is "tok" and reads in the next symbol,
     otherwise returns "0"
   */

extern int lex_have_keyw (LEX_T *l, const char *s);
   /*
     Returns a "1" if the next keyword is "s" and reads in the next token,
     otherwise returns "0"
   */

extern int lex_is_keyw (LEX_T *l, const char *s);
   /*
     Returns a "1" if the next keyword is "s" otherwise returns "0"
   */

extern char *lex_errstring (LEX_T *l);
   /*
     Returns a string containing file, line, column information preformatted.
   */

extern void lex_free (LEX_T *);
   /*
     Frees any memory allocated for the LEX_T structure
   */

extern int lex_eof (LEX_T *l);
   /*
     Returns "1" if the end of the input stream was reached, "0" otherwise.
   */

#define lex_id(l) ((l)->token)
   /*
     Returns the last identifier in the input stream.
     Undefined if there has not been an identifier token received previously.
   */

#define lex_prev(l) ((l)->tokprev)
   /*
     Returns the last identifier in the input stream.
     Undefined if there has not been an identifier token received previously.
   */

#define lex_integer(l) ((signed)(l)->integer)
   /*
     Returns the integer in the input stream. Undefined if
     there has not been an integer in the input stream.
   */

#define lex_real(l)   ((l)->real)
   /*
     Returns the real number in the input stream. Undefined if
     there has not been a real number in the input stream.
   */

#define lex_tokenstring(l) ((l)->token)
   /*
     Returns the string that was parsed into the current token.
   */

#define lex_whitespace(l)  ((l)->whitespace)
   /*
     Returns the whitespace before the previously parsed token.
   */

#define lex_linenumber(l)  ((l)->lineno-(l)->changed)
   /*
     Returns the line number for the last token in the input stream.
   */

#define lex_colnumber(l)  ((l)->colno)

#define lex_sym(l)     ((l)->sym)
   /*
     Returns the current input symbol.
   */

extern const char *lex_tokenname (LEX_T *l, int tok);
   /*
     Return string corresponding to token name
    */

extern void lex_begin_save (LEX_T *l);
   /*
     Begin saved token
   */

extern void lex_end_save (LEX_T *l);
   /*
     End saving
    */

extern void lex_push_position (LEX_T *l);
   /*
      Save current lex position into the position stack
    */

extern void lex_pop_position (LEX_T *l);
   /*
      Pop the position stack
    */

extern void lex_set_position (LEX_T *l);
   /*
      Sets current position to the top of the position stack
    */

#define lex_saved_string(l)  ((l)->saved)
   /*
     Saved string
    */

extern void lex_skip (LEX_T *L, const char *str);
   /* 
     Skip characters that belong to "str" and make them into a token;
     useful for character-mode parsing.
    */

extern void lex_skipws (LEX_T *L, const char *str);
   /* 
     Skip characters that belong to "str" and make them into whitespace;
     useful for character-mode parsing.
    */

extern void lex_skipwhite (LEX_T *L);
   /*
     Skip whitespace; useful for character-mode parsing.
    */

extern void lex_mustbe_ckeyw (LEX_T *l, const char *str);
   /*
     Skip precisely "str" in character form and make it a token
    */

#ifdef __cplusplus
}
#endif

#endif

