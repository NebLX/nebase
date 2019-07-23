
add_executable(io_test_pty_send_sigint test_pty_send_sigint.c)
target_link_libraries(io_test_pty_send_sigint
  $<TARGET_NAME:nebase>
  $<$<BOOL:${OS_LINUX}>:NebulaX::Threads>
  $<$<BOOL:${OS_DFLYBSD}>:NebulaX::Threads>
  $<$<BOOL:${OS_NETBSD}>:rt>
)
add_test(NAME io_test_pty_send_sigint COMMAND $<TARGET_NAME:io_test_pty_send_sigint>)

add_executable(io_test_redirect_to_null test_redirect_to_null.c)
target_link_libraries(io_test_redirect_to_null $<TARGET_NAME:nebase>)
add_test(NAME io_test_redirect_to_null COMMAND $<TARGET_NAME:io_test_redirect_to_null>)
set_tests_properties(io_test_redirect_to_null PROPERTIES FAIL_REGULAR_EXPRESSION "message to null")
