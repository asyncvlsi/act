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
#include "act/common/agraph.h"
#include "act/common/config.h"
#include "act/transform/v2act/vnet.h"
#include "act/transform/v2act/v_parse.h"
#include "act/transform/v2act/v_walk_X.h"

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
  w->port_count = 0;

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

class library_vertex_info : public AGinfo {
 public:
  const char *pin;		/* pin name */
  unsigned int isclk:3;		/* clock state:
				   0 = not a clock
				   1 = clock, rising edge (postflop)
				   2 = clock, falling edge (negflop)
				   3 = clock, transparent high (latch)
				   4 = clock, transparent low
				   (neglatch)
				   5 = a clock (no spec)
				*/
  
  unsigned int isinp:1;		/* is this an input? */
  unsigned int isset:1;		/* is the state set correctly? */
  
  int portid;			/* which port id it corresponds to */
  int port_offset;		/* port offset, if an array */
  int clk_port;			/* vertex number port for clock, if any */
};

/* XXX: assumption: an array port has all its elements in the same
   clock domain */
class  library_edge_info : public AGinfo {
 public:
  int srcpin;			/* -1 if an I/O pin; otherwise the
                                    vertex id in the instance */
  int dstpin;			/* -1 if an I/O pin; otherwise the
                                    vertex id in the instance */
  int genclk:1;		 	/* set to 1 if this edge corresponds
				   to a generated clock */
};

static library_edge_info *newedge ()
{
  library_edge_info *ei;
  NEW (ei, library_edge_info);
  ei->srcpin = -1;
  ei->dstpin = -1;
  ei->genclk = 0;
  return ei;
}
   

class inst_vertex_info : public AGinfo {
 public:
  id_info_t *id;
  AGraph *g;
};

static void dump_li_info (library_vertex_info *li)
{
  printf ("  pin: %s; clk: %d; inp: %d; clk_port: %d\n", li->pin,
	  li->isclk, li->isinp, li->clk_port);
}

AGvertex *find_pin (AGraph *g, const char *nm)
{
  AGraphInpVertexIter it(g);

  for (it = it.begin(); it != it.end(); it++) {
    AGvertex *v = (*it);
    library_vertex_info *li = (library_vertex_info *) v->info;
    if (strcmp (li->pin, nm) == 0) {
      return v;
    }
  }

  AGraphOutVertexIter jt(g);
  for (jt = jt.begin(); jt != jt.end(); jt++) {
    AGvertex *v = (*jt);
    library_vertex_info *li = (library_vertex_info *) v->info;
    if (strcmp (li->pin, nm) == 0) {
      return v;
    }
  }
  return NULL;
}

class AGprocinfo : public AGinfo {
 public:
  Process *p;
  AGprocinfo (Process *_p) { p = _p; }
};


class AGmoduleinfo : public AGinfo {
 public:
  module_t *m;
  AGmoduleinfo (module_t *_m) { m = _m; }
};


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

  g = new AGraph (new AGprocinfo (p));

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

#define UPDATE_LEN_SZ				\
  do {						\
    int tmp = strlen (buf+len);			\
    len += tmp;					\
    sz -= tmp;					\
  } while (0)


static void sprint_individual_id (char *buf, int sz, id_deref_t *id)
{
  int len = 0;
  buf[0] = '\0';
  snprintf (buf+len, sz, "%s", id->id->myname);
  UPDATE_LEN_SZ;
  if (id->isderef) {
    snprintf (buf+len, sz, "[%d]", id->deref - id->id->a[0].lo);
    UPDATE_LEN_SZ;
  }
}

static void sprint_individual_idx (char *buf, int sz, id_deref_t *id, int k)
{
  int len = 0;
  buf[0] = '\0';
  snprintf (buf+len, sz, "%s", id->id->myname);
  UPDATE_LEN_SZ;
  snprintf (buf+len, sz, "[%d]", k - id->id->a[0].lo);
  UPDATE_LEN_SZ;
}

struct netinfo {
  int idriver;			/* instance vertex for driver, -1 if none */
  AGvertex *drive_pin;		/* pin for driver */
  int offset;			/* for an array pin */

  list_t *targets;		/* list of (instv,pin) pairs */
};


