#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

BINARY=aflat.$(EXT)

TARGETS=$(BINARY)
EXTRA=../passes/$(EXT)/aflat.o

OBJS=main.o aflat.o

SRCS=$(OBJS:.o=.cc)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(BINARY): $(LIB) $(OBJS) $(ACTDEPEND)
	$(CXX) $(CFLAGS) $(OBJS) -o $(BINARY) $(LIBACT)

../passes/$(EXT)/aflat.o: aflat.o
	-if [ ! -d ../passes/$(EXT) ]; \
         then \
           mkdir ../passes/$(EXT); \
        fi
	cp aflat.o ../passes/$(EXT)


-include Makefile.deps
