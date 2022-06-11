/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2019 Rajit Manohar
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
#include <unistd.h>
#include <string.h>
#include <act/act.h>
#include <act/passes/booleanize.h>
#include <act/passes/cells.h>
#include <act/iter.h>
#include <common/config.h>
#include <map>


static ActBooleanizePass *BOOL = NULL;

static int name_mangle = 0;
static int fuse_signal_directives = 0;
static const char *global_signal_prefix = NULL;

static void usage (char *name)
{
  fprintf (stderr, "Usage: %s [act-options] [-Bmf] [-c <cells>] [-p <proc>] <act>\n", name);
  fprintf (stderr, " -p <proc> : Emit process <proc>\n");
  exit (1);
}


/*
  Initialize globals from the configuration file.
  Returns process name
*/
static char *initialize_parameters (int *argc, char ***argv, char **cells)
{
  char *proc_name;
  int ch;
  int black_box = 0;

  name_mangle = 0;
  fuse_signal_directives = 0;
  proc_name = NULL;
  *cells = NULL;

  Act::Init (argc, argv);

  while ((ch = getopt (*argc, *argv, "mfBc:p:")) != -1) {
    switch (ch) {
    case 'f':
      fuse_signal_directives = 1;
      break;
      
    case 'm':
      name_mangle = 1;
      break;
    case 'B':
      black_box = 1;
      break;
    case 'p':
      if (proc_name) {
	FREE (proc_name);
      }
      proc_name = Strdup (optarg);
      break;
    case 'c':
      if (*cells) {
        FREE (*cells);
      }
      *cells = Strdup (optarg);
      break;
    case '?':
      fprintf (stderr, "Unknown option.\n");
      usage ((*argv)[0]);
      break;
    default:
      fatal_error ("shouldn't be here");
      break;
    }
  }

  /* optind points to what is left */
  /* expect 1 argument left */
  if (optind != *argc - 1) {
    fprintf (stderr, "Missing act file name.\n");
    usage ((*argv)[0]);
  }

  if (!proc_name) {
    fprintf (stderr, "Missing process name.\n");
    usage ((*argv)[0]);
  }

  *argc = 2;
  (*argv)[1] = (*argv)[optind];
  (*argv)[2] = NULL;

  if (black_box) {
    config_set_int ("net.black_box_mode", 1);
  }

  return proc_name;
}

static void emit_verilog_id (act_connection *c)
{
  ActId *id;

  id = c->toid();

  if (c->isglobal()) {
    /* XXX: this only works in simulation land */
    printf ("%s", global_signal_prefix);
  }

  if (name_mangle) {
    char buf[10240];
    id->sPrint (buf, 10240);
    ActNamespace::Act()->mfprintf (stdout, "%s", buf);
  }
  else {
    if (id->arrayInfo() || id->Rest()) {
      printf ("\\");
    }
    id->Print (stdout);
    if (id->arrayInfo() || id->Rest()) {
      printf (" ");
    }
  }
  delete id;
}


static void emit_verilog_prs (act_prs_lang_t *p)
{
  /* print one production rule */
}


static void emit_verilog_moduletype (Act *a, Process *p)
{
  char buf[10240];
  InstType it(NULL, p, 1);
  it.sPrint (buf, 10240, 1);
  a->mfprintf (stdout, "%s", buf);
}

