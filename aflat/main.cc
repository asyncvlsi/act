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

output_formats export_format;

void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-prsim|-lvs] <file.act>\n", s);
  exit (1);
}

int main (int argc, char **argv)
{
  Act *a;
  char *file;

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

  aflat_prs (a);

  return 0;
}
