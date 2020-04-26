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
#include <config.h>
#include "vnet.h"
#include "misc.h"
#include "agraph.h"

static void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-c clkname] [-o outfile] <file.v>\n", s);
  fprintf (stderr, "  -c <clkname>: specifies the clock port name [default: clock]\n");
  fprintf (stderr, "  -o <file> : specify output file name [default: stdout]\n");
  exit (1);
}


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

  while ((ch = getopt (argc, argv, "c:o:")) != -1) {
    switch (ch) {
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
  
  w = verilog_read (argv[optind], config_get_string ("s2a.library"));
  AGraph *ag = verilog_create_netgraph (w);

  verilog_mark_clock_nets (w);

  
  
  if (fname) {
    w->out = fopen (fname, "w");
    if (!w->out) {
      fatal_error ("Could not open file `%s' for writing", fname);
    }
  }

  fclose (w->out);

  return 0;
}
