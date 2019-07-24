
add_executable(log_test_stdio_perror test_log_stdio_perror.c)
target_link_libraries(log_test_stdio_perror $<TARGET_NAME:nebase>)
add_test(NAME log_test_stdio_perror COMMAND $<TARGET_NAME:log_test_stdio_perror>)
set_tests_properties(log_test_stdio_perror PROPERTIES FAIL_REGULAR_EXPRESSION "%m")

add_executable(log_test_stdio_debug test_log_stdio_debug.c)
target_link_libraries(log_test_stdio_debug $<TARGET_NAME:nebase>)
add_test(NAME log_test_stdio_debug COMMAND $<TARGET_NAME:log_test_stdio_debug>)
set_tests_properties(log_test_stdio_debug PROPERTIES PASS_REGULAR_EXPRESSION "Debug message OK")

if(WITH_GLIB2)
  add_executable(log_test_glog_file test_log_glog_file.c)
  target_link_libraries(log_test_glog_file
    $<TARGET_NAME:nebase>
    PkgConfig::GLIB2
  )
  add_test(NAME log_test_glog_file COMMAND $<TARGET_NAME:log_test_glog_file>)
endif(WITH_GLIB2)
