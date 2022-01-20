/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __PARSE_H__
#define __PARSE_H__

#include <stdio.h>
#include "act/common/lex.h"
#include "act/common/pp.h"
#include "act/common/bool.h"
#include "var.h"

enum {
  E_AND, E_OR, E_NOT, E_TRUE, E_FALSE, E_ID
};

typedef struct expr {
  int type;
  struct expr *l, *r;
} expr_t;                       /* expression */

extern int parse_prs_input (LEX_T *, VAR_T *,int);
extern void print_prs_expr (pp_t *, VAR_T *, int);

extern void print_expr (pp_t *, expr_t *);
extern bool_t *expr_to_bool (BOOL_T *, expr_t *);

extern void create_prs_connection (char *s1, char *s2);

#endif /* __PARSE_H__ */
