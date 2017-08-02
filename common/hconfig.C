/*************************************************************************
 *
 *  Copyright (c) 1999-2016 Rajit Manohar
 *
 *************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "lex.h"
#include "hconfig.h"
#include "hash.h"
#include "misc.h"

static struct Hashtable *H = NULL; // hashtable for config variables

struct helem {
  char *s;			// string
  int dint;			// default int
  float dfloat;			// default float
  char *ds;			// default string
  LL dLL;			// default LL
};


static struct helem *newhelem (void)
{
  struct helem *h;

  NEW (h, struct helem);
  h->s = NULL;
  h->dint = 0;
  h->dfloat = 0;
  h->ds = NULL;
  return h;
}

static struct helem *gethelem (const char *name)
{
  
  hash_bucket_t *b;
  struct helem *h;
  
  b = hash_lookup (H, name);
  if (!b) {
    b = hash_add (H, name);
    h = newhelem ();
    b->v = (char*)h;
  }
  else {
    h = (struct helem *)b->v;
  }
  return h;
}

static void Init (void)
{
  static int init_config = 0;
  if (!init_config) {
    H = hash_new (128);
    init_config = 1;
    Log::Initialize_LogLevel (NULL);
  }
}


/*
 *   ReadConfigFile --
 *
 *     Read parameters from configuration file and store in hash table.
 *     If name == NULL,  DEFAULT_CONFIG_FILE is used.
 *
 */
void ReadConfigFile (const char *name)
{
  LEX_T *l;
  int lbrack, rbrack, equal;
  char *prefix = NULL;
  char *s;
  int prefix_max;
  int nest;
  FILE *fp;
  struct helem *h;

  if (name == NULL) name = DEFAULT_CONFIG_FILE;

  Init ();

  /* open config file, initialize tokens */
  fp = fopen (name, "r");
  if (!fp) {
    fprintf (stderr, "WARNING: could not open configuration file `%s' for reading\n", name);
    return;
  }
  fclose (fp);
  l = lex_fopen (name);
  lex_setflags (l, LEX_FLAGS_HEXINT);
  lbrack = lex_addtoken (l, "{");
  rbrack = lex_addtoken (l, "}");
  equal  = lex_addtoken (l, "=");
  lex_getsym (l);		// read first symbol

  /* initialize prefix */
  prefix_max = 256;
  MALLOC (prefix, char, prefix_max);
  *prefix = '\0';
  nest = 0;
  
  while (!lex_eof (l)) {
    if (lex_sym (l) == l_id) {
      lex_getsym (l);
      if (lex_sym (l) == lbrack) {
	nest++;
	if (prefix_max < (signed)(strlen (lex_prev (l)) + strlen (prefix) + 2)) {
	  /* prefix ++ "." ++ id = len of each + 1 (for the \0) */
	  prefix_max = Max (2*prefix_max,
			    (signed)(strlen (lex_prev (l)) + strlen (prefix) + 2));
	  REALLOC (prefix, char, prefix_max);
	}
	/* Enough space here */
	if (*prefix) strcat (prefix, ".");
	strcat (prefix, lex_prev (l));
	lex_getsym (l);
	continue;
      }
      else if (lex_sym (l) == equal) {
	/* name = value pair */

	/* put "name" in the hash table */
	if (*prefix == '\0') {
	  /* no prefix. name is given by the previous token */
	  h = gethelem (lex_prev (l));
	  if (h->s) {
	    fprintf (stderr, "WARNING: redefining symbol `%s' in config file\n\t%s\n",
		     lex_prev (l), lex_errstring (l));
	    FREE (h->s);
	    h->s = NULL;
	  }
	}
	else {
	  /* name is given by prefix . previous token */
	  if (prefix_max < (signed)(strlen (lex_prev (l)) + strlen (prefix) + 2)) {
	    prefix_max = Max (2*prefix_max,
			      (signed)(strlen (lex_prev (l)) + strlen (prefix) + 2));
	    REALLOC (prefix, char, prefix_max);
	  }
	  strcat (prefix, ".");
	  strcat (prefix, lex_prev (l));
	  h = gethelem (prefix);
	  if (h->s) {
	    fprintf (stderr, "WARNING: redefining symbol `%s' in config file\n\t%s\n",
		     prefix, lex_errstring (l));
	    FREE (h->s);
	    h->s = NULL;
	  }
	  /* delete  the .xxx suffix just tacked on to the string */
	  s = prefix + strlen (prefix);
	  while (*s != '.')
	    s--;
	  *s = '\0';
	}

	lex_mustbe (l, equal);	// skip =

	int is_string = 0;

	is_string = (lex_sym (l) == l_string);

	lex_getsym (l);		// skip next symbol

	/* save next symbol */
	MALLOC (h->s, char, strlen (lex_prev (l)) + 1);
	if (is_string) {
	  strcpy (h->s, lex_prev(l)+1);
	  h->s[strlen(h->s)-1] = '\0';
	}
	else {
	  strcpy (h->s, lex_prev (l));
	}

	while (!lex_eof (l) && strcmp (lex_tokenstring (l), ";") == 0)
	  lex_getsym (l);
	/* skip ; if any */
      }
      else {
	fatal_error ("Reading config file, got %s, expected `{' or `='\n\t%s",
		     lex_tokenstring (l), lex_errstring (l));
      }
    }
    else if (lex_sym (l) == rbrack) {
      if (!nest) {
	fatal_error ("} without matching {\n\t%s", lex_errstring (l));
      }
      nest--;
      /* delete .xxx */
      s = prefix + strlen (prefix);
      while (s != prefix && *s != '.')
	s--;
      *s = '\0';
      lex_getsym (l);
      while (!lex_eof (l) && strcmp (lex_tokenstring (l), ";") == 0)
	lex_getsym (l);
    }
    else {
      fatal_error ("Reading config file, got %s, expected `id' or `}'\n\t%s",
		   lex_tokenstring (l), lex_errstring (l));
    }
  }
  if (nest != 0) {
    fprintf (stderr, "WARNING: reached end of config file, missing %d `}'\n",
	     nest);
  }
  FREE (prefix);
  lex_free (l);
  
  Log::Initialize_LogLevel (ParamGetString ("Log.Level"));
}


