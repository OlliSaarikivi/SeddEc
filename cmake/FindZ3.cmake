set(Z3_ROOT "" CACHE PATH "Root of Z3 distribution.")

find_path(Z3_INCLUDE_DIR NAMES z3.h z3++.h PATHS ${Z3_ROOT}/include NO_DEFAULT_PATH)
find_library(Z3_LIBRARY NAMES z3 libz3 PATHS ${Z3_ROOT}/lib ${Z3_ROOT}/bin NO_DEFAULT_PATH)

if( WIN32 )
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
else()
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
endif()
find_library(Z3_SHARED_LIBRARY NAMES z3 libz3 PATHS ${Z3_ROOT}/lib ${Z3_ROOT}/bin NO_DEFAULT_PATH)

find_program (Z3_EXECUTABLE
  NAMES z3 PATHS ${Z3_ROOT} PATH_SUFFIXES bin 
  DOC "z3 command line executable" NO_DEFAULT_PATH)

if (Z3_EXECUTABLE)
  execute_process (COMMAND ${Z3_EXECUTABLE} -version
    OUTPUT_VARIABLE z3_version
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if (z3_version MATCHES "^Z3 version [0-9]")
    string (REPLACE "Z3 version " "" Z3_VERSION_STRING ${z3_version})
  endif()
endif()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(Z3
  REQUIRED_VARS Z3_LIBRARY Z3_INCLUDE_DIR Z3_EXECUTABLE
  VERSION_VAR Z3_VERSION_STRING)