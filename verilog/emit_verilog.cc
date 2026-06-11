/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
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
#include <act/act.h>
#include <act/passes/booleanize.h>
#include <act/iter.h>

static ActBooleanizePass *BOOL = NULL;
static int name_mangle = 0;
static int fuse_signal_directives = 0;
static int no_signal_decl = 0;
static const char *global_signal_prefix = NULL;
static int verilog_emit_cells;
static struct pHashtable *STOP_AT = NULL;

static void emit_verilog_id (FILE *fp, act_connection *c)
{
  ActId *id;

  id = c->toid();

  if (c->isglobal()) {
    /* XXX: this only works in simulation land */
    fprintf (fp, "%s", global_signal_prefix);
  }

  if (name_mangle) {
    char buf[10240];
    id->sPrint (buf, 10240);
    ActNamespace::Act()->mfprintf (fp, "%s", buf);
  }
  else {
    if (id->arrayInfo() || id->Rest()) {
      fprintf (fp, "\\");
    }
    id->Print (fp);
    if (id->arrayInfo() || id->Rest()) {
      fprintf (fp, " ");
    }
  }
  delete id;
}


static void emit_verilog_prs (FILE *fp, act_prs_lang_t *p)
{
  /* print one production rule */
}

static void emit_verilog_moduletype (FILE *fp, Act *a, Process *p)
{
  char buf[10240];
  InstType it(NULL, p, 1);
  it.sPrint (buf, 10240, 1);
  a->mfprintf (fp, "%s", buf);
}

static void emit_verilog (FILE *fp, Act *a, Process *p)
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
	    fwrite (buf, 1, sz, fp);
	  }
	  fclose (xfp);
	}
	return;
      }
    }
  }

  if (p->isCell() && verilog_emit_cells == 0) {
    return;
  }

  if (STOP_AT && phash_lookup (STOP_AT, p)) {
    return;
  }

  ActUniqProcInstiter inst(p->CurScope());
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;
    emit_verilog (fp, a, dynamic_cast<Process *>(vx->t->BaseType()));
  }

  /* now emit this module */
  fprintf (fp, "//\n");
  fprintf (fp, "// Verilog module for: %s\n", p->getName());
  fprintf (fp, "//\n");
  fprintf (fp, "module ");
  emit_verilog_moduletype (fp, a, p);
  fprintf (fp, "(");

  int first = 1;
  for (int i=0; i < A_LEN (n->ports); i++) {
    if (n->ports[i].omit) continue;
    if (!first) {
      fprintf (fp, ", ");
    }
    first = 0;
    emit_verilog_id (fp, n->ports[i].c);
  }
  fprintf (fp, ");\n");

  if (!fuse_signal_directives) {
    for (int i=0; i < A_LEN (n->ports); i++) {
      if (n->ports[i].omit) continue;
      if (n->ports[i].input) {
	fprintf (fp, "   input ");
      }
      else {
	fprintf (fp, "   output ");
      }
      emit_verilog_id (fp, n->ports[i].c);
      fprintf (fp, ";\n");
    }
  }

  fprintf (fp, "\n// -- signals ---\n");
  int i;
  ihash_bucket_t *b;
  for (i=0; i < n->cH->size; i++) {
    for (b = n->cH->head[i]; b; b = b->next) {
      act_booleanized_var_t *v = (act_booleanized_var_t *)b->v;
      int io_sig = 0;
      if (!v->used) continue;
      if (v->id->isglobal()) continue;

      if (fuse_signal_directives) {
	for (int k=0; k < A_LEN (n->ports); k++) {
	  if (n->ports[k].omit) continue;
	  if (n->ports[k].c == v->id) {
	    io_sig = 1;
	    if (n->ports[k].input) {
	      fprintf (fp, "   input ");
	    }
	    else {
	      fprintf (fp, "   output ");
	    }
	    break;
	  }
	}
      }

      if (!no_signal_decl) {
	if (v->input && !v->output) {
	  fprintf (fp, "   wire ");
	}
	else {
	  fprintf (fp, "   reg ");
	}
	emit_verilog_id (fp, v->id);
	fprintf (fp, ";\n");
      }
      else if (io_sig) {
	emit_verilog_id (fp, v->id);
	fprintf (fp, ";\n");
      }
    }
  }

  int iport = 0;

  /* now we print instances */
  fprintf (fp, "\n// --- instances\n");
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
	  emit_verilog_moduletype (fp, a, instproc);
          if (name_mangle) {
	    ActNamespace::Act()->mfprintf (fp, " %s", vx->getName());
	    if (as) {
	      char *s = as->string();
	      ActNamespace::Act()->mfprintf (fp, "%s", s);
	      FREE (s);
	    }
          }
          else {
	    fprintf (fp, " \\%s", vx->getName());
	    if (as) {
	      as->Print (fp);
	    }
          }
	  fprintf (fp, "  (");

	  int first = 1;
	  for (i=0; i < A_LEN (sub->ports); i++) {
	    if (sub->ports[i].omit) continue;
	    if (!first) {
	      fprintf (fp, ", ");
	    }
	    first = 0;
	    fprintf (fp, ".");
	    emit_verilog_id (fp, sub->ports[i].c);
	    fprintf (fp, "(");
	    emit_verilog_id (fp, n->instports[iport]);
	    fprintf (fp, ")");
	    iport++;
	  }
	  fprintf (fp, ");\n");
	}
	if (as) {
	  as->step ();
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
    fprintf (fp, "\n// -- circuit production rules\n");
    while (prs) {
      act_prs_lang_t *p;
      for (p = prs->p; p; p = p->next) {
	emit_verilog_prs (fp, p);
      }
      prs = prs->next;
    }
  }
