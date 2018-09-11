#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
BINARY=lvs.$(EXT)
BIN2=extfix.$(EXT)

TARGETS=$(BINARY) $(BIN2)

OBJS1=main.o lvs.o config.o dots.o excl.o flatten.o \
	parse_ext.o hier.o parse.o \
	sneak.o table.o var.o prs.o print.o path.o \
	pchg.o

OBJS2=extfix.o

OBJS=$(OBJS1) $(OBJS2)

# cap.o bump.o bump2.o

SRCS=$(OBJS:.o=.c)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

DFLAGS+=-DDIGITAL_ONLY

$(BINARY): $(LIB) $(OBJS1) $(LIBDEPEND)
	$(CXX) $(CFLAGS) $(OBJS1) -o $(BINARY) $(LIBCOMMON)

$(BIN2): $(LIB) $(OBJS2) $(LIBDEPEND)
	$(CXX) $(CFLAGS) $(OBJS2) -o $(BIN2) $(LIBCOMMON)

-include Makefile.deps
