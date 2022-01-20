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
 *  lispIO.c --
 *
 *   This module contains the builtin mini-scheme I/O functions.
 *
 *************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>

#include "lisp.h"
#include "lispInt.h"
#include "lispargs.h"

/*-----------------------------------------------------------------------------
 *
 *  LispLoad --
 *
 *      ("load-scm" "filename")
 *      Reads and evaluates file.
 *      
 *
 *  Results:
 *      #t => file was opened successfully.
 *      #f => failure.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispLoad (char *name, Sexp *s, Sexp *f)
{
  extern int LispEchoResult;
  LispObj *l, *inp, *res;
  FILE *fp;
  int val, pos;
  char *buffer, *tmp;
  int buflen;
  int nest;
  int line;

  if (!ARG1P(s) || LTYPE(ARG1(s)) != S_STRING || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s string)\n", name);
    RETURN;
  }
  l = LispFrameLookup (LispNewString ("scm-library-path"), f);
  if (!l)
    tmp = NULL;
  else if (LTYPE(l) != S_STRING) {
    fprintf (stderr, "%s: scm-library-path is not a string\n", name);
    RETURN;
  }
  else
    tmp = LSTR(l);
  if (!(fp = LispPathOpen (LSTR(ARG1(s)), "r", tmp))) {
    fprintf (stderr, "%s: could not open file %s for reading\n",name,LSTR(ARG1(s)));
    l = LispNewObj ();
    LTYPE(l) = S_BOOL;
    LBOOL(l) = 0;
    RETURN;
  }

  LispGCAddSexp (s);

  buflen = 4096;
  MALLOC (buffer, char, buflen);
  pos = 0;
  nest = 0;
  line = 1;
  while ((val = fgetc (fp)) != EOF) {
    if (pos == buflen) {
      int i;
      /* extend buffer */
      tmp = buffer;
      buflen += 1024;
      MALLOC (buffer, char, buflen);
      for (i=0; i < pos; i++)
	buffer[i] = tmp[i];
      FREE (tmp);
    }
    if (val == ';') {
      /* skip to eol */
      while ((val = fgetc(fp)) != EOF && val != '\n') 
	;
      if (val == '\n') line++;
      continue;
    }
    if (val == '\n') line++;
    if (val == '\t' || val == '\n') val = ' ';
    /* skip white space at nesting level zero */
    if (nest == 0 && isspace (val)) 
      continue;
    if (nest == 0 && val != '(') {
      fprintf (stderr, "Error reading file %s, line %d\n", LSTR(ARG1(s)), line);
      l = LispNewObj ();
      LTYPE(l) = S_BOOL;
      LBOOL(l) = 0;
      FREE (buffer);
      fclose (fp);
      LispGCRemoveSexp (s);
      RETURN;
    }
    buffer[pos++] = val;
    if (val == '(')
      nest++;
    else if (val == ')') {
      nest--;
      if (nest == 0) {
	buffer[pos] = '\0';
	inp = LispParseString (buffer);
	if (inp) {
	  res = LispEval (inp, f);
	  inp = LispFrameLookup (LispNewString ("scm-echo-result"), f);
	  if (res && inp && LTYPE(inp) == S_BOOL && LBOOL(inp)) {
	    LispPrint (stdout,res);
	    printf ("\n");
	  }
	  if (!res) {
	    if (!LispInterruptExecution)
	      fprintf (stderr, "Error evaluating file %s, line %d\n",
			LSTR(ARG1(s)), line);
            FREE(buffer);
	    fclose (fp);
	    LispGCRemoveSexp (s);
	    RETURN;
	  }
	}
	else {
	  fprintf (stderr, "Error parsing file %s, line %d\n", LSTR(ARG1(s)), line);
	  FREE(buffer);
	  fclose (fp);
	  LispGCRemoveSexp (s);
	  RETURN;
	}
	pos = 0;
      }
      if (nest < 0) {
	fprintf (stderr, "Error reading file %s, line %d\n", LSTR(ARG1(s)), line);
	l = LispNewObj ();
	LTYPE(l) = S_BOOL;
	LBOOL(l) = 0;
	FREE(buffer);
	fclose (fp);
	LispGCRemoveSexp (s);
	return l;
      }
    }
    else if (val == '\"') {
      while ((val = fgetc (fp)) != EOF && val != '\"') {
	if (val == '\n') line++;
	if (pos > buflen-1) {
	  /* extend buffer */
	  int i;
	  tmp = buffer;
	  buflen += 1024;
	  MALLOC (buffer, char, buflen);
	  for (i=0; i < pos; i++)
	    buffer[i] = tmp[i];
	  FREE(tmp);
	}
	buffer[pos++] = val;
	if (val == '\\') {
	  val = fgetc (fp);
	  buffer[pos++] = val;
	  if (val == '\n') line++;
	}
      }
      if (val == EOF) {
	fprintf (stderr, "Error reading file %s, line %d\n", LSTR(ARG1(s)), line);
	FREE(buffer);
	fclose (fp);
	LispGCRemoveSexp (s);
	RETURN;
      }
      buffer[pos++] = val;
    }
  }
  FREE(buffer);
  fclose (fp);
  if (pos > 0) {
    fprintf (stderr, "Error reading file %s, line %d\n", LSTR(ARG1(s)), line);
    LispGCRemoveSexp (s);
    RETURN;
  }
  else  {
    LispGCRemoveSexp (s);
    l = LispNewObj ();
    LTYPE(l) = S_BOOL;
    LBOOL(l) = 1;
  }
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispWrite --
 *
 *      Write an object to a file.
 *
 *  Results:
 *      none.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispWrite (char *name, Sexp *s, Sexp *f)
{
  FILE *fp;
  LispObj *l;

  if (!ARG1P(s) || LTYPE(ARG1(s)) != S_STRING || !ARG2P(s) || ARG3P(s)) {
    fprintf (stderr, "Usage: (%s str obj)\n", name);
    RETURN;
  }
  if (!(fp = LispPathOpen (LSTR(ARG1(s)), "a", NULL))) {
    fprintf (stderr, "%s: could not open file %s for writing\n",name,LSTR(ARG1(s)));
    RETURN;
  }
  fprintf (fp, ";\n");
  LispPrint (fp,ARG2(s));
  fprintf (fp, "\n");
  fclose (fp);
  l = LispNewObj ();
  LTYPE(l) = S_BOOL;
  LBOOL(l) = 1;
  return l;
}



