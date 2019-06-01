
add_executable(evdp_test_destroy_clean test_destroy_clean.c)
target_link_libraries(evdp_test_destroy_clean $<TARGET_NAME:nebase>)
add_test(NAME evdp_test_destroy_clean COMMAND $<TARGET_NAME:evdp_test_destroy_clean>)
