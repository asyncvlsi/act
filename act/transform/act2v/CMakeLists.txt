add_executable(act2v main.cc)

target_link_libraries(act2v ActCommon ActLang ActPasses)

install (TARGETS act2v RUNTIME DESTINATION bin)
