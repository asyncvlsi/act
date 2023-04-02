/*************************************************************************
 *
 *  Copyright (c) 2023 Rajit Manohar
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
#ifndef __ACT_CHECK_H__
#define __ACT_CHECK_H__

/**
 * @file typecheck.h
 *
 * @brief Typechecking identifiers and expression
 *
 * The typechecking functions return an integer code. This code is
 * T_ERR on an error, or a flag that is used to summarize the type of
 * the result.
 *
 * The function act_type_errmsg() can be used to get a string that
 * corresponds to a more verbose message corresponding to the last
 * error that was encountered during type-checking.
 *
 */
#define T_ERR         -1    ///< return if the checking function
			    ///returns an error

#define T_ARRAYOF    0x20   ///< array flag

#define T_STRICT     0x40   ///< a port parameter flag that is in the
			    /// "strict" list... i.e. not optional,
			    /// i.e. before the vertical bar (NOT
			    /// USED)

#define T_PARAM      0x80   ///< a parameter type flag for pint/... etc

#define T_INT        0x1    ///< an integer (data or parameter type)
#define T_REAL       0x2    ///< a real number
#define T_BOOL       0x4    ///< a Boolean (data or parameter)
#define T_PROC       0x5    ///< a process
#define T_CHAN       0x6    ///< a channel
#define T_DATA_INT   0x7    ///< an integer within a channel
#define T_DATA_BOOL  0x8    ///< a bool within a channel
#define T_SELF       0x9    ///< special type, "self"
#define T_DATA       0xa    ///< a structure
#define T_DATA_ENUM  0xb    ///< an enum that is not an int
#define T_PTYPE      0x10   ///< used when the expression is a type

#define T_MASK       0x1f   ///< mask for the list of options

/**
 * Used to process return types by converting T_DATA_BOOL to T_BOOL,
 * and T_DATA_INT to T_INT.
 */
#define T_FIXBASETYPE(x)  ((((x) & T_MASK) == T_DATA_BOOL) ? T_BOOL : ((((x) & T_MASK) == T_DATA_INT) ? T_INT : ((x) & T_MASK)))

/**
 * Used to extract the type case minus any flags (the "base type")
 */
#define T_BASETYPE(x) ((x) & T_MASK)

/**
 * Check if the base type is an integer or real number, i.e. a numeric
 * value 
 */
#define T_BASETYPE_ISNUM(x) (T_FIXBASETYPE (x) == T_INT || T_BASETYPE (x) == T_REAL)

/**
 * Check if the processed base type is an int
 */
#define T_BASETYPE_INT(x) (T_FIXBASETYPE(x) == T_INT)

/**
 * Check if the processed base type is a bool
 */
#define T_BASETYPE_BOOL(x) (T_FIXBASETYPE(x) == T_BOOL)

/**
 * Check if the processed base type is an integer or a boolean. This
 * means it could be a chan(bool) or a chan(int) as well.
 */
#define T_BASETYPE_ISINTBOOL(x) (T_BASETYPE_INT(x) || T_BASETYPE_BOOL(x))


/**
 * Typecheck an expression, and return the result type if the
 * type-checking succeeds.
 * 
 * The only_chan flag is used to set the mode of the check. 
 *   - only_chan = 0 : normal expression, used for standard expression
 * type checking.
 *   - only_chan = 1 : the identifiers must always be channel types,
 * and the type corresponds to the data values being communicated on
 * the channels. This is used for dataflow checking.
 *   - only_chan = 2 : permit both identifiers and channels. This is
 * used for CHP expressions that can inspect the value pending on a
 * channel.
 *
 * @param s is the scope in which the type-checking is performed
 * @param e is the expression
 * @param width if non-NULL, this is used to return the bit-width of
 * the result
 * @param only_chan is a flag that determines valid identifiers.
 * @return a T_... flag that describes the result of checking
 */
int act_type_expr (Scope *s, Expr *e, int *width, int only_chan = 0);


/**
 * Used to type-check a variable
 *
 * @param s is the scope in which to peform the type-checking
 * @param id is the variable to be checked
 * @param xit is used to return the actual InstType * for the
 * identifier, if xit is non-NULL
 * @return a T_... flag that describes the result of the checking
 */
