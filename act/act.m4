/*************************************************************************
 *
 *  Copyright (c) 2011-2018 Rajit Manohar
 *  All Rights Reserved
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
      $0->curns->setBody (r->u.body);
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

ns_body[ActBody *]
: ns_body_item ns_body
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
| instance
{{X:
    ActBody_Inst *b;
    if ($0->override) {
      $E("Can't have override types at the top-level in a namespace");
    }
    $A($1);
    b = dynamic_cast <ActBody_Inst*>($1);
    $A(b);
    if (TypeFactory::isProcessType (b->BaseType())) {
      $E("Cannot instantiate processes within a namespace");
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
