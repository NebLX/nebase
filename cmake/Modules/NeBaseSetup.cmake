
set(CMAKE_MINIMUM_REQUIRED_VERSION "3.7")
if(CMAKE_VERSION VERSION_LESS "${CMAKE_MINIMUM_REQUIRED_VERSION}")
  #
  #   3.3 IN_LIST in if command
  #       CMAKE_CROSSCOMPILING_EMULATOR
  #   3.6 clang-tidy support
  #       IMPORTED_TARGET support in PkgConfig Module
  #         NOTE target_link_libraries(INTERFACE) won't export include_directories
  #              target_include_directories(INTERFACE) should be used
  # Current Minimal Version:
  #   3.7 xxx_EQUAL comparison operations in if command
  # Planned Minimal Version:
  #   3.10 cppcheck support
  #   3.11 RHEL 8 version
  #   3.12 allow to set link libraries for OBJECT library
  #        $<TARGET_EXISTS:...> generator expression added
  #   3.13 new command:
  #          - add_link_options
  #          - target_link_directories
  #          - target_link_options
  #   3.14 CMAKE_BUILD_RPATH_USE_ORIGIN this enable relative RPATHs
  #        CheckPIESupported which check and include -pie as linker flags
  #   3.15 IBM Clang-based XL compilers that define __ibmxl__ now use the
  #        compiler id XLClang instead of XL
  #   3.18 CheckLinkerFlag to check validity of link flags
  #        file(CONFIGURE) added to `configure_file` with no file no disk
  #        The find_program/library/path/file commands gained a new REQUIRED option
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
  include(NeBaseLoadOSRelease)
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
  set(OSTYPE_SUN ON)
  include(NeBaseLoadOSRelease)
  if(OS_RELEASE_ID STREQUAL "solaris")
    set(OS_SOLARIS ON)
  else()
    set(OS_ILLUMOS ON)
    add_definitions(-D_XOPEN_SOURCE=700) # Open Group Technical Standard, Issue 7
    add_definitions(-D__EXTENSIONS__)
  endif()
  set(CMAKE_LIBRARY_ARCHITECTURE "64") # currently we only support 64
  add_definitions(-D_LARGEFILE64_SOURCE) # see lfcompile64(7)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Haiku")
  set(OS_HAIKU ON)
  add_definitions(-D_BSD_SOURCE)
  include_directories(BEFORE SYSTEM "/system/develop/headers/bsd")
else()
  message(FATAL_ERROR "Unsupported Host System: ${CMAKE_HOST_SYSTEM_NAME}")
endif()

# use rpath
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# do not use default module prefix, set them in target name
set(CMAKE_SHARED_MODULE_PREFIX "")
