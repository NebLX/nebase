
add_executable(dispatch_test_pipe_read_close test_pipe_read_close.c)
target_link_libraries(dispatch_test_pipe_read_close $<TARGET_NAME:nebase>)
add_test(NAME dispatch_test_pipe_read_close COMMAND $<TARGET_NAME:dispatch_test_pipe_read_close>)


