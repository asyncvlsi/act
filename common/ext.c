/*************************************************************************
 *
 *  Parser for magic extract file format
 *
 *  Copyright (c) 1996-1998, 2018-2019 Rajit Manohar
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
#include <pwd.h>
#include <ctype.h>
#include <string.h>
#include "ext.h"
#include "lex.h"
#include "config.h"
#include "hash.h"
#include "misc.h"

#define MAXLINE 1024

static int num_devices = 0;
static char **device_names = NULL;

static int path_first_time = 1;

static
struct pathlist {
  char *path;
  struct pathlist *next;
} *hd = NULL, *tl;

static char *expand (char *s)
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
	/*fprintf (stderr, "WARNING: could not find user `%s' for path name expansion\n", s+1);*/
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


static
FILE *mag_path_open (const char *name, FILE **dumpfile)
{
  struct pathlist *p;
  char *file, *try;
  FILE *fp;

  if (dumpfile) {
    *dumpfile = NULL;
  }
  if (path_first_time) {
    addpath(Strdup("\".\""), 0);
    read_dotmagic (Strdup("~cad/lib/magic/sys/.magicrc"));
    read_dotmagic (Strdup("~/.magicrc"));
    read_dotmagic (Strdup(".magicrc"));
  }
  path_first_time = 0;
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
      if (dumpfile) {
	sprintf (try + strlen (try) - 3, "hxt");
	*dumpfile = fopen (try, "r");
      }
      FREE (try);
      return fp; 
    }
    strcat (try, ".ext");
    fp = fopen (try, "r");
    if (fp) {
      if (dumpfile) {
	sprintf (try + strlen(try)-3, "hxt");
	*dumpfile = fopen (try, "r");
      }
      FREE (try);
      return fp;
    }
    FREE (try);
    p = p->next;
  }
  fatal_error ("Could not find cell %s", name);
  return NULL;
}


/*
 *
 * Mixed integer/floating-point
 *
 */
static
double lex_mustbe_number (LEX_T *L)
{
  double x, y;
  x = lex_integer (L);
  y = lex_real (L);
  if (lex_have (L, l_integer))
    return x;
  else if (lex_have (L, l_real))
    return y;
  else
    fatal_error ("Expected number.");
  return 0;
}

static
char *lex_mustbe_string_id (LEX_T *L, const char *name, int line)
{
  char *s;
  if (lex_have (L, l_id)) {
    return lex_prev (L);
  }
  else if (lex_have (L, l_string)) {
    s = lex_prev (L);
    s[strlen(s)-1] = '\0';
    return s+1;
  }
  else {
    fatal_error ("Error in file %s:%d, expected string/id.", name, line);
    return NULL;
  }
}

static
char *lex_shouldbe_string_id (LEX_T *L)
{
  char *s;
  if (lex_have (L, l_id)) {
    return lex_prev (L);
  }
  else if (lex_have (L, l_string)) {
    s = lex_prev (L);
    s[strlen(s)-1] = '\0';
    return s+1;
  }
  else
    return NULL;
}


static
void lex_mustbe_string_contiguous_id (LEX_T *L)
{
  if (lex_have (L, l_string)) {
    ;
  }
  else if (lex_have (L, l_id)) {
    while (lex_whitespace (L)[0] == '\0' && !lex_eof (L)) {
      lex_getsym (L);
    }
  }
  else 
    fatal_error ("Error in file, expected string or id");
}


static
char *getint (char *s, int *i)
{
  char c;
  char *t = s;
  while (*s && *s != ':' && *s != ',' && *s != ']')
    s++;
  c  = *s;
  *s = '\0';
  *i = atoi (t);
  *s = c;
  return s;
}

/*------------------------------------------------------------------------
 *
 * addcap --
 *
 *   add capacitance
 *
 *------------------------------------------------------------------------
 */
static
void addcap (struct ext_file *ext, char *a, char *b, double cap, int type)
{
  struct ext_cap *c;
  MALLOC (c, struct ext_cap, 1);
  c->type = type;
  c->cap = cap*1e-18;		/* capacitance in aF; convert to F */
  c->n1 = a;
  c->n2 = b;
  c->next = ext->cap;
  ext->cap = c;
}

