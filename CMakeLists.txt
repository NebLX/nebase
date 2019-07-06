cmake_minimum_required(VERSION 3.3)
project(NeBase VERSION 0.1.1 LANGUAGES C)
enable_testing()

include(CTest)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
include(NeBaseCompatLayer)
include(NeBaseFeatures)
include(NeBaseSetup)
include(NeBaseInstallDirs)
include(NeBaseLoadCCompiler)

include(cmake/feature.cmake)

find_package(PkgConfig REQUIRED)

if(WITH_SYSTEMD)
  pkg_check_modules(SYSTEMD REQUIRED libsystemd)
endif()
pkg_check_modules(GLIB2 REQUIRED glib-2.0)

include_directories(BEFORE include)

# for MacOS or other private headers
if(EXISTS "${CMAKE_SOURCE_DIR}/private/include")
  include_directories(BEFORE SYSTEM "${CMAKE_SOURCE_DIR}/private/include")
endif()

include(cmake/CompileTests/printf_strerr.cmake)
include(cmake/CompileTests/glog_strerr.cmake)

configure_file(options.h.in options.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/)

add_subdirectory(compat)
set(ExportCompatIncludes "")
foreach(_Inc IN LISTS NeBaseCompatIncludes)
  include_directories(BEFORE "${CMAKE_SOURCE_DIR}/include/nebase/compat/${_Inc}")
  list(APPEND ExportCompatIncludes "\${PACKAGE_PREFIX_DIR}/${CMAKE_INSTALL_INCLUDEDIR}/nebase/compat/${_Inc}")
endforeach()

add_subdirectory(cmake)
add_subdirectory(include)
add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(test)
endif(BUILD_TESTING)

feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
