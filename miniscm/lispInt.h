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
 *  lispInt.h -- 
 *
 *   Internals of the lisp module.
 *
 *************************************************************************
 */
#ifndef __LISPINT_H__
#define __LISPINT_H__

#include <stdio.h>
#include <string.h>
#include <common/misc.h>

#define LISP_MAXARGS 256
#define LISP_MAX_CMDLEN 10240

/*
  Lisp Sexp's are allocated in blocks of the following size
*/
#define LISP_SEXP_BLKSZ  1000
#define LISP_MAX_LEN  (LISP_MAX_CMDLEN+3)

enum Sexp_types {
  S_LIST,
  S_SYM,
  S_MAGIC_BUILTIN,
  S_LAMBDA_BUILTIN,
  S_LAMBDA_BUILTIN_DYNAMIC,
  S_LAMBDA,
  S_INT,
  S_FLOAT,
  S_STRING,
  S_BOOL
};

#define ATOM(t) ((t) == S_INT || (t) == S_FLOAT || \
		 (t) == S_STRING || (t) == S_BOOL)


#define LTYPE(l) ((l)->t)
#define LLIST(m) ((m)->u.l)
#define LUSERDEF(m) ((m)->u.l)
#define LBUILTIN(m) ((m)->u.i)
#define LSYM(l) ((l)->u.s)
#define LSTR(l) ((l)->u.s)
#define LBOOL(l) ((l)->u.i)
#define LFLOAT(l) ((l)->u.d)
#define LINTEGER(l) ((l)->u.i)

#define TYPECAR(s) LTYPE((s)->l[0])
#define TYPECDR(s) LTYPE((s)->l[1])

#define LCAR(s) LLIST(CAR(s))
#define SYMCAR(s) LSYM((s)->l[0])
#define STRCAR(s) LSTR((s)->l[0])
#define INTCAR(s) LINTEGER((s)->l[0])
#define FLOATCAR(s) LFLOAT((s)->l[0])
#define BOOLCAR(s) LBOOL((s)->l[0])

#define LCDR(s) LLIST((s)->l[1])
#define SYMCDR(s) LSYM((s)->l[1])
#define STRCDR(s) LSTR((s)->l[1])
#define INTCDR(s) LINTEGER((s)->l[1])
#define FLOATCDR(s) LFLOAT((s)->l[1])
#define BOOLCDR(s) LBOOL((s)->l[1])

#define CAR(s)  ((s)->l[0])
#define CDR(s)  ((s)->l[1])



#define NBITS (sizeof(unsigned int)*8)

struct Sexp;

typedef struct LispObj {
  struct LispObj *n;
  unsigned char t;		/* type */
  union {
    struct Sexp *l;
    long i;
    double d;
    char *s;
  } u;
} LispObj;
       
typedef struct Sexp {
  struct Sexp *n;
  LispObj *l[2];
} Sexp;

/*
 * Internal commands 
 */

extern LispObj *LispNewObj ();
extern LispObj *LispCopyObj (LispObj *);

extern Sexp *LispNewSexp ();
extern Sexp *LispCopySexp (Sexp *);

extern LispObj *LispEval (LispObj *l, Sexp *f);
extern LispObj *LispApply (Sexp *s, Sexp *l, Sexp *f);
extern LispObj *LispMagicSend (char *, Sexp *, Sexp *);
extern LispObj *LispEvalBuiltin (char *, Sexp *, Sexp *);
extern LispObj *LispBuiltinApply (int, Sexp *, Sexp *);
extern LispObj *LispBuiltinApplyDynamic (int, Sexp *, Sexp *);
extern int LispAddDynamicFunc (char *name, void *func);

extern void LispPrint (FILE *, LispObj *);
extern void LispPrintType (FILE *, LispObj *);

extern LispObj *LispParseString (char *);
extern LispObj *LispAtomParse (char **, int);
extern const char *LispNewString (char *);
extern void LispFnInit (void);

extern void LispGC (LispObj *root);
extern int LispGCHasWork;
extern int LispCollectAllocQ;
extern void LispGCAddSexp (Sexp *);
extern void LispGCRemoveSexp (Sexp *);

extern LispObj *LispFrameLookup (const char *s, Sexp *f);
extern char *LispFrameRevLookup (LispObj *l, Sexp *f);
extern void LispFrameInit (void);
extern void LispAddBinding (LispObj *s, LispObj *l, Sexp *f);
extern Sexp *LispFramePush (Sexp *);

extern int LispModifyBinding (LispObj *name, LispObj *val, Sexp *f);

extern void LispStackPush (char *);
extern void LispStackPop (void);
extern void LispStackDisplay (void);
extern void LispStackClear (void);

extern int lispInFile;		/* flag used for :setpoint */

extern FILE *LispPathOpen (char *name, char *mode, char *pathspec);
extern char *LispPathFile (char *name, char *pathspec);

extern LispObj *LispDlopen (char *name, Sexp *s, Sexp *f);
extern LispObj *LispDlbind (char *name, Sexp *s, Sexp *f);
  
#endif /* __LISPINT_H__ */
