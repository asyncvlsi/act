/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011, 2019, 2021 Rajit Manohar
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
#include <common/misc.h>
#include "path.h"
#include "qops.h"

struct pathlist {
  char *path;
  struct pathlist *next;
};

struct path_info {
  struct pathlist *hd, *tl;
};

static int addpath (path_info_t *p, char *s)
{
  char *t;
  char c;
  struct pathlist *tmp;
  int i = 0;

  if (s[i] == '"') i++;
  i--;
  do {
    i++;
    t = s+i;
    while (s[i] && !isspace(s[i]) && s[i] != '"' && s[i] != ':')
      i++;
    if (t != s+i) {
      c = s[i];
      s[i] = '\0';

      NEW (tmp, struct pathlist);
      tmp->path = NULL;
      q_ins (p->hd, p->tl, tmp);
      
      MALLOC (tmp->path, char, strlen (t)+1);
      strcpy (tmp->path, t);
      s[i] = c;
    }
  } while (s[i] == ':');
  if (s[i] == '"') i++;
  return i;
}

static int rmpath (path_info_t *p, char *s)
{
  char *t;
  char c;
  struct pathlist *tmp, *prev;
  int i = 0;

  if (s[i] == '"') i++;
  i--;
  do {
    i++;
    t = s+i;
    while (s[i] && !isspace(s[i]) && s[i] != '"' && s[i] != ':')
      i++;
    if (t != s+i) {
      c = s[i];
      s[i] = '\0';

      prev = NULL;
      tmp = p->hd;
      while (tmp) {
	if (strcmp (tmp->path, t) == 0) {
	  struct pathlist *nxt = tmp->next;
	  q_delete_item (p->hd, p->tl, prev, tmp);
	  tmp = nxt;
	}
	else {
	  tmp = tmp->next;
	}
      }
      s[i] = c;
    }
  } while (s[i] == ':');
  if (s[i] == '"') i++;
  return i;
}


/*------------------------------------------------------------------------
 *
 *  Return empty path
 *
 *------------------------------------------------------------------------
 */
path_info_t *path_init (void)
{
  path_info_t *ret;
  NEW (ret, path_info_t);
  ret->hd = NULL;
  ret->tl = NULL;
  return ret;
}

static
char *expand (char *s)
{
  char *path;
  char *t;
  char *ret;
  struct passwd *pwd;

  if (s[0] == '~' && *(s+1) == '/') {
    path = getenv ("HOME");
    if (!path) path = "";
    MALLOC (ret, char, strlen (s) + strlen(path)+ 5);
    strcpy (ret, path);
    strcat (ret, s+1);
  }
  else if (s[0] == '~') {
    t = s+1;
    while (*t && *t != '/') t++;
    if (!*t) fatal_error ("Invalid pathname!");
    *t = '\0';
    if (strcmp (s+1, "cad") == 0 && getenv ("CAD_HOME"))
      path = getenv ("CAD_HOME");
    else {
      pwd  = getpwnam (s+1);
      if (pwd->pw_dir == NULL) path = "";
      else path = pwd->pw_dir;
    }
    *t = '/';
    MALLOC (ret, char, strlen (t) + strlen (path) + 6);
    strcpy (ret, path);
    strcat (ret, t);
  }
  else {
    MALLOC (ret, char, strlen (s) + 6);
    strcpy (ret, s);
  }
  return ret;
}


/*------------------------------------------------------------------------
 *
 *  path_open --
 *
 *    Search through the previously registered path list
 *
 *------------------------------------------------------------------------
 */
static char *_path_open (path_info_t *pi, const char *name, const char *ext,
			 int skip_cwd)
{
  struct pathlist *p;
  char *file, *try;
  FILE *fp;
  int ext_len;

  if (ext) {
    ext_len = strlen (ext);
  }
  else {
    ext_len = 0;
  }

  p = pi->hd;

  if (!skip_cwd) {
    char *tmp = Strdup (name);
    try = expand(tmp);
    FREE (tmp);
    if ((fp = fopen (try, "r"))) {
      fclose (fp);
      return try;
    }
    FREE (try);
  }
  while (p) {
    MALLOC (file, char, strlen (p->path)+strlen(name)+ext_len+2);
    strcpy (file, p->path);
    strcat (file, "/");
    strcat (file, name);
    try = expand (file);
    FREE (file);
    fp = fopen (try, "r");
    if (fp) { 
      fclose (fp);
      return try;
    }
    if (ext) {
      strcat (try, ext);
      fp = fopen (try, "r");
      if (fp) {
	fclose (fp);
	return try;
      }
    }
    FREE (try);
    p = p->next;
  }
  return Strdup (name);
}


/*------------------------------------------------------------------------
 *
 *  path_open --
 *
 *    Search through the previously registered path list
 *
 *------------------------------------------------------------------------
 */
char *path_open (path_info_t *pi, const char *name, const char *ext)
{
  return _path_open (pi, name, ext, 0);
}

char *path_open_skipcwd (path_info_t *pi, const char *name, const char *ext)
{
  return _path_open (pi, name, ext, 1);
}



/*------------------------------------------------------------------------
 *
 * path_add -- add paths
 *
 *------------------------------------------------------------------------
 */
void path_add (path_info_t *p, const char *s)
{
  char *tmp = Strdup (s);
  addpath (p, tmp);
  FREE (tmp);
  return;
}

/*------------------------------------------------------------------------
 *
 * path_add -- add paths
 *
 *------------------------------------------------------------------------
 */
void path_remove (path_info_t *p, const char *s)
{
  char *tmp = Strdup (s);
  rmpath (p, tmp);
  FREE (tmp);
  return;
}


void path_clear (path_info_t *p)
{
  struct pathlist *pi, *next;
  pi = p->hd;
  while (pi) {
    next = pi->next;
    FREE (pi->path);
    FREE (pi);
    pi = next;
  }
  p->hd = NULL;
  p->tl = NULL;
}


void path_free (path_info_t *p)
{
  path_clear (p);
  FREE (p);
}
