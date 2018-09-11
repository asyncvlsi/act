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
#include <act/netlist.h>
#include "config.h"


static void usage (char *name)
{
  fprintf (stderr, "Usage: %s <actfile> <process> <stkfile>\n", usage);
  exit (1);
}


int main (int argc, char **argv)
{
  Act *a;
  char *proc;

  Act::Init (&argc, &argv);

  if (argc != 4) {
    usage (argv[0]);
  }

  a = new Act (argv[1]);
  a->Expand ();
  config_read ("prs2net.conf");

  Process *p = a->findProcess (argv[2]);

  if (!p) {
    fatal_error ("Could not find process `%s' in file `%s'", argv[2], argv[1]);
  }

  if (!p->isExpanded()) {
    fatal_error ("Process `%s' is not expanded.", argv[2]);
  }

  act_prs_to_netlist (a, p);

  /* stack generation! */

  return 0;
}
