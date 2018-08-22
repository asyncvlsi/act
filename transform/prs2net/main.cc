/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <act/act.h>
#include "config.h"
#include "netlist.h"

FILE *fpout;			/* output file */
int emit_verilog;
FILE *fvout;			/* output verilog module file */
int emit_pinfo;
FILE *fpinfo;			/* output pinfo file */


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
  fprintf (stderr, "Usage: %s [-dltBR] [-C <conf>] [-p <proc>] [-o <file>] [-V <file>] <act>\n", name);
  fprintf (stderr, " -C <conf> Configuration file name\n");
  fprintf (stderr, " -t        Only emit top-level cell (no sub-cells)\n");
  fprintf (stderr, " -p <proc> Emit process <proc>\n");
  fprintf (stderr, " -o <file> Save result to <file> rather than stdout\n");
  fprintf (stderr, " -d	       Emit parasitic source/drain diffusion area/perimeters with fets\n");
  fprintf (stderr, " -B	       Black-box mode. Assume empty act process is an external .sp file\n");
  fprintf (stderr, " -V <file> Print Verilog module for the top level in <file> and port information on <file>.port_info \n");
  fprintf (stderr, " -l	       LVS netlist; ignore all load capacitances\n");
  exit (1);
}


/*
  Initialize globals from the configuration file.
  Returns process name
*/
char *initialize_parameters (int *argc, char ***argv)
{
  char *conf_file;
  char *proc_name;
  char *act_cmdline;
  int ch;
  int ignore_loadcap = 0;
  int emit_parasitics = 0;
  int black_box_mode = 0;
  int top_level_only = 0;

  emit_verilog = 0;
  emit_pinfo = 0;
  fpout = stdout;
  top_level_only = 0;
  conf_file = NULL;
  proc_name = NULL;

  while ((ch = getopt (*argc, *argv, "BdC:tp:o:lV:")) != -1) {
    switch (ch) {
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
      if (fpout != stdout) {
	fclose (fpout);
      }
      fpout = fopen (optarg, "w");
      if (!fpout) {
	fatal_error ("Could not open file `%s' for writing.", optarg);
      }
      break;

    case 'V':
      if (optarg == 0) {
	usage ((*argv)[0]);
      }
      char port_info_f[1024];
      strncpy (port_info_f, optarg, 1000);
      strcat (port_info_f, ".port_info");
      fvout = fopen (optarg, "w");
      if (!fvout) {
	fatal_error ("Could not open file `%s' for writing.", optarg);
      }
      fpinfo = fopen (port_info_f, "w");
      if (!fpinfo) {
	fatal_error ("Could not open file '%s' for writing", port_info_f);
      }
      emit_verilog = 1;
      emit_pinfo = 1;
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

  if (!conf_file) {
    conf_file = Strdup ("tsmc65.conf");
  }

  config_std_path ("netgen");

  /* set default configurations */
  config_set_default_int ("std_p_width", 5);
  config_set_default_int ("std_p_length", 2);

  config_set_default_int ("std_n_width", 3);
  config_set_default_int ("std_n_length", 2);

  /* min w, l */
  config_set_default_int ("min_width", 2);
  config_set_default_int ("min_length", 2);

  /* max w */
  config_set_default_int ("max_n_width", 0);
  config_set_default_int ("max_p_width", 0);

  /* staticizer sizing */
  config_set_default_int ("stat_p_width", 5);
  config_set_default_int ("stat_p_length", 4);

  config_set_default_int ("stat_n_width", 3);
  config_set_default_int ("stat_n_length", 4);

  /* spacing */
  config_set_default_int ("fet_spacing_diffonly", 4);
  config_set_default_int ("fet_spacing_diffcontact", 8);
  config_set_default_int ("fet_diff_overhang", 6);

  /* strength ratios */
  config_set_default_real ("p_n_ratio", 2.0);
  config_set_default_real ("weak_to_strong_ratio", 0.1);

  config_set_default_real ("lambda", 0.1e-6);

  config_set_default_string ("mangle_chars", "");

  config_set_default_string ("act_cmdline", "");

  config_set_default_real ("default_load_cap", 0);

  config_set_default_string ("extra_fet_string", "");

  config_set_default_int ("disable_keepers", 0);

  config_set_default_int ("discrete_length", 0);

  config_set_default_int ("swap_source_drain", 0);

  config_set_default_int ("use_subckt_models", 0);

  config_set_default_int ("fold_pfet_width", 0);
  config_set_default_int ("fold_nfet_width", 0);

  config_set_default_int ("ignore_loadcap", ignore_loadcap);
  config_set_default_int ("emit_parasitics", emit_parasitics);
  config_set_default_int ("black_box_mode", black_box_mode);
  config_set_default_int ("top_level_only", top_level_only);
  
  /* read config file */
  if (has_trailing_extension (conf_file, "conf")) {
    config_read (conf_file);
  }
  else {
    char buf[10240];
    sprintf (buf, "%s.conf", conf_file);
    config_read (buf);
  }

  if (!proc_name) {
    fprintf (stderr, "Missing process name.\n");
    usage ((*argv)[0]);
  }

  act_cmdline = config_get_string ("act_cmdline");
  
  /* create a new command line for ACT out of act_cmdline */
  int count = 1;
  
  for (int i=0; act_cmdline[i]; i++) {
    if (act_cmdline[i] == ',') {
      count++;
    }
  }
  if (strcmp (act_cmdline, "") == 0) {
    count = 0;
  }

  char **act_argv;
  char *tmp;
  
  MALLOC (act_argv, char *, count+2);
  act_argv[0] = Strdup ((*argv)[0]);
  if (count > 0) {
     tmp = strtok (act_cmdline, ",");
     for (int i=1; i <= count; i++)  {
       act_argv[i] = Strdup (tmp);
       tmp = strtok (NULL, ",");
     }
     Assert (tmp == NULL, "What?");
  }
  act_argv[count+1] = NULL;
  count++;

  Act::Init (&count, &act_argv);
  FREE (act_argv);

  *argc = 2;
  (*argv)[1] = (*argv)[optind];
  (*argv)[2] = NULL;

  return proc_name;
}



int main (int argc, char **argv)
{
  Act *a;
  char *proc;

  proc = initialize_parameters (&argc, &argv);

  if (argc != 2) {
    fatal_error ("Something strange happened!");
  }
  if (proc == NULL) {
    fatal_error ("Missing process name!");
  }
  
  a = new Act (argv[1]);
  a->Expand ();
  a->mangle (config_get_string ("mangle_chars"));

  Process *p = a->findProcess (proc);

  if (!p) {
    fatal_error ("Could not find process `%s' in file `%s'", proc, argv[1]);
  }

  if (!p->isExpanded()) {
    fatal_error ("Process `%s' is not expanded.", proc);
  }

  act_prs_to_netlist (a, p);
  act_create_bool_ports (a, p);
  act_emit_netlist (a, p, fpout);

  if (emit_verilog || emit_pinfo) {
    emit_verilog_pins (a, fvout, fpinfo, p);
  }

  return 0;
}
