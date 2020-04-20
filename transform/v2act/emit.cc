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

void emit_id_deref_fo (FILE *fp, id_deref_t *id);

static void _emit_adjusted_full_deref (FILE *fp, id_info_t *id, int k)
{
  fprintf (fp, "%s", id->myname);
  if (id->isport && id->a[0].lo > 0) {
    fprintf (fp, "[%d] /* port */", k - id->a[0].lo);
  }
  else {
    fprintf (fp, "[%d]", k);
  }
}

static void _emit_adjusted_deref (FILE *fp, id_info_t *id, int k)
{
  if (id->isport && id->a[0].lo > 0) {
    fprintf (fp, "[%d] /* port */", k - id->a[0].lo);
  }
  else {
    fprintf (fp, "[%d]", k);
  }
}

static void _emit_rhs_element (FILE *fp, struct connectinfo *rhs, int k)
{
  conn_rhs_t *r;
  if (rhs->r) {
    r = rhs->r;
  }
  else {
    listitem_t *li;
    li = list_first (rhs->l);
    while (k > 0) {
      Assert (li, "??");
      li = list_next (li);
      k--;
    }
    r = (conn_rhs_t *)list_value (li);
  }
  Assert (r, "What?");
  
  fprintf (fp, "%s", r->id.id->myname);
  if (r->id.isderef) {
    _emit_adjusted_deref (fp, r->id.id, r->id.deref);
  }
}

static void _emit_one_type_rhs (FILE *fp, struct idinfo *id)
{
  int i;

  fprintf (fp, "%s", id->myname);
  if (A_LEN (id->a) > 0) {
    for (i=0; i < A_LEN (id->a); i++) {
      if (id->a[i].lo == 0) {
	fprintf (fp, "[%d]", id->a[i].hi+1);
      }
      else {
	if (id->isport) {
	  fprintf (fp, "[%d] /* port */", id->a[i].hi - id->a[i].lo + 1);
	}
	else {
	  fprintf (fp, "[%d..%d]", id->a[i].lo, id->a[i].hi);
	}
      }
    }
  }
}


static void _emit_one_type (FILE *fp, struct idinfo *id)
{
  if (mode == V_SYNC) {
    fprintf (fp, "bool");
  }
  else {
    fprintf (fp, "%s", channame);
  }
  if (id->isinput) {
    fprintf (fp, "? ");
  }
  else if (id->isoutput) {
    fprintf (fp, "! ");
  }
  else {
    fprintf (fp, " ");
  }
  _emit_one_type_rhs (fp, id);
}

int array_length (conn_info_t *c)
{
  conn_rhs_t *r;
  int x;
  listitem_t *li;

  if (c->r) {
    /* simple */
    r = c->r;
    if (r->id.isderef) {
      /* not an array */
      return -1;
    }
    else {
      if (r->issubrange) {
	return (r->hi - r->lo + 1);
      }
      else {
	return -1;
      }
    }
  }
  else {
    x = 0;
    for (li = list_first (c->l); li; li = list_next (li)) {
      r = (conn_rhs_t *)list_value (li);
      if (r->id.isderef) {
	x += 1;
      }
      else {
	if (r->issubrange) {
	  x += (r->hi - r->lo + 1);
	}
	else {
	  x += 1;
	}
      }
    }
  }
  return x;
}

/*------------------------------------------------------------------------
 *
 *  emit_conn_rhs -- emit the rhs of a connection
 *
 *------------------------------------------------------------------------
 */
