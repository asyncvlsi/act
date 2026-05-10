/*************************************************************************
 *
 *  This file is part of the ACT library
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
#include <string.h>
#include <act/act.h>
#include <act/passes/booleanize.h>
#include <act/passes/cells.h>
#include <act/iter.h>
#include <common/config.h>
#include <vnet.h>
#include <map>


static void usage (char *name)
{
  fprintf (stderr, "Usage: %s [act-options] [-Bmf] [-c <cells>] [-p <proc>] <act>\n", name);
  fprintf (stderr, " -p <proc> : Emit process <proc>\n");
  exit (1);
}


/*
  Initialize globals from the configuration file.
  Returns process name
*/
static char *initialize_parameters (int *argc, char ***argv, char **cells)
{
  char *proc_name;
  int ch;

  proc_name = NULL;
  *cells = NULL;

  Act::Init (argc, argv);

  while ((ch = getopt (*argc, *argv, "mfBc:p:")) != -1) {
    switch (ch) {
    case 'f':
      config_set_int ("act2v.fuse_signal_directives", 1);
      break;
      
    case 'm':
      config_set_int ("act2v.name_mangle", 1);
      break;
    case 'B':
      config_set_int ("net.black_box_mode", 1);
      break;
    case 'p':
      if (proc_name) {
	FREE (proc_name);
      }
      proc_name = Strdup (optarg);
      break;
    case 'c':
      if (*cells) {
        FREE (*cells);
      }
      *cells = Strdup (optarg);
      break;
    case '?':
      fprintf (stderr, "Unknown option.\n");
      usage ((*argv)[0]);
      break;
    default:
      fatal_error ("shouldn't be here");
      break;
    }
  }

  /* optind points to what is left */
  /* expect 1 argument left */
  if (optind != *argc - 1) {
    fprintf (stderr, "Missing act file name.\n");
    usage ((*argv)[0]);
  }

  if (!proc_name) {
    fprintf (stderr, "Missing process name.\n");
    usage ((*argv)[0]);
  }

  *argc = 2;
  (*argv)[1] = (*argv)[optind];
  (*argv)[2] = NULL;

  return proc_name;
}


int main (int argc, char **argv)
{
  Act *a;
  char *proc;
  char *cells;

  proc = initialize_parameters (&argc, &argv, &cells);
  config_set_default_string ("act.global_signal_prefix", "top.");

  if (argc != 2) {
    fatal_error ("Something strange happened!");
  }
  if (proc == NULL) {
    fatal_error ("Missing process name!");
  }
  
  a = new Act (argv[1]);
  if (cells) {
    a->Merge (cells);
  }
  a->Expand ();

  if (cells) {
    ActCellPass *cp = new ActCellPass (a);
    cp->run ();
  }

  Process *p = a->findProcess (proc);

  if (!p) {
    fatal_error ("Could not find process `%s' in file `%s'", proc, argv[1]);
  }

  if (!p->isExpanded()) {
    p = p->Expand (ActNamespace::Global(), p->CurScope(), 0, NULL);
  }

  act_emit_verilog (a, stdout, p);
  return 0;
}
