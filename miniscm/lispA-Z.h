/*************************************************************************
 *
 *  Copyright (c) 1996, 2020 Rajit Manohar
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
/*************************************************************************
 *
 *  lispA-Z.h -- 
 *
 *   Declarations for builtins.
 *
 *************************************************************************
 */

#include <stdio.h>

#include "lisp.h"
#include "lispInt.h"


/*
 * predicates
 */
extern LispObj *LispIsBool (char *, Sexp *, Sexp *);
extern LispObj *LispIsSym (char *, Sexp *, Sexp *);
extern LispObj *LispIsList (char *, Sexp *, Sexp *);
extern LispObj *LispIsPair (char *, Sexp *, Sexp *);
extern LispObj *LispIsNumber (char *, Sexp *, Sexp *);
extern LispObj *LispIsString (char *, Sexp *, Sexp *);
extern LispObj *LispIsProc (char *, Sexp *, Sexp *);
extern LispObj *LispEqv (char *, Sexp *, Sexp *);

/*
 * lists
 */
extern LispObj *LispCar (char *, Sexp *, Sexp *);
extern LispObj *LispCdr (char *, Sexp *, Sexp *);
extern LispObj *LispCons (char *, Sexp *, Sexp *);
extern LispObj *LispNull (char *, Sexp *, Sexp *);
extern LispObj *LispList (char *, Sexp *, Sexp *);
extern LispObj *LispLength (char *, Sexp *, Sexp *);

/*
 * controlling evaluation
 */
extern LispObj *LispQuote (char *, Sexp *, Sexp *);
extern LispObj *Lispeval (char *, Sexp *, Sexp *);
extern LispObj *LispLambda (char *, Sexp *, Sexp *);
extern LispObj *Lispapply (char *, Sexp *, Sexp *);

/*
 * definitions and side-effects
 */
extern LispObj *LispDefine (char *, Sexp *, Sexp *);
extern LispObj *LispSetBang (char *, Sexp *, Sexp *);
extern LispObj *LispLet (char *, Sexp *, Sexp *);
extern LispObj *LispLetRec (char *, Sexp *, Sexp *);
extern LispObj *LispLetStar (char *, Sexp *, Sexp *);
extern LispObj *LispSetCarBang (char *, Sexp *, Sexp *);
extern LispObj *LispSetCdrBang (char *, Sexp *, Sexp *);

/*
 * arithmetic
 */
extern LispObj *LispAdd (char *, Sexp *, Sexp *);
extern LispObj *LispSub (char *, Sexp *, Sexp *);
extern LispObj *LispMult (char *, Sexp *, Sexp *);
extern LispObj *LispDiv (char *, Sexp *, Sexp *);
extern LispObj *LispTruncate (char *, Sexp *, Sexp *);
extern LispObj *LispZeroQ (char *, Sexp *, Sexp *);
extern LispObj *LispPositiveQ (char *, Sexp *, Sexp *);
extern LispObj *LispNegativeQ (char *, Sexp *, Sexp *);

/*
 * control flow
 */
extern LispObj *LispBegin (char *, Sexp *, Sexp *);
extern LispObj *LispIf (char *, Sexp *, Sexp *);
extern LispObj *LispCond (char *, Sexp *, Sexp *);

/*
 * debugging
 */
extern LispObj *LispShowFrame (char *, Sexp *, Sexp *);
extern LispObj *LispDisplayObj (char *, Sexp *, Sexp *);
extern LispObj *LispPrintObj (char *, Sexp *, Sexp *);
extern LispObj *LispError (char *, Sexp *, Sexp *);

/*
 * I/O
 */
extern LispObj *LispLoad (char *, Sexp *, Sexp *);
extern LispObj *LispWrite (char *, Sexp *, Sexp *);
extern LispObj *LispSpawn (char *, Sexp *, Sexp *);
extern LispObj *LispWait (char *, Sexp *, Sexp *);

/*
 * utilities
 */
extern LispObj *LispCollectGarbage (char *, Sexp *, Sexp *);

/*
 *  String functions
 */
extern LispObj *LispStrCat (char *, Sexp *, Sexp *);
extern LispObj *LispSymbolToString (char *, Sexp *, Sexp *);
extern LispObj *LispStringToSymbol (char *, Sexp *, Sexp *);
extern LispObj *LispNumberToString (char *, Sexp *, Sexp *);
extern LispObj *LispStringToNumber (char *, Sexp *, Sexp *);
extern LispObj *LispStringLength (char *, Sexp *, Sexp *);
extern LispObj *LispStringCompare (char *, Sexp *, Sexp *);
extern LispObj *LispStringRef (char *, Sexp *, Sexp *);
extern LispObj *LispStringSet (char *, Sexp *, Sexp *);
extern LispObj *LispSubString (char *, Sexp *, Sexp *);
