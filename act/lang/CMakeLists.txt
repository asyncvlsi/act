configure_file(${CMAKE_CURRENT_SOURCE_DIR}/act_walk.extra.h
               ${CMAKE_CURRENT_BINARY_DIR}/act_walk.extra.h COPYONLY)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/act_parse_id.h
         ${CMAKE_CURRENT_BINARY_DIR}/act_parse.h
         ${CMAKE_CURRENT_BINARY_DIR}/act_parse_int.h
         ${CMAKE_CURRENT_BINARY_DIR}/act_parse.cc
         ${CMAKE_CURRENT_BINARY_DIR}/act_walk_X.cc
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND rm -rf act_parse.gram act_parse.def act_parse.h act_parse_int.h
          act_walk_X.h act_parse_id.h
  COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && m4 -s act.m4 >
          ${CMAKE_CURRENT_BINARY_DIR}/act.cy
  COMMAND ${CMAKE_BINARY_DIR}/act/pgen/pgen ${CMAKE_CURRENT_BINARY_DIR}/act.cy -w X
          -h -b -p -n act "act/lang/"
  COMMAND mv act_parse.c act_parse.cc
  COMMAND mv act_walk_X.c act_walk_X.cc
  # COMMAND pwd && cp ${CMAKE_CURRENT_SOURCE_DIR}/expr.h
  # ${CMAKE_CURRENT_BINARY_DIR}/expr.h COMMAND pwd && cp
  # ${CMAKE_CURRENT_BINARY_DIR}/act_walk.extra.h
  VERBATIM
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/act.cy
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse.gram
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse.def
  DEPENDS pgen
          act.m4
          defs.m4
          expr.m4
          lang.m4
          namespaces.m4
          types.m4
          expr.c
          expr.h)

add_library(
  ActLang
  act_walk.extra.h
  lang.h
  expr.h
  ${CMAKE_CURRENT_BINARY_DIR}/act_parse_id.h
  ${CMAKE_CURRENT_BINARY_DIR}/act_parse.h
  ${CMAKE_CURRENT_BINARY_DIR}/act_parse_int.h
  ${CMAKE_CURRENT_BINARY_DIR}/act_parse.cc
  ${CMAKE_CURRENT_BINARY_DIR}/act_walk_X.cc
  path.h
  namespaces.h
  expr_width.h
  act.h
  types.h
  inst.h
  iter.h
  act_array.h
  basetype.h
  body.h
  value.h
  tech.h
  warn.def
  act_id.h
  inline.h
  expr.c
  path.c
  expr_extra.c
  act_walk_X.cc
  wrap.cc
  prs.cc
  scope.cc
  act.cc
  namespaces.cc
  body.cc
  lang.cc
  id.cc
  array.cc
  expr2.cc
  inst.cc
  types.cc
  process.cc
  func.cc
  typefactory.cc
  check.cc
  connect.cc
  error.cc
  iter.cc
  mangle.cc
  pass.cc
  tech.cc
  fexpr.cc
  macros.cc
  inline.cc)
  
target_link_libraries(ActLang ActCommon)

set_target_properties(
   ActLang PROPERTIES PUBLIC_HEADER "act.h;act_array.h;act_id.h;basetype.h;body.h;expr.h;expr_width.h;inline.h;inst.h;iter.h;lang.h;namespaces.h;path.h;tech.h;types.h;value.h;warn.def"
)
set_target_properties(ActLang PROPERTIES OUTPUT_NAME actlang)

add_subdirectory(test)
install (TARGETS ActLang
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/act/lang
)
install(FILES 
  ${CMAKE_CURRENT_BINARY_DIR}/act_parse_id.h
  ${CMAKE_CURRENT_BINARY_DIR}/act_parse.h
  ${CMAKE_CURRENT_BINARY_DIR}/act_parse_int.h
  DESTINATION include/act/lang)

install (FILES global.conf prs2net.conf prs2net_opts.conf DESTINATION conf/generic)
