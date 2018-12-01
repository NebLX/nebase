
include(FeatureSummary)

set(WITH_DEBUG_LOG_DESC "Build with debug info")
option(WITH_DEBUG_LOG ${WITH_DEBUG_LOG_DESC} OFF)
add_feature_info(WITH_DEBUG_LOG WITH_DEBUG_LOG ${WITH_DEBUG_LOG_DESC})

set(WITH_HARDEN_FLAGS_DESC "Build with security hardening flags")
option(WITH_HARDEN_FLAGS ${WITH_HARDEN_FLAGS_DESC} ON)
add_feature_info(WITH_HARDEN_FLAGS WITH_HARDEN_FLAGS ${WITH_HARDEN_FLAGS_DESC})

set(WITH_CLANG_TIDY_DESC "Do static analysis with clang-tidy")
option(WITH_CLANG_TIDY ${WITH_CLANG_TIDY_DESC} ON)
find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy exectuable")
if(NOT CLANG_TIDY_EXE)
  message(STATUS "clang-tidy not found, disabling integration")
  set(WITH_CLANG_TIDY OFF)
else()
  message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
endif()
add_feature_info(WITH_CLANG_TIDY WITH_CLANG_TIDY ${WITH_CLANG_TIDY_DESC})

set(COMPAT_CODE_COVERAGE_DESC "Build with code coverage test compile options")
option(COMPAT_CODE_COVERAGE ${COMPAT_CODE_COVERAGE_DESC} OFF)
add_feature_info(COMPAT_CODE_COVERAGE COMPAT_CODE_COVERAGE ${COMPAT_CODE_COVERAGE_DESC})
