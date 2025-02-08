#include <stdio.h>
#include <string.h>
#include <common/file.h>
#include <common/misc.h>
#include <act/extlang.h>

int is_a_newlang (LFILE *l)
{
  if (strcmp (file_tokenstring (l), "{") == 0) {
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
  int start, end;
  start = file_addtoken (l, "{");
  end = file_addtoken (l, "}");

  file_mustbe (l, start);
  
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

  file_mustbe (l, end);

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
