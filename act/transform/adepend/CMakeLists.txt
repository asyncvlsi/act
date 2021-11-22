add_executable(adepend main.cc)

target_link_libraries(adepend ActLang)

install (TARGETS adepend RUNTIME DESTINATION bin)