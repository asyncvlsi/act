/*************************************************************************
 *
 *  Copyright (c) 2019 Rajit Manohar
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <common/ext.h>
#include <common/hash.h>
#include <common/array.h>
#include <common/config.h>
#include <common/misc.h>
#include <act/act.h>

static double mincap = 0.1e-15;
static double scale = 1.0;
static int use_subckt_models = 0;
const char *gnd_node = "GND";
static char SEP_CHAR = ':';

L_A_DECL (char *, globals);
static char **devnames = NULL;
static int num_devices = 0;

void addglobal (char *name)
{
  for (int i=0; i < A_LEN (globals); i++) {
    if (strcmp (globals[i], name) == 0) return;
  }
  A_NEW (globals, char *);
  A_NEXT (globals) = Strdup (name);
  A_INC (globals);
}

/*
  Name munge: converts name to add in "xfoo<sep>", as needed by SPICE.
  Key is the name from the .ext file, name is the name translated to
  the appropriate SPICE version.
*/
char *name_munge (const char *name)
{
  int count = 0;
  char *ret;
  int i, j;
  int flag;

  i = 0;
  while (name[i]) {
    if (name[i] == '/') 
      count++;
    i++;
  }

  MALLOC (ret, char, strlen (name) + count + 1);
  i = 0;
  j = 0;

  flag = (count > 0) ? 1 : 0;
  while (name[i]) {
    if (flag) {
      ret[j++] = 'x';
      flag = 0;
    }
    if (name[i] == '/') {
      ret[j] = SEP_CHAR;
      count--;
      flag = (count > 0) ? 1 : 0;
    }
    else {
      ret[j] = name[i];
    }
    i++;
    j++;
  }
  ret[j] = '\0';
  return ret;
}

static void usage (char *name)
{
  fprintf (stderr, "Usage: %s [act-options] [-c <mincap>] [-s <scale>] <file.ext>\n", name);
  fprintf (stderr, " -c <mincap> : filter caps at or below this threshold\n");
  fprintf (stderr, " -s <scale>  : scale all units by <scale>\n");
  exit (1);
}

static struct Hashtable *seen = NULL;

struct alias_tree {
  struct alias_tree *up;
  struct alias_tree *noglob;
  hash_bucket_t *b;
  char *name;
  int global;
  double cap_gnd;
  double *perim, *area;
};


struct alias_tree *getroot (struct alias_tree *a)
{
  while (a->up) {
    a = a->up;
  }
  return a;
}

struct alias_tree *getroot_noglob (struct alias_tree *a)
{
  struct alias_tree *tmp;

  tmp = a;
  while (a->up) {
    a = a->up;
  }

  while (tmp->up) {
    struct alias_tree *x;
    x = tmp->up;
    tmp->up = a;
    tmp = x;
  }
  
  return a;
}

struct alias_tree *getalias (struct alias_tree *a)
{
  struct alias_tree *root, *tmp;

  root = getroot (a);
  while (a->up) {
    tmp = a->up;
    a->up = root;
    a = tmp;
  }
  return root;
}

void global_name (const char *name, int *start, int *end)
{
  int s, e;

  e = strlen (name);
  while (e > 0 && name[e] != '!') {
    e--;
  }
  Assert (e > 0, "What?");
  e--;
  s = e;
  while (s > 0 && name[s] != '/') {
    s--;
  }
  if (s != 0) { s++; }
  *start = s;
  *end = e;
}

void print_substr (const char *name, int s, int e)
{
  while (s <= e) {
    putchar (name[s++]);
  }
}

static struct alias_tree *newnode ()
{
  struct alias_tree *a;
  
  NEW (a, struct alias_tree);
  a->b = NULL;
  a->global = 0;
  a->up = NULL;
  a->noglob = NULL;
  a->name = NULL;
  a->cap_gnd = 0;
  MALLOC (a->area, double, num_devices);
  MALLOC (a->perim, double, num_devices);
  for (int i=0; i < num_devices; i++) {
    a->area[i] = 0;
    a->perim[i] = 0;
  }
  return a;
}


static int islocal (char *s)
{
  int i = 0;
  while (s[i]) {
    if (s[i] == '/') return 0;
    i++;
  }
  return 1;
}

