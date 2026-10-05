/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2022 Rajit Manohar
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
#ifndef __ACT_PASS_CHPDECOMP_H__
#define __ACT_PASS_CHPDECOMP_H__

#include <act/act.h>
#include <act/passes/booleanize.h>

class ActCHPMemory : public ActPass {
public:
  ActCHPMemory (Act *a);
  int run (Process *p = NULL);


  struct valid_read_ref {
    // both ref and var are aliases, so don't need to be free'd
  
    ActId *ref;			/* the memory reference */
    ActId *var;			/* the variable that holds the value */
    int fresh_idx;		/* the idx used to construct the fresh
				   variable */

    list_t *used;		/* the list of IDs involved in the
				   array dereference */
    bool cloned;

    void dump (FILE *fp) {
      fprintf (fp, "[mem=");
      ref->Print (fp);
      fprintf (fp, " | ");
      var->Print (fp);
      fprintf (fp, " | idx=%d]", fresh_idx);
    }
      

    valid_read_ref() { cloned = false; ref = NULL; var = NULL; fresh_idx = -1; used = NULL; }
    ~valid_read_ref() {
      if (cloned) return;
      if (ref) { delete ref; }
      if (var) { delete var; }
      if (used) { list_free (used); }
    }

    valid_read_ref  *clone () {
      valid_read_ref *ret = new valid_read_ref;
      ret->cloned = true;
      ret->used = used;
      ret->ref = ref;
      ret->var = var;
      ret->fresh_idx = fresh_idx;
      return ret;
    }

    bool invalidated (ActId *wr) {
      if (ref->getName() == wr->getName()) return true;
      if (!used) {
	used = act_expr_used_ids (ref->arrayInfo()->getDeref (0));
      }
      for (listitem_t *li = list_first (used); li; li = list_next (li)) {
	ActId *x = (ActId *) list_value (li);
	if (x->getName() == wr->getName()) {
	  return true;
	}
      }
      return false;
    }    
  };
  
private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  void _extract_memory (act_chp_lang_t *c);

  int _fresh_memdata (Scope *sc, int bw, Data *isstruct);

  void _subst_dynamic_array (list_t *l, Expr *e);
  void _subst_dynamic_array_top (list_t *l, Expr *e);

  /* Append to valid read list based on the memory access in id, and
     then return the cached variable.
  */
  ActId *_gen_mem_read (list_t *l, ActId *id);

  int _is_dynamic_array (ActId *id);

  void _append_mem_write (list_t *top, ActId *access, int idx, Expr *e, Scope *sc);
  void _append_mem_read (list_t *top, ActId *access, int idx, Scope *sc);
  int _elemwise_assign (list_t *top, int idx, ActId *field, Data *d, int off, Scope *sc);
  Expr *_elemwise_fieldlist (int idx, ActId *field, Data *d);
  
  int _inv_idx (int idx);

  void _invalidate_stmt_refs (act_chp_lang_t *s);

  ActBooleanizePass *_bp;

  ExprDagVisit *_E;

  /*
   * Information about an individual variable used to hold memory references
   */
  struct memvar_info {
    Data *isstruct;		// if non-NULL, this holds a
				// structure; for structures we have a
				// struct variable and packed int variable

    int bw;			// the bit-width of the integer variable

    int idx;			// the suffix of the variable name
    
    int used;			// 0 = the variable is available for
				// re-cycling; 1 = variable is
				// currently being used in the
				// expression, 2 = variable got used
				// from the cache so should not be recycled.
    
    
    valid_read_ref *ref;	// if this variable holds a valid
				// reference, what is it a reference to?


    // initialize blank entry
    memvar_info() {
      isstruct = NULL;
      bw = -1;
      idx = -1;
      used = 0;
      ref = NULL;
    }

    // standard constructor, and mark this as used
    memvar_info(Data *_str, int _bw, int _idx) {
      isstruct = _str;
      bw = _bw;
      idx = _idx;
      used = 1;
      ref = NULL;
    }

    void dump (FILE *fp) {
      fprintf (fp, "{id%d - ", idx);
      fprintf (fp, "u=%d, bw=%d, str=%c, ", used, bw, isstruct ? 'Y' : 'N');
      if (ref) {
	fprintf (fp, "ref=");
	ref->dump (fp);
      }
      else {
	fprintf (fp, "noref}");
      }
    }

