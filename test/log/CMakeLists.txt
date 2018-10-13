
add_executable(log_test_stdio test_log_stdio.c)
target_link_libraries(log_test_stdio
  $<TARGET_NAME:nebase>
)
add_test(NAME log_test_stdio COMMAND $<TARGET_NAME:log_test_stdio>)
set_tests_properties(log_test_stdio PROPERTIES FAIL_REGULAR_EXPRESSION ".*%m.*")
