add_executable(prs2sim main.cc)

target_link_libraries(prs2sim ActCommon ActLang ActPasses)

install (TARGETS prs2sim RUNTIME DESTINATION bin)