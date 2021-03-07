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
 *  lispEval.c -- 
 *
 *   This module contains the core of the mini-scheme interpreter.
 *
 *************************************************************************
 */

#include <stdio.h>

#include "lispInt.h"
#include "lispA-Z.h"
#include "lispargs.h"

struct LispBuiltinFn {
  char *name;
  const char *id;
  int lazy;			/* != 0 if lazy */
  LispObj *(*f) ();		/* built-in */
};


struct LispDynamicFn {
  char *name;
  const char *id;
  LispObj *(*f) ();		/* dynamic */
};

static struct LispDynamicFn *DyTable = NULL;
static int DyTableMax = 0;
static int DyTableNum = 0;

static struct LispBuiltinFn FnTable[] = {

/*------------------------------------------------------------------------
 *      E a g e r   F u n c t i o n s
 *------------------------------------------------------------------------
 */

  /* inspect arguments */

  { "boolean?", NULL, 0, LispIsBool },
  { "symbol?", NULL, 0, LispIsSym },
  { "list?", NULL, 0, LispIsList },
  { "pair?", NULL, 0, LispIsPair },
  { "number?", NULL, 0, LispIsNumber },
  { "string?", NULL, 0, LispIsString },
  { "procedure?", NULL, 0, LispIsProc },

  /* standard list manipulation */

  { "car", NULL, 0, LispCar },
  { "cdr", NULL, 0, LispCdr },
  { "cons", NULL, 0, LispCons },
  { "set-car!", NULL, 0, LispSetCarBang },
  { "set-cdr!", NULL, 0, LispSetCdrBang },
  { "null?", NULL, 0, LispNull },
  { "list", NULL, 0, LispList },
  { "length", NULL, 0, LispLength },

  { "eval", NULL, 0, Lispeval },
  { "apply", NULL, 0, Lispapply },

  { "eqv?", NULL, 0, LispEqv },

  /* math */
  { "+", NULL, 0, LispAdd },
  { "*", NULL, 0, LispMult },
  { "-", NULL, 0, LispSub },
  { "/", NULL, 0, LispDiv },
  { "truncate", NULL, 0, LispTruncate },


  /* comparison */
  { "zero?", NULL, 0, LispZeroQ },
  { "positive?", NULL, 0, LispPositiveQ },
  { "negative?", NULL, 0, LispNegativeQ },

  /* string manipulation */
  { "string-append", NULL, 0, LispStrCat },
  { "symbol->string", NULL, 0, LispSymbolToString },
  { "string->symbol", NULL, 0, LispStringToSymbol },
  { "number->string", NULL, 0, LispNumberToString },
  { "string->number", NULL, 0, LispStringToNumber },
  { "string-length", NULL, 0, LispStringLength },
  { "string-compare", NULL, 0, LispStringCompare },
  { "string-ref", NULL, 0, LispStringRef },
  { "string-set!", NULL, 0, LispStringSet },
  { "substring", NULL, 0, LispSubString },

  /* file I/O and spawn/wait */
  { "load-scm", NULL, 0, LispLoad },
  { "save-scm", NULL, 0, LispWrite },
  { "spawn", NULL, 0, LispSpawn },
  { "wait", NULL, 0, LispWait },
  { "dlopen", NULL, 0, LispDlopen },
  { "dlbind", NULL, 0, LispDlbind },

  /* utilities */
  { "collect-garbage", NULL, 0, LispCollectGarbage },

  /* debugging help */

  { "error", NULL, 0, LispError },
  { "showframe", NULL, 0, LispShowFrame },
  { "display-object", NULL, 0, LispDisplayObj },
  { "print-object", NULL, 0, LispPrintObj },

  /* magic */
  { "builtin", NULL, 1, LispEvalBuiltin }, /* lazy */

/*------------------------------------------------------------------------
 *    N o t - s o - e a g e r   F u n c t i o n s
 *------------------------------------------------------------------------
 */
  /* lazy functions, don't evaluate any arguments */
  { "quote", NULL, 1, LispQuote },
  { "lambda", NULL, 1, LispLambda },
  { "let", NULL, 1, LispLet },
  { "let*", NULL, 1, LispLetStar },
  { "letrec", NULL, 1, LispLetRec },
  { "cond", NULL, 1, LispCond },
  