struct ext_ap *add_ap_empty (struct ext_file *ext, char *s)
{
  struct ext_ap *a;
  MALLOC (a, struct ext_ap, 1);
  a->node = s;
  a->perim = NULL;
  a->area = NULL;
  a->next = ext->ap;
  ext->ap = a;

  return a;
}

static
void addattr (struct ext_file *ext, char *n, char *attr)
{
  struct ext_attr *a;
  
  MALLOC (a, struct ext_attr, 1);
  a->n = n;
  a->attr = 0;
  if (strcmp (attr, "pchg") == 0) {
    a->attr |= EXT_ATTR_PCHG;
  }
  else if (strcmp (attr, "pup") == 0) {
    a->attr |= EXT_ATTR_PUP;
  }
  else if (strcmp (attr, "pdn") == 0) {
    a->attr |= EXT_ATTR_PDN;
  }
  else if (strcmp (attr, "nup") == 0) {
    a->attr |= EXT_ATTR_NUP;
  }
  else if (strcmp (attr, "ndn") == 0) {
    a->attr |= EXT_ATTR_NDN;
  }
  else if (strcmp (attr, "voltage_converter") == 0) {
    a->attr |= EXT_ATTR_VC;
  }
  a->next = ext->attr;
  ext->attr = a;
}

/*------------------------------------------------------------------------
 *
 *  Expands alias lists out
 *
 *------------------------------------------------------------------------
 */
static
void expand_aliases (char *a, char *b, struct ext_file *ext, double cap)
{
  char *s, *t;
  struct ext_alias *alias;
  char *sta, *stb;
  int i, j;
  int xrange, yrange;
  int xloa, yloa, xlob, ylob;
  int lena, lenb;

  lena = strlen (a) + 1;
  lenb = strlen (b) + 1;

  s = a;
  while (*s && *s != '[')
    s++;
  t = b;
  while (*t && *t != '[') t++;
  if (!*s || !*t) {
    MALLOC (alias, struct ext_alias, 1);
    alias->n1 = Strdup(a);
    alias->n2 = Strdup(b);
    alias->next = ext->aliases;
    ext->aliases = alias;
    if (cap != 0) 
      addcap (ext,Strdup(alias->n1), Strdup(alias->n2), cap, CAP_CORRECT);
  }
  else {
    sta = s+1;
    stb = t+1;
    *s = '\0';
    *t = '\0';
    yrange = 0;
    xrange = 0;

    s++;
    if (!*s) fatal_error ("Invalid merge line");

    s = getint (s, &xloa);
    if (*s == ':') {
      s++;
      s = getint (s, &i);
      xrange = i - xloa + 1;
    }
    if (*s == ',') {
      s++;
      s = getint (s, &yloa);
      if (*s == ':') {
	s++;
	s = getint (s, &i);
	yrange = i - yloa + 1;
      }
      else
	yrange = -1;
    }
    if (xrange == 0 && yrange <= 0) {
      *(sta-1) = '[';
      *(stb-1) = '[';
      MALLOC (alias, struct ext_alias, 1);
      alias->n1 = Strdup(a);
      alias->n2 = Strdup(b);
      alias->next = ext->aliases;
      ext->aliases = alias;
      if (cap != 0) 
	addcap (ext,
		Strdup(alias->n1), Strdup(alias->n2), cap, CAP_CORRECT);
      FREE (a);
      FREE (b);
      return;
    }
    if (xrange == 0) xrange = 1;
    if (yrange == -1) yrange = 1;

    if (*s != ']')
      fatal_error ("Error on merge line");
    s++;

    t++;
    if (!*t) fatal_error ("Invalid merge line");

    t = getint (t, &xlob);
    if (*t == ':') {
      t++;
      t = getint (t, &i);
      if (xrange != (i - xlob + 1))
	fatal_error ("Range check failed on merge line %s, %s", a, b);
    }
    else
      if (xrange != 1)
	fatal_error ("Range check failed on merge line %s, %s", a, b);
    if (*t == ',') {
      t++;
      t = getint (t, &ylob);
      if (*t == ':') {
	t++;
	t = getint (t, &i);
	if (yrange != (i - ylob + 1))
	  fatal_error ("Range check failed on merge line %s, %s", a, b);
      }
      else
	if (yrange != 1) fatal_error ("Range check failed on merge line %s, %s", a, b);
    }
    else
      if (yrange != 0) fatal_error ("Range check failed");
    if (*t != ']')
      fatal_error ("Error on merge line");
    t++;

    if (yrange == 0)
      for (i = 0; i < xrange; i++) {
	MALLOC (alias, struct ext_alias, 1);
	MALLOC (alias->n1, char, lena);
	sprintf (alias->n1, "%s[%d]%s", a, xloa+i, s);
	MALLOC (alias->n2, char, lenb);
	sprintf (alias->n2, "%s[%d]%s", b, xlob+i, t);
	alias->next = ext->aliases;
	ext->aliases = alias;
	if (cap != 0) 
	  addcap (ext,
		  Strdup(alias->n1), Strdup(alias->n2), cap, CAP_CORRECT);
      }
    else {
      for (i=0; i < xrange; i++)
	for (j=0; j < yrange; j++) {
	  MALLOC (alias, struct ext_alias, 1);
	  MALLOC (alias->n1, char, lena);
	  sprintf (alias->n1, "%s[%d,%d]%s", a, xloa+i, yloa+j, s);
	  MALLOC (alias->n2, char, lenb);
	  sprintf (alias->n2, "%s[%d,%d]%s", b, xlob+i, ylob+j, t);
	  alias->next = ext->aliases;
	  ext->aliases = alias;
	  if (cap != 0) 
	    addcap (ext,
		    Strdup(alias->n1), Strdup(alias->n2), cap,CAP_CORRECT);
	}
    }
  }
  FREE (a);
  FREE (b);
}