/*
  name is in the EXT file namespace 
*/
struct alias_tree *getname (struct Hashtable *N, const char *name)
{
  hash_bucket_t *b;
  struct alias_tree *a;
  int l;

  b = hash_lookup (N, name);
  if (!b) {
    b = hash_add (N, name);
    a = newnode ();
    a->name = name_munge (b->key);
    b->v = a;
    a->b = b;
    l = strlen (b->key);

    /* if the name has a "!" in it, it is a global signal */
    while (l > 0) {
      if (b->key[l] == '!') {
	a->global = 1;
	break;
      }
      if (b->key[l] == '/') {
	break;
      }
      l--;
    }
    if (a->global) {
      /* a->global = 1 => the signal has a ! */
      int s, e;
      char *tmp;
      hash_bucket_t *g;
      struct alias_tree *x;

      global_name (b->key, &s, &e);
      /* s, e correspond to start and end indices in b->key that
	 delimits the global signal name. 
	 e+1 should be '!'
      */
      Assert (b->key[e+1] == '!', "What?");
      MALLOC (tmp, char, e-s+2);
      tmp[e-s+1] = '\0';
      while (s <= e) {
	tmp[e-s] = b->key[e];
	e--;
      }
      /* tmp is the name of the global, without the ! */
      g = hash_lookup (N, tmp);
      if (!g) {
	g = hash_add (N, tmp);
	x = newnode ();
	x->global = 2;
	x->name = g->key;
	addglobal (x->name);
	g->v = x;
	x->b = g;
      }
      x = (struct alias_tree *)g->v;
      a->up = x;
      FREE (tmp);
    }
  }
  return getalias ((struct alias_tree *)b->v);
}

void mergealias (struct alias_tree *a1, struct alias_tree *a2)
{
  struct alias_tree *t1, *t2;

  /* noglob merge trees */
  t1 = getroot_noglob (a1);
  t2 = getroot_noglob (a2);
  if (t1 != t2) {
    t1->noglob = t2;
  }
  
  a1 = getalias (a1);
  a2 = getalias (a2);
  if (a1 != a2) {
    if (!a2->global) {
      a2->up = a1;
      a2->cap_gnd += a1->cap_gnd;
      a1->cap_gnd = 0;
    }
    else if (!a1->global) {
      a1->up = a2;
      a1->cap_gnd += a2->cap_gnd;
      a2->cap_gnd = 0;
    }
    else if (a2->global == 1) {
      a2->up = a1;
      a2->cap_gnd += a1->cap_gnd;
      a1->cap_gnd = 0;
    }
    else if (a1->global == 1) {
      a1->up = a2;
      a1->cap_gnd += a2->cap_gnd;
      a2->cap_gnd = 0;
    }
    else {
      warning ("Connecting `%s' and `%s': two globals?", a1->name, a2->name);
      a1->up = a2;
      a1->cap_gnd += a2->cap_gnd;
      a2->cap_gnd = 0;
    }
  }
}

static struct alias_tree *get_cur_node (hash_bucket_t *b, struct Hashtable *cur,
					char *buf, int l)
{
  struct alias_tree *x = (struct alias_tree *)b->v;
  hash_bucket_t *newb;
  struct alias_tree *newx;
  
  if (x->global == 2) {
    newb = hash_lookup (cur, b->key);
    if (!newb) {
      return NULL;
    }
    else {
      return (struct alias_tree *)newb->v;
    }
  }
  else {
    sprintf (buf+l+1, "%s", b->key);
    newb = hash_lookup (cur, buf);
    if (!newb) {
      return NULL;
    }
    else {
      return (struct alias_tree *)newb->v;
    }
  }
}

static void import_base_node (hash_bucket_t *b, struct Hashtable *cur,
			      char *buf, int l)
{
  struct alias_tree *x = (struct alias_tree *)b->v;
  int new_node;
  hash_bucket_t *newb;
  struct alias_tree *newx;

  sprintf (buf+l+1, "%s", b->key);

  new_node = 1;

  if (x->global == 2) {
    newb = hash_lookup (cur, b->key);
    if (!newb) {
      newb = hash_add (cur, b->key);
    }
    else {
      new_node = 0;
    }
  }
  else {
    newb = hash_lookup (cur, buf);
    if (newb) {
      new_node = 0;
    }
    else {
      newb = hash_add (cur, buf);
    }
  }
  if (new_node) {
    newx = newnode();
    newx->name = name_munge (newb->key);
    newx->global = x->global;
    newb->v = newx;
    newx->b = newb;
  }
}

