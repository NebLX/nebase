
set(WITH_SYSTEMD_DESC "Build with systemd support")
option(WITH_SYSTEMD ${WITH_SYSTEMD_DESC} ON)
if(OS_LINUX)
  add_feature_info(WITH_SYSTEMD WITH_SYSTEMD ${WITH_SYSTEMD_DESC})
else()
  set(WITH_SYSTEMD OFF)
endif()
