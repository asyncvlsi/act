add_executable(aflat main.cc)

target_link_libraries(aflat ActCommon ActLang ActPasses)

install (TARGETS aflat RUNTIME DESTINATION bin)
