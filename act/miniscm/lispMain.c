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
 *  lispMain.c -- 
 *
 *   This module contains the mini-scheme interpreter interface.
 *
 *************************************************************************
 */

#include <stdio.h>

#include "lisp.h"
#include "lispInt.h"

int lispInFile;			/* global variable used within the lisp
				   module used to figure out whether we're
				   in a file */

int LispInterruptExecution;

/*------------------------------------------------------------------------
 *
 *  LispEvaluate --
 *
 *      Evaluate the command-line as a lisp expression, and generate
 *      a list of commands in a local Cmd queue.
 *
 *  Results:
 *      Returns 0 if the evaluation failed; 1 otherwise.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */
int
LispEvaluate (int argc, char **argv, int inFile)
{
  extern Sexp *LispMainFrame;
  extern LispObj *LispMainFrameObj;
  LispObj *l, *res;
  char output_buf[LISP_MAX_LEN];
  int i,j,k;
  static int my_depth = 0;
  int old_infile;
  int ret_val;

  old_infile = lispInFile;
  lispInFile = inFile;
  my_depth ++;
  /*
   * convert input string into a lisp line.
   */
  k=0;
  output_buf[k++] = '(';
  for (i=0; i < argc; i++) {
    for (j=0; argv[i][j]; j++) {
      if (argv[i][j] < 32)
	output_buf[k++] = '\\';
      output_buf[k++] = argv[i][j];
    }
    output_buf[k++] = ' ';
  }
  output_buf[k++] = ')';
  output_buf[k] = '\0';

  if (my_depth == 1)
    LispCollectAllocQ = 1;

  l = LispFrameLookup (LispNewString ("scm-echo-parser-input"),
		       LispMainFrame);
  if (l && LTYPE(l) == S_BOOL && LBOOL(l)) 
    printf (" [ %s ]\n", output_buf);
  l = LispParseString (output_buf);
  res = LispFrameLookup (LispNewString ("scm-echo-parser-output"),
			 LispMainFrame);

  ret_val = 1;
  if (l) {
    if (res && LTYPE(res) == S_BOOL && LBOOL(res)) {
      printf (" >> ");
      LispPrint (stdout, l);
      printf ("\n\n");
    }
    LispInterruptExecution = 0;
    res = LispEval(l,LispMainFrame);
    if (res == NULL) {
      ret_val = 0;
    }
  }
  if (l && res) {
    l = LispFrameLookup (LispNewString ("scm-echo-result"),LispMainFrame);
    if (l && LTYPE(l) == S_BOOL && LBOOL(l)) {
      LispPrint (stdout, res);
      printf ("\n");
    }
  }
  else {
    if (LispInterruptExecution) {
      fprintf (stderr, "[Evaluation Interrupted]\n");
    }
  }
  /* collect garbage */
  if (my_depth == 1)
    LispGC (LispMainFrameObj);
  my_depth--;
  lispInFile = old_infile;
  return ret_val;
}


/*------------------------------------------------------------------------
 *
 *  LispInit --
 *
 *      Initialize lisp builtins.
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

void
LispInit (void)
{
  extern LispObj *LispMainFrameObj;

  LispMainFrameObj = LispNewObj ();
  LTYPE(LispMainFrameObj) = S_LIST;
  LLIST(LispMainFrameObj) = NULL;

  LispFnInit ();
  LispFrameInit ();
  LispGCHasWork = 0;
  LispCollectAllocQ = 0;
  LispGC (LispMainFrameObj);

  //LispSetEdit ("*unknown*");
  LispInterruptExecution = 0;

  if (getenv ("ACT_HOME")) {
    char buf[10240];
    char *args[] = { "load-scm", "\"default.scm\"", NULL };
    snprintf (buf, 10240, "%s/lib/scm", getenv ("ACT_HOME"));
    LispSetVariable ("scm-library-path", buf);
    Assert (LispEvaluate (2, args, 0) == 1, "Unexpected internal error!");
    snprintf (buf, 10240, "%s/lib", getenv ("ACT_HOME"));
    LispSetVariable ("scm-dynamic-path", buf);
  }
}


/*------------------------------------------------------------------------
 *
 *  LispSetVariable --
 *
 *      Sets the scheme variable "technology" to the technology name.
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      None.
 *
 *------------------------------------------------------------------------
 */

void
LispSetVariable (char *s, char *t)
{
  extern Sexp *LispMainFrame;
  extern LispObj *LispMainFrameObj;
  LispObj *l, *m;

  m = LispNewObj ();
  LTYPE(m) = S_SYM;
  LSYM(m) = (char *)LispNewString (s);
  l = LispNewObj ();
  LTYPE(l) = S_STRING;
  LSTR(l) = Strdup (t);
  LispAddBinding (m, l, LispMainFrame);
  LispCollectAllocQ = 0;
  LispGC (LispMainFrameObj);
}

