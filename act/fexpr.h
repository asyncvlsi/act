#ifndef __ACT_FEXPR_H__
#define __ACT_FEXPR_H__

#include <stdio.h>
#include <act/lang.h>
#include <common/file.h>
#include "treetypes.h"
#include "expr.h"

extern Expr *(*expr_parse_basecase_extra)(LFILE *l);

/**
   @file fexpr.h

   @brief Functions to support the faster expression parser used by
   CHP/dataflow bodies.
*/

#ifdef __cplusplus
extern "C" {
#endif

  /**
     External parser for fast expression parsing: initialization
     function
  */
void act_init_fexpr (LFILE *);

  /**
     External parser for fast expression parsing: return 1 if next
     symbol could be the start of an expression
  */
int act_is_a_fexpr (LFILE *);

  /**
     External parser for fast expression parsing: free returned data
  */
void act_free_a_fexpr (void *);

  /**
     External parser for fast expression parsing: parse an expression,
     returning NULL on error, or data structure otherwise (returns an
     Expr *)
  */
void *act_parse_a_fexpr (LFILE *);

  /**
     External parser for fast expression parsing: walk the parser data
     structure, returning the walked fexpr type (which is also an Expr
     *). This just calls the act_walk_X_expr() function, since it
     returns the same data structure as a standard expression.
  */
void *act_walk_X_fexpr (ActTree *, void *);

#ifdef __cplusplus
}
#endif


#endif /* __ACT_FEXPR_H__ */
