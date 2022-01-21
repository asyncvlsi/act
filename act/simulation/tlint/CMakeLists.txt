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

add_executable(ActTLintExe tlint.cc)
set_target_properties(ActTLintExe PROPERTIES OUTPUT_NAME tlint)
target_link_libraries(ActTLintExe ActLangLib ActPrsLib)

add_executable(ActTr2ALintExe hs.cc)
set_target_properties(ActTr2ALintExe PROPERTIES OUTPUT_NAME tr2alint)
target_link_libraries(ActTr2ALintExe ActLangLib z)

add_compile_options(ActTLintExe -DHAVE_ZLIB)
add_compile_options(ActTr2ALintExe -DHAVE_ZLIB)

install(TARGETS ActTLintExe ActTr2ALintExe DESTINATION $ENV{ACT_HOME}/bin)
install(
  TARGETS ActTLintExe
  DESTINATION $ENV{ACT_HOME}/bin
  RENAME tlint.mark)
install(
  TARGETS ActTr2ALintExe
  DESTINATION $ENV{ACT_HOME}/bin
  RENAME tr2alint.mark)

install(FILES lint.conf DESTINATION $ENV{ACT_HOME}/conf/generic)
install(
  FILES lint.conf
  DESTINATION $ENV{ACT_HOME}/conf/generic
  RENAME lint.conf.mark)
install(FILES lint_opts.conf DESTINATION $ENV{ACT_HOME}/conf/generic)
install(
  FILES lint_opts.conf
  DESTINATION $ENV{ACT_HOME}/conf/generic
  RENAME lint_opts.conf.mark)
