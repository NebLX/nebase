
add_executable(io_test_redirect_to_null test_redirect_to_null.c)
target_link_libraries(io_test_redirect_to_null $<TARGET_NAME:nebase>)
add_test(NAME io_test_redirect_to_null COMMAND $<TARGET_NAME:io_test_redirect_to_null>)
set_tests_properties(io_test_redirect_to_null PROPERTIES FAIL_REGULAR_EXPRESSION "message to null")
