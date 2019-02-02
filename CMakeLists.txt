cmake_minimum_required(VERSION 3.3)
project(NeBase VERSION 0.1.1 LANGUAGES C)
enable_testing()

include(CTest)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
include(NeBaseFeatures)
include(NeBaseSetup)
include(NeBaseInstallDirs)
include(NeBaseLoadCCompiler)

include(cmake/feature.cmake)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)

find_package(PkgConfig REQUIRED)

if(WITH_SYSTEMD)
  pkg_check_modules(SYSTEMD REQUIRED libsystemd)
endif()
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
if(OS_LINUX)
  pkg_check_modules(LIBBSD REQUIRED libbsd)
endif()

include_directories(BEFORE include)

# for MacOS or other private headers
if(EXISTS "${CMAKE_SOURCE_DIR}/private/include")
  include_directories(BEFORE SYSTEM "${CMAKE_SOURCE_DIR}/private/include")
endif()

include(cmake/CompileTests/glog_strerr.cmake)

configure_file(options.h.in options.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/)

add_subdirectory(cmake)
add_subdirectory(include)
add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(test)
endif(BUILD_TESTING)

feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
