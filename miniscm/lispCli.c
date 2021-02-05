/*************************************************************************
 *
 *  Copyright (c) 2020 Rajit Manohar
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
 *  lispCli.c --
 *
 *   This module contains a generic command-line interface
 *
 *************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>

#include <histedit.h>

#include "lisp.h"
#include "lispInt.h"
#include "lispargs.h"
#include "lispCli.h"
#include <array.h>


#define DEFAULT_PROMPT "cli> "

#if !defined(LIBEDIT_MAJOR) && !defined(H_SETSIZE)
#define OLD_LIBEDIT
#endif

static History *el_hist;
static EditLine *el_ptr;
static char *prompt_val;

static char *histrc_file;

static char *prompt_func (EditLine *el)
{
  return prompt_val;
}

/*-- two built-in commands:
  1. source <file>
  2. help <cmd>
  --*/

static struct LispCliCommand *cli_commands = NULL;
static int num_commands = 0;
static int use_editline;

/*------------------------------------------------------------------------
 *
 *  LispCliInit --
 *
 *   Initialize command-line interface and editline/history
 *
 *------------------------------------------------------------------------
 */
void LispCliInit (const char *elrc, const char *histrc, const char *prompt,
		  struct LispCliCommand *cmds, int cmd_len)
{
  HistEvent ev;

  use_editline = 1;

#ifdef OLD_LIBEDIT
  el_ptr = el_init ("editline", stdin, stdout);
#else
  el_ptr = el_init ("editline", stdin, stdout, stderr);
#endif

  el_set (el_ptr, EL_EDITOR, "emacs");
  
  /*-- read editline settings, if any --*/
  if (elrc) {
    el_source (el_ptr, elrc);
  }

  if (prompt) {
    prompt_val = Strdup (prompt);
  }
  else {
    prompt_val = Strdup (DEFAULT_PROMPT);
  }

  el_hist = history_init ();
  el_set (el_ptr, EL_PROMPT, prompt_func);

#ifdef OLD_LIBEDIT
  history (el_hist, &ev, H_EVENT, 1000);
#else
  history (el_hist, &ev, H_SETSIZE, 1000);
#endif

  if (histrc) {
    history (el_hist, &ev, H_LOAD, histrc);
    histrc_file = Strdup (histrc);
  }
  else {
    histrc_file = NULL;
  }

  el_set (el_ptr, EL_HIST, history, el_hist);
  el_set (el_ptr, EL_BIND, "^P", "ed-prev-history", NULL);
  el_set (el_ptr, EL_BIND, "^N", "ed-next-history", NULL);

  if (LispCliAddCommands (cmds, cmd_len) != cmd_len) {
    fatal_error ("Could not add initial command set!");
  }
  Assert (num_commands == cmd_len, "what?");
}


void LispCliInitPlain (const char *prompt,
		       struct LispCliCommand *cmds, int cmd_len)
{
  use_editline = 0;

  if (prompt) {
    prompt_val = Strdup (prompt);
  }
  else {
    prompt_val = Strdup (DEFAULT_PROMPT);
  }
  
  if (LispCliAddCommands (cmds, cmd_len) != cmd_len) {
    fatal_error ("Could not add initial command set!");
  }
  Assert (num_commands == cmd_len, "what?");
}


/*------------------------------------------------------------------------
 *
 *  LispCliEnd --
 *
 *   Terminate command-line interface
 *
 *------------------------------------------------------------------------
 */
void LispCliEnd (void)
{
  HistEvent ev;

  if (use_editline) {
    if (histrc_file) {
      history (el_hist, &ev, H_SAVE, histrc_file);
    }

    history (el_hist, &ev, H_END, NULL);
    el_end (el_ptr);
  }
}



/*------------------------------------------------------------------------
 *
 *  LispCliGetLine --
 *
 *   Read an input line
 *
 *------------------------------------------------------------------------
 */
static char *cli_getline (char *prompt)
{
  int count;
  HistEvent ev;
  const char *s;
  char *t;

  prompt_val = prompt;

  count = 0;
  while (count == 0) {
    s = el_gets (el_ptr, &count);
    if (!s) break;
  }   
  if (s && *s) {
    history (el_hist, &ev, H_ENTER, s);
  }
  if (s) {
    t = Strdup (s);
    if (count > 0 && t[count-1] == '\n') {
      t[count-1] = '\0';
    }
  }
  else {
    t = NULL;
  }
  return t;
}

