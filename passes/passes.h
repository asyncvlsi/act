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
#ifndef __ACT_PASSES_H__
#define __ACT_PASSES_H__

#include <act/act.h>
#include <act/passes/aflat.h>
#include <act/passes/booleanize.h>
#include <act/passes/netlist.h>
#include <act/passes/cells.h>
#include <act/passes/statepass.h>
#include <act/passes/sizing.h>
#include <act/passes/finline.h>
#include <act/passes/chpdecomp.h>

/**

  @mainpage The Standard ACT Pass Library

  The standard ActPass library contains a collection of pre-defined
  passes that are used by many of the standard ACT tools. Each pass
  has an associated class name and a string name. These passes are
  combined into the ACT pass library (libactpass.a or
  libactpass_sh.so) and installed with the ACT repository.

  The standard passes are:

  1. ActBooleanizePass (name: "booleanize")
  2. ActSizingPass (name: "sizing")
  3. ActNetlistPass (name: "prs2net")
  4. ActCellPass (name: "prs2cells")
  5. ActStatePass (name: "collect_state")
  6. ActCHPFuncInline (name: "finline")
  7. ActCHPMemory (name: "chpmem")
  8. ActCHPArbiter (name: "chparb")

*/
 
#endif /* __ACT_PASSES_H__ */
