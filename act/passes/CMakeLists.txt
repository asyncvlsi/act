
add_library(
  ActPasses
aflat.h
booleanize.cc
booleanize.h
cells.cc
cells.h
emit.cc
finline.cc
finline.h
flatten.cc
netlist.cc
netlist.h
passes.h
sizing.cc
sizing.h
statepass.cc
statepass.h
)
  
target_link_libraries(ActPasses ActLang ActCommon)

set_target_properties(
   ActPasses PROPERTIES PUBLIC_HEADER "aflat.h;booleanize.h;cells.h;finline.h;netlist.h;passes.h;sizing.h;statepass.h"
)
set_target_properties(ActPasses PROPERTIES OUTPUT_NAME actpass)


install (TARGETS ActPasses
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/act/passes
)