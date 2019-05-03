#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
BINARY=lvp.$(EXT)
BIN2=extfix.$(EXT)

TARGETS=$(BINARY) $(BIN2)

TARGETCONF=lvp.conf

OBJS1=main.o lvs.o dots.o excl.o flatten.o \
	hier.o parse.o \
	sneak.o table.o var.o prs.o print.o \
	pchg.o 

OBJS2=extfix.o

OBJS=$(OBJS1) $(OBJS2) init.o

# cap.o bump.o bump2.o

SRCS=$(OBJS:.o=.c) init.cc


include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

install_conf: lvp.conf
	@$(VLSI_TOOLS_SRC)/scripts/install lvp.conf $(INSTALLLIB)/act/lvp.conf


DFLAGS+=-DDIGITAL_ONLY

$(BINARY): $(LIB) $(OBJS1) init.o $(ACTDEPEND)
	$(CXX) $(CFLAGS) $(OBJS1) init.o -o $(BINARY) $(LIBACT)

$(BIN2): $(LIB) $(OBJS2) $(LIBDEPEND)
	$(CXX) $(CFLAGS) $(OBJS2) -o $(BIN2) $(LIBCOMMON)

-include Makefile.deps
