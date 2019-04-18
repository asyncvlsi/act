/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2007, 2018-2019 Rajit Manohar
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
#include <stdlib.h>
#include <unistd.h>
#include "v_parse.h"
#include "v_walk_X.h"
#include "misc.h"
#include <string.h>

char *channame;

static
char *find_library (char *s)
{
  char buf[10240];
  char *ret;
  FILE *tmp;

  ret = NULL;
  if (getenv ("CAD_HOME")) {
    sprintf (buf, "%s/lib/v2act/%s", getenv ("CAD_HOME"), s);
    tmp = fopen (buf, "r");
    if (tmp) {
      fclose (tmp);
      ret = buf;
    }
  }
  if (!ret && getenv ("ACT_HOME")) {
    sprintf (buf, "%s/lib/v2act/%s", getenv ("ACT_HOME"), s);
    tmp = fopen (buf, "r");
    if (tmp) {
      fclose (tmp);
      ret = buf;
    }
  }
  if (ret) {
    return Strdup (ret);
  }
  else {
    return NULL;
  }
}

static void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-a] [-c clkname] [-o outfile] -L <lib> <file.v>\n", s);
  fprintf (stderr, "  -a : async output\n");
  fprintf (stderr, "  -C <chan>: change default channel name to <chan>\n");
  fprintf (stderr, "  -c <clkname>: specifies the clock port name [default: clock]\n");
  fprintf (stderr, "  -o <file> : specify output file name [default: stdout]\n");
  fprintf (stderr, "  -L <lib>  : synchronous library (act file) [default: sync.act]\n");
  exit (1);
}

int mode;			/* sync or async mode */

int main (int argc, char **argv)
{
  Act *a;
  v_Token *t;
  VWalk *w;
  extern int optind, opterr;
  extern char *optarg;
  int ch;
  char *clkname;
  char *fname;
  char *libname;
  int i, j;

  Act::Init (&argc, &argv);

  channame = NULL;

  /* generate the sed command */
  j = -1;
  for (i=0; argv[0][i]; i++) {
    if (argv[0][i] == '/') j = i;
  }

  mode = V_SYNC;
  clkname = NULL;
  fname = NULL;
  libname = NULL;

  while ((ch = getopt (argc, argv, "C:c:ao:L:")) != -1) {
    switch (ch) {
    case 'C':
      if (channame) {
	FREE (channame);
      }
      channame = Strdup (optarg);
      break;

    case 'c':
      if (clkname) {
	FREE (clkname);
      }
      clkname = Strdup (optarg);
      break;

    case 'o':
      if (fname) {
	FREE (fname);
      }
      fname = Strdup (optarg);
      break;

    case 'a':
      mode = V_ASYNC;
      break;

    case 'L':
      if (libname) {
	FREE (libname);
      }
      libname = Strdup (optarg);
      break;

    case '?':
      usage (argv[0]);
      break;
    default:
      fatal_error ("getopt buggy?");
      break;
    }
  }
    
  if (optind != argc - 1) {
    fprintf (stderr, "Missing verilog netlist.\n");
    usage (argv[0]);
  }
  if (!libname) {
    fprintf (stderr, "Missing synchronous library.\n");
    usage (argv[0]);
  }

  if (!channame) {
    channame = Strdup ("e1of2");
  }

  {
    char *s;
    
    s = find_library (libname);
    if (!s) {
      a = new Act (libname);
    }
    else {
      a = new Act (s);
      FREE (s);
    }
  }

  t = v_parse (argv[optind]);

  NEW (w, VWalk);
  w->a = a;
  w->out = stdout;
  w->prefix = NULL;
  w->M = hash_new (32);
  w->hd = NULL;
  w->tl = NULL;
  w->missing = hash_new (8);

  v_walk_X (w, t);

  label_clocks (w, clkname ? clkname : "clock");

  if (fname) {
    w->out = fopen (fname, "w");
    if (!w->out) {
      fatal_error ("Could not open file `%s' for writing", fname);
    }
  }

  if (mode == V_ASYNC) {
    module_t *m;
    for (m = w->hd; m; m = m->next) {
      compute_fanout (w, m);
    }
  }


  emit_types (w);

  fclose (w->out);

  return 0;
}
