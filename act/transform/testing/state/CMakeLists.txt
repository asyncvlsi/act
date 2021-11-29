add_executable(test_statepass main.cc)

target_link_libraries(test_statepass ActCommon ActLang ActPasses)

install (TARGETS test_statepass RUNTIME DESTINATION bin)
