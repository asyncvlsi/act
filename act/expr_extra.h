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
#ifndef __ACT_EXPR_EXTRA_H__
#define __ACT_EXPR_EXTRA_H__

/**
 * @file expr_extra.h
 *
 * @brief Contains extra expression types beyond the standard ones
 * from expr.h defined in the generic expression parser included with
 * the parser generator.
 *
 * Some of these are only used during expansion of expressions to
 * return new result types (e.g. an array result)
 */

/*-- more options for expanded expressions --*/

#define E_TYPE  (E_END + 10)  ///< the "l" field will point to an InstType

#define E_ARRAY (E_END + 11) ///< an expanded paramter array
                             /// - the l field will point to the ValueIdx
			     /// - the r field will point to the Scope 

#define E_SUBRANGE (E_END + 12) ///< like array, but it is a subrange
				///   - l points to the ValueIdx
				///   - r points to another Expr whose
				///      l points to Scope
				///	 r points to the array range

#define E_SELF (E_END + 20)	///< used for "self"
#define E_SELF_ACK (E_END + 19) ///< used for "selfack"

/* 
   For loops:
      e->l->l = id, e->r->l = lo, e->r->r->l = hi, e->r->r->r = expr
      WARNING: replicated in expr_extra.c
*/
#define E_ANDLOOP (E_END + 21)        ///< used for (&i:lo..hi:expr)	   
				      ///expressions. l->l = id, r->l	   
				      ///= lo, r->r->l = hi, r->r->r = expr
      
#define E_ORLOOP (E_END + 22)         ///< used for (|i:lo..hi:expr)	   
				      ///expressions. l->l = id, r->l	   
				      ///= lo, r->r->l = hi, r->r->r = expr

#define E_BUILTIN_BOOL (E_END + 23)   ///< for bool(x)
#define E_BUILTIN_INT  (E_END + 24)   ///< for int(x) or int(x,v)

/*
  ENUM_CONST during parsing only used for
     ::foo::bar::baz.N  
     u.fn.s field == string to enum
     u.fn.r field = string for N
     
  After the "walk" re-writing, this is used as:
     u.fn.s = enum type pointer (Data *)
     u.fn.r field = string corresponding to enum element

 After type-checking, this is eliminated and replaced with an 
 int const.
*/
#define E_ENUM_CONST   (E_END + 25)  ///< used for an enumeration constant

#define E_PLUSLOOP (E_END + 26)
#define E_MULTLOOP (E_END + 27)
#define E_XORLOOP  (E_END + 28)


/*
  This is used for a macro function within a user-defined type.

  Parsing: ID.func(...)
  e->u.fn.s = pId
  e->u.fn.r- = arguments

  After walking:
  e->u.fn.s = UserMacro pointer
  e->u.fn.r = arguments, where first arg is the original ID!

  For built-in user macro int, e->u.fn.r is the
  expression itself that is the argument.

  For built-in user macro struct<...>, e->u.fn.r is the standard
  function arguments.

  After real expansion
  e->u.fn.s = synthetic function *
  e->u.fn.r = normal arguments
  converted to E_FUNCTION
 */
#define E_USERMACRO (E_END + 29)


/*
  A bitslice is  "bitslice(x,E,b,a)"
  This is used to implement
      x{b..a} := E

  E_BITSLICE
     /      |
   ActId    E_LT
            /  \
	  E    E_LT
	       /  \
	      b   a
*/	      
  
#define E_BITSLICE (E_END + 30)

#define E_NEWEND  E_END + 32	     ///< new "end" of expression options

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parsing functions for external function extras
 */
Expr *act_parse_expr_syn_loop_bool (LFILE *l);
Expr *act_parse_expr_intexpr_base (LFILE *l);
Expr *act_expr_any_basecase (LFILE *l);
int act_expr_parse_newtokens (LFILE *l);
int act_expr_free_default (Expr *);  

#ifdef __cplusplus
}
#endif

#endif /* __ACT_EXPR_EXTRA_H__ */