void emit_conn_rhs (FILE *fp, conn_rhs_t *r, list_t *l)
{
  listitem_t *li;
  int j;
  
  if (r) {
    /* simple */
    fprintf (fp, "%s", r->id.id->myname);
    if (r->id.isderef) {
      _emit_adjusted_deref (fp, r->id.id, r->id.deref);
    }
    else {
      if (r->issubrange) {
	fprintf (fp, "[%d..%d]", r->lo, r->hi);
      }
    }
  }
  else {
    int first = 1;
    /* exciting! */
    fprintf (fp, "{");
    list_reverse (l);
    for (li = list_first (l); li; li = list_next (li)) {
      r = (conn_rhs_t *)list_value (li);
      if (r->id.isderef) {
	if (!first) { fprintf (fp, ","); }
	first = 0;
	_emit_adjusted_full_deref (fp, r->id.id, r->id.deref);
      }
      else {
	if (r->issubrange) {
	  for (j=r->lo; j <= r->hi; j++) {
	    if (!first) { fprintf (fp, ","); }
	    first = 0;
	    _emit_adjusted_full_deref (fp, r->id.id, j);
	  }
	}
	else {
	  if (A_LEN (r->id.id->a) > 0) {
	    for (j=r->id.id->a[0].lo; j <= r->id.id->a[0].hi; j++) {
	      if (!first) { fprintf (fp, ","); }
	      first = 0;
	      _emit_adjusted_full_deref (fp, r->id.id, j);
	    }
	  }
	  else {
	    if (!first) { fprintf (fp, ","); }
	    first = 0;
	    fprintf (fp, "%s", r->id.id->myname);
	  }
	}
      }
    }
    list_reverse (l);
    fprintf (fp, "}");
  }
}

static
void emit_module_header (FILE *fp, module_t *m)
{
  int  first;
  struct idinfo *id;
  
  fprintf (fp, "defproc %s (", m->b->key);
  first = 1;

  for (int i=0; i < A_LEN (m->port_list); i++) {
    id = m->port_list[i];
    Assert (id->isport, "What?");

    if (id->isclk && mode == V_ASYNC) {
      /* ignore clock nets in async mode */
      continue;
    }

    if (first) {
      first = 0;
    }
    else {
      fprintf (fp, "; ");
    }
    _emit_one_type (fp, id);
  }
  fprintf (fp, ")");
}

/*------------------------------------------------------------------------
 *
 *  emit_types --
 *
 *   Emit all the types
 *
 *------------------------------------------------------------------------
 */
