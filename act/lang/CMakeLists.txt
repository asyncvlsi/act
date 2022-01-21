# -------------------------------------------------------------------------
#
# Copyright (c) 2022 Henry Heffan
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
# -------------------------------------------------------------------------

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.cc
         ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.def
         ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.gram
         ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.h
         ${CMAKE_CURRENT_SOURCE_DIR}/act_parse_id.h
         ${CMAKE_CURRENT_SOURCE_DIR}/act_parse_int.h
         ${CMAKE_CURRENT_SOURCE_DIR}/act_walk_X.cc
         ${CMAKE_CURRENT_SOURCE_DIR}/act_walk_X.h
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/act.m4
             ${CMAKE_CURRENT_BINARY_DIR}/namespaces.m4
             ${CMAKE_CURRENT_BINARY_DIR}/types.m4
             ${CMAKE_CURRENT_BINARY_DIR}/defs.m4
             ${CMAKE_CURRENT_BINARY_DIR}/lang.m4
             ${CMAKE_CURRENT_BINARY_DIR}/expr.m4
             ${CMAKE_CURRENT_BINARY_DIR}/act_walk.extra.h
             ${CMAKE_CURRENT_BINARY_DIR}/act.cy
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse.c
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse.def
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse.gram
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse.h
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse_id.h
             ${CMAKE_CURRENT_BINARY_DIR}/act_parse_int.h
             ${CMAKE_CURRENT_BINARY_DIR}/act_walk_X.c
             ${CMAKE_CURRENT_BINARY_DIR}/act_walk_X.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    cp ${CMAKE_CURRENT_SOURCE_DIR}/act.m4 ${CMAKE_CURRENT_BINARY_DIR}/act.m4 &&
    cp ${CMAKE_CURRENT_SOURCE_DIR}/namespaces.m4
    ${CMAKE_CURRENT_BINARY_DIR}/namespaces.m4 && cp
    ${CMAKE_CURRENT_SOURCE_DIR}/types.m4 ${CMAKE_CURRENT_BINARY_DIR}/types.m4 &&
    cp ${CMAKE_CURRENT_SOURCE_DIR}/defs.m4 ${CMAKE_CURRENT_BINARY_DIR}/defs.m4
    && cp ${CMAKE_CURRENT_SOURCE_DIR}/lang.m4
    ${CMAKE_CURRENT_BINARY_DIR}/lang.m4 && cp
    ${CMAKE_CURRENT_SOURCE_DIR}/expr.m4 ${CMAKE_CURRENT_BINARY_DIR}/expr.m4 &&
    cp ${CMAKE_CURRENT_SOURCE_DIR}/act_walk.extra.h
    ${CMAKE_CURRENT_BINARY_DIR}/act_walk.extra.h && rm -f act_parse.def
    act_parse.h act_parse_int.h act_parse_id.h act_parse.c act_walk_X.h
    act_walk_X.c && m4 -s act.m4 > act.cy && ${CMAKE_BINARY_DIR}/act/pgen/pgen
    act.cy -w X -h -b -p -n act && mv ${CMAKE_CURRENT_BINARY_DIR}/act_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.cc && mv
    ${CMAKE_CURRENT_BINARY_DIR}/act_parse.def
    ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.def && mv
    ${CMAKE_CURRENT_BINARY_DIR}/act_parse.gram
    ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.gram && mv
    ${CMAKE_CURRENT_BINARY_DIR}/act_parse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.h && mv
    ${CMAKE_CURRENT_BINARY_DIR}/act_parse_id.h
    ${CMAKE_CURRENT_SOURCE_DIR}/act_parse_id.h && mv
    ${CMAKE_CURRENT_BINARY_DIR}/act_parse_int.h
    ${CMAKE_CURRENT_SOURCE_DIR}/act_parse_int.h && mv
    ${CMAKE_CURRENT_BINARY_DIR}/act_walk_X.c
    ${CMAKE_CURRENT_SOURCE_DIR}/act_walk_X.cc && mv
    ${CMAKE_CURRENT_BINARY_DIR}/act_walk_X.h
    ${CMAKE_CURRENT_SOURCE_DIR}/act_walk_X.h
  DEPENDS ActPGenExe
          ${CMAKE_CURRENT_SOURCE_DIR}/act.m4
          ${CMAKE_CURRENT_SOURCE_DIR}/namespaces.m4
          ${CMAKE_CURRENT_SOURCE_DIR}/types.m4
          ${CMAKE_CURRENT_SOURCE_DIR}/defs.m4
          ${CMAKE_CURRENT_SOURCE_DIR}/lang.m4
          ${CMAKE_CURRENT_SOURCE_DIR}/expr.m4
          ${CMAKE_CURRENT_SOURCE_DIR}/act_walk.extra.h)

