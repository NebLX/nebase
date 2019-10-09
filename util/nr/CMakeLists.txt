
add_executable(nr main.c)
target_link_libraries(nr PRIVATE $<TARGET_NAME:nebase>)
