add_executable(tlint tlint.cc)
target_link_libraries(tlint ActLang ActPrsim)
install (TARGETS tlint RUNTIME DESTINATION bin)

add_executable(tr2alint hs.cc)
target_link_libraries(tr2alint ActCommon)
install (TARGETS tr2alint RUNTIME DESTINATION bin)

install (FILES lint.conf lint_opts.conf DESTINATION conf/generic)