/*------------------------------------------------------------------------
 *
 *  Check .ext file timestamps
 *
 *------------------------------------------------------------------------
 */
static
void _check_ext_timestamp (FILE *fp, unsigned long tm)
{
  char buf[MAXLINE];
  char cell[MAXLINE];
  FILE *subcell, *sdump;
  char *s;
  unsigned long t;

  buf[MAXLINE-1] = '\n';
  buf[0]='\0';
  while (fgets (buf, MAXLINE, fp)) {
    if (buf[MAXLINE-1] == '\0')
      fatal_error ("This needs to be fixed");
    if (strncmp (buf, "use ", 4) == 0) {
      s = buf+4;
      while (*s && *s != ' ') s++;
      *s = '\0';
      strcat (buf, ".ext");
      subcell = mag_path_open (buf+4, &sdump);
      strcpy (cell, buf+4);
      if (sdump) fclose (sdump);
      if (!fgets (buf, MAXLINE, subcell) || 
	  strncmp (buf, "timestamp ", 10) != 0)
	fatal_error ("Subcell `%s' has incorrect .ext format", cell);
      sscanf (buf+10, "%lu", &t);
      if (t > tm) {
	warning ("`%s' is newer than parent", cell);
      }
      _check_ext_timestamp (subcell, t);
      fclose (subcell);
    }
  }
}


void
ext_validate_timestamp (const char *file)
{
  FILE *fp;
  static char buf[MAXLINE];
  unsigned long tm;

  fp = fopen (file, "r");
  if (!fp) {
    fp = mag_path_open (file, NULL);
  }
  if (!fp) {
    fatal_error ("File `%s' not found", file);
  }
  if (!fgets (buf, MAXLINE, fp) ||
      strncmp (buf, "timestamp ", 10) != 0)
    fatal_error ("Top level file has incorrect .ext format");
  sscanf (buf+10, "%lu", &tm);
  _check_ext_timestamp (fp, tm);
  fclose (fp);
}

/*------------------------------------------------------------------------
 *
 *  Parse .ext file hierarchically
 *
 *------------------------------------------------------------------------
 */
