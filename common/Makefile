#-------------------------------------------------------------------------
#
#  Copyright (c) 2011 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

LIB1=libvlsilib_$(EXT).a
LIB2=libALINT_$(EXT).so

TARGETLIBS=$(LIB1) $(LIB2)

TARGETINCS=array.h bitset.h misc.h hash.h qops.h config.h atrace.h \
	avl.h lzw.h lex.h file.h heap.h except.h pp.h list.h bool.h \
	names.h

OBJS1=bitset.o misc.o hash.o config.o atrace.o avl.o lzw.o lex.o file.o \
	heap.o except.o pp.o list.o bool.o names.o
OBJS2=hash2.os atrace2.os ns.os misc.os

OBJS=$(OBJS1) $(OBJS2)
CLEAN=hash2.c hash2.h atrace2.c atrace2.h

include $(VLSITOOLS)/scripts/Makefile.std

hash2.c: hash.c
	sed 's/hash_/myhash_/g' $< | sed 's/myhash_bucket/hash_bucket/g' | sed 's/hash\.h/hash2.h/' > hash2.c

hash2.h: hash.h
	sed 's/hash_/myhash_/g' $< | sed 's/myhash_bucket/hash_bucket/g' | sed 's/hash\.h/hash2.h/' > hash2.h

atrace2.c: atrace.c
	sed 's/hash_/myhash_/g' $< | sed 's/myhash_bucket/hash_bucket/g' | sed 's/atrace\.h/atrace2.h/' > atrace2.c
atrace2.h: atrace.h
	sed 's/hash\.h/hash2.h/' $< > atrace2.h

$(LIB1): $(OBJS1)
	ar ruv $(LIB1) $(OBJS1)
	$(RANLIB) $(LIB1)

$(LIB2): $(OBJS2)
	$(CC) $(SH_LINK_OPTIONS) -o $(LIB2) $(OBJS2)

hash2.os: hash2.h
atrace2.os: atrace2.h

-include Makefile.deps