static void import_subcell_conns (struct Hashtable *N,
				  const char *instname, const char *tname,
				  int xl, int xh, int yl, int yh)
{
  struct Hashtable *sub;
  hash_bucket_t *b, *tb;
  char *strbuf;
  int l, t;

  l = strlen (instname);

  b = hash_lookup (seen, tname);
  Assert (b, "What?");
  sub = (struct Hashtable *) b->v;

  t = 0;
  for (int i=0; i < sub->size; i++) {
    for (b = sub->head[i]; b; b = b->next) {
      int x = strlen (b->key);
      if (x > t) {
	t = x;
      }
    }
  }

  int dims = 0;

  if (xl != xh) { t += 10; dims++; }
  if (yl != yh) { t += 10; dims++; }
  
  MALLOC (strbuf, char, l + t + 2);

  if (xl == xh && yl != yh) {
    xl = yl;
    xh = yh;
    yl = 0;
    yh = 0;
  }

  int xval, yval;
  xval = xl;
  yval = yl;

  do {
    if (dims == 0) {
      sprintf (strbuf, "%s/", instname);
    }
    else if (dims == 1) {
      sprintf (strbuf, "%s[%d]/", instname, xval);
    }
    else if (dims == 2) {
      sprintf (strbuf, "%s[%d,%d]/", instname, xval, yval);
    }

    for (int i=0; i < sub->size; i++) {
      for (b = sub->head[i]; b; b = b->next) {
	struct alias_tree *base;
	struct alias_tree *x, *x1;

	base = (struct alias_tree *)b->v;
	import_base_node (b, N, strbuf, l);

	/* x is the current node */
	x = get_cur_node (b, N, strbuf, l);

	/* now import connections */
	if (base->up) {
	  import_base_node (base->up->b, N, strbuf, l);
	  x1 = get_cur_node (base->up->b, N, strbuf, l);
	  Assert (x1, "What?");
	  x->up = x1;
	}
	if (base->noglob) {
	  import_base_node (base->noglob->b, N, strbuf, l);
	  x1 = get_cur_node (base->noglob->b, N, strbuf, l);
	  Assert (x1, "What?");
	  x->noglob = x1;
	}
      }
    }
    
    if (dims == 0) {
      break;
    }
    else if (dims == 1) {
      xval++;
      if (xval > xh) {
	break;
      }
    }
    else if (dims == 2) {
      xval++;
      if (xval > xh) {
	xval = xl;
	yval++;
	if (yval > yh) {
	  break;
	}
      }
    }
  } while (1);

  FREE (strbuf);
}

static void print_number (FILE *fp, double x)
{
  if (x > 1e3) {
    fprintf (fp, "%gK", x*1e-3);
  }
  if (x > 1e-3) {
    fprintf (fp, "%g", x);
  }
  else if (x > 1e-9) {
    fprintf (fp, "%gU", x*1e6);
  }
  else {
    fprintf (fp, "%gP", x*1e12);
  }
}


