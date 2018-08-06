/*************************************************************************
 *
 *  Copyright (c) 2009-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "hash.h"
#include "misc.h"
#include "array.h"

/*
  Configuration file format:

  include "string" -- reads this in
  int    <var> value
  string <var> value
  real   <var> value
  int_table <var> values
  real_table <var> values
  string_table <var> values

  begin name -- appends "name." as a prefix
  end  -- drops last prefix
  
*/

enum {
  CONFIG_INT,
  CONFIG_STR,
  CONFIG_REAL,
  CONFIG_TABLE_INT,
  CONFIG_TABLE_STR,
  CONFIG_TABLE_REAL
};

typedef struct {
  int type;
  union {
    char *s;
    int i;
    double r;
    struct {
      union {
	char **s;
	int *i;
	double *r;
      } u;
      int sz;
    } t;
  } u;
} config_t;

static struct Hashtable *H = NULL;

static struct search_path {
  char *path;
  struct search_path *next;
} *Path = NULL;

L_A_DECL (char *, files_read);
L_A_DECL (char, global_prefix);

/*------------------------------------------------------------------------
 *
 *  config_append_path --
 *
 *   Add to the search path for the configuration file
 *
 *------------------------------------------------------------------------
 */
void config_append_path (const char *s)
{
  struct search_path *p;
  if (!s) return;
  
  NEW (p, struct search_path);
  p->next = Path;
  p->path = Strdup (s);
  Path = p;
}

/*------------------------------------------------------------------------
 *
 *  config_std_path --
 *
 *   Add standard config path
 *
 *------------------------------------------------------------------------
 */
void config_std_path (const char *tool)
{
  char buf[10240];

  if (getenv ("CAD_HOME")) {
    sprintf (buf, "%s/lib/%s/", getenv ("CAD_HOME"), tool);
    config_append_path (buf);
  }
  if (getenv ("ACT_HOME")) {
    sprintf (buf, "%s/lib/%s/", getenv ("ACT_HOME"), tool);
    config_append_path (buf);
  }
  config_append_path (".");
}

/*------------------------------------------------------------------------
 *
 *  config_push_prefix --
 *
 *   add prefix to the name. used when a tool uses multiple
 *   configuration files.
 *
 *------------------------------------------------------------------------
 */
void config_push_prefix (const char *s)
{
  A_NEWP (global_prefix, char, strlen (s) + 2);
  if (A_LEN (global_prefix) > 0) {
    A_APPEND (global_prefix, char, '.');
  }
  while (*s) {
    A_APPEND (global_prefix, char, *s);
    s++;
  }
  A_NEXT (global_prefix) = '\0';
}

/*------------------------------------------------------------------------
 *
 *  config_pop_prefix --
 *
 *   Drop last prefix 
 *
 *------------------------------------------------------------------------
 */
void config_pop_prefix (void)
{
  int i;

  if (A_LEN (global_prefix) == 0) {
    fatal_error ("config_pop_prefix() called too many times");
  }
  i = A_LEN(global_prefix)-1;
  while (i > 0 && global_prefix[i] != '.') {
    i--;
  }
  A_LEN (global_prefix) = i;
  global_prefix[i] = '\0';
}

/*------------------------------------------------------------------------
 *
 *  config_read --
 *
 *   Read configuration file
 *
 *------------------------------------------------------------------------
 */
