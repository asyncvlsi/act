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

void f (void *x, ActId *one, ActId *two)
{
  FILE *fp = (FILE *) x;
  fprintf (fp, "= ");
  one->Print (fp);
  fprintf (fp, " ");
  two->Print (fp);
  fprintf (fp, "\n");
}

static std::map<Process *, netlist_t *> *netmap = NULL;

static void _print_node (FILE *fp, ActId *prefix, node_t *n)
{
  if (n->v) {
    ActId *tmp = n->v->id->toid();
    if (!n->v->id->isglobal()) {
      prefix->Print (fp);
      fprintf (fp, ".");
    }
    tmp->Print (fp);
    delete tmp;
  }
  else {
    prefix->Print (fp);
    fprintf (fp, ".n#%d", n->i);
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
      _print_node (fp, prefix, e->g);
      fprintf (fp, " ");
      _print_node (fp, prefix, e->a);
      fprintf (fp, " ");
      _print_node (fp, prefix, e->b);
      fprintf (fp, " %d %d\n", e->w, e->l);
      e->visited = 1;
    }
  }

  printf ("process name: ");
  prefix->Print (stdout);
  printf ("; type: %s\n", p->getName());

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

  /* set default configurations */
  config_set_default_int ("std_p_width", 5);
  config_set_default_int ("std_p_length", 2);

  config_set_default_int ("std_n_width", 3);
  config_set_default_int ("std_n_length", 2);

  /* min w, l */
  config_set_default_int ("min_width", 2);
  config_set_default_int ("min_length", 2);

  /* max w */
  config_set_default_int ("max_n_width", 0);
  config_set_default_int ("max_p_width", 0);

  /* staticizer sizing */
  config_set_default_int ("stat_p_width", 5);
  config_set_default_int ("stat_p_length", 4);

  config_set_default_int ("stat_n_width", 3);
  config_set_default_int ("stat_n_length", 4);

  /* spacing */
  config_set_default_int ("fet_spacing_diffonly", 4);
  config_set_default_int ("fet_spacing_diffcontact", 8);
  config_set_default_int ("fet_diff_overhang", 6);

  /* strength ratios */
  config_set_default_real ("p_n_ratio", 2.0);
  config_set_default_real ("weak_to_strong_ratio", 0.1);

  config_set_default_real ("lambda", 0.1e-6);

  config_set_default_string ("mangle_chars", "");

  config_set_default_string ("act_cmdline", "");

  config_set_default_real ("default_load_cap", 0);

  config_set_default_string ("extra_fet_string", "");

  config_set_default_int ("disable_keepers", 0);

  config_set_default_int ("discrete_length", 0);

  config_set_default_int ("swap_source_drain", 0);

  config_set_default_int ("use_subckt_models", 0);

  config_set_default_int ("fold_pfet_width", 0);
  config_set_default_int ("fold_nfet_width", 0);

  /* generate netlist */
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
  fprintf (fps, "| units: 30 tech: scmos format: MIT\n");
  act_flat_apply_processes (a, fps, g);
  fclose (fps);
  
  act_flat_apply_conn_pairs (a, fpal, f);
  fclose (fpal);

  return 0;
}