void ext2spice (const char *name, struct ext_file *E, int toplevel)
{
  hash_bucket_t *b;
  int l;
  struct Hashtable *N;
  int devcount = 1;
  
  b = hash_lookup (seen, name);
  if (b) {
    return;
  }
  b = hash_add (seen, name);

  /*-- create names table --*/
  N = hash_new (32);
  b->v = N;

  for (struct ext_list *lst = E->subcells; lst; lst = lst->next) {
    int xl, xh, yl, yh;
    ext2spice (lst->file, lst->ext, 0);
    /* import connections */
    if (lst->xhi < lst->xlo) {
      xl = lst->xhi;
      xh = lst->xlo;
    }
    else {
      xl = lst->xlo;
      xh = lst->xhi;
    }
    if (lst->yhi < lst->ylo) {
      yl = lst->yhi;
      yh = lst->ylo;
    }
    else {
      yl = lst->ylo;
      yh = lst->yhi;
    }
    import_subcell_conns (N, lst->id, lst->file, xl, xh, yl, yh);
  }

  if (!toplevel) {
    l = strlen (name);
    if (l >= 4 && (strcmp (name + l - 4, ".ext") == 0)) {
      l -= 4;
    }
    printf ("*---------------------------------------------------\n");
    printf ("* Subcircuit from %s\n", name);
    printf ("*---------------------------------------------------\n");
    printf (".subckt ");
    while (l) {
      putchar (*name);
      name++;
      l--;
    }
    printf (" _\n");
  }
  else {
    printf ("*\n");
    printf ("*---------------------------------------------------\n");
    printf ("*  Main extract file %s [scale=%g]\n", name, scale);
    printf ("*---------------------------------------------------\n");
    printf ("*\n");
  }

  if (E->subcells) {
    printf ("*--- subcircuits ---\n");
    for (struct ext_list *lst = E->subcells; lst; lst = lst->next) {
      int p = 0;
      printf ("x%s %s ", lst->id, gnd_node);
      l = strlen (lst->file);
      if (l >= 4 && (strcmp (lst->file + l - 4, ".ext") == 0)) {
	l -= 4;
      }
      while (l) {
	putchar (lst->file[p++]);
	l--;
      }
      putchar ('\n');
    }
  }
  
  /*-- process aliases --*/
  printf ("* -- connections ---\n");
  for (struct ext_alias *a = E->aliases; a; a = a->next) {
    struct alias_tree *t1, *t2;
    t1 = getname (N, a->n1);
    t2 = getname (N, a->n2);
    if (t1 != t2) {
      char *s1, *s2;
      s1 = name_munge (a->n1);
      s2 = name_munge (a->n2);
      printf ("V%d %s %s\n", devcount++, s2, s1);
      FREE (s1);
      FREE (s2);
    }
    if (islocal (a->n1)) {
      mergealias (t1, t2);
    }
    else {
      mergealias (t2, t1);
    }      
  }

  /*-- process area/perim --*/
  for (struct ext_ap *a = E->ap; a; a = a->next) {
    struct alias_tree *t = getname (N, a->node);
    for (int i=0; i < num_devices; i++) {
      t->area[i] += a->area[i];
      t->perim[i] += a->perim[i];
    }
  }

  /*--- now print out fets ---*/
  if (E->fet) {
    printf ("* -- fets ---\n");
    for (struct ext_fets *fl = E->fet; fl; fl = fl->next) {
      struct alias_tree *tsrc, *tdrain, *t;
      if (use_subckt_models) {
	printf ("x");
      }
      printf ("M%d ", devcount++);
      tdrain = getname (N, fl->t2); /* drain */
      printf ("%s ", tdrain->name); /* gate */
      t = getname (N, fl->g);  /* src */
      printf ("%s ", t->name);
      tsrc = getname (N, fl->t1);
      printf ("%s ", tsrc->name);
      t = getname (N, fl->sub);
      printf ("%s ", t->name);
      if (devnames) {
	printf ("%s ", config_get_string (devnames[fl->type]));
      }
      else {
	if (fl->type == EXT_FET_PTYPE) {
	  printf ("pfet ");
	}
	else {
	  printf ("nfet ");
	}
      }
      printf ("W=");
      print_number (stdout, fl->width*scale);
      printf (" L=");
      print_number (stdout, fl->length*scale);
      printf ("\n+ AS=");
      print_number (stdout, tsrc->area[fl->type]*scale*scale);
      printf (" PS=");
      print_number (stdout, tsrc->perim[fl->type]*scale);
      tsrc->area[fl->type] = 0;
      tsrc->perim[fl->type] = 0;
      printf (" AD=");
      print_number (stdout, tdrain->area[fl->type]*scale*scale);
      printf (" PD=");
      print_number (stdout, tdrain->perim[fl->type]*scale);
      tdrain->area[fl->type] = 0;
      tdrain->perim[fl->type] = 0;
      printf ("\n");
    }
  }

  /*-- caps --*/
  if (E->cap) {
    printf ("* -- caps ---\n");

    /*--- collect cap to GND ---*/
    for (struct ext_cap *l = E->cap; l; l = l->next) {
      struct alias_tree *t, *u;

      t = getname (N, l->n1);
      if (l->type == CAP_GND || l->type == CAP_SUBSTRATE) {
	if (l->type == CAP_GND) {
	  t->cap_gnd += l->cap;
	}
      }
      else {
	u = getname (N, l->n2);
	if (strcmp (t->name, gnd_node) == 0) {
	  u->cap_gnd += l->cap;
	}
	else if (strcmp (u->name, gnd_node) == 0) {
	  t->cap_gnd += l->cap;
	}
      }
    }
      
    for (struct ext_cap *l = E->cap; l; l = l->next) {
      struct alias_tree *t, *u;

      if (l->type == CAP_GND || l->type == CAP_SUBSTRATE) continue;
    
      t = getname (N, l->n1);
      u = getname (N, l->n2);
      if (strcmp (t->name, gnd_node) == 0) {
	continue;
      }
      else if (strcmp (u->name, gnd_node) == 0) {
	continue;
      }

      if (l->cap < mincap) continue;
    
      printf ("C%d %s %s %gF\n", devcount++, t->name, u->name, l->cap*1.0e15);
    }

    /* print caps to GND */
    for (int i=0; i < N->size; i++) {
      for (hash_bucket_t *b = N->head[i]; b; b = b->next) {
	struct alias_tree *t = (struct alias_tree *)b->v;
	if (t->cap_gnd > 0 && t->cap_gnd >= mincap) {
	  if (strcmp (t->name, gnd_node) == 0) continue;
	  printf ("C%d %s %s %gF\n", devcount++, t->name,
		  gnd_node, t->cap_gnd*1.0e15);
	}
      }
    }
  }


  if (!toplevel) {
    printf (".ends\n");
  }
  else {
    /* print globals! */
    printf ("*--- inferred globals\n");
    for (int i=0; i < A_LEN (globals); i++) {
      printf (".global %s\n", globals[i]);
    }
  }

#if 0  
  /*--- delete hashtable ---*/
  for (int i=0; i < N->size; i++) {
    for (hash_bucket_t *b = N->head[i]; b; b = b->next) {
      FREE (b->v);
    }
  }
  hash_free (N);
#endif  
  N = NULL;
}


