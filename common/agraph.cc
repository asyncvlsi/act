/*************************************************************************
 *
 *  Copyright (c) 2020 Rajit Manohar
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
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
#include "agraph.h"

AGraph::AGraph(AGinfo *_info)
{
  A_INIT (edges);
  A_INIT (vertices);
  A_INIT (inp);
  A_INIT (outp);
  info = _info;
  _vtx_info = NULL;
  _edge_info = NULL;
  _dfs_apply_node = NULL;
  _dfs_apply_edge = NULL;
  _dfs_cookie = NULL;
}

AGraph::~AGraph()
{
  for (int i=0; i < A_LEN (edges); i++) {
    if (edges[i].info) {
      delete edges[i].info;
    }
  }
  A_FREE (edges);

  for (int i=0; i < A_LEN (vertices); i++) {
    if (vertices[i].info) {
      delete vertices[i].info;
    }
  }

  A_FREE (vertices);
  A_FREE (inp);
  A_FREE (outp);
  if (info) {
    delete info;
  }
  if (_vtx_info) {
    delete _vtx_info;
  }
  if (_edge_info) {
    delete _edge_info;
  }
  info = NULL;
}

AGinfo *AGraph::getInfo ()
{
  return info;
}

int AGraph::V2idx (AGvertex *v)
{
  if (v == NULL) return -1;
  return (v - &vertices[0]);
}

int AGraph::addVertex (AGinfo *info)
{
  AGvertex *v;
  
  A_NEW (vertices, AGvertex);
  v = &A_NEXT (vertices);
  A_INC (vertices);

  v->ehd = -1;
  v->bhd = -1;
  v->info = info;
  v->vid = A_LEN (vertices)-1;
  v->isio = 0;
  v->visited = 0;

  return v->vid;
}

int AGraph::addInput (AGinfo *info)
{
  int v = addVertex (info);
  vertices[v].isio = 1;
  A_NEW (inp, int);
  A_NEXT (inp) = v;
  A_INC (inp);
  return v;
}

void AGraph::mkInput (int v)
{
  Assert (v >= 0 && v < A_LEN (vertices), "vertex indx out of range");
  Assert (vertices[v].isio == 0, "Already I/O vertex");
  vertices[v].isio = 1;
  A_NEW (inp, int);
  A_NEXT (inp) = v;
  A_INC (inp);
}

void AGraph::mkOutput (int v)
{
  Assert (v >= 0 && v < A_LEN (vertices), "vertex indx out of range");
  Assert (vertices[v].isio == 0, "Already I/O vertex");
  vertices[v].isio = 2;
  A_NEW (outp, int);
  A_NEXT (outp) = v;
  A_INC (outp);
}  

int AGraph::addOutput (AGinfo *info)
{
  int v = addVertex (info);
  vertices[v].isio = 2;
  A_NEW (outp, int);
  A_NEXT (outp) = v;
  A_INC (outp);
  return v;
}

int AGraph::addEdge (int src, int dst, AGinfo *info)
{
  AGedge *e;
  AGvertex *sv;

  Assert (src >= 0 && src < A_LEN (vertices), "src indx out of range");
  Assert (dst >= 0 && dst < A_LEN (vertices), "dst indx out of range");
  
  A_NEW (edges, AGedge);
  e = &A_NEXT (edges);
  A_INC (edges);

  e->eid = A_LEN (edges)-1;
  e->src = src;
  e->dst = dst;
  e->info = info;

  sv = getVertex (src);
  
  e->enext = sv->ehd;
  sv->ehd = e->eid;

  sv = getVertex (dst);
  e->eback = sv->bhd;
  sv->bhd = e->eid;
  
  return e->eid;
}



int AGraph::numEdges ()
{
  return A_LEN (edges);
}

int AGraph::numVertices ()
{
  return A_LEN (vertices);
}

AGedge *AGraph::getEdge (int i)
{
  return &edges[i];
}

AGvertex *AGraph::getVertex (int i)
{
  Assert (i >= 0 && i < A_LEN (vertices), "What?");
  return &vertices[i];
}

int AGraph::numInputs ()
{
  return A_LEN (inp);
}

int AGraph::numOutputs ()
{
  return A_LEN (outp);
}

AGvertex *AGraph::getInput (int i)
{
  Assert (i >= 0 && i < A_LEN (inp), "AGraph::getInput() range error");
  return &vertices[inp[i]];
}

AGvertex *AGraph::getOutput (int i)
{
  Assert (i >= 0 && i < A_LEN (outp), "AGraph::getInput() range error");
  return &vertices[outp[i]];
}
  



/*
 * Iterators
 */

