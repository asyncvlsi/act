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

#include "act/miniscm/lisp.h"
#include "act/miniscm/lispInt.h"


/*
 * predicates
 */
extern LispObj *LispIsBool ();
extern LispObj *LispIsSym ();
extern LispObj *LispIsList ();
extern LispObj *LispIsPair ();
extern LispObj *LispIsNumber ();
extern LispObj *LispIsString ();
extern LispObj *LispIsProc ();
extern LispObj *LispEqv ();

/*
 * lists
 */
extern LispObj *LispCar ();
extern LispObj *LispCdr ();
extern LispObj *LispCons ();
extern LispObj *LispNull ();
extern LispObj *LispList ();
extern LispObj *LispLength ();

/*
 * controlling evaluation
 */
extern LispObj *LispQuote ();
extern LispObj *Lispeval ();
extern LispObj *LispLambda ();
extern LispObj *Lispapply ();

/*
 * definitions and side-effects
 */
extern LispObj *LispDefine ();
extern LispObj *LispSetBang ();
extern LispObj *LispLet ();
extern LispObj *LispLetRec ();
extern LispObj *LispLetStar ();
extern LispObj *LispSetCarBang ();
extern LispObj *LispSetCdrBang ();

/*
 * arithmetic
 */
extern LispObj *LispAdd ();
extern LispObj *LispSub ();
extern LispObj *LispMult ();
extern LispObj *LispDiv ();
extern LispObj *LispTruncate ();
extern LispObj *LispZeroQ ();
extern LispObj *LispPositiveQ ();
extern LispObj *LispNegativeQ ();

/*
 * control flow
 */
extern LispObj *LispBegin ();
extern LispObj *LispIf ();
extern LispObj *LispCond ();

/*
 * debugging
 */
extern LispObj *LispShowFrame ();
extern LispObj *LispDisplayObj ();
extern LispObj *LispPrintObj ();
extern LispObj *LispError ();

/*
 * I/O
 */
extern LispObj *LispLoad ();
extern LispObj *LispWrite ();
extern LispObj *LispSpawn ();
extern LispObj *LispWait ();

/*
 * utilities
 */
extern LispObj *LispCollectGarbage ();

/*
 *  String functions
 */
extern LispObj *LispStrCat ();
extern LispObj *LispSymbolToString ();
extern LispObj *LispStringToSymbol ();
extern LispObj *LispNumberToString ();
extern LispObj *LispStringToNumber ();
extern LispObj *LispStringLength ();
extern LispObj *LispStringCompare ();
extern LispObj *LispStringRef ();
extern LispObj *LispStringSet ();
extern LispObj *LispSubString ();

/*
 *  magic interaction
 */
extern LispObj *LispGetPaint ();
extern LispObj *LispGetSelPaint ();
extern LispObj *LispGetbox ();
extern LispObj *LispGetPoint ();
extern LispObj *LispGetLabel ();
extern LispObj *LispGetSelLabel ();
extern LispObj *LispGetCellNames ();
extern LispObj *LispEvalMagic ();
