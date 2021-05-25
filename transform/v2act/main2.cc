/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2020 Rajit Manohar
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
#include <common/config.h>
#include "vnet.h"
#include <common/misc.h>
#include <common/agraph.h>

static char *find_exepath (const char *s)
{
  char buf[10240];
  char *ret;
  FILE *tmp;

  ret = NULL;
  if (getenv ("ACT_HOME")) {
    snprintf (buf, 10240, "%s/bin/%s", getenv ("ACT_HOME"), s);
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
  fprintf (stderr, "Usage: %s [-c clkname] [-o outfile] <file.v>\n", s);
  fprintf (stderr, "  -c <clkname>: specifies the clock port name [default: clock]\n");
  fprintf (stderr, "  -o <file> : specify output file name [default: stdout]\n");
  exit (1);
}

char *lib_namespace;

int main (int argc, char **argv)
{
  VNet *w;
  extern int optind, opterr;
  extern char *optarg;
  int ch;
  char *clkname;
  char *fname;
  char *libname;
  int i, j;

  Act::Init (&argc, &argv);

  clkname = NULL;
  fname = NULL;
  lib_namespace = Strdup ("sync");

  while ((ch = getopt (argc, argv, "c:o:n:")) != -1) {
    switch (ch) {
    case 'n':
      if (lib_namespace) {
	FREE (lib_namespace);
      }
      lib_namespace = Strdup (optarg);
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

  config_read ("s2a.conf");


  char *script = find_exepath ("v2act_quote.sed");
  if (!script) {
    warning ("Multi-bit constants may not work; could not find pre-processing script");
    w = verilog_read (argv[optind], config_get_string ("s2a.library"));
  }
  else {
    char buf[10240];
    snprintf (buf, 10240, "sed -f %s %s > %sp", script, argv[optind],
	      argv[optind]);
    FREE (script);
    system (buf);
    snprintf (buf, 10240, "%sp", argv[optind]);
    w = verilog_read (buf, config_get_string ("s2a.library"));
    script = Strdup (buf);
  }
  
  
  AGraph *ag = verilog_create_netgraph (w);

  verilog_mark_clock_nets (w);

  
  
  if (fname) {
    w->out = fopen (fname, "w");
    if (!w->out) {
      fatal_error ("Could not open file `%s' for writing", fname);
    }
  }

  fclose (w->out);

  if (script) {
    unlink (script);
    FREE (script);
  }

  return 0;
}
