/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011, 2019 Rajit Manohar
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
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <ctype.h>
#include "act/common/misc.h"
#include "act/common/path.h"
#include "path.h"

static int first = 1;
static path_info_t *act_path = NULL;

/*
 * Initialize the search path
 */
static void _path_init (void)
{
  char buf[2];
  char *env;
  char *file;

  if (first) {
    act_path = path_init ();
    path_add (act_path, ".");
    
    /* next thing in the search path is $ACT_PATH */
    env = getenv ("ACT_PATH");
    if (env) {
      path_add (act_path, env);
    }
    
    /* last item in the search path is $ACT_HOME */
    env = getenv ("ACT_HOME");
    if (env) {
      MALLOC (file, char, strlen (env)+8);
      strcpy (file, env);
      strcat (file, "/act");
      path_add (act_path, file);
      FREE (file);
    }
  }
  first = 0;
}

/*------------------------------------------------------------------------
 *
 *  path_open --
 *
 *    Search through $ACT_PATH and $ACT_HOME/act to find a path name
 * where the file is located.
 *
 *------------------------------------------------------------------------
 */
char *act_path_open (char *name)
{
  _path_init ();
  return path_open (act_path, name, ".act");
}


/*--- import list ---*/

struct import_list {
  char *file;
  struct import_list *next;
};

static struct import_list *il = NULL;
static struct import_list *pending = NULL;

void act_push_import (char *file)
{
  struct import_list *t;

  NEW (t, struct import_list);
  t->file = Strdup (file);
  t->next = pending;
  pending = t;
}

int act_isimported (const char *file)
{
  struct import_list *t;

  for (t = il; t; t = t->next) {
    if (strcmp (t->file, file) == 0) return 1;
  }
  return 0;
}

void act_pop_import (char *file)
{
  struct import_list *t;
  
  Assert (strcmp (pending->file, file) == 0, "what?");

  t = pending;
  pending = pending->next;
  t->next = il;
  il = t;
}

int act_pending_import (char *file)
{
  struct import_list *t;

  for (t = pending; t; t = t->next) {
    if (strcmp (file, t->file) == 0) return 1;
  }
  return 0;
}

void act_print_import_stack (FILE *fp)
{
  struct import_list *t;

  fprintf (fp, "Import history:\t");
  for (t = il; t != pending; t = t->next) {
    if (t != il) {
      fprintf (fp, "\n\t<- ");
    }
    fprintf (fp, "%s", t->file);
  }
  if (il == NULL) {
    fprintf (fp, "<toplevel>");
  }
  fprintf (fp, "\n");
}
