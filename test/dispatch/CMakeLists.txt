
add_executable(dispatch_test_pipe_read_close test_pipe_read_close.c)
target_link_libraries(dispatch_test_pipe_read_close $<TARGET_NAME:nebase>)
add_test(NAME dispatch_test_pipe_read_close COMMAND $<TARGET_NAME:dispatch_test_pipe_read_close>)

add_executable(dispatch_test_destroy_clean test_destroy_clean.c)
target_link_libraries(dispatch_test_destroy_clean $<TARGET_NAME:nebase>)
add_test(NAME dispatch_test_destroy_clean COMMAND $<TARGET_NAME:dispatch_test_destroy_clean>)

add_executable(dispatch_test_timer_del_self test_timer_del_self.c)
target_link_libraries(dispatch_test_timer_del_self $<TARGET_NAME:nebase>)
add_test(NAME dispatch_test_timer_del_self COMMAND $<TARGET_NAME:dispatch_test_timer_del_self>)
