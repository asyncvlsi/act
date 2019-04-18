/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2007, 2018-2019 Rajit Manohar
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
#include <string.h>
#include "v2act.h"
#include "misc.h"


Process *v2act_find_lib (Act *a, const char *nm)
{
  char buf[10240];

  sprintf (buf, "sync::%s", nm);
  return a->findProcess (buf);
}


static void _label_clock_helper (VWalk *v, module_t *m, const char *clk)
{
  module_t *n;
  int i;
  id_info_t *id;
  conn_info_t *c;
  hash_bucket_t *b;

  m->flags |= M_FLAG_CLKDONE;

  /* Label all clock ports */
  for (i=0; i < A_LEN (m->port_list); i++) {
    id = m->port_list[i];
    if (id->isinput && strcmp (clk, id->b->key) == 0) {
#if 0
      printf ("MODULE %s, clock port %s\n", m->b->key, clk);
#endif
      id->isclk = 1;
    }
  }

  /* Walk through connections, and if there is a clock as a label
     to a module then go in and label the module */
  for (i=0; i < A_LEN (m->conn); i++) {
    c = m->conn[i];
    if (c->prefix) {
      if (c->r) {
	if (c->r->id.id->isclk) {
	  /* clock found! */
	  c->isclk = 1;

	  Assert (c->prefix->nm, "Prefix without module name?");
	  b = hash_lookup (v->M, c->prefix->nm);
	  if (!b) {
	    /* library, ignore */
	    if (v2act_find_lib (v->a, c->prefix->nm) == NULL) {
	      fatal_error ("Module `%s' not found in library!", c->prefix->nm);
	    }
#if 0 
	    printf ("LIBMODULE %s, clock port %s\n", c->prefix->nm,
		    c->id.id->b->key);
#endif
	  }
	  else {
	    n = (module_t *)b->v;
	    if ((n->flags & M_FLAG_CLKDONE) == 0) {
	      _label_clock_helper (v, n, c->id.id->b->key);
	    }
	  }
	}
      }
    }
    else {
      /* ignore it */
    }
  }
}

/*------------------------------------------------------------------------
 *
 *  label_clocks --
 *
 *   Label all the clock nets
 *
 *------------------------------------------------------------------------
 */
void
label_clocks (VWalk *v, const char *clk)
{
  module_t *m;

  for (m = v->hd; m; m = m->next) {
    if (m->inst_exists == 0) {
      /* Top level module, process it */
      _label_clock_helper (v, m, clk);
    }
  }
}

/*------------------------------------------------------------------------
 *
 *  _increment_fanout --
 *
 *   Increment  fanout of a basic RHS element
 *
 *------------------------------------------------------------------------
 */
