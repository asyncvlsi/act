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
#include "act/common/agraph.h"

AGraph::AGraph(AGinfo *_info)
{
  A_INIT (edges);
  A_INIT (vertices);
  A_INIT (inp);
  A_INIT (outp);
  info = _info;
}

AGraph::~AGraph()
{
  A_FREE (edges);
  A_FREE (vertices);
  A_FREE (inp);
  A_FREE (outp);
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
