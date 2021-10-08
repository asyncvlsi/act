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

struct idmap {
  A_DECL (ActId *, ids)
  int nout;
  int nat;
};


class ActCellPass : public ActPass {
public:
  ActCellPass (Act *a);
  ~ActCellPass ();

  int run (Process *p = NULL);

  void Print (FILE *fp);

  int numCellMax () { return cell_count-1; }
  Process *getCell(int i);

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

  /*-- private functions --*/
  void add_new_cell (struct act_prsinfo *pi);
  void add_passgates ();
  struct act_prsinfo *_gen_prs_attributes (act_prs_lang_t *prs, int ninp = -1,
					   int noutp = -1);
  void dump_celldb (FILE *);
  Expr *_idexpr (int idx, struct act_prsinfo *pi);
  ActBody_Conn *_build_connections (const char *name,
				    struct act_prsinfo *pi);
  ActBody_Conn *_build_connections (const char *name,
				    act_prs_lang_t *gate);
  
  void _collect_one_prs (Scope *sc, act_prs_lang_t *prs);
  void _collect_one_passgate (Scope *sc, act_prs_lang_t *prs);
  void collect_gates (Scope *sc, act_prs_lang_t **pprs);
  void prs_to_cells (Process *p);
  int _collect_cells (ActNamespace *cells);
  void flush_pending (Scope *sc);
};


#endif /* __ACT_PASS_CELL_H__ */