    // check if this is available to hold the specified data
    bool match_and_unused (int inbw, Data *str) {
      if (used) return false;
      if (str) {
	if (isstruct->isEqual (str)) {
	  return true;
	}
	return false;
      }
      else {
	if (bw == inbw) {
	  return true;
	}
	return false;
      }
    }

    /*
      Marks the variable used in preparation for use; this is  called
      when recycling an existing variable identified from
      match_and_unused() above. This also clears the variable ref it held.
     */
    void mark_used() {
      used = 1;
      if (ref) {
	delete ref;
      }
      ref = NULL;
    }

    /*
      Shallow copy, used because we need to keep track of old values
      of this reference when processing selections and parallel composition.
    */
    memvar_info clone() {
      memvar_info m;
      m.isstruct = isstruct;
      m.bw = bw;
      m.idx = idx;
      m.used = used;
      if (ref) {
         m.ref = ref->clone ();
      }
      else {
         m.ref = NULL;
      }
      return m;
    }
  };



  /*
   * Used to track all the variables that hold memory references
   * introduced during memory decomposition
   */
  
  struct memvar_map {
    std::vector<ValueIdx *> newvars; // the variables introduced

    // information about the current memory variables
    //   - last vector is the current sequential/statement fragment
    //   - one before it is the previous one encountered
    // A new vector slot is created on selections and parallel
    // composition
    std::vector<std::vector<memvar_info>> v;

    // for selections, any invalidated reference within a branch must
    // propagate outside after the end of the selection. This keeps
    // track of those invalidations.
    std::vector<std::unordered_set<int>> invals;

    /**
     * Find an unused idx to recycle that matches the data type we
     * need
     * @return -1 if not found, otherwise the index of the variable
     */
    int get_recycled_idx (int bw, Data *isstruct);

    /**
     * @return the max variable suffix integer in the current scope
     */
    int get_max_var_suffix ();

    /**
     * Return the integer suffix of the variable specified by the
     * index.
     * @return the idx field of the memvar structure.
     */
    int idx_to_var_suffix (int idx) {
      return v.back()[idx].idx;
    }

    /**
     * Add a new variable to the current scope
     */
    int add_new_var (memvar_info &&m) {
      v.back().push_back (m);
      return v.back().size()-1;
    }

    /**
     * Indexing routine: returns slot matching the index.
     */
    int find_idx (int idx) {
      auto &last = v.back();
      for (int i=0; i < last.size(); i++) {
	if (last[i].idx == idx) return i;
      }
      return -1;
    }

    /**
     * Mark a variable as available
     */
    void mark_unused (int idx) {
      auto &last = v.back();
  
      for (int i=0; i < last.size(); i++) {
	if (last[i].idx == idx) {
	  last[i].used = 0;
	  return;
	}
      }
    }

    /**
     * Invalidate any valid references that might need the value of
     * the variable wr.
     */
    void invalidate_refs (ActId *wr) {
      auto &last = v.back();
      for (int i=0; i < last.size(); i++) {
	if (last[i].ref) {
	  if (last[i].ref->invalidated (wr)) {
	    delete last[i].ref;
	    last[i].ref = NULL;
	    if (last[i].used) {
	      last[i].used = 0;
	    }
	    // if I am in some select scope, then this entry needs to
	    // be invalidated after the selection
	    if (invals.size() > 0) {
	      if (invals.back().find(i) == invals.back().end()) {
		invals.back().insert(i);
	      }
	    }
	  }
	}
      }
    }

    /**
     * If the actid reference matches something in the cached values,
     * return it and mark the used flag for the slot as 2. This is
     * used in the parallel case to make sure that this slot cannot be
     * recycled.
     */
    ActId *find_cached (ActId *ref, int *retval = NULL) {
      ActId *tail = ref->Rest();
      ref->prune ();

      listitem_t *li;

      auto &last = v.back();
      for (int i=0; i < last.size(); i++) {
	if (last[i].ref && last[i].ref->ref->isEqual (ref)) {
	  if (tail) {
	    ref->Append (tail);
	  }
	  if (!last[i].used) {
	    last[i].used = 2; // cached value is used
	  }
	  if (retval) {
	    *retval = i;
	  }
	  return last[i].ref->var;
	}
      }
      if (tail) {
	ref->Append (tail);
      }
      if (retval) {
	*retval = -1;
      }
      return NULL;
    }

