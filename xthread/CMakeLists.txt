#Project xthread
#Author:

cmake_minimum_required (VERSION 2.8)
project (xthread C CXX)

MESSAGE(STATUS "ProjectName: ${PROJECT_NAME}")
MESSAGE(STATUS "Project_Source_Dir: ${PROJECT_SOURCE_DIR}")

enable_testing()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CXX_FLAGS
    -std=c++0x
    -g
    -DCHECK_PTHREAD_RETURN_VALUE
    -D_FILE_OFFSET_BITS=64
    -Wall
    -Wextra
    -Werror
    -Wconversion
    -Wno-unused-parameter
    -Wold-style-cast
    -Woverloaded-virtual
    -Wpointer-arith
    -Wshadow
    -Wwrite-strings
    -march=native
    -rdynamic
)

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -finline-limit=1000 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

find_package(Boost 1.56 REQUIRED)
find_package(GTest REQUIRED)

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "CXX_FLAGS = " ${CMAKE_CXX_FLAGS} " " ${CMAKE_CXX_FLAGS_${BUILD_TYPE}})

add_subdirectory(src/base)
add_subdirectory(src/common)
add_subdirectory(src/tests)