  { "begin", NULL, 1, LispBegin },

  /* define: evaluate only second argument */
  { "define", NULL, 2, LispDefine },
  { "set!",   NULL, 2, LispSetBang },

  /* if: evaluate only first argument */
  { "if", NULL, 3, LispIf },

  { NULL, 0, 0, NULL }
};

static LispObj *evalList ();



/*------------------------------------------------------------------------
 *
 *  LispFnInit --
 *
 *      Initialize function table.
 *
 *  Results:
 *      Returns result of evaluation.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

void
LispFnInit (void)
{
  int i;
  for (i=0; FnTable[i].name; i++) {
    (void) LispNewString (FnTable[i].name);
    FnTable[i].id = LispNewString (FnTable[i].name);
  }
}


/*------------------------------------------------------------------------
 *
 *  LispAddDynamicFunc --
 *
 *      Add a function to the dynamic function table
 *
 *  Results:
 *      Returns 1 on success, 0 on error
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */
int
LispAddDynamicFunc (char *name, void *f)
{
  int i;
  const char *id = LispNewString (name);

  for (i=0; FnTable[i].name; i++) {
    if (id == FnTable[i].id)
      return 0;
  }
  for (i=0; i < DyTableNum; i++) {
    if (DyTable[i].id == id) {
      DyTable[i].f = (LispObj *(*)())f;
      return 1;
    }
  }
  if (DyTableNum == DyTableMax) {
    if (DyTableMax == 0) {
      DyTableMax = 32;
      MALLOC (DyTable, struct LispDynamicFn, DyTableMax);
    }
    else {
      DyTableMax *= 2;
      REALLOC (DyTable, struct LispDynamicFn, DyTableMax);
    }
  }
  DyTable[DyTableNum].name = Strdup (name);
  DyTable[DyTableNum].id = id;
  DyTable[DyTableNum].f = (LispObj *(*)()) f;
  DyTableNum++;
  return 1;
}

  


/*-----------------------------------------------------------------------------
 *
 *  ispair --
 *
 *      Checks if its argument is a dotted-pair.
 *
 *  Results:
 *      1 if dotted-pair, zero otherwise.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
ispair (Sexp *s)
{
  while (s && LTYPE(CDR(s)) == S_LIST)
    s = LLIST(CDR(s));
  if (s)
    return 1;
  else
    return 0;
}


/*------------------------------------------------------------------------
 *
 *  lookup --
 *
 *      Lookup a name in a frame.
 *
 *  Results:
 *      Returns result of lookup.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

LispObj *
lookup (char *s, Sexp *f)
{
  LispObj *l;
  Sexp *f1;
  int i;
  const char *k;

  /* keywords have precedence */
  k = LispNewString (s);
  for (i=0; FnTable[i].name; i++)
    if (FnTable[i].id == k) {
      l = LispNewObj ();
      LTYPE(l) = S_LAMBDA_BUILTIN;
      LBUILTIN(l) = i;
      return l;
    }
  for (i=0; i < DyTableNum; i++) {
    if (DyTable[i].id == k) {
      l = LispNewObj ();
      LTYPE(l) = S_LAMBDA_BUILTIN_DYNAMIC;
      LBUILTIN(l) = i;
      return l;
    }
  }
  /* look in frame */
  l = LispFrameLookup (s,f);
  if (l) return l;
  /* assume that it is a builtin command */
  l = LispNewObj ();
  LTYPE(l) = S_MAGIC_BUILTIN;
  LSYM(l) = s;
  return l;
}


/*------------------------------------------------------------------------
 *
 *  LispMagicSend --
 *
 *      Send magic command to magic window.
 *
 *  Results:
 *      Returns #t if magic command exists, #f otherwise.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */
LispObj *
LispMagicSend (char *name, Sexp *s, Sexp *f)
{
  int trace;
  LispObj *l;
  int argc;
  char *argv[LISP_MAXARGS];
  char argstring[LISP_MAX_CMDLEN];
  int k = 0;
  int i, j;
  int ret;
  
  argc = 1;
  argv[0] = name;
  while (s) {
    l = CAR(s);
    if (LTYPE(CDR(s)) != S_LIST) {
      fprintf (stderr, "%s: invalid argument!\n",name);
      RETURN;
    }
    s = LLIST(CDR(s));
    switch (LTYPE(l)) {
    case S_INT:
      argv[argc] = argstring+k;
      sprintf (argstring+k, "%d", LINTEGER(l));
      k = k + strlen(argstring+k)+1;
      break;
    case S_FLOAT:
      argv[argc] = argstring+k;
      sprintf (argstring+k, "%lf", LFLOAT(l));
      k = k + strlen(argstring+k)+1;
      break;
    case S_STRING:
      /* undo one level of literal parsing . . . */
      argv[argc] = LSTR(l);
      i = 0; j = 0;
      while (argv[argc][i]) {
	if (argv[argc][i] == '\\')
	  i++;
	argv[argc][j] = argv[argc][i];
	i++; j++;
      }
      argv[argc][j] = '\0';
      break;
    case S_BOOL:
      argv[argc] = argstring+k;
      sprintf (argstring+k, "#%c", LINTEGER(l) ? 't' : 'f');
      k = k + strlen(argstring+k)+1;
      break;
    case S_SYM:
      argv[argc] = LSYM(l);
      break;
    case S_LAMBDA:
      fprintf (stderr, "%s: Type #proc in builtin command argument.\n",name);
      RETURN;
      break;
    case S_LAMBDA_BUILTIN:
      argv[argc] = FnTable[LBUILTIN(l)].name;
      break;
    case S_LAMBDA_BUILTIN_DYNAMIC:
      argv[argc] = DyTable[LBUILTIN(l)].name;
      break;
    case S_MAGIC_BUILTIN:
      argv[argc] = LSYM(l);
      break;
    case S_LIST:
      fprintf (stderr, "%s: Type #list in builtin command argument.\n",name);
      RETURN;
      break;
    default:
      argc--;
      break;
    }
    argc++;
  }
  l = LispFrameLookup (LispNewString ("scm-trace-builtin"), f);
  if (!l)
    trace = 0;
  else if (LTYPE(l) != S_BOOL) {
    fprintf (stderr, "builtin-dispatch: scm-trace-builtin is not a boolean\n");
    RETURN;
  }
  else
    trace = LBOOL(l);
  ret = LispDispatch (argc, argv, trace, lispInFile);
  if (ret == 0) {
    RETURN;
  }
  else if (ret == 1) {
    l = LispNewObj ();
    LTYPE(l) = S_BOOL;
    LBOOL(l) = 1;
    return l;
  }
  else if (ret == 2) {
    l = LispNewObj ();
    LTYPE (l) = S_INT;
    LINTEGER (l) = LispGetReturnInt ();
    return l;
  }
  else if (ret == 3) {
    l = LispNewObj ();
    LTYPE (l) = S_STRING;
    LSTR (l) = LispGetReturnString ();
    return l;
  }
  /* default: return #f */
  RETURN;
}


