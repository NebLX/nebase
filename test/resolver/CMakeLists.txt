
add_executable(resolver_test_addr_to_arpa test_addr_to_arpa.c)
target_link_libraries(resolver_test_addr_to_arpa $<TARGET_NAME:nebase>)
add_test(NAME resolver_test_v4_addr_to_arpa COMMAND $<TARGET_NAME:resolver_test_addr_to_arpa> "223.5.5.5")
set_tests_properties(resolver_test_v4_addr_to_arpa
  PROPERTIES
    PASS_REGULAR_EXPRESSION "5.5.5.223.in-addr.arpa"
)
add_test(NAME resolver_test_v6_addr_to_arpa COMMAND $<TARGET_NAME:resolver_test_addr_to_arpa> "2001:bc8:47c0:142a::1")
set_tests_properties(resolver_test_v6_addr_to_arpa
  PROPERTIES
    PASS_REGULAR_EXPRESSION "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.a.2.4.1.0.c.7.4.8.c.b.0.1.0.0.2.ip6.arpa"
)
