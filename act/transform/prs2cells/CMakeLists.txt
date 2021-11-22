add_executable(prs2cells main.cc)

target_link_libraries(prs2cells ActCommon ActLang ActPasses)

install (TARGETS prs2cells RUNTIME DESTINATION bin)