struct ext_file *ext_read (const char *name)
{
  FILE *fp, *dump;
  static char buf[MAXLINE];
  static char tok1[MAXLINE], tok2[MAXLINE];
  struct ext_file *ext = NULL;
  LEX_T *l;
  struct ext_fets *fet;
  struct ext_list *subcell;
  int l_comma;
  char *s, *t;
  int line = 0;
  unsigned long timestamp;
  double cscale; /*, rscale;*/
  double lscale;
  double x;
  static int depth = 0;
  static struct Hashtable *ehash;
  hash_bucket_t *eb;

  dump = NULL;
  fp = NULL;
  if (depth == 0) {
    ehash = hash_new (2);
    fp = fopen (name, "r");

    if (!device_names) {
      if (config_exists ("net.ext_devs")) {
	num_devices = config_get_table_size ("net.ext_devs");
	device_names = config_get_table_string ("net.ext_devs");
      }
    }
  }

  eb = hash_lookup (ehash, name);
  if (eb) {
    return (struct ext_file *)eb->v;
  }
  eb = hash_add (ehash, name);
  
  if (!fp) {
    fp = mag_path_open (name, &dump);
  }
  if (!fp) {
    fatal_error ("Could not find extract file for `%s'", name);
  }
  depth++;


  l = lex_string ("boo");
  l_comma = lex_addtoken (l, ",");

  MALLOC (ext, struct ext_file, 1);
  ext->fet = NULL;
  ext->subcells = NULL;
  ext->aliases = NULL;
  ext->mark = 0;
  ext->h = NULL;
  ext->cap = NULL;
  ext->attr = NULL;
  ext->ap = NULL;
  eb->v = ext;

  buf[MAXLINE-1] = '\n';

  if (fgets (buf, MAXLINE, fp)) {
    /* first line in ext file is the timestamp */
    l = lex_restring (l, buf);
    lex_getsym (l);
    lex_mustbe_keyw (l, "timestamp");
    sscanf (buf+10, "%lu", &ext->timestamp);
  }
  else {
    fclose (fp);
    if (dump) fclose (dump);
    lex_free (l);
    depth--;

    if (depth == 0) {
      hash_free (ehash);
      ehash = NULL;
    }
    return ext;
  }

