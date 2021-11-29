add_executable(test_inlinepass main.cc)

target_link_libraries(test_inlinepass ActCommon ActLang ActPasses)

install (TARGETS test_inlinepass RUNTIME DESTINATION bin)
