#include <act/act.h>
#include <act/passes.h>
#include <act/passes/statepass.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail (const char *message)
{
  fprintf (stderr, "state refresh regression: %s\n", message);
  exit (1);
}

static bool same_counts (state_counts left, state_counts right)
{
  return left.numAllBools () == right.numAllBools ()
      && left.numAllVars () == right.numAllVars ();
}

int main (int argc, char **argv)
{
  if (argc != 2) {
    fprintf (stderr, "usage: %s <refresh.act>\n", argv[0]);
    return 2;
  }

  Act::Init (&argc, &argv);
  Act *act = new Act (argv[1]);
  act->Expand ();

  Process *top_a = act->findProcess ("top_a<>", true);
  Process *top_b = act->findProcess ("top_b<>", true);
  Process *replacement = act->findProcess ("leaf_b<>", true);
  if (!top_a || !top_b || !replacement) {
    fail ("fixture processes are missing");
  }

  ActStatePass *state = new ActStatePass (act);
  if (!state->run (top_a)) {
    fail ("initial state pass failed");
  }
  stateinfo_t *before = state->getStateInfo (top_a);
  if (!before || state->rootStateInfo () != before) {
    fail ("initial root state map is not the top-level map");
  }

  char instance[] = "u";
  if (!top_a->updateInst (instance, replacement)) {
    fail ("compatible replacement was rejected");
  }

  ActBooleanizePass *booleanize = dynamic_cast<ActBooleanizePass *>(
      act->pass_find ("booleanize"));
  if (!booleanize) {
    fail ("booleanize pass is missing");
  }
  booleanize->invalidateNets ();
  ActPass::refreshAll (act, top_a);

  stateinfo_t *after = state->getStateInfo (top_a);
  if (!after || state->rootStateInfo () != after) {
    fail ("refresh left root state info out of sync with the current map");
  }
  if (!after->bnl || after->bnl->p != top_a) {
    fail ("refreshed root boolean netlist points at the wrong process");
  }
  state_counts refreshed_counts = after->all;
  state_counts refreshed_globals = state->getGlobals ();
  int refreshed_vars = after->all.numAllVars ();
  ActId *id = ActId::parseId ("q1[1]");
  int refreshed_offset = 0;
  int refreshed_type = 0;
  int refreshed_width = 0;
  act_connection *refreshed_connection = id
      ? id->Canonical (top_a->CurScope ()) : NULL;
  if (!id || !state->checkIdExists (id)
      || !refreshed_connection
      || !state->getTypeOffset (after, refreshed_connection,
                                &refreshed_offset, &refreshed_type,
                                &refreshed_width)) {
    fail ("root array element q1[1] does not resolve after refresh");
  }
  delete id;

  if (!booleanize->run (top_b) || !state->run (top_b)) {
    fail ("fresh B state pass failed");
  }
  /* top_b is a separately declared B topology in this Act object. Re-running
     this pass for top_b is a fresh B computation, but not an independently
     constructed Act instance; the root-map invariant and negative control
     cover the refresh itself.
   */
  if (!same_counts (refreshed_counts, state->getStateInfo (top_b)->all)) {
    fail ("refreshed B state counts differ from the fresh top-B computation");
  }
  if (!same_counts (refreshed_globals, state->getGlobals ())) {
    fail ("refreshed global counts differ from the fresh top-B computation");
  }
  ActId *fresh_id = ActId::parseId ("q1[1]");
  act_connection *fresh_connection = fresh_id
      ? fresh_id->Canonical (top_b->CurScope ()) : NULL;
  int fresh_offset = 0;
  int fresh_type = 0;
  int fresh_width = 0;
  if (!fresh_id || !fresh_connection
      || !state->getTypeOffset (state->getStateInfo (top_b), fresh_connection,
                                &fresh_offset, &fresh_type, &fresh_width)
      || refreshed_offset != fresh_offset
      || refreshed_type != fresh_type
      || refreshed_width != fresh_width) {
    fail ("refreshed q1[1] offset/type differs from the fresh top-B computation");
  }
  delete fresh_id;

  printf ("state refresh regression: PASS q1[1]=1 vars=%d globals=%d\n",
          refreshed_vars, refreshed_globals.numAllVars ());
  return 0;
}
