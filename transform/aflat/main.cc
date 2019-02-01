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
#include <string.h>
#include <act/passes/aflat.h>

void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-prsim|-lvs] <file.act>\n", s);
  exit (1);
}

typedef void (*export_pass)(Act *, output_formats);
			
int main (int argc, char **argv)
{
  Act *a;
  char *file;
  output_formats export_format;

  Act::Init (&argc, &argv);
  
  export_format = PRSIM_FMT;

  if (argc > 3) usage(argv[0]);
  if (argc == 3) {
    if (strcmp (argv[1], "-prsim") == 0) {
      export_format = PRSIM_FMT;
    }
    else if (strcmp (argv[1], "-lvs") == 0) {
      export_format = LVS_FMT;
    }
    else {
      usage (argv[0]);
    }
    file = argv[2];
  }
  else if (argc == 2) {
    file = argv[1];
  }
  else {
    usage (argv[0]);
  }

  a = new Act (file);
  a->Expand ();

  aflat_prs (a, export_format);

  return 0;
}
