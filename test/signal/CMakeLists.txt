
add_executable(signal_test_quit_event test_quit_event.c)
target_link_libraries(signal_test_quit_event $<TARGET_NAME:nebase>)
add_test(NAME signal_test_quit_event COMMAND $<TARGET_NAME:signal_test_quit_event>)

add_executable(signal_test_proc_block_chld test_proc_block_chld.c)
target_link_libraries(signal_test_proc_block_chld $<TARGET_NAME:nebase>)
add_test(NAME signal_test_proc_block_chld COMMAND $<TARGET_NAME:signal_test_proc_block_chld>)
