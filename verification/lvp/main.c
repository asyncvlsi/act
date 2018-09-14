/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

/*
 *
 * Compare layout against the production rule set.
 *
 */

#include <stdio.h>
#include "lvs.h"
#include "misc.h"
#include "cap.h"
#include "config.h"

/*
 *
 *  Global options
 *
 */
char *Vddnode;			/* node name for power supply */

char *GNDnode;			/* node name for ground */

int check_staticizers;		/* report missing staticizers */

int strip_by_width;		/* flag */

int and_hack;			/* and hack */

int pass_gates;			/* pass gates */

int dont_strip_bang;		/* don't strip trailing "!" */

double strip_threshold;		/* w/l ratio for "weakness" */

double width_threshold;		/* width ration */

int print_only;			/* only print prs */

int pr_aliases;			/* print out aliases */

int verbose;			/* verbosity */

int debug_level;		/* debugging level */

int connect_globals;		/* connect global nodes */

int outputs_by_name;		/* use # to identify output nodes */

int print_differences;		/* print differences */

int extract_file;		/* use extract file */

pp_t *PPout;                    /* output error stream */

pp_t *PPdump;			/* normal output stream */

int exit_status;		/* exit status */

int warnings;			/* warning option */

int only_check_connects;        /* check alias lists only */

int use_dot_separator;		/* use . as subcircuit separator */

double strength_ratio_up;	/* reset pull-up strength ratio */

double strength_ratio_dn;	/* reset pull-down strength ratio */

int connect_warn_only;		/* only warn on connections */

int no_sneak_path_check;	/* don't check sneak paths */

int dump_hier_file;		/* create output dump */
int dump_hier_force;

int connect_globals_in_prs;     /* connect global names in prs file only */

int wizard;			/* wizard option */

double N_P_Ratio;		/* ^weak strength ratio */

double cap_coupling_ratio;	/* ratio for cap coupling */

int digital_only;		/* don't check analog problems */

double gate_cap;		/* gate area cap (F/m^2) */

double comb_threshold;	        /* charge-sharing ratio */

double stateholding_threshold;	/* state-holding ratio */

double Vdd_value;		/* vdd */

double Vtn_value;		/* n threshold */

double Vtp_value;		/* p threshold */

double lambda;			/* lambda */

double min_gate_length;		/* in lambda */

Table *PThresholdTable;		/* threshold estimators */
Table *NThresholdTable;
Table *PCombThresholdTable;
Table *NCombThresholdTable;

char *bsim_library_path;	/* model parameters */

int echo_external_voltage;	/* off */

int overkill_mode;		/* use spice to check each input case */

#if 0
double BSIM2_OXE = -1;
double BSIM2_TOX = -1;
double BSIM2_N_LDAC = -1;
double BSIM2_N_CGD0 = -1;
double BSIM2_P_LDAC = -1;
double BSIM2_P_CGD0 = -1;

double BSIM2_N_CJ = -1;
double BSIM2_N_MJ = -1;
double BSIM2_N_PB = -1;
double BSIM2_N_CJSW = -1;
double BSIM2_N_MJSW = -1;
double BSIM2_N_PHP = -1;
double BSIM2_P_CJ = -1;
double BSIM2_P_MJ = -1;
double BSIM2_P_PB = -1;
double BSIM2_P_CJSW = -1;
double BSIM2_P_MJSW = -1;
double BSIM2_P_PHP = -1;
#endif

double N_gate_perim;		/* perim cap (from bsim2) */

double P_gate_perim;		/* area cap (from bsim2) */

int dump_pchg_paths;		/* dump paths  */

int display_all_bumps;		/* show all bumps */

int prefix_reset;		/* special _xResety! connection directive */

/*------------------------------------------------------------------------
 *
 *  Usage message
 *
 *------------------------------------------------------------------------
 */