void config_read (const char *name)
{
  FILE *fp;
  char buf[10240];
  char buf2[10240];
  char buf3[10240];
  char *s;
  hash_bucket_t *b;
  config_t *c;
  int line = 0;
  struct search_path *p;
  static int level = 0;
  int i;
  char *prefix = NULL;
  int prefix_len = 0;
  int initial_phase = 1;

  if (level == 0) {
    A_INIT (files_read);
  }

  for (i=0; i < A_LEN (files_read); i++) {
    if (strcmp (name, files_read[i]) == 0) {
      return;
    }
  }

  A_NEW (files_read, char *);
  A_NEXT (files_read) = Strdup (name);
  A_INC (files_read);

  level++;

  if (!Path) {
    config_append_path (".");
  }
  p = Path;

  fp = NULL;
  if (*name == '/') {
    /* absolute path name */
    fp = fopen (name, "r");
  }
  else {
    while ((fp == NULL) && (p != NULL)) {
      sprintf (buf, "%s/%s", p->path, name);
      fp = fopen (buf, "r");
      p = p->next;
    }
  }
  if (!fp) {
    fatal_error ("Could not open configuration file `%s' for reading.", name);
  }

  if (!H) {
    H = hash_new (8);
  }

  MALLOC (prefix, char, 1024);
  prefix_len = 1024;
  prefix[0] = '\0';

#define RAW_GET_NEXT						\
  do {								\
    s = strtok (NULL, " \t");					\
    if (!s) fatal_error ("Invalid format [%s:%d]", name, line);	\
  } while (0)

#define GET_NAME							\
  do {									\
    initial_phase = 0;							\
    RAW_GET_NEXT;							\
    if (A_LEN (global_prefix) + strlen (prefix) + 1 + strlen (s) > 10240) { \
      fatal_error ("Names are too long [%s:%d]", name, line);		\
    }									\
    if (A_LEN (global_prefix) > 0) {					\
      sprintf (buf3, "%s.%s%s", global_prefix, prefix, s);		\
    }									\
    else {								\
      sprintf (buf3, "%s%s", prefix, s);				\
    }									\
  } while (0)

  buf[0] = '\0';
  buf[10239] = '\0';
  while (fgets (buf, 10240, fp)) {
    line++;
    if (buf[10239] != '\0') {
      fatal_error ("Line too long [%s:%d]!", name, line);
    }
    buf[strlen(buf)-1] = '\0';
    if (buf[0] == '#' || buf[0] == '\0') continue;
    s = strtok (buf, " \t");
    if (!s || !*s || s[0] == '#') continue;

    if (strcmp (s, "include") == 0) {
      if (!initial_phase) {
	fatal_error ("`include' directive is only permitted at the start of the file, before any definitions");
      }
      RAW_GET_NEXT;
      if (s[0] != '"' || s[strlen(s)-1] != '"') {
	fatal_error ("String on [%s:%d] needs to be of the form \"...\"", name, line);
      }
      s[strlen(s)-1] = '\0';
      config_read (s+1);
    }
    else if (strcmp (s, "int") == 0) {
      GET_NAME;
      if (!(b = hash_lookup (H, buf3))) {
	b = hash_add (H, buf3);
      }
      else {
	Assert (((config_t*)b->v)->type == CONFIG_INT, "Switching types!?");
	FREE (b->v);
      }
      NEW (c, config_t);
      c->type = CONFIG_INT;
      s = strtok (NULL, " \t");
      if (!s) fatal_error ("Invalid format [%s:%d]", name, line);
      sscanf (s, "%d", &c->u.i);
      b->v = c;
    }
    else if (strcmp (s, "string") == 0) {
      GET_NAME;
      if (!(b = hash_lookup (H, buf3))) {
	b = hash_add (H, buf3);
      }
      else {
	Assert (((config_t*)b->v)->type == CONFIG_STR, "Switching types!?");
	FREE (((config_t*)b->v)->u.s);
	FREE (b->v);
      }
      NEW (c, config_t);
      c->type = CONFIG_STR;

      s = strtok (NULL, " \t");
      if (!s) fatal_error ("Invalid format [%s:%d]", name, line);

     
      if (s[0] != '"') {
	fatal_error ("String on [%s:%d] needs to be of the form \"...\"", name, line);
      }

      strcpy (buf2, s);

      while (s && buf2[strlen(buf2)-1] != '"') {
	s = strtok (NULL, " \t");
	if (s) {
	  strcat (buf2, " ");
	  strcat (buf2, s);
	}
      }
      if (!s) {
	fatal_error ("String on [%s:%d] needs to be of the form \"...\"", name, line);
      }
      buf2[strlen(buf2)-1] = '\0';
      c->u.s = Strdup (buf2+1);
      b->v = c;
    }
    else if  (strcmp (s, "real") == 0) {
      GET_NAME;
      if (!(b = hash_lookup (H, buf3))) {
	b = hash_add (H, buf3);
      }
      else {
	Assert (((config_t*)b->v)->type == CONFIG_REAL, "Switching types!?");
	FREE (b->v);
      }
      NEW (c, config_t);
      c->type = CONFIG_REAL;
      s = strtok (NULL, " \t");
      if (!s) fatal_error ("Invalid format [%s:%d]", name, line);
      sscanf (s, "%lg", &c->u.r);
      b->v = c;
    }
    else if (strcmp (s, "int_table") == 0) {
      GET_NAME;
      if (!(b = hash_lookup (H, buf3))) {
	b = hash_add (H, buf3);
      }
      else {
	Assert (((config_t*)b->v)->type == CONFIG_TABLE_INT, "Switching types!?");
	FREE (b->v);
      }
      NEW (c, config_t);
      b->v = c;
      c->type = CONFIG_TABLE_INT;
      {
	A_DECL (int, x);
	A_INIT (x);
	
	/* read in a space-separated list of integers */
	s = strtok (NULL, " \t");
	while (s && s[0] != '#') {
	  /* accumulate s in the table */
	  A_NEWM (x,int);
	  sscanf (s, "%d", &A_NEXT (x));
	  A_INC (x);
	  s = strtok (NULL, " \t");
	}
	c->u.t.sz = A_LEN (x);
	c->u.t.u.i = x;
      }
    }
    else if (strcmp (s, "real_table") == 0) {
      GET_NAME;
      if (!(b = hash_lookup (H, buf3))) {
	b = hash_add (H, buf3);
      }
      else {
	Assert (((config_t*)b->v)->type == CONFIG_TABLE_INT, "Switching types!?");
	FREE (b->v);
      }
      NEW (c, config_t);
      b->v = c;
      c->type = CONFIG_TABLE_REAL;
      {
	A_DECL (double, x);
	A_INIT (x);
	
      /* read in a space-separated list of integers */
	s = strtok (NULL, " \t");
	while (s && s[0] != '#') {
	  /* accumulate s in the table */
	  A_NEWM (x,double);
	  sscanf (s, "%lg", &A_NEXT (x));
	  A_INC (x);
	  s = strtok (NULL, " \t");
	}
	c->u.t.sz = A_LEN (x);
	c->u.t.u.r = x;
      }
    }
    else if (strcmp (s, "string_table") == 0) {
      GET_NAME;
      if (!(b = hash_lookup (H, buf3))) {
	b = hash_add (H, buf3);
      }
      else {
	Assert (((config_t*)b->v)->type == CONFIG_TABLE_STR, "Switching types!?");
	FREE (b->v);
      }
      NEW (c, config_t);
      b->v = c;
      c->type = CONFIG_TABLE_STR;
      {
	A_DECL (char *, x);
	A_INIT (x);
	
      /* read in a space-separated list of integers */
	s = strtok (NULL, " \t");
	if (!s) fatal_error ("Invalid format [%s:%d]", name, line);

	do {
	  if (s[0] != '"') {
	    fatal_error ("String on [%s:%d] needs to be of the form \"...\"", name, line);
	  }
	  strcpy (buf2, s);

	  while (s && buf2[strlen(buf2)-1] != '"') {
	    s = strtok (NULL, " \t");
	    if (s) {
	      strcat (buf2, " ");
	      strcat (buf2, s);
	    }
	  }
	  if (!s) {
	    fatal_error ("String on [%s:%d] needs to be of the form \"...\"", name, line);
	  }

	  buf2[strlen(buf2)-1] = '\0';

	  A_APPEND (x, char *, Strdup (buf2+1));

	  s = strtok (NULL, " \t");

	  if (s && s[0] == '#') break;  /* comment */

	} while (s);

	c->u.t.sz = A_LEN (x);
	c->u.t.u.s = x;
      }
    } else if (strcmp (s, "begin") == 0) {
      initial_phase = 0;
      RAW_GET_NEXT;
      while (strlen (s) + 1 + strlen (prefix) >= prefix_len) {
	prefix_len += 1024;
	REALLOC (prefix, char, prefix_len);
      }
      strcat (prefix, s);
      strcat (prefix, ".");
    }
    else if (strcmp (s, "end") == 0) {
      int x = strlen (prefix)-1;
      initial_phase = 0;
      if (x < 0) {
	fatal_error ("end found without matching begin [%s:%d]\n", name, line);
      }
      x--;
      while (x >= 0 && prefix[x] != '.') 
	x--;
      x++;
      prefix[x] = '\0';
    }
    else {
      fatal_error ("Expecting int|string|real|int_table|real_table|begin|end|include [%s:%d]\n", name, line);
    }
  }
  fclose (fp);
  level--;

  if (level == 0) {
    for (i=0; i < A_LEN (files_read); i++) {
      FREE (files_read[i]);
    }
    A_FREE (files_read);
  }
}


