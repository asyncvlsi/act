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

LIB1=libvlsilib_$(EXT).a
#LIB2=libALINT_$(EXT).so
#LIB3=libsim_$(EXT).a -- removed
LIB4=libasim_$(EXT).a

SHLIB1=libvlsilib_sh_$(EXT).so

TARGETLIBS=$(LIB1) $(LIB2) $(LIB3) $(LIB4) $(SHLIB1) $(SHLIB4)

TARGETINCSUBDIR=common

TARGETINCS=array.h bitset.h misc.h hash.h qops.h config.h atrace.h \
	avl.h lzw.h lex.h file.h heap.h except.h pp.h list.h bool.h \
	names.h mstring.h mytime.h \
	channel.h hconfig.h contexts.h count.h \
	machine.h mem.h mutex.h  thread.h sim.h \
	log.h ext.h simthread.h simdes.h agraph.h int.h path.h

# general library support
OBJSC1=bitset.o misc.o hash.o config.o atrace.o avl.o lzw.o lex.o file.o \
	heap.o except.o pp.o list.o bool.o names.o mstring.o time.o ext.o \
	path.o


OBJSCC1=log.o sim.o agraph.o int.o

OBJS1=$(OBJSC1) $(OBJSCC1)

SHOBJS1=$(OBJS1:.o=.os)

# shared libs hsim integration
#OBJS2=hash2.os atrace2.os ns.os misc.os

# asynchronous simulation
OBJS4CPP=hconfig.o 
OBJS4CPP2=simdes.o
#OBJS4CPP2=simthread.o simdes.o
#OBJS4C=thread.o mutex.o count.o channel.o
#OBJS4C2=contexts_f.o 

OBJS4=$(OBJS4C) $(OBJS4C2) $(OBJS4CPP) $(OBJS4CPP2) amem.o
SHOBJS4=$(OBJS4:.o=.os)

SRCS=$(OBJSC1:.o=.c) $(OBJSCC1:.o=.cc) $(OBJS3C:.o=.c) $(OBJS3CPP:.o=.cc) \
	$(OBJS4C:.o=.c) $(OBJS4CPP:.o=.cc)  $(OBJS4CPP2:.o=.cc)

# ns.c

OBJS=$(OBJS1) $(OBJS3) $(OBJS4) $(OBJS2) $(SHOBJS1)
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

amem.o: mem.cc
	$(CXX) -c $(CFLAGS) $(DFLAGS)  -DFAIR -DASYNCHRONOUS $< -o amem.o

amem.os: mem.cc
	$(CXX) -c $(CFLAGS) $(DFLAGS) $(SH_BUILD_OPTIONS) -DFAIR -DASYNCHRONOUS $< -o amem.os

#acheckpoint.o: checkpoint.cc
#	$(CXX) -c $(CFLAGS) $(DFLAGS) -DFAIR -DASYNCHRONOUS $<  -o acheckpoint.o

#queue.o: queue.cc
#	$(CXX) -c $(CFLAGS) $(DFLAGS) -DSYNCHRONOUS $<

$(LIB1): $(OBJS1)
	ar ruv $(LIB1) $(OBJS1)
	$(RANLIB) $(LIB1)

$(SHLIB1): $(SHOBJS1)
	$(VLSI_TOOLS_SRC)/scripts/linkso $(SHLIB1) $(SHOBJS1)

$(LIB4): $(OBJS4)
	ar ruv $(LIB4) $(OBJS4)
	$(RANLIB) $(LIB4)

$(SHLIB4): $(SHOBJS4)
	$(VLSI_TOOLS_SRC)/scripts/linkso $(SHLIB4) $(SHOBJS4)
	$(CXX) $(shell echo $(SH_LINK_OPTIONS)) -o $(SHLIB4) $(SHOBJS4)

$(LIB2): $(OBJS2)
	$(CC) $(shell echo $(SH_LINK_OPTIONS)) -o $(LIB2) $(OBJS2)

hash2.os: hash2.h
atrace2.os: atrace2.h

-include Makefile.deps

