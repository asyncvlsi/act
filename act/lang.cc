/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <act/types.h>
#include <act/lang.h>
#include <act/inst.h>



act_prs *prs_expand (act_prs *p, ActNamespace *ns, Scope *s)
{
  /* s->u must exist */
  return p;
}

act_prs_lang_t *prs_expand (act_prs_lang_t *p, ActNamespace *ns, Scope *s)
{
  return p;
}

act_chp *chp_expand (act_chp *c, ActNamespace *ns, Scope *s)
{
  /* s->u must exist */
  return c;
}

act_chp_lang_t *chp_expand (act_chp_lang_t *c, ActNamespace *ns, Scope *s)
{
  /* s->u must exist */
  return c;
}



