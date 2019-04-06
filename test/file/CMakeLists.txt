
add_executable(file_test_dirfd_permission test_dirfd_permission.c)
target_link_libraries(file_test_dirfd_permission $<TARGET_NAME:nebase>)
add_test(NAME file_test_dirfd_permission COMMAND $<TARGET_NAME:file_test_dirfd_permission>)