struct file_stack {
  char *name;
  struct file_stack *next;
};

static struct file_stack *flist = NULL;

static
int in_stack (char *s)
{
  struct file_stack *fstk;

  for (fstk = flist; fstk; fstk = fstk->next)
    if (strcmp (fstk->name, s) == 0)
      return 1;
  return 0;
}

static
void push_file (char *s)
{
  struct file_stack *fstk;

  NEW (fstk, struct file_stack);
  fstk->next = flist;
  flist = fstk;
  fstk->name = Strdup (s);
  return;
}

static
void pop_file (void)
{
  struct file_stack *fstk;

  Assert (flist, "Yow");
  fstk = flist;
  flist = flist->next;
  free (fstk->name);
  free (fstk);
  return;
}

static int handle_source (int argc, char **argv);

static int lisp_return_value;

static int dispatch_command (int argc, char **argv)
{
  int i;

  if (strcmp (argv[0], "source") == 0) {
    return handle_source (argc, argv);
  }
  else if (strcmp (argv[0], "help") == 0) {
    if (argc == 1) {
      printf ("== Builtin ==\n");
      printf ("   help [cmd] - print help information/list commands\n");
      printf ("   source <filename> [<repeat-count>] - read in a script file\n");
      printf ("   exit - terminate\n");
      printf ("   quit - terminate\n");

      for (i=0; i < num_commands; i++) {
	if (!cli_commands[i].name) {
	  printf ("\n== %s ==\n", cli_commands[i].help);
	}
	else {
	  printf ("   %s\n", cli_commands[i].help);
	}
      }
      return 1;
    }
    else if (argc == 2) {
      if (strcmp (argv[1], "help") == 0) {
	printf ("   help [cmd] - print help information/list commands\n");
      }
      else if (strcmp (argv[1], "source") == 0) {
	printf ("   source <filename> [<repeat-count>] - read in a script file\n\n");
      }
      else {
	for (i=0; i < num_commands; i++) {
	  if (!cli_commands[i].name) continue;
	  if (strcmp (cli_commands[i].name, argv[1]) == 0) {
	    printf ("   %s\n", cli_commands[i].help);
	    break;
	  }
	}
	if (i == num_commands) {
	  fprintf (stderr, "Error: command `%s' not found\n", argv[1]);
	  return 0;
	}
      }
      return 1;
    }
    else {
      printf ("Usage: help [cmd]\n");
      return 0;
    }
  }
  
  for (i=0; i < num_commands; i++) {
    if (!cli_commands[i].name) continue;
    if (strcmp (cli_commands[i].name, argv[0]) == 0) {
      return (*cli_commands[i].f)(argc, argv);
    }
  }
  fprintf (stderr, "Unknown command name `%s'\n", argv[0]);
  return 0;
}


void LispSetReturnInt (int val)
{
  lisp_return_value = val;
}

int LispGetReturnInt (void)
{
  return lisp_return_value;
}

int LispDispatch (int argc, char **argv, int echo_cmd, int infile)
{
  int i;
  if (echo_cmd) {
    printf ("[cmd]");
    for (i=0; i < argc; i++) {
      printf (" %s", argv[i]);
    }
    printf ("\n");
  }
  return dispatch_command (argc, argv);
}


static void do_command (char *s)
{
  char *t;
  int i;
  A_DECL (char *, args);
  A_INIT (args);

  t = strtok (s, " \t");
  if (!t) return;
  if (*t == '#') return;
  if (strcmp (t, "quit") == 0 || strcmp (t, "exit") == 0) {
    t = strtok (NULL, " \t");
    if (t && (*t != '#')) {
      fprintf (stderr, "Trailing garbage ignored, terminating anyway\n");
    }
    LispCliEnd ();
    exit (0);
  }

  A_NEW (args, char *);
  A_NEXT (args) = t;
  A_INC (args);

  do {
    t = strtok (NULL, " \t");
    if (t) {
      A_NEW (args, char *);
      A_NEXT (args) = t;
      A_INC (args);
    }
  } while (t);

  LispEvaluate (A_LEN (args), args, flist ? 1 : 0);
}


