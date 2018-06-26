/*************************************************************************
 *
 *  Copyright (c) 2012-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */

/*------------------------------------------------------------------------
 *
 *  
 *   Namespace management
 *
 *
 *------------------------------------------------------------------------
 */

import_item: "import" STRING ";"
{{X:
    act_Token *t;
    char *tmp;
    char *s;

    MALLOC (tmp, char, strlen ($2)-1);
    strncpy (tmp, $2+1, strlen($2)-2);
    tmp[strlen ($2)-2] = '\0';
    s = path_open (tmp);
    FREE (tmp);
    
    /* Check if this is a recursive call within another import of the
       same file. */
    if (act_pending_import (s)) {
      $e("Your file has a recursive import of file ``%s''\n", s);
      act_print_import_stack (stderr);
      exit (1);
    }

    /* If the file has already been imported in the past, then we can
       ignore this import statement */
    if (act_isimported (s)) {
      FREE (s);
      return NULL;
    }

    /* Record the fact that we are in the middle of importing this
       file */
    act_push_import (s);

    /* Process the new file */
    TRY {
      t = act_parse (s);
      act_walk_X ($0, t);
      act_parse_free (t);
    } CATCH {
      EXCEPT_SWITCH {
      default:
	$W("while processing ``import'' on file ``%s''\n", s);
	FREE (s);
	except_throw (except_type (), except_arg ());
	break;
      }
    }

    /* Mark the file as imported, but no longer open */
    act_pop_import (s);
    FREE (s);

    return NULL;
}}
| "import" [ "::" ] { ID "::" }* [ "->" ID ] ";"
{{X:
    char *s, *tmp;
    int len, len2;
    listitem_t *li;
    ActNamespace *ns, *tmpns;

    if (OPT_EMPTY ($2)) {
      ns = $0->curns;
    }
    else {
      ns = ActNamespace::Global();
    }

    tmpns = ns;
    len = 0;
    while (tmpns->Parent()) {
      len = len + strlen (tmpns->Name()) + 1;
      tmpns = tmpns->Parent ();
    }

    len2 = len + 4;

    for (li = list_first ($3); li; li = list_next (li)) {
      s = (char *) list_value (li);
      len2 = len2 + strlen (s) + 1;
    }

    MALLOC (s, char, len2+2);
    s[0] = '"';
    s++;
    s[len] = '\0';

    /* there is stuff from the current namespace */
    if (len > 0) {
      tmpns = ns;
      while (tmpns->Parent ()) {
	s[len-1] = '/';
	len--;
	len2 = strlen (tmpns->Name ());
	strncpy (&s[len-len2], tmpns->Name(), len2);
	len -= len2;
      }
    }
    
    for (li = list_first ($3); li; li = list_next (li)) {
      strcat (s, (char *)list_value (li));
      if (list_next (li)) {
	strcat (s, "/");
      }
    }
    strcat (s, ".act\"");
    s--;

    apply_X_import_item_opt0 ($0, s);
    FREE (s);

    ns = apply_X_qualified_ns_opt0 ($0, $2, $3);

    if (!OPT_EMPTY ($4)) {
      /* done */
      ActRet *r;
      r = OPT_VALUE ($4);
      $A(r->type == R_STRING);
      apply_X_open_item_opt0 ($0, ns, r->u.str);
      FREE (r);
    }
    OPT_FREE ($4);
    return NULL;
}}
;

/*------------------------------------------------------------------------
 *
 *  open namespace ;
 *
 *  open namespace -> id ;
 *
 *------------------------------------------------------------------------
 */
open_item: "open" qualified_ns "->" ID  ";"
{{X:
    /* Open the namespace and rename it with the specified identifer */
    if (!$0->os->Open ($2, $4)) {
      $E("Cannot rename namespace ``%s'' as ``::%s''---``::%s'' already exists",
	 $2->Name(), $4, $4);
    }
    return NULL;
}}
| "open" qualified_ns ";"
{{X:
    /* Add the namespace to the search path, along with access
       permissions for types */
    if (!$0->os->Open ($2)) {
      $E("Failed to open namespace ``%s''", $2->Name ());
    }
    return NULL;
}}
;


/*------------------------------------------------------------------------
 *
 *
 *  namespace foo { ... }
 *
 *
 *------------------------------------------------------------------------
 */
namespace_management: [ "export" ] "namespace" ID 
{{X:
    ActNamespace *ns;
    int new_ns;

    new_ns = 0;
    if ((ns = $0->curns->findNS ($3))) {
      if (OPT_EXISTS ($1)) {
	if (!ns->isExported()) {
	  $E("Inconsistent ``export'': all instances of the same namespace must either be\nexported, or not exported.");
	}
      }
      else if (ns->isExported()) {
	  $E("Inconsistent ``export'': all instances of the same namespace must either be\nexported, or not exported.");
      }
    }
    else {
      ns = new ActNamespace($0->curns, $3);
      if (OPT_EXISTS ($1)) {
	ns->MkExported ();
      }
      new_ns = 1;
    }
    if (new_ns) {
      /* append something to the body! */
      $0->curns->AppendBody (new ActBody_Namespace (ns));
    }
    
    $0->curns = ns;
    $0->scope = ns->CurScope ();
    OPT_FREE ($1);
}}
"{" [ ns_body ] "}"
{{X:
    if (!OPT_EMPTY ($5)) {
      ActRet *r;
      r = OPT_VALUE ($5);
      $A(r->type == R_ACT_BODY);
      $0->curns->AppendBody (r->u.body);
      FREE (r);
    }
    $0->curns = $0->curns->Parent();
    OPT_FREE ($5);
    return NULL;
}}
;

/*
  qualified_ns is an optionally scoped root identifier that
  corresponds to the name of a namespace.
*/
qualified_ns[ActNamespace *]: [ "::" ] { ID "::" }*
{{X:
    listitem_t *li;
    ActNamespace *ns, *tmp;

    if (OPT_EXISTS ($1)) {
      ns = ActNamespace::Global();
    }
    else {
      ns = $0->curns;
    }
    OPT_FREE ($1);
    li = list_first ($2);
    $A(li);
    tmp = $0->os->find (ns, (char *)list_value (li));
    if (!tmp) {
      $E("Could not find namespace ``%s'' in ``%s''", 
	 (char *) list_value (li), ns->Name());
    }
    ns = tmp;
    for (li = list_next (li); li; li = list_next (li)) {
      if (!(tmp = ns->findNS ((char *)list_value (li)))) {
	$E("Could not find namespace ``%s'' in ``%s''", (char *)list_value (li),
	   ns->Name());
      }
      ns = tmp;
    }
    list_free ($2);
    return ns;
}}
;
