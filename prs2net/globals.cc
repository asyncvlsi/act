/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "misc.h"
#include "config.h"
#include "globals.h"
#include <act/act.h>

/* 
   Global variables for netlist generation.
*/

int verbose;

double lambda;			/* scale factor from width expressions
				   to absolute units */

int min_w_in_lambda;		/* minimum width in lambda */
int min_l_in_lambda;		/* minimum length in lambda */

int max_n_w_in_lambda;		/* maximum n-width in lambda */
int max_p_w_in_lambda;		/* maximum p-width in lambda	*/

/* standard transistor sizes */
int std_p_width;
int std_p_length;
int std_n_width;
int std_n_length;

/* standard staticizer sizes */
int stat_p_width;
int stat_p_length;
int stat_n_width;
int stat_n_length;

/* strength ratios */
double p_n_ratio;
double weak_to_strong_ratio;

/* internal diffusion */
int fet_spacing_diffonly;
int fet_spacing_diffcontact;
int fet_diff_overhang;

int top_level_only;		/* only emit top-level cell! */

int emit_parasitics;		/* emit parasitic source/drain area/perim */

int black_box_mode;		/* 1 if implicit blackbox */

/* load cap on each node */
double default_load_cap;

int ignore_loadcap;		/* ignore loadcap directives (lvs only) */

/* discrete widths */
double *fet_width_ranges;
int fet_width_num;

/* discrete lengths */
int discrete_length;

/* fet extra string */
char *extra_fet_string;

/* disable staticizers/keepers */
int disable_keepers;

/* swap source and drain */
int swap_source_drain;

/* use subckt models */
int use_subckt_models;

/* fold transistors */
int fold_nfet_width;
int fold_pfet_width;


FILE *fpout;			/* output file */

int emit_verilog;
FILE *fvout;			/* output verilog module file */

int emit_pinfo;
FILE *fpinfo;			/* output pinfo file */

int omit_printing;

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
  fprintf (stderr, "Usage: %s [-vdltBR] [-C <conf>] [-p <proc>] [-o <file>] [-V <file>] <act>\n", name);
  fprintf (stderr, " -C <conf> Configuration file name\n");
  fprintf (stderr, " -t        Only emit top-level cell (no sub-cells)\n");
  fprintf (stderr, " -p <proc> Emit process <proc>\n");
  fprintf (stderr, " -o <file> Save result to <file> rather than stdout\n");
  fprintf (stderr, " -d	       Emit parasitic source/drain diffusion area/perimeters with fets\n");
  fprintf (stderr, " -B	       Black-box mode. Assume empty act process is an external .sp file\n");
  fprintf (stderr, " -V <file> Print Verilog module for the top level in <file> and port information on <file>.port_info \n");
  fprintf (stderr, " -l	       LVS netlist; ignore all load capacitances\n");
  fprintf (stderr, " -v	       Increase verbosity level\n");
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

  verbose = 0;
  omit_printing = 0;
  emit_verilog = 0;
  emit_pinfo = 0;
  fpout = stdout;
  top_level_only = 0;
  conf_file = NULL;
  emit_parasitics = 0;
  fet_width_num = 0;
  fet_width_ranges = NULL;
  extra_fet_string = NULL;
  black_box_mode = 0;
  disable_keepers = 0;
  ignore_loadcap = 0;
  proc_name = NULL;

  while ((ch = getopt (*argc, *argv, "BdC:tp:o:RlX:V:v")) != -1) {
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

    case 'v':
      verbose++;
      break;

    case 'V':
      if (optarg == 0) {
	usage ((*argv)[0]);
      }
      char port_info_f[1024];
      strncpy (port_info_f, optarg, 1000);
      strcat (port_info_f, ".port_info");
      if (verbose) {
	fprintf (stderr, "Printing Verilog module to file %s and port information in %s\n", optarg, port_info_f);
      }
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

  /* read config file */
  if (has_trailing_extension (conf_file, "conf")) {
    config_read (conf_file);
  }
  else {
    char buf[10240];
    sprintf (buf, "%s.conf", conf_file);
    config_read (buf);
  }

  lambda = config_get_real ("lambda");

  min_w_in_lambda = config_get_int ("min_width");
  min_l_in_lambda = config_get_int ("min_length");

  max_n_w_in_lambda = config_get_int ("max_n_width");
  max_p_w_in_lambda = config_get_int ("max_p_width");

  std_p_width = config_get_int ("std_p_width");
  std_p_length = config_get_int ("std_p_length");
  std_n_width = config_get_int ("std_n_width");
  std_n_length = config_get_int ("std_n_length");

  stat_p_width = config_get_int ("stat_p_width");
  stat_p_length = config_get_int ("stat_p_length");
  stat_n_width = config_get_int ("stat_n_width");
  stat_n_length = config_get_int ("stat_n_length");

  p_n_ratio = config_get_real ("p_n_ratio");
  weak_to_strong_ratio = config_get_real ("weak_to_strong_ratio");

  fet_spacing_diffonly = config_get_int ("fet_spacing_diffonly");
  fet_spacing_diffcontact = config_get_int ("fet_spacing_diffcontact");
  fet_diff_overhang = config_get_int ("fet_diff_overhang");

  default_load_cap = config_get_real ("default_load_cap");

  extra_fet_string = config_get_string ("extra_fet_string");

  disable_keepers = config_get_int ("disable_keepers");

  act_cmdline = config_get_string ("act_cmdline");

  discrete_length = config_get_int ("discrete_length");

  swap_source_drain = config_get_int ("swap_source_drain");

  use_subckt_models = config_get_int ("use_subckt_models");

  fold_pfet_width = config_get_int ("fold_pfet_width");
  fold_nfet_width = config_get_int ("fold_nfet_width");

  if (config_exists ("width_limits")) {
    int i;
    fet_width_num = config_get_table_size ("width_limits");
    if (fet_width_num % 2 == 1) {
      fatal_error ("width limit table must contain an even number of entries");
    }
    if (fet_width_num > 0) {
      fet_width_ranges = config_get_table_real ("width_limits");
      for (i=0; i < fet_width_num-1; i++) {
	if (fet_width_ranges[i] > fet_width_ranges[i+1]) {
	  fatal_error ("width_limits: Range values must be monotonically increasing");
	}
      }
      fet_width_num /= 2;
    }
  }

  if (!proc_name) {
    fprintf (stderr, "Missing process name.\n");
    usage ((*argv)[0]);
  }

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
