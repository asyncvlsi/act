/*************************************************************************
 *
 *  Copyright (c) 2025 Rajit Manohar
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
#ifndef __ACT_EXPR_API_H__
#define __ACT_EXPR_API_H__

#include <stdio.h>
#include <act/expr.h>


class ActNamespace;
class Scope;
class ActId;
class Data;

/*------------------------------------------------------------------------
 *
 *  Helper functions for expression manipulation
 *
 *------------------------------------------------------------------------
 */

/**
 * Check if two expressions are equal. This is a structural test.
 *
 * @param a is the first expression
 * @param b is the second expression
 * @return 1 if they are equal, 0 otherwise
 */
int expr_equal (const Expr *a, const Expr *b);

/**
 * Print an expression to a file. This is used to print parameter
 * expressions (e.g. in the core ACT language), and not chp/dataflow
 * expressions.
 *
 * @param fp is the output file
 * @param e is the expression
 */
void print_expr (FILE *fp, const Expr *e);

/**
 * Print an expression to a string buffer. This is used to print
 * parameter expressions (e.g. in the core ACT language), and not
 * chp/dataflow expressions.
 *
 * @param buf is the output buffer 
 * @param sz is the size of the output buffer
 * @param e is the expression
 */
void sprint_expr (char *buf, int sz, const Expr *e);


/**
 * Print an expression to a file. This is used to print unsigned
 * expressions from chp/dataflow expressions.
 *
 * @param fp is the output file
 * @param e is the expression
 */
void print_uexpr (FILE *fp, const Expr *e);


/**
 * Print an expression to an output buffer. This is used to print unsigned
 * expressions from chp/dataflow expressions.
 *
 * @param buf is the output buffer 
 * @param sz is the size of the output buffer
 * @param e is the expression
 */
void sprint_uexpr (char *buf, int sz, const Expr *e);

/**
 * Check if the expression is a simple constant leaf expression
 *
 * @param e is the expression to be checked
 * @return 1 if it is a constant, 0 otherwise
 */
int expr_is_a_const (Expr *e);

/**
 * Duplicate an expression that is a constant. Some constant
 * expressions are cached by the TypeFactory. If that is the case,
 * then this simply returns the same pointer. Otherwise, it actually
 * allocates new memory and returns a duplicate of the constant
 * expression.
 *
 * @param e the constant expression to be duplicated
 * @return the duplicated expression (may or may not be a deep copy)
 */
Expr *expr_dup_const (Expr *e);

/**
 * This translates the E_... options into the string corresponding to
 * the expression operator
 *
 * @return a string constant for the expression operator
*/
const char *expr_op_name (int);

/* unified expression expansion code, with flags to control
   different variations */
#define ACT_EXPR_EXFLAG_ISLVAL   0x1 ///< if set, this expression must
				     ///be an lvalue (i.e. a variable)

#define ACT_EXPR_EXFLAG_PARTIAL  0x2 ///< if set, partial constant
				     ///propagation during expansion
				     ///is used

#define ACT_EXPR_EXFLAG_CHPEX    0x4 ///< if set, this uses CHP
				     ///expansion mode

#define ACT_EXPR_EXFLAG_DUPONLY  0x8 ///< this just duplicates the
				     ///expression

#define ACT_EXPR_EXFLAG_PREEXDUP 0x10 ///< flag is like DUP, only
                                      ///this is prior to expansion!

extern int _act_chp_is_synth_flag;   ///< this flag is set as a
				     ///side-effect of expression
				     ///expansion. Set to 0 if the
				     ///expression calls a
				     ///non-synthesizable function.

/**
 * Used to expand an expression. The flags are used to specify how the
 * expansion is performed. A duplicate expression is returned that
 * corresponds to the expanded expression. Parameter constants are
 * substituted, and for CHP/dataflow expressions, a BigInt is attached
 * to each constant value with the appropriate bit-width.
 *
 * @param e is the expression to be expanded
 * @param ns is the namespace
 * @param s is the evaluation scope
 * @param flag is the ACT_EXPR_EXFLAG_... flag
 * @return the expanded expression
 */
Expr *expr_expand (Expr *e, ActNamespace *ns, Scope *s, unsigned int flag = 0x2);

/**
 * A macro that just calls expr_expand() with the right flags.
 */
#define expr_dup(e) expr_expand ((e), NULL, NULL, ACT_EXPR_EXFLAG_DUPONLY)

#define expr_predup(e) expr_expand ((e), NULL, NULL, ACT_EXPR_EXFLAG_PREEXDUP|ACT_EXPR_EXFLAG_DUPONLY)

/**
 * Walk through the expression, moving it into a new namespace.
 */
Expr *expr_update (Expr *e, ActNamespace *orig, ActNamespace *newns);

/**
 * Hash function for expanded expressions
 */
int expr_getHash (int prev, unsigned long sz, Expr *e);

/**
 * Free an expanded expression
 */
void expr_ex_free (Expr *);


/**
 * Helper function for bit-width determination
 * @param etype is the expression type (E_AND, etc.)
 * @param lw is the width of the left arg
 * @param rw is the width of the right arg
 * @return the resulting bitwidth by ACT expression rules
 */
int act_expr_bitwidth (int etype, int lw, int rw);

/**
 * Helper function for bit-width determination
 * @param v is the integer
 * @return bitwidth for integer 
 */
int act_expr_intwidth (unsigned long v);

/**
 * Helper function for extracting a real value from a constant
 * expression. The expression has to be of type E_INT or E_REAL to
 * extract its value.
 * @param e is th expression
 * @param val is used to return the value
 * @return 1 if value extracted, 0 otherwise
 */
int act_expr_getconst_real (Expr *e, double *val);

/**
 * Helper function for extracting an integer value from a constant
 * expression. The expression has to be of type E_INT or E_REAL to
 * extract its value.
 * @param e is th expression
 * @param val is used to return the value
 *      
 * @return 1 if value extracted, 0 otherwise
 */
int act_expr_getconst_int (Expr *e, int *val);

/**
 * Helper function for creating an expression that's an ID
 * @param id is the ActId
 * @return the expression for it
 */
Expr *act_expr_var (ActId *id);

/**
 * Helper function: used to get a structure type from an
 * expression. This only works in the expanded scenarios.
 * @param s is the scope for evaluation
 * @param e is the expression
 * @param error is used to return the error code, if provided. Error
 * codes are: 0 = no error, 1 = invalid expression type, 2 = can't
 * find ID, 3 = not a structure, 4 = not expanded scope, 5 = e is NULL
 * @return the Data type if it is a structure, NULL otherwise
 */
Data *act_expr_is_structure (Scope *s, Expr *e, int *error = NULL);

Expr *expr_bw_adjust (int needed_width, Expr *e, Scope *s);


/**
 * Print an expression as a DAG to a file. This is used to print parameter
 * expressions (e.g. in the core ACT language), and not chp/dataflow
 * expressions.
 *
 * @param fp is the output file
 * @param e is the expression
 */
void print_dag_expr (FILE *fp, const Expr *e);

/**
 * Convert an expression into a dag. This can only be called on
 * expanded expressions.
 */
Expr *expr_dag (Expr *e);

/**
 * Free a dag expression
 */
void expr_dag_free (Expr *);


#endif /* __ACT_EXPR_API_H__ */
