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
#ifndef __AFLAT_H__
#define __AFLAT_H__

#include "act/lang/act.h"

class ActApplyPass : public ActPass {
 public:
  ActApplyPass (Act *a);
  ~ActApplyPass ();

  int run (Process *p = NULL);

  void run_per_type (Process *p = NULL);

  void setCookie (void *);
  void setInstFn (void (*f) (void *, ActId *, UserDef *));
  void setConnPairFn (void (*f) (void *, ActId *, ActId *));

  void setProcFn (void (*f) (void *, Process *));
  void setChannelFn (void (*f) (void *, Channel *));
  void setDataFn (void (*f) (void *, Data *));

  void printns (FILE *fp);

 private:
  int init ();

  void *local_op (Process *p, int mode);
  void *local_op (Channel *c, int mode);
  void *local_op (Data *d, int mode);
  

  void (*apply_per_proc_fn) (void *, Process *);
  void (*apply_per_channel_fn) (void *, Channel *);
  void (*apply_per_data_fn) (void *, Data *);

  
  void (*apply_user_fn) (void *, ActId *, UserDef *);
  void (*apply_conn_fn) (void *, ActId *, ActId *);
  void *cookie;
  
  list_t *prefixes;
  list_t *prefix_array;
  list_t *suffixes;
  list_t *suffix_array;

  /*-- private functions --*/
  void push_namespace_name (const char *);
  
  void push_name (const char *, Array *arr = NULL);
  void pop_name ();
    
  void push_name_suffix (const char *, Array *arr = NULL);
  void pop_name_suffix ();

  void _flat_connections_bool (ValueIdx *vx);
  
  void _flat_single_connection (ActId *one, Array *oa,
				ActId *two, Array *ta,
				const char *nm, Arraystep *na,
				ActNamespace *isoneglobal);

  void _flat_rec_bool_conns (ActId *one, ActId *two, UserDef *ux,
			     Array *oa, Array *ta,
			     ActNamespace *isoneglobal);
  void _any_global_conns (act_connection *c);
  void _flat_scope (Scope *);
  void _flat_ns (ActNamespace *);

};

#endif /* __AFLAT_H__ */
