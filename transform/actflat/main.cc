/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2024 Rajit Manohar
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
#include <string.h>
#include <act/act.h>
#include <act/passes.h>
#include <common/config.h>

static void usage (char *name)
{
  fprintf (stderr, "Usage: %s [act-options] [-u] [-c <cells>] -p <proc> <actfile>\n", name);
  fprintf (stderr, "-u         : unmangled cell ports\n");
  fprintf (stderr, "-c <cells> : specify cells files\n");
  fprintf (stderr, "-p <proc>  : top-level process\n");
  exit (1);
}


int main (int argc, char **argv)
{
  Act *a;
  char *proc, *cells;
  extern int optind, opterr;
  extern char *optarg;
  int ch;

  /* initialize ACT library */
  Act::Init (&argc, &argv);

  config_set_int ("net.mangled_ports_actflat", 1);

  proc = NULL;
  cells = NULL;
  while ((ch = getopt (argc, argv, "c:p:u")) != -1) {
    switch (ch) {
    case 'u':
      config_set_int ("net.mangled_ports_actflat", 0);
      break;
      
    case 'p':
      if (proc) {
	FREE (proc);
      }
      proc = Strdup (optarg);
      break;
    case 'c':
      if (cells) {
	FREE (cells);
      }
      cells = Strdup (optarg);
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
    fprintf (stderr, "Missing ACT file.\n");
    usage (argv[0]);
  }
  if (proc == NULL) {
    fprintf (stderr, "Missing top-level process.\n");
    usage (argv[0]);
  }
  
  /* read in the ACT file */
  a = new Act (argv[optind]);
  if (cells) {
    a->Merge (cells);
  }
  FREE (cells);
  
  /* expand it */
  a->Expand ();
 
  /* find the process specified on the command line */
  Process *p = a->findProcess (proc);
  if (!p) {
    fatal_error ("Could not find process `%s' in file `%s'", proc, argv[optind]);
  }

  if (!p->isExpanded()) {
    p = p->Expand (ActNamespace::Global(), p->CurScope(), 0, NULL);
  }

  /* do stuff here */
  ActCellPass *cp = new ActCellPass (a);
  cp->run (p);

  ActNetlistPass *nl = new ActNetlistPass (a);
  nl->run (p);
  nl->printActFlat (stdout);

  return 0;
}
