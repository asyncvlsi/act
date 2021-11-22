add_executable(prs2net main.cc)

target_link_libraries(prs2net ActCommon ActLang ActPasses)

install (TARGETS prs2net RUNTIME DESTINATION bin)