static
void usage (void)
{
  char *usage_str[] = {
    "Usage: lvs [options] <sim|ext file> [<prsfile>]",
    "",
    "lvs compares a flat sim or extract file against a production rule file",
    "(<prsfile>). If the <prsfile> is omitted, it is read from stdin.",
    "",
    "The program handles shared gate networks. To enable correct gate",
    "generation, the production rule file can contain exclhi(n1,n2,...,nN)",
    "directives to specify that nodes n1 thru nN are exclusive high and",
    "similar excllo(n1,n2,...,nN) directives to specify that nodes n1 thru",
    "nN are exclusive low.",
    "",
    "Dangling inverters with weak feedback are assumed to be part of",
    "staticizers. Reset, Reset!, _Reset, Reset_, _Reset!, and Reset_! are",
    "special names which can be used with NAND and NOR gate configuration",
    "staticizers.",
    "",
    "By default, the input is assumed to be a sim file; use the -E option to",
    "use an extract file as input.",
    "",
    "Options:",
    " -a         print alias information [off]",
    " -b         connect global nodes for production-rule check [off]",
    " -c         don't set exit status when layout connections missing [off]",
    " -d         skip charge-sharing checks [off]",
    " -e         echo final output voltage during charge sharing [off]",
    " -f         print differences between layout and prs files [off]",
    " -g         keep trailing \"!\" for globals; don't strip it [off]",
    " -h         nodes ending in \"&\" are not output nodes [off]",
    " -i         print gate list from Vdd/GND to precharged node [off]",
    " -n         treat named nodes as output nodes [off]",
    " -o ratio   fraction of coupling to take into account [0.25]",
    " -p         print production rules from layout [off]",
    " -r ratio   transistors with W/L less than \"ratio\" are weak [off]",
    " -s         check if nodes are staticized [off]",
    " -v         be verbose; repeat option for even more verbose output [off]",
    " -w width   transistors with width less than \"width\" are weak [3.1]",
    " -z level   debugging level [0]",
    " -B         connect global nodes (nodes with a \"!\") automatically [off]",
    " -C         terminate after checking connections [off]",
    " -D         use \".\" as subcircuit separator instead of \"/\" [off]",
    " -E         input is an extract file [off]",
    " -G name    use \"name\" as GND [GND]",
    " -H         hierarchical analysis (requires -sE) [off]",
    " -K         overkill mode for charge-sharing analysis [off]",
    " -P         generate pass transistors (n passes GND, p passes Vdd) [off]",
    " -R         merge _xResety signals with _Reset [off]",
    " -S         don't look for sneak paths [off]",
    " -V name    use \"name\" as Vdd [Vdd]",
    " -W         return non-zero exit status on any warnings [off]",
    "",
    "Options -r and -w are exclusive. Defaults are in square brackets.",
    "If the -p option is used, only one filename argument is accepted.",
    "",
    NULL
  };
  int i = 0;

  while (usage_str[i]) {
    pp_printf (PPout, "%s", usage_str[i]);
    pp_forced (PPout, 0);
    i++;
  }
  pp_flush (PPout);
}

/*------------------------------------------------------------------------
 *
 *  Parse arguments
 *
 *------------------------------------------------------------------------
 */

/* 
   do this so that the tech defaults can be overridden by command-line
   options
*/
static struct xxx {
  double *d;
  double v;
  struct xxx *next;
} *cmd_tl, *cmd_hd = NULL;

static void push_cmdline_options (double *var, double val)
{
  struct xxx *x;
  MALLOC (x, struct xxx, 1);
  x->d = var;
  x->v = val;
  x->next = NULL;
  if (!cmd_hd)
    cmd_hd = cmd_tl = x;
  else {
    cmd_tl->next = x;
    cmd_tl = x;
  }
}

static void do_cmdline_options (void)
{
  while (cmd_hd) {
    cmd_tl = cmd_hd;
    *(cmd_hd->d) = cmd_hd->v;
    if (cmd_hd->d == &strip_threshold)
      strip_by_width = 0;
    else if (cmd_hd->d == &width_threshold)
      strip_by_width = 1;
    cmd_hd = cmd_hd->next;
    FREE (cmd_tl);
  }
}

