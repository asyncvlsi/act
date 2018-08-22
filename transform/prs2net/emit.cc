/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <map>
#include <string.h>
#include "netlist.h"
#include "config.h"
#include <act/iter.h>

static std::map<Process *, netlist_t *> *netmap = NULL;

static double lambda;			/* scale factor from width expressions
					to absolute units */

/* min transistor size */
static int min_w_in_lambda;
static int min_l_in_lambda;

/* max fet widths */
static int max_n_w_in_lambda;
static int max_p_w_in_lambda;

static int ignore_loadcap;  /* ignore loadcap directives (lvs only) */

/* discrete lengths */
static int discrete_length;

/* fet extra string */
static char *extra_fet_string;

/* fold transistors */
static int fold_nfet_width;
static int fold_pfet_width;

/* swap source and drain */
static int swap_source_drain;

/* use subckt models */
static int use_subckt_models;

/* emit area of source/drain along with fet */
static int emit_parasitics;

/* internal diffusion */
static int fet_spacing_diffonly;
static int fet_spacing_diffcontact;
static int fet_diff_overhang;

/* black box mode */
static int black_box_mode;

/* top level only */
static int top_level_only;

static void special_emit_procname (Act *a, FILE *fp, Process *p)
{
  const char *x = p->getName ();
  int len;

  len = strlen (x);
  if (len > 2 && x[len-1] == '>' && x[len-2] == '<') {
    for (int i=0; i < len-2; i++) {
      fputc (x[i], fp);
    }		   
  }
  else {
    a->mfprintf (fp, "%s", x);
  }
}

static var_t *raw_lookup (netlist_t *n, act_connection *c)
{
  ihash_bucket_t *b;

  b = ihash_lookup (n->cH, (long)c);
  if (b) {
    return (var_t *)b->v;
  }
  else {
    return NULL;
  }
}


static void mark_c_used (netlist_t *n, netlist_t *subinst, act_connection *c,
			 int *count)
{
  var_t *v = raw_lookup (n, c);

  A_NEW (n->instports, act_connection *);
  A_NEXT (n->instports) = c;
  A_INC (n->instports);
  
  if (v) {
    v->used = 1;
    if (subinst->ports[*count].input) {
      v->input = 1;
    }
    else {
      v->output = 1;
    }
  }
  else {
    ihash_bucket_t *b;
    b = ihash_lookup (n->uH, (long)c);
    if (!b) {
      b = ihash_add (n->uH, (long)c);
      b->v = NULL;
    }
    if (!subinst->ports[*count].input) {
      b->v = (void *)1;
    }
  }
}

static void append_bool_port (netlist_t *n, act_connection *c)
{
  int i;

  A_NEWM (n->ports, struct netlist_bool_port);
  A_NEXT (n->ports).c = c;
  A_NEXT (n->ports).omit = 0;
  A_NEXT (n->ports).input = 0;
  A_INC (n->ports);

  if (c->isglobal()) {
    /* globals do not need to be in the port list */
    A_LAST (n->ports).omit = 1;
    return;
  }

  if (black_box_mode && n->isempty) {
    /* assume this is needed! */
    return;
  }

  ihash_bucket_t *b;
  b = ihash_lookup (n->cH, (long)c);
  if (b) {
    var_t *v = (var_t *)b->v;
    if (!v->used) {
      b = ihash_lookup (n->uH, (long)c);
      if (!b) {
	A_LAST (n->ports).omit = 1;
	return;
      }
      else {
	if (b->v) {
	  A_LAST (n->ports).input = 0;
	}
	else {
	  A_LAST (n->ports).input = 1;
	}
      }
    }
    if (v->input && !v->output) {
      A_LAST (n->ports).input = 1;
    }
  }
  else {
    /* connection pointers that were not found were also not used! */
    b = ihash_lookup (n->uH, (long)c);
    if (!b) {
      A_LAST (n->ports).omit = 1;
      return;
    }
    else {
      if (b->v) {
	A_LAST (n->ports).input = 0;
      }
      else {
	A_LAST (n->ports).input = 1;
      }
    }
  }
  
  for (i=0; i < A_LEN (n->ports)-1; i++) {
    /* check to see if this is already in the port list;
       make this faster with a map if necessary since it is O(n^2) */
    if (c == n->ports[i].c) {
      A_LAST (n->ports).omit = 1;
      return;
    }
  }
}