void emit_types (VNet *v)
{
  int i, j;
  hash_bucket_t *b;
  struct idinfo *id;
  conn_rhs_t *r;
  listitem_t *li;
  int first;
  module_t *m;
  int top_level_module = 0;
  int sink_count = 0;

  //fprintf (v->out, "import \"globals.act\";\n");
  //fprintf (v->out, "/* globals */\n");
  //fprintf (v->out, "bool Vdd, GND;\n\n");

  fprintf (v->out, "/* -- declarations -- */\n");
  for (m = v->hd; m; m = m->next) {
    emit_module_header (v->out, m);
    fprintf (v->out, ";\n");
  }
  fprintf (v->out, "\n");
  
  for (m = v->hd; m; m = m->next) {

    if (m->next == NULL) {
	top_level_module = 1;
    }

    emit_module_header (v->out, m);
    fprintf (v->out, "\n{\n");

    fprintf (v->out, "   /*--- types ---*/\n");
    /* now print all the other types out */
    for (i=0; i < m->H->size; i++) 
      for (b = m->H->head[i]; b; b = b->next) {
	id = (struct idinfo *)b->v;
	if (strcmp (id->myname, "Vdd") == 0 ||
	    strcmp (id->myname, "GND") == 0) {
	  int j;
	  for (j=0; j < id->fanout; j++) {
	    fprintf (v->out, "   source_%s src_%s_%d;\n", id->myname,
		     id->myname, j);
	  }
	  continue;
	}
	if (id->isclk && mode == V_ASYNC) {
	  /* ignore clock nets */
	  continue;
	}
	if (id->isport) {
	  goto copy_check;
	}
	if (id->ismodname) continue;
	fprintf (v->out, "   ");
	if (id->isinst) {
	  fprintf (v->out, "%s ", id->nm);
	  _emit_one_type_rhs (v->out, id);
	}
	else {
	  /* bool */
	  _emit_one_type (v->out, id);
	}
	fprintf (v->out, ";\n");
	if (id->isinst) {
	  if (mode == V_ASYNC) {
	    int k;

	    //fprintf (v->out, "   %s.g = g;\n", id->b->key);
	    for (k=0; k < id->nused; k++) {
	      if (id->used[k] == 0) {
		if (id->p) {
		  InstType *it;
		  it = id->p->getPortType (k);
		  Assert (it, "Hmm");
		  if (it->getDir() == Type::OUT) {
		    fprintf (v->out, "   sink __sink__%d(%s.%s);\n",
			     sink_count++, id->myname, id->p->getPortName(k));
		  }
		}
		else {
		  Assert (id->m, "Eh?");
		  if (id->m->port_list[k]->isoutput) {
		    if (A_LEN (id->m->port_list[k]->a) == 0) {
		      fprintf (v->out, "   sink __sink_%d(%s.%s);\n",
			       sink_count++, id->myname, id->m->port_list[k]->myname);
		    }
		    else {
		      int ii;
		      Assert (A_LEN (id->m->port_list[k]->a) == 1, "Not a 1D dangling array!");
		      for (ii=0; ii <= id->m->port_list[k]->a[0].hi - id->m->port_list[k]->a[0].lo; ii++) {
			fprintf (v->out, "   sink __sink_%d(%s.%s[%d]);\n",
				 sink_count++, id->myname, id->m->port_list[k]->myname,
				 ii);
		      }
		    }
		  }
		}
	      }
	    }
	  }
	  else {
	    /*
	    fprintf (v->out, "   %s.VDD = VDD;\n", id->b->key);
	    fprintf (v->out, "   %s.VSS = VSS;\n", id->b->key);
	    */
	  }
	}

      copy_check:	
	if (mode == V_ASYNC) {
	  if (A_LEN (id->a) > 0) {
	    int j;
	    /* array */
	    if (id->fa) {
	      for (j=0; j < id->a[0].hi - id->a[0].lo + 1; j++) {
		if (id->fa[j] > 1) {
		  fprintf (v->out, "   copy<%d> __cpy_%s_%d (%s[%d],);\n",
			   id->fa[j], id->myname, j + id->a[0].lo, 
			   id->myname, j + id->a[0].lo);
		}
	      }
	    }
	  }
	  else {
	    if (id->fanout > 1) {
	      /* we need to do something! */
	      fprintf (v->out, "   copy<%d> __cpy_%s_ (%s,);\n",
		       id->fanout, id->myname, id->myname);
	    }
	  }
	}
      }

    /* print out the connections */
    fprintf (v->out, "   /*--- connections ---*/\n");


    char *pending = NULL;
    
    for (i=0; i < A_LEN (m->conn); i++) {
      /* skip clock connections */
      if ((mode == V_ASYNC) && m->conn[i]->isclk) continue;

      if (!pending) fprintf (v->out, "   ");

      if (mode == V_ASYNC) {
	if (m->conn[i]->dir == 1) {
	  /* RHS := LHS */
	  if (m->conn[i]->prefix) {
	    /* we know that LHS fanout := 1 */
	    fprintf (v->out, "%s.", m->conn[i]->prefix->myname);
	    emit_id_deref (v->out, &m->conn[i]->id);
	    fprintf (v->out, "=");
	    emit_conn_rhs (v->out, m->conn[i]->r, m->conn[i]->l);
	    fprintf (v->out, ";");
	  }
	  else {
	    if (m->conn[i]->id.isderef) {
	      if (m->conn[i]->id.id->fanout > 1) {
		emit_id_deref_fo (v->out, &m->conn[i]->id);
		fprintf (v->out, "=");
		emit_conn_rhs (v->out, m->conn[i]->r, m->conn[i]->l);
		fprintf (v->out, ";");
	      }
	      else {
		emit_id_deref (v->out, &m->conn[i]->id);
		fprintf (v->out, "=");
		emit_conn_rhs (v->out, m->conn[i]->r, m->conn[i]->l);
		fprintf (v->out, ";");
	      }
	    }
	    else {
	      if (A_LEN (m->conn[i]->id.id->a) > 0) {
		int j;
		/* element-by-element connection! */
		for (j=m->conn[i]->id.id->a[0].lo; 
		     j <= m->conn[i]->id.id->a[0].hi; j++) {
		  if (m->conn[i]->id.id->fa &&
		      m->conn[i]->id.id->fa[j-m->conn[i]->id.id->a[0].lo] > 1) {
		    /* fanout! */
		    fprintf (v->out, "__cpy_%s_%d.out[%d]", 
			     m->conn[i]->id.id->myname, j,
			     m->conn[i]->id.id->fcur[j-m->conn[i]->id.id->a[0].lo]++);
		  }
		  else {
		    _emit_adjusted_full_deref (v->out, m->conn[i]->id.id, j);
		  }
		  fprintf (v->out, "=");
		  /* RHS */
		  _emit_rhs_element (v->out, m->conn[i], 
				     j - m->conn[i]->id.id->a[0].lo);
		}
	      }
	      else {
		if (m->conn[i]->id.id->fanout > 1) {
		  emit_id_deref_fo (v->out, &m->conn[i]->id);
		  fprintf (v->out, "=");
		  emit_conn_rhs (v->out, m->conn[i]->r, m->conn[i]->l);
		  fprintf (v->out, ";");
		}
		else {
		  emit_id_deref (v->out, &m->conn[i]->id);
		  fprintf (v->out, "=");
		  emit_conn_rhs (v->out, m->conn[i]->r, m->conn[i]->l);
		  fprintf (v->out, ";");
		}
	      }
	    }
	  }
	}
	else {
 	  /* LHS := RHS */
	  if (m->conn[i]->l) {
	    listitem_t *li;
	    conn_rhs_t *r;
	    int j = 0;
	    int first;

	    if (m->conn[i]->prefix) {
	      fprintf (v->out, "%s.", m->conn[i]->prefix->myname);
	    }
	    emit_id_deref (v->out, &m->conn[i]->id);
	    if (!m->conn[i]->prefix) {
	      Assert (A_LEN (m->conn[i]->id.id->a) > 0, "???");
	      fprintf (v->out, "[%d..%d]", m->conn[i]->id.id->a[0].lo,
		       m->conn[i]->id.id->a[0].hi);
	    }
	    fprintf (v->out, "=");

	    list_reverse (m->conn[i]->l);
	    fprintf (v->out, "{");
	    first = 1;
	    for (li = list_first (m->conn[i]->l); li; li = list_next (li)) {
	      r = (conn_rhs_t *)list_value (li);
	      if (r->id.isderef) {
		if (!first) {
		  fprintf (v->out, ",");
		}
		first = 0;
		if (r->id.id->fa && r->id.id->fa[r->id.deref - 
						 r->id.id->a[0].lo] > 1) {
		  fprintf (v->out, "__cpy_%s_%d.out[%d]",
			   r->id.id->myname, 
			   r->id.deref, 
			   r->id.id->fcur[r->id.deref - r->id.id->a[0].lo]++);
		}
		else {
		  _emit_adjusted_full_deref (v->out, r->id.id, r->id.deref);
		}
	      }
	      else {
		int j;
		if (r->issubrange) {
		  for (j = r->lo; j <= r->hi; j++) {
		    if (!first) {
		      fprintf (v->out, ",");
		    }
		    first = 0;
		    if (r->id.id->fa && r->id.id->fa[j - r->id.id->a[0].lo] > 1) {
		      fprintf (v->out, "__cpy_%s_%d.out[%d]",
			       r->id.id->myname, j,
			       r->id.id->fcur[j - r->id.id->a[0].lo]++);
		    }
		    else {
		      _emit_adjusted_full_deref (v->out, r->id.id, j);
		    }
		  }
		}
		else {
		  if (A_LEN (r->id.id->a) > 0) {
		    for (j=r->id.id->a[0].lo; j <= r->id.id->a[0].hi; j++) {
		      if (!first) {
			fprintf (v->out, ",");
		      }
		      first = 0;
		      if (r->id.id->fa && r->id.id->fa[j - r->id.id->a[0].lo] > 1) {
			fprintf (v->out, "__cpy_%s_%d.out[%d]",
				 r->id.id->myname, j, 
				 r->id.id->fcur[j - r->id.id->a[0].lo]++);
		      }
		      else {
			_emit_adjusted_full_deref (v->out, r->id.id, j);
		      }
		    }
		  }
		  else {
		    if (!first) {
		      fprintf (v->out, ",");
		    }
		    first = 0;
		    if (r->id.id->fanout > 1) {
		      emit_id_deref_fo (v->out, &r->id);
		    }
		    else {
		      emit_id_deref (v->out, &r->id);
		    }
		  }
		}
	      }
	    }
	    list_reverse (m->conn[i]->l);
	    fprintf (v->out, "};");
	  }
	  else if (m->conn[i]->r->issubrange) {
	    conn_rhs_t *r = m->conn[i]->r;
	    int first;

	    if (m->conn[i]->prefix) {
	      fprintf (v->out, "%s.", m->conn[i]->prefix->myname);
	    }
	    emit_id_deref (v->out, &m->conn[i]->id);
	    if (!m->conn[i]->prefix) {
	      Assert (A_LEN (m->conn[i]->id.id->a) > 0, "???");
	      fprintf (v->out, "[%d..%d]", m->conn[i]->id.id->a[0].lo,
		       m->conn[i]->id.id->a[0].hi);
	    }
	    fprintf (v->out, "=");

	    fprintf (v->out, "{");
	    first = 1;
	    for (j=r->lo; j <= r->hi; j++) {
	      if (!first) {
		fprintf (v->out, ",");
	      }
	      first = 0;
	      if (r->id.id->fa && r->id.id->fa[j - r->id.id->a[0].lo] > 1) {
		fprintf (v->out, "__cpy_%s_%d.out[%d]",
			 r->id.id->myname, j, 
			 r->id.id->fcur[j - r->id.id->a[0].lo]++);
	      }
	      else {
		_emit_adjusted_full_deref (v->out, r->id.id, j);
	      }
	    }
	    fprintf (v->out, "};");
	  }
	  else if (m->conn[i]->r->id.isderef == 0 &&
		   A_LEN (m->conn[i]->r->id.id->a) > 0) {
	    conn_rhs_t *r = m->conn[i]->r;
	    int first;

	    if (m->conn[i]->prefix) {
	      fprintf (v->out, "%s.", m->conn[i]->prefix->myname);
	    }
	    emit_id_deref (v->out, &m->conn[i]->id);
	    if (!m->conn[i]->prefix) {
	      Assert (A_LEN (m->conn[i]->id.id->a) > 0, "???");
	      fprintf (v->out, "[%d..%d]", m->conn[i]->id.id->a[0].lo,
		       m->conn[i]->id.id->a[0].hi);
	    }
	    fprintf (v->out, "=");

	    fprintf (v->out, "{");
	    first = 1;

	    for (j=r->id.id->a[0].lo; j <= r->id.id->a[0].hi; j++) {
	      if (!first) {
		fprintf (v->out, ",");
	      }
	      first = 0;
	      if (r->id.id->fa && r->id.id->fa[j - r->id.id->a[0].lo] > 1) {
		fprintf (v->out, "__cpy_%s_%d.out[%d]",
			 r->id.id->myname, j, 
			 r->id.id->fcur[j - r->id.id->a[0].lo]++);
	      }
	      else {
		_emit_adjusted_full_deref (v->out, r->id.id, j);
	      }
	    }
	    fprintf (v->out, "};");
	  }
	  else {
	    if (m->conn[i]->prefix) {
	      fprintf (v->out, "%s.", m->conn[i]->prefix->myname);
	    }
	    emit_id_deref (v->out, &m->conn[i]->id);
	    fprintf (v->out, "=");

	    if (m->conn[i]->r->id.isderef == 0) {
	      if (m->conn[i]->r->id.id->fanout > 1) {
		emit_id_deref_fo (v->out, &m->conn[i]->r->id);
	      }
	      else {
		emit_id_deref (v->out, &m->conn[i]->r->id);
	      }
	    }
	    else {
	      int idx = m->conn[i]->r->id.deref;
	      int base = m->conn[i]->r->id.id->a[0].lo;

	      if (m->conn[i]->r->id.id->fa && m->conn[i]->r->id.id->fa[idx - base] > 1) {
		fprintf (v->out, "__cpy_%s_%d.out[%d]",
			 m->conn[i]->r->id.id->myname, idx, 
			 m->conn[i]->r->id.id->fcur[idx-base]++);
	      }
	      else {
		emit_id_deref (v->out, &m->conn[i]->r->id);
	      }
	    }
	    fprintf (v->out, ";");
	  }
	}
      }
      else {
	int f = 0;
	if (m->conn[i]->prefix) {
	  if (!pending) {
	    fprintf (v->out, "%s(", m->conn[i]->prefix->myname);
	    f = 1;
	  }
	  else if (m->conn[i]->prefix->myname != pending) {
	    fprintf (v->out, ");\n   ");
	    fprintf (v->out, "%s(", m->conn[i]->prefix->myname);
	    f = 1;
	  }
	  pending = m->conn[i]->prefix->myname;
	}
	else {
	  if (pending) {
	    fprintf (v->out, ");\n");
	  }
	  pending = NULL;
	}
	if (pending) {
	  if (!f) {
	    fprintf (v->out, ", ");
	  }
	  fprintf (v->out, ".");
	  emit_id_deref (v->out, &m->conn[i]->id);
	  fprintf (v->out, "=");
	  emit_conn_rhs (v->out, m->conn[i]->r, m->conn[i]->l);
	}
	else {
	  emit_id_deref (v->out, &m->conn[i]->id);
	  fprintf (v->out, "=");
	  emit_conn_rhs (v->out, m->conn[i]->r, m->conn[i]->l);
	  fprintf (v->out, ";");
	}
      }
      if (!pending) {
	fprintf (v->out, "\n");
      }
    }
    if (pending) {
      fprintf (v->out, ");\n");
    }
    fprintf (v->out, "}\n\n");
  }
}


