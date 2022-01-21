/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __LVS_H__
#define __LVS_H__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "var.h"
#include "act/common/pp.h"
#include "table.h"

/*
 *
 * Globals
 *
 */
extern double Vdd_value;
extern double Vtn_value;
extern double Vtp_value;
extern double min_gate_length;
extern double lambda;
extern Table *PThresholdTable;
extern Table *NThresholdTable;
extern Table *PCombThresholdTable;
extern Table *NCombThresholdTable;
extern char *bsim_library_path;

extern char *Vddnode;			/* node name for power supply */

extern char *GNDnode;			/* node name for ground */

extern int check_staticizers;		/* report missing staticizers */

extern int strip_by_width;	        /* what type of filter */

extern int and_hack;		        /* hack for funny precharge */

extern int pass_gates;		        /* enable pass gates */

extern int dont_strip_bang;	        /* keep globals */

extern double strip_threshold;		/* w/l ratio for "weakness" */

extern double width_threshold;	        /* width threshold */

extern int print_only;		        /* print prs */

extern int pr_aliases;	                /* print alias information */

extern int verbose;			/* verbosity */

extern int debug_level;		        /* debugging level */

extern int connect_globals;	        /* connect globals */

extern int outputs_by_name;	        /* output nodes by name  */

extern int print_differences;	        /* print differences in two nodes */

extern int extract_file;	        /* use extract file */

extern pp_t *PPout;		        /* pretty printer */

extern pp_t *PPdump;		        /* output stream */

extern int exit_status;		        /* exit status */

#if 0
extern int warnings;		        /* warnings */
#endif

extern int only_check_connects;	        /* check connections only */

extern int use_dot_separator;	        /* use . as subcircuit separator */

extern double strength_ratio_up;	/* reset pull-up strength ratio */

extern double strength_ratio_dn;	/* reset pull-dn strength ratio */

extern int connect_warn_only;	        /* only warn on connections */

extern int no_sneak_path_check;	        /* don't check sneak paths */

extern int dump_hier_file;	        /* create output dump */
extern int dump_hier_force;

extern int connect_globals_in_prs;      /* connect globals in prs file only */

extern int wizard;		        /* wizard */

extern double N_P_Ratio;	        /* ^weak strength ratios */

extern double cap_coupling_ratio;       /* ratio for capacitive coupling */

extern double P_gate_perim, N_gate_perim; /* calculated from BSIM2 */

extern int digital_only;		/* don't check analog problems */

extern int echo_external_voltage;	/* off */

extern double gate_cap;		        /* gate cap */

extern double comb_threshold;	        /* charge-sharing ratio */

extern double stateholding_threshold;   /* state-holding ratio */

extern int overkill_mode;	/* overkill mode */

extern int dump_pchg_paths;	/* print pchg paths */

extern int display_all_bumps;	/* show all scenario/bump combinations */

extern int prefix_reset;	/* special _xResety! connection directive */

extern void lvs (char *name, FILE *, FILE *, FILE *, FILE *);
extern void gen_prs (VAR_T *, BOOL_T *);
extern void check_prs (VAR_T *, BOOL_T *);
extern void print_prs (VAR_T *, BOOL_T *);
extern void print_aliases (VAR_T *);
extern void print_bool (pp_t *, BOOL_T *, VAR_T *, bool_t *, int);
extern void print_slow_special (pp_t *, BOOL_T *, VAR_T *, bool_t *, int);
extern void name_convert (char *, dots_t *);
extern void array_fixup (char *s);
extern void validate_name (var_t *, dots_t *);
extern void check_sneak_paths (VAR_T *);
extern void check_cap_ratios (VAR_T *);
extern void save_io_nodes (VAR_T *, FILE *, unsigned long);
int generate_summary_anyway (void);

extern int extra_exclhi (var_t *, var_t *);
extern int extra_excllo (var_t *, var_t *);
extern bool_t *compute_additional_local_p_invariant (BOOL_T *, var_t *);
extern bool_t *compute_additional_local_n_invariant (BOOL_T *, var_t *);
extern void add_exclhi_list (struct excl *);
extern void add_excllo_list (struct excl *);
extern struct excl *add_to_excl_list (struct excl *, var_t *);
extern char *expand (char *s);
extern void read_config_file (char *tech);
double operator_strength (var_t *node, int type, var_t *vdd, var_t *gnd);
double strong_ratio (var_t *node, int type, var_t *vdd, var_t *gnd);
extern var_t *canonical_name (var_t *);
extern void check_precharges (VAR_T *V, var_t *vdd, var_t *gnd);
extern void inc_pchg_errors (void);
extern void inc_naming_violations (void);
extern void inc_sneak_paths (void);

void flatten_ext_file (struct ext_file *ext, VAR_T *V);
void initialize (int *argc, char ***argv);


#define MAXLINE 1024
#define MAXNAME 1024


#endif /* __LVS_H__ */
