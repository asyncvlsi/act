/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#include "parse_ext.h"
#include "lex.h"
#include "misc.h"
#include "var.h"
#include "lvs.h"

#define MAXLINE 1024

static char *mystrdup (char *s)
{
  char *t;
  MALLOC (t, char, strlen(s)+1);
  strcpy (t, s);
  return t;
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
char *lex_mustbe_string_id (LEX_T *L)
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
    fatal_error ("Error in file, expected string/id.");
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
  char *s;

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

static void add_ap (struct ext_file *ext, char *s, double p_a, double p_p,
		    double n_a, double n_p)
{
  struct ext_ap *a;
  MALLOC (a, struct ext_ap, 1);
  a->node = s;
  a->p_perim = p_p;
  a->p_area = p_a;
  a->n_perim = n_p;
  a->n_area = n_a;
  a->next = ext->ap;
  ext->ap = a;
}

static
void addattr (struct ext_file *ext, char *n, char *attr)
{
  struct ext_attr *a;
  
  MALLOC (a, struct ext_attr, 1);
  a->n = n;
  a->attr = 0;
#define ATTR(p,q) if (strcmp (attr,p) == 0) { a->attr |= q; goto attrdone; }
#include "attr.def"
#undef ATTR
attrdone:
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
    alias->n1 = mystrdup(a);
    alias->n2 = mystrdup(b);
    alias->next = ext->aliases;
    ext->aliases = alias;
    if (cap != 0) 
      addcap (ext,mystrdup(alias->n1), mystrdup(alias->n2), cap, CAP_CORRECT);
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
      alias->n1 = mystrdup(a);
      alias->n2 = mystrdup(b);
      alias->next = ext->aliases;
      ext->aliases = alias;
      if (cap != 0) 
	addcap (ext,
		mystrdup(alias->n1), mystrdup(alias->n2), cap, CAP_CORRECT);
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
		  mystrdup(alias->n1), mystrdup(alias->n2), cap, CAP_CORRECT);
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
		    mystrdup(alias->n1), mystrdup(alias->n2), cap,CAP_CORRECT);
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
	if (warnings)
	  exit_status = 1;
	pp_printf (PPout, 
		   "WARNING: Subcell `%s' is newer than parent",
		   cell);
	pp_forced (PPout, 0);
      }
      _check_ext_timestamp (subcell, t);
      fclose (subcell);
    }
  }
}


void
check_ext_timestamp (FILE *fp)
{
  static char buf[MAXLINE];
  unsigned long tm;

  if (!fgets (buf, MAXLINE, fp) ||
      strncmp (buf, "timestamp ", 10) != 0)
    fatal_error ("Top level file has incorrect .ext format");
  sscanf (buf+10, "%lu", &tm);
  _check_ext_timestamp (fp, tm);
  fseek (fp, 0, 0 /* SEEK_SET */);
}

/*------------------------------------------------------------------------
 *
 *  Parse .ext file hierarchically
 *
 *------------------------------------------------------------------------
 */
struct ext_file *parse_ext_file (FILE *fp, FILE *dump, char *path)
{
  static char buf[MAXLINE];
  static char tok1[MAXLINE], tok2[MAXLINE];
  struct ext_file *ext = NULL;
  LEX_T *l;
  struct ext_fets *fet;
  struct ext_list *subcell;
  int l_comma;
  char *s, *t;
  int line = 0;
  FILE *tmp1, *tmp2;
  unsigned long timestamp;
  unsigned long fpos;
  double cscale;
  double lscale;
  double x;
  double n_a, n_p, p_a, p_p;

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
    return ext;
  }

  if (dump) {
    struct hash_cell *hc, *root;
    if (fgets (buf, MAXLINE, dump)) {
      l = lex_restring (l, buf);
      lex_getsym (l);
      if (!lex_have_keyw (l, "timestamp")) {
	if (!lex_have_keyw (l, "timestampF")) {
	  fclose (dump);
	  if (warnings)
	    exit_status = 1;
	  if (path)
	    pp_printf (PPout, "WARNING: summary file for `%s' not used [format err]", path);
	  else
	    pp_printf (PPout, "WARNING: summary file for top cell not used [format err]");
	  pp_forced (PPout, 0);
	  goto readext;
	}
	if (warnings)
	  exit_status = 1;
	if (path)
	  pp_printf (PPout, "WARNING: summary file for `%s' may have unconnected globals", path);
	else
	  pp_printf (PPout, "WARNING: summary file for top cell may have unconnected globals");
	pp_forced (PPout, 0);
      }
      sscanf (buf+11, "%lu", &timestamp);
      if (timestamp < ext->timestamp) {
	fclose (dump);
	if (path)
	  pp_printf (PPout, "WARNING: summary file out-of-date for cell `%s'",
		     path);
	else
	  pp_printf (PPout, "WARNING: summary file out-of-date for top cell");
	pp_forced (PPout, 0);
	goto readext;
      }
      fclose (fp);
      lex_free (l);
      l = lex_file (dump);
      lex_getsym (l);
      ext->h = hier_create (16);
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
	  if (!hier_find (ext->h, lex_tokenstring(l)+1)) {
	    hc = hier_add (ext->h, lex_tokenstring(l)+1, line);
	    if (!root) 
	      root = hc;
	    else
	      hc->root = root;
	  }
	  lex_getsym (l);
	}
      }
      lex_free (l);
      return ext;
    }
    else {
      if (warnings)
	exit_status = 1;
      if (path)
	pp_printf (PPout, "WARNING: summary file for `%s' not used [empty]", path);
	else
	  pp_printf (PPout, "WARNING: summary file for top cell not used [empty]");
      pp_forced (PPout, 0);
      fclose (dump);
    }
  }
  /* dump file is closed */
