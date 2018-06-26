/*************************************************************************
 *
 *  Copyright (c) 2017-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <act/types.h>
#include "misc.h"
#include "qops.h"

struct err_ctxt {
  const char *s;
  const char *file;
  int line;
  struct err_ctxt *next;
};

static struct err_ctxt *hd = NULL;

void act_error_push (const char *s, const char *file, int line)
{
  struct err_ctxt *ec;

  NEW (ec, struct err_ctxt);
  ec->s = s;
  ec->file = file;
  ec->line = line;
  ec->next = NULL;

  l_ins (hd,ec);
}

void act_error_update (const char *file, int line)
{
  if (hd) {
    if (file) {
      hd->file = file;
    }
    hd->line = line;
  }
  else {
    fatal_error ("act_error_update(): no error context");
  }
}

void act_error_pop ()
{
  struct err_ctxt *ec;
  l_del (hd, ec);
  FREE (ec);
}

void act_error_ctxt (FILE *fp)
{
  struct err_ctxt *ec;
  int x = 0;

  for (ec = hd; ec; ec = ec->next) {
#if 0
    for (int i=0; i < x; i++) {
      fprintf (fp, " ");
    }
    x++;
#endif    
    fprintf (fp, "In expanding %s", ec->s);
    if (ec->file) {
      fprintf (fp, " (%s:%d)", ec->file, ec->line);
    }
    fprintf (fp, "\n");
  }
}
