/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2020 Rajit Manohar
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
#ifndef __ACT_PASS_FINLINE_H__
#define __ACT_PASS_FINLINE_H__

#include <act/act.h>

class ActCHPFuncInline : public ActPass {
public:
  ActCHPFuncInline (Act *a);
  int run (Process *p = NULL);

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  Scope *_cursc;
  act_inline_value _inline_funcs (list_t *, Expr *);
  act_inline_value _inline_funcs_general (list_t *, Expr *);
  
  void _inline_funcs (list_t *, act_chp_lang_t *);
  void _inline_funcs (list_t *, act_dataflow_element *);

  void _full_inline (act_chp_lang_t *c);
  void _do_complex_inline (struct pHashtable *, list_t *, act_chp_lang_t *);
  void _complex_inline_helper (struct pHashtable *, act_chp_lang_t *);

  void _collect_complex_inlines (list_t *, Expr *);
  act_chp_lang_t *_do_inline (struct pHashtable *, list_t *);
  void _apply_complex_inlines (list_t *, Expr *);

  void _structure_assign (act_chp_lang_t *c);

  int _get_fresh_idx (const char *prefix, int *idx);
  int _useidx;
};

#endif /* __ACT_PASS_FINLINE_H__ */