static void
_increment_fanout (conn_rhs_t *r, id_deref_t *lhs)
{
  id_info_t *info;
  int i;
  id_info_t *lhs_info;
  struct driver_info *d;

  info = r->id.id;
  
  if (A_LEN (info->a) > 0) {
    Assert (A_LEN (info->a) == 1, "No multi-dim arrays?!");
    if (!info->fa) {
      /* allocate fanout table */
      MALLOC (info->fa, int, info->a[0].hi - info->a[0].lo + 1);
      MALLOC (info->fcur, int, info->a[0].hi - info->a[0].lo + 1);
      for (i=0; i < info->a[0].hi - info->a[0].lo + 1; i++) {
	info->fa[i] = 0;
	info->fcur[i] = 0;
      }
    }
    /* array */
    if (r->id.isderef) {
      /* only increment one array element */
      i = r->id.deref - info->a[0].lo;
      info->fa[i]++;

      if (lhs) {
	NEW (d, struct driver_info);
	d->id = info->b->key;
	d->port = NULL;
	d->isderef = 1;
	d->deref = r->id.deref;
      }
      else {
	d = NULL;
      }

      if (d) {
	lhs_info = lhs->id;
	if (A_LEN (lhs_info->a) > 0) {
	  if (lhs_info->d == NULL) {
	    MALLOC (lhs_info->d, struct driver_info, lhs_info->a[0].hi - lhs_info->a[0].lo + 1);
	  }
	  Assert (lhs->isderef, "array without deref?");
	  lhs_info->d[lhs->deref - lhs_info->a[0].lo] = *d;
	  FREE (d);
	}
	else {
	  lhs_info->d = d;
	}
      }
    }
    else {
      Assert (lhs == NULL, "no way");
      if (r->issubrange) {
	for (i=r->lo; i <= r->hi; i++) {
	  info->fa[i - info->a[0].lo]++;
	}
      }
      else {
	for (i = info->a[0].lo; i <= info->a[0].hi; i++) {
	  info->fa[i - info->a[0].lo]++;
	}
      }
    }
  }
  else {
    if (lhs) {
      NEW (d, struct driver_info);
      d->id = info->b->key;
      d->port = NULL;
      d->isderef = 0;
      d->deref = 0;
    }
    else {
      d = NULL;
    }

    Assert (r->id.isderef == 0, "Deref for non-array?");

    info->fanout++;

    if (d) {
      lhs_info = lhs->id;
      if (A_LEN (lhs_info->a) > 0) {
	Assert (lhs->isderef == 1, "???");
	if (lhs_info->d == NULL) {
	  int j;
	  MALLOC (lhs_info->d, struct driver_info, 
		  lhs_info->a[0].hi - lhs_info->a[0].lo + 1);
	}
	lhs_info->d[lhs->deref - lhs_info->a[0].lo] = *d;
	FREE (d);
      }
      else {
	lhs_info->d = d;
      }
    }
  }
}

static void
_set_driver (conn_rhs_t *r, struct driver_info *d)
{
  id_info_t *info;
  int i;

  info = r->id.id;

  if (A_LEN (info->a) > 0) {
    Assert (A_LEN (info->a) == 1, "No multi-dim arrays?!");
    if (!info->d) {
      MALLOC (info->d, struct driver_info, info->a[0].hi - info->a[0].lo + 1);
      for (i=0; i < info->a[0].hi - info->a[0].lo + 1; i++) {
	info->d[i].id = NULL;
      }
    }
    /* array */
    if (r->id.isderef) {
      /* only increment one array element */
      i = r->id.deref - info->a[0].lo;
      info->d[i] = *d;
      FREE (d);
    }
    else {
      for (i=0; i < info->a[0].hi - info->a[0].lo; i++) {
	info->d[i] = *d;
	info->d[i].isderef = 1;
	info->d[i].deref = d->lo + i;
      }
    }
  }
  else {
    Assert (r->id.isderef == 0, "Deref for non-array?");
    Assert (info->d == NULL, "Multiple drivers?");
    info->d = d;
  }
}

static int
_set_driver_array (conn_rhs_t *r, struct driver_info *d, int offset)
{
  id_info_t *info;
  int i;

  info = r->id.id;

  if (A_LEN (info->a) > 0) {
    Assert (A_LEN (info->a) == 1, "No multi-dim arrays?!");
    if (!info->d) {
      MALLOC (info->d, struct driver_info, info->a[0].hi - info->a[0].lo + 1);
      for (i=0; i < info->a[0].hi - info->a[0].lo + 1; i++) {
	info->d[i].id = NULL;
      }
    }
    /* array */
    if (r->id.isderef) {
      /* only increment one array element */
      i = r->id.deref - info->a[0].lo;
      info->d[i] = *d;
      info->d[i].isderef = 1;
      info->d[i].deref = offset + d->lo;
    }
    else {
      for (i = 0; i < info->a[0].hi - info->a[0].lo; i++) {
	info->d[i] = *d;
	info->d[i].isderef = 1;
	info->d[i].deref = offset + d->lo;
	offset++;
      }
      offset--;
    }
  }
  else {
    Assert (r->id.isderef == 0, "Deref for non-array?");
    Assert (info->d == NULL, "Multiple drivers?");
    NEW (info->d, struct driver_info);
    *info->d = *d;
    info->d->isderef = 1;
    info->d->deref = offset + info->d->lo;
  }
  return offset + 1;
}

