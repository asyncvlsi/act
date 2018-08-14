/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <act/act.h>
#include "globals.h"
#include "config.h"
#include "netlist.h"

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
  act_emit_netlist (a, p, fpout);

  if (emit_verilog || emit_pinfo) {
    emit_verilog_pins (a, fvout, fpinfo, p);
  }

  return 0;
}