static void record_connection_idx_both (struct Hashtable *H, char *name,
					AGraph *ag,
					int inst_v, AGvertex *pin,
					int idx1,
					id_info_t *id,
					int idx2)
{
  hash_bucket_t *b;
  struct netinfo *ni;

  b = hash_lookup (H, name);
  if (!b) {
    NEW (ni, struct netinfo);
    ni->idriver = -2;
    ni->targets = list_new ();

    b = hash_add (H, name);
    b->v = ni;
  }
  
  ni = (struct netinfo *) b->v;

  /* process <inst_v,pin,indx1> */
  library_vertex_info *li = (library_vertex_info *)pin->info;
  if (li->isinp) {
    list_iappend (ni->targets, inst_v);
    list_append (ni->targets, pin);
    list_iappend (ni->targets, idx1);
  }
  else {
    if (ni->idriver != -2) {
      warning ("Multiple driver for `%s'?", name);
    }
    else {
      ni->idriver = inst_v;
      ni->drive_pin = pin;
      ni->offset = idx1;
    }
  }

  /* process id, idx2 */
  if (id->isport) {
    AGvertex *v;
    v = find_pin (ag, id->myname);
    if (id->isoutput) {
      list_iappend (ni->targets, -1); /* local instance */
      list_append (ni->targets, v);
      list_iappend (ni->targets, idx2);
    }
    else {
      if (ni->idriver != -2) {
	if (ni->idriver == -1 && ni->drive_pin == v && ni->offset == idx2) {
	  /* ok! */
	}
	else {
	  warning ("Already found internal driver; input port `%s'; net: %s",
		   id->myname, name);
	}
      }
      else {
	ni->idriver = -1;
	ni->drive_pin = v;
	ni->offset = idx2;
      }
    }
  }
  else {
    /* nothing to do here; just a place holder */
  }
}


static void record_connection (struct Hashtable *H, char *name,
			       AGraph *ag,
			       int inst_v, AGvertex *pin,
			       id_info_t *id)
{
  record_connection_idx_both (H, name, ag, inst_v, pin, -1, id, -1);
}

static void record_connection_idx (struct Hashtable *H, char *name,
				   AGraph *ag,
				   int inst_v, AGvertex *pin,
				   id_info_t *id, int idx)
{
  record_connection_idx_both (H, name, ag, inst_v, pin, idx - id->a[0].lo,
			      id, idx - id->a[0].lo);
}


static void record_connection_idx2 (struct Hashtable *H, char *name,
				    AGraph *ag,
				    int inst_v, AGvertex *pin, int idx,
				    id_info_t *id)
{
  record_connection_idx_both (H, name, ag, inst_v, pin, idx, id, -1);
}
    
