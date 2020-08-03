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
#include <act/act.h>

void usage (char *s)
{
  fprintf (stderr, "Usage: %s [act-options]  <file.act>\n", s);
  exit (1);
}

int main (int argc, char **argv)
{
  Act *a;
  char *file;

  Act::Init (&argc, &argv);
  Act::emit_depend = 1;
  
  if (argc != 2) usage(argv[0]);

  /* emit dependencies */
  printf ("%s: ", argv[1]);
  a = new Act (argv[1]);
  printf ("\n");

  return 0;
}
