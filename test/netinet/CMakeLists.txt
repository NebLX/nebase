
add_executable(netinet_test_addr_to_arpa test_addr_to_arpa.c)
target_link_libraries(netinet_test_addr_to_arpa $<TARGET_NAME:nebase>)
add_test(NAME netinet_test_v4_addr_to_arpa COMMAND $<TARGET_NAME:netinet_test_addr_to_arpa> "223.5.5.5")
set_tests_properties(netinet_test_v4_addr_to_arpa
  PROPERTIES
    PASS_REGULAR_EXPRESSION "5.5.5.223.in-addr.arpa"
)
add_test(NAME netinet_test_v6_addr_to_arpa COMMAND $<TARGET_NAME:netinet_test_addr_to_arpa> "2001:bc8:47c0:142a::1")
set_tests_properties(netinet_test_v6_addr_to_arpa
  PROPERTIES
    PASS_REGULAR_EXPRESSION "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.a.2.4.1.0.c.7.4.8.c.b.0.1.0.0.2.ip6.arpa"
)

add_executable(netinet_test_prefix_to_netmask test_prefix_to_netmask.c)
target_link_libraries(netinet_test_prefix_to_netmask $<TARGET_NAME:nebase>)
add_test(NAME netinet_test_v4_netmask_24 COMMAND $<TARGET_NAME:netinet_test_prefix_to_netmask> "24")
set_tests_properties(netinet_test_v4_netmask_24
  PROPERTIES
    PASS_REGULAR_EXPRESSION "255.255.255.0"
)
add_test(NAME netinet_test_v4_netmask_25 COMMAND $<TARGET_NAME:netinet_test_prefix_to_netmask> "25")
set_tests_properties(netinet_test_v4_netmask_25
  PROPERTIES
    PASS_REGULAR_EXPRESSION "255.255.255.128"
)

add_executable(netinet_test_v4_net_pton test_net4_pton.c)
target_link_libraries(netinet_test_v4_net_pton $<TARGET_NAME:nebase>)
add_test(NAME netinet_test_v4_net_pton COMMAND $<TARGET_NAME:netinet_test_v4_net_pton>)

add_executable(netinet_test_v6_net_pton test_net6_pton.c)
target_link_libraries(netinet_test_v6_net_pton $<TARGET_NAME:nebase>)
add_test(NAME netinet_test_v6_net_pton COMMAND $<TARGET_NAME:netinet_test_v6_net_pton>)

add_executable(netinet_test_v4_net_radix test_net4_radix.c)
target_link_libraries(netinet_test_v4_net_radix $<TARGET_NAME:nebase>)
add_test(NAME netinet_test_v4_net_radix COMMAND $<TARGET_NAME:netinet_test_v4_net_radix>)

add_executable(netinet_test_v6_net_radix test_net6_radix.c)
target_link_libraries(netinet_test_v6_net_radix $<TARGET_NAME:nebase>)
add_test(NAME netinet_test_v6_net_radix COMMAND $<TARGET_NAME:netinet_test_v6_net_radix>)
