/*************************************************************************
 *
 *  Copyright (c) 2020 Rajit Manohar
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
#include <stdlib.h>
#include <string.h>
#include <agraph.h>
#include <config.h>
#include "vnet.h"
#include "v_parse.h"
#include "v_walk_X.h"

static
char *find_library (const char *s)
{
  char buf[10240];
  char *ret;
  FILE *tmp;

  ret = NULL;
  if (getenv ("CAD_HOME")) {
    sprintf (buf, "%s/lib/v2act/%s", getenv ("CAD_HOME"), s);
    tmp = fopen (buf, "r");
    if (tmp) {
      fclose (tmp);
      ret = buf;
    }
  }
  if (!ret && getenv ("ACT_HOME")) {
    sprintf (buf, "%s/lib/v2act/%s", getenv ("ACT_HOME"), s);
    tmp = fopen (buf, "r");
    if (tmp) {
      fclose (tmp);
      ret = buf;
    }
  }
  if (ret) {
    return Strdup (ret);
  }
  else {
    return NULL;
  }
}


VNet *verilog_read (const char *netlist, const char *actlib)
{
  char *s;
  v_Token *t;
  VNet *w;
  Act *a;
    
  s = find_library (actlib);
  if (!s) {
    a = new Act (actlib);
  }
  else {
    a = new Act (s);
    FREE (s);
  }

  t = v_parse (netlist);

  NEW (w, VNet);
  w->a = a;
  w->out = stdout;
  w->prefix = NULL;
  w->M = hash_new (32);
  w->hd = NULL;
  w->tl = NULL;
  w->missing = hash_new (8);

  v_walk_X (w, t);


  if (w->missing->n != 0) {
    fprintf (stderr, "ERROR: missing %d module%s.\n", w->missing->n,
	     w->missing->n == 1 ? "" : "s");
    for (int i=0; i < w->missing->size; i++) {
      for (hash_bucket_t *b = w->missing->head[i]; b; b = b->next) {
	fprintf (stderr, "  %s\n", b->key);
      }
    }
    exit (1);
  }
  
  return w;
}

struct library_vertex_info {
  const char *pin;		/* pin name */
  unsigned int isclk:3;		/* clock state:
				   0 = not a clock
				   1 = clock, rising edge (postflop)
				   2 = clock, falling edge (negflop)
				   3 = clock, transparent high (latch)
				   4 = clock, transparent low
				   (neglatch)
				*/
  
  unsigned int isinp:1;		/* is this an input? */
  unsigned int isset:1;		/* is the state set correctly? */
  
  int portid;			/* which port id it corresponds to */
  int port_offset;		/* port offset, if an array */
  int clk_port;			/* vertex number port for clock, if any */
};


static void dump_li_info (library_vertex_info *li)
{
  printf ("  pin: %s; clk: %d; inp: %d; clk_port: %d\n", li->pin,
	  li->isclk, li->isinp, li->clk_port);
}

static AGraph *_act_create_graph (VNet *v, Process *p)
{
  const char *instname;
  hash_bucket_t *b;
  AGraph *g;
  char buf[10240];
  
  /*-- single node, corresponding to the process --*/
  g = NULL;
  instname = p->getName();

  if (!p->isExpanded()) {
    if (p->getNumParams() != 0) {
      fatal_error ("Library cell `%s' requires paramters!", p->getName());
    }
    snprintf (buf, 10240, "%s<>", p->getName());
    if (!p->getns()->findType (buf)) {
      p->Expand (p->getns(), p->getns()->CurScope(), 0, NULL);
    }
    p = dynamic_cast<Process *>(p->getns()->findType (buf));
    Assert (p, "Expanded process found");
  }

  g = new AGraph (p);

#if 0
  printf ("Process: %s\n", instname);
#endif  

  for (int i=0; i < p->getNumPorts(); i++) {
    InstType *it;
    int count = 1;
    it = p->getPortType (i);
    if (!TypeFactory::isBoolType (it)) {
      fatal_error ("Library cell `%s', port `%s': not a bool!",
		   instname, p->getPortName (i));
    }
    if (it->arrayInfo()) {
      count = it->arrayInfo()->size();
    }

    for (int j=0; j < count; j++) {
      library_vertex_info *li;
      NEW (li, library_vertex_info);
      li->pin = p->getPortName(i);
      li->isclk = 0;
      li->clk_port = -1;
      li->isset = 1;		/* library module: state is set */

      if (it->getDir() == Type::IN) {
	li->isinp = 1;
      }
      else if (it->getDir() == Type::OUT) {
	li->isinp = 0;
      }
      else {
	fatal_error ("Library cell `%s', port `%s': no direction specified",
		     instname, p->getPortName (i));
      }
      /* config parameter to determine clocks */
      sprintf (buf, "s2a.%s.%s", instname, li->pin);
      if (config_exists (buf)) {

	if (li->isinp) {
	  int clk_state = config_get_int (buf);
	  if (clk_state > 4 || clk_state < 0) {
	    fatal_error ("Clock state `%s' is %d; should be one of 0, 1 (R), 2 (F), 3 (H), 4 (L)\n", buf, clk_state);
	  }
	  li->isclk = clk_state;
	}
	else {
	  /* specifies the clock pin id */
	  const char *nm = config_get_string (buf);
	  li->clk_port = p->FindPort (nm);
	  if (li->clk_port == 0) {
	    fatal_error ("Clock state `%s': no such port exists",
			 buf);
	  }
	  li->clk_port--;
	}
      }
      li->portid = i;
      if (it->arrayInfo()) {
	li->port_offset = j;
      }
      else {
	li->port_offset = -1;
      }
      if (li->isinp) {
	g->addInput (li);
      }
      else {
	g->addOutput (li);
      }
#if 0
      dump_li_info (li);
#endif      
    }
  }

  /* replace clk_port with the vertex # in the AGraph */
  AGraphVertexIter it(g);
  for (it = it.begin(); it != it.end(); it++) {
    AGvertex *v = *it;
    library_vertex_info *li = (library_vertex_info *)v->info;
    if (li->clk_port != -1) {
      const char *nm = p->getPortName (li->clk_port);
      AGraphVertexIter jt(g);
      for (jt = jt.begin(); jt != jt.end(); jt++) {
	library_vertex_info *mi = (library_vertex_info *) (*jt)->info;
	if (strcmp (mi->pin, nm) == 0) {
	  li->clk_port = jt.pos();
	  break;
	}
      }
      Assert (jt != jt.end(), "What?");
    }
  }
  
  b = hash_add (v->missing, instname);
  b->v = g;
  
  return g;
}