/*------------------------------------------------------------------------
 *
 *  config_clear --
 *
 *   Clear hash table
 *
 *------------------------------------------------------------------------
 */
void config_clear (void)
{
  int i;
  hash_bucket_t *b;
  config_t *c;
  
  if (!H) return;

  for (i=0; i < H->size; i++)
    for (b = H->head[i]; b; b = b->next) {
      c = (config_t *) b->v;
      if (c->type == CONFIG_STR) {
	FREE (c->u.s);
      }
      FREE (c);
    }
  hash_clear (H);
}


/*------------------------------------------------------------------------
 *
 *  config_get_int --
 *
 *   Return integer corresponding to string
 *
 *------------------------------------------------------------------------
 */
int config_get_int (const char *s)
{
  hash_bucket_t *b;
  config_t *c;

  if (!H || !(b = hash_lookup (H, s))) {
    fatal_error ("%s : int, not in configuration file", s);
  }
  c = (config_t *)b->v;
  if (c->type != CONFIG_INT)
    fatal_error ("%s : not of type int!", s);
  return c->u.i;
}

/*------------------------------------------------------------------------
 *
 *  config_get_real --
 *
 *   Return real number corresponding to string
 *
 *------------------------------------------------------------------------
 */
double config_get_real (const char *s)
{
  hash_bucket_t *b;
  config_t *c;

  if (!H || !(b = hash_lookup (H, s))) {
    fatal_error ("%s : real, not in configuration file", s);
  }
  c = (config_t *)b->v;
  if (c->type != CONFIG_REAL)
    fatal_error ("%s : not of type real!", s);
  return c->u.r;
}

