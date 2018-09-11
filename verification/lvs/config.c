/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#include <stdio.h>
#include "lvs.h"
#include "misc.h"
#include "table.h"
#include "cap.h"


static struct {
  char *name;
  double *variable;
} config_file_options[] = 
{
  { "Vdd", &Vdd_value },
  { "Vtn", &Vtn_value },
  { "Vtp", &Vtp_value },
  { "lambda", &lambda },
  { "min_gate_length", &min_gate_length },
#if 0
  /* 
   * Estimated from bsim2 parameters rather than calibrated from spice
   * output to take nonlinearity of capacitance into account
   */
  { "Ndiffcap", &N_diff_cap },
  { "Pdiffcap", &P_diff_cap },

  /* calculated from thresholds */
  { "N_PchgDv", &N_PchgDv },
  { "P_PchgDv", &P_PchgDv },
#endif

  { "Gatecap", &gate_cap },
  { "WeakByWidth", &width_threshold },
  { "WeakByStrength", &strip_threshold },
  { "NResetPStat", &strength_ratio_dn },
  { "PResetNStat", &strength_ratio_up },
  { "PWeakNStrong", &N_P_Ratio },
  { "CombThreshold", &comb_threshold },
  { "StateThreshold", &stateholding_threshold },

  { "BSIM2_OXE", &BSIM2_OXE },
  { "BSIM2_TOX", &BSIM2_TOX },
  { "BSIM2_N_LDAC", &BSIM2_N_LDAC },
  { "BSIM2_N_CGD0", &BSIM2_N_CGD0 },
  { "BSIM2_P_LDAC", &BSIM2_P_LDAC },
  { "BSIM2_P_CGD0", &BSIM2_P_CGD0 },

  /*
   *   *** NOTE ***
   *
   *   The program uses strncmp to match parameters. If one parameter is
   *   the prefix of another one, it will match first! So put the longer
   *   one first.
   *
   */

  { "BSIM2_N_CJSW", &BSIM2_N_CJSW },
  { "BSIM2_N_MJSW", &BSIM2_N_MJSW },
  { "BSIM2_N_CJ", &BSIM2_N_CJ },
  { "BSIM2_N_MJ", &BSIM2_N_MJ },
  { "BSIM2_N_PB", &BSIM2_N_PB },
  { "BSIM2_N_PHP", &BSIM2_N_PHP },

  { "BSIM2_P_CJSW", &BSIM2_P_CJSW },
  { "BSIM2_P_MJSW", &BSIM2_P_MJSW },
  { "BSIM2_P_CJ", &BSIM2_P_CJ },
  { "BSIM2_P_MJ", &BSIM2_P_MJ },
  { "BSIM2_P_PB", &BSIM2_P_PB },
  { "BSIM2_P_PHP", &BSIM2_P_PHP },

  { NULL, NULL }
};

static struct {
  char *name;
  Table **table;
} config_file_tables[] =
{
  { "PThresholdTable", &PThresholdTable },
  { "NThresholdTable", &NThresholdTable },
  { "PCombThresholdTable", &PCombThresholdTable },
  { "NCombThresholdTable", &NCombThresholdTable },
  { NULL, NULL }
};

#define MODEL0 "BSIM2_LIBRARY_PATH"
#define MODEL1 "BSIM3_LIBRARY_PATH"
#define MVAR bsim_library_path

static
int parse_table (char *buf, FILE *fp)
{
  int j;
  double offset;
  Table *t;
  double idx, val;

  for (j=0; config_file_tables[j].name; j++) {
    if (strncmp (buf+1, config_file_tables[j].name,
		 strlen(config_file_tables[j].name)) == 0) {
      sscanf (buf+strlen(config_file_tables[j].name)+2, "%lf", &offset);
      break;
    }
  }
  if (!config_file_tables[j].name)
    return -1;
  if (!*(config_file_tables[j].table))
    *(config_file_tables[j].table) = table_create ();
  t = *(config_file_tables[j].table);
  while (fgets (buf, 1024, fp)) {
    if (buf[0] == '#' || buf[0] == '\0' || buf[0] == '\n') continue;
    if (strncmp (buf, "+EndTable", 9) == 0) return j;
    sscanf (buf, "%lf %lf", &idx, &val);
    val += offset;
    table_add (t, idx, val);
  }
  return -2;
}

static
int parse_option (char *buf, FILE *fp)
{
  int j;
#ifndef DIGITAL_ONLY
#ifndef OLD_ASPICE
  extern int BSIM_MODEL_TYPE;
#endif
#endif

  if (buf[0] == '+')
    return parse_table (buf, fp);
  for (j=0; config_file_options[j].name; j++) {
    if (strncmp (buf, config_file_options[j].name,
		 strlen (config_file_options[j].name)) == 0) {
      sscanf (buf+strlen (config_file_options[j].name)+1, "%lf", 
	      config_file_options[j].variable);
      return j;
    }
  }
  if (strncmp (buf, MODEL0, strlen(MODEL0)) == 0) {
#ifndef DIGITAL_ONLY    
#ifndef OLD_ASPICE
    BSIM_MODEL_TYPE = 5;	/* bsim2 */
#endif
#endif    
    MALLOC (MVAR, char, strlen (buf)+1);
    sscanf (buf+strlen(MODEL0)+1, "%s", MVAR);
    return j;
  }
  if (strncmp (buf, MODEL1, strlen(MODEL1)) == 0) {
#ifndef DIGITAL_ONLY    
#ifndef OLD_ASPICE
    BSIM_MODEL_TYPE = 2;	/* bsim3 */
#endif
#endif    
    MALLOC (MVAR, char, strlen (buf)+1);
    sscanf (buf+strlen(MODEL1)+1, "%s", MVAR);
    return j;
  }
  return -1;
}

void read_config_file (char *tech)
{
  char *file;
  FILE *fp;
  char buf[1024];

  sprintf (buf, "%s.conf", tech);
  file = expand (buf);
  if (!(fp = fopen (file, "r"))) {
    FREE (file);
    if ((file = getenv ("ACT_HOME"))) {
      sprintf (buf, "%s/lib/lvs/%s.conf", file, tech);
      if (!(fp = fopen (buf, "r")))
	fatal_error ("Could not find technology file for `%s'", tech);
    }
    else
      fatal_error ("Could not find technology file for `%s'", tech);
  }
  else 
    FREE (file);
  
  while (fgets (buf, 1024, fp)) {
    if (buf[0] == '#' || buf[0] == '\0' || buf[0] == '\n') continue;
    if (strncmp (buf, "WeakByWidth", 11) == 0)
      strip_by_width = 1;
    else if (strncmp (buf, "WeakByStrength", 15) == 0)
      strip_by_width = 0;
    if (parse_option (buf, fp) == -1)
      pp_printf_raw (PPout, "%s: unknown config option line\n%s",
		     tech, buf);
  }
}
