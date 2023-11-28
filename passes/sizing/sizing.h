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

  int run (Process *p = NULL);

  /*------------------------------------------------------------------------
   * Various utilities in the sizing pass that are exported for
   * external use
   *------------------------------------------------------------------------
   */

  /*
   * This is used to determine if a production rule expression
   * contains any sizing information. If it does, then it is viewed as
   * a user-specified fixed size rule that is not impacted by any gate sizing
   * requests.
   *
   * @param e is the production rule expression to be checked
   * @return 1 if the expression does not include any explicit sizing,
   * 0 otherwise
   */
  static int no_sizing (act_prs_expr_t *e);

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  ActBooleanizePass *bp;
};


#endif /* __ACT_PASS_STATE_H__ */