static
void parse_arguments (int argc, char **eargv, char **file1, char **file2)
{
  extern int opterr, optind;
  extern char *optarg;
  char ch;
  char **argv;
  int i;
  int rspec=0;
  int wspec=0;
  int readconfig = 0;

  if (getenv ("LVS_OPTIONS")) {
    MALLOC (argv, char *, argc+1);
    argv[0] = eargv[0];
    argv[1] = getenv ("LVS_OPTIONS");
    for (i=1; i < argc; i++)
      argv[1+i] = eargv[i];
    eargv = NULL;
    argc++;
  }
  else {
    argv = eargv;
  }

  /* set defaults */
  Vddnode = "Vdd";
  GNDnode = "GND";
  check_staticizers = 0;
  strip_threshold = 3.0;
  verbose = 0;
  strip_by_width = 1;
  width_threshold = 3.1;
  debug_level = 0;
  pass_gates = 0;
  dont_strip_bang = 0;
  print_only = 0;
  pr_aliases = 0;
  connect_globals = 0;
  outputs_by_name = 0;
  print_differences = 0;
  extract_file = 0;
  warnings = 0;
  only_check_connects = 0;
  use_dot_separator = 0;
  strength_ratio_up = 20;
  strength_ratio_dn = 5;
  connect_warn_only = 0;
  no_sneak_path_check = 0;
  dump_hier_file = 0;
  dump_hier_force = 0;
  connect_globals_in_prs = 1;
  wizard = 0;
  N_P_Ratio = 0.5;
  cap_coupling_ratio = 0.25;
  digital_only = 0;

  Vdd_value = 3.3;
  Vtn_value = 0.7;
  Vtp_value = 0.9;
  lambda = 0.3e-6;
  min_gate_length = 2;

  PThresholdTable = NULL;
  NThresholdTable = NULL;
  PCombThresholdTable = NULL;
  NCombThresholdTable = NULL;
  bsim_library_path = NULL;
  echo_external_voltage = 0;
  
  comb_threshold = 1.1;
  stateholding_threshold = 0.57;
  overkill_mode = 0;
  dump_pchg_paths = 0;
  display_all_bumps = 0;
  prefix_reset = 0;

  opterr = 0;
  while ((ch=getopt (argc,argv,"bHcCWEfnBapgRPDz:hvr:w:sV:G:SZo:deKi"))!=-1){
    switch (ch) {
    case 'R':
      prefix_reset = 1;
      break;
    case 'i':
      dump_pchg_paths = 1;
      break;
    case 'K':
      display_all_bumps = overkill_mode;
      overkill_mode = 1;
      break;
    case 'e':
      echo_external_voltage = 1;
      break;
    case 'd':
      digital_only = 1;
      break;
    case 'o':
      sscanf (optarg, "%lf", &cap_coupling_ratio);
      break;
    case 'Z':
      wizard = 1;
      break;
    case 'H':
      if (dump_hier_file) dump_hier_force = 1;
      dump_hier_file = 1;
      break;
    case 'S':
      no_sneak_path_check = 1;
      break;
    case 'c':
      connect_warn_only = 1;
      break;
    case 'D':
      use_dot_separator = 1;
      break;
    case 'C':
      only_check_connects = 1;
      break;
    case 'W':
      warnings = 1;
      break;
    case 'E':
      extract_file = 1;
      break;
    case 'f':
      print_differences = 1;
      break;
    case 'n':
      outputs_by_name = 1;
      break;
    case 'b':
      connect_globals_in_prs = 1;
      break;
    case 'B':
      connect_globals = 1;
      break;
    case 'a':
      pr_aliases = 1;
      break;
    case 'p':
      print_only = 1;
      break;
    case 'g':
      dont_strip_bang = 1;
      break;
    case 'P':
      pass_gates = 1;
      break;
    case 'z':
      sscanf (optarg, "%d", &debug_level);
      break;
    case 'h':
      and_hack = 1;
      break;
    case '?':
      usage();
      fatal_error ("Bad option arguments");
      break;
    case 'w':
      wspec=1;
      if (rspec)
	fatal_error ("Specify only one of -r or -w");
      sscanf (optarg, "%lf", &width_threshold);
      push_cmdline_options (&width_threshold, width_threshold);
      strip_by_width = 1;
      break;
    case 'r':
      rspec = 1;
      if (wspec)
	fatal_error ("Specify only one of -r or -w");
      sscanf (optarg, "%lf", &strip_threshold);
      push_cmdline_options (&strip_threshold, strip_threshold);
      strip_by_width = 0;
      break;
    case 's':
      check_staticizers = 1;
      break;
    case 'V':
      MALLOC (Vddnode, char, strlen(optarg)+1);
      strcpy (Vddnode, optarg);
      break;
    case 'G':
      MALLOC (GNDnode, char, strlen(optarg)+1);
      strcpy (GNDnode, optarg);
      break;
    case 'v':
      verbose++;
      break;
    default:
      fatal_error ("getopt seems to be a bit buggy . . .");
      break;
    }
  }
  config_read ("lint.conf");
  config_read ("lvp.conf");

  /* set values */
  Vdd_value = config_get_real ("lint.Vdd");
  Vtn_value = config_get_real ("lvp.Vtn");
  Vtp_value = config_get_real ("lvp.Vtp");
  lambda = config_get_real ("net.lambda");
  min_gate_length = config_get_int ("net.min_length");
  if (config_exists ("lvp.Gatecap")) {
    gate_cap = config_get_real ("lvp.Gatecap");
  }
  if (config_exists ("lvp.WeakByWidth")) {
    width_threshold = config_get_real ("lvp.WeakByWidth");
  }
  if (config_exists ("lvp.WeakByStrength")) {
    strip_threshold = config_get_real ("lvp.WeakByStrength");
  }
  N_P_Ratio = 1.0/config_get_real ("net.p_n_ratio");
  comb_threshold = config_get_real ("lvp.CombThreshold");
  stateholding_threshold = config_get_real ("lvp.StateThreshold");

  do_cmdline_options ();
#ifndef DIGITAL_ONLY
  compute_derived_params ();
#endif
  if (optind < argc-2) {
    usage();
    fatal_error ("Too many arguments.");
  }
  if (optind != argc-1 && optind != argc-2) {
    usage();
    fatal_error ("Missing file name.");
  }
  if (optind == argc-1) {
    *file1 = argv[optind];
    *file2 = NULL;
  }
  else {
    if (print_only) {
      usage ();
      fatal_error ("Too many arguments.");
    }
    *file1 = argv[optind];
    *file2 = argv[optind+1];
  }
  
  if (dump_hier_file && 
      (!extract_file || no_sneak_path_check 
       ||  (!dump_hier_force && connect_globals)
       || !check_staticizers || connect_warn_only || print_only )) {
    usage ();
    pp_printf (PPout, "-H requires: -sE");
    pp_forced (PPout, 0);
    pp_printf (PPout, "-H excludes: -cBSp");
    pp_forced (PPout, 0);
    fatal_error ("Hierarchical analysis incompatible with these options.");
  }

  if (wizard) {
#if 0
    if (getuid() != 8219 && getuid() != 8328 && getuid() != 8069 && getuid () != 8259) {
      pp_printf (PPout, "You don't look like a wizard to me.");
      pp_forced (PPout, 0);
      exit (1);
    }
#endif
  }
  if (eargv == NULL)
    FREE (argv);
  
  return;
}

