
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(NeBase_C_FLAGS "")
set(NeBase_C_HARDEN_FLAGS "")

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "5.1")
    message(SEND_ERROR "GCC version >= 5.1 is required")
  endif()

  set(NeBase_C_FLAGS "${CC_SPECIFIC_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Werror=format-security")

  if(WITH_HARDEN_FLAGS)
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -fstack-protector-strong")
    if(OS_LINUX) # for glibc
      set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -D_FORTIFY_SOURCE=2")
    endif()
  endif()

  if(COMPAT_CODE_COVERAGE)
    set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -fprofile-arcs -ftest-coverage -O0")
  endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "3.5")
    message(SEND_ERROR "Clang version >= 3.5 is required")
  endif()

  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Werror=format-security")

  if(WITH_HARDEN_FLAGS)
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -fstack-protector-strong")
    if(OS_LINUX) # for glibc
      set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -D_FORTIFY_SOURCE=2")
    endif()
  endif()

 if(COMPAT_CODE_COVERAGE)
    # Code Coverage in LLVM < 3.7 is compitable with gcov, use gcc instead
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS "3.7")
      message(SEND_ERROR "No code coverage support with clang < 3.7")
    else()
      set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping -O0")
    endif()
  endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "SunPro")
  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "5.14")
    message(SEND_ERROR "SunPro CC version >= 5.14 (Oracle Developer Studio >= 12.5) is required")
  endif()

  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -m64 -features=zla,no%extinl -errtags")
  set(SUNPRO_ERROFF_BASIC "E_ATTRIBUTE_UNKNOWN,E_EMPTY_INITIALIZER,E_STATEMENT_NOT_REACHED,E_END_OF_LOOP_CODE_NOT_REACHED")
  set(SUNPRO_ERROFF_EXTRA "E_ASSIGNMENT_TYPE_MISMATCH,E_INITIALIZATION_TYPE_MISMATCH")
  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -erroff=%none,${SUNPRO_ERROFF_BASIC},${SUNPRO_ERROFF_EXTRA}")
  set(NeBase_CC_USE_WL ON)

  if(WITH_HARDEN_FLAGS)
    if(OS_SOLARIS) # stack overflow support only on Solaris
      set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -xcheck=stkovf")
    endif()
  endif()

  if(COMPAT_CODE_COVERAGE)
    message(SEND_ERROR "No code coverage support with Oracle Developer Studio C Compiler")
  endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "Intel")
  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "18.0")
    message(SEND_ERROR "ICC version >= 18.0 is required")
  endif()

  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} -intel-extensions")
  set(INTEL_WARNOFF_FLAGS "-Wno-attributes")
  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} ${INTEL_WARNOFF_FLAGS}")

  if(WITH_HARDEN_FLAGS)
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -fstack-protector-strong")
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -D_FORTIFY_SOURCE=2")
  endif()

  if(COMPAT_CODE_COVERAGE)
    message(SEND_ERROR "No code coverage support with Intel C++ Compiler")
  endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "XL")
  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "16.1")
    message(SEND_ERROR "XL C version >= 16.1 is required")
  endif()

  set(IBMXL_WARNOFF_FLAGS "-Wno-attributes")
  set(NeBase_C_FLAGS "${NeBase_C_FLAGS} ${IBMXL_WARNOFF_FLAGS}")
  set(NeBase_CC_USE_WL ON)

  if(WITH_HARDEN_FLAGS)
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -qstackprotect=strong")
    set(NeBase_C_HARDEN_FLAGS "${NeBase_C_HARDEN_FLAGS} -D_FORTIFY_SOURCE=2")
  endif()

  if(COMPAT_CODE_COVERAGE)
    message(SEND_ERROR "No code coverage support with IBM XL C Compiler")
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