void emit_verilog (Act *a, Process *p)
{
  Assert (p->isExpanded(), "What?");

  act_boolean_netlist_t *n = BOOL->getBNL (p);
  if (!n) {
    Assert (0, "emit_verilog internal inconsistency");
  }

  if (n->visited) return;
  n->visited = 1;

  if (p->isBlackBox() || p->isLowLevelBlackBox()) {
    if (n->macro && n->macro->isValid()) {
      const char *nm = n->macro->getVerilogFile();
      if (nm) {
	FILE *xfp = fopen (nm, "r");
	if (xfp) {
	  int sz;
	  char buf[1024];
	  while ((sz = fread (buf, 1, 1024, xfp)) > 0) {
	    fwrite (buf, 1, sz, stdout);
	  }
	  fclose (xfp);
	}
	return;
      }
    }
  }
  

  ActUniqProcInstiter inst(p->CurScope());
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;
    emit_verilog (a, dynamic_cast<Process *>(vx->t->BaseType()));
  }

  /* now emit this module */
  printf ("//\n");
  printf ("// Verilog module for: %s\n", p->getName());
  printf ("//\n");
  printf ("module ");
  emit_verilog_moduletype (a, p);
  printf ("(");

  int first = 1;
  for (int i=0; i < A_LEN (n->ports); i++) {
    if (n->ports[i].omit) continue;
    if (!first) {
      printf (", ");
    }
    first = 0;
    emit_verilog_id (n->ports[i].c);
  }
  printf (");\n");

  if (!fuse_signal_directives) {
    for (int i=0; i < A_LEN (n->ports); i++) {
      if (n->ports[i].omit) continue;
      if (n->ports[i].input) {
	printf ("   input ");
      }
      else {
	printf ("   output ");
      }
      emit_verilog_id (n->ports[i].c);
      printf (";\n");
    }
  }

  printf ("\n// -- signals ---\n");
  int i;
  ihash_bucket_t *b;
  for (i=0; i < n->cH->size; i++) {
    for (b = n->cH->head[i]; b; b = b->next) {
      act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;
      if (!v->used) continue;
      if (v->id->isglobal()) continue;

      if (fuse_signal_directives) {
	for (int k=0; k < A_LEN (n->ports); k++) {
	  if (n->ports[k].omit) continue;
	  if (n->ports[k].c == v->id) {
	    if (n->ports[k].input) {
	      printf ("   input ");
	    }
	    else {
	      printf ("   output ");
	    }
	    break;
	  }
	}
      }

      if (v->input && !v->output) {
	printf ("   wire ");
      }
      else {
	printf ("   reg ");
      }
      emit_verilog_id (v->id);
      printf (";\n");
    }
  }

  int iport = 0;

  /* now we print instances */
  printf ("\n// --- instances\n");
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;
    act_boolean_netlist_t *sub;
    Process *instproc = dynamic_cast<Process *>(vx->t->BaseType());
    int ports_exist = 0;

    sub = BOOL->getBNL (instproc);
    for (i=0; i < A_LEN (sub->ports); i++) {
      if (!sub->ports[i].omit) {
	ports_exist = 1;
	break;
      }
    }
    /* if there are no ports, we can skip the instance */
    if (ports_exist || instproc->isBlackBox()) {
      Arraystep *as;
      if (vx->t->arrayInfo()) {
	as = vx->t->arrayInfo()->stepper();
      }
      else {
	as = NULL;
      }

      do {
	if (!as || (!as->isend() && vx->isPrimary (as->index()))) {
	  emit_verilog_moduletype (a, instproc);
          if (name_mangle) {
	    ActNamespace::Act()->mfprintf (stdout, " %s", vx->getName());
	    if (as) {
	      char *s = as->string();
	      ActNamespace::Act()->mfprintf (stdout, "%s", s);
	      FREE (s);
	    }
          }
          else {
	    printf (" \\%s", vx->getName());
	    if (as) {
	      as->Print (stdout);
	    }
          }
	  printf ("  (");

	  int first = 1;
	  for (i=0; i < A_LEN (sub->ports); i++) {
	    if (sub->ports[i].omit) continue;
	    if (!first) {
	      printf (", ");
	    }
	    first = 0;
	    printf (".");
	    emit_verilog_id (sub->ports[i].c);
	    printf ("(");
	    emit_verilog_id (n->instports[iport]);
	    printf (")");
	    iport++;
	  }
	  printf (");\n");
	}
	if (as) {
	  as->step();
	}
      } while (as && !as->isend());
      if (as) {
	delete as;
      }
    }
  }
  
#if 0
  /*-- now we print out the production rules! --*/
  act_prs *prs = p->getprs();
  if (prs) {
    printf ("\n// -- circuit production rules\n");
    while (prs) {
      act_prs_lang_t *p;
      for (p = prs->p; p; p = p->next) {
	emit_verilog_prs (p);
      }
      prs = prs->next;
    }
  }
#endif  

  printf ("endmodule\n\n");
  return;
}

int main (int argc, char **argv)
{
  Act *a;
  char *proc;
  char *cells;

  proc = initialize_parameters (&argc, &argv, &cells);
  config_set_default_string ("act.global_signal_prefix", "top.");

  if (argc != 2) {
    fatal_error ("Something strange happened!");
  }
  if (proc == NULL) {
    fatal_error ("Missing process name!");
  }
  
  a = new Act (argv[1]);
  if (cells) {
    a->Merge (cells);
  }
  a->Expand ();

  if (cells) {
    ActCellPass *cp = new ActCellPass (a);
    cp->run ();
  }

  Process *p = a->findProcess (proc);

  if (!p) {
    fatal_error ("Could not find process `%s' in file `%s'", proc, argv[1]);
  }

  if (!p->isExpanded()) {
    p = p->Expand (ActNamespace::Global(), p->CurScope(), 0, NULL);
  }

  BOOL = new ActBooleanizePass (a);
  Assert (BOOL->run (p), "booleanize pass failed?");

  global_signal_prefix = config_get_string ("act.global_signal_prefix");
  
  emit_verilog (a, p);
  return 0;
}
