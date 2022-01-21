/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
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
#include "act/common/atrace.h"
#include "act/common/misc.h"
#include "act/common/int.h"

static void usage (const char *name)
{
  fprintf (stderr, "Usage: %s <file> <signal-name>\n", name);
  exit (1);
}

int main (int argc, char **argv)
{
  atrace *a;
  name_t *n;
  int Nnodes, Nsteps, fmt, ts;
  
  
  if (argc != 3) {
    usage (argv[0]);
  }

  a = atrace_open (argv[1]);
  if (!a) {
    fatal_error ("Could not open file `%s' for reading", argv[1]);
  }

  n = atrace_lookup (a, argv[2]);

  if (!n) {
    atrace_close (a);
    fatal_error ("Could not find node `%s' in trace file `%s'", argv[2], argv[1]);
  }
  atrace_header (a, &ts, &Nnodes, &Nsteps, &fmt);
  atrace_init_time (a);
  for (int i=0; i < Nsteps; i++) {
    atrace_val_t v = ATRACE_GET_VAL (n);
    printf ("%g ", i*ATRACE_GET_STEPSIZE (a));
    if (atrace_is_analog (n)) {
      printf ("%g\n", ATRACE_FLOATVAL (&v));
    }
    else {
      int blk;
      if (atrace_is_channel (n)) {
	blk = atrace_is_channel_blocked (n, &v);
	if (blk != -1) {
	  printf ("%s-block\n", blk == ATRACE_CHAN_SEND_BLOCKED ? "send" : "recv");
	}
      }
      else {
	blk = -1;
      }
      if (blk == -1) {
	if (atrace_bitwidth (n) <= ATRACE_SHORT_WIDTH) {
	  printf ("%lu\n", ATRACE_SMALLVAL (&v));
	}
	else {
	  BigInt b;
	  b.setWidth (atrace_bitwidth (n));
	  for (int i=0; i < b.getLen(); i++) {
	    b.setVal (i, ATRACE_BIGVAL(&v)[i]);
	  }
	  b.decPrint (stdout);
	  printf ("\n");
	}
      }
    }
    atrace_advance_time (a, 1);
  }
  atrace_close (a);
  return 0;
}  
