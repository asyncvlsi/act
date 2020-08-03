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
#include <act/passes/cells.h>
#include "config.h"


static void usage (char *name)
{
  fprintf (stderr, "Usage: %s [act-options] <actfile> <cellin> <cellout>\n", name);
  exit (1);
}


int main (int argc, char **argv)
{
  Act *a;
  char *proc;
  FILE *fp;

  Act::Init (&argc, &argv);

  if (argc != 4) {
    usage (argv[0]);
  }

  a = new Act (argv[1]);
  a->Merge (argv[2]);
  a->Expand ();
  /* for each expanded ACT process, read in cells */

  /* for each ACT process, find production rules and try and match
     to cells */

  ActCellPass *cp = new ActCellPass (a);
  cp->run();

  /* now emit new cells file */
  fp = fopen (argv[3], "w");
  if (!fp) {
    fatal_error ("Could not open file `%s' for writing", argv[3]);
  }
  cp->Print (fp);
  fclose (fp);

  a->Print (stdout);

  return 0;
}