AGraphVertexIter::AGraphVertexIter (AGraph *_g)
{
  g = _g;
  i = 0;
}

AGraphVertexIter::AGraphVertexIter (const AGraphVertexIter& gi)
{
  g = gi.g;
  i = gi.i;
}

AGraphVertexIter AGraphVertexIter::operator ++(int)
{
  AGraphVertexIter tmp(*this);
  operator++();
  return tmp;
}

bool AGraphVertexIter::operator==(const AGraphVertexIter &rhs) const
{
   return (rhs.g == g && rhs.i == i);
}

bool AGraphVertexIter::operator!=(const AGraphVertexIter &rhs) const
{
 return !operator==(rhs);
}

AGvertex *AGraphVertexIter::operator*()
{
 if (i < 0 || i >= g->numVertices()) return NULL;
 return g->getVertex (i);
}

AGraphVertexIter AGraphVertexIter::begin()
{
 AGraphVertexIter tmp(*this);
 tmp.i = 0;
 return tmp;
}

AGraphVertexIter AGraphVertexIter::end()
{
 AGraphVertexIter tmp(*this);
 tmp.i = g->numVertices();
 return tmp;
}

AGraphVertexIter& AGraphVertexIter::operator++()
{
 i++;
 return *this;
}



AGraphEdgeIter::AGraphEdgeIter (AGraph *_g)
{
  g = _g;
  i = 0;
}

AGraphEdgeIter::AGraphEdgeIter (const AGraphEdgeIter& gi)
{
  g = gi.g;
  i = gi.i;
}

AGraphEdgeIter AGraphEdgeIter::operator ++(int)
{
  AGraphEdgeIter tmp(*this);
  operator++();
  return tmp;
}

bool AGraphEdgeIter::operator==(const AGraphEdgeIter &rhs) const
{
   return (rhs.g == g && rhs.i == i);
}

bool AGraphEdgeIter::operator!=(const AGraphEdgeIter &rhs) const
{
 return !operator==(rhs);
}

AGedge *AGraphEdgeIter::operator*()
{
 if (i < 0 || i >= g->numEdges()) return NULL;
 return g->getEdge (i);
}

AGraphEdgeIter AGraphEdgeIter::begin()
{
 AGraphEdgeIter tmp(*this);
 tmp.i = 0;
 return tmp;
}

AGraphEdgeIter AGraphEdgeIter::end()
{
 AGraphEdgeIter tmp(*this);
 tmp.i = g->numEdges();
 return tmp;
}

AGraphEdgeIter& AGraphEdgeIter::operator++()
{
 i++;
 return *this;
}


AGvertexFwdIter::AGvertexFwdIter (AGraph *_g, int v)
{
  g = _g;
  vid = v;
  i = _g->getVertex (v)->ehd;
}

AGvertexFwdIter::AGvertexFwdIter (AGraph *_g, AGvertex *v)
{
  g = _g;
  vid = _g->V2idx (v);
  i = _g->getVertex (vid)->ehd;
}

AGvertexFwdIter::AGvertexFwdIter (const AGvertexFwdIter& gi)
{
  g = gi.g;
  vid = gi.vid;
  i = gi.i;
}

AGvertexFwdIter AGvertexFwdIter::operator ++(int)
{
  AGvertexFwdIter tmp(*this);
  operator++();
  return tmp;
}

bool AGvertexFwdIter::operator==(const AGvertexFwdIter &rhs) const
{
   return (rhs.g == g && rhs.i == i && rhs.vid == vid);
}

bool AGvertexFwdIter::operator!=(const AGvertexFwdIter &rhs) const
{
 return !operator==(rhs);
}

AGedge *AGvertexFwdIter::operator*()
{
 if (i < 0 || i >= g->numEdges()) return NULL;
 return g->getEdge (i);
}

AGvertexFwdIter AGvertexFwdIter::begin()
{
 AGvertexFwdIter tmp(*this);
 tmp.i = g->getVertex (vid)->ehd;
 return tmp;
}

AGvertexFwdIter AGvertexFwdIter::end()
{
 AGvertexFwdIter tmp(*this);
 tmp.i = -1;
 return tmp;
}

AGvertexFwdIter& AGvertexFwdIter::operator++()
{
 i = g->getEdge (i)->enext;
 return *this;
}

AGvertexBwdIter::AGvertexBwdIter (AGraph *_g, int v)
{
  g = _g;
  vid = v;
  i = _g->getVertex (v)->bhd;
}

