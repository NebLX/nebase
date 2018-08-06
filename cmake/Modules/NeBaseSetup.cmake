
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  set(OS_LINUX ON)
  set(DEFAULT_INSTALL_PREFIX "/usr")
  add_definitions(-D_GNU_SOURCE)
  add_definitions(-D_FILE_OFFSET_BITS=64) # see feature_test_macros(7)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD")
  set(OS_FREEBSD ON)
  set(DEFAULT_INSTALL_PREFIX "/usr/local")
else()
  message(FATAL_ERROR "Unsupported Host System: ${CMAKE_HOST_SYSTEM_NAME}")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${DEFAULT_INSTALL_PREFIX}" CACHE PATH "Install Prefix" FORCE)
endif()

include(GNUInstallDirs)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
link_directories(${CMAKE_INSTALL_FULL_LIBDIR})
