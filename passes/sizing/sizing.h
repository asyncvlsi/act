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
#ifndef __ACT_PASS_SIZING_H__
#define __ACT_PASS_SIZING_H__

#include <map>
#include <act/act.h>
#include <act/iter.h>
#include <act/passes/booleanize.h>
#include <common/hash.h>
#include <common/array.h>

class ActSizingPass : public ActPass {
public:
  ActSizingPass (Act *a);
  ~ActSizingPass ();

  int run (Process *p = NULL);

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  ActBooleanizePass *bp;
};


#endif /* __ACT_PASS_STATE_H__ */
