#include <stdio.h>
#include <act/act.h>
#include <string.h>

int main (int argc, char **argv)
{
  Act *a;
  int exp;

  Act::Init (&argc, &argv);

  if (argc > 3 || (argc == 3 && (strcmp (argv[1], "-e") != 0))) {
    fatal_error ("Usage: %s [-e] <file.act>\n", argv[0]);
  }
  if (argc == 3) {
    exp = 1;
  }
  else {
    exp = 0;
  }

  a = new Act (argv[argc-1]);

  if (exp) {
    if (a) {
      a->Expand ();
    }
  }

  return 0;
}



