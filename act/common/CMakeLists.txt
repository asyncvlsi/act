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

set(ACT_COMMON_LIB_VLSI_HEADERS
    array.h
    bitset.h
    misc.h
    hash.h
    qops.h
    config.h
    atrace.h
    avl.h
    lzw.h
    lex.h
    file.h
    heap.h
    except.h
    pp.h
    list.h
    bool.h
    names.h
    mstring.h
    mytime.h
    path.h
    log.h
    sim.h
    agraph.h
    int.h)
set(ACT_COMMON_LIB_VLSI_SOURCES
    bitset.c
    misc.c
    hash.c
    config.c
    atrace.c
    avl.c
    lzw.c
    lex.c
    file.c
    heap.c
    except.c
    pp.c
    list.c
    bool.c
    names.c
    mstring.c
    time.c
    ext.c
    path.c
    log.cc
    sim.cc
    agraph.cc
    int.cc)

add_library(ActCommonLib STATIC ${ACT_COMMON_LIB_VLSI_HEADERS}
                                ${ACT_COMMON_LIB_VLSI_SOURCES})
set_target_properties(ActCommonLib PROPERTIES OUTPUT_NAME vlsilib)

add_library(ActCommonLibSh SHARED ${ACT_COMMON_LIB_VLSI_HEADERS}
                                  ${ACT_COMMON_LIB_VLSI_SOURCES})
set_target_properties(ActCommonLibSh PROPERTIES OUTPUT_NAME vlsilib_sh)

add_library(ActASimLib STATIC mem.h mem.cc hconfig.h hconfig.cc simdes.h
                              simdes.cc)
target_compile_options(ActASimLib PRIVATE -DFAIR -DASYNCHRONOUS)
set_target_properties(ActASimLib PROPERTIES OUTPUT_NAME asim)

add_library(ActASimLibSh SHARED mem.h mem.cc hconfig.h hconfig.cc simdes.h
                                simdes.cc)
target_compile_options(ActASimLibSh PRIVATE -DFAIR -DASYNCHRONOUS)
set_target_properties(ActASimLibSh PROPERTIES OUTPUT_NAME asim_sh)

install(FILES ${ACT_COMMON_LIB_VLSI_HEADERS}
        DESTINATION $ENV{ACT_HOME}/include/act/common/)
install(FILES mem.h hconfig.h simdes.h
        DESTINATION $ENV{ACT_HOME}/include/act/common/)
install(TARGETS ActCommonLib ActCommonLibSh ActASimLib ActASimLibSh
        DESTINATION $ENV{ACT_HOME}/lib/)

# create and install shims
foreach(X IN ITEMS ${ACT_COMMON_LIB_VLSI_HEADERS})
  install_shim(Common_Lib common/ ${X} act/common/ ${X})
endforeach()
