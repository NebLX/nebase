
add_executable(sem_test_proc_one_same_user test_proc_one_same_user.c)
target_link_libraries(sem_test_proc_one_same_user $<TARGET_NAME:nebase>)
add_test(NAME sem_test_proc_one_same_user COMMAND $<TARGET_NAME:sem_test_proc_one_same_user>)

add_executable(sem_test_proc_wait_removed test_proc_wait_removed.c)
target_link_libraries(sem_test_proc_wait_removed $<TARGET_NAME:nebase>)
add_test(NAME sem_test_proc_wait_removed COMMAND $<TARGET_NAME:sem_test_proc_wait_removed>)