/*------------------------------------------------------------------------
 *
 *  free_typetable --
 *
 *   Release type storage
 *
 *------------------------------------------------------------------------
 */
void free_typetable (struct Hashtable *H)
{
  int i;
  hash_bucket_t *b;
  struct idinfo *id;

  for (i=0; i < H->size; i++) 
    for (b = H->head[i]; b; b = b->next) {
      id = (struct idinfo *)b->v;
      A_FREE (id->a);
      FREE (id);
    }
  hash_free (H);
}


/*
 * update ID info
 *
 */
void update_id_info (id_info_t *id)
{
  if (id->m) {
    id->m->inst_exists = 1;
    id->nused = A_LEN (id->m->port_list);
    id->p = NULL;
  }
  if (id->nused > 0) {
    MALLOC (id->used, int, id->nused);
    for (int i=0; i < id->nused; i++) {
      id->used[i] = 0;
    }
  }
}

void update_conn_info (id_info_t *id)
{
  if (!id->m) return;
  if (id->mod && id->conn_start != -1) {
    for (int i=id->conn_start; i <= id->conn_end; i++) {
      int k;
      printf ("mod len = %d\n", A_LEN (id->m->port_list));
      for (k=0; k < A_LEN (id->m->port_list); k++) {
	if (strcmp (id->mod->conn[i]->id.id->myname,
		    id->m->port_list[k]->myname) == 0) {
	  id->used[k] = 1;
	  break;
	}
      }
      if (k == A_LEN (id->m->port_list)) {
	fatal_error ("Connection to unknown port `%s' for %s?", id->mod->conn[i]->id.id->myname, id->m->b->key);
      }
    }
  }
}