/*-----------------------------------------------------------------------------
 *
 *  LispApply --
 *
 *      Evaluate a lambda.
 *          s = definition of the lambda
 *          l = list of arguments
 *          f = frame
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispApply (Sexp *s, Sexp *l, Sexp *f)
{
  int len;
  int dp;
  Sexp *t, *tp;
  int number, anum;
  Sexp *arglist;
  Sexp *frame;
  LispObj *eval;

  number = LINTEGER(ARG1(s));
  arglist = LLIST(ARG2(s));
  dp = ispair (arglist);
  frame = LLIST(ARG3(s));
  eval = ARG4(s);
  len=0;
  tp = NULL;
  t = l;
  while (t && LTYPE(CDR(t)) == S_LIST) {
    tp = t;
    t = LLIST(CDR(t));
    len++;
  }
  
  anum = (number < 0)  ? -number : number;
  if (len != anum) {
    fprintf (stderr, "apply: mismatch in # of arguments. Expected %d, got %d\n",
	      anum, len);
    RETURN;
  }
  t = arglist;
  f = LispFramePush (frame);
  while (t && LTYPE(CDR(t)) == S_LIST) {
    LispAddBinding (CAR(t),LispCopyObj(CAR(l)),f);
    t = LLIST(CDR(t));
    l = LLIST(CDR(l));
  }
  if (t) {
    LispAddBinding (CAR(t),LispCopyObj(CAR(l)),f);
    LispAddBinding (CDR(t),LispCopyObj(CDR(l)),f);
  }
  eval = LispEval (eval, f);
  return eval;
}
    


/*-----------------------------------------------------------------------------
 *
 *  LispBuiltinApply --
 *
 *      Apply a builtin function to a list
 *
 *  Results:
 *      The results of the builtin function
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispBuiltinApply (int num, Sexp *s, Sexp *f)
{
  return FnTable[num].f(FnTable[num].name, s, f);
}


/*-----------------------------------------------------------------------------
 *
 *  LispBuiltinApplyDynamic --
 *
 *      Apply a builtin function to a list
 *
 *  Results:
 *      The results of the builtin function
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispBuiltinApplyDynamic (int num, Sexp *s, Sexp *f)
{
  LispObj *l;
  LispObj *tmp = DyTable[num].f(DyTable[num].name, s);
  l = LispCopyObj (tmp);
  FREE (tmp);
  return l;
}


  
/*------------------------------------------------------------------------
 *
 *  evalList --
 *
 *      Evaluate list
 *
 *  Results:
 *      Returns result of evaluation.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

static
LispObj *
evalList (Sexp *s, Sexp *f)
{
  LispObj *l;
  Sexp *t;

  if (!s) {
    l = LispNewObj ();
    LTYPE(l) = S_LIST;
    LLIST(l) = NULL;
    return l;
  }
  /* evaluate car field */
  s = LispCopySexp (s);

  LispGCAddSexp (s);
  CAR(s) = LispEval (CAR(s),f);
  LispGCRemoveSexp (s);

  if (!CAR(s))
    return NULL;

  if (LTYPE(CAR(s)) != S_MAGIC_BUILTIN &&
      LTYPE(CAR(s)) != S_LAMBDA_BUILTIN &&
      LTYPE(CAR(s)) != S_LAMBDA_BUILTIN_DYNAMIC &&
      LTYPE(CAR(s)) != S_LAMBDA) {
    fprintf (stderr, "eval: First argument of list is not a procedure.\n");
    fprintf (stderr, "\t");
    if (CAR(s)) LispPrint (stdout, CAR(s));
    else fprintf (stderr, "()");
    fprintf (stderr, "\n");
    RETURN;
  }
  /* evaluate rest of list, if the car field corresponds to a non-lazy
     function.
  */
  if (LTYPE(CAR(s)) == S_LAMBDA_BUILTIN) {
    LispGCAddSexp (s);
    if (FnTable[LBUILTIN(CAR(s))].lazy == 2) {
      LispStackPush (FnTable[LBUILTIN(CAR(s))].name);
      /* define: evaluate second argument only */
      CDR(s) = LispCopyObj (CDR(s));
      if (LTYPE(CDR(s)) != S_LIST || LLIST(CDR(s)) == NULL) {
	fprintf (stderr, "define: argument error\n");
	LispGCRemoveSexp (s);
	RETURNPOP;
      }
      LLIST(CDR(s)) = LispCopySexp (LLIST(CDR(s)));
      t = LLIST(CDR(s));
      if (LTYPE(CDR(t)) != S_LIST || LLIST(CDR(t)) == NULL) {
	fprintf (stderr, "define: argument error\n");
	LispGCRemoveSexp (s);
	RETURNPOP;
      }
      CDR(t) = LispCopyObj (CDR(t));
      LLIST(CDR(t)) = LispCopySexp (LLIST(CDR(t)));
      t = LLIST(CDR(t));
      CAR(t) = LispEval (CAR(t),f);
      LispStackPop ();
      if (!CAR(t)) {
	LispGCRemoveSexp (s);
	return NULL;
      }
    }
    else if (FnTable[LBUILTIN(CAR(s))].lazy == 3) {
      /* if: evaluate first argument only */
      LispStackPush (FnTable[LBUILTIN(CAR(s))].name);
      CDR(s) = LispCopyObj (CDR(s));
      if (LTYPE(CDR(s)) != S_LIST || LLIST(CDR(s)) == NULL) {
	fprintf (stderr, "if: argument error\n");
	LispGCRemoveSexp (s);
	RETURNPOP;
      }
      LLIST(CDR(s)) = LispCopySexp (LLIST(CDR(s)));
      t = LLIST(CDR(s));
      CAR(t) = LispEval (CAR(t),f);
      LispStackPop ();
      if (!CAR(t)) {
	LispGCRemoveSexp (s);
	return NULL;
      }
    }
    LispGCRemoveSexp (s);
  }
  if (!(LTYPE(CAR(s)) == S_LAMBDA_BUILTIN && FnTable[LBUILTIN(CAR(s))].lazy)) {
    LispGCAddSexp (s);
    if (LTYPE(CAR(s)) == S_LAMBDA_BUILTIN)
      LispStackPush (FnTable[LBUILTIN(CAR(s))].name);
    else if (LTYPE(CAR(s)) == S_MAGIC_BUILTIN)
      LispStackPush (LSYM(CAR(s)));
    else if (LTYPE(CAR(s)) == S_LAMBDA_BUILTIN_DYNAMIC)
      LispStackPush (DyTable[LBUILTIN(CAR(s))].name);
    else {
      char *str;
      str = LispFrameRevLookup (CAR(s),f);
      LispStackPush (str ? str : "#proc-userdef");
    }
    t = s;
    while (LTYPE(CDR(t)) == S_LIST && LLIST(CDR(t))) {
      CDR(t) = LispCopyObj (CDR(t));
      LLIST(CDR(t)) = LispCopySexp (LLIST(CDR(t)));
      t = LLIST(CDR(t));
      CAR(t) = LispEval (CAR(t),f);
      if (CAR(t) == NULL) {
	LispStackPop ();
	LispGCRemoveSexp (s);
	return NULL;
      }
    }
    if (LTYPE(CDR(t)) != S_LIST) {
      CDR(t) = LispEval (CDR(t),f);
      if (CDR(t) == NULL) {
	LispStackPop ();
	LispGCRemoveSexp (s);
	return NULL;
      }
    }
    LispStackPop ();
    LispGCRemoveSexp (s);
  }
  if (LTYPE(CDR(s)) != S_LIST) {
    /* a dotted pair . . . */
    l = LispNewObj ();
    LTYPE(l) = S_LIST;
    LLIST(l) = s;
    return l;
  }
  /* dispatch function */
  if (LTYPE(CAR(s)) == S_LAMBDA_BUILTIN) {
    LispStackPush (FnTable[LBUILTIN(CAR(s))].name);
    l = LispBuiltinApply (LBUILTIN(CAR(s)), LLIST(CDR(s)), f);
    LispStackPop ();
  }
  else if (LTYPE(CAR(s)) == S_LAMBDA_BUILTIN_DYNAMIC) {
    LispStackPush (DyTable[LBUILTIN(CAR(s))].name);
    l = LispBuiltinApplyDynamic (LBUILTIN(CAR(s)), LLIST(CDR(s)), f);
    LispStackPop ();
  }
  else if (LTYPE(CAR(s)) == S_LAMBDA) {
    char *str;
    str = LispFrameRevLookup (CAR(s),f);
    LispStackPush (str ? str : "#proc-userdef");
    l = LispApply (LUSERDEF(CAR(s)), LLIST(CDR(s)),f);
    LispStackPop ();
  }
  else {
    LispStackPush (LSYM(CAR(s)));
    l = LispMagicSend (LSYM(CAR(s)),LLIST(CDR(s)), f);
    LispStackPop ();
  }
  return l;
}




/*------------------------------------------------------------------------
 *
 *  LispEval --
 *
 *      Evaluate object in a frame.
 *
 *  Results:
 *      Returns result of evaluation.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

LispObj *
LispEval (LispObj *l, Sexp *f)
{
  LispObj *ret;

  if (LispInterruptExecution) return NULL;

  if (LTYPE(l) == S_LIST) {
    LispGCAddSexp (f);
    ret = evalList (LLIST(l),f);
    LispGCRemoveSexp (f);
    return ret;
  }
  else if (LTYPE(l) == S_SYM)
    return lookup (LSYM(l),f);
  else
    return l;
}
