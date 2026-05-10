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
BINARY2=sv2act.$(EXT)

TARGETS=$(BINARY) $(BINARY2)
TARGETSCRIPTS=v2act_quote.sed

TARGETCONF=s2a.conf

OBJS1=main.o emit.o sync.o
OBJS2=main2.o
OBJS=$(OBJS1) $(OBJS2)

SRCS=$(OBJS:.o=.cc)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(BINARY): $(LOCAL_LIB) $(OBJS1) $(ACTPASSDEPEND)
	$(CXX) $(CFLAGS) $(OBJS1) -o $(BINARY) -lverilog $(LIBACTPASS)

$(BINARY2): $(LOCAL_LIB) $(OBJS2) $(ACTPASSDEPEND)
	$(CXX) $(CFLAGS) $(OBJS2) -o $(BINARY2) -lverilog $(LIBACTPASS)

-include Makefile.deps