/*------------------------------------------------------------------------
 *
 *  emit_id_deref --
 *
 *   Emit the id with optional deref
 *
 *------------------------------------------------------------------------
 */
void emit_id_deref (FILE *fp, id_deref_t *id)
{
  if (mode == V_ASYNC) {
    if (strcmp (id->id->myname, "Vdd") == 0 ||
	strcmp (id->id->myname, "GND") == 0) {
      fprintf (fp, "src_%s_%d.out", id->id->myname,
	       id->id->curnum++);
    }
    else {
      fprintf (fp, "%s", id->id->myname);
    }
  }
  else {
    fprintf (fp, "%s", id->id->myname);
  }
  if (id->isderef) {
    if (id->id->isport && id->id->a[0].lo > 0) {
      fprintf (fp, "[%d]", id->deref - id->id->a[0].lo);
    }
    else {
      fprintf (fp, "[%d]", id->deref);
    }
  }
}

/*------------------------------------------------------------------------
 *
 *  emit_id_deref --
 *
 *   Emit the id with optional deref
 *
 *------------------------------------------------------------------------
 */
void emit_id_deref_fo (FILE *fp, id_deref_t *id)
{
  if (strcmp (id->id->myname, "Vdd") == 0) {
    fprintf (fp, "src_%s_%d.out", id->id->myname, id->id->curnum++);
    return;
  }
  else if (strcmp (id->id->myname, "GND") == 0) {
    fprintf (fp, "src_%s_%d.out", id->id->myname, id->id->curnum++);
    return;
  }
  if (id->isderef) {
    fprintf (fp, "__cpy_%s_%d.out[%d]", 
	     id->id->myname, 
	     id->deref,
	     id->id->fcur[id->deref - id->id->a[0].lo]++);
  }
  else {
    fprintf (fp, "__cpy_%s_.out[%d]",
	     id->id->myname,
	     id->id->curnum++);
  }
}