static void flatten_ports_to_bools (netlist_t *n, ActId *prefix,
				    Scope *s, UserDef *u)
{
  int i;

  Assert (u, "Hmm...");
  
  for (i=0; i < u->getNumPorts(); i++) {
    InstType *it;
    const char *name;
    ActId *sub, *tail;
    act_connection *c;

    name = u->getPortName (i);
    it = u->getPortType (i);

    if (prefix) {
      sub = prefix->Clone ();

      tail = sub;
      while (tail->Rest()) {
	tail = tail->Rest();
      }
      tail->Append (new ActId (name));
      tail = tail->Rest();
    }
    else {
      sub = new ActId (name);
      tail = sub;
    }
      
    /* if it is a complex type, we need to traverse it! */
    if (TypeFactory::isUserType (it)) {
      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	while (!step->isend()) {
	  Array *t = step->toArray ();
	  tail->setArray (t);
	  flatten_ports_to_bools (n, sub, s,
				  dynamic_cast<UserDef *>(it->BaseType ()));
	  delete t;
	  tail->setArray (NULL);
	  step->step();
	}
      }
      else {
	flatten_ports_to_bools (n, sub, s,
				dynamic_cast<UserDef *>(it->BaseType ()));
      }
    }
    else if (TypeFactory::isBoolType (it)) {
      /* now check! */
      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	while (!step->isend()) {
	  Array *t = step->toArray ();
	  tail->setArray (t);
	  c = sub->Canonical (s);
	  Assert (c == c->primary (), "What?");
	  append_bool_port (n, c);
	  delete t;
	  tail->setArray (NULL);
	  step->step();
	}
	delete step;
      }
      else {
	c = sub->Canonical (s);
	Assert (c == c->primary(), "What?");
	append_bool_port (n, c);
      }
    }
    else {
      fatal_error ("This cannot handle int/enumerated types; everything must be reducible to bool");
    }
    delete sub;
  }
}


static void rec_update_used_flags (netlist_t *n,
				   netlist_t *subinst,
				   ActId *prefix,
				   Scope *s, UserDef *u, int *count)
{
  int i;

  Assert (u, "Hmm...");
  Assert (prefix, "Hmm");
  
  for (i=0; i < u->getNumPorts(); i++) {
    InstType *it;
    const char *name;
    ActId *sub, *tail;
    act_connection *c;

    name = u->getPortName (i);
    it = u->getPortType (i);

    sub = prefix->Clone ();

    tail = sub;
    while (tail->Rest()) {
      tail = tail->Rest();
    }
    tail->Append (new ActId (name));
    tail = tail->Rest();
      
    /* if it is a complex type, we need to traverse it! */
    if (TypeFactory::isUserType (it)) {
      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	while (!step->isend()) {
	  Array *t = step->toArray ();
	  tail->setArray (t);
	  rec_update_used_flags (n, subinst, sub, s,
				 dynamic_cast<UserDef *>(it->BaseType ()),
				 count);
	  delete t;
	  tail->setArray (NULL);
	  step->step();
	}
      }
      else {
	rec_update_used_flags (n, subinst, sub, s,
			       dynamic_cast<UserDef *>(it->BaseType ()), count);

      }
    }
    else if (TypeFactory::isBoolType (it)) {
      
      /* now check! */
      if (it->arrayInfo()) {
	Arraystep *step = it->arrayInfo()->stepper();
	while (!step->isend()) {
	  Assert (*count < A_LEN (subinst->ports), "What?");
	  if (!subinst->ports[*count].omit) {
	    Array *t = step->toArray ();
	    tail->setArray (t);
	    c = sub->Canonical (s);
	    Assert (c == c->primary (), "What?");

	    /* mark c as used */
	    mark_c_used (n, subinst, c, count);

	    delete t;
	    tail->setArray (NULL);
	  }
	  *count = *count + 1;
	  step->step();
	}
	delete step;
      }
      else {
	Assert (*count < A_LEN (subinst->ports), "Hmm");
	if (!subinst->ports[*count].omit) {
	  c = sub->Canonical (s);
	  Assert (c == c->primary(), "What?");

	  /* mark c as used */
	  mark_c_used (n, subinst, c, count);

	}
	*count = *count + 1;
      }
    }
    else {
      fatal_error ("This cannot handle int/enumerated types; everything must be reducible to bool");
    }
    delete sub;
  }
}