AGvertexBwdIter::AGvertexBwdIter (const AGvertexBwdIter& gi)
{
  g = gi.g;
  vid = gi.vid;
  i = gi.i;
}

AGvertexBwdIter AGvertexBwdIter::operator ++(int)
{
  AGvertexBwdIter tmp(*this);
  operator++();
  return tmp;
}

bool AGvertexBwdIter::operator==(const AGvertexBwdIter &rhs) const
{
   return (rhs.g == g && rhs.i == i && rhs.vid == vid);
}

bool AGvertexBwdIter::operator!=(const AGvertexBwdIter &rhs) const
{
 return !operator==(rhs);
}

AGedge *AGvertexBwdIter::operator*()
{
 if (i < 0 || i >= g->numEdges()) return NULL;
 return g->getEdge (i);
}

AGvertexBwdIter AGvertexBwdIter::begin()
{
 AGvertexBwdIter tmp(*this);
 tmp.i = g->getVertex (vid)->bhd;
 return tmp;
}

AGvertexBwdIter AGvertexBwdIter::end()
{
 AGvertexBwdIter tmp(*this);
 tmp.i = -1;
 return tmp;
}

AGvertexBwdIter& AGvertexBwdIter::operator++()
{
 i = g->getEdge (i)->eback;
 return *this;
}


AGraphOutVertexIter::AGraphOutVertexIter (AGraph *_g)
{
  g = _g;
  i = 0;
}

AGraphOutVertexIter::AGraphOutVertexIter (const AGraphOutVertexIter& gi)
{
  g = gi.g;
  i = gi.i;
}

AGraphOutVertexIter AGraphOutVertexIter::operator ++(int)
{
  AGraphOutVertexIter tmp(*this);
  operator++();
  return tmp;
}

bool AGraphOutVertexIter::operator==(const AGraphOutVertexIter &rhs) const
{
   return (rhs.g == g && rhs.i == i);
}

bool AGraphOutVertexIter::operator!=(const AGraphOutVertexIter &rhs) const
{
 return !operator==(rhs);
}

AGvertex *AGraphOutVertexIter::operator*()
{
 if (i < 0 || i >= g->numOutputs()) return NULL;
 return g->getOutput(i);
}

AGraphOutVertexIter AGraphOutVertexIter::begin()
{
 AGraphOutVertexIter tmp(*this);
 tmp.i = 0;
 return tmp;
}

AGraphOutVertexIter AGraphOutVertexIter::end()
{
 AGraphOutVertexIter tmp(*this);
 tmp.i = g->numOutputs();
 return tmp;
}

AGraphOutVertexIter& AGraphOutVertexIter::operator++()
{
 i++;
 return *this;
}

AGraphInpVertexIter::AGraphInpVertexIter (AGraph *_g)
{
  g = _g;
  i = 0;
}

AGraphInpVertexIter::AGraphInpVertexIter (const AGraphInpVertexIter& gi)
{
  g = gi.g;
  i = gi.i;
}

AGraphInpVertexIter AGraphInpVertexIter::operator ++(int)
{
  AGraphInpVertexIter tmp(*this);
  operator++();
  return tmp;
}

bool AGraphInpVertexIter::operator==(const AGraphInpVertexIter &rhs) const
{
   return (rhs.g == g && rhs.i == i);
}

bool AGraphInpVertexIter::operator!=(const AGraphInpVertexIter &rhs) const
{
 return !operator==(rhs);
}

AGvertex *AGraphInpVertexIter::operator*()
{
 if (i < 0 || i >= g->numInputs ()) return NULL;
 return g->getInput (i);
}

AGraphInpVertexIter AGraphInpVertexIter::begin()
{
 AGraphInpVertexIter tmp(*this);
 tmp.i = 0;
 return tmp;
}

AGraphInpVertexIter AGraphInpVertexIter::end()
{
 AGraphInpVertexIter tmp(*this);
 tmp.i = g->numInputs(); 
 return tmp;
}

AGraphInpVertexIter& AGraphInpVertexIter::operator++()
{
 i++;
 return *this;
}

void AGraph::printDot (FILE *fp, const char *name)
{
 AGvertex *v;
 AGedge *e;
 const char *t;
 
 fprintf (fp, "digraph \"%s\" {\n", name ? name : "default_name");

 for (int i=0; i < A_LEN (vertices); i++) {
   v = &vertices[i];
   if (v->info) {
     t = v->info->info();
   }
   else {
     t = "";
   }
   fprintf (fp, " v%d [label=\"#%d%s %s\"];\n", i, i,
	    (v->isio == 0 ? "" : (v->isio == 1 ? "-i" : "-o")), t);
 }

 for (int i=0; i < A_LEN (edges); i++) {
   e = &edges[i];
   if (e->info) {
     t = e->info->info();
   }
   else {
     t = "";
   }
   fprintf (fp, " v%d -> v%d [label=\"%s\"];\n", e->src, e->dst, t);
 }
 fprintf (fp, "}\n");
}


