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

AGraph::AGraph(void *_info)
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

void *AGraph::getInfo ()
{
  return info;
}

int AGraph::addVertex (void *info)
{
  AGvertex *v;
  
  A_NEW (vertices, AGvertex);
  v = &A_NEXT (vertices);
  A_INC (vertices);

  v->ehd = -1;
  v->info = info;
  v->vid = A_LEN (vertices)-1;

  return v->vid;
}

int AGraph::addInput (void *info)
{
  int v = addVertex (info);
  A_NEW (inp, int);
  A_NEXT (inp) = v;
  A_INC (inp);
  return v;
}

int AGraph::addOutput (void *info)
{
  int v = addVertex (info);
  A_NEW (outp, int);
  A_NEXT (outp) = v;
  A_INC (outp);
  return v;
}

int AGraph::addEdge (int src, int dst, void *info)
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
 return &vertices[i];
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
