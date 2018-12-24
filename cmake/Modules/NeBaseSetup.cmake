
set(CMAKE_MINIMUM_REQUIRED_VERSION "3.3")
if(CMAKE_VERSION VERSION_LESS "${CMAKE_MINIMUM_REQUIRED_VERSION}")
  #
  # Current Minimal Version:
  #   3.3 IN_LIST in if command
  #       CMAKE_CROSSCOMPILING_EMULATOR
  # Planned Minimal Version:
  #   3.6 clang-tidy support
  #   3.7 xxx_EQUAL comparison operations in if command
  #   3.10 cppcheck support
  #   3.12 allow to set link libraries for OBJECT library
  #   3.13 new command target_link_directories and target_link_options
  #
  message(FATAL_ERROR "CMake version >= ${CMAKE_MINIMUM_REQUIRED_VERSION} is required")
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  set(OS_LINUX ON)
  add_definitions(-D_GNU_SOURCE)
  add_definitions(-D_FILE_OFFSET_BITS=64) # see feature_test_macros(7)
  if(WITH_HARDEN_FLAGS)
    add_definitions(-D_FORTIFY_SOURCE=2)
  endif(WITH_HARDEN_FLAGS)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD")
  set(OS_FREEBSD ON)
  set(OSTYPE_BSD ON)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "NetBSD")
  set(OS_NETBSD ON)
  set(OSTYPE_BSD ON)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "OpenBSD")
  set(OS_OPENBSD ON)
  set(OSTYPE_BSD ON)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "DragonFly")
  set(OS_DFLYBSD ON)
  set(OSTYPE_BSD ON)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  set(OS_DARWIN ON)
  add_definitions(-D_DARWIN_C_SOURCE)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "SunOS")
  set(OS_SOLARIS ON)
  set(CMAKE_LIBRARY_ARCHITECTURE "64") # currently we only support 64
  add_definitions(-D_LARGEFILE64_SOURCE) # see lfcompile64(7)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Haiku")
  set(OS_HAIKU ON)
  add_definitions(-D_BSD_SOURCE)
  include_directories(BEFORE SYSTEM "/system/develop/headers/bsd")
else()
  message(FATAL_ERROR "Unsupported Host System: ${CMAKE_HOST_SYSTEM_NAME}")
endif()

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# set -fPIC/-fPIE
#   cmake will add -fPIE to execute src files, if compiler complains about
# recompiling with -fPIC, just add a object library for that src first.
if(WITH_HARDEN_FLAGS)
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif(WITH_HARDEN_FLAGS)