char *config_get_string (const char *s)
{
  hash_bucket_t *b;
  config_t *c;

  if (!H || !(b = hash_lookup (H, s))) {
    fatal_error ("%s : string, not in configuration file", s);
  }
  c = (config_t *)b->v;
  if (c->type != CONFIG_STR)
    fatal_error ("%s : not of type string!", s);
  return c->u.s;
}

/*------------------------------------------------------------------------
 *
 *  config_get_table_int --
 *
 *   Return integer table corresponding to string
 *
 *------------------------------------------------------------------------
 */
int *config_get_table_int (const char *s)
{
  hash_bucket_t *b;
  config_t *c;

  if (!H || !(b = hash_lookup (H, s))) {
    fatal_error ("%s : int_table, not in configuration file", s);
  }
  c = (config_t *)b->v;
  if (c->type != CONFIG_TABLE_INT)
    fatal_error ("%s : not of type int_table!", s);
  return c->u.t.u.i;
}


/*------------------------------------------------------------------------
 *
 *  config_get_table_real --
 *
 *   Return real table corresponding to string
 *
 *------------------------------------------------------------------------
 */
double *config_get_table_real (const char *s)
{
  hash_bucket_t *b;
  config_t *c;

  if (!H || !(b = hash_lookup (H, s))) {
    fatal_error ("%s : real_table, not in configuration file", s);
  }
  c = (config_t *)b->v;
  if (c->type != CONFIG_TABLE_REAL)
    fatal_error ("%s : not of type real_table!", s);
  return c->u.t.u.r;
}

/*------------------------------------------------------------------------
 *
 *  config_get_table_string --
 *
 *   Return string table
 *
 *------------------------------------------------------------------------
 */
char **config_get_table_string (const char *s)
{
  hash_bucket_t *b;
  config_t *c;
  
  if (!H || !(b = hash_lookup (H, s))) {
    fatal_error ("%s: string_table, not in configuration file", s);
  }
  c = (config_t *)b->v;
  if (c->type != CONFIG_TABLE_STR) 
    fatal_error ("%s: not of type string_table!", s);
  return c->u.t.u.s;
}

/*------------------------------------------------------------------------
 *
 *  config_get_table_size --
 *
 *   Return number of entries in the table
 *
 *------------------------------------------------------------------------
 */
