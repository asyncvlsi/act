/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2020 Manohar
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
#ifndef __ACT_PASS_CHPMEM_H__
#define __ACT_PASS_CHPMEM_H__

#include <act/act.h>
#include <act/passes/booleanize.h>

class ActCHPMemory : public ActPass {
public:
  ActCHPMemory (Act *a);
  int run (Process *p = NULL);

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  void _extract_memory (act_chp_lang_t *c);

  int _fresh_memdata (Scope *sc, int bw);
  void _fresh_release (int idx);

  void _subst_dynamic_array (list_t *l, Expr *e);


  int _is_dynamic_array (ActId *id);

  ActBooleanizePass *_bp;

  struct memvar_info {
    int bw;
    int idx;
    int used;
  };

  int _memdata_len;
  struct memvar_info *_memdata_var;
  act_boolean_netlist_t *_curbnl;
};

#endif /* __ACT_PASS_CHPMEM_H__ */
