# Copyright 2015 Software Radio Systems Limited
#
# This file is part of srsUE
#
# srsUE is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# srsUE is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# A copy of the GNU Affero General Public License can be found in
# the LICENSE file in the top-level directory of this distribution
# and at http://www.gnu.org/licenses/.
#


########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.6)
project(srsUE CXX C)
enable_testing()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)
include(SRSUEVersion) #sets version information

########################################################################
# Add general includes and dependencies
########################################################################
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_BINARY_DIR})

#select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Build type not specified: defaulting to release.")
elseif(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

########################################################################
# Compiler specific setup
########################################################################
IF(CMAKE_COMPILER_IS_GNUCXX)
    #Any additional flags for CXX
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

IF( ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-write-strings -Wno-format-extra-args -Winline -Wno-unused-result -Wno-format -std=c99 -D_GNU_SOURCE -g")
ENDIF( ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") )

IF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
ELSE(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  FIND_PACKAGE(SSE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
  IF(HAVE_AVX) 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mfpmath=sse -mavx -DLV_HAVE_AVX -DLV_HAVE_SSE")      
  ELSEIF(HAVE_SSE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mfpmath=sse -msse4.1 -DLV_HAVE_SSE")
  ENDIF(HAVE_AVX)    
ENDIF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")

########################################################################
# Find boost
########################################################################
SET(BOOST_REQUIRED_COMPONENTS
    program_options
    system
    date_time
    thread
)
if(UNIX AND EXISTS "/usr/lib64")
    list(APPEND BOOST_LIBRARYDIR "/usr/lib64") #fedora 64-bit fix
endif(UNIX AND EXISTS "/usr/lib64")
set(Boost_ADDITIONAL_VERSIONS
    "1.35.0" "1.35" "1.36.0" "1.36" "1.37.0" "1.37" "1.38.0" "1.38" "1.39.0" "1.39"
    "1.40.0" "1.40" "1.41.0" "1.41" "1.42.0" "1.42" "1.43.0" "1.43" "1.44.0" "1.44"
    "1.45.0" "1.45" "1.46.0" "1.46" "1.47.0" "1.47" "1.48.0" "1.48" "1.49.0" "1.49"
    "1.50.0" "1.50" "1.51.0" "1.51" "1.52.0" "1.52" "1.53.0" "1.53" "1.54.0" "1.54"
    "1.55.0" "1.55" "1.56.0" "1.56" "1.57.0" "1.57" "1.58.0" "1.58" "1.59.0" "1.59"
    "1.60.0" "1.60" "1.61.0" "1.61" "1.62.0" "1.62" "1.63.0" "1.63" "1.64.0" "1.64"
    "1.65.0" "1.65" "1.66.0" "1.66" "1.67.0" "1.67" "1.68.0" "1.68" "1.69.0" "1.69"
)
find_package(Boost "1.35" COMPONENTS ${BOOST_REQUIRED_COMPONENTS})

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost required to compile srsUE")
endif()

########################################################################
# Find dependencies
########################################################################
find_package(Polarssl REQUIRED)
find_package(srsLTE REQUIRED)

########################################################################
# Setup the include and linker paths
########################################################################
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${POLARSSL_INCLUDE_DIRS}
    ${SRSLTE_INCLUDE_DIRS}
)

link_directories(
    ${Boost_LIBRARY_DIRS}
    ${POLARSSL_LIBRARY_DIRS}
    ${SRSLTE_LIBRARY_DIRS}
)

########################################################################
# Create uninstall target
########################################################################
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
@ONLY)

add_custom_target(uninstall
    ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)

########################################################################
# Add headers to cmake project (useful for IDEs)
########################################################################
set(HEADERS_ALL "")
file(GLOB headers *)
foreach(_header ${headers})
    if(IS_DIRECTORY ${_header})
        file(GLOB_RECURSE tmp "${_header}/*.h")
        list(APPEND HEADERS_ALL ${tmp})
    endif(IS_DIRECTORY ${_header})
endforeach()
add_custom_target(add_srsUE_headers SOURCES ${HEADERS_ALL})

########################################################################
# Add subdirectories
########################################################################
add_subdirectory(liblte)
add_subdirectory(ue)

