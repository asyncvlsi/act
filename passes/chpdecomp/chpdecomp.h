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

    void dump (FILE *fp) {
      fprintf (fp, "[mem=");
      ref->Print (fp);
      fprintf (fp, " | ");
      var->Print (fp);
      fprintf (fp, " | idx=%d]", fresh_idx);
    }
      

    valid_read_ref() { ref = NULL; var = NULL; fresh_idx = -1; used = NULL; }
    ~valid_read_ref() {
      if (ref) { delete ref; }
      if (var) { delete var; }
      if (used) { list_free (used); }
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

  ActBooleanizePass *_bp;

  /*
   * Information about new variables used for memory references
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
    
    
    valid_read_ref *ref;


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

    memvar_info(Data *_str, int _bw, int _idx) {
      isstruct = _str;
      bw = _bw;
      idx = _idx;
      used = 1;
      ref = NULL;
    }

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
      Marks the variable used in preparation for recycling;
      this also clears the variable ref it held.
     */
    void mark_used() {
      used = 1;
      if (ref) {
	delete ref;
      }
      ref = NULL;
    }

    memvar_info() {
      isstruct = NULL;
      bw = -1;
      idx = -1;
      used = 0;
      ref = NULL;
    }
  };

  struct memvar_map {
    std::vector<ValueIdx *> newvars;
    std::vector<std::vector<memvar_info>> v;

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
	  }
	}
      }
    }

    ActId *find_cached (ActId *ref, int *retval = NULL) {
      ActId *tail = ref->Rest();
      ref->prune ();

      listitem_t *li;
      for (int j=v.size()-1; j >= 0; j--) {
	auto &last = v[j];
	for (int i=0; i < last.size(); i++) {
	  if (last[i].ref && last[i].ref->ref->isEqual (ref)) {
	    if (tail) {
	      ref->Append (tail);
	    }
	    if (!last[i].used && (j == v.size()-1)) {
	      last[i].used = 2; // cached value is used
	    }
	    if (retval) {
	      *retval = i;
	    }
	    return last[i].ref->var;
	  }
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
