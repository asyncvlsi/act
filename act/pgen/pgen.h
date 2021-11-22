/*************************************************************************
 *
 *  Parser generator
 *
 *  Copyright (c) 2003-2011, 2018, 2019 Rajit Manohar
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
#ifndef __PGEN_H__
#define __PGEN_H__

#include <stdio.h>

#include "act/common/array.h"
#include "act/common/pp.h"
#include "act/common/bitset.h"
#include "act/common/misc.h"

extern char *prefix;
extern char *path_prefix;

typedef enum token_tag {
  T_L_EXPR,
  T_L_IEXPR,
  T_L_BEXPR,
  T_L_REXPR,
  T_L_ID,
  T_L_STRING,
  T_L_FLOAT,
  T_L_INT,
  T_KEYW,
  T_TOKEN,
  T_LHS, 
  T_OPT,
  T_LIST,
  T_LIST_SPECIAL,
  T_EXTERN
} token_t;


#define HAS_DATA(t)  ((t).type != T_KEYW && (t).type != T_TOKEN)

struct token_type;
typedef struct token_type token_type_t;

/* list of tokens (implemented as an array) */
typedef struct token_list {
  bitset_t *s;
  A_DECL (token_type_t, a);
} token_list_t;

struct body_info {
  char *s;
  char *file;
  int line;
};

/* single token */
struct token_type {
  token_t type;
  int int_flag;			/* set to suppress reals next! */
  int end_gt;			/* something ends in ">" for
				   expression parser */

  struct body_info *opt_next;

  void *toks; /* token_list_t for [ .. ] and { ... }**, and
		 string for STRING or LHS
		 string for EXTERN as well

		 if type is T_TOKEN, then this is actually an index
		 into the token table TT[...]

		 if this is of type T_LHS, then it is a pointer to the
		 bnf_item_t corresponding to the non-terminal symbol
		 (pointer into BNF[...])
	      */
};

#define IS_RAW_TOKEN(t) ((t).type == T_L_ID || (t).type == T_L_STRING || (t).type == T_L_FLOAT || (t).type == T_L_INT || (t).type == T_KEYW || (t).type == T_TOKEN)

/*
  lhs = lhs of a production;
  a = array of token lists, each one corresponding to a single rhs
*/
typedef struct bnf_item {
  char *lhs;			/* string corresponding to the left
				   hand side token name */

  char *lhs_ret;		/* return value for the LHS */

  char *lhs_ret_base;		/* return value string in a format
				   that can be appended to as an
				   identifier---see _p  */

  int lhs_ret_p; /* 0 = not a pointer, 1 = *, 2 = @ */

  int tail_recursive;		/* 0 if not tail recursive
				   1 if
				      lhs : S lhs | S

				   2 if 
				      lhs : lhs S | S
				 */

  int is_exclusive;		/* 1 if all options are mutually
				   exclusive by the first token, 0
				   otherwise */

  A_DECL (int, tok_opts);	/* a list of possible starter tokens
				   for this particular BNF item */

  A_DECL (token_list_t, a);

  token_type_t *raw_tokens[2];	/* first two raw tokens for this to be
				   a candidate */

} bnf_item_t;


/* array of bnf items */
E_A_DECL(bnf_item_t, BNF);
E_A_DECL(char *, EXTERN_P);

/* tokens */
E_A_DECL(char *, TT);

pp_t *std_open (char *s);
void std_close (pp_t *);

#define BEGIN_INDENT				\
   do {						\
     pp_nl;					\
     pp_puts (pp, "   ");			\
     pp_setb (pp);				\
   } while (0)

#define END_INDENT				\
   do {						\
     pp_endb (pp); pp_nl;			\
   } while (0)


#define pp_nl pp_forced (pp,0)

extern int found_expr;
extern int cyclone_code;

E_A_DECL(char *, WALK);
E_A_DECL(char *, cookie_type);
E_A_DECL(char *, return_type);
extern int gen_parse;
E_A_DECL(char *, GWALK);

void print_munged_string (pp_t *pp, char *s, char *file, int line);
char *user_ret_id (int id);
char *user_ret (bnf_item_t *b);
void print_header_prolog (pp_t *pp);
char *production_to_ret_type (token_type_t *t);
char *wrapper_name (int i);
char *special_wrapper_name (token_type_t *t);
char *special_user_ret_id (int i);
char *tok_type_to_parser_type (token_type_t *t);


void print_walker_prolog (pp_t *);
void print_walker_main (pp_t *);
void print_walker_recursive (pp_t *, pp_t *);
void print_walker_apply_fns (pp_t *);
void print_walker_local_apply_fns (pp_t *);
void emit_walker (void);

#define Tok_ID_offset 0
#define Tok_STRING_offset 1
#define Tok_FLOAT_offset 2
#define Tok_INT_offset 3
#define Tok_OptList_offset 4
#define Tok_SeqList_offset 5
#define Tok_EXTERN_offset 6
#define Tok_expr_offset 7

#endif /* __PGEN_H__ */