int main (int argc, char **argv)
{
  int ch;
  extern int optind, opterr;
  extern char *optarg;
  struct ext_file *E;

  A_INIT (globals);

  Act::Init (&argc, &argv);

  while ((ch = getopt (argc, argv, "c:s:")) != -1) {
    switch (ch) {
    case 'c':
      mincap = atof (optarg);
      break;
    case 's':
      scale = atof (optarg);
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  if (optind != argc - 1) {
    fprintf (stderr, "Missing extract file name\n");
    usage (argv[0]);
  }

  seen = hash_new (4);
  ext_validate_timestamp (argv[optind]);
  E = ext_read (argv[optind]);

  if (config_exists ("net.ext_map")) {
    char **rawdevs;
    int j;
    num_devices = config_get_table_size ("net.ext_map");
    Assert (config_get_table_size ("net.ext_map") ==
	    config_get_table_size ("net.ext_devs"), "Inconsistency in config");
    rawdevs = config_get_table_string ("net.ext_map");
    MALLOC (devnames, char *, num_devices);
    for (j=0; j < num_devices; j++) {
      MALLOC (devnames[j], char, strlen (rawdevs[j]) + 4 + 1);
      sprintf (devnames[j], "net.%s", rawdevs[j]);
    }
  }
  else {
    num_devices = 2;
    devnames = NULL;
  }
  if (config_exists ("net.spice_path_sep")) {
    char *s = config_get_string ("net.spice_path_sep");
    if (strlen (s) != 1) {
      fatal_error ("net.spice_path_sep must have length 1");
    }
    SEP_CHAR = s[0];
  }

  if (config_exists ("net.use_subckt_models")) {
    use_subckt_models = config_get_int ("net.use_subckt_models");
  }

  ext2spice (argv[optind], E, 1);

  return 0;
}
