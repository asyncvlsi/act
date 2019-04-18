#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor,
#  Boston, MA  02110-1301, USA.
#
#-------------------------------------------------------------------------
BINARY=v2act.$(EXT)

TARGETS=$(BINARY)

OBJS1=main.o emit.o sync.o wrap.o v_parse.o v_walk_X.o
OBJS=$(OBJS1) $(OBJS2)

CLEAN=v_walk_X.h v_walk_X.cc v_parse.cc v_parse.h \
	v_parse_int.h v_parse.def v_parse.gram v_parse_id.h

SRCS=$(OBJS:.o=.cc)

tgt: v_parse_int.h all

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(BINARY): $(LIB) $(OBJS1) $(ACTPASSDEPEND)
	$(CXX) $(CFLAGS) $(OBJS1) -o $(BINARY) $(LIBACTPASS)

v_parse_int.h v_parse.cc v_walk_X.cc: v2act.cy
	$(INSTALLDIR)/bin/pgen v2act.cy -V -w X -p -n v
	-mv v_parse.c v_parse.cc
	-mv v_walk_X.c v_walk_X.cc

-include Makefile.deps
