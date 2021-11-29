
add_library(
  ActCommon
  agraph.h
  agraph.cc
array.h
atrace.c
atrace.h
avl.c
avl.h
bitset.c
bitset.h
bool.c
bool.h
channel.h
config.c
config.h
contexts.h
count.h
except.c
except.h
ext.c
ext.h
file.c
file.h
hash.c
hash.h
hconfig.h
heap.c
heap.h
int.h
int.cc
lex.c
lex.h
list.c
list.h
log.cc
log.h
lzw.c
lzw.h
machine.h
mem.h
misc.c
misc.h
mstring.c
mstring.h
mutex.h
mytime.h
names.c
names.h
pp.c
pp.h
qops.h
sim.cc
sim.h
simdes.h
simthread.h
thread.h
time.c)


set_target_properties(
   ActCommon PROPERTIES PUBLIC_HEADER "agraph.h;array.h;atrace.h;avl.h;bitset.h;bool.h;channel.h;config.h;contexts.h;count.h;except.h;ext.h;file.h;hash.h;hconfig.h;heap.h;int.h;lex.h;list.h;log.h;lzw.h;machine.h;mem.h;misc.h;mstring.h;mutex.h;mytime.h;names.h;pp.h;qops.h;sim.h;simdes.h;simthread.h;thread.h"
)
set_target_properties(ActCommon PROPERTIES OUTPUT_NAME actcommon)

install (TARGETS ActCommon
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/act/common
)
