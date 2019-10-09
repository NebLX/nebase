
add_executable(nh main.c query.c)
target_link_libraries(nh
PRIVATE
  $<TARGET_NAME:nebase>
  $<$<BOOL:${WITH_CARES}>:PkgConfig::CARES>
)
