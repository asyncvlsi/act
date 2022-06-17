#include <stdio.h>
#include <act/act.h>
#include <string.h>

int main (int argc, char **argv)
{
  Act *a;
  int exp, pr, glob;

  Act::Init (&argc, &argv);

  if (argc > 3 || argc < 2 ||
      (argc == 3 
       && (strcmp (argv[1], "-e") != 0)
       && (strcmp (argv[1], "-p") != 0)
       && (strcmp (argv[1], "-ep") != 0)
       && (strcmp (argv[1], "-epg") != 0))) {
    fatal_error ("Usage: %s [-epg] <file.act>\n", argv[0]);
  }
  if (argc == 3) {
    if (strcmp (argv[1], "-p") != 0) {
      exp = 1;
    }
    else {
      exp = 0;
    }
  }
  else {
    exp = 0;
  }
  glob = 0;
  if (argc == 3 && strcmp (argv[1], "-ep") == 0) {
    pr = 1;
  }
  else if (argc == 3 && strcmp (argv[1], "-epg") == 0) {
    pr = 1;
    glob = 1;
  }
  else {
    if (argc == 3 && strcmp (argv[1], "-p") == 0) {
      pr = 1;
    }
    else {
      pr = 0;
    }
  }

  a = new Act (argv[argc-1]);

  if (glob) {
     a->LocalizeGlobal ("Reset");
  }

  if (exp) {
    if (a) {
      a->Expand ();
    }
  }
  if (pr) {
    a->Print (stdout);
  }

  return 0;
}