static
AGraph *_module_create_graph (VNet *v, module_t *m)
{
  hash_bucket_t *b;
  
  AGraph *g = new AGraph (m);

  /* clear ispace field */
  for (id_info_t *id = m->hd; id; id = id->next) {
    id->ispace = -1;
  }

  /* create input and output vertices */
  for (int i=0; i < A_LEN (m->port_list); i++) {
    int count = 1;

    if (A_LEN (m->port_list[i]->a) > 0) {
      count = m->port_list[i]->a[0].hi - m->port_list[i]->a[0].lo + 1;
    }

    Assert (m->port_list[i]->isinput == (1 - m->port_list[i]->isoutput), "What?");
    Assert (m->port_list[i]->isport, "What?");

    for (int j=0; j < count; j++) {
      library_vertex_info *li;
      NEW (li, library_vertex_info);
      li->pin = m->port_list[i]->myname;
      li->isinp = m->port_list[i]->isinput;
      li->portid = i;


      if (A_LEN (m->port_list[i]->a) > 0) {
	li->port_offset = j;
      }
      else {
	li->port_offset = -1;
      }
    
      li->isset = 0;		/* clk flags, etc. need to be set */
      li->clk_port = -1;
      li->isclk = 0;

      int v;
      if (li->isinp) {
	v = g->addInput (li);
      }
      else {
	v = g->addOutput (li);
      }
      if (j == 0) {
	m->port_list[i]->ispace = v;
	/* assumes a linear allocation of vertex ids */
      }
    }
  }
  
  /* -- now walk through the instances -- */
  for (int i=0; i < m->H->size; i++) {
    for (b = m->H->head[i]; b; b = b->next) {
      id_info_t *id = (id_info_t *)b->v;
      if (id->isport) continue;
      if (strcmp (id->myname, "Vdd") == 0 ||
	  strcmp (id->myname, "GND") == 0) {
	continue;
      }
      if (id->ismodname) {
	continue;
      }
      if (id->isinst) {
	/* create vertex */
	AGraph *subg;

	Process *p;
	Assert (id->nm, "Hmm...");
	if ((p = verilog_find_lib (v->a, id->nm))) {
	  /* this is a library module */
	  hash_bucket_t *tmp;
	  tmp = hash_lookup (v->missing, id->nm);
	  if (!tmp) {
	    _act_create_graph (v, p);
	    tmp = hash_lookup (v->missing, id->nm);
	  }
	  Assert (tmp, "Hmm");
	  subg = (AGraph *)tmp->v;
	}
	else {
	  module_t *n;
	  hash_bucket_t *tmp;
	  tmp = hash_lookup (v->M, id->nm);
	  if (!tmp) {
	    fatal_error ("Module name `%s': missing?", id->nm);
	  }
	  n = (module_t *)tmp->v;
	  if (!n->space) {
	    _module_create_graph (v, n);
	    Assert (n->space, "Hmm");
	  }
	  subg = (AGraph *)n->space;
	}
	/* subg = subgraph pointer */
	id->ispace = g->addVertex (subg);
      }
      else {
	/* signal wire; ignore */
      }
    }
  }

  /* now create the edges */
  for (int i=0; i < A_LEN (m->conn); i++) {
    if (!m->conn[i]->prefix) {
      /* lhs <- rhs : what do we do about this?! */
      if (m->conn[i]->r) {
	if (!m->conn[i]->r->issubrange &&
	    (m->conn[i]->r->id.isderef == 0) &&
	    (strcmp (m->conn[i]->r->id.id->myname, "Vdd") == 0 ||
	     strcmp (m->conn[i]->r->id.id->myname, "GND") == 0)) {
	  /* nothing to see here! */
	  printf ("skip %s <- supply\n", m->conn[i]->id.id->myname);
	}
	else {
	  warning ("Local net connection; not handled at present");
	  fprintf (stderr, "\tpartial info %s <- %s\n",
		   m->conn[i]->id.id->myname,
		   m->conn[i]->r->id.id->myname);
	}
      }
      else {
	  warning ("Local net connection; not handled at present");
	  fprintf (stderr, "\tpartial info %s <- { ... }\n",
		   m->conn[i]->id.id->myname);
      }
    }
    else {
      /* instance and port */
      
    }


  }
  

  
  m->space = g;

  return g;
}

/*
 *
 * Build netlist graph
 *
 */
AGraph *verilog_create_netgraph (VNet *v)
{
  AGraph *g;
  module_t *m;

  /* find top-level module */
  for (m = v->hd; m; m = m->next) {
    if (!m->inst_exists) {
      break;
    }
  }
  return _module_create_graph (v, m);
}