/*-----------------------------------------------------------------------------
 *
 *  LispSpawn --
 *
 *      (spawn list-of-strings)
 *      Reads and evaluates file.
 *      
 *
 *  Results:
 *      pid => the pid of the spawned process.
 *      -1  => if spawn failed.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispSpawn (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  int pid;
  Sexp *t;
  char **argv;
  int n;

  if (!ARG1P(s)) {
    fprintf (stderr, "Usage: (%s string-list)\n", name);
    RETURN;
  }

  t = s;
  n = 1;
  while (ARG1P(t)) {
    if (LTYPE(CAR(t)) != S_STRING) {
      fprintf (stderr, "Usage: (%s string-list)\n", name);
      RETURN;
    }
    n++;
    t = LLIST(CDR(t));
  }
  MALLOC (argv, char *, n);
  t = s;
  n = 0;
  while (ARG1P(t)) {
    argv[n] = LSTR(CAR(t));
    n++;
    t = LLIST(CDR(t));
  }
  argv[n] = NULL;
  
  pid = fork();
  if (pid < 0) {
    fprintf (stderr, "Error: could not fork a process!\n");
    FREE(argv);
    RETURN;
  }
  else if (pid == 0) {
    int i;
    /* try closing all files, so that we don't mess up the state of
     the parent */
    for (i=3; i < 256; i++)
	close (i);
    execvp (argv[0], argv);
    _exit (1000);
  }
  FREE(argv);
  l = LispNewObj ();
  LTYPE(l) = S_INT;
  LINTEGER(l) = pid;
  return l;
}