static
AGraph *_module_create_graph (VNet *v, module_t *m)
{
  struct Hashtable *H;
  char buf[10240];
  
  AGraph *g = new AGraph (new AGmoduleinfo(m));

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
    for (hash_bucket_t *b = m->H->head[i]; b; b = b->next) {
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
	inst_vertex_info *iv;
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
	NEW (iv, inst_vertex_info);
	iv->g = subg;
	iv->id = id;
	id->ispace = g->addVertex (iv);
      }
      else {
	/* signal wire; ignore */
      }
    }
  }

  /* now create the edges
     Step 1: find all drivers and associate them with the id for the net.
   */

  /* A net is a collection of input pins + a driver. 
     We need to elaborate arrays.
   */
  H = hash_new (16);
  
  for (int i=0; i < A_LEN (m->conn); i++) {
    conn_info_t *c = m->conn[i];

    if (!c->prefix) {
      /* lhs <- rhs : what do we do about this?! */
      if (c->r) {
	if (!c->r->issubrange && (c->r->id.isderef == 0) &&
	    (strcmp (c->r->id.id->myname, "Vdd") == 0 ||
	     strcmp (c->r->id.id->myname, "GND") == 0)) {
	  /* nothing to see here! */
	  //printf ("skip %s <- supply\n", c->id.id->myname);
	}
	else {
	  warning ("Local net connection; not handled at present");
	  fprintf (stderr, "\tpartial info %s <- %s\n",
		   c->id.id->myname, c->r->id.id->myname);
	}
      }
      else {
	  warning ("Local net connection; not handled at present");
	  fprintf (stderr, "\tpartial info %s <- { ... }\n", c->id.id->myname);
      }
    }
    else {
      /* instance and port */
      AGraph *ig;
      int inst_v;

      /* instance is from the prefix */
      /* port name is from id, and if there is a deref */
      if (c->prefix->isinst) {
	inst_vertex_info *iv;
	inst_v = c->prefix->ispace;
	iv = (inst_vertex_info *) (g->getVertex (inst_v))->info;
	ig = iv->g;
	Assert (ig, "What?");
      }
      else {
	fatal_error ("Prefix in connection `%s' is not an instance!",
		     c->prefix->myname);
	ig = NULL;
      }
      
      AGvertex *pin = find_pin (ig, c->id.id->myname);
      
      if (!pin) {
	warning ("Pin `%s' not found in inst `%s'",
		 c->id.id->myname, c->prefix->myname);
	continue;
      }
      /*
	one side of the connection:
	     <inst_v, pin> 
      */

      /* the other piece is the RHS... this is the local net name or
	 it is an I/O pin to this module
       */
      if (c->r) {
	/* simple connection */
	/* look in ig to see if this pin is an input or output */
	if (c->r->issubrange
	    || (!c->r->id.isderef && (A_LEN (c->r->id.id->a) > 0)) ||
	    c->r->id.isderef) {
	  /* subrange or array or array deref */
	  int lo, hi; // both lo, hi are inclusive
	  if (c->r->issubrange) {
	    lo = c->r->lo;
	    hi = c->r->hi;
	  }
	  else if (c->r->id.isderef) {
	    lo = c->r->id.deref - c->r->id.id->a[0].lo;
	    hi = lo;
	  }
	  else {
	    lo = c->r->id.id->a[0].lo;
	    hi = c->r->id.id->a[0].hi;
	  }
	  /* lo..hi */
	  for (int i=lo; i <= hi; i++) {
	    sprint_individual_idx (buf, 10240, &c->r->id, i);
	    if (c->r->id.isderef) {
	      record_connection_idx_both (H, buf, g, inst_v, pin, -1,
					  c->r->id.id, i);
	    }
	    else {
	      record_connection_idx (H, buf, g, inst_v, pin, c->r->id.id, i);
	    }
	  }
	}
	else {
	  /* simple connection */
	  sprint_individual_id (buf, 10240, &c->r->id);
	  record_connection (H, buf, g, inst_v, pin, c->r->id.id);
	}
      }
      else {
	/* list connection */
	listitem_t *li;
	int pos = 0;

	list_reverse (c->l);
	for (li = list_first (c->l); li; li = list_next (li)) {
	  conn_rhs_t *rhs = (conn_rhs_t *) list_value (li);
	  
	  if (rhs->issubrange ||
	      ((rhs->id.isderef == 0) && (A_LEN (rhs->id.id->a) > 0))) {
	    int lo, hi;
	    if (rhs->issubrange) {
	      lo = rhs->lo;
	      hi = rhs->hi;
	    }
	    else {
	      lo = rhs->id.id->a[0].lo;
	      hi = rhs->id.id->a[0].hi;
	    }
	    for (int i=lo; i <= hi; i++) {
	      sprint_individual_idx (buf, 10240, &rhs->id, i);
	      record_connection_idx (H, buf, g, inst_v, pin, rhs->id.id, i);
	      pos++;
	    }
	  }
	  else {
	    sprint_individual_id (buf, 10240, &rhs->id);
	    record_connection_idx2 (H, buf, g, inst_v, pin, pos,
				    rhs->id.id);
	    pos++;
	  }
	}
	list_reverse (c->l);
      }
    }
  }

  /* build the edges */
  for (int i=0; i < H->size; i++) {
    for (hash_bucket_t *b = H->head[i]; b; b = b->next) {
      struct netinfo *ni = (struct netinfo *)b->v;
      /* process net! */
#if 0      
      printf ("[NET %s (%d)]:\n", b->key, ni->idriver);
#endif      
      if (ni->idriver == -2) {
	if (Act::no_local_driver) {
	  if (!config_exists ("s2a.warnings.no_driver") ||
	      (config_get_int ("s2a.warnings.no_driver") == 1)) {
	    warning ("Missing driver, net `%s'", b->key);
	  }
	}
      }
      else {
	int srcvertex, srcpin;

	if (ni->idriver == -1) {
	  if (ni->drive_pin == NULL) {
	    /* must be Vdd or GND */
	    /* supply; const values, skip! */
	    srcvertex = -1;
	    srcpin = -1;
	  }
	  else {
	    inst_vertex_info *v;
	    library_vertex_info *li = (library_vertex_info *)ni->drive_pin->info;
	    srcvertex = -1;
	    srcpin = g->V2idx (ni->drive_pin);
	    if (ni->offset != -1) {
	      srcpin += ni->offset;
	    }
	  }
	}
	else {
	  inst_vertex_info *iv = (inst_vertex_info *)(g->getVertex (ni->idriver))->info;
	  Assert (iv->g, "Hmm");
	  srcvertex = ni->idriver; // no array instances
	  srcpin = iv->g->V2idx (ni->drive_pin);

	  if (ni->offset != -1) {
	    srcpin += ni->offset;
	  }
	}

	/* 
	   srcvertex = -1 if pin, inst_id otherwise
	   srcpin = pin loc 
	*/
#if 0
	{
	  if (srcvertex == -1) {
	    if (srcpin != -1) {
	      AGvertex *v = g->getVertex (srcpin);
	      library_vertex_info *vi = (library_vertex_info *) v->info;
	      printf ("%s [pin %d] ", vi->pin, v->isio);
	    }
	  }
	  else {
	    AGvertex *v = g->getVertex (srcvertex);
	    inst_vertex_info *vi = (inst_vertex_info *) v->info;
	    printf ("%s.{%d}", vi->id->myname, srcpin);
	    AGvertex *v2 = vi->g->getVertex (srcpin);
	    library_vertex_info *li = (library_vertex_info *) v2->info;
	    printf ("%s [pin %d] ", li->pin, v2->isio);
	  }
	}	
	printf (" ->{%d} \n", list_length (ni->targets)/3);
#endif	
	
	for (listitem_t *li = list_first (ni->targets); li;
	     li = list_next (li)) {
	  int inst_v;
	  AGvertex *pin;
	  int idx;
	  library_edge_info *ei;
	  int dst_v = -1;

	  inst_v = list_ivalue (li); li = list_next (li);
	  pin = (AGvertex *)list_value (li); li = list_next (li);
	  idx = list_ivalue (li);

	  ei = newedge();
	  if (srcvertex != -1) {
	    ei->srcpin = srcpin;
	  }
	  else {
	    ei->srcpin = -1;
	  }

	  if (inst_v == -1) {
	    /* local pin */
	    if (pin == NULL) {
	      /* skip */
	    }
	    else {
	      dst_v = g->V2idx (pin);
	      if (idx != -1) {
		dst_v += idx;
	      }
	      ei->dstpin = -1;
	    }
	  }
	  else {
	    Assert (pin->isio != 0, "What?");
	    inst_vertex_info *iv = (inst_vertex_info *)(g->getVertex (inst_v))->info;
	    ei->dstpin = iv->g->V2idx (pin);
	    if (idx != -1) {
	      ei->dstpin += idx;
	    }
	    dst_v = inst_v;
	    AGvertex *pin2 = iv->g->getVertex (ei->dstpin);
	    Assert (pin2->isio != 0, "What?");
	  }
	  if (srcvertex != -1) {
	    g->addEdge (srcvertex, dst_v, ei);
	  }
	  else {
	    if (srcpin != -1) {
	      g->addEdge (srcpin, dst_v, ei);
	    }
	  }
	}
      }
      list_free (ni->targets);
      FREE (ni);
    }
  }
  hash_free (H);
  m->space = g;

  return g;
}

