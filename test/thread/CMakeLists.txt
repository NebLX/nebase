
add_executable(thread_test_create test_create.c)
target_link_libraries(thread_test_create
  $<TARGET_NAME:nebase>
  Threads::Threads
)
add_test(NAME thread_test_create COMMAND $<TARGET_NAME:thread_test_create>)

add_executable(thread_test_id test_id.c)
target_link_libraries(thread_test_id
  $<TARGET_NAME:nebase>
  Threads::Threads
)
add_test(NAME thread_test_id COMMAND $<TARGET_NAME:thread_test_id>)

add_executable(thread_test_running test_running.c)
target_link_libraries(thread_test_running
  $<TARGET_NAME:nebase>
  Threads::Threads
)
add_test(NAME thread_test_running COMMAND $<TARGET_NAME:thread_test_running>)
