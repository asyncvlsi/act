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

function(install_shim folder old_path old_file new_path new_file)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${new_file}.shim
    COMMAND
      echo \"\#pragma message \\\"DEPRECATED ACT INCLUDE: prefer \\'\#include
      <${new_path}${new_file}>\\' to \\'\#include
      <${old_path}${old_file}>\\'\\\"\" >
      ${CMAKE_CURRENT_BINARY_DIR}/${old_file}.shim
    COMMAND echo \"\#include \\\"${new_path}${new_file}\\\"\" >>
            ${CMAKE_CURRENT_BINARY_DIR}/${old_file}.shim)
  add_custom_target(MakeShim_Act_${folder}_${new_file}.shim ALL
                    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${old_file}.shim)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${old_file}.shim
    DESTINATION $ENV{ACT_HOME}/include/${old_path}
    RENAME ${old_file})
endfunction()

add_subdirectory(common)
add_subdirectory(pgen)
add_subdirectory(miniscm)
add_subdirectory(lang)
add_subdirectory(passes)
add_subdirectory(transform)
add_subdirectory(simulation)
add_subdirectory(tech)
