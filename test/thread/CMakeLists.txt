
add_executable(thread_test_id test_id.c)
target_link_libraries(thread_test_id
  $<TARGET_NAME:nebase>
  Threads::Threads
)
add_test(NAME thread_test_id COMMAND $<TARGET_NAME:thread_test_id>)