#endif  

  fprintf (fp, "endmodule\n\n");
  return;
}

static void clear_visited_flag (Process *p)
{
  Assert (p->isExpanded(), "What?");

  act_boolean_netlist_t *n = BOOL->getBNL (p);
  if (!n) {
    Assert (0, "emit_verilog internal inconsistency");
  }

  if (!n->visited) return;
  n->visited = 0;

  ActUniqProcInstiter inst(p->CurScope());
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;
    clear_visited_flag (dynamic_cast<Process *>(vx->t->BaseType()));
  }
  return;
}


void act_emit_verilog (Act *a, FILE *fp, Process *p)
{
  ActPass *ap = a->pass_find ("booleanize");
  if (!ap) {
    BOOL = new ActBooleanizePass (a);
  }
  else {
    BOOL = dynamic_cast<ActBooleanizePass *> (ap);
  }
  config_set_default_string ("act.global_signal_prefix", "top.");
  config_set_default_int ("act2v.name_mangle", 0);
  config_set_default_int ("act2v.fuse_signal_directives", 0);
  config_set_default_int ("act2v.emit_cells", 1);
  config_set_default_int ("act2v.no_signal_decl", 0);

  verilog_emit_cells = config_get_int ("act2v.emit_cells");
  name_mangle = config_get_int ("act2v.name_mangle");
  fuse_signal_directives = config_get_int ("act2v.fuse_signal_directives");
  no_signal_decl = config_get_int ("act2v.no_signal_decl");
  Assert (BOOL, "what?");
  if (!BOOL->completed()) {
    BOOL->run (p);
  }
  global_signal_prefix = config_get_string ("act.global_signal_prefix");

  if (config_exists ("act2v.stop_at_process")) {
    int sz = config_get_table_size ("act2v.stop_at_process");
    char **tab = config_get_table_string ("act2v.stop_at_process");

    STOP_AT = phash_new (4);

    for (int i=0; i < sz; i++) {
      Process *p;
      p = a->findProcess (tab[i]);
      if (!p) {
	warning ("act2v.stop_at_process: %s not found", tab[i]);
      }
      else {
	phash_bucket_t *b;
	if (!p->isExpanded()) {
	  p = p->Expand (ActNamespace::Global(), p->CurScope(), 0, NULL);
	}
	b = phash_add (STOP_AT, p);
      }
    }
  }
  emit_verilog (fp, a, p);
  clear_visited_flag (p);
  if (STOP_AT) {
    phash_free (STOP_AT);
    STOP_AT = NULL;
  }
}
