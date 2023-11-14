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
#include <common/array.h>
#include <common/list.h>

class AGinfo {
public:
  virtual ~AGinfo() { }
  virtual const char *info() { return ""; }
  virtual void save(FILE *fp) { }
  virtual AGinfo *restore(FILE *fp) { return new AGinfo(); }
};

class AGSCCInfo : public AGinfo {
public:
  AGSCCInfo() { n = 0; scc_idx = NULL; scc_num = 0; }
  ~AGSCCInfo() { if (n > 0) { FREE (scc_idx); } }
  void save(FILE *fp);
  AGinfo *restore(FILE *fp);

  int getCompId (int v) {
    Assert (v >= 0 && v < n, "Vertex index out of range");
    return scc_idx[v];
  }
  int getNumComp() { return scc_num; }
  
private:
  int scc_num;
  int n;
  int *scc_idx;

  friend class AGraph;
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
  unsigned int visited:1;	// visited flag, if needed
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

  void mkInput (int v);
  void mkOutput (int v);

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

  /* Save/restore graph to/from a file */
  void save (FILE *fp);
  void restore (FILE *fp);

  /* Use these to set up a dummy vertexinfo/edgeinfo pointer; this is
     needed to get the correct virtual functions for restoring info
     pointers when reading a saved graph from a file.
  */
  void setVtxInfo (AGinfo *i) { _vtx_info = i; }
  void setEdgeInfo (AGinfo *i) { _edge_info = i; }

  /* 
     Returns a DAG where each vertex is a strongly connected component.
     The "info" field of the newly retured graph is an AGSCCInfo, and 
     contains a map between vertices in the original graph and the SCC
     component #
     
     The component # is the vertex index in the newly returned graph.
  */
  AGraph *computeSCC ();


  /*
   * Run depth-first search algorithm, and return a list of
   * integer-valued vertex indicies that are roots of the DFS forest
   *
   * If "fn_edge" is not NULL, then this function is applied once per
   * edge during DFS traversal. The cookie is passed to the function
   * "fn".  Each vertex has a "visited" flag; "fn" is called before
   * setting the visited flag (note: the visited flag may be already
   * set via some other previous path during DFS traversal.
   *
   * If "fn_node" is not NULL, then this function is called twice:
   * once on entry (the bool is set to true), and once before
   * returning (bool is set to false).
   */
  list_t *runDFS (void *cookie,
		  void (*fn_node) (void *, AGvertex *, bool),
		  void (*fn_edge) (void *, AGedge *));

 private:
  AGinfo *info;
  A_DECL (AGedge, edges);
  A_DECL (AGvertex, vertices);
  A_DECL (int, inp);		// input vertices (index)
  A_DECL (int, outp);		// output vertices (index)

  AGinfo *_vtx_info;		// dummy fields
  AGinfo *_edge_info;		// dummy fields

  void _mark_reverse (int, AGSCCInfo *);
  void _compute_scc_helper (int);
  void (*_dfs_apply_edge) (void *cookie, AGedge *e);
  void (*_dfs_apply_node) (void *cookie, AGvertex *v, bool onentry);
  void *_dfs_cookie;
};



/*-- iterators --*/

class AGraphVertexIter
#if __cplusplus < 201703L
/* deprecated in C++17 onward */
  : public std::iterator<std::input_iterator_tag, AGvertex *>
#endif
{
  AGraph *g;
  int i;

 public:
#if __cplusplus >= 201703L  
  using iterator_category = std::input_iterator_tag;
  using value_type = AGvertex *;
#endif  
  
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

class AGraphEdgeIter 
#if __cplusplus < 201703L
  : public std::iterator<std::input_iterator_tag, AGedge *>
#endif
{
  AGraph *g;
  int i;

 public:
#if __cplusplus >= 201703L  
  using iterator_category = std::input_iterator_tag;
  using value_type = AGedge *;
#endif  
  
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

class AGvertexFwdIter
#if __cplusplus < 201703L
  : public std::iterator<std::input_iterator_tag, AGedge *>
#endif
{
  AGraph *g;
  int vid;
  int i;

 public:
#if __cplusplus >= 201703L  
  using iterator_category = std::input_iterator_tag;
  using value_type = AGedge *;
#endif  
  
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

class AGvertexBwdIter
#if __cplusplus < 201703L
  : public std::iterator<std::input_iterator_tag, AGedge *>
#endif
{
  AGraph *g;
  int vid;
  int i;

 public:
#if __cplusplus >= 201703L  
  using iterator_category = std::input_iterator_tag;
  using value_type = AGedge *;
#endif  
  
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


class AGraphInpVertexIter
#if __cplusplus < 201703L
  : public std::iterator<std::input_iterator_tag, AGvertex *>
#endif
{
  AGraph *g;
  int i;

 public:
#if __cplusplus >= 201703L  
  using iterator_category = std::input_iterator_tag;
  using value_type = AGvertex *;
#endif  
  
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

class AGraphOutVertexIter
#if __cplusplus < 201703L
  : public std::iterator<std::input_iterator_tag, AGvertex *>
#endif
{
  AGraph *g;
  int i;

 public:
#if __cplusplus >= 201703L  
  using iterator_category = std::input_iterator_tag;
  using value_type = AGvertex *;
#endif  
  
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