/*
 *   ParamGetString --
 *
 *      Read parameter, return as a string. Returns NULL if not defined.
 *
 */
char *ParamGetString (const char *name)
{
  struct helem *h;

  Init ();

  h = gethelem (name);
  if (h->s)
    return h->s;
  else
    return h->ds;
}


/*
 *  ParamGetInt --
 *
 *     Read parameter, return as an integer. Returns 0 if unknown
 *
 */
int ParamGetInt (const char *name)
{
  struct helem *h;
  int i;
  
  Init ();

  h = gethelem (name);
  if (h->s) {
    sscanf (h->s, "%i", &i);
    return i;
  }
  else return h->dint;
}


/*
 *  ParamGetFloat --
 *
 *    Read parameter, return as float. Returns 0 if unknown
 *
 */
float ParamGetFloat (const char *name)
{
  struct helem *h;
  
  Init ();

  h = gethelem (name);
  if (h->s) return atof(h->s);
  else return h->dfloat;
}

/*
 *  ParamGetBool --
 *
 *     Read parameter, return 1 if true, 0 if false. Returns 0 if unknown
 *
 */
int ParamGetBool (const char *name)
{
  char *str;
  struct helem *h;

  Init ();

  h = gethelem (name);
  if (h->s) {
     if (!strcasecmp(h->s, PARAM_YES))
	return 1;
     else
	return 0;
  }
  else return 0;
}

/*
 *  ParamGetLL --
 * 
 *     Read parameter, Returns 0 if unknown
 *
 */
LL ParamGetLL (const char *name)
{ 
  char *str, *ptr;
  struct helem *h;
  LL l;

  Init ();

  h = gethelem (name);

  if (h->s) {
#if defined(__FreeBSD__)
    l = strtouq(h->s, &ptr, 0);
     if (str != ptr)
	return l;
     else
	return h->dLL;
#elif defined (__alpha___)
     if (sscanf (h->s, "%li", &l) == 1)
       return l;
     else
       return h->dLL;
#endif
  }
  else {
     return h->dLL;
  }
}

/*
 *  RegisterDefault --
 *
 *    Set defaults for parameters
 *
 */
void RegisterDefault (const char *name, LL i)
{
  struct helem *h;

  Init ();

  h = gethelem (name);
  h->dLL = i;
}

void RegisterDefault (const char *name, int i)
{
  struct helem *h;

  Init ();

  h = gethelem (name);
  h->dint = i;
}


void RegisterDefault (const char *name, float f)
{
  struct helem *h;

  Init ();

  h = gethelem (name);
  h->dfloat = f;
}

void RegisterDefault (const char *name, char *s)
{
  struct helem *h;

  Init ();

  h = gethelem (name);
  if (h->ds) FREE (h->ds);
  MALLOC (h->ds, char, strlen (s)+1);
  strcpy (h->ds, s);
}

void OverrideConfig (const char *name, char *s)
{
  struct helem *h;

  Init ();

  h = gethelem (name);
  if (h->s) FREE (h->s);
  h->s = Strdup (s);
}