void AGraph::save (FILE *fp)
{
 if (info) {
   fprintf (fp, "+ ");
   info->save (fp);
 }
 else {
   fprintf (fp, "* ");
 }
 fprintf (fp, "\n%d %d\n", A_LEN (vertices), A_LEN (edges));
 for (int i=0; i < A_LEN (vertices); i++) {
   fprintf (fp, "%d ", vertices[i].isio);
   if (vertices[i].getInfo()) {
     fprintf (fp, "+ ");
     vertices[i].getInfo()->save (fp);
   }
   else {
     fprintf (fp, "* ");
   }
   fprintf (fp, "\n");
 }
 for (int i=0; i < A_LEN (edges); i++) {
   fprintf (fp, "%d %d ", edges[i].src, edges[i].dst);
   if (edges[i].getInfo()) {
     fprintf (fp, "+ ");
     edges[i].getInfo()->save (fp);
   }
   else {
     fprintf (fp, "* ");
   }
   fprintf (fp, "\n");
 }
}

#define RESTORE_ERR					\
  do {							\
    fprintf (stderr, "Error restoring graph\n");	\
    return ;						\
  } while (0)

void AGraph::restore (FILE *fp)
{
  char buf[128];
  if (A_LEN (vertices) != 0) {
    fprintf (stderr, "Can only restore an empty graph\n");
    return;
  }
  if (fscanf (fp, " %1s", buf) != 1) {
    RESTORE_ERR;
  }
  if (buf[0] == '+') {
    if (!info) {
      RESTORE_ERR;
    }
    info->restore (fp);
  }
  else if (buf[0] != '*') {
    RESTORE_ERR;
  }
  int vl, el;
  if (fscanf (fp, "%d %d", &vl, &el) != 2) {
    RESTORE_ERR;
  }
  if (vl < 0 || el < 0) {
    RESTORE_ERR;
  }
  if (vl == 0) {
    return;
  }
  A_NEWP (vertices, AGvertex, vl);
  for (int i=0; i < vl; i++) {
    int flag;
    if (fscanf (fp, "%d", &flag) != 1) {
      RESTORE_ERR;
    }
    int tmp = addVertex (NULL);
    if (tmp != i) {
      RESTORE_ERR;
    }
    if (flag == 1) {
      mkInput (i);
    }
    else if (flag == 2) {
      mkOutput (i);
    }
    if (fscanf (fp, " %1s", buf) != 1) {
      RESTORE_ERR;
    }
    if (buf[0] == '+') {
      if (!_vtx_info) {
	RESTORE_ERR;
      }
      vertices[i].setInfo (_vtx_info->restore (fp));
    }
    else if (buf[0] != '*') {
      RESTORE_ERR;
    }
  }
  if (el > 0) {
    A_NEWP (edges, AGedge, el);
    for (int i=0; i < el; i++) {
      int  src, dst;
      if (fscanf (fp, "%d %d", &src, &dst) != 2) {
	RESTORE_ERR;
      }
      int idx = addEdge (src, dst);
      if (idx != i) {
	RESTORE_ERR;
      }
      if (fscanf (fp, " %1s", buf) != 1) {
	RESTORE_ERR;
      }
      if (buf[0] == '+') {
	if (!_edge_info) {
	  RESTORE_ERR;
	}
	edges[i].setInfo (_edge_info->restore (fp));
      }
      else if (buf[0] != '*') {
	RESTORE_ERR;
      }
    }
  }
}

void AGraph::_mark_reverse (int idx, AGSCCInfo *scc)
{
 if (scc->scc_idx[idx] != -1) return;
 scc->scc_idx[idx] = scc->scc_num;

 AGvertexBwdIter bw(this, idx);
 for (bw = bw.begin(); bw != bw.end(); bw++) {
   AGedge *e = (*bw);
   _mark_reverse (e->src, scc);
 }
}


