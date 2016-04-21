cmake_minimum_required (VERSION 2.6)
project (SeddEC)
set(VERSION_MAJOR_FROM_CMAKE 0)
set(VERSION_MINOR_FROM_CMAKE 1)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

configure_file ("src/Version.hpp.in" "${PROJECT_BINARY_DIR}/Version.hpp")
include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_BINARY_DIR}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

file(GLOB seddec_SRC
    "src/*.cpp"
)
add_executable(seddec ${seddec_SRC})

find_package(Z3 4.4.1 REQUIRED)
include_directories(${Z3_INCLUDE_DIR})
target_link_libraries(seddec ${Z3_LIBRARY})

#find_package(Boost)
#include_directories(${Boost_INCLUDE_DIRS})

# Enables precompiled headers
#include(cotire)
#set_target_properties(seddec PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/Pch.hpp")
#cotire(seddec)

add_custom_command(TARGET seddec PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${PROJECT_SOURCE_DIR}/res $<TARGET_FILE_DIR:seddec>)

add_custom_command(TARGET seddec PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		${Z3_SHARED_LIBRARY} $<TARGET_FILE_DIR:seddec>)