static void update_used_flags (netlist_t *n, ValueIdx *vx, Process *p)
{
  ActId *id;
  int count;
  netlist_t *subinst;
  /* vx is a process instance in process p.
     
     Update used flags in the current netlist n, based on connections
     to the ports of vx
   */
  id = new ActId (vx->getName());

  subinst = netmap->find (dynamic_cast <Process *>(vx->t->BaseType()))->second;
  
  if (vx->t->arrayInfo()) {
    /* ok, we need an outer loop now */
    Arraystep *as = vx->t->arrayInfo()->stepper();

    while (!as->isend()) {
      Array *t = as->toArray();
      id->setArray (t);
      count = 0;
      rec_update_used_flags (n, subinst,
			     id, p->CurScope(),
			     dynamic_cast<UserDef *>(vx->t->BaseType()), &count);
      delete t;
      id->setArray (NULL);
      as->step();
    }
    delete as;
  }
  else {
    count = 0;
    rec_update_used_flags (n, subinst,
			   id, p->CurScope (),
			   dynamic_cast<UserDef *>(vx->t->BaseType()), &count);
  }
  delete id;
}

static void create_bool_ports (Act *a, Process *p)
{
  Assert (p->isExpanded(), "Process must be expanded!");
  if (netmap->find(p) == netmap->end()) {
    fprintf (stderr, "Could not find process `%s'", p->getName());
    fprintf (stderr, " in created netlists; inconstency!\n");
    fatal_error ("Internal inconsistency or error in pass ordering!");
  }
  netlist_t *n = netmap->find (p)->second;
  if (n->visited) return;
  n->visited = 1;
    
  /* emit sub-processes */
  ActInstiter i(p->CurScope());

  /* handle all processes instantiated by this one */
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      create_bool_ports (a, dynamic_cast<Process *>(vx->t->BaseType()));
      update_used_flags (n, vx, p);
    }
  }
  flatten_ports_to_bools (n, NULL, p->CurScope(), p);
  return;
}


static void aemit_node (Act *a, netlist_t *N, FILE *fp, node_t *n)
{
  if (n->v) {
    char buf[10240];
    ActId *id = n->v->id->toid();
    id->sPrint (buf, 10240);
    a->mfprintf (fp, "%s", buf);
    delete id;
  }
  else {
    if (n == N->Vdd) {
      fprintf (fp, "Vdd");
    }
    else if (n == N->GND) {
      fprintf (fp, "GND");
    }
    else {
      if (n->inv) {
	fprintf (fp, "#fb%d#", n->i);
      }
      else {
	fprintf (fp, "#%d", n->i);
      }
    }
  }
}

