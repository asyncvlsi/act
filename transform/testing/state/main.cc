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
#include <act/passes.h>
#include <common/config.h>


static void usage (char *name)
{
  fprintf (stderr, "Usage: %s [act-options] [-v] <actfile> <process>\n", name);
  exit (1);
}


int main (int argc, char **argv)
{
  Act *a;
  char *proc;
  bool verbose = false;
  int shift = 0;

  /* initialize ACT library */
  Act::Init (&argc, &argv);

  /* some usage check */
  if (argc != 3 && argc != 4) {
    usage (argv[0]);
  }
  if (argc == 4 && strcmp (argv[1], "-v") != 0) {
    usage (argv[0]);
  }
  else if (argc == 4) {
    verbose = true;
    shift = 1;
  }

  /* read in the ACT file */
  a = new Act (argv[1+shift]);

  /* expand it */
  a->Expand ();
 
  /* find the process specified on the command line */
  Process *p = a->findProcess (argv[2+shift]);

  if (!p) {
    fatal_error ("Could not find process `%s' in file `%s'", argv[2+shift], argv[1+shift]);
  }

  if (!p->isExpanded()) {
    fatal_error ("Process `%s' is not expanded.", argv[2+shift]);
  }

  /* do stuff here */
  ActStatePass *sp = new ActStatePass (a);
  sp->run (p);
  sp->setVerbose (verbose);

  sp->Print (stdout, p);

//  ActBooleanizePass *bp = dynamic_cast<ActBooleanizePass *>(a->pass_find ("booleanize"));
//  bp->Print (stdout, p);

  return 0;
}