static char *read_input_line (FILE *fp, char *buf, int len)
{
  char *s;

  if (fp == stdin && isatty (0)) {
    if (!use_editline) {
      printf ("%s", prompt_val);
      fflush (stdout);
      if (fgets (buf, len, fp)) {
	len = strlen (buf);
	if (buf[len-1] == '\n') {
	  buf[len-1] = '\0';
	}
	return buf;
      }
      else {
	return NULL;
      }
    }
    else {
      s = cli_getline (prompt_val);
      return s;
    }
  }
  else {
    if (fgets (buf, len, fp)) {
      len = strlen (buf);
      if (buf[len-1] == '\n') {
	buf[len-1] = '\0';
      }
      return buf;
    }
    else {
      return NULL;
    }
  }
}

static int handle_user_input (FILE *fp)
{
  char buf[10240];
  char *s, *t;

  while ((s = read_input_line (fp, buf, 10240))) {
    if (LispInterruptExecution) {
      return 0;
    }
    else {
      if (*s) {
	do_command (s);
	if (LispInterruptExecution) {
	  return 0;
	}
      }
      if (s != buf) {
	FREE (s);
      }
      fflush (stdout);
      fflush (stderr);
    }
  }
  return 1;
}

static int handle_source (int argc, char **argv)
{
  FILE *fp;
  char *fname;
  int count;
    
  if (argc != 2 && argc != 3) {
    fprintf (stderr, "Usage: source <filename> [<repeat-count>]\n");
    return 0;
  }

  fname = Strdup (argv[1]);
  if (argc == 3) {
    count = atoi (argv[2]);
  }
  else {
    count = 1;
  }

  while (!LispInterruptExecution && (count > 0)) {
    count--;
    if (in_stack (fname)) {
      fprintf (stderr, "Error: recursive source command for file `%s'\n",
	       fname);
      return 0;
    }
    push_file (fname);
    Assert (flist, "Hmm");
    fp = fopen (flist->name, "r");
    if (!fp) {
      fprintf (stderr, "Error: could not open file `%s' for reading\n",
	       flist->name);
      return 0;
    }
    handle_user_input (fp);
    fclose (fp);
    pop_file ();
  }
  if (LispInterruptExecution) {
    fprintf (stderr, "\t*** interrupted source command `%s'\n", fname);
  }
  FREE (fname);
  if (LispInterruptExecution) {
    return 0;
  }
  else {
    return 1;
  }
}


/*------------------------------------------------------------------------
 *
 *  LispCliRun --
 *
 *   Run the command-line interface loop.
 *  
 *   Return 1 if successful, 0 if interrupted
 *
 *------------------------------------------------------------------------
 */
int LispCliRun (FILE *inp)
{
  return handle_user_input (inp);
}

/*------------------------------------------------------------------------
 *
 *  LispCliSetPrompt --
 *
 *   Set the prompt to a new string
 *
 *------------------------------------------------------------------------
 */
void LispCliSetPrompt (const char *s)
{
  if (prompt_val) {
    FREE (prompt_val);
  }
  if (s) {
    prompt_val = Strdup (s);
  }
  else {
    prompt_val = Strdup (DEFAULT_PROMPT);
  }
}


/*------------------------------------------------------------------------
 *
 *  LispCliAddCommands --
 *
 *   Add new commands to the command-line interface
 *
 *------------------------------------------------------------------------
 */
int LispCliAddCommands (struct LispCliCommand *cmd, int cmd_len)
{
  int i, j;
  
  if (cmd_len <= 0) return cmd_len;

  if (!cli_commands) {
    MALLOC (cli_commands, struct LispCliCommand, cmd_len);
  }
  else {
    REALLOC (cli_commands, struct LispCliCommand, cmd_len + num_commands);
  }
  for (i=0; i < cmd_len; i++) {
    if (cmd[i].name) {
      for (j=0; j < num_commands; j++) {
	if (!cli_commands[j].name) continue;
	if (strcmp (cli_commands[j].name, cmd[i].name) == 0) {
	  warning ("Duplicate command name `%s'", cmd[i].name);
	  return i;
	}
      }
      if (strcmp (cmd[i].name, "help") == 0 ||
	  strcmp (cmd[i].name, "exit") == 0 ||
	  strcmp (cmd[i].name, "quit") == 0 ||
	  strcmp (cmd[i].name, "source") == 0) {
	  warning ("Reserved command name `%s'", cmd[i].name);
	  return i;
      }
    }
    cli_commands[num_commands] = cmd[i];
    num_commands++;
  }
  return i;
}
