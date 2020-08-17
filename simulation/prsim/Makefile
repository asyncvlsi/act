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
BIN1=prsim.$(EXT)
BIN2=prspack.$(EXT)
TARGETS=$(BIN1) $(BIN2)

OBJS1=prsim.o channel.o prs.o
OBJS2=prspack.o

OBJS=$(OBJS1) $(OBJS2)

SRCS=$(OBJS:.o=.c)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

#
# If you want the mini-scm interpreter
#
DFLAGS+=-DUSE_SCM


$(BIN1): $(LIB) $(OBJS1) $(LIBDEPEND) $(LIBSCMDEPEND)
	$(CXX) $(CFLAGS) $(OBJS1) -o $(BIN1) $(LIBCOMMON) $(LIBACTSCM) -ledit -ldl

$(BIN2): $(LIB) $(OBJS2) $(LIBDEPEND)
	$(CXX) $(CFLAGS) $(OBJS2) -o $(BIN2) $(LIBCOMMON)


-include Makefile.deps
