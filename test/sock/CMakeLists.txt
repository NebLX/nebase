
add_executable(sock_test_socketpair_cred test_socketpair_cred.c)
target_link_libraries(sock_test_socketpair_cred
  $<TARGET_NAME:nebase>
)
add_test(NAME sock_test_socketpair_cred_stream COMMAND $<TARGET_NAME:sock_test_socketpair_cred> "stream")
add_test(NAME sock_test_socketpair_cred_seqpacket COMMAND $<TARGET_NAME:sock_test_socketpair_cred> "seqpacket")
add_test(NAME sock_test_socketpair_cred_dgram COMMAND $<TARGET_NAME:sock_test_socketpair_cred> "dgram")

add_executable(sock_test_unix_connect_cred test_unix_connect_cred.c)
target_link_libraries(sock_test_unix_connect_cred
  $<TARGET_NAME:nebase>
)
add_test(NAME sock_test_unix_connect_cred_stream COMMAND $<TARGET_NAME:sock_test_unix_connect_cred> "stream")
add_test(NAME sock_test_unix_connect_cred_seqpacket COMMAND $<TARGET_NAME:sock_test_unix_connect_cred> "seqpacket")

add_executable(sock_test_unix_dgram_cred test_unix_dgram_cred.c)
target_link_libraries(sock_test_unix_dgram_cred
  $<TARGET_NAME:nebase>
)
add_test(NAME sock_test_unix_dgram_cred COMMAND $<TARGET_NAME:sock_test_unix_dgram_cred>)

add_executable(sock_test_unix_bind_check test_unix_bind_check.c)
target_link_libraries(sock_test_unix_bind_check
  $<TARGET_NAME:nebase>
)
add_test(NAME sock_test_unix_bind_check_stream COMMAND $<TARGET_NAME:sock_test_unix_bind_check> "stream")
add_test(NAME sock_test_unix_bind_check_seqpacket COMMAND $<TARGET_NAME:sock_test_unix_bind_check> "seqpacket")
add_test(NAME sock_test_unix_bind_check_dgram COMMAND $<TARGET_NAME:sock_test_unix_bind_check> "dgram")

add_executable(sock_test_socketpair_pipefd test_socketpair_pipefd.c)
target_link_libraries(sock_test_socketpair_pipefd
  $<TARGET_NAME:nebase>
)
add_test(NAME sock_test_socketpair_pipefd_stream COMMAND $<TARGET_NAME:sock_test_socketpair_pipefd> "stream")
add_test(NAME sock_test_socketpair_pipefd_seqpacket COMMAND $<TARGET_NAME:sock_test_socketpair_pipefd> "seqpacket")
add_test(NAME sock_test_socketpair_pipefd_dgram COMMAND $<TARGET_NAME:sock_test_socketpair_pipefd> "dgram")

add_executable(sock_test_seqpacket_exact test_seqpacket_exact.c)
target_link_libraries(sock_test_seqpacket_exact
  $<TARGET_NAME:nebase>
)
add_test(NAME sock_test_seqpacket_exact COMMAND $<TARGET_NAME:sock_test_seqpacket_exact>)

add_executable(sock_test_socketpair_close test_socketpair_close.c)
target_link_libraries(sock_test_socketpair_close $<TARGET_NAME:nebase>)
add_test(NAME sock_test_socketpair_close COMMAND $<TARGET_NAME:sock_test_socketpair_close>)
