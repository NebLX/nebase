
add_executable(io_test_pty_send_sigint test_pty_send_sigint.c)
target_link_libraries(io_test_pty_send_sigint
  $<TARGET_NAME:nebase>
  $<$<BOOL:${OS_LINUX}>:NeBase::Threads>
  $<$<BOOL:${OS_DFLYBSD}>:NeBase::Threads>
  $<$<BOOL:${OS_NETBSD}>:rt>
)
add_test(NAME io_test_pty_send_sigint COMMAND $<TARGET_NAME:io_test_pty_send_sigint>)
