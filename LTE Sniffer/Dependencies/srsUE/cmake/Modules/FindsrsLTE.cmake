# - Try to find srslte
#
# Once done this will define
#  SRSLTE_FOUND        - System has srslte
#  SRSLTE_INCLUDE_DIRS - The srslte include directories
#  SRSLTE_LIBRARIES    - The srslte libraries
#
# The following variables are used:
#  SRSLTE_DIR          - Environment variable giving srslte install directory
#  SRSLTE_SRCDIR       - Directory containing srslte sources
#  SRSLTE_BUILDDIR     - Directory containing srslte build

find_package(PkgConfig)
pkg_check_modules(PC_SRSLTE QUIET srslte)
set(SRSLTE_DEFINITIONS ${PC_SRSLTE_CFLAGS_OTHER})

FIND_PATH(
    SRSLTE_INCLUDE_DIRS
    NAMES   srslte/srslte.h
    HINTS   $ENV{SRSLTE_DIR}/include
            ${SRSLTE_SRCDIR}/srslte/include
            ${PC_SRSLTE_INCLUDEDIR}
            ${CMAKE_INSTALL_PREFIX}/include
    PATHS   /usr/local/include 
            /usr/include 
)

FIND_LIBRARY(
    SRSLTE_LIBRARY
    NAMES   srslte
    HINTS   $ENV{SRSLTE_DIR}/lib
            ${SRSLTE_BUILDDIR}/srslte/lib
            ${PC_SRSLTE_LIBDIR}
            ${CMAKE_INSTALL_PREFIX}/lib
            ${CMAKE_INSTALL_PREFIX}/lib64
    PATHS   /usr/local/lib
            /usr/local/lib64
            /usr/lib
            /usr/lib64
)

FIND_LIBRARY(
    SRSLTE_LIBRARY_RF
    NAMES   srslte_rf 
    HINTS   $ENV{SRSLTE_DIR}/lib
            ${SRSLTE_BUILDDIR}/srslte/lib
            ${PC_SRSLTE_LIBDIR}
            ${CMAKE_INSTALL_PREFIX}/lib
            ${CMAKE_INSTALL_PREFIX}/lib64
    PATHS   /usr/local/lib
            /usr/local/lib64
            /usr/lib
            /usr/lib64
)

IF(DEFINED SRSLTE_SRCDIR) 
    set(SRSLTE_INCLUDE_DIRS ${SRSLTE_SRCDIR}/srslte/include 
                            ${SRSLTE_SRCDIR}/cuhd/include 
                            ${SRSLTE_SRCDIR}/common/include)
ENDIF(DEFINED SRSLTE_SRCDIR)

SET(SRSLTE_LIBRARIES    ${SRSLTE_LIBRARY_RF}
                        ${SRSLTE_LIBRARY})

message(STATUS "SRSLTE LIBRARIES: " ${SRSLTE_LIBRARIES})
message(STATUS "SRSLTE INCLUDE DIRS: " ${SRSLTE_INCLUDE_DIRS})

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SRSLTE DEFAULT_MSG SRSLTE_LIBRARIES SRSLTE_INCLUDE_DIRS)
MARK_AS_ADVANCED(SRSLTE_LIBRARIES SRSLTE_INCLUDE_DIRS)

