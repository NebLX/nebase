
add_executable(signal_test_quit_event test_quit_event.c)
target_link_libraries(signal_test_quit_event
  $<TARGET_NAME:nebase>
)
add_test(NAME signal_test_quit_event COMMAND $<TARGET_NAME:signal_test_quit_event>)
