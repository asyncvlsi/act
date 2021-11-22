
add_library(
  ActMiniScheme
lisp.h
lispargs.h
lispArith.c
lispA-Z.c
lispA-Z.h
lispBuiltin.c
lispCli.c
lispCli.h
lispEval.c
lispFrame.c
lispGC.c
lispInt.h
lispIO.c
lispMain.c
lispParse.c
lispPath.c
lispPrint.c
lispString.c
lispTrace.c
  )

add_library(
  ActMiniSchemeCli
lispCli.c
lispCli.h
  )

target_link_libraries(ActMiniScheme ActCommon edit)
set_target_properties(
   ActMiniScheme PROPERTIES PUBLIC_HEADER "lisp.h;lispCli.h;lispInt.h"
)
set_target_properties(ActMiniScheme PROPERTIES OUTPUT_NAME actscm)

install (TARGETS ActMiniScheme
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/act/miniscm
)