/*------------------------------------------------------------------------
 *
 *  compute_fanout --
 *
 *   Compute the fanout of each channel
 *
 *------------------------------------------------------------------------
 */
void compute_fanout (VWalk *v, module_t *m)
{
  int i, j;
  hash_bucket_t *b;
  id_info_t *info;
  Process *p;
  int dir = -1; /* 0 = input, 1 = output */
  struct driver_info *d = NULL;

  for (i=0; i < A_LEN (m->conn); i++) {
    dir = -1;
    d = NULL;
    /* check the LHS */
    if (m->conn[i]->prefix) {
      info = m->conn[i]->prefix;
      /* instance: look at the type */
      Assert (info->isinst, "What?");
      Assert (info->nm, "No module name?");
      if (p = v2act_find_lib (v->a, info->nm)) {
	/* port direction */
	j = p->FindPort (m->conn[i]->id.id->b->key);
	if (j == 0) {
	  fatal_error ("Port name `%s' not found in library process `%s'",
		       m->conn[i]->id.id->b->key, p->getName());
	}
	InstType *it;
	j--;
	it = p->getPortType (j);
	if (it->getDir() == Type::IN) {
	  /* input */
	  dir = 0;
	}
	else if (it->getDir() == Type::OUT) {
	  /* output */
	  dir = 1;
	  /* create driver */
	  NEW (d, struct driver_info);
	  d->id = info->b->key;
	  d->port = m->conn[i]->id.id->b->key;
	  d->isderef = m->conn[i]->id.isderef;
	  d->deref = m->conn[i]->id.deref;
	  if (it->arrayInfo()) {
	    Assert (0, "Libraries with array ports not supported");
	  }
	  d->lo = d->hi = -1;
	}
	else {
	  fatal_error ("Port `%s' for library process `%s' has no direction [%d]",
		       m->conn[i]->id.id->b->key, p->getName(),
		       it->getDir());
	}
      }
      else {
	module_t *n;
	/* lookup module in the module table */
	b = hash_lookup (v->M, info->nm);
	if (!b) {
	  fatal_error ("Module name `%s' missing?", info->nm);
	}
	n = (module_t *)b->v;
	for (j=0; j < A_LEN (n->port_list); j++) {
	  if (strcmp (n->port_list[j]->b->key, m->conn[i]->id.id->b->key) == 0) {
	    Assert (n->port_list[j]->isport, "??");
	    if (n->port_list[j]->isinput) {
	      dir = 0;
	      break;
	    }
	    else if (n->port_list[j]->isoutput) {
	      dir = 1;

	      NEW (d, struct driver_info);
	      d->id = info->b->key;
	      d->port = m->conn[i]->id.id->b->key;
	      d->isderef = m->conn[i]->id.isderef;
	      d->deref = m->conn[i]->id.deref;

	      if (A_LEN (n->port_list[j]->a) > 0) {
		d->lo = n->port_list[j]->a[0].lo;
		d->hi = n->port_list[j]->a[0].hi;
	      }
	      else {
		d->lo = d->hi = -1;
	      }
	      break;
	    }
	    else {
	      fatal_error ("Undirected port `%s' in module `%s'", 
			   n->port_list[j]->b->key,
			   b->key);
	    }
	  }
	}
	if (j == A_LEN (n->port_list)) {
	  fatal_error ("Unknown port name `%s' for module `%s'", 
		       m->conn[i]->id.id->b->key, b->key);
	}
      }
    }
    else {
      /* like an input */
       dir = 2;
    }
    m->conn[i]->dir = dir;

#if 0
    printf (" LHS: ");
    if (m->conn[i]->prefix) {
      printf ("%s.", m->conn[i]->prefix->b->key);
    }
    printf ("%s", m->conn[i]->id.id->b->key);
    if (m->conn[i]->id.isderef) {
      printf ("[%d]", m->conn[i]->id.deref);
    }
    printf (" dir=%d\n", dir);
#endif

    Assert (dir != -1, "What?");

    if (dir == 0 || dir == 2) {
      /* LHS is an input, so increment RHS fanout */
      if (m->conn[i]->r) {
	if (dir != 2) {
	  _increment_fanout (m->conn[i]->r, NULL);
	}
	else {
	  /* m->conn[i]->r is the DRIVER for the LHS */
	  Assert (m->conn[i]->prefix == NULL, "?");
	  _increment_fanout (m->conn[i]->r, &m->conn[i]->id);
	}
      }
      else {
	listitem_t *li;
	int j;
	if (dir == 2) {
	  Assert (m->conn[i]->prefix == NULL, "?");
	  info = m->conn[i]->id.id;
	  Assert (A_LEN (info->a) > 0, "Deref for non-array?");
	}
	j = 0;
	for (li = list_first (m->conn[i]->l); li; li = list_next (li)) {
	  if (dir != 2) {
	    _increment_fanout ((conn_rhs_t*)list_value (li), NULL);
	  }
	  else {
	    id_deref_t tmp;
	    tmp.id = info;
	    tmp.isderef = 1;
	    tmp.deref = j + info->a[0].lo;
	    _increment_fanout ((conn_rhs_t*)list_value (li), &tmp);
	  }
	  j++;
	}
      }
    }
    else {
      /* LHS is an output, so it DRIVES the RHS */
      Assert (d, "Driver not set");

      if (m->conn[i]->r) {
	/* this is simple! */
	_set_driver (m->conn[i]->r, d);
	/* LHS fanout has been incremented already */
      }
      else {
	int j = 0;
	listitem_t *li;
	for (li = list_first (m->conn[i]->l); li; li = list_next (li)) {
	  j = _set_driver_array ((conn_rhs_t *)list_value (li), d, j);
	  /* LHS fanout has been incremented already */
	}
	FREE (d);
      }
    }
  }

#if 0
  printf ("\n====\n");
  printf ("Module %s:\n", m->b->key);
  for (i=0; i < m->H->size; i++) {
    for (b = m->H->head[i]; b; b = b->next) {
      id_info_t *id = (id_info_t *)b->v;

      if (id->isinst || id->ismodname || id->nm) continue;

      printf ("%s : flags ", id->b->key);
      if (id->isinput) {
	printf (" ?");
      }
      if (id->isoutput) {
	printf (" !");
      }
      if (id->isport) {
	printf (" port");
      }
      if (id->isclk) {
	printf (" clk");
      }
#if 0
      if (id->isinst) {
	printf (" inst");
      }
      if (id->ismodname) {
	printf (" modname");
      }
      if (id->nm) {
	printf (" {complex}");
      }
#endif
      if (id->d) {
	if (id->isinput) {
	  printf (" *** driver set on INPUT ***");
	}
	else {
	  printf (" driver");
	}
      }
      else {
	if (id->isinput) {
	  ;
	}
	else if (id->fanout == 0) {
	  printf (" no-drive");
	}
	else if (strcmp (id->b->key, "Vdd") == 0) {
	  printf (" no-drive");
	}
	else if (strcmp (id->b->key, "GND") == 0) {
	  printf (" no-drive");
	}
	else {
	  printf (" ** no driver **");
	}
      }
      printf ("; fanout: ");
      if (id->fa) {
	int j;
	Assert (A_LEN (id->a) > 0, "???");
	printf ("(");
	for (j=0; j < id->a[0].hi - id->a[0].lo+1; j++) {
	  printf (" %d", id->fa[j]);
	}
	printf (" )");
      }
      else {
	printf ("%d", id->fanout);
      }
      printf ("\n");
    }
  }
#endif
}
