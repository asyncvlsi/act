#-------------------------------------------------------------------------
#
#  Copyright (c) 2011 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

weird: install_inc all

LIB=libact_$(EXT).a

TARGETLIBS=$(LIB)

TARGETINCS=lang.h expr.h act_parse.h act_walk_X.h id.h act_walk.extra.h \
	fets.def path.h namespaces.h prs.h act_parse_int.h treetypes.h act.h \
	types.h inst.h

TARGETINCSUBDIR=act

OBJS1=expr.o path.o
OBJS2=namespaces.o act_parse.o act_walk_X.o wrap.o act.o prs.o types.o \
	body.o check.o error.o

OBJS=$(OBJS1) $(OBJS2)

SRCS=$(OBJS1:.o=.c) $(OBJS2:.o=.cc)

CLEAN=act_parse.C act_walk_X.C expr.c expr.h act_parse.h \
	act_walk_X.h act_parse_int.h act_parse.def act_parse.gram \
	act.cy id.h

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(LIB): $(OBJS)
	ar ruv $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

docs: 
	doxygen
	(cd doc; gen)

act.cy: act.m4 defs.m4 expr.m4 lang.m4 namespaces.m4 types.m4 expr.c expr.h
	m4 -s act.m4 > act.cy

id.h act_walk.h act_parse.h act_parse_int.h act_parse.C act_walk_X.C: act.cy
	$(INSTALLDIR)/bin/pgen act.cy -w X -p -n act
	-mv act_parse.c act_parse.cc
	-mv act_walk_X.c act_walk_X.cc

expr.c: $(INSTALLDIR)/lib/expr.c
	cp $(INSTALLDIR)/lib/expr.c .

expr.h: $(INSTALLDIR)/lib/expr.h
	cp $(INSTALLDIR)/lib/expr.h .


-include Makefile.deps

