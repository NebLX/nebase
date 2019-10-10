
cmake_policy(PUSH)
cmake_policy(SET CMP0056 NEW)
cmake_policy(SET CMP0057 NEW)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

#
# Set C Compile Flags
#

set(NeBase_C_FLAGS "")
set(NeBase_C_HARDEN_FLAGS "")
set(NeBase_LINK_PIE_FLAG "-pie")

if(COMPAT_CODE_COVERAGE)
  if(CMAKE_BUILD_TYPE NOT EQUAL "Debug")
    message(SEND_ERROR "Code coverage is only supported in Debug mode")
  endif()
endif(COMPAT_CODE_COVERAGE)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")

  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "4.9")
    message(SEND_ERROR "GCC version >= 4.9 is required")
  endif()

  if(OSTYPE_SUN)
    # force to use m64 for SunOS
    set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -m64")
  endif()
  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Werror=format-security")

  if(WITH_HARDEN_FLAGS)
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -fstack-protector-strong")
  endif()

  if(COMPAT_CODE_COVERAGE)
    set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -fprofile-arcs -ftest-coverage -O0")
    # analyze with gcov/lcov/gcovr
  endif()

elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")

  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "3.5")
    message(SEND_ERROR "Clang version >= 3.5 is required")
  endif()

  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Werror=format-security")

  if(WITH_HARDEN_FLAGS)
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -fstack-protector-strong")
  endif()

  if(COMPAT_CODE_COVERAGE)
    # Code Coverage in LLVM < 3.7 is compitable with gcov, use gcc instead
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS "3.7")
      message(SEND_ERROR "No code coverage support with clang < 3.7")
    else()
      set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping -O0")
      # analyze with llvm-cov
    endif()
  endif()

elseif(CMAKE_C_COMPILER_ID STREQUAL "SunPro")

  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "5.14")
    message(SEND_ERROR "SunPro CC version >= 5.14 (Oracle Developer Studio >= 12.5) is required")
  endif()

  if(OSTYPE_SUN)
    # force to use m64 for SunOS
    set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -m64")
  endif()
  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -features=zla,no%extinl -errtags")
  set(SUNPRO_ERROFF_BASIC "E_ATTRIBUTE_UNKNOWN,E_EMPTY_INITIALIZER,E_STATEMENT_NOT_REACHED,E_END_OF_LOOP_CODE_NOT_REACHED")
  set(SUNPRO_ERROFF_EXTRA "E_ASSIGNMENT_TYPE_MISMATCH,E_INITIALIZATION_TYPE_MISMATCH")
  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -erroff=%none,${SUNPRO_ERROFF_BASIC},${SUNPRO_ERROFF_EXTRA}")

  if(WITH_HARDEN_FLAGS)
    if(OS_SOLARIS) # stack overflow support only on Solaris
      set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -xcheck=stkovf")
    endif()
  endif()

  if(COMPAT_CODE_COVERAGE)
    # do nothing, as only '-g' is needed
    # analyze with uncover
  endif()

elseif(CMAKE_C_COMPILER_ID STREQUAL "Intel")
  # Usage:
  #  $ . <install-dir>/bin/compilervars.sh -arch <arch> -platform <platform>
  #  $ export CC=icc
  #  $ export LD=xild
  #  $ export AR=xiar

  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "18.0")
    message(SEND_ERROR "ICC version >= 18.0 is required")
  elseif(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "19.0.5")
    message("You may want to set -qnextgen flag to use (LLVM based) ICC NextGen")
  endif()

  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -intel-extensions")
  set(INTEL_WARNOFF_FLAGS "-Wno-attributes")
  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} ${INTEL_WARNOFF_FLAGS}")

  # use initial-exec/global-dynamic as tls model
  # "local-dynamic" shouldn't be used as it cause pie tls symbol lookup to fail
  message(STATUS "Set TLS model to initial-exec, which is restrictive and optimized")
  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -ftls-model=initial-exec")

  if(WITH_HARDEN_FLAGS)
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -fstack-protector-strong")
  endif()

  if(COMPAT_CODE_COVERAGE)
    set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -prof-gen=srcpos -O0")
    # analyze with codecov
  endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "PGI")
  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "15.0")
    # NOTE _Atomic is still not supported(at least 18.10)
    #      see <version dir>/include/_c_macros.h
    message(SEND_ERROR "PGCC version > 15.0 is required")
  endif()

  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -Minform=warn")
  # TODO

