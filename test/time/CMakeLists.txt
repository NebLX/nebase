
add_executable(time_test_up test_up.c)
target_link_libraries(time_test_up $<TARGET_NAME:nebase>)
add_test(NAME time_test_up COMMAND $<TARGET_NAME:time_test_up>)

add_executable(time_test_boot test_boot.c)
target_link_libraries(time_test_boot $<TARGET_NAME:nebase>)
add_test(NAME time_test_boot COMMAND $<TARGET_NAME:time_test_boot>)

add_executable(time_test_daytime_abs test_daytime_abs.c)
target_link_libraries(time_test_daytime_abs $<TARGET_NAME:nebase>)
add_test(NAME time_test_daytime_abs_0 COMMAND $<TARGET_NAME:time_test_daytime_abs> 0)
add_test(NAME time_test_daytime_abs_6 COMMAND $<TARGET_NAME:time_test_daytime_abs> 6)
add_test(NAME time_test_daytime_abs_12 COMMAND $<TARGET_NAME:time_test_daytime_abs> 12)
add_test(NAME time_test_daytime_abs_18 COMMAND $<TARGET_NAME:time_test_daytime_abs> 18)
