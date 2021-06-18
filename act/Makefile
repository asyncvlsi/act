#-------------------------------------------------------------------------
#
#  Copyright (c) 2011-2019 Rajit Manohar
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
LIB=libact_$(EXT).a
SHLIB=libact_sh_$(EXT).so

TARGETLIBS=$(LIB) $(SHLIB)

TARGETINCS=lang.h expr.h act_parse_id.h path.h namespaces.h act.h \
	types.h inst.h iter.h act_array.h basetype.h body.h value.h \
	tech.h warn.def act_id.h

TARGETINCSUBDIR=act

TARGETCONF=global.conf prs2net.conf prs2net_opts.conf

OBJS1=expr.o path.o expr_extra.o
OBJS2=act_parse.o act_walk_X.o wrap.o prs.o scope.o \
	act.o namespaces.o body.o lang.o id.o array.o expr2.o \
	inst.o types.o process.o func.o typefactory.o check.o \
	connect.o error.o iter.o \
	mangle.o pass.o tech.o fexpr.o

OBJS=$(OBJS1) $(OBJS2)

SHOBJS=$(OBJS:.o=.os)

SRCS=$(OBJS1:.o=.c) $(OBJS2:.o=.cc)

GENERATED=act_parse.cc act_walk_X.cc

CLEAN=act_parse.cc act_walk_X.cc expr.c expr.h act_parse.h \
	act_walk_X.h act_parse_int.h act_parse.def act_parse.gram \
	act.cy act_parse_id.h

SUBDIRSPOST=test

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(LIB): $(OBJS)
	ar ruv $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

$(SHLIB): $(SHOBJS)
	$(VLSI_TOOLS_SRC)/scripts/linkso $(SHLIB) $(SHOBJS) $(SHLIBCOMMON)

docs: 
	doxygen
	(cd doc; gen)

act.cy: act.m4 defs.m4 expr.m4 lang.m4 namespaces.m4 types.m4 expr.c expr.h
	m4 -s act.m4 > act.cy

act_parse_id.h act_walk.h act_parse.h act_parse_int.h act_parse.cc act_walk_X.cc: act.cy
	$(INSTALLDIR)/bin/pgen act.cy -w X -h -b -p -n act
	-mv act_parse.c act_parse.cc
	-mv act_walk_X.c act_walk_X.cc

expr.c: $(INSTALLDIR)/lib/expr.c
	cp $(INSTALLDIR)/lib/expr.c .

expr.h: $(INSTALLDIR)/lib/expr.h
	cat $(INSTALLDIR)/lib/expr.h | sed 's,\"id.h\",<act/act_parse_id.h>,' > expr.h


-include Makefile.deps

