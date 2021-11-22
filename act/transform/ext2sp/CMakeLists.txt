add_executable(ext2sp main.cc)

target_link_libraries(ext2sp ActCommon ActLang)

install (TARGETS ext2sp RUNTIME DESTINATION bin)

install (FILES extract.conf DESTINATION conf/generic)