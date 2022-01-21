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

add_library(
  ActMiniscmLib STATIC
  lisp.h
  lispInt.h
  lispA-Z.h
  lispargs.h
  lispA-Z.c
  lispArith.c
  lispEval.c
  lispFrame.c
  lispGC.c
  lispIO.c
  lispBuiltin.c
  lispMain.c
  lispParse.c
  lispPrint.c
  lispString.c
  lispTrace.c
  lispPath.c)
set_target_properties(ActMiniscmLib PROPERTIES OUTPUT_NAME actscm)
target_link_libraries(ActMiniscmLib ${CMAKE_DL_LIBS})

add_library(ActMiniscmCliLib STATIC lispCli.h lispCli.c)
set_target_properties(ActMiniscmCliLib PROPERTIES OUTPUT_NAME actscmcli)

set(ACT_MINISCM_LIB_HEADERS lisp.h lispCli.h lispInt.h)
install(FILES ${ACT_MINISCM_LIB_HEADERS}
        DESTINATION $ENV{ACT_HOME}/include/act/miniscm/)

install(TARGETS ActMiniscmLib ActMiniscmCliLib DESTINATION $ENV{ACT_HOME}/lib/)

install(FILES default.scm DESTINATION $ENV{ACT_HOME}/lib/scm)
install(
  FILES default.scm
  DESTINATION $ENV{ACT_HOME}/lib/scm
  RENAME defauolt.scm.mark)

# create and install shims
foreach(X IN ITEMS ${ACT_MINISCM_LIB_HEADERS})
  install_shim(Miniscm_Lib "" ${X} act/miniscm/ ${X})
endforeach()