int act_type_var (Scope *s, ActId *id, InstType **xit);

/**
 * Used to type-check a channel operation. The channel operation is of
 * one of the following forms:
 *       ch!e ch?v ch!e?v ch?v!e
 *
 * An identifier could also be of the form bool(v) or int(v), which is
 * used to implicitly convert the identifier type from int to bool or
 * bool to int. The override_id flag is used for this purpose.
 *
 * @param sc is the scope in which the type-checking is performed
 * @param ch is the channel type
 * @param is_send is 1 for a send operation, 0 for a receive operation
 * @param e is the (optional) expression being sent
 * @param id is the (optional) identifer for the receive operation
 * @param override_id is -1 if int/bool overrides are absent, 0 for
 * bool(.), and 1 for int(.)
 * @return 1 on success, 0 on error
 */
int act_type_chan (Scope *sc, Chan *ch, int is_send, Expr *e, ActId *id,
		   int override_id);

/**
 * Used to type-check a connection between an identifier and an array
 * expression
 * @return 1 on success, 0 on error
 */
int act_type_conn (Scope *, ActId *, AExpr *);

/**
 * Used to type-check a connection between two array expressions
 *
 * @return 1 on success, 0 on error
 */
int act_type_conn (Scope *, AExpr *, AExpr *);


/**
 * Used to check if two types can be connected to each other. 
 * If a type is an array type, then for unexpanded types this only
 * checks that the number of dimensions are equal. For expanded types,
 * it checks that the dimension sizes also match.
 *
 * If skip_last_array is set to 1, this will skip checking the size of
 * the first dimension.
 * 
 * Return values:
 *  - 0 = they are not compatible
 *  - 1 = they are compatible
 *  - 2 = they are compatible, lhs is more specific
 *  - 3 = they are compatible, rhs is more specific
 *
 * @param lhs is the first type to check
 * @param rhs is the second type to check
 * @param skip_last_array is used to omit the first dimension check
 * @return the result of checking if the types can be connected to
 * each other
 */
int type_connectivity_check (InstType *lhs, InstType *rhs, int skip_last_array = 0);

/**
 * This checks if a value with a type corresponding to the rhs can be
 * assigned to one on the lhs in a CHP program
 *
 * @param lhs is the left-hand side type: the type of the id
 * @param rhs is the right-hand side type: the type of the expression
 * @return 1 if they are assignable, 0 otherwise
 */
int type_chp_check_assignable (InstType *lhs, InstType *rhs);

/**
 * Returns the actual type of the expression. only_chan is the same
 * flag as act_type_expr(), and can be 0, 1, 2, or 3.
 *
 * @param s is the scope
 * @param e is the expression
 * @param islocal is used to return 1 if all identifiers within this
 * expression are local to the scope and cannot be accessed from
 * outside the scope, 0 otherwise
 * @param only_chan is the same as the only_chan flag for
 * act_type_expr()
 * @return the InstType for the expression (if successful), NULL otherwise.
 */
InstType *act_expr_insttype (Scope *s, Expr *e, int *islocal, int only_chan);

/**
 * Returns the actual type of the identifier.
 *
 * @param s is the scope
 * @param id is the identifier
 * @param islocal is used to return 1 if the identifier is only
 * accessible within the local scope specified, 0 otherwise
 * @return the type of the identifier (NULL if not found)
 */
InstType *act_actual_insttype (Scope *s, ActId *id, int *islocal);

/**
 * Used to record the line, column, and file for error reporting
 *
 * @param l is the line number
 * @param c is the column number
 * @param n is the file name
 */
void type_set_position (int l, int c, char *n);

/**
 * Used to return the type-checking error message
 *
 * @return error string from last recorded type-check error message
 */
const char *act_type_errmsg (void);

/**
 * Use this to set the type-checking error message that is later
 * returned by act_type_errmsg()
 *
 * @param s is the printf-style format string
 */
void typecheck_err (const char *s, ...);

#endif /* __ACT_CHECK_H__ */
