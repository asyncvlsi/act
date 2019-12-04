/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#include <act/passes/netlist.h>
#include <config.h>

/*
  Check for trailing .extension
*/
static int has_trailing_extension (const char *s, const char *ext)
{
  int l_s, l_ext;
	
  l_s = strlen (s);
  l_ext = strlen (ext);
	
  if (l_s < l_ext + 2) return 0;
	
  if (strcmp (s + l_s - l_ext, ext) == 0) {
    if (s[l_s-l_ext-1] == '.')
      return 1;
  }
  return 0;
}

static void usage (char *name)
{
  fprintf (stderr, "Usage: %s [-dltBR] [-C <conf>] [-p <proc>] [-o <file>] <act>\n", name);
  fprintf (stderr, " -C <conf> Configuration file name\n");
  fprintf (stderr, " -t        Only emit top-level cell (no sub-cells)\n");
  fprintf (stderr, " -p <proc> Emit process <proc>\n");
  fprintf (stderr, " -o <file> Save result to <file> rather than stdout\n");
  fprintf (stderr, " -d	       Emit parasitic source/drain diffusion area/perimeters with fets\n");
  fprintf (stderr, " -B	       Black-box mode. Assume empty act process is an external .sp file\n");
  fprintf (stderr, " -l	       LVS netlist; ignore all load capacitances\n");
  fprintf (stderr, " -S        Enable shared long-channel devices in staticizers\n");
  exit (1);
}


static int enable_shared_stat = 0;

/*
  Initialize globals from the configuration file.
  Returns process name
*/
static char *initialize_parameters (int *argc, char ***argv, FILE **fpout)
{
  char *conf_file;
  char *proc_name;
  char *act_cmdline;
  int ch;
  int ignore_loadcap = 0;
  int emit_parasitics = 0;
  int black_box_mode = 0;
  int top_level_only = 0;


  *fpout = stdout;
  top_level_only = 0;
  conf_file = NULL;
  proc_name = NULL;

  config_set_default_string ("net.global_vdd", "Vdd");
  config_set_default_string ("net.global_gnd", "GND");
  config_set_default_string ("net.local_vdd", "VddN");
  config_set_default_string ("net.local_gnd", "GNDN");

  Act::Init (argc, argv);

  while ((ch = getopt (*argc, *argv, "SBdC:tp:o:l")) != -1) {
    switch (ch) {
    case 'S':
      enable_shared_stat = 1;
      break;
      
    case 'l':
      ignore_loadcap = 1;
      break;

    case 'B':
      black_box_mode = 1;
      break;

    case 'd':
      emit_parasitics = 1;
      break;

    case 'C':
      if (conf_file) {
	FREE (conf_file);
      }
      conf_file = Strdup (optarg);
      break;

    case 't':
      top_level_only = 1;
      break;

    case 'p':
      if (proc_name) {
	FREE (proc_name);
      }
      proc_name = Strdup (optarg);
      break;
      
    case 'o':
      if (*fpout != stdout) {
	fclose (*fpout);
      }
      *fpout = fopen (optarg, "w");
      if (!*fpout) {
	fatal_error ("Could not open file `%s' for writing.", optarg);
      }
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

  if (conf_file) {
    /* read config file */
    if (has_trailing_extension (conf_file, "conf")) {
      config_read (conf_file);
    }
    else {
      char buf[10240];
      sprintf (buf, "%s.conf", conf_file);
      config_read (buf);
    }
  }

  config_set_default_int ("net.ignore_loadcap", ignore_loadcap);
  config_set_default_int ("net.emit_parasitics", emit_parasitics);
  config_set_default_int ("net.black_box_mode", black_box_mode);
  config_set_default_int ("net.top_level_only", top_level_only);
  
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
  FILE *fpout;

  proc = initialize_parameters (&argc, &argv, &fpout);

  if (argc != 2) {
    fatal_error ("Something strange happened!");
  }
  if (proc == NULL) {
    fatal_error ("Missing process name!");
  }
  
  a = new Act (argv[1]);
  a->Expand ();
  config_read ("prs2net.conf");
  if (config_exists ("net.mangle_chars")) {
    a->mangle (config_get_string ("net.mangle_chars"));
  }

  Process *p = a->findProcess (proc);

  if (!p) {
    fatal_error ("Could not find process `%s' in file `%s'", proc, argv[1]);
  }

  if (!p->isExpanded()) {
    fatal_error ("Process `%s' is not expanded.", proc);
  }

  ActNetlistPass *np = new ActNetlistPass (a);
  if (enable_shared_stat) {
    np->enableSharedStat();
  }
  np->run (p);
  np->Print (fpout, p);
  if (fpout != stdin) {
    fclose (fpout);
  }

  return 0;
}
