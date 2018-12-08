#
#.rst:
# NebInstallDirs
# --------------
#
# Based on GNUInstallDirs, with the following changes:
#  1. add support for more OS, i.e. Solaris
#  2. no special for CMAKE_CROSSCOMPILING
#  3. remove INFODIR and MANDIR
#  4. make CMAKE_INSTALL_PREFIX default to OS official package dir

if(OS_LINUX)
  set(_NebInstallDirs_DEFAULT_INSTALL_PREFIX "/usr")
elseif(OS_FREEBSD)
  set(_NebInstallDirs_DEFAULT_INSTALL_PREFIX "/usr/local")
elseif(OS_NETBSD)
  set(_NebInstallDirs_DEFAULT_INSTALL_PREFIX "/usr/pkg")
elseif(OS_OPENBSD)
  set(_NebInstallDirs_DEFAULT_INSTALL_PREFIX "/usr/local")
elseif(OS_DFLYBSD)
  set(_NebInstallDirs_DEFAULT_INSTALL_PREFIX "/usr/local")
elseif(OS_DARWIN)
  set(_NebInstallDirs_DEFAULT_INSTALL_PREFIX "/opt/local")
elseif(OS_SOLARIS)
  set(_NebInstallDirs_DEFAULT_INSTALL_PREFIX "/usr")
elseif(OS_HAIKU)
  set(_NebInstallDirs_DEFAULT_INSTALL_PREFIX "/system")
else()
  set(_NebInstallDirs_DEFAULT_INSTALL_PREFIX "/usr/local")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${_NebInstallDirs_DEFAULT_INSTALL_PREFIX}" CACHE PATH "Install Prefix" FORCE)
endif()

# Convert a cache variable to PATH type
macro(_NebInstallDirs_cache_convert_to_path var description)
  get_property(_NebInstallDirs_cache_type CACHE ${var} PROPERTY TYPE)
  if(_NebInstallDirs_cache_type STREQUAL "UNINITIALIZED")
    file(TO_CMAKE_PATH "${${var}}" _NebInstallDirs_cmakepath)
    set_property(CACHE ${var} PROPERTY TYPE PATH)
    set_property(CACHE ${var} PROPERTY VALUE "${_NebInstallDirs_cmakepath}")
    set_property(CACHE ${var} PROPERTY HELPSTRING "${description}")
    unset(_NebInstallDirs_cmakepath)
  endif()
  unset(_NebInstallDirs_cache_type)
endmacro()

# Create a cache variable with default for a path.
macro(_NebInstallDirs_cache_path var default description)
  if(NOT DEFINED ${var})
    set(${var} "${default}" CACHE PATH "${description}")
  endif()
  _NebInstallDirs_cache_convert_to_path("${var}" "${description}")
endmacro()

# Create a cache variable with not default for a path, with a fallback
# when unset; used for entries slaved to other entries such as
# DATAROOTDIR.
macro(_NebInstallDirs_cache_path_fallback var default description)
  if(NOT ${var})
    set(${var} "" CACHE PATH "${description}")
    set(${var} "${default}")
  endif()
  _NebInstallDirs_cache_convert_to_path("${var}" "${description}")
endmacro()

# Installation directories
#

_NebInstallDirs_cache_path(CMAKE_INSTALL_BINDIR "bin"
  "User executables (bin)")
_NebInstallDirs_cache_path(CMAKE_INSTALL_SBINDIR "sbin"
  "System admin executables (sbin)")
_NebInstallDirs_cache_path(CMAKE_INSTALL_LIBEXECDIR "libexec"
  "Program executables (libexec)")
_NebInstallDirs_cache_path(CMAKE_INSTALL_SYSCONFDIR "etc"
  "Read-only single-machine data (etc)")
_NebInstallDirs_cache_path(CMAKE_INSTALL_LOCALSTATEDIR "var"
  "Modifiable single-machine data (var)")

