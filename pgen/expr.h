/*************************************************************************
 *
 *  Copyright (c) 1999-2010 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __EXPR_H__
#define __EXPR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "file.h"
#include "pp.h"
#include "id.h"

#define  E_AND 	 0
#define  E_OR 	 1
#define  E_NOT 	 2
#define  E_PLUS  3
#define  E_MINUS 4
#define  E_MULT	 5
#define  E_DIV	 6
#define  E_MOD	 7
#define  E_LSL	 8
#define  E_LSR	 9
#define  E_ASR	 10
#define  E_UMINUS 11
#define  E_INT	 12  /* not a real token */
#define  E_VAR	 13  /* not a real token */
#define  E_QUERY 14
#define  E_LPAR	 15
#define  E_RPAR	 16
#define  E_XOR	 17
#define  E_LT	 18
#define  E_GT    19
#define  E_LE    20
#define  E_GE	 21
#define  E_EQ    22
#define  E_NE    23
#define  E_TRUE  24  /* not a real token */
#define  E_FALSE 25  /* not a real token */
#define  E_COLON 26
#define  E_PROBE 27
#define  E_COMMA 28
#define  E_CONCAT 29
#define  E_BITFIELD 30
#define  E_COMPLEMENT 31 /* bitwise complement */
#define  E_REAL 32  /* not a real token */
#define  E_END   33

#define E_NUMBER 34 /* # of E_xxx things */

#define E_FUNCTION 100

typedef struct expr {
  int type;
  union {
    struct {
      struct expr *l, *r;
    } e;
    struct {
      char *s;
      struct expr *r;
    } fn;
    double f;
    unsigned int v;
  } u;
} Expr;

/* Function calls are represented as:
   
   type = E_FUNCTION

   l = function name
   
   E_FUNCTION
     /      |
    name    E_LT 
           /  \
         arg1  E_LT
              /  \
            arg2  ...


   A ? B : C is represented as:
 
        E_QUERY
       /      \
       A      E_COLON
               /  \
              B    C


   type = E_CONCAT

       E_CONCAT
        /     |
      expr   E_CONCAT
              /    |
            expr   ...


   type = E_BITFIELD

       E_BITFIELD
       /       |
     var     E_BITFIELD
               /    |
              lo    hi
*/


void expr_settoken (int name, int value);
  /* 
     Set the lexical id for token name (from the E_xxx list above) to
     value. "value" is the integer that lex_addtoken() returns.
     To delete expression operators, simply set the value of a token
     to -1.
  */

Expr *expr_parse_any (LFILE *l);
  /*
    Parse any expression
  */

Expr *expr_parse_int (LFILE *l);
  /*
    Parse an integer expression and returns its parse tree.
  */

Expr *expr_parse_bool (LFILE *l);
 /*
   Parse a Boolean expression and return its parse tree.
 */

Expr *expr_parse_real (LFILE *l);
 /*
   Parse a real expression and return its parse tree.
 */

void expr_free (Expr *e);
 /*
   Free expression tree
 */

int expr_init (LFILE *l);
 /*
   Initialize all tokens with standard symbols, and add tokens to the
   lexer. Returns the number of tokens added. This number might be 
   less than the # of tokens used by the expression evaluation package
   because the lexer may already know about some of the tokens.
 */

void expr_clear (void);
  /*
    Clears expressions from expression parser
  */

void expr_endgtmode (int m);

extern pId *(*expr_parse_id)(LFILE *l);
  /* The function must do the following:
       - if there is a valid identifier, then create a data structure
         for it and return a pointer to it.
       - if there isn't, don't do anything, return a NULL pointer. The
         lexer state must be unchanged in this case.
  */
extern void (*expr_free_id) (void *);
  /* Free an allocated id structure */

extern void (*expr_print_id) (pp_t *, void *);
  /* Print an allocated id structure */

extern void (*expr_print_probe) (pp_t *, void *);
  /* Print #chan */


extern void expr_print (pp_t *, Expr *);
/*  Print expression */

extern const char *expr_operator_name (int type);
/* return string corresponding to the operator token */

#ifdef __cplusplus
}
#endif

#endif /* __EXPR_H__ */
