/*
 * Regression for compatible process re-elaboration.
 *
 * Process::updateInst documents one requirement: the replacement must have the
 * same port list as the type it replaces. It additionally refused anything that
 * was not a cell, which is not part of that contract and blocks replacing a
 * composite process with a compatible one. These checks pin the documented
 * behaviour in both directions -- a compatible composite is accepted, and an
 * incompatible port list is still refused.
 *
 * ActBooleanizePass::createNets caches its work, so a second call returns
 * immediately; invalidateNets drops that cache. The check here runs on a design
 * that has not been edited, which is deliberate: after an updateInst the
 * replacement type has no booleanized map yet, so invalidating and rebuilding
 * without first refreshing the pass walks into the Assert in _createNets.
 * Refreshing passes across an edit is a separate concern, covered by the
 * state-refresh regression.
 */
#include <act/act.h>
#include <act/passes.h>
#include <stdio.h>
#include <stdlib.h>

static void fail (const char *message)
{
  fprintf (stderr, "re-elaboration regression: %s\n", message);
  exit (1);
}

static int net_count (ActBooleanizePass *pass, Process *p)
{
  act_boolean_netlist_t *netlist = pass->getBNL (p);
  if (!netlist) {
    return -1;
  }
  return A_LEN (netlist->nets);
}

int main (int argc, char **argv)
{
  if (argc != 2) {
    fprintf (stderr, "usage: %s <reelab.act>\n", argv[0]);
    return 2;
  }

  Act::Init (&argc, &argv);
  Act *act = new Act (argv[1]);
  act->Expand ();

  Process *holder = act->findProcess ("holder<>", true);
  Process *leaf_one = act->findProcess ("leaf_one<>", true);
  Process *leaf_two = act->findProcess ("leaf_two<>", true);
  Process *leaf_wide = act->findProcess ("leaf_wide<>", true);
  if (!holder || !leaf_one || !leaf_two || !leaf_wide) {
    fail ("fixture processes are missing");
  }

  /* The whole point of the fixture: none of these are cells. */
  if (leaf_one->isCell () || leaf_two->isCell () || leaf_wide->isCell ()) {
    fail ("fixture leaves must be composite processes, not cells");
  }

  ActBooleanizePass *booleanize = new ActBooleanizePass (act);
  if (!booleanize->run (holder)) {
    fail ("initial booleanize pass failed");
  }
  booleanize->createNets (holder);

  const int nets_before = net_count (booleanize, holder);
  if (nets_before <= 0) {
    fail ("the fixture produced no nets to begin with");
  }

  /* Dropping the cache must let the next createNets rebuild the same answer. */
  booleanize->invalidateNets ();
  booleanize->createNets (holder);
  if (net_count (booleanize, holder) != nets_before) {
    fail ("rebuilding nets after invalidateNets changed the connectivity");
  }

  /* A composite replacement with an identical port list must be accepted. */
  char instance[] = "u";
  if (!holder->updateInst (instance, leaf_two)) {
    fail ("compatible composite replacement was rejected");
  }

  /* A different port list must still be refused: the edit contract is unchanged. */
  char wide_instance[] = "u";
  if (holder->updateInst (wide_instance, leaf_wide)) {
    fail ("replacement with a different port list was accepted");
  }

  printf ("re-elaboration regression passed\n");
  return 0;
}
