
include(CheckIncludeFile)

CHECK_INCLUDE_FILE("sys/cdefs.h" ALL_SYS_CDEFS_H)
if(ALL_SYS_CDEFS_H)
  set(INCLUDE_ALL_SYS_CDEFS_H "#include <sys/cdefs.h>")
endif()
CHECK_INCLUDE_FILE("TargetConditionals.h" MACOS_TARGET_CONDITIONALS_H)
if(MACOS_TARGET_CONDITIONALS_H)
  set(INCLUDE_MACOS_TARGET_CONDITIONALS_H "#include <TargetConditionals.h>")
endif()
configure_file(platform.h.in platform.h @ONLY)

install(DIRECTORY nebase DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/platform.h" DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}/nebase")
