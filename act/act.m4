/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011-2019 Rajit Manohar
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

/*------------------------------------------------------------------------
 *
 *  Act top-level pgen file
 *
 *------------------------------------------------------------------------
 */

%type[X] {{ ActTree ActRet }};

/*------------------------------------------------------------------------
 *
 * Grammar
 *
 *------------------------------------------------------------------------
 */
toplevel: [ imports_opens ] [ body ]
{{X:
    OPT_FREE ($1);

    if (!OPT_EMPTY ($2)) {
      ActRet *r;

      r = OPT_VALUE ($2);
      $A(r->type == R_ACT_BODY);
      $0->curns->AppendBody (r->u.body);
      FREE (r);
    }
    OPT_FREE ($2);
    return NULL;
}}
;

imports_opens
: import_open_item imports_opens
| import_open_item
;

body[ActBody *]
: body_item body
{{X:
    if ($1 == NULL) {
      return $2;
    }
    if ($2 == NULL) {
      return $1;
    }
    $1->Append ($2);
    return $1;
}}
| body_item
{{X: return $1; }}
;

ns_body[ActBody *]: ns_body_item ns_body
{{X:
    if ($1 == NULL) {
      return $2;
    }
    if ($2 == NULL) {
      return $1;
    }
    $1->Append ($2);
    return $1;
}}
| ns_body_item
{{X:
    return $1;
}}
;

ns_body_item[ActBody *]
: definition
{{X:
    return NULL;
}}
| namespace_management
{{X:
    return NULL;
}}
| namespace_other
{{X: return $1; }}
;

namespace_other[ActBody *]: instance
{{X:
    ActBody_Inst *b;
    $A($1);
    b = dynamic_cast <ActBody_Inst*>($1);
    $A(b);
    if (TypeFactory::isUserType (b->BaseType())) {
      $E("Cannot instantiate user-defined type within a namespace");
    }
    return $1;
}}
| connection
{{X:
    return $1;
}}
| alias
{{X:
    return $1;
}}
;

body_item[ActBody *]
: namespace_management
{{X: return NULL; }}
| base_item
{{X: return $1; }}
| definition
{{X: return NULL; }}
;

import_open_item: import_item  | open_item ;

/* namespaces */
include(namespaces.m4)

/* types */
include(types.m4)

/* definitions */
include(defs.m4)

/* languages */
include(lang.m4)

/* expressions */
include(expr.m4)
