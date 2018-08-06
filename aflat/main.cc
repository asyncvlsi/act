/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include "aflat.h"

enum output_formats {
  PRSIM_FMT,
  LVS_FMT
};

void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-prsim|-lvs] <file.act>\n", s);
  exit (1);
}

int main (int argc, char **argv)
{
  Act *a;
  char *file;
  output_formats opt;

  Act::Init (&argc, &argv);

  if (argc > 3) usage(argv[0]);
  if (argc == 3) {
    if (strcmp (argv[1], "-prsim") == 0) {
      opt = PRSIM_FMT;
    }
    else if (strcmp (argv[1], "-lvs") == 0) {
      opt = LVS_FMT;
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

  aflat_prs (a);

  return 0;
}
