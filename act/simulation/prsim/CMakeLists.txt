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

add_library(ActPrsLib channel.c prs.c channel.h prs.h)
target_link_libraries(ActPrsLib ActCommonLib)

add_executable(ActPrsimExe prsim.c)
set_target_properties(ActPrsimExe PROPERTIES OUTPUT_NAME prsim)
target_link_libraries(ActPrsimExe ActCommonLib ActMiniscmLib ActMiniscmCliLib
                      ActPrsLib edit)

add_executable(ActPrsPackExe prspack.c)
set_target_properties(ActPrsPackExe PROPERTIES OUTPUT_NAME prspack)
target_link_libraries(ActPrsPackExe ActCommonLib)

install(TARGETS ActPrsimExe ActPrsPackExe DESTINATION $ENV{ACT_HOME}/bin)
install(
  TARGETS ActPrsimExe
  DESTINATION $ENV{ACT_HOME}/bin
  RENAME prsim.mark)
install(
  TARGETS ActPrsPackExe
  DESTINATION $ENV{ACT_HOME}/bin
  RENAME prspack.mark)