void AGraph::_compute_scc_helper (int idx)
{
 AGvertexFwdIter fw(this, idx);
 
 getVertex(idx)->visited = 1;

 /* on entry */
 if (_dfs_apply_node != NULL) {
   (*_dfs_apply_node) (_dfs_cookie, getVertex (idx), true);
 }
 
 for (fw = fw.begin(); fw != fw.end(); fw++) {
   AGedge *e = (*fw);

   if (_dfs_apply_edge != NULL) {
     (*_dfs_apply_edge) (_dfs_cookie, this, e);
   }
   
   if (!getVertex (e->dst)->visited) {
     _compute_scc_helper (e->dst);
   }
 }

 /* on exit */
 if (_dfs_apply_node != NULL) {
   (*_dfs_apply_node) (_dfs_cookie, getVertex (idx), false);
 }

}

list_t *AGraph::runDFS (void *cookie,
			void (*fn_node) (void *, AGvertex *, bool),
			void (*fn_edge) (void *, AGraph *, AGedge *))
{
 if (A_LEN (vertices) == 0) return NULL;

 for (int i=0; i < A_LEN (vertices); i++) {
   vertices[i].visited = 0;
 }
 _dfs_cookie = cookie;
 _dfs_apply_node = fn_node;
 _dfs_apply_edge = fn_edge;

 list_t *l = list_new ();
 for (int i=0; i < A_LEN (vertices); i++) {
   if (vertices[i].visited == 0) {
     list_iappend_head (l, i);
     _compute_scc_helper (i);
   }
 }

 _dfs_cookie = NULL;
 _dfs_apply_node = NULL;
 _dfs_apply_edge = NULL;
 
 return l;
}

AGraph *AGraph::computeSCC()
{
 AGSCCInfo *info = new AGSCCInfo ();
 AGraph *ret = new AGraph (info);

 info->n = A_LEN (vertices);
 if (info->n == 0) {
   return ret;
 }
 MALLOC (info->scc_idx, int, info->n);
 for (int i=0; i < info->n; i++) {
   info->scc_idx[i] = -1;
   vertices[i].visited = 0;
 }

 list_t *l = list_new ();
 for (int i=0; i < A_LEN (vertices); i++) {
   if (vertices[i].visited == 0) {
     list_iappend_head (l, i);
     _compute_scc_helper (i);
   }
 }
 for (int i=0; i < A_LEN (vertices); i++) {
   vertices[i].visited = 0;
 }

 while (!list_isempty (l)) {
   int v = list_delete_ihead (l);
   if (info->scc_idx[v] != -1) {
     continue;
   }
   _mark_reverse (v, info);
   info->scc_num++;
 }
 list_free (l);

 for (int i=0; i < info->scc_num; i++) {
   ret->addVertex (NULL);
 }

 for (int i=0; i < A_LEN (vertices); i++) {
   AGvertexFwdIter fw(this, i);
   for (fw = fw.begin(); fw != fw.end(); fw++) {
     AGedge *e = (*fw);
     if (info->scc_idx[e->src] != info->scc_idx[e->dst]) {
       AGvertexFwdIter fw2(ret, info->scc_idx[e->src]);
       for (fw2 = fw2.begin(); fw2 != fw2.end(); fw2++) {
	 AGedge *e2 = (*fw2);
	 if (e2->dst == info->scc_idx[e->dst])
	   break;
       }
       if (fw2 == fw2.end()) {
	 ret->addEdge (info->scc_idx[e->src], info->scc_idx[e->dst]);
       }
     }
   }
 }
 return ret;
}

void AGSCCInfo::save (FILE *fp)
{
 fprintf (fp, "%d %d\n", n, scc_num);
 for (int i=0; i < n; i++) {
   fprintf (fp, "%d ", scc_idx[i]);
   if (((i + 1) & 0xf) == 0) {
     fprintf (fp, "\n");
   }
 }
 fprintf (fp, "\n");
}

AGinfo *AGSCCInfo::restore (FILE *fp)
{
 AGSCCInfo *ret = new AGSCCInfo ();
 if (fscanf (fp, "%d %d", &ret->n, &ret->scc_num) != 2) {
   fprintf (stderr, "Error restoring SCCInfo\n");
   ret->n = 0;
   ret->scc_num = 0;
   return ret;
 }
 if (ret->n < 0) {
   fprintf (stderr, "Error restoring SCCInfo\n");
   ret->n = 0;
   ret->scc_num = 0;
   return ret;
 }
 if (ret->n == 0) {
   return ret;
 }
 MALLOC (ret->scc_idx, int, ret->n);
 for (int i=0; i < ret->n; i++) {
   if (fscanf (fp, "%d", &ret->scc_idx[i]) != 1) {
     fprintf (stderr, "Error restoring SCCInfo\n");
     return ret;
   }
 }
 return ret;
}
