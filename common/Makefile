#-------------------------------------------------------------------------
#
#  Copyright (c) 2011 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

LIB1=libvlsilib_$(EXT).a
LIB2=libALINT_$(EXT).so
#LIB3=libssim_$(EXT).a
LIB4=libasim_$(EXT).a

TARGETLIBS=$(LIB1) $(LIB2) $(LIB3) $(LIB4)

TARGETINCS=array.h bitset.h misc.h hash.h qops.h config.h atrace.h \
	avl.h lzw.h lex.h file.h heap.h except.h pp.h list.h bool.h \
	names.h mstring.h mytime.h \
	channel.h hconfig.h checkpoint.h contexts.h count.h \
	machine.h mem.h mutex.h queue.h tasking.h thread.h sim.h \
	log.h ext.h

# general library support
OBJS1=bitset.o misc.o hash.o config.o atrace.o avl.o lzw.o lex.o file.o \
	heap.o except.o pp.o list.o bool.o names.o mstring.o time.o ext.o

# shared libs hsim integration
OBJS2=hash2.os atrace2.os ns.os misc.os

# synchronous simulation
#OBJS3CPP=queue.o tasking.o wakeupcall.o hconfig.o
#OBJS3C2=contexts_uf.o
#OBJS3CPP2=slog.o smem.o ssim.o scheckpoint.o

# asynchronous simulation
OBJS4CPP=hconfig.o 
OBJS4CPP2=alog.o amem.o asim.o
OBJS4C=thread.o mutex.o count.o channel.o
OBJS4C2=contexts_f.o 

OBJS3=$(OBJS3C) $(OBJS3C2) $(OBJS3CPP) $(OBJS3CPP2)

OBJS4=$(OBJS4C) $(OBJS4C2) $(OBJS4CPP) $(OBJS4CPP2)

SRCS=$(OBJS1:.o=.c) ns.c $(OBJS3C:.o=.c) $(OBJS3CPP:.o=.cc) \
	$(OBJS4C:.o=.c) $(OBJS4CPP:.o=.cc)

OBJS=$(OBJS1) $(OBJS2) $(OBJS3) $(OBJS4)
CLEAN=hash2.c hash2.h atrace2.c atrace2.h 

DEPEND_FLAGS=-DASYNCHRONOUS -DFAIR

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

hash2.c: hash.c
	sed 's/hash_/myhash_/g' $< | sed 's/myhash_bucket/hash_bucket/g' | sed 's/hash\.h/hash2.h/' > hash2.c

hash2.h: hash.h
	sed 's/hash_/myhash_/g' $< | sed 's/myhash_bucket/hash_bucket/g' | sed 's/hash\.h/hash2.h/' > hash2.h

atrace2.c: atrace.c
	sed 's/hash_/myhash_/g' $< | sed 's/myhash_bucket/hash_bucket/g' | sed 's/atrace\.h/atrace2.h/' > atrace2.c
atrace2.h: atrace.h
	sed 's/hash\.h/hash2.h/' $< > atrace2.h

#contexts_uf.o: contexts.c 
#	$(CC) -c $(CFLAGS) $(DFLAGS)  $< -o contexts_uf.o

contexts_f.o: contexts.c 
	$(CC) -c $(CFLAGS) $(DFLAGS) -DFAIR  $< -o contexts_f.o

thread.o: thread.c 
	$(CC) -c $(CFLAGS) $(DFLAGS) -DFAIR  $< 

mutex.o: mutex.c 
	$(CC) -c $(CFLAGS) $(DFLAGS) -DFAIR  $< 


count.o: count.c 
	$(CC) -c $(CFLAGS) $(DFLAGS) -DFAIR  $< 

channel.o: channel.c 
	$(CC) -c $(CFLAGS) $(DFLAGS) -DFAIR -DASYNCHRONOUS -DCLASS_HACKERY $< 

#ssim.o: sim.cc
#	$(CXX) -c $(CFLAGS) $(DFLAGS)  -DSYNCHRONOUS $< -o ssim.o

#smem.o: mem.cc
#	$(CXX) -c $(CFLAGS) $(DFLAGS)  -DSYNCHRONOUS $< -o smem.o

#slog.o: log.cc
#	$(CXX) -c $(CFLAGS) $(DFLAGS) -DSYNCHRONOUS $< -o slog.o

#scheckpoint.o: checkpoint.cc
#	$(CXX) -c $(CFLAGS) $(DFLAGS) -DSYNCHRONOUS $<  -o scheckpoint.o

asim.o: sim.cc
	$(CXX) -c $(CFLAGS) $(DFLAGS)  -DFAIR -DASYNCHRONOUS $< -o asim.o

amem.o: mem.cc
	$(CXX) -c $(CFLAGS) $(DFLAGS)  -DFAIR -DASYNCHRONOUS $< -o amem.o

alog.o: log.cc
	$(CXX) -c $(CFLAGS) $(DFLAGS) -DFAIR -DASYNCHRONOUS $< -o alog.o

acheckpoint.o: checkpoint.cc
	$(CXX) -c $(CFLAGS) $(DFLAGS) -DFAIR -DASYNCHRONOUS $<  -o acheckpoint.o

#queue.o: queue.cc
#	$(CXX) -c $(CFLAGS) $(DFLAGS) -DSYNCHRONOUS $<

$(LIB1): $(OBJS1)
	ar ruv $(LIB1) $(OBJS1)
	$(RANLIB) $(LIB1)

$(LIB3): $(OBJS3)
	ar ruv $(LIB3) $(OBJS3)
	$(RANLIB) $(LIB3)

$(LIB4): $(OBJS4)
	ar ruv $(LIB4) $(OBJS4)
	$(RANLIB) $(LIB4)

$(LIB2): $(OBJS2)
	$(CC) $(SH_LINK_OPTIONS) -o $(LIB2) $(OBJS2)

hash2.os: hash2.h
atrace2.os: atrace2.h

-include Makefile.deps

