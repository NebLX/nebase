cmake_minimum_required(VERSION 3.3)
project(NeBase VERSION 0.1 LANGUAGES C)
enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
include(NeBaseSetup)

include(FeatureSummary)
include(cmake/feature.cmake)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)

if(WITH_SYSTEMD)
  pkg_check_modules(SYSTEMD REQUIRED libsystemd)
endif()

include_directories(BEFORE include)

configure_file(options.h.in options.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(cmake)
add_subdirectory(include)
add_subdirectory(src)

feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
