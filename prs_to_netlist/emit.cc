/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <map>
#include "netlist.h"
#include <act/types.h>
#include <act/inst.h>
#include <act/iter.h>

static std::map<Process *, netlist_t *> *netmap = NULL;


static void flatten_ports_to_bools (netlist_t *n, UserDef *u)
{
  int i;

  for (i=0; i < u->getNumPorts(); i++) {
    InstType *it;
    const char *name;

    name = u->getPortName (i);
    it = u->getPortType (i);

    /* if it is a complex type, we need to traverse it! */
    if (TypeFactory::isUserType (it)) {
      
      
    }
    else if (TypeFactory::isBoolType (it)) {
      /* now check! */

    }
    else {
      fatal_error ("This cannot handle int/enumerated types; everything must be reducible to bool");
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
    
  /* emit sub-processes */
  ActInstiter i(p->CurScope());

  /* handle all processes instantiated by this one */
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType (vx->t)) {
      emit_netlist (a, dynamic_cast<Process *>(vx->t->BaseType()), fp);
    }
  }

  /*-- step 1: port list --*/
  flatten_ports_to_bools (n, p);
  
  
  
  return;
}

void act_emit_netlist (Act *a, Process *p, FILE *fp)
{
  Assert (p->isExpanded (), "Process must be expanded!");

  netmap = (std::map<Process *, netlist_t *> *) a->aux_find ("prs_to_netlist");
  if (!netmap) {
    fatal_error ("emit_netlist pass called before prs_to_netlist pass!");
  }

  /* clear visited flag */
  std::map<Process *, netlist_t *>::iterator it;
  for (it = netmap->begin(); it != netmap->end(); it++) {
    netlist_t *n = it->second;
    n->visited = 0;
  }
  emit_netlist (a, p, fp);
}


void emit_verilog_pins (Act *a, FILE *fpv, FILE *fpp, Process *p)
{
  
}
