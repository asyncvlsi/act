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
BIN=tlint.$(EXT)
BIN2=tr2alint.$(EXT)
TARGETS=$(BIN) $(BIN2)

TARGETCONF=lint.conf lint_opts.conf

OBJS2=hs.o
OBJS1=tlint.o

OBJS=$(OBJS1) $(OBJS2)

SRCS=$(OBJS:.o=.cc)

ZLIBS=-lz

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

CFLAGS+=-DHAVE_ZLIB

$(BIN): $(LIB) $(OBJS1) $(ACTDEPEND)
	$(CXX) $(CFLAGS) $(OBJS1) ../prsim/$(EXT)/prs.o -o $(BIN) $(LIBACT)

$(BIN2): $(LIB) $(OBJS2) $(ACTDEPEND)
	$(CXX) $(CFLAGS) $(OBJS2) -o $(BIN2) $(LIBACT) $(ZLIBS)


-include Makefile.deps