/*-----------------------------------------------------------------------------
 *
 *  LispWait --
 *
 *      (wait pid)
 *      Wait for pid to terminate.
 *
 *  Results:
 *      The status, error if the pid is an invalid pid.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispWait (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  int stat;

  if (!ARG1P(s) || LTYPE(ARG1(s)) != S_INT) {
    fprintf (stderr, "Usage: (%s pid)\n", name);
    RETURN;
  }
  
  if (waitpid (LINTEGER(ARG1(s)), &stat, 0) < 0) {
    fprintf (stderr, "%s: waiting for an invalid pid\n", name);
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_INT;
  LINTEGER(l) = stat;
  return l;
}

#define MAX_LIBS 16

static int num_libs = 0;
static void *tot_libs[MAX_LIBS];

static int _stash_library (void *lib)
{
  if (lib == NULL) {
    return -1;
  }
  if (num_libs == MAX_LIBS) {
    return -2;
  }
  tot_libs[num_libs++] = lib;
  return num_libs-1;
}

static void *_stash_get (int lib)
{
  if (lib < 0) return NULL;
  if (lib >= num_libs) return NULL;
  return tot_libs[lib];
}

/*-----------------------------------------------------------------------------
 *
 *  LispDlopen --
 *
 *      (scm-dlopen "file")
 *      Returns handler to library
 *
 *  Results:
 *      The library ID if successful,  error if the library could not be opened
 *
 *  Side effects:
 *      Library is opened and available
 *
 *-----------------------------------------------------------------------------
 */
LispObj *
LispDlopen (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  char *tmp;
  char *fname;
  void *dl;
  int ret;

  if (!ARG1P(s) || LTYPE(ARG1(s)) != S_STRING || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s string)\n", name);
    RETURN;
  }
  l = LispFrameLookup (LispNewString ("scm-dynamic-path"), f);
  if (!l)
    tmp = NULL;
  else if (LTYPE(l) != S_STRING) {
    fprintf (stderr, "%s: scm-dynamic-path is not a string\n", name);
    RETURN;
  }
  else
    tmp = LSTR(l);
  if (!(fname = LispPathFile (LSTR(ARG1(s)), tmp))) {
    fprintf (stderr, "%s: could not find library %s\n",name,LSTR(ARG1(s)));
    l = LispNewObj ();
    LTYPE(l) = S_BOOL;
    LBOOL(l) = 0;
    RETURN;
  }

  dl = dlopen (fname, RTLD_LAZY);
  ret = _stash_library (dl);

  if (ret >= 0) {
    l = LispNewObj ();
    LTYPE(l) = S_INT;
    LINTEGER(l) = ret;
    return l;
  }
  else {
    if (ret == -1) {
      fprintf (stderr, "%s: could not find library %s\n",name,LSTR(ARG1(s)));
    }
    else {
      fprintf (stderr, "%s: too many open libraries\n",name);
    }      
    l = LispNewObj ();
    LTYPE(l) = S_BOOL;
    LBOOL(l) = 0;
    RETURN;
  }
}


LispObj *
LispDlbind (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  char *fname;
  void *func;
  void *dl;
  int ret;

  if (!ARG1P(s) || !ARG2P(s) || !ARG3P(s) || ARG4P(s) ||
      LTYPE(ARG1(s)) != S_INT || LTYPE(ARG2(s)) != S_STRING ||
      LTYPE(ARG3(s)) != S_STRING) {
    fprintf (stderr, "Usage: (%s int string string)\n", name);
    RETURN;
  }
  dl = _stash_get (LINTEGER(ARG1(s)));
  if (!dl) {
    fprintf (stderr, "%s: could not find library #%ld\n",name,LINTEGER(ARG1(s)));
    l = LispNewObj ();
    LTYPE(l) = S_BOOL;
    LBOOL(l) = 0;
    RETURN;
  }
  func = dlsym (dl, LSTR(ARG3(s)));
  if (!func) {
    fprintf (stderr, "%s: could not find function %s in library #%ld\n",name,
	     LSTR(ARG3(s)),
	     LINTEGER(ARG1(s)));
    l = LispNewObj ();
    LTYPE(l) = S_BOOL;
    LBOOL(l) = 0;
    RETURN;
  }
  if (!LispAddDynamicFunc (LSTR(ARG2(s)), func)) {
    fprintf (stderr, "%s: error binding function %s\n",name, LSTR(ARG2(s)));
    l = LispNewObj ();
    LTYPE(l) = S_BOOL;
    LBOOL(l) = 0;
    RETURN;
  }
  l = LispNewObj ();
  LTYPE(l) = S_BOOL;
  LBOOL(l) = 1;
  return l;
}