int config_get_table_size (const char *s)
{
  hash_bucket_t *b;
  config_t *c;

  if (!H || !(b = hash_lookup (H, s))) {
    fatal_error ("%s : table not in configuration file", s);
  }
  c = (config_t *)b->v;
  if (c->type != CONFIG_TABLE_INT && c->type != CONFIG_TABLE_REAL &&
      c->type != CONFIG_TABLE_STR)
    fatal_error ("%s : not of type table!", s);
  return c->u.t.sz;
}


void config_set_default_int (const char *s, int v)
{
  hash_bucket_t *b;
  config_t *c;

  if (!H) {
    H = hash_new (8);
  }
  b = hash_lookup (H, s);
  if (b) {
    c = (config_t*) b->v;
    if (c->type != CONFIG_INT) {
      fatal_error ("Changing types on default!");
    }
  }
  else {
    b = hash_add (H, s);
    NEW (c, config_t);
    c->type = CONFIG_INT;
    b->v = c;
  }
  c->u.i = v;
}

void config_set_default_real (const char *s, double v)
{
  hash_bucket_t *b;
  config_t *c;

  if (!H) {
    H = hash_new (8);
  }
  b = hash_lookup (H, s);
  if (b) {
    c = (config_t*) b->v;
    if (c->type != CONFIG_REAL) {
      fatal_error ("Changing types on default!");
    }
  }
  else {
    b = hash_add (H, s);
    NEW (c, config_t);
    c->type = CONFIG_REAL;
    b->v = c;
  }
  c->u.r = v;
}


void config_set_default_string (const char *s, const char *t)
{
  hash_bucket_t *b;
  config_t *c;

  if (!H) {
    H = hash_new (8);
  }
  b = hash_lookup (H, s);
  if (b) {
    c = (config_t*) b->v;
    if (c->type != CONFIG_STR) {
      fatal_error ("Changing types on default!");
    }
    FREE (c->u.s);
  }
  else {
    b = hash_add (H, s);
    NEW (c, config_t);
    c->type = CONFIG_STR;
    b->v = c;
  }
  c->u.s = Strdup (t);
}


/*------------------------------------------------------------------------
 *
 *  config_exists --
 *
 *   Returns 1 if variable exists.
 *
 *------------------------------------------------------------------------
 */
int config_exists (const char *s)
{
  hash_bucket_t *b;

  if (!H || !(b = hash_lookup (H, s))) {
    return 0;
  }
  return 1;
}


/*------------------------------------------------------------------------
 *
 *  config_dump --
 *
 *   Dump config table to a file
 *
 *------------------------------------------------------------------------
 */
void config_dump (FILE *fp)
{
  int i;
  int j;
  hash_bucket_t *b;
  config_t *c;
  
  if (!H) return;

  for (i=0; i < H->size; i++) {
    for (b = H->head[i]; b; b = b->next) {
      c = (config_t *)b->v;
      switch (c->type) {
      case CONFIG_INT:
	fprintf (fp, "int %s %d\n", b->key, c->u.i);
	break;

      case CONFIG_REAL:
	fprintf (fp, "real %s %g\n", b->key, c->u.r);
	break;

      case CONFIG_STR:
	fprintf (fp, "string %s \"%s\"\n", b->key, c->u.s);
	break;
	
      case CONFIG_TABLE_INT:
	fprintf (fp, "int_table %s", b->key);
	for (j=0; j < c->u.t.sz; j++) {
	  fprintf (fp, " %d", c->u.t.u.i[j]);
	}
	fprintf (fp, "\n");
	break;

      case CONFIG_TABLE_REAL:
	fprintf (fp, "real_table %s", b->key);
	for (j=0; j < c->u.t.sz; j++) {
	  fprintf (fp, " %g", c->u.t.u.r[j]);
	}
	fprintf (fp, "\n");
	break;

      case CONFIG_TABLE_STR:
	fprintf (fp, "string_table %s", b->key);
	for (j=0; j < c->u.t.sz; j++) {
	  fprintf (fp, " \"%s\"", c->u.t.u.s[j]);
	}
	fprintf (fp, "\n");
	break;

      default:
	fatal_error ("Unknown configuration variable type?");
	break;
      }
    }
  }
}
