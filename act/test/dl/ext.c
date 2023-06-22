#include <stdio.h>

long test (int nargs, long *args)
{
  int i;
  printf ("got %d args\n", nargs);
  for (i=0; i < nargs; i++) {
    printf ("  arg[%d] = %ld\n", i, args[i]);
  }
  return args[0] + 4;
}
