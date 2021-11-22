add_executable(act-test test.cc)

target_link_libraries(act-test ActLang)

install (TARGETS act-test RUNTIME DESTINATION bin)