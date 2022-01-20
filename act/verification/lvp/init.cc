#include "act/lang/act.h"

extern "C" void initialize (int *argc, char ***argv)
{
  Act::Init (argc, argv);
}