static void emit_netlist (Act *a, Process *p, FILE *fp)
{
  Assert (p->isExpanded(), "Process must be expanded!");
  if (netmap->find(p) == netmap->end()) {
    fprintf (stderr, "Could not find process `%s'", p->getName());
    fprintf (stderr, " in created netlists; inconstency!\n");
    fatal_error ("Internal inconsistency or error in pass ordering!");
  }
  netlist_t *n = netmap->find (p)->second;
  if (n->visited) return;
  n->visited = 1;

  if (n->isempty && black_box_mode) return;

  ActInstiter i(p->CurScope());
  
  if (!top_level_only) {
    /* emit sub-processes */

    /* handle all processes instantiated by this one */
    for (i = i.begin(); i != i.end(); i++) {
      ValueIdx *vx = *i;
      if (TypeFactory::isProcessType (vx->t)) {
	emit_netlist (a, dynamic_cast<Process *>(vx->t->BaseType()), fp);
      }
    }
  }

  fprintf (fp, "*\n");
  fprintf (fp, "*---- act defproc: %s -----\n", p->getName());
  if (a->mangle_active()) {
    fprintf (fp, "* raw ports: ");
    for (int k=0; k < A_LEN (n->ports); k++) {
      if (n->ports[k].omit) continue;
      fprintf (fp, " ");
      ActId *id = n->ports[k].c->toid();
      id->Print (fp);
      delete id;
    }
    fprintf (fp, "\n*\n");
  }
  fprintf (fp, ".subckt ");
  /* special mangling for processes that only end in <> */
  special_emit_procname (a, fp, p);
  int out = 0;
  for (int k=0; k < A_LEN (n->ports); k++) {
    if (n->ports[k].omit) continue;
    ActId *id = n->ports[k].c->toid();
    char buf[10240];
    id->sPrint (buf, 10240);
    a->mfprintf (fp, " %s", buf);
    delete id;
    out = 1;
  }
  fprintf (fp, "\n");

  /* print pininfo */
  if (out) {
    fprintf (fp, "*.PININFO");
    for (int k=0; k < A_LEN (n->ports); k++) {
      if (n->ports[k].omit) continue;
      ActId *id = n->ports[k].c->toid();
      char buf[10240];
      id->sPrint (buf, 10240);
      a->mfprintf (fp, " %s", buf);
      fprintf (fp, ":%c", n->ports[k].input ? 'I' : 'O');
      delete id;
    }
    fprintf (fp, "\n");
  }

  listitem_t *vi;

  if (!list_isempty (n->vdd_list)) {
    fprintf (fp, "*.POWER VDD");
    for (vi = list_first (n->vdd_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      aemit_node (a, n, fp, x);
    }
    fprintf (fp, "\n");
  }

  if (!list_isempty (n->gnd_list)) {
    fprintf (fp, "*.POWER GND");
    for (vi = list_first (n->gnd_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      aemit_node (a, n, fp, x);
    }
    fprintf (fp, "\n");
  }

  if (!list_isempty (n->psc_list)) {
    /* this is the power supply for the subtrate of n-type
       transistors. It's a P-type substrate! 
    */
    fprintf (fp, "*.POWER NSUB");
    for (vi = list_first (n->psc_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      aemit_node (a, n, fp, x);
    }
    fprintf (fp, "\n");
  }

  if (!list_isempty (n->nsc_list)) {
    fprintf (fp, "*.POWER PSUB");
    for (vi = list_first (n->nsc_list); vi; vi = list_next (vi)) {
      node_t *x = (node_t *) list_value (vi);
      fprintf (fp, " ");
      aemit_node (a, n, fp, x);
    }
    fprintf (fp, "\n");
  }

  out = 0;
  node_t *x;
  for (x = n->hd; x; x = x->next) {
    if (!x->v) continue;
    if (!x->v->output) continue;
    ActId *id = x->v->id->toid();
    char buf[10240];
    id->sPrint (buf, 10240);
    delete id;
    if (!out) {
      fprintf (fp, "*\n* --- node flags ---\n*\n");
      out = 1;
    }
    a->mfprintf (fp, "* %s ", buf);
    if (x->v->stateholding) {
      fprintf (fp, "(state-holding): pup_reff=%g; pdn_reff=%g\n",
	       x->reff[EDGE_PFET], x->reff[EDGE_NFET]);
    }
    else {
      fprintf (fp, "(combinational)\n");
    }
  }
  if (out) {
    fprintf (fp, "*\n* --- end node flags ---\n*\n");
  }
 
  
  /*-- emit local netlist --*/
  int fets = 0;
  int ncaps = 0;
  char **fetnames = config_get_table_string ("act.fet_flavors");
  char devname[1024];
  int repnodes = 0;

  for (x = n->hd; x; x = x->next) {
    listitem_t *li;

    /* emit node cap, if any */
    if ((x->cap > 0) && !ignore_loadcap) {
      fprintf (fp, "C_per_node_%d ", ncaps++);
      aemit_node (a, n, fp, x);
      fprintf (fp, " ");
      aemit_node (a, n, fp, n->GND);
      fprintf (fp, " %g\n", x->cap*1e-15);
    }
    
    for (li = list_first (x->e); li; li = list_next (li)) {
      edge_t *e = (edge_t *)list_value (li);
      node_t *src, *drain;

      int len_repeat, width_repeat;
      int width_last;
      int il, iw;
      int w, l;
      
      if (e->visited || e->pruned) continue;
      e->visited = 1;

      w = e->w;
      l = e->l;

      /* discretize lengths */
      if (discrete_length > 0) {
	len_repeat = (discrete_length - 1 + e->l)/discrete_length;
	l = discrete_length;
      }
      else {
	len_repeat = 1;
      }

      /* fold widths */
      width_repeat = 1;
      if (e->type == EDGE_NFET) {
	if (fold_nfet_width > 0) {
	  width_repeat = e->w/fold_nfet_width;
	  width_last = e->w % fold_nfet_width;
	  if (width_last < min_w_in_lambda) {
	    width_last += fold_nfet_width;
	  }
	  else {
	    width_repeat++;
	  }
	}
      }
      else {
	Assert (e->type == EDGE_PFET, "Hmm");
	if (fold_pfet_width > 0) {
	  width_repeat = e->w/fold_pfet_width;
	  width_last = e->w % fold_pfet_width;
	  if (width_last < min_w_in_lambda) {
	    width_last += fold_pfet_width;
	  }
	  else {
	    width_repeat++;
	  }
	}
      }

      if (swap_source_drain) {
	src = e->b;
	drain = e->a;
      }
      else {
	src = e->a;
	drain = e->b;
      }
	
      for (il = 0; il < len_repeat; il++) {
	for (iw = 0; iw < width_repeat; iw++) {

	  if (width_repeat > 1) {
	    if (iw != width_repeat-1) {
	      if (e->type == EDGE_NFET) {
		w = fold_nfet_width;
	      }
	      else {
		w = fold_pfet_width;
	      }
	    }
	    else {
	      w = width_last;
	    }
	  }
	  
	  if (use_subckt_models) {
	    fprintf (fp, "x");
	  }

	  fprintf (fp, "M%d", fets);
	  if (len_repeat > 1) {
	    fprintf (fp, "_%d", il);
	  }
	  if (width_repeat > 1) {
	    fprintf (fp, "_%d", iw);
	  }

	  /* name of the instance includes how the fet was generated in it */
	  if (e->pchg) {
	    fprintf (fp, "_pchg ");
	  }
	  else if (e->combf) {
	    fprintf (fp, "_ckeeper ");
	  }
	  else if (e->keeper) {
	    fprintf (fp, "_keeper ");
	  }
	  else if (e->raw) {
	    fprintf (fp, "_pass ");
	  }
	  else {
	    fprintf (fp, "d_ ");
	  }

	  /* if length repeat, source/drain changes */
	  if (il == 0) {
	    aemit_node (a, n, fp, src);
	  }
	  else {
	    fprintf (fp, "#l%d", repnodes);
	  }
	  fprintf (fp, " ");
	  aemit_node (a, n, fp, e->g);
	  fprintf (fp, " ");

	  if (il == len_repeat-1) {
	    aemit_node (a, n, fp, drain);
	  }
	  else {
	    fprintf (fp, "#l%d", repnodes+1);
	  }
	  if (len_repeat > 1 && il != len_repeat-1) {
	    repnodes++;
	  }

	  fprintf (fp, " ");
	  aemit_node (a, n, fp, e->bulk);

	  sprintf (devname, "%cfet_%s", (e->type == EDGE_NFET ? 'n' : 'p'),
		   fetnames[e->flavor]);
	  fprintf (fp, " %s", config_get_string (devname));
	  if (e->subflavor != -1) {
	    fprintf (fp, "_%d", e->subflavor);
	  }
	  fprintf (fp, " W=%gU L=%gU", w*lambda*1e6, l*lambda*1e6);

	  /* print extra fet string */
	  if (extra_fet_string && strcmp (extra_fet_string, "") != 0) {
	    fprintf (fp, " %s\n", extra_fet_string);
	  }
	  else {
	    fprintf (fp, "\n");
	  }
      
	  /* area/perim for source/drain */
	  if (emit_parasitics) {
	    int gap;

	    if (il == 0) {
	      if (src->v) {
		gap = fet_diff_overhang;
	      }
	      else {
		if (list_length (src->e) > 2 || width_repeat > 1) {
		  gap = fet_spacing_diffcontact;
		}
		else {
		  gap = fet_spacing_diffonly;
		}
	      }
	    }
	    else {
	      gap = fet_spacing_diffonly;
	    }
	    fprintf (fp, "+ AS=%gP PS=%gU",
		     (e->w*gap)*(lambda*lambda)*1e12,
		     2*(e->w + gap)*lambda*1e6);

	    if (il == len_repeat-1) {
	      if (drain->v) {
		gap = fet_diff_overhang;
	      }
	      else {
		if (list_length (drain->e) > 2 || width_repeat > 1) {
		  gap = fet_spacing_diffcontact;
		}
		else {
		  gap = fet_spacing_diffonly;
		}
	      }
	    }
	    else {
	      gap = fet_spacing_diffonly;
	    }
	    fprintf (fp, " AD=%gP PD=%gU\n",
		     (e->w*gap)*(lambda*lambda)*1e12,
		     2*(e->w + gap)*lambda*1e6);
	  }

	  fflush (fp);

	  if (e->type == EDGE_NFET) {
	    if (max_n_w_in_lambda != 0 && e->w > max_n_w_in_lambda) {
	      fatal_error ("Device #%d: pfet width (%d) exceeds maximum limit (%d)\n", fets-1, e->w, max_n_w_in_lambda);
	    }
	  }
	  else {
	    if (max_p_w_in_lambda != 0 && e->w > max_p_w_in_lambda) {
	      fatal_error ("Device #%d: pfet width (%d) exceeds maximum limit (%d)\n", fets-1, e->w, max_p_w_in_lambda);
	    }
	  }
	}
      }
      fets++;
    }
  }

  /* clear visited flag */
  for (x = n->hd; x; x = x->next) {
    listitem_t *li;
    for (li = list_first (x->e); li; li = list_next (li)) {
      edge_t *e = (edge_t *)list_value (li);
      e->visited = 0;
    }
  }
  
  /*-- emit instances --*/
  int iport = 0;
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      netlist_t *sub;
      Process *instproc = dynamic_cast<Process *>(vx->t->BaseType ());
      int ports_exist;
      sub = netmap->find (instproc)->second;
      
      ports_exist = 0;
      for (int i=0; i < A_LEN (sub->ports); i++) {
	if (sub->ports[i].omit == 0) {
	  ports_exist = 1;
	  break;
	}
      }

      if (ports_exist) {
	if (vx->t->arrayInfo()) {
	  Arraystep *as = vx->t->arrayInfo()->stepper();
	  while (!as->isend()) {
	    char *str = as->string();
	    a->mfprintf (fp, "x%s%s", vx->getName(), str);
	    FREE (str);
	    for (int i=0; i < A_LEN (sub->ports); i++) {
	      ActId *id;
	      char buf[10240];
	      if (sub->ports[i].omit) continue;

	      Assert (iport < A_LEN (n->instports), "Hmm");
	      id = n->instports[iport]->toid();
	      fprintf (fp, " ");
	      id->sPrint (buf, 10240);
	      a->mfprintf (fp, "%s", buf);
	      delete id;
	      iport++;
	    }
	    a->mfprintf (fp, " ");
	    special_emit_procname (a, fp, instproc);
	    a->mfprintf (fp, "\n");
	    as->step();
	  }
	  delete as;
	}
	else {
	  a->mfprintf (fp, "x%s", vx->getName ());
	  for (int i =0; i < A_LEN (sub->ports); i++) {
	    ActId *id;
	    char buf[10240];
	    if (sub->ports[i].omit) continue;
	  
	    Assert (iport < A_LEN (n->instports), "Hmm");
	    id = n->instports[iport]->toid();
	    fprintf (fp, " ");
	    id->sPrint (buf, 10240);
	    a->mfprintf (fp, "%s", buf);
	    delete id;
	    iport++;
	  }
	  a->mfprintf (fp, " ");
	  special_emit_procname (a, fp, instproc);
	  a->mfprintf (fp, "\n");
	}
      }
    }
  }
  Assert (iport == A_LEN (n->instports), "Hmm...");
  
  fprintf (fp, ".ends\n");
  fprintf (fp, "*---- end of process: %s -----\n", p->getName());

  return;
}


void act_create_bool_ports (Act *a, Process *p)
{
  Assert (p->isExpanded (), "Process must be expanded!");

  netmap = (std::map<Process *, netlist_t *> *) a->aux_find ("prs2net");
  if (!netmap) {
    fatal_error ("emit_netlist pass called before prs2net pass!");
  }

  black_box_mode = config_get_int ("black_box_mode");
  
  /* clear visited flag */
  std::map<Process *, netlist_t *>::iterator it;
  for (it = netmap->begin(); it != netmap->end(); it++) {
    netlist_t *n = it->second;
    n->visited = 0;
  }
  create_bool_ports (a, p);
  for (it = netmap->begin(); it != netmap->end(); it++) {
    netlist_t *n = it->second;
    n->visited = 0;
  }
}


void act_emit_netlist (Act *a, Process *p, FILE *fp)
{
  Assert (p->isExpanded (), "Process must be expanded!");

  netmap = (std::map<Process *, netlist_t *> *) a->aux_find ("prs2net");
  if (!netmap) {
    fatal_error ("emit_netlist pass called before prs2net pass!");
  }

  lambda = config_get_real ("lambda");
  
  min_w_in_lambda = config_get_int ("min_width");
  min_l_in_lambda = config_get_int ("min_length");
  
  max_n_w_in_lambda = config_get_int ("max_n_width");
  max_p_w_in_lambda = config_get_int ("max_p_width");
  
  discrete_length = config_get_int ("discrete_length");
  fold_pfet_width = config_get_int ("fold_pfet_width");
  fold_nfet_width = config_get_int ("fold_nfet_width");

  ignore_loadcap = config_get_int ("ignore_loadcap");

  emit_parasitics = config_get_int ("emit_parasitics");
  fet_spacing_diffonly = config_get_int ("fet_spacing_diffonly");
  fet_spacing_diffcontact = config_get_int ("fet_spacing_diffcontact");
  fet_diff_overhang = config_get_int ("fet_diff_overhang");
  
  use_subckt_models = config_get_int ("use_subckt_models");
  swap_source_drain = config_get_int ("swap_source_drain");
  extra_fet_string = config_get_string ("extra_fet_string");

  black_box_mode = config_get_int ("black_box_mode");

  top_level_only = config_get_int ("top_level_only");
  
  emit_netlist (a, p, fp);
}


void emit_verilog_pins (Act *a, FILE *fpv, FILE *fpp, Process *p)
{
  
}
