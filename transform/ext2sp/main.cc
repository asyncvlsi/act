/*************************************************************************
 *
 *  Copyright (c) 2019 Rajit Manohar
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
#include <stdlib.h>
#include "ext.h"

double mincap = 0.0;

static void usage (char *name)
{
  fprintf (stderr, "Usage: %s [-c <mincap>] <file.ext>\n", name);
  fprintf (stderr, " -c <mincap> : filter caps at or below this threshold\n");
  exit (1);
}


int main (int argc, char **argv)
{
  int ch;
  extern int optind, opterr;
  extern char *optarg;
  struct ext_file *E;

  while ((ch = getopt (argc, argv, "c:")) != -1) {
    switch (ch) {
    case 'c':
      mincap = atof (optarg);
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  if (optind != argc - 1) {
    fprintf (stderr, "Missing extract file name\n");
    usage (argv[0]);
  }

  ext_validate_timestamp (argv[optind]);
  E = ext_read (argv[optind]);

  return 0;
}
