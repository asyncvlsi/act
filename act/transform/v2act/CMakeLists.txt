configure_file(${CMAKE_CURRENT_SOURCE_DIR}/v_walk.extra.h
               ${CMAKE_CURRENT_BINARY_DIR}/v_walk.extra.h COPYONLY)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/v_parse.h
         ${CMAKE_CURRENT_BINARY_DIR}/v_parse_int.h
         ${CMAKE_CURRENT_BINARY_DIR}/v_parse.cc
         ${CMAKE_CURRENT_BINARY_DIR}/v_walk_X.cc
         ${CMAKE_CURRENT_BINARY_DIR}/v_walk_X.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND rm -rf v_parse.gram v_parse.def v_parse.h v_parse_int.h
          v_walk_X.h v_parse_id.h
  COMMAND ${CMAKE_BINARY_DIR}/act/pgen/pgen ${CMAKE_CURRENT_SOURCE_DIR}/v2act.cy -w X
          -h -b -p -n v "act/transform/v2act/"
  COMMAND mv v_parse.c v_parse.cc
  COMMAND mv v_walk_X.c v_walk_X.cc
  # COMMAND pwd && cp ${CMAKE_CURRENT_SOURCE_DIR}/expr.h
  # ${CMAKE_CURRENT_BINARY_DIR}/expr.h COMMAND pwd && cp
  # ${CMAKE_CURRENT_BINARY_DIR}/act_walk.extra.h
  VERBATIM
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/act.cy
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse.gram
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse.def
  DEPENDS pgen
          v2act.cy)

add_library(
  ActV2Act
         ${CMAKE_CURRENT_BINARY_DIR}/v_walk_X.h
  ${CMAKE_CURRENT_BINARY_DIR}/v_parse.h
  ${CMAKE_CURRENT_BINARY_DIR}/v_parse_int.h
  ${CMAKE_CURRENT_BINARY_DIR}/v_parse.cc
  ${CMAKE_CURRENT_BINARY_DIR}/v_walk_X.cc
v2act.h
vnet.cc
vnet.h
v_walk.extra.h
wrap.cc
)
target_link_libraries(ActV2Act ActCommon)
  
  add_executable(v2act main.cc emit.cc sync.cc)
target_link_libraries(v2act ActV2Act ActPasses)
install (TARGETS v2act
    RUNTIME DESTINATION bin
)
  add_executable(sv2act main2.cc)
target_link_libraries(sv2act ActV2Act ActPasses)
install (TARGETS sv2act
    RUNTIME DESTINATION bin
)
install (FILES v2act_quote.sed
     DESTINATION bin
)

install (FILES s2a.conf DESTINATION conf/generic)
#../act/transform/ext2sp/Makefile:TARGETCONF=extract.conf