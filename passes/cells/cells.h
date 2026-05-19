/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#ifndef __ACT_PASS_CELL_H__
#define __ACT_PASS_CELL_H__

#include <set>
#include <map>
#include <act/act.h>
#include <act/iter.h>
#include <common/hash.h>
#include <common/bitset.h>
#include <common/array.h>

struct act_prsinfo;

/*
 * This data structure is used to hold all the output IDs for a cell.
 * In addition, it holds the label names. However, label names are
 * always after outputs.
 *   ids [0 .. nout - 1] : output variables
 *   ids [nout ... nout + nat - 1] : label names, but actually stored
 *                                   as strings (not ActId pointers)
 * 
 *   ids [nout + nat - 1 ... end] : input variables
 */
class idmap {
private:
  A_DECL (ActId *, ids);
  int nout;
  int nat;

public:
  
  idmap() {
    A_INIT (ids);
    nout = -1;
    nat = 0;
  }
  ~idmap() {
    A_FREE (ids);
  }
  void clear() {
    A_FREE (ids);
    A_INIT (ids);
    nout = -1;
    nat = 0;
  }
  void moved() {
    A_INIT (ids);
  }

  void finalize_outs () {
    nout = A_LEN (ids);
  }

  int num_outputs () {
    return nout;
  }

  int find_idx (ActId *id) {
    for (int i=0; i < A_LEN (ids); i++) {
      if (ids[i] == id) return i;
      if (ids[i]->isEqual (id)) return i;
    }
    return -1;
  }

  int num_ids () {
    return A_LEN (ids);
  }

  int find_label_idx (ActId *id) {
    for (int i=0; i < A_LEN (ids); i++) {
      if (ids[i] == id) return i;
    }
    return -1;
  }
  
  int alloc_new_id (ActId *id) {
    A_NEW (ids, ActId *);
    A_NEXT (ids) = id;
    A_INC (ids);
    return A_LEN(ids)-1;
  }
  
  int alloc_new_atid (ActId *id) {
    int i = alloc_new_id (id);
    nat++;
    return i;
  }

  /*
   * Once the outputs have been set, this can be used to find or
   * allocate fresh IDs and labels.
   */
  int find_or_alloc (ActId *id, int islabel = 0) {
    int i;
    Assert (nout >= 0, "Only call find_or_alloc after outputs have been set!");
    for (i=0; i < A_LEN (ids); i++) {
      if (ids[i] == id) return i;
      if (!islabel && (i < nout || i >= nout + nat)) {
	if (ids[i]->isEqual (id)) return i;
      }
    }
    Assert (!islabel || (A_LEN (ids) != nout + nat), "find_or_alloc() error");
    if (islabel) {
      return alloc_new_atid (id);
    }
    else {
      return alloc_new_id (id);
    }
  }

  ActId *getId (int idx) {
    return ids[idx];
  }

};


class ActCellPass : public ActPass {
public:
  ActCellPass (Act *a);
  ~ActCellPass ();

  int run (Process *p = NULL);

  void Print (FILE *fp);

  list_t *getNewCells () { return _new_cells; }
  list_t *getUsedCells() { return _used_cells; }

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  /*-- private data structures --*/
  struct cHashtable *cell_table;
  ActNamespace *cell_ns;
  int proc_inst_count;
  int cell_count;
  struct idmap current_idmap;
  int _leak_flag;

  list_t *_new_cells;
  list_t *_used_cells;

  const char *_inport_name;
  const char *_outport_name;

  struct pending_group {
    act_prs_lang_t *tree;
    std::vector<act_prs_lang_t *> _pending;

    pending_group() {
      tree = NULL;
      _pending.clear ();
    }
  };

  /*-- cell extraction, temporary data structures --*/

  // _pending_prs : production rules we have collected so far without
  // a matching rule to complete the gate
  std::vector<act_prs_lang_t *> _pending_prs;

  // _pending_group: pending tree + other rules
  std::vector<pending_group> _pending_group;

  /*-- private functions --*/
  void add_new_cell (struct act_prsinfo *pi);
  void add_passgates_cap ();
  struct act_prsinfo *_gen_prs_attributes (act_prs_lang_t *prs,
					   int ninp = -1,
					   int noutp = -1);
  void dump_celldb (FILE *);
  Expr *_idexpr (int idx, struct act_prsinfo *pi);
  ActBody_Conn *_build_connections (const char *name,
				    struct act_prsinfo *pi);
  ActBody_Conn *_build_connections (const char *name,
				    act_prs_lang_t *gate);
  
  bool _collect_one_prs (Scope *sc, act_prs_lang_t *prs);
  void _collect_one_passgate (Scope *sc, act_prs_lang_t *prs);
  void _collect_one_cap (Scope *sc, act_prs_lang_t *prs);
  void collect_gates (Scope *sc, act_prs_lang_t **pprs);
  void prs_to_cells (Process *p);
  int _collect_cells (ActNamespace *cells);
  void flush_pending (Scope *sc);

  void _create_new_cell (Scope *sc, act_prs_lang_t *prslist);
  

  /* set the bit for all the @ variables used in "e" */
  void _mark_at_used (act_prs_expr_t *e, bitset_t *b,
		      int *idx_array, int len);
  void _mark_at_used2 (act_prs_expr_t *e, bitset_t *b,
		       act_prs_lang_t **rules, int len);
};


#endif /* __ACT_PASS_CELL_H__ */
