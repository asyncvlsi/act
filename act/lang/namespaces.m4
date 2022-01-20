/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2012, 2018-2019 Rajit Manohar
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
    s = act_path_open (tmp);
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

    if ($0->emit_depend) {
      printf ("%s ", s);
    }

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
      ns = $0->global;
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

    MALLOC (tmp, char, strlen(s)+14);
    sprintf (tmp, "%s/_all_.act", s);

    char *t = act_path_open (tmp);
    FILE *tfp = fopen (t, "r");
    FREE (t);
    if (tfp) {
      fclose (tfp);
      sprintf (tmp, "%s/_all_.act", s-1);
    }
    else {
      sprintf (tmp, "%s.act", s-1);
    }
    strcat (tmp, "\"");
    s--;
    FREE (s);

    apply_X_import_item_opt0 ($0, tmp);
    FREE (tmp);

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
| "import" ID "=>" ID ";"
{{X:
    ActNamespace *ns;
    ActNamespace *newns;

    ns = $0->curns->findNS ($2);
    if (!ns) {
      $E("Namespace `%s' not found.", $2);
    }
    newns = $0->curns->findNS ($4);
    if (!newns) {
      newns = new ActNamespace ($0->curns, $4);
    }
    ns->MkExported();
    ns->Unlink ();
    ns->Link (newns, $2);
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
namespace_management[ActNamespace *]: [ "export" ] "namespace" ID 
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
      //$0->curns->AppendBody (new ActBody_Namespace (ns));
    }
    
    $0->curns = ns;
    $0->scope = ns->CurScope ();
    OPT_FREE ($1);
}}
"{" [ ns_body ] "}"
{{X:
    ActNamespace *ret = $0->curns;
    if (!OPT_EMPTY ($5)) {
      ActRet *r;
      r = OPT_VALUE ($5);
      $A(r->type == R_ACT_BODY);
      $0->curns->AppendBody (r->u.body);
      FREE (r);
    }
    $0->curns = $0->curns->Parent();
    $A($0->curns);
    $0->scope = $0->curns->CurScope();
    OPT_FREE ($5);
    return ret;
}}
;

/*
  qualified_ns is an optionally scoped root identifier that
  corresponds to the name of a namespace.
*/
qualified_ns[ActNamespace *]: [ "::" ] { ID "::" }*
{{X:
    listitem_t *li, *ni;
    ActNamespace *ns, *tmp;
    list_t *tmpns;

    if (OPT_EXISTS ($1)) {
      ns = $0->global;
    }
    else {
      ns = $0->curns;
    }
    li = list_first ($2);
    $A(li);

    tmp = $0->os->find (ns, (char *)list_value (li));
    if (!tmp) {
      $E("Could not find namespace ``%s'' in ``%s''", 
	 (char *) list_value (li), ns->Name());
    }
    if (OPT_EXISTS ($1)) {
      tmpns = list_new ();
      list_append (tmpns, tmp);
    }
    else {
      tmpns = $0->os->findAll (ns, (char *) list_value (li));
      tmp = (ActNamespace *) list_value (list_first (tmpns));
    }
    $A(tmpns);
    OPT_FREE ($1);

    char *nmerr = NULL;
    ActNamespace *nserr = NULL;
    for (ni = list_first (tmpns); ni; ni = list_next (ni)) {
      ns = (ActNamespace *) list_value (ni);
      li = list_first ($2);
      for (li = list_next (li); li; li = list_next (li)) {
	if (!(tmp = ns->findNS ((char *)list_value (li)))) {
	  nmerr = (char *) list_value (li);
	  nserr = ns;
	  break;
	}
	ns = tmp;
      }
      if (!li) {
	break;
      }
    }
    if (!ni) {
      $A(nmerr && nserr);
      $E("Could not find namespace ``%s'' in ``%s''", nmerr, nserr->Name());
    }
    list_free ($2);
    list_free (tmpns);
    return ns;
}}
;
