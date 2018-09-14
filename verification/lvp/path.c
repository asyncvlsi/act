/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#include "parse_ext.h"
#include "misc.h"
#include "lvs.h"
#include <pwd.h>
#include <ctype.h>

static int first = 1;

static
struct pathlist {
  char *path;
  struct pathlist *next;
} *hd = NULL, *tl;

static char *mystrdup (char *s)
{
  char *t;
  MALLOC (t, char, strlen(s)+1);
  strcpy (t, s);
  return t;
}

char *expand (char *s)
{
  char *path;
  char *t;
  char *ret;
  struct passwd *pwd;

  if (s[0] == '~' && *(s+1) == '/') {
    path = getenv ("HOME");
    if (!path) path = "";
    MALLOC (ret, char, strlen (s) + strlen(path) + 4);
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
      if (!pwd) {
	fprintf (stderr, "WARNING: could not find user `%s' for path name expansion\n", s+1);
	path = "";
      }
      else {
	if (pwd->pw_dir == NULL) path = "";
	else path = pwd->pw_dir;
      }
      endpwent ();
    }
    *t = '/';
    MALLOC (ret, char, strlen (t) + strlen (path) + 5);
    strcpy (ret, path);
    strcat (ret, t);
  }
  else {
    MALLOC (ret, char, strlen (s) + 5);
    strcpy (ret, s);
  }
  return ret;
}

static
int skipblanks (char *s, int i)
{
  while (s[i] && isspace (s[i]))
    i++;
  return i;
}

static
int addpath (char *s, int i)
{
  char *t;
  char c;

  if (s[i] == '"') i++;
  i--;
  do {
    i++;
    t = s+i;
    while (s[i] && !isspace(s[i]) && s[i] != '"' && s[i] != ':')
      i++;
    if (s[i]) {
      c = s[i];
      s[i] = '\0';
      if (!hd) {
	MALLOC (hd, struct pathlist, 1);
	tl = hd;
	tl->next = NULL;
      }
      else {
	MALLOC (tl->next, struct pathlist, 1);
	tl = tl->next;
	tl->next = NULL;
      }
      MALLOC (tl->path, char, strlen (t)+1);
      strcpy (tl->path, t);
      s[i] = c;
    }
  } while (s[i] == ':');
  if (s[i] == '"') i++;
  return i;
}

static
void read_dotmagic (char *file)
{
  FILE *fp;
  char buf[MAXLINE];
  int i;

  file = expand (file);
  fp = fopen (file, "r");
  FREE (file);
  if (!fp) return;
  buf[MAXLINE-1] = '\n';
  while (fgets (buf, MAXLINE, fp)) {
    if (buf[MAXLINE-1] == '\0') fatal_error ("FIX THIS!");
    i=-1;
    do {
      i++;
      i = skipblanks (buf, i);
      if (strncmp (buf+i, "path", 4) == 0) {
        hd = NULL;
        i+=4;
        i = skipblanks(buf,i);
        i = addpath (buf, i);
        i = skipblanks (buf, i);
      }
      else if (strncmp (buf+i, "addpath", 7) == 0) {
        i += 7;
        i = skipblanks(buf,i);
        i = addpath (buf, i);
        i = skipblanks (buf, i);
      }
      else {
	while (buf[i] && buf[i] != ';') {
	  if (buf[i] == '"') {
	    i++;
	    while (buf[i] && buf[i] != '"') {
	      if (buf[i] == '\\') {
		i++;
		if (!buf[i]) fatal_error ("Malformed input");
	      }
	      i++;
	    }
	  }
	  else if (buf[i] == '\'')
	    if (buf[i+1] && buf[i+2] == '\'')
	      i+=2;
	  i++;
	}
      }
    } while (buf[i] == ';');
  }
}


FILE *mag_path_open (char *name, FILE **dumpfile)
{
  struct pathlist *p;
  char *file, *try;
  FILE *fp;

  *dumpfile = NULL;
  if (first) {
    read_dotmagic (mystrdup("~cad/lib/magic/sys/.magic"));
    read_dotmagic (mystrdup("~/.magic"));
    read_dotmagic (mystrdup(".magic"));
  }
  first = 0;
  p = hd;
  while (p) {
    MALLOC (file, char, strlen (p->path)+strlen(name)+6);
    strcpy (file, p->path);
    strcat (file, "/");
    strcat (file, name);
    try = expand (file);
    FREE (file);
    fp = fopen (try, "r");
    if (fp) {
      sprintf (try + strlen (try) - 3, "hxt");
      *dumpfile = fopen (try, "r");
      FREE (try);
      return fp; 
    }
    strcat (try, ".ext");
    fp = fopen (try, "r");
    if (fp) {
      sprintf (try + strlen(try)-3, "hxt");
      *dumpfile = fopen (try, "r");
      FREE (try);
      return fp;
    }
    FREE (try);
    p = p->next;
  }
  fatal_error ("Could not find cell %s", name);
  return NULL;
}
