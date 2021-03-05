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
LOCAL_LIB=libverilog_$(EXT).a

TARGETS=$(BINARY) $(BINARY2)

TARGETCONF=s2a.conf

TARGETINCS=v_parse_id.h

LOCAL_LIBOBJS=wrap.o v_parse.o v_walk_X.o vnet.o

OBJS1=main.o emit.o sync.o
OBJS2=main2.o
OBJS=$(OBJS1) $(OBJS2) $(LOCAL_LIBOBJS)

CLEAN=v_walk_X.h v_walk_X.cc v_parse.cc v_parse.h \
	v_parse_int.h v_parse.def v_parse.gram v_parse_id.h

SRCS=$(OBJS:.o=.cc)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(BINARY): $(LOCAL_LIB) $(OBJS1) $(ACTPASSDEPEND)
	$(CXX) $(CFLAGS) $(OBJS1) -o $(BINARY) $(LOCAL_LIB) $(LIBACTPASS)

$(BINARY2): $(LOCAL_LIB) $(OBJS2) $(ACTPASSDEPEND)
	$(CXX) $(CFLAGS) $(OBJS2) -o $(BINARY2) $(LOCAL_LIB) $(LIBACTPASS)

$(LOCAL_LIB): $(LOCAL_LIBOBJS)
	ar ruv $(LOCAL_LIB) $(LOCAL_LIBOBJS)
	$(RANLIB) $(LOCAL_LIB)

v_parse_id.h v_parse_int.h v_parse.cc v_walk_X.cc: v2act.cy
	$(INSTALLDIR)/bin/pgen v2act.cy -V -w X -h -b -p -n v
	-mv v_parse.c v_parse.cc
	-mv v_walk_X.c v_walk_X.cc
	-touch v_parse_id.h

-include Makefile.deps