static
int exists_file (char *s)
{
  FILE *fp;
  if ((fp = fopen (s, "r"))) {
    fclose (fp);
    return 1;
  }
  return 0;
}

/*------------------------------------------------------------------------
 *
 *  LVS: main program
 *
 *------------------------------------------------------------------------
 */
int main (int argc, char **argv)
{
  FILE *fp1, *fp2, *al, *dmp;
  char *file1, *file2;
  char *sim;
  char *orig;
  char *dump;

  exit_status = 0;
  PPout = pp_init (stderr, 72);

  /* parse the arguments */
  parse_arguments (argc, argv, &file1, &file2);

  /* compare files */
  orig = file1;
  if (!exists_file (file1)) {
    /* try file1.sim/.ext */
    MALLOC (sim, char, strlen (file1)+5);
    strcpy (sim, file1);
    if (extract_file)
      strcat (sim, ".ext");
    else
      strcat (sim, ".sim");
    file1 = sim;
  }
  if (exists_file (file1)) {
    fp1 = fopen (file1, "r");
    /* strip extension, and append .al */
    sim = file1+strlen(file1)-1;
    while (sim != file1 && *sim != '.')
      sim--;
    if (sim != file1)
      *sim = '\0';
    MALLOC (sim, char, strlen(file1)+4);
    strcpy (sim, file1);

    MALLOC (dump, char, strlen (file1)+5);
    strcpy (dump, file1);

    strcat (dump, ".hxt");
    strcat (sim, ".al");
    if (exists_file (sim))
      al = fopen (sim, "r");
    else
      al = NULL;

    if (dump_hier_file) {
      dmp = fopen (dump, "w");
      if (!dmp)
	fatal_error ("Unable to open dump file %s for writing.\n",
		     dump);
    }
    else
      dmp = NULL;
  }
  else
    fatal_error ("Unable to open file %s for reading.\n", orig);

  if (file2) {
    if (!(fp2 = fopen (file2, "r")))
      fatal_error ("Unable to open file %s for reading.\n", file2);
  }
  else
    fp2 = stdin;

  if (print_only || pr_aliases)
    PPdump = pp_init (stdout, 72);
  else
    PPdump = NULL;
  lvs (fp1, fp2, al, dmp);
  pp_close (PPout);
  if (print_only || pr_aliases) pp_close (PPdump);
  if (dmp) fclose (dmp);
  return exit_status;
}
