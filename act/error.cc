/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2017-2019 Rajit Manohar
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
#include <act/types.h>
#include <common/misc.h>
#include <common/qops.h>

struct err_ctxt {
  const char *s;
  const char *file;
  int line;
  struct err_ctxt *next;
};

static struct err_ctxt *hd = NULL;

static int _curline = -1;

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
#if 0  
  int x = 0;
#endif  

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
  if (_curline != -1) {
    fprintf (fp, "Error on or near line number %d.\n", _curline);
  }
}

const char *act_error_top ()
{
  if (hd) {
    return hd->s;
  }
  else {
    return NULL;
  }
}

void act_error_setline (int line)
{
  _curline = line;
}
