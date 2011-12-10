CMAKE_MINIMUM_REQUIRED(VERSION 2.8.4)

PROJECT(ICU C CXX)

# Options
OPTION(ENABLE_STRICT "Compile with strict compiler options" ON)
OPTION(ENABLE_SHARED "Enable building of shared libraries" ON)
OPTION(ENABLE_STATIC "Enable building of static libraries" OFF)
OPTION(ENABLE_AUTO_CLEANUP "Enable auto cleanup of libraries" OFF) # Not implemented
OPTION(ENABLE_DRAFT "Enable draft APIs (and internal APIs)" ON)
OPTION(ENABLE_RENAMING "Add a version suffix to symbols" ON)
OPTION(ENABLE_TRACING "Enable function and data tracing" OFF)
OPTION(ENABLE_DYNAMIC_LOADING "Enable dynamic loading" ON)
OPTION(ENABLE_RPATH "Link with rpath" OFF)
OPTION(ENABLE_THREADS "Build with ICU thread safety" ON)
OPTION(ENABLE_WEAK_THREADS "Weakly reference the threading library" OFF) # Not implemented
OPTION(ENABLE_EXTRAS "Enable building of extras" ON)
OPTION(ENABLE_IO "Enable building of the io library" ON)
OPTION(ENABLE_LAYOUT "Enable building of the layout library" ON)
OPTION(ENABLE_TESTS "Enable building of tests" ON) # Not implemented
OPTION(ENABLE_SAMPLES "Enable building of samples" ON) # Not implemented

# User settings
SET(CROSS_BUILD_DIR "" CACHE STRING "The path to the build directory of an ICU built for the current platform") # Not implemented
SET(LIBRARY_BITS nochange CACHE STRING "How many bits to use for building: 32, 64, 64else32 or nochange") # Not implemented. See aclocal.m4:AC_DEFUN(AC_CHECK_64BIT_LIBS)
SET(ICU_DATA_PACKAGING library CACHE STRING "The type of data package: files, archive or library")
SET(LIBRARY_SUFFIX "" CACHE STRING "Add a suffix to library names")

# Validate options
IF(NOT ENABLE_STATIC AND NOT ENABLE_SHARED)
    MESSAGE(FATAL_ERROR "Neither ENABLE_STATIC nor ENABLE_SHARED is on. Nothing will be built.")
ENDIF()

# Validate user settings
IF(CROSS_BUILD_DIR)
# TODO: A more complete check.
    GET_FILENAME_COMPONENT(FULL_CROSS_BUILD_DIR "${CROSS_BUILD_DIR}" ABSOLUTE)
    IF(NOT EXISTS "${FULL_CROSS_BUILD_DIR}")
        MESSAGE(FATAL_ERROR "The cross build directory ${CROSS_BUILD_DIR} was given, but it does not exist.")
    ENDIF()
ENDIF()

SET(VALID_LIBRARY_BITS 32 64 64else32 nochange)
STRING(TOLOWER "${LIBRARY_BITS}" LOWER_LIBRARY_BITS)
LIST(FIND VALID_LIBRARY_BITS "${LOWER_LIBRARY_BITS}" VALID_LIBRARY_BITS_INDEX)
IF(VALID_LIBRARY_BITS_INDEX EQUAL -1)
    MESSAGE(FATAL_ERROR "LIBRARY_BITS must be one of ${VALID_LIBRARY_BITS}. Found ${LIBRARY_BITS}.")
ENDIF()

SET(VALID_DATA_PACKAGING files archive library)
STRING(TOLOWER "${ICU_DATA_PACKAGING}" LOWER_ICU_DATA_PACKAGING)
LIST(FIND VALID_DATA_PACKAGING "${LOWER_ICU_DATA_PACKAGING}" VALID_DATA_PACKAGING_INDEX)
IF(VALID_DATA_PACKAGING_INDEX EQUAL -1)
    MESSAGE(FATAL_ERROR "ICU_DATA_PACKAGING must be one of ${VALID_DATA_PACKAGING}. Found ${ICU_DATA_PACKAGING}.")
ENDIF()

# Configure for this platform
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules")
INCLUDE(Configure)

# We'll need these handy macros for creating ICU libraries and also
# referring to them later.
INCLUDE(IcuLibrary)

# Everyone will need stuff from common
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/common")

# These are used in IcuLibrary.cmake and in data/CMakeLists.txt
SET(ICU_PREFIX icu)
SET(ICU_STATIC_PREFIX s)

# The libraries are referred to in this build system by their stub names.
# Since the stub name varies according to platform, always use one of the
# following variables to refer to a library.
IF(WIN32)
    SET(DATA_STUB dt)
    SET(I18N_STUB in)
ELSE()
    SET(DATA_STUB data)
    SET(I18N_STUB i18n)
ENDIF()
SET(COMMON_STUB uc)
SET(IO_STUB io)
SET(LAYOUT_STUB le)
SET(LAYOUTEX_STUB lx)
SET(TOOLUTIL_STUB tu)
SET(CTESTFW_STUB test)
# The target name of the real data must be different from the stub
# data target name, even though the build output files will be the
# same
SET(REAL_DATA_STUB realdata)
# This list of all stubs is used in the macros TARGET_NAMES_FROM_STUB and
# LINK_ICU_LIBRARIES (defined in source/cmake-modules/IcuLibrary.cmake), but we
# define it here so that we only have to maintain the list of stub
# names in one place, here.
SET(ALL_STUBS ${DATA_STUB} ${I18N_STUB} ${COMMON_STUB} ${IO_STUB} ${LAYOUT_STUB}
              ${LAYOUTEX_STUB} ${TOOLUTIL_STUB} ${CTESTFW_STUB} ${REAL_DATA_STUB})

# Define the installation directories
SET(BIN_DIR "bin")
SET(SBIN_DIR "sbin")
SET(DATA_ROOT_DIR "share")
SET(LIB_DIR "lib")
SET(INCLUDE_DIR "include")
SET(MAN_DIR "${DATA_ROOT_DIR}/man")
SET(SYS_CONF_DIR "etc")
SET(DATA_DIR "${DATA_ROOT_DIR}/icu${LIBRARY_SUFFIX}/${ICU_VERSION}")

# Some variables are explicitly set to values from the autoconf build
# system because they are replaced in man pages before installation.
# It easier to set the variables here than to go and touch every
# man page .in file.
SET(PACKAGE icu)
SET(VERSION ICU_VERSION)
SET(thepkgicudatadir "${CMAKE_INSTALL_PREFIX}/${DATA_ROOT_DIR}")

# Now add all our targets
ADD_SUBDIRECTORY(stubdata)
ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(i18n)
IF(ENABLE_IO)
    ADD_SUBDIRECTORY(io)
ENDIF()
IF(ENABLE_LAYOUT)
    ADD_SUBDIRECTORY(layout)
    ADD_SUBDIRECTORY(layoutex)
ENDIF()
ADD_SUBDIRECTORY(tools)
ADD_SUBDIRECTORY(data)
IF(ENABLE_TESTS)
#    ADD_SUBDIRECTORY(test)
ENDIF()
IF(ENABLE_SAMPLES)
#    ADD_SUBDIRECTORY(samples)
ENDIF()