module_t *verilog_top_module (VNet *v)
{
  module_t *m;

  /* find top-level module */
  for (m = v->hd; m; m = m->next) {
    if (!m->inst_exists) {
      break;
    }
  }
  if (m) {
    return m;
  }
  else {
    return NULL;
  }
}

/*
 *
 * Build netlist graph
 *
 */
AGraph *verilog_create_netgraph (VNet *v)
{
  AGraph *g;
  module_t *m = verilog_top_module (v);

  if (!m) {
    fatal_error ("Could not find top-level module!");
  }
  return _module_create_graph (v, m);
}


static void _mark_clock_nets (VNet *v, module_t *m)
{
  if (m->flags != 0) return;

  for (int i=0; i < m->H->size; i++) {
    for (hash_bucket_t *b = m->H->head[i]; b; b = b->next) {
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
	Process *p;
	Assert (id->nm, "Hmm...");
	if ((p = verilog_find_lib (v->a, id->nm))) {
	  /* nothing to do */
	}
	else {
	  module_t *n;
	  hash_bucket_t *tmp;
	  tmp = hash_lookup (v->M, id->nm);
	  if (!tmp) {
	    fatal_error ("Module name `%s': missing?", id->nm);
	  }
	  n = (module_t *)tmp->v;
	  _mark_clock_nets (v, n);
	}
      }
      else {
	/* signal wire; ignore */
      }
    }
  }

  /* now walk the graph */
  AGraph *ag = (AGraph *)m->space;
  int repeat_prop;
  do {
    repeat_prop = 0;
    AGraphVertexIter it(ag);
    for (it = it.begin(); it != it.end(); it++) {
      AGvertex *v = (*it);
      inst_vertex_info *inst = (inst_vertex_info *)v->info;
      if (v->isio != 0) continue;

#if 0
      printf ("inst: %s [%s]\n", inst->id->myname, inst->id->nm);
#endif    

      AGvertexBwdIter ei(ag,v->vid);

      for (ei = ei.begin(); ei != ei.end(); ei++) {
	AGedge *e = (*ei);
	library_edge_info *lei;
	lei = (library_edge_info *) e->info;

	Assert (lei->dstpin != -1, "What?");
	AGvertex *dstpin = inst->g->getVertex (lei->dstpin);
	Assert (dstpin->isio != 0, "Huh?");
	library_vertex_info *dstpin_inf = (library_vertex_info *) dstpin->info;

	if (dstpin_inf->isclk != 0 || lei->genclk) {
#if 0
	  printf ("  clk pin [%s] at dst\n", dstpin_inf->pin);
#endif	
	  /* propagate clock */
	
	  AGvertex *srcpin = ag->getVertex (e->src);
	  library_vertex_info *tmp;
	
	  if (lei->srcpin == -1) {
	    Assert (srcpin->isio != 0, "What?");
	    tmp = (library_vertex_info *)srcpin->info;
	    if (tmp->isclk == 0) {
	      tmp->isclk = 5;
	      repeat_prop = 1;
	    }
	  }
	  else {
	    inst_vertex_info *itmp;
	    Assert (srcpin->isio == 0, "What?");
	    itmp = (inst_vertex_info *)srcpin->info;
	    AGvertex *pin = itmp->g->getVertex (lei->srcpin);
	    Assert (pin->isio != 0, "What?");
	    tmp = (library_vertex_info *)pin->info;
	    if (!tmp->isset) {
	      /*-- module, pins can become clocks as needed --*/
	      if (tmp->isclk == 0) {
		tmp->isclk = 5;
		repeat_prop = 1;
		/* check if there is fanin */
		if (pin->hasFanin()) {
		  warning ("Module pin has fan-in?");
		}
	      }
	    }
	    else {
	      /*-- basic library cell: we're supposed to know what the
		clock nets here are apriori --*/
	      if (tmp->isclk == 0) {
		if (!config_exists ("s2a.warnings.gen_clk") ||
		    (config_get_int ("s2a.warnings.gen_clk") == 1)) {
		  warning ("Pin %s.%s is a generated clock",
			   inst->id->myname, dstpin_inf->pin);
		}
		/* -- generated clock: mark the *edge* -- */
		if (!lei->genclk) {
		  lei->genclk = 1;
		  repeat_prop = 1;
		  /* -- now mark all edges *backward* through this as a
		     generated clock pin -- */
	      
		  AGvertexBwdIter fi(ag, lei->srcpin);
		  for (fi = fi.begin(); fi != fi.end(); fi++) {
		    AGedge *f = (*fi);
		    library_edge_info *fei;
		    fei = (library_edge_info *) f->info;
		    if (!fei->genclk) {
		      fei->genclk = 1;
		    }
		  }
		}
	      }
	    }
	  }
	}
#if 0      
	else {
	  printf (" not clock pin [%s]\n", dstpin_inf->pin);
	}
#endif      
      }
    }
  } while (repeat_prop);
  
  m->flags = 1;

#if 0  
  printf ("Pin info for `%s'\n", m->b->key);
  for (int i=0; i < ag->numInputs(); i++) {
    AGvertex *iv = ag->getVertex (i);
    library_vertex_info *vi = (library_vertex_info *) iv->info;
    if (vi->isclk) {
      printf ("Clock pin: %s\n", vi->pin);
    }
  }
#endif
}
			   

/*
 * Mark all the clock nets
 */
void verilog_mark_clock_nets (VNet *v)
{
  module_t *m;
  list_t *worklist, *tmp;

  for (m = v->hd; m; m = m->next) {
    m->flags = 0;
  }

  /* reverse propagation of clock net from clock pins */
  m = verilog_top_module (v);
  _mark_clock_nets (v, m);

  for (m = v->hd; m; m = m->next) {
    m->flags = 0;
  }
}