    /* debugging */
    void dump_memrefs (FILE *fp) {
      fprintf (fp, "-- mem refs: ");
      if (v.size() == 0 || (v.size() == 1 && v[0].size() == 0)) {
	fprintf (fp, "none\n");
	return;
      }
      fprintf (fp, "levs: %d\n", (int)v.size());
      for (int i=0; i < v.size(); i++) {
	fprintf (fp, " l%d: ", i);
	for (int j=0; j < v[i].size(); j++) {
	  if (j != 0) {
	    fprintf (fp, " ");
	  }
	  v[i][j].dump (fp);
	}
	fprintf (fp, "\n");
      }
    }

    /*
     * Allow recycling of variables that were re-used from the cache.
     */
    void clear_unused2_flag () {
      auto &last = v.back();
      for (int i=0; i < last.size(); i++) {
	if (last[i].used == 2) {
	  last[i].used = 0;
	}
      }
    }

    /*
     * Nesting of memory variable state
     */
    void push () {
      auto &last = v.back();
      std::vector<memvar_info> copy;
      for (auto i = 0; i < last.size(); i++) {
	copy.push_back (last[i].clone());
      }
      v.push_back(copy);
    }

    /* discard current scope */
    void pop () {
      v.pop_back();
    }
    
    /*
     * Adding flags to be updated at the end of selection
     */
    void push_select() {
      invals.push_back({});
    }

    /*
     * Discard branch scope within a selection statement. If there are
     * any invalidated references, we need to track them for
     * invalidation after all the selection branches have been handled
     */
    void pop_select_branch() {
      auto pos = v.size() - 2;
      auto &last = v.back();
      auto &prev = v[pos];
      for (auto i = 0; i < prev.size(); i++) {
	if (last[i].ref == NULL) {
	  // a reference was invalidated, so we add it to the inval
	  // flag for clearing at the end of processing all the
	  // selection options
	  if (invals.back().find(i) == invals.back().end()) {
	    invals.back().insert(i);
	  }
	}
      }
      pop();
    }

    // invalidate references at the end of the selection
    void pop_select() {
      auto &last = v.back();
      for (int x : invals.back()) {
	if (x < last.size()) {
	  // There may be invalidations of new things, but we don't
	  // care about those.
	  last[x].ref = NULL;
	}
      }
      invals.pop_back();
    }
    
    /*
     * Discard parallel scope. Before doing so, any recycled variable
     * flag must propagate to the outer scope. In addition, any
     * invalidated reference must also propagate to the outer scope.
     */
    void pop_parallel() {
      auto pos = v.size() - 2;
      auto &last = v.back();
      auto &prev = v[pos];
      for (auto i = 0; i < prev.size(); i++) {
	if (last[i].used == 2) {
	  // propagate recycled flag
	  prev[i].used = 2;
	}
	if (last[i].ref == NULL) {
	  prev[i].ref = NULL;
	}
      }
      pop ();
    }

  } _map;

  act_boolean_netlist_t *_curbnl;
};


class ActCHPArbiter : public ActPass {
public:
  ActCHPArbiter (Act *a);
  int run (Process *p = NULL);

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  void _find_potential_arbiters (list_t *l, act_chp_lang_t *c);
  int _fresh_channel (Scope *sc, int bw);
  void _substitute (act_chp_lang_t *c, list_t *l1, list_t *l2);

  ActBooleanizePass *_bp;
  act_boolean_netlist_t *_curbnl;
};


class ActDflowSplitMerge : public ActPass {
public:
  ActDflowSplitMerge (Act *a);
  int run (Process *p = NULL);

private:
  int _split_merge_limit;
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  int _idx;			// running count for tmp channel names
  Scope *_sc;			// hidden arg

  void _apply_recursive_decomp (act_dataflow_element *e, list_t *l);
};


#endif /* __ACT_PASS_CHPDECOMP_H__ */
