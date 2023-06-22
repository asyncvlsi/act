#include <stdio.h>
#include <string.h>
#include <common/file.h>
#include <common/misc.h>
#include <act/extlang.h>


long test (int nargs, long *args)
{
  int i;
  printf ("got %d args\n", nargs);
  for (i=0; i < nargs; i++) {
    printf ("  arg[%d] = %ld\n", i, args[i]);
  }
  return args[0] + 4;
}

int is_a_newlang (LFILE *l)
{
  if (file_sym (l) == f_id &&
      strcmp (file_tokenstring (l), "hello") == 0) {
    return 1;
  }
  return 0;
}

struct testme {
  struct act_extern_language_header h;
  int val;
};

void *parse_a_newlang (LFILE *l)
{
  struct testme *x;
  
  if (file_sym (l) == f_id &&
      strcmp (file_tokenstring (l), "hello") == 0) {
    file_getsym (l);
  }
  else {
    printf ("Hmm: looking-at: %s\n", file_tokenstring (l));
  }
  NEW (x, struct testme);
  x->h.name = "newlang";
  x->val = 0xf43;

  return x;
}

void free_a_newlang (void *v)
{
  if (v) {
    FREE (v);
  }
}

struct ActTree;

void *walk_a_newlang (struct ActTree *t, void *v)
{
  struct testme *ret;
  NEW (ret, struct testme);
  *ret = *((struct testme *)v);
  return ret;
}

void print_a_newlang (FILE *fp, void *v)
{
  fprintf (fp, "hello");
}
