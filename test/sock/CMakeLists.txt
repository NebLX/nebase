
add_executable(sock_test_socketpair_cred test_socketpair_cred.c)
target_link_libraries(sock_test_socketpair_cred
  $<TARGET_NAME:nebase>
)
add_test(NAME sock_test_socketpair_cred_stream COMMAND $<TARGET_NAME:sock_test_socketpair_cred> "stream")
add_test(NAME sock_test_socketpair_cred_seqpacket COMMAND $<TARGET_NAME:sock_test_socketpair_cred> "seqpacket")
add_test(NAME sock_test_socketpair_cred_dgram COMMAND $<TARGET_NAME:sock_test_socketpair_cred> "dgram")
