
add_executable(time_test_up test_up.c)
target_link_libraries(time_test_up $<TARGET_NAME:nebase>)
add_test(NAME time_test_up COMMAND $<TARGET_NAME:time_test_up>)

add_executable(time_test_boot test_boot.c)
target_link_libraries(time_test_boot $<TARGET_NAME:nebase>)
add_test(NAME time_test_boot COMMAND $<TARGET_NAME:time_test_boot>)