# We check if the variable was manually set and not cached, in order to
# allow projects to set the values as normal variables before including
# NebInstallDirs to avoid having the entries cached or user-editable. It
# replaces the "if(NOT DEFINED CMAKE_INSTALL_XXX)" checks in all the
# other cases.
# If CMAKE_INSTALL_LIBDIR is defined, if _libdir_set is false, then the
# variable is a normal one, otherwise it is a cache one.
get_property(_libdir_set CACHE CMAKE_INSTALL_LIBDIR PROPERTY TYPE SET)
if(NOT DEFINED CMAKE_INSTALL_LIBDIR OR (_libdir_set
    AND DEFINED _NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX
    AND NOT "${_NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX}" STREQUAL "${CMAKE_INSTALL_PREFIX}"))
  # If CMAKE_INSTALL_LIBDIR is not defined, it is always executed.
  # Otherwise:
  #  * if _libdir_set is false it is not executed (meaning that it is
  #    not a cache variable)
  #  * if _NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX is not defined it is
  #    not executed
  #  * if _NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX and
  #    CMAKE_INSTALL_PREFIX are the same string it is not executed.
  #    _NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX is updated after the
  #    execution, of this part of code, therefore at the next inclusion
  #    of the file, CMAKE_INSTALL_LIBDIR is defined, and the 2 strings
  #    are equal, meaning that the if is not executed the code the
  #    second time.
  if(NOT DEFINED CMAKE_SIZEOF_VOID_P)
    message(FATAL_ERROR
      "Unable to determine default CMAKE_INSTALL_LIBDIR directory because no target architecture is known. "
      "Please enable at least one language before including NebInstallDirs.")
  endif()

  set(_LIBDIR_DEFAULT "lib")
  # Override this default 'lib' with 'lib64' if:
  #  - we are on Linux system but NOT cross-compiling
  #  - we are NOT on debian
  #  - we are on a 64 bits system
  # reason is: amd64 ABI: https://github.com/hjl-tools/x86-psABI/wiki/X86-psABI
  # For Debian with multiarch, use 'lib/${CMAKE_LIBRARY_ARCHITECTURE}' if
  # CMAKE_LIBRARY_ARCHITECTURE is set (which contains e.g. "i386-linux-gnu"
  # and CMAKE_INSTALL_PREFIX is "/usr"
  # See http://wiki.debian.org/Multiarch
  if(DEFINED _NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX)
    set(__LAST_LIBDIR_DEFAULT "lib")
    # __LAST_LIBDIR_DEFAULT is the default value that we compute from
    # _NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX, not a cache entry for
    # the value that was last used as the default.
    # This value is used to figure out whether the user changed the
    # CMAKE_INSTALL_LIBDIR value manually, or if the value was the
    # default one. When CMAKE_INSTALL_PREFIX changes, the value is
    # updated to the new default, unless the user explicitly changed it.
  endif()
  if(OS_LINUX)
    if (EXISTS "/etc/debian_version") # is this a debian system ?
      if(CMAKE_LIBRARY_ARCHITECTURE)
        if("${CMAKE_INSTALL_PREFIX}" MATCHES "^/usr/?$")
          set(_LIBDIR_DEFAULT "lib/${CMAKE_LIBRARY_ARCHITECTURE}")
        endif()
        if(DEFINED _NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX
            AND "${_NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX}" MATCHES "^/usr/?$")
          set(__LAST_LIBDIR_DEFAULT "lib/${CMAKE_LIBRARY_ARCHITECTURE}")
        endif()
      endif()
    else() # not debian, rely on CMAKE_SIZEOF_VOID_P:
      if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        set(_LIBDIR_DEFAULT "lib64")
        if(DEFINED _NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX)
          set(__LAST_LIBDIR_DEFAULT "lib64")
        endif()
      endif()
    endif()
  elseif(OS_SOLARIS)
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
      set(_LIBDIR_DEFAULT "lib/64")
      if(DEFINED _NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX)
        set(__LAST_LIBDIR_DEFAULT "lib/64")
      endif()
    endif()
  endif()
  if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "${_LIBDIR_DEFAULT}" CACHE PATH "Object code libraries (${_LIBDIR_DEFAULT})")
  elseif(DEFINED __LAST_LIBDIR_DEFAULT
      AND "${__LAST_LIBDIR_DEFAULT}" STREQUAL "${CMAKE_INSTALL_LIBDIR}")
    set_property(CACHE CMAKE_INSTALL_LIBDIR PROPERTY VALUE "${_LIBDIR_DEFAULT}")
  endif()
endif()
_NebInstallDirs_cache_convert_to_path(CMAKE_INSTALL_LIBDIR "Object code libraries (lib)")

# Save for next run
set(_NebInstallDirs_LAST_CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE INTERNAL "CMAKE_INSTALL_PREFIX during last run")
unset(_libdir_set)
unset(__LAST_LIBDIR_DEFAULT)

_NebInstallDirs_cache_path(CMAKE_INSTALL_INCLUDEDIR "include"
  "C header files (include)")
_NebInstallDirs_cache_path(CMAKE_INSTALL_OLDINCLUDEDIR "/usr/include"
  "C header files for non-gcc (/usr/include)")
