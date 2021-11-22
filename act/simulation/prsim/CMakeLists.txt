add_library(ActPrsim channel.c prs.c channel.h prs.h)
target_link_libraries(ActPrsim ActCommon ActMiniScheme)

add_executable(prsim prsim.c)
target_link_libraries(prsim ActCommon ActMiniScheme ActPrsim)
install (TARGETS prsim RUNTIME DESTINATION bin)

add_executable(prspack prspack.c)
target_link_libraries(prspack ActCommon)
install (TARGETS prspack RUNTIME DESTINATION bin)
