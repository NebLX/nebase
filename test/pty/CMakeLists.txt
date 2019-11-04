
add_executable(pty_test_send_sigint test_send_sigint.c)
target_link_libraries(pty_test_send_sigint $<TARGET_NAME:nebase>)
add_test(NAME pty_test_send_sigint COMMAND $<TARGET_NAME:pty_test_send_sigint>)