_NebInstallDirs_cache_path(CMAKE_INSTALL_DATAROOTDIR "share"
  "Read-only architecture-independent data root (share)")

#-----------------------------------------------------------------------------
# Values whose defaults are relative to DATAROOTDIR.  Store empty values in
# the cache and store the defaults in local variables if the cache values are
# not set explicitly.  This auto-updates the defaults as DATAROOTDIR changes.

_NebInstallDirs_cache_path_fallback(CMAKE_INSTALL_DATADIR "${CMAKE_INSTALL_DATAROOTDIR}"
  "Read-only architecture-independent data (DATAROOTDIR)")

_NebInstallDirs_cache_path_fallback(CMAKE_INSTALL_LOCALEDIR "${CMAKE_INSTALL_DATAROOTDIR}/locale"
  "Locale-dependent data (DATAROOTDIR/locale)")
_NebInstallDirs_cache_path_fallback(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_NAME}"
  "Documentation root (DATAROOTDIR/doc/PROJECT_NAME)")

_NebInstallDirs_cache_path_fallback(CMAKE_INSTALL_RUNSTATEDIR "${CMAKE_INSTALL_LOCALSTATEDIR}/run"
  "Run-time variable data (LOCALSTATEDIR/run)")

#-----------------------------------------------------------------------------

mark_as_advanced(
  CMAKE_INSTALL_BINDIR
  CMAKE_INSTALL_SBINDIR
  CMAKE_INSTALL_LIBEXECDIR
  CMAKE_INSTALL_SYSCONFDIR
  CMAKE_INSTALL_LOCALSTATEDIR
  CMAKE_INSTALL_RUNSTATEDIR
  CMAKE_INSTALL_LIBDIR
  CMAKE_INSTALL_INCLUDEDIR
  CMAKE_INSTALL_OLDINCLUDEDIR
  CMAKE_INSTALL_DATAROOTDIR
  CMAKE_INSTALL_DATADIR
  CMAKE_INSTALL_LOCALEDIR
  CMAKE_INSTALL_DOCDIR
  )

macro(NebInstallDirs_get_absolute_install_dir absvar var)
  if(NOT IS_ABSOLUTE "${${var}}")
    # Handle special cases:
    # - CMAKE_INSTALL_PREFIX == /
    # - CMAKE_INSTALL_PREFIX == /usr
    # - CMAKE_INSTALL_PREFIX == /opt/...
    if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/")
      if("${dir}" STREQUAL "SYSCONFDIR" OR "${dir}" STREQUAL "LOCALSTATEDIR" OR "${dir}" STREQUAL "RUNSTATEDIR")
        set(${absvar} "/${${var}}")
      else()
        if (NOT "${${var}}" MATCHES "^usr/")
          set(${var} "usr/${${var}}")
        endif()
        set(${absvar} "/${${var}}")
      endif()
    elseif("${CMAKE_INSTALL_PREFIX}" MATCHES "^/usr/?$")
      if("${dir}" STREQUAL "SYSCONFDIR" OR "${dir}" STREQUAL "LOCALSTATEDIR" OR "${dir}" STREQUAL "RUNSTATEDIR")
        set(${absvar} "/${${var}}")
      else()
        set(${absvar} "${CMAKE_INSTALL_PREFIX}/${${var}}")
      endif()
    elseif("${CMAKE_INSTALL_PREFIX}" MATCHES "^/opt/.*")
      if("${dir}" STREQUAL "SYSCONFDIR" OR "${dir}" STREQUAL "LOCALSTATEDIR" OR "${dir}" STREQUAL "RUNSTATEDIR")
        set(${absvar} "/${${var}}${CMAKE_INSTALL_PREFIX}")
      else()
        set(${absvar} "${CMAKE_INSTALL_PREFIX}/${${var}}")
      endif()
    else()
      set(${absvar} "${CMAKE_INSTALL_PREFIX}/${${var}}")
    endif()
  else()
    set(${absvar} "${${var}}")
  endif()
endmacro()

# Result directories
#
foreach(dir
    BINDIR
    SBINDIR
    LIBEXECDIR
    SYSCONFDIR
    SHAREDSTATEDIR
    LOCALSTATEDIR
    RUNSTATEDIR
    LIBDIR
    INCLUDEDIR
    OLDINCLUDEDIR
    DATAROOTDIR
    DATADIR
    LOCALEDIR
    DOCDIR
    )
  NebInstallDirs_get_absolute_install_dir(CMAKE_INSTALL_FULL_${dir} CMAKE_INSTALL_${dir})
endforeach()

link_directories(${CMAKE_INSTALL_FULL_LIBDIR})