add_custom_target(
  ActLangAutogen
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.cc
          ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.def
          ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.gram
          ${CMAKE_CURRENT_SOURCE_DIR}/act_parse.h
          ${CMAKE_CURRENT_SOURCE_DIR}/act_parse_id.h
          ${CMAKE_CURRENT_SOURCE_DIR}/act_parse_int.h
          ${CMAKE_CURRENT_SOURCE_DIR}/act_walk_X.cc
          ${CMAKE_CURRENT_SOURCE_DIR}/act_walk_X.h)

set(ACT_LANG_LIB_SOURCES
    act_parse.cc
    act_parse.def
    act_parse.gram
    act_parse.h
    act_parse_id.h
    act_parse_int.h
    act_walk_X.cc
    act_walk_X.h
    act.cc
    act.h
    act_array.h
    act_id.h
    act_parse.cc
    act_parse.h
    act_parse_id.h
    act_parse_int.h
    act_walk.extra.h
    act_walk_X.cc
    act_walk_X.h
    array.cc
    basetype.h
    body.cc
    body.h
    check.cc
    CMakeLists.txt
    connect.cc
    error.cc
    expr.c
    expr.h
    expr2.cc
    expr_extra.c
    expr_width.h
    fexpr.cc
    fexpr.h
    func.cc
    id.cc
    inline.cc
    inline.h
    inst.cc
    inst.h
    iter.cc
    iter.h
    lang.cc
    lang.h
    macros.cc
    mangle.cc
    namespaces.cc
    namespaces.h
    pass.cc
    path.c
    path.h
    process.cc
    prs.cc
    prs.h
    scope.cc
    tech.cc
    tech.h
    treetypes.h
    typefactory.cc
    types.cc
    types.h
    value.h
    wrap.cc)

add_library(ActLangLib STATIC ${ACT_LANG_LIB_SOURCES})
add_dependencies(ActLangLib ActLangAutogen)
set_target_properties(ActLangLib PROPERTIES OUTPUT_NAME act)
target_link_libraries(ActLangLib ActCommonLib ${CMAKE_DL_LIBS})

add_library(ActLangLibSh SHARED ${ACT_LANG_LIB_SOURCES})
add_dependencies(ActLangLibSh ActLangAutogen)
set_target_properties(ActLangLibSh PROPERTIES OUTPUT_NAME act_sh)
target_link_libraries(ActLangLibSh ActCommonLibSh)

set(ACT_LANG_LIB_HEADERS
    lang.h
    expr.h
    act_parse_id.h
    path.h
    namespaces.h
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
    inline.h)
install(FILES ${ACT_LANG_LIB_HEADERS}
        DESTINATION $ENV{ACT_HOME}/include/act/lang/)

install(FILES global.conf prs2net.conf prs2net_opts.conf
        DESTINATION $ENV{ACT_HOME}/conf/generic)

install(TARGETS ActLangLib ActLangLibSh DESTINATION $ENV{ACT_HOME}/lib/)

# create and install shims
foreach(X IN ITEMS ${ACT_LANG_LIB_HEADERS})
  install_shim(Lang_Lib act/ ${X} act/lang/ ${X})
endforeach()

add_subdirectory(test)
