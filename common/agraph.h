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
#ifndef __ACT_GRAPH_H__
#define __ACT_GRAPH_H__

#include <iterator>
#include "array.h"

class AGinfo {
public:
  virtual const char *info() { return ""; }
};

struct AGedge {
  int eid;
  int src, dst;
  int enext;		     /* next ptr for all edges for a vertex */
  int eback;
  AGinfo *info;
  AGinfo *getInfo() { return info; }
  void setInfo (AGinfo *x) { info = x; }
};

struct AGvertex {
  int vid;
  int ehd;
  int bhd;			// backward edges
  unsigned int isio:2;		// 0 = not I/O, 1 = inp, 2 = outp
  AGinfo *info;
  int hasFanout () { return ehd != -1; }
  int hasFanin () { return bhd != -1; }
  AGinfo *getInfo() { return info; }
  void setInfo (AGinfo *x) { info = x; }
};

class AGraph {
 public:
  AGraph(AGinfo *info = NULL);
  ~AGraph();
  
  int addInput (AGinfo *info = NULL);
  int addOutput (AGinfo *info = NULL);
  int addVertex(AGinfo *info = NULL);
  int addEdge(int src, int dst, AGinfo *info = NULL);

  int numEdges ();
  int numVertices ();
  int numOutputs ();
  int numInputs ();
  AGedge *getEdge (int i);
  AGvertex *getVertex (int i);
  AGvertex *getInput (int i);
  AGvertex *getOutput (int i);
  int V2idx (AGvertex *v);
  AGinfo *getInfo();

  void printDot (FILE *fp, const char *name);

 private:
  AGinfo *info;
  A_DECL (AGedge, edges);
  A_DECL (AGvertex, vertices);
  A_DECL (int, inp);		// input vertices (index)
  A_DECL (int, outp);		// output vertices (index)
};



/*-- iterators --*/

class AGraphVertexIter : public
      std::iterator<std::input_iterator_tag, AGvertex *> {

  AGraph *g;
  int i;

 public:
  AGraphVertexIter (AGraph *);
  AGraphVertexIter (const AGraphVertexIter & c);
  AGraphVertexIter& operator++();
  AGraphVertexIter operator++(int);
  bool operator==(const AGraphVertexIter& rhs) const;
  bool operator!=(const AGraphVertexIter& rhs) const;
  AGvertex *operator*();
  AGraphVertexIter begin();
  AGraphVertexIter end();
  int pos() { return i; }
};

class AGraphEdgeIter : public
      std::iterator<std::input_iterator_tag, AGedge *> {

  AGraph *g;
  int i;

 public:
  AGraphEdgeIter (AGraph *);
  AGraphEdgeIter (const AGraphEdgeIter & c);
  AGraphEdgeIter& operator++();
  AGraphEdgeIter operator++(int);
  bool operator==(const AGraphEdgeIter& rhs) const;
  bool operator!=(const AGraphEdgeIter& rhs) const;
  AGedge *operator*();
  AGraphEdgeIter begin();
  AGraphEdgeIter end();
  int pos() { return i; }
};

class AGvertexFwdIter : public
      std::iterator<std::input_iterator_tag, AGedge *> {

  AGraph *g;
  int vid;
  int i;

 public:
  AGvertexFwdIter (AGraph *, int);
  AGvertexFwdIter (AGraph *, AGvertex *);
  AGvertexFwdIter (const AGvertexFwdIter & c);
  AGvertexFwdIter& operator++();
  AGvertexFwdIter operator++(int);
  bool operator==(const AGvertexFwdIter& rhs) const;
  bool operator!=(const AGvertexFwdIter& rhs) const;
  AGedge *operator*();
  AGvertexFwdIter begin();
  AGvertexFwdIter end();
  int pos() { return i; }
};

class AGvertexBwdIter : public
      std::iterator<std::input_iterator_tag, AGedge *> {

  AGraph *g;
  int vid;
  int i;

 public:
  AGvertexBwdIter (AGraph *, int);
  AGvertexBwdIter (const AGvertexBwdIter & c);
  AGvertexBwdIter& operator++();
  AGvertexBwdIter operator++(int);
  bool operator==(const AGvertexBwdIter& rhs) const;
  bool operator!=(const AGvertexBwdIter& rhs) const;
  AGedge *operator*();
  AGvertexBwdIter begin();
  AGvertexBwdIter end();
  int pos() { return i; }
};


class AGraphInpVertexIter : public
      std::iterator<std::input_iterator_tag, AGvertex *> {

  AGraph *g;
  int i;

 public:
  AGraphInpVertexIter (AGraph *);
  AGraphInpVertexIter (const AGraphInpVertexIter & c);
  AGraphInpVertexIter& operator++();
  AGraphInpVertexIter operator++(int);
  bool operator==(const AGraphInpVertexIter& rhs) const;
  bool operator!=(const AGraphInpVertexIter& rhs) const;
  AGvertex *operator*();
  AGraphInpVertexIter begin();
  AGraphInpVertexIter end();
  int pos() { return i; }
};

class AGraphOutVertexIter : public
      std::iterator<std::input_iterator_tag, AGvertex *> {

  AGraph *g;
  int i;

 public:
  AGraphOutVertexIter (AGraph *);
  AGraphOutVertexIter (const AGraphOutVertexIter & c);
  AGraphOutVertexIter& operator++();
  AGraphOutVertexIter operator++(int);
  bool operator==(const AGraphOutVertexIter& rhs) const;
  bool operator!=(const AGraphOutVertexIter& rhs) const;
  AGvertex *operator*();
  AGraphOutVertexIter begin();
  AGraphOutVertexIter end();
  int pos() { return i; }
};



#endif /* __ACT_GRAPH_H__ */
