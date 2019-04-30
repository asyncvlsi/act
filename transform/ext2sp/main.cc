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
#include "ext.h"
#include "hash.h"
#include "array.h"
#include "config.h"
#include "misc.h"
#include <act/act.h>

double mincap = 0.1e-15;
const char *gnd_node = "GND";

#define SEP_CHAR ':'

L_A_DECL (char *, globals);


void addglobal (char *name)
{
  for (int i=0; i < A_LEN (globals); i++) {
    if (strcmp (globals[i], name) == 0) return;
  }
  A_NEW (globals, char *);
  A_NEXT (globals) = Strdup (name);
  A_INC (globals);
}

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
      ret[j] = SEP_CHAR;  /* xyce */
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
  fprintf (stderr, "Usage: %s [-c <mincap>] <file.ext>\n", name);
  fprintf (stderr, " -c <mincap> : filter caps at or below this threshold\n");
  exit (1);
}

static struct Hashtable *seen = NULL;

struct alias_tree {
  int global;
  struct alias_tree *up;
  char *name;
  double cap_gnd;
  double p_perim, p_area, n_perim, n_area;
};


struct alias_tree *getalias (struct alias_tree *a)
{
  struct alias_tree *root, *tmp;
  
  while (a->up) {
    a = a->up;
  }
  root = a;
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
  while (s > 0 && name[s] != SEP_CHAR) {
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
    NEW (a, struct alias_tree);
    a->global = 0;
    a->up = NULL;
    a->name = name_munge (b->key);
    a->cap_gnd = 0;
    a->n_area = 0;
    a->n_perim = 0;
    a->p_area = 0;
    a->p_perim = 0;
    b->v = a;
    l = strlen (b->key);
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
      int s, e;
      char *tmp;
      hash_bucket_t *g;
      struct alias_tree *x;
      
      global_name (a->name, &s, &e);

      if (s == 0 && a->name[e+1] == '\0') {
	a->global = 2;
	addglobal (a->name);
      }
      else {
	MALLOC (tmp, char, e-s+2);
	tmp[e-s+1] = '\0';
	while (s <= e) {
	  tmp[e-s] = a->name[e];
	  e--;
	}
	g = hash_lookup (N, tmp);
	if (!g) {
	  g = hash_add (N, tmp);
	  NEW (x, struct alias_tree);
	  x->global = 2;
	  x->name = g->key;
	  addglobal (x->name);
	  x->cap_gnd = 0;
	  x->up = NULL;
	  g->v = x;
	}
	x = (struct alias_tree *)g->v;
	a->up = x;
	FREE (tmp);
      }
    }
  }
  return getalias ((struct alias_tree *)b->v);
}

void mergealias (struct alias_tree *a1, struct alias_tree *a2)
{
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

void ext2spice (const char *name, struct ext_file *E, int toplevel)
{
  hash_bucket_t *b;
  int l;
  struct Hashtable *N;
  int devcount = 1;
  static char **devnames = NULL;
  static int num_devices = 0;
  
  b = hash_lookup (seen, name);
  if (b) {
    return;
  }
  b = hash_add (seen, name);


  for (struct ext_list *lst = E->subcells; lst; lst = lst->next) {
    ext2spice (lst->file, lst->ext, 0);
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
    printf ("*  Main extract file %s\n", name);
    printf ("*---------------------------------------------------\n");
    printf ("*\n");
  }

  /*-- create names table --*/
  N = hash_new (32);

  /*-- process aliases --*/
  for (struct ext_alias *a = E->aliases; a; a = a->next) {
    struct alias_tree *t1, *t2;
    t1 = getname (N, a->n1);
    t2 = getname (N, a->n2);
    mergealias (t1, t2);
  }

  /*-- process area/perim --*/
  for (struct ext_ap *a = E->ap; a; a = a->next) {
    struct alias_tree *t = getname (N, a->node);
    t->n_area += a->n_area;
    t->n_perim += a->n_perim;
    t->p_area += a->p_area;
    t->p_perim += a->p_perim;
  }
  
  /*--- now print out aliases ---*/
  if (N->n > 0) {
    printf ("* -- connections ---\n");
    for (int i=0; i < N->size; i++) {
      for (hash_bucket_t *b = N->head[i]; b; b = b->next) {
	struct alias_tree *t = (struct alias_tree *)b->v;
	if (t->up) {
	  printf ("V%d %s ", devcount++, t->name);
	  if (0 && (t->up->global == 1)) {
	    int s, e;
	    global_name (t->up->name, &s, &e);
	    print_substr (t->up->name, s, e);
	  }
	  else {
	    printf ("%s", t->up->name);
	  }
	  printf ("\n");
	}
      }
    }
  }

  /*--- now print out fets ---*/

  if (!devnames) {
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
  }
  
  if (E->fet) {
    printf ("* -- fets ---\n");
    for (struct ext_fets *fl = E->fet; fl; fl = fl->next) {
      struct alias_tree *tsrc, *tdrain, *t;
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
      printf ("W=%gU L=%gU", fl->width*1e6, fl->length*1e6);
      if (fl->type == EXT_FET_PTYPE) {
	printf ("\n+ AS=%gP PS=%gU", tsrc->p_area*1e12, tsrc->p_perim*1e6);
	tsrc->p_perim = 0;
	tsrc->p_area = 0;
	printf (" AD=%gP PD=%gU", tdrain->p_area*1e12, tdrain->p_perim*1e6);
	tdrain->p_perim = 0;
	tdrain->p_area = 0;
      }
      else {
	printf ("\n+ AS=%gP PS=%gU", tsrc->n_area*1e12, tsrc->n_perim*1e6);
	tsrc->n_perim = 0;
	tsrc->n_area = 0;
	printf (" AD=%gP PD=%gU", tdrain->n_area*1e12, tdrain->n_perim*1e6);
	tdrain->n_perim = 0;
	tdrain->n_area = 0;
      }
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

  /*--- delete hashtable ---*/
  for (int i=0; i < N->size; i++) {
    for (hash_bucket_t *b = N->head[i]; b; b = b->next) {
      FREE (b->v);
    }
  }
  hash_free (N);
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

  while ((ch = getopt (argc, argv, "c:")) != -1) {
    switch (ch) {
    case 'c':
      mincap = atof (optarg);
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
  ext2spice (argv[optind], E, 1);

  return 0;
}
