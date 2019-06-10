
add_executable(evdp_test_destroy_clean test_destroy_clean.c)
target_link_libraries(evdp_test_destroy_clean $<TARGET_NAME:nebase>)
add_test(NAME evdp_test_destroy_clean COMMAND $<TARGET_NAME:evdp_test_destroy_clean>)

add_executable(evdp_test_itimer_overrun test_itimer_overrun.c)
target_link_libraries(evdp_test_itimer_overrun $<TARGET_NAME:nebase>)
add_test(NAME evdp_test_itimer_overrun COMMAND $<TARGET_NAME:evdp_test_itimer_overrun>)

add_executable(evdp_test_pipe_read_close test_pipe_read_close.c)
target_link_libraries(evdp_test_pipe_read_close $<TARGET_NAME:nebase>)
add_test(NAME evdp_test_pipe_read_close COMMAND $<TARGET_NAME:evdp_test_pipe_read_close>)
