/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __NETGEN_GLOBALS_H__
#define __NETGEN_GLOBALS_H__

#include <map>
#include "netlist.h"

/* 
   Global variables initialized from configuration file
*/

extern int verbose;

extern double lambda;			/* scale factor from width expressions
					to absolute units */

extern int min_w_in_lambda;		/* minimum width in lambda */
extern int min_l_in_lambda;		/* minimum length in lambda */

extern int max_n_w_in_lambda;		/* maximum n-width in lambda */
extern int max_p_w_in_lambda;		/* maximum p-width in lambda	*/

/* standard sizes */
extern int std_p_width;
extern int std_p_length;
extern int std_n_width;
extern int std_n_length;

/* staticizer sizes */
extern int stat_p_width;
extern int stat_p_length;
extern int stat_n_width;
extern int stat_n_length;

/* strength ratios */
extern double p_n_ratio;
extern double weak_to_strong_ratio;

/* internal diffusion */
extern int fet_spacing_diffonly;
extern int fet_spacing_diffcontact;
extern int fet_diff_overhang;

extern int top_level_only;	/* only emit top-level cell! */

extern int emit_parasitics;	/* emit parasitic source/drain area/perim */

extern int black_box_mode;	/* 1 if implicit blackbox */

/* load cap on each node */
extern double default_load_cap;

/* resistance in series with the output node */
extern double default_output_resis;

extern int ignore_loadcap;  /* ignore loadcap directives (lvs only) */

/* discrete widths */
extern double *fet_width_ranges;
extern int fet_width_num;

/* discrete lengths */
extern int discrete_length;

/* fet extra string */
extern char *extra_fet_string;

/* disable staticizers/keepers */
extern int disable_keepers;

/* swap source and drain */
extern int swap_source_drain;

/* use subckt models */
extern int use_subckt_models;

/* fold transistors */
extern int fold_nfet_width;
extern int fold_pfet_width;


extern FILE *fpout;		/* output file */

extern int emit_verilog;
extern FILE *fvout;		/* output verilog module file */

extern int emit_pinfo;
extern FILE *fpinfo;		/* output pinfo file */

extern int omit_printing;

char *initialize_parameters (int *argc, char ***argv);

#endif /* __GLOBALS_H__ */
