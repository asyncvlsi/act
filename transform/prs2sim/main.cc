/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include <act/passes.h>
#include <act/netlist.h>
#include <map>
#include "config.h"

void usage (char *s)
{
  fprintf (stderr, "Usage: %s <file.act> <simfile>\n", s);
  exit (1);
}

void idprint (FILE *fp, ActId *id)
{
  char buf[10240];
  int i;
  id->sPrint (buf, 10240);
  for (i=0; buf[i]; i++) {
    if (buf[i] == '.') {
      buf[i] = '/';
    }
  }
  fprintf (fp, "%s", buf);
}


void f (void *x, ActId *one, ActId *two)
{
  FILE *fp = (FILE *) x;
  fprintf (fp, "= ");
  idprint (fp, one);
  //one->Print (fp);
  fprintf (fp, " ");
  idprint (fp, two);
  //two->Print (fp);
  fprintf (fp, "\n");
}

static std::map<Process *, netlist_t *> *netmap = NULL;

static void _print_node (netlist_t *N, FILE *fp, ActId *prefix, node_t *n)
{
  if (n->v) {
    ActId *tmp = n->v->id->toid();
    if (!n->v->id->isglobal()) {
      idprint (fp, prefix);
      //prefix->Print (fp);
      //fprintf (fp, ".");
      fprintf (fp, "/");
    }
    //tmp->Print (fp);
    idprint (fp, tmp);
    delete tmp;
  }
  else {
    if (n == N->Vdd) {
      fprintf (fp, "Vdd");
    }
    else if (n == N->GND) {
      fprintf (fp, "GND");
    }
    else {
      if (prefix) {
	//prefix->Print (fp);
	//fprintf (fp, ".");
	idprint (fp, prefix);
	fprintf (fp, "/");
      }
      fprintf (fp, "n#%d", n->i);
    }
  }
}

void g (void *x, ActId *prefix, Process *p)
{
  FILE *fp;
  netlist_t *N;
  N = netmap->find (p)->second;
  Assert (N, "Hmm");

  fp = (FILE *)x;

  /* now print out the netlist, with all names having the prefix
     specified */
  node_t *n;
  edge_t *e;
  listitem_t *li;
  for (n = N->hd; n; n = n->next) {
    for (li = list_first (n->e); li; li = list_next (li)) {
      e = (edge_t *) list_value (li);
      if (e->visited) continue;
      /* p <gate> <src> <drain> l w */
      if (e->type == EDGE_NFET) {
	fprintf (fp, "n ");
      }
      else {
	fprintf (fp, "p ");
      }
      _print_node (N, fp, prefix, e->g);
      fprintf (fp, " ");
      _print_node (N, fp, prefix, e->a);
      fprintf (fp, " ");
      _print_node (N, fp, prefix, e->b);
      fprintf (fp, " %d %d\n", e->w, e->l);
      e->visited = 1;
    }
  }

  for (n = N->hd; n; n = n->next) {
    for (li = list_first (n->e); li; li = list_next (li)) {
      e = (edge_t *) list_value (li);
      e->visited = 0;
    }
  }
  
}

int main (int argc, char **argv)
{
  Act *a;
  char *file;

  Act::Init (&argc, &argv);

  if (argc != 3) {
    usage (argv[0]);
  }
  
  a = new Act (argv[1]);
  act_expand (a);
  config_read ("prs2net.conf");

  /* generate netlist */
  config_set_int ("net.disable_keepers", 1);
  act_prs_to_netlist (a, NULL);
  netmap = (std::map<Process *, netlist_t *> *) a->aux_find ("prs2net");
  Assert (netmap, "Hmm...");

  FILE *fps, *fpal;
  char buf[10240];

  sprintf (buf, "%s.sim", argv[2]);
  fps = fopen (buf, "w");
  if (!fps) {
    fatal_error ("Could not open file `%s' for writing", buf);
  }
  sprintf (buf, "%s.al", argv[2]);
  fpal = fopen (buf, "w");
  if (!fpal) {
    fatal_error ("Could not open file `%s' for writing", buf);
  }


  /* print as sim file 
     units: lambda in centimicrons
   */
  int units = (1.0e8*config_get_real ("net.lambda")+0.5);
  if (units <= 0) {
    warning ("Technology lambda is less than 1 centimicron; setting to 1");
    units = 1;
  }

  fprintf (fps, "| units: %d tech: %s format: MIT\n", units, config_get_string ("net.name"));

  act_flat_apply_processes (a, fps, g);
  g(fps, NULL, NULL);
  fclose (fps);
  
  act_flat_apply_conn_pairs (a, fpal, f);
  fprintf (fpal, "= Vdd Vdd!\n");
  fprintf (fpal, "= GND GND!\n");
  fclose (fpal);

  return 0;
}
