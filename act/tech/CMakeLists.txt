add_executable(techgen techgen.cc)
target_link_libraries(techgen ActLang)
install (TARGETS techgen RUNTIME DESTINATION bin)
install (FILES layout.conf DESTINATION conf/generic)