  if (dump) {
    hash_bucket_t *b, *root;
    struct hier_cell_val *hc;
    if (fgets (buf, MAXLINE, dump)) {
      l = lex_restring (l, buf);
      lex_getsym (l);
      if (!lex_have_keyw (l, "timestamp")) {
	if (!lex_have_keyw (l, "timestampF")) {
	  fclose (dump);
	  warning ("summary file for `%s' not used [format err]", name);
	  goto readext;
	}
	warning ("summary file for `%s' may have unconnected globals", name);
      }
      sscanf (buf+11, "%lu", &timestamp);
      if (timestamp < ext->timestamp) {
	fclose (dump);
	warning ("summary file out-of-date for cell `%s'", name);
	goto readext;
      }
      fclose (fp);
      lex_free (l);
      l = lex_file (dump);
      lex_getsym (l);
      ext->h = hash_new (8);
      while (lex_sym (l) == l_id) {
	if (strcmp (lex_id (l), "i") == 0)
	  line = 1;
	else if (strcmp (lex_id (l), "o") == 0)
	  line = 0;
	else
	  fatal_error ("%s\n\t.hxt file corrupted!\n",
		       lex_errstring (l));
	lex_getsym (l);
	root = NULL;
	while (lex_sym (l) == l_string) {
	  lex_tokenstring(l)[strlen(lex_tokenstring(l))-1] = '\0';
	  if (!hash_lookup (ext->h, lex_tokenstring(l)+1)) {
	    b = hash_add (ext->h, lex_tokenstring(l)+1);
	    NEW (hc, struct hier_cell_val);
	    b->v = hc;
	    hc->root = root;
	    hc->flags = line ? HIER_IS_INPUT : 0;
	    if (!root)
	      root = b;
	  }
	  lex_getsym (l);
	}
      }
      lex_free (l);
      depth--;
      if (depth == 0) {
	hash_free (ehash);
	ehash = NULL;
      }
      return ext;
    }
    else {
      warning ("summary file for `%s' not used [empty]", name);
      fclose (dump);
    }
  }
  /* dump file is closed */
readext:
  cscale = 1;
  /*rscale = 1;*/
  lscale = 1e-8;		/* 1 centimicron */
  while (fgets (buf, MAXLINE, fp)) {
    line++;
    if (buf[MAXLINE-1] == '\0')
      fatal_error ("This needs to be fixed!");      /* FIXME */
    l = lex_restring (l, buf);
    lex_getsym (l);
    if (lex_have_keyw (l, "scale")) {
      /*rscale = */(void)(lex_mustbe_number (l));
      cscale = lex_mustbe_number (l);
      lscale = lex_mustbe_number (l)*1e-8;
    }
    else if (lex_have_keyw (l, "use")) {
      int i;
      MALLOC (subcell, struct ext_list, 1);
      sscanf (buf+4, "%s%s", tok1, tok2);
      MALLOC (subcell->file, char, strlen(tok1)+5);
      strcpy (subcell->file, tok1);
      strcat (subcell->file, ".ext");
      subcell->id = Strdup (tok2);
      subcell->xlo = 0; subcell->xhi = 0;
      subcell->ylo = 0; subcell->yhi = 0;
      for (s=subcell->id; *s; s++)
	if (*s == '[') {
	  *s = '\0';
	  t = s+1;
	  while (*t && *t != ':') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %s:%d, array syntax", name, line);
	  *t = '\0'; subcell->xlo = atoi (s+1);
	  s = t;
	  t++;
	  while (*t && *t != ':') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %s:%d, array syntax", name, line);
	  *t = '\0'; subcell->xhi = atoi (s+1);
	  s = t;
	  t++;
	  while (*t && *t != ']') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %s:%d, array syntax", name, line);
	  t++;
	  if (*t != '[')
	    fatal_error ("Error parsing line %s:%d, array syntax", name, line);
	  s = t;
	  while (*t && *t != ':') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %s:%d, array syntax", name, line);
	  *t = '\0'; subcell->ylo = atoi (s+1);
	  s = t;
	  t++;
	  while (*t && *t != ':') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %s:%d, array syntax", name, line);
	  *t = '\0'; subcell->yhi = atoi (s+1);
	  t++;
	  while (*t && *t != ']') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %s:%d, array syntax", name, line);
	  break;
	}
      /* 
       * Strip the subcircuit multiplier
       */
      i = strlen (subcell->id) - 1;
      subcell->mult = 1;
      while (i > 0) {
	if (subcell->id[i] == '/') break;
	if (subcell->id[i] == '*') {
	  subcell->id[i] = '\0';
	  sscanf (subcell->id + i + 1, "%f", &subcell->mult);
	  break;
	}
	i--;
      }
      subcell->next = ext->subcells;
      ext->subcells = subcell;
      subcell->ext = ext_read (subcell->file);
    }
    else if (lex_have_keyw (l, "device")) {
      if (lex_have_keyw (l, "mosfet")) {
	double gperim, t1perim, t2perim;
	int dev;

	MALLOC (fet, struct ext_fets, 1);
	if (device_names) {
	  for (dev = 0; dev < num_devices; dev++) {
	    if (lex_have_keyw (l, device_names[dev])) {
	      break;
	    }
	  }
	  if (dev == num_devices) {
	    fatal_error ("fet %s: unknown device type at %s:%d\n",
			 lex_tokenstring (l), name, line);
	  }
	  fet->type = dev;
	}
	else {
	  if (lex_have_keyw (l, "nfet"))
	    fet->type = EXT_FET_NTYPE;
	  else {
	    lex_mustbe_keyw (l, "pfet");
	    fet->type = EXT_FET_PTYPE;
	  }
	}
	/* coordinates; ignore */
	lex_mustbe_number (l); lex_mustbe_number (l); lex_mustbe_number (l);
	lex_mustbe_number (l);

	/*double dim1, dim2;*/

	/*dim1 = */(void)(lex_mustbe_number (l)*lscale);
	/*dim2 = */(void)(lex_mustbe_number (l)*lscale);

	/* substrate */
	fet->sub = Strdup (lex_mustbe_string_id (l, name, line));

	/* gate */
	fet->g = Strdup(lex_mustbe_string_id (l, name, line));

	gperim = lex_mustbe_number (l)*lscale; /* convert to SI units */
	fet->isweak = 0;
	if (strcmp (lex_tokenstring(l), "0") == 0)
	  lex_getsym (l);
	else
	  do {
	    lex_mustbe (l, l_string);
	    if (strcmp (lex_prev(l), "\"weak\"") == 0)
	      fet->isweak = 1;
	  } while (lex_have (l,l_comma));

	/* t1 */
	fet->t1 = Strdup(lex_mustbe_string_id (l, name, line));
	t1perim = lex_mustbe_number (l)*lscale; /* convert to SI units */
	if (strcmp (lex_tokenstring (l), "0") == 0)
	  lex_getsym (l);
	else
	  do lex_mustbe (l, l_string); while (lex_have (l,l_comma));

	/* t2 */
	fet->t2 = lex_shouldbe_string_id (l);
	if (!fet->t2) {
	  fatal_error ("fet in layout does not have enough terminals; t=%s; gate=%s", fet->t1, fet->g);
	}
	fet->t2 = Strdup(fet->t2);
	t2perim = lex_mustbe_number (l)*lscale; /* convert to SI unitS
						   */

	fet->width = (t1perim + 0.0 + t2perim)/2;
	fet->length = (gperim + 0.0)/2;

	//if (fet->length < 2) { fet->length = 2; }
	fet->next = ext->fet;
	ext->fet = fet;
      }
      /* XXX: other devices ignored right now */
    }
    else if (lex_have_keyw (l, "fet")) {
      double gperim, t1perim, t2perim;
      int dev;

      MALLOC (fet, struct ext_fets, 1);
      if (device_names) {
	for (dev = 0; dev < num_devices; dev++) {
	  if (lex_have_keyw (l, device_names[dev])) {
	    break;
	  }
	}
	if (dev == num_devices) {
	  fatal_error ("fet %s: unknown device type at %s:%d\n",
		       lex_tokenstring (l), name, line);
	}
	fet->type = dev;
      }
      else {
	if (lex_have_keyw (l, "nfet"))
	  fet->type = EXT_FET_NTYPE;
	else {
	  lex_mustbe_keyw (l, "pfet");
	  fet->type = EXT_FET_PTYPE;
	}
      }
      lex_mustbe_number (l); lex_mustbe_number (l); lex_mustbe_number (l);
      lex_mustbe_number (l); lex_mustbe_number (l); lex_mustbe_number (l);

      /* substrate */
      fet->sub = Strdup (lex_mustbe_string_id (l, name, line));

      /* gate */
      fet->g = Strdup(lex_mustbe_string_id (l, name, line));

      gperim = lex_mustbe_number (l)*lscale; /* convert to SI units */
      fet->isweak = 0;
      if (strcmp (lex_tokenstring(l), "0") == 0)
	lex_getsym (l);
      else
	do {
	  lex_mustbe (l, l_string);
	  if (strcmp (lex_prev(l), "\"weak\"") == 0)
	    fet->isweak = 1;
	} while (lex_have (l,l_comma));

      /* t1 */
      fet->t1 = Strdup(lex_mustbe_string_id (l, name, line));
      t1perim = lex_mustbe_number (l)*lscale; /* convert to SI units */
      if (strcmp (lex_tokenstring (l), "0") == 0)
	lex_getsym (l);
      else
	do lex_mustbe (l, l_string); while (lex_have (l,l_comma));

      /* t2 */
      fet->t2 = lex_shouldbe_string_id (l);
      if (!fet->t2) {
	fatal_error ("fet in layout does not have enough terminals; t=%s; gate=%s", fet->t1, fet->g);
      }
      fet->t2 = Strdup(fet->t2);
      t2perim = lex_mustbe_number (l)*lscale; /* convert to SI unitS */

      fet->width = (t1perim + t2perim)/2;
      fet->length = gperim/2;

      //if (fet->length < 2) { fet->length = 2; }
      fet->next = ext->fet;
      ext->fet = fet;
    }
    else if (lex_have_keyw (l, "equiv")) {
      s = Strdup(lex_mustbe_string_id (l, name, line));
      t = Strdup(lex_mustbe_string_id (l, name, line));
      expand_aliases (s, t, ext, 0);
    }
    else if (lex_have_keyw (l, "merge")) {
      s = Strdup(lex_mustbe_string_id (l, name, line));
      t = Strdup(lex_mustbe_string_id (l, name, line));
      if (lex_sym (l) == l_integer || lex_sym (l) == l_real)
	x = cscale*lex_mustbe_number (l);
      else
	x = 0;
      expand_aliases (s, t, ext, x);
    }
    else if (lex_have_keyw (l, "node") || lex_have_keyw (l, "substrate")) {
      struct ext_ap *ap;
      s = Strdup (lex_mustbe_string_id (l, name, line));
      lex_mustbe_number (l); /* R */
      x = lex_mustbe_number(l)*cscale; /* C */
      /* FIXME: resistclass 1 = ndiff, 2 = pdiff -- hardcoded */
      lex_mustbe_number (l); /* x */
      lex_mustbe_number (l); /* y */
      lex_mustbe_string_contiguous_id (l); /* type */

      addcap (ext, Strdup (s), NULL, x, CAP_GND);

      if (!lex_eof (l)) {
	ap = add_ap_empty (ext, Strdup (s));
	if (device_names) {
	  int j;
	  MALLOC (ap->area, double, num_devices);
	  MALLOC (ap->perim, double, num_devices);
	  for (j = 0; j < num_devices; j++) {
	    ap->area[j] = lex_mustbe_number(l)*lscale*lscale;
	    ap->perim[j] = lex_mustbe_number (l)*lscale;
	  }
	}
	else {
	  MALLOC (ap->area, double, 2);
	  MALLOC (ap->perim, double, 2);
	  ap->area[0] = lex_mustbe_number(l)*lscale*lscale;
	  ap->perim[0] = lex_mustbe_number (l)*lscale;

	  ap->area[1] = lex_mustbe_number(l)*lscale*lscale;
	  ap->perim[1] = lex_mustbe_number (l)*lscale;
	}
      }
      expand_aliases (s, Strdup(s), ext, 0);
    }
    else if (lex_have_keyw (l, "cap")) {
      s = Strdup (lex_mustbe_string_id (l, name, line));
      t = Strdup (lex_mustbe_string_id (l, name, line));
      x = lex_mustbe_number (l)*cscale;
      addcap (ext, s, t, x, CAP_INTERNODE);
    }
    else if (lex_have_keyw (l, "subcap")) {
      /* figure out what to do */
      s = Strdup (lex_mustbe_string_id (l, name, line));
      x = lex_mustbe_number (l)*cscale;
      addcap (ext, s, NULL, x, CAP_SUBSTRATE);
    }
    else if (lex_have_keyw (l, "attr")) {
      s = Strdup (lex_mustbe_string_id (l, name, line));
      while (lex_whitespace (l)[0] == '\0' && !lex_eof (l)) {
        int nlen = strlen(s)+strlen(lex_tokenstring(l))+1;
	REALLOC (s, char, nlen);
	strcat (s, lex_tokenstring (l));
	lex_getsym (l);
      }
      lex_mustbe_number (l);
      lex_mustbe_number (l);
      lex_mustbe_number (l);
      lex_mustbe_number (l);
      lex_mustbe_string_id (l, name, line);
      addattr (ext, s, lex_mustbe_string_id (l, name, line));
    }
  }
  fclose (fp);
  lex_free (l);
  depth--;
  if (depth == 0) {
    hash_free (ehash);
    ehash = NULL;
  }
  return ext;
}