elseif(CMAKE_C_COMPILER_ID STREQUAL "XL")
  # TODO test clang based xlc and original xlc with cmake >= 3.15

  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "16.1")
    message(SEND_ERROR "XL C version >= 16.1 is required")
  endif()

  set(IBMXL_WARNOFF_FLAGS "-Wno-attributes")
  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} ${IBMXL_WARNOFF_FLAGS}")

  if(WITH_HARDEN_FLAGS)
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -qstackprotect=strong")
  endif()

  if(COMPAT_CODE_COVERAGE)
    message(SEND_ERROR "No code coverage support with IBM XL C Compiler")
    # no command line support for at least 16.1.1
  endif()

elseif(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")

  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "7.0.0")
    # for the coresponding llvm version, see src/CMakeLists.txt in
    #    https://opensource.apple.com/source/clang/
    message(SEND_ERROR "Clang version >= 7.0.0 (based on llvm 3.7) is required")
  endif()

  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Werror=format-security")

  if(WITH_HARDEN_FLAGS)
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -fstack-protector-strong")
  endif()

  if(COMPAT_CODE_COVERAGE)
    set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping -O0")
  endif()

else()
  message(SEND_ERROR "Unsupported C Compiler")
endif()

set(CMAKE_C_FLAGS "-O3 ${NeBase_C_FLAGS} ${CMAKE_C_FLAGS}")
if(WITH_HARDEN_FLAGS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NeBase_C_HARDEN_FLAGS}")
endif(WITH_HARDEN_FLAGS)

# Generate Position-independent code
if(WITH_HARDEN_FLAGS)
  # set -pie while linking exe
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.14")
    include(CheckPIESupported)
    check_pie_supported(OUTPUT_VARIABLE _pie_output LANGUAGES C)
    if(NOT CMAKE_C_LINK_PIE_SUPPORTED)
      message(WARNING "PIE is not supported at link time: ${_pie_output}.\n"
                      "PIE link options will not be passed to linker.")
    endif()
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${NeBase_LINK_PIE_FLAG}")
  endif()

  # this will set -fPIC/-fPIE according to the target type
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif(WITH_HARDEN_FLAGS)

#
# Detect pthread, and export NebulaX::Threads
#

set(OLD_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "${NeBase_C_FLAGS} ${NeBase_C_HARDEN_FLAGS}")
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
if(NOT TARGET NebulaX::Threads)
  add_library(NebulaX::Threads INTERFACE IMPORTED GLOBAL)
  target_link_libraries(NebulaX::Threads INTERFACE Threads::Threads)
endif()
set(CMAKE_C_FLAGS "${OLD_CMAKE_C_FLAGS}")

#
# Set ld link flags
#

macro(_NebLoadCCompiler_test_ld_flag _flag)
  set(SAFE_CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET})
  set(SAFE_CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
  set(CMAKE_REQUIRED_QUIET ON)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_flag}")
  include(CheckCCompilerFlag)
  unset(NeBase_LD_FLAG_OK CACHE)
  check_c_compiler_flag("${_flag}" NeBase_LD_FLAG_OK)
  set(CMAKE_REQUIRED_QUIET ${SAFE_CMAKE_REQUIRED_QUIET})
  set(CMAKE_EXE_LINKER_FLAGS "${SAFE_CMAKE_EXE_LINKER_FLAGS}")
endmacro(_NebLoadCCompiler_test_ld_flag)

