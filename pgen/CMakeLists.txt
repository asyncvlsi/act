
add_executable(pgen
pgen.h pgen.c genwalk.c print.c
)

target_link_libraries(pgen ActCommon)