readext:
  cscale = 1;
  lscale = 1e-8/lambda;		/* 1 centimicron / lambda */
  while (fgets (buf, MAXLINE, fp)) {
    line++;
    if (buf[MAXLINE-1] == '\0')
      fatal_error ("This needs to be fixed!");      /* FIXME */
    l = lex_restring (l, buf);
    lex_getsym (l);
    if (lex_have_keyw (l, "scale")) {
      lex_mustbe (l, l_integer);
      cscale = lex_integer (l);
      lex_mustbe (l, l_integer);
      lscale = lex_integer (l)*1e-8/lambda;
      lex_mustbe (l, l_integer);
    }
    else if (lex_have_keyw (l, "use")) {
      int i;
      MALLOC (subcell, struct ext_list, 1);
      sscanf (buf+4, "%s%s", tok1, tok2);
      /*subcell->file = mystrdup (tok1);*/
      MALLOC (subcell->file, char, strlen(tok1)+5);
      strcpy (subcell->file, tok1);
      strcat (subcell->file, ".ext");
      subcell->id = mystrdup (tok2);
      subcell->xlo = 0; subcell->xhi = 0;
      subcell->ylo = 0; subcell->yhi = 0;
      for (s=subcell->id; *s; s++)
	if (*s == '[') {
	  *s = '\0';
	  t = s+1;
	  while (*t && *t != ':') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %d, array syntax", line);
	  *t = '\0'; subcell->xlo = atoi (s+1);
	  s = t;
	  t++;
	  while (*t && *t != ':') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %d, array syntax", line);
	  *t = '\0'; subcell->xhi = atoi (s+1);
	  s = t;
	  t++;
	  while (*t && *t != ']') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %d, array syntax", line);
	  t++;
	  if (*t != '[')
	    fatal_error ("Error parsing line %d, array syntax", line);
	  s = t;
	  while (*t && *t != ':') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %d, array syntax", line);
	  *t = '\0'; subcell->ylo = atoi (s+1);
	  s = t;
	  t++;
	  while (*t && *t != ':') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %d, array syntax", line);
	  *t = '\0'; subcell->yhi = atoi (s+1);
	  t++;
	  while (*t && *t != ']') t++;
	  if (!*t)
	    fatal_error ("Error parsing line %d, array syntax", line);
	  break;
	}
      /* 
       * Strip the subcircuit multiplier
       */
      i = strlen (subcell->id) - 1;
      while (i > 0) {
	if (subcell->id[i] == '/') break;
	if (subcell->id[i] == '*') { subcell->id[i] = '\0'; break; }
	i--;
      }
      subcell->next = ext->subcells;
      ext->subcells = subcell;
      tmp1 = mag_path_open (subcell->file, &tmp2);
      subcell->ext = parse_ext_file (tmp1, tmp2, subcell->file);
    }
    else if (lex_have_keyw (l, "fet")) {
      double gperim, t1perim, t2perim;

      MALLOC (fet, struct ext_fets, 1);
      if (lex_have_keyw (l, "nfet"))
	fet->type = N_TYPE;
      else {
	lex_mustbe_keyw (l, "pfet");
	fet->type = P_TYPE;
      }
      lex_mustbe_number (l); lex_mustbe_number (l); lex_mustbe_number (l);
      lex_mustbe_number (l); lex_mustbe_number (l); lex_mustbe_number (l);

      /* substrate */
      lex_mustbe_string_id (l);

      /* gate */
      fet->g = mystrdup(lex_mustbe_string_id (l));

      gperim = lex_mustbe_number (l)*lscale; /* convert to lambda */
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
      fet->t1 = mystrdup(lex_mustbe_string_id (l));
      t1perim = lex_mustbe_number (l)*lscale; /* convert to lambda */
      if (strcmp (lex_tokenstring (l), "0") == 0)
	lex_getsym (l);
      else
	do lex_mustbe (l, l_string); while (lex_have (l,l_comma));

      /* t2 */
      fet->t2 = lex_shouldbe_string_id (l);
      if (!fet->t2) {
	fatal_error ("fet in layout does not have enough terminals; t=%s; gate=%s", fet->t1, fet->g);
      }
      fet->t2 = mystrdup(fet->t2);
      t2perim = lex_mustbe_number (l)*lscale; /* convert to lambda */

      fet->width = (t1perim + t2perim)/2;
      fet->length = gperim/2;

      if (fet->length < 2) { fet->length = 2; }
      fet->next = ext->fet;
      ext->fet = fet;
    }
    else if (lex_have_keyw (l, "equiv")) {
      s = mystrdup(lex_mustbe_string_id (l));
      t = mystrdup(lex_mustbe_string_id (l));
      expand_aliases (s, t, ext, 0);
    }
    else if (lex_have_keyw (l, "merge")) {
      s = mystrdup(lex_mustbe_string_id (l));
      t = mystrdup(lex_mustbe_string_id (l));
      if (lex_sym (l) == l_integer || lex_sym (l) == l_real)
	x = cscale*lex_mustbe_number (l);
      else
	x = 0;
      expand_aliases (s, t, ext, x);
    }
    else if (lex_have_keyw (l, "node")) {
      s = mystrdup (lex_mustbe_string_id (l));
      lex_mustbe_number (l); /* R */
      x = lex_mustbe_number(l)*cscale; /* C */
      /* FIXME: resistclass 1 = ndiff, 2 = pdiff -- hardcoded */
      lex_mustbe_number (l); /* x */
      lex_mustbe_number (l); /* y */
#if 0
      lex_mustbe_string_id (l); /* type */
#endif
      lex_mustbe_string_contiguous_id (l); /* type */
#if 0
      x += lex_mustbe_number(l)*lscale*lscale*N_diff_cap; /* a1 */
      lex_mustbe_number (l); /* p1 */
      x += lex_mustbe_number(l)*lscale*lscale*P_diff_cap; /* a2 */
      addcap (ext, mystrdup (s), NULL, x, CAP_GND);
#endif

#ifndef DIGITAL_ONLY
      n_a = lex_mustbe_number(l)*(lscale*lambda)*(lscale*lambda);
      n_p = lex_mustbe_number (l)*(lscale*lambda);

      p_a = lex_mustbe_number(l)*(lscale*lambda)*(lscale*lambda);
      p_p = lex_mustbe_number (l)*(lscale*lambda);

      addcap (ext, mystrdup (s), NULL, x, CAP_GND);
      add_ap (ext, mystrdup (s), p_a, p_p, n_a, n_p);
#endif
      expand_aliases (s, mystrdup(s), ext, 0);
    }
    else if (lex_have_keyw (l, "cap")) {
#ifndef DIGITAL_ONLY
      s = mystrdup (lex_mustbe_string_id (l));
      t = mystrdup (lex_mustbe_string_id (l));
      x = lex_mustbe_number (l)*cscale;
      addcap (ext, s, t, x, CAP_INTERNODE);
#endif
    }
    else if (lex_have_keyw (l, "attr")) {
      s = mystrdup (lex_mustbe_string_id (l));
      while (lex_whitespace (l)[0] == '\0' && !lex_eof (l)) {
	REALLOC (s, char, strlen(s)+strlen(lex_tokenstring(l))+1);
	strcat (s, lex_tokenstring (l));
	lex_getsym (l);
      }
      lex_mustbe_number (l);
      lex_mustbe_number (l);
      lex_mustbe_number (l);
      lex_mustbe_number (l);
      lex_mustbe_string_id (l);
      addattr (ext, s, lex_mustbe_string_id (l));
    }
  }
  fclose (fp);
  lex_free (l);
  return ext;
}
