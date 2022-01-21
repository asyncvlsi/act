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

set(ACT_PASSES_LIB_SOURCES
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
    statepass.h)

add_library(ActPassesLib STATIC ${ACT_PASSES_LIB_SOURCES})
set_target_properties(ActPassesLib PROPERTIES OUTPUT_NAME actpass)
target_link_libraries(ActPassesLib ActCommonLib ActLangLib)

add_library(ActPassesLibSh SHARED ${ACT_PASSES_LIB_SOURCES})
set_target_properties(ActPassesLibSh PROPERTIES OUTPUT_NAME actpass_sh)
target_link_libraries(ActPassesLibSh ActCommonLibSh ActLangLibSh)

set(ACT_PASSES_LIB_HEADERS
    aflat.h
    booleanize.h
    cells.h
    finline.h
    netlist.h
    passes.h
    sizing.h
    statepass.h)
install(FILES ${ACT_PASSES_LIB_HEADERS}
        DESTINATION $ENV{ACT_HOME}/include/act/passes/)

install(TARGETS ActPassesLib ActPassesLibSh DESTINATION $ENV{ACT_HOME}/lib/)

install_shim(Passes_Lib act passes.h act/passes/ passes.h)