macro(_NebLoadCCompiler_get_ld_option _opt)
  set(NeBase_LD_REAL_FLAG "${_opt}")
  _NebLoadCCompiler_test_ld_flag(${NeBase_LD_REAL_FLAG})
  if(NOT NeBase_LD_FLAG_OK)
    set(NeBase_LD_REAL_FLAG "-Wl,${_opt}")
    _NebLoadCCompiler_test_ld_flag(${NeBase_LD_REAL_FLAG})
    if(NOT NeBase_LD_FLAG_OK)
      set(NeBase_LD_REAL_FLAG "")
    endif()
  endif()
  if(NeBase_LD_REAL_FLAG)
    message(STATUS "Linker flag for ${_opt}: ${NeBase_LD_REAL_FLAG}")
  else()
    message(STATUS "Linker flag for ${_opt}: unsupported")
  endif()
endmacro(_NebLoadCCompiler_get_ld_option)

macro(_NebLoadCCompiler_get_ld_z_option)
  set(NeBase_LD_REAL_FLAG "-z ${_opt}")
  _NebLoadCCompiler_test_ld_flag(${NeBase_LD_REAL_FLAG})
  if(NOT NeBase_LD_FLAG_OK)
    set(NeBase_LD_REAL_FLAG "-Wl,z,${_opt}")
    _NebLoadCCompiler_test_ld_flag(${NeBase_LD_REAL_FLAG})
    if(NOT NeBase_LD_FLAG_OK)
      set(NeBase_LD_REAL_FLAG "")
    endif()
  endif()
  if(NeBase_LD_REAL_FLAG)
    message(STATUS "Linker flag for -z ${_opt}: ${NeBase_LD_REAL_FLAG}")
  else()
    message(STATUS "Linker flag for -z ${_opt}: unsupported")
  endif()
endmacro(_NebLoadCCompiler_get_ld_z_option)

macro(_NebLoadCCompiler_set_linker_flag)
  include(NeBaseDetectLinker)

  set(NeBase_LD_FLAGS "")
  set(NeBase_LD_HARDEN_FLAGS "")

  set(NeBase_LD_OPTIONS "--xxx-assert-failed;--as-needed")

  set(GNU_COMPATIBLE_LINKERS "GNU.bfd;GNU.gold;LLVM.lld")
  if(NeBase_LINKER_ID IN_LIST GNU_COMPATIBLE_LINKERS)
    set(NeBase_LD_Z_HARDEN_KEYWORDS "relro;now")
  elseif(NeBase_LINKER_ID STREQUAL "SUN.ld")
    set(NeBase_LD_Z_HARDEN_KEYWORDS "nodeferred")
  endif()

  foreach(_opt IN LISTS NeBase_LD_OPTIONS)
    _NebLoadCCompiler_get_ld_option(${_opt})
    set(NeBase_LD_FLAGS "${NeBase_LD_FLAGS} ${NeBase_LD_REAL_FLAG}")
  endforeach()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${NeBase_LD_FLAGS}")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${NeBase_LD_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${NeBase_LD_FLAGS}")

  if(WITH_HARDEN_FLAGS)
    foreach(_opt IN LISTS NeBase_LD_Z_HARDEN_KEYWORDS)
      _NebLoadCCompiler_get_ld_z_option(${_opt})
      set(NeBase_LD_HARDEN_FLAGS "${NeBase_LD_HARDEN_FLAGS} ${NeBase_LD_REAL_FLAG}")
    endforeach()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${NeBase_LD_HARDEN_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${NeBase_LD_HARDEN_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${NeBase_LD_HARDEN_FLAGS}")
  endif(WITH_HARDEN_FLAGS)
endmacro(_NebLoadCCompiler_set_linker_flag)

_NebLoadCCompiler_set_linker_flag()

#
# Setup static analyzers
#

if(WITH_CLANG_TIDY)
  set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}" "-header-filter=.*")
endif(WITH_CLANG_TIDY)

if(WITH_CPPCHECK)
  set(CMAKE_C_CPPCHECK "${CPPCHECK_EXE}" "--language=c" "--std=c11" "--enable=warning,performance")
endif(WITH_CPPCHECK)

cmake_policy(POP)
