# Modules we use
INCLUDE(CheckCSourceCompiles)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckCXXSourceCompiles)
INCLUDE(CheckTypeSize)
INCLUDE(TestBigEndian)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckCSourceRuns)

# Set a reasonable default for the build type
IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release CACHE STRING "Build type, one of: Release, Debug, RelWithDebInfo, or MinSizeRel" FORCE)
ENDIF()
MESSAGE(STATUS "Build type -- ${CMAKE_BUILD_TYPE}")

# On Windows we want to set the runtime output directory, so that
# executables can find all their DLLs at run time.
IF(WIN32)
    FOREACH(TYPE "" None Debug Release RelWithDebInfo MinSizeRel)
        IF(TYPE)
            STRING(TOUPPER ${TYPE} FORXX_UPPER_TYPE)
            SET(FORXX_RT_OUT_VAR CMAKE_RUNTIME_OUTPUT_DIRECTORY_${FORXX_UPPER_TYPE})
        ELSE()
            SET(FORXX_RT_OUT_VAR CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        ENDIF()
        SET(${FORXX_RT_OUT_VAR} "${CMAKE_BINARY_DIR}/bin")
    ENDFOREACH(TYPE)
ENDIF()

# Set the output directory for libraries to the same one for all
# libraries so that if/when the data library is created it will
# overwrite the stub data library.
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Rpath handling
# NOTE: This rpath support does not work on Macintosh with CMake 2.8.5.
# (Use my Forxx trick for rpath on Macintosh -WRM)
IF(ENABLE_RPATH)
    SET(CMAKE_SKIP_BUILD_RPATH FALSE)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}")
    SET(CMAKE_INSTALL_RPATH_USE_LINK_RPATH FALSE)
ENDIF()

# Version numbers
MESSAGE(STATUS "Checking for ICU version numbers")

# The ICU version
FILE(STRINGS "${CMAKE_SOURCE_DIR}/common/unicode/uvernum.h" ICU_VERSION_LINES)
SET(ICU_VERSION_DEF_REGEX "^[ \\t]*#define[ \\t]+U_ICU_VERSION[ \\t]+\\\"(.*)\\\"")
FOREACH(LINE ${ICU_VERSION_LINES})
    IF("${LINE}" MATCHES "${ICU_VERSION_DEF_REGEX}")
        STRING(REGEX REPLACE "${ICU_VERSION_DEF_REGEX}" "\\1" ICU_VERSION "${LINE}")
        STRING(REGEX REPLACE "([0-9]+)\\..*" "\\1" ICU_LIB_MAJOR_VERSION "${ICU_VERSION}")
        BREAK()
    ENDIF()
ENDFOREACH(LINE)
IF(NOT ICU_VERSION OR NOT ICU_LIB_MAJOR_VERSION)
    MESSAGE(FATAL_ERROR "Cannot determine ICU version number from uvernum.h header file")
ENDIF()

# The Unicode version
FILE(STRINGS "${CMAKE_SOURCE_DIR}/common/unicode/uchar.h" ICU_VERSION_LINES)
SET(ICU_UNICODE_VERSION_DEF_REGEX "^[ \\t]*#define[ \\t]+U_UNICODE_VERSION[ \\t]+\\\"(.*)\\\"")
FOREACH(LINE ${ICU_VERSION_LINES})
    IF("${LINE}" MATCHES "${ICU_UNICODE_VERSION_DEF_REGEX}")
        STRING(REGEX REPLACE "${ICU_UNICODE_VERSION_DEF_REGEX}" "\\1" ICU_UNICODE_VERSION "${LINE}")
        BREAK()
    ENDIF()
ENDFOREACH(LINE)
IF(NOT ICU_UNICODE_VERSION)
    MESSAGE(FATAL_ERROR "Cannot determine Unicode version number from uchar.h header file")
ENDIF()

MESSAGE(STATUS "Checking for ICU version numbers - release ${ICU_VERSION}, unicode version ${ICU_UNICODE_VERSION}")

# TODO: Figure out why this doesn't work
#
# Check to see if we can use some optimizations when building a static library
#IF(ENABLE_STATIC AND NOT ENABLE_SHARED AND CMAKE_COMPILER_IS_GNUCXX)
#    SET(GCC_STATIC_OPTS "-ffunction-sections -fdata-sections")
#    SET(GCC_STATIC_LOPTS "-Wl,--gc-sections")
#    SET(CMAKE_REQUIRED_FLAGS "${GCC_STATIC_OPTS} ${GCC_STATIC_LOPTS}")
#    CHECK_C_SOURCE_COMPILES("int main() { return 0; }" HAVE_GCC_STATIC_OPT)
#    IF(HAVE_GCC_STATIC_OPT)
#        SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${GCC_STATIC_OPTS}")
#        SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${GCC_STATIC_OPTS}")
#        SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${GCC_STATIC_OPTS}")
#        SET(CMAKE_CXX_FLAGS_RELIWTHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${GCC_STATIC_OPTS}")
#        SET(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${GCC_STATIC_OPTS}")
#        SET(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${GCC_STATIC_OPTS}")
#        SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${GCC_STATIC_LOPTS}")
#        SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${GCC_STATIC_LOPTS}")
#        SET(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} ${GCC_STATIC_LOPTS}")
#    ENDIF()
#ENDIF()

# Changes for auto cleanup
IF(ENABLE_AUTO_CLEANUP)
    ADD_DEFINITIONS(-DUCLN_NO_AUTO_CLEANUP=0)
ENDIF()

# Whether to enable draft APIs
IF(NOT ENABLE_DRAFT)
    ADD_DEFINITIONS(-DU_SHOW_DRAFT_API=0)
ENDIF()

# Whether renaming is enabled
IF(NOT ENABLE_RENAMING)
    ADD_DEFINITIONS(-DU_DISABLE_RENAMING=1)
ENDIF()

# Whether tracing is enabled
IF(ENABLE_TRACING)
    ADD_DEFINITIONS(-DU_ENABLE_TRACING=1)
ENDIF()

# Whether dynamic loading of plugins is enabled
IF(NOT ENABLE_DYNAMIC_LOADING)
    ADD_DEFINITIONS(-DU_ENABLE_DYLOAD=0)
ENDIF()

# Check for runtime dynamic linker API.
#
# NOTE: This check appears to be broken in the autoconf build system. In
# source/common/putil.cpp two defaults are made: one for dlfcn.h and one
# for dlopen(). In configure.in only the absense of dlopen() is recorded,
# which could break platforms that don't also have dlfcn.h (though I have
# no idea which platforms those might be or why they would be that way).
IF(UNIX)
    CHECK_INCLUDE_FILE(dlfcn.h HAVE_DLFCN_H)
    # The following definition does not appear in configure.in
    IF(NOT HAVE_DLFCN_H)
        ADD_DEFINITIONS(-DHAVE_DLFCN_H=0)
    ENDIF()
    CHECK_SYMBOL_EXISTS(dlopen dlfcn.h HAVE_DLOPEN_NO_LIB)
    IF(NOT HAVE_DLOPEN_NO_LIB)
        FIND_LIBRARY(LIBDL dl)
        IF(LIBDL)
            SET(CMAKE_REQUIRED_LIBRARIES "${LIBDL}")
            CHECK_SYMBOL_EXISTS(dlopen dlfcn.h HAVE_DLOPEN_WITH_LIB)
            IF(NOT HAVE_DLOPEN_WITH_LIB)
                ADD_DEFINITIONS(-DHAVE_DLOPEN=0)
                UNSET(LIBDL)
            ENDIF()
        ENDIF()
    ENDIF()
ENDIF()

# Check gettimeofday
IF(UNIX)
    CHECK_FUNCTION_EXISTS("gettimeofday" HAVE_GETTIMEOFDAY)
    IF(NOT HAVE_GETTIMEOFDAY)
        ADD_DEFINITIONS(-DHAVE_GETTIMEOFDAY=0)
    ENDIF()
ENDIF()

# See if we have a std::string
CHECK_INCLUDE_FILE_CXX("string" HAVE_STD_STRING)
IF(NOT HAVE_STD_STRING)
    ADD_DEFINITIONS(-DU_HAVE_STD_STRING=0)
ENDIF()

# Check for threads and set the appropriate compiler flags.
#
# NOTE: In the autoconf build ICU_USE_THREADS is never added to the -D flags,
# so it is impossible to turn off threads.
#
# This defect is corrected here.
IF(ENABLE_THREADS)
    FIND_PACKAGE(Threads)
    IF(Threads_FOUND)
        IF(CMAKE_COMPILER_IS_GNUCXX)
            ADD_DEFINITIONS(-D_REENTRANT)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
        ELSEIF(CMAKE_C_COMPILER_ID STREQUAL SunPro)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mt")
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mt")
        ELSEIF(MSVC)
            # Nothing to do
        ELSE()
            MESSAGE(WARNING "The compiler ${CMAKE_C_COMPILER_ID} may not have its threading flags correctly set.")
        ENDIF()
        IF(ENABLE_WEAK_THREADS)
            SET(LIBTHREAD_STRONG ${CMAKE_THREAD_LIBS_INIT})
        ELSE()
            SET(LIBTHREAD_WEAK ${CMAKE_THREAD_LIBS_INIT})
        ENDIF()
    ENDIF()
ENDIF()
IF(NOT ENABLE_THREADS OR NOT Threads_FOUND)
    ADD_DEFINITIONS(-DICU_USE_THREADS=0)
ENDIF()

# Check for mmap
IF(UNIX)
    CHECK_C_SOURCE_COMPILES("
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
int main()
{
    mmap((void*)0, 0, PROT_READ, 0, 0, 0);
    return 0;
}" U_HAVE_MMAP)
    IF(NOT U_HAVE_MMAP)
        ADD_DEFINITIONS(-DU_HAVE_MMAP=0)
    ENDIF()
ENDIF()

# Check for standard integer type header
#
# NOTE: The autoconf build omits a check for stdint.h. This is not a big
# deal except that there is a check for U_HAVE_STDINT_H in
# source/common/unicode/ptypes.h. Build-time checks in
# source/common/unicode/platform.h do account for U_HAVE_STDINT_H by
# making assumptions about the platform, but there is no check done for
# whether it *actually* exists or not.
#
# The following code corrects this defect.
IF(UNIX)
    CHECK_INCLUDE_FILE("stdint.h" HAVE_STDINT_H)
    IF(NOT HAVE_STDINT_H)
        ADD_DEFINITIONS(-DU_HAVE_STDINT_H=0)
    ENDIF()
    CHECK_INCLUDE_FILE("inttypes.h" HAVE_INTTYPES_H)
    IF(NOT HAVE_INTTYPES_H)
        ADD_DEFINITIONS(-DU_HAVE_INTTYPES_H=0)
    ENDIF()
ENDIF()

# Check for dirent.h
IF(UNIX)
    CHECK_INCLUDE_FILE("dirent.h" HAVE_DIRENT_H)
    IF(NOT HAVE_DIRENT_H)
        ADD_DEFINITIONS(-DU_HAVE_DIRENT_H=0)
    ENDIF()
ENDIF()

# Check endianness
#
# NOTE: Although this check is identical in the autoconf build, the
# command-line definition for U_IS_BIG_ENDIAN is never set in the
# autoconf build (even though it is later used), so presumably the
# check in the source/common/unicode/platform.h header is sufficient.
#
# This presumed defect is not corrected here.
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

# Check for langinfo and CODESET
#
# NOTE: This check is broken in the autoconf build system.
#
# The definition U_NL_LANGINFO_CODESET suffers from the problem that it
# is defined as NL_LANGINFO_CODESET on the command line, but queried and
# possibly set in source/common/putilimp.h as U_NL_LANGINFO_CODESET.
# 
# This defect is corrected here.
IF(UNIX)
    CHECK_FUNCTION_EXISTS(nl_langinfo HAVE_NL_LANGINFO)
    IF(HAVE_NL_LANGINFO)
        CHECK_C_SOURCE_COMPILES("
#include <langinfo.h>
int main()
{
    nl_langinfo(CODESET);
    return 0;
}" NL_LANGINFO_CODESET)
        IF(NL_LANGINFO_CODESET)
            ADD_DEFINITIONS(-DU_NL_LANGINFO_CODESET=CODESET)
        ELSE()
            CHECK_C_SOURCE_COMPILES("
#include <langinfo.h>
int main()
{
    nl_langinfo(_NL_CTYPE_CODESET_NAME);
    return 0;
}" NL_LANGINFO_CODESET)
            IF(NL_LANGINFO_CODESET)
                ADD_DEFINITIONS(-DU_NL_LANGINFO_CODESET=_NL_CTYPE_CODESET_NAME)
            ENDIF()
        ENDIF()
        IF(NOT NL_LANGINFO_CODESET)
            ADD_DEFINITIONS(-DU_HAVE_NL_LANGINFO_CODESET=0)
        ENDIF()
    ENDIF()
ENDIF()

# Check how to override new and delete
CHECK_CXX_SOURCE_COMPILES("
#include <stdlib.h>
class UMemory
{
public:
    void* operator new(size_t size) { return malloc(size); }
    void* operator new[](size_t size) { return malloc(size); }
    void operator delete(void* p) { free(p); }
    void operator delete[](void* p) { free(p); }
};
int main()
{
    return 0;
}" OVERRIDE_CXX_ALLOCATION)
IF(NOT OVERRIDE_CXX_ALLOCATION)
    ADD_DEFINITIONS(-DU_OVERRIDE_CXX_ALLOCATION=0)
ENDIF()

CHECK_CXX_SOURCE_COMPILES("
#include <stdlib.h>
class UMemory
{
public:
    void* operator new(size_t size) { return malloc(size); }
    void* operator new[](size_t size) { return malloc(size); }
    void operator delete(void* p) { free(p); }
    void operator delete[](void* p) { free(p); }
    void* operator new(size_t size, void* ptr) { return ptr; }
    void operator delete(void*, void*) { }
};
int main()
{
    return 0;
}" HAVE_PLACEMENT_NEW)
IF(NOT HAVE_PLACEMENT_NEW)
    ADD_DEFINITIONS(-DU_HAVE_PLACEMENT_NEW=0)
ENDIF()

# Check for popen
IF(UNIX)
    CHECK_FUNCTION_EXISTS("popen" HAVE_POPEN)
    IF(NOT HAVE_POPEN)
        ADD_DEFINITIONS(-DU_HAVE_POPEN=0)
    ENDIF()
ENDIF()

# Time zone stuff
#
# NOTE: Both of these checks in the autoconf build for tzset and
# tzname are kind of curious. Both will add a defintion indicating
# whether the platform *has* the function in question, but neither
# bothers to indicate what the definition is. Later, in
# source/common/putilimp.h assumptions about the functions are made
# based on platform, but the checks done here are ignored.
#
# The other curious thing about these checks in autoconf is that
# neither U_HAVE_TZSET nor U_HAVE_TIMZONE are ever used by any actual
# code. So, the existence of these checks is either buggy or
# superfluous.
#
# These apparent defects are not corrected here.
CHECK_FUNCTION_EXISTS(tzset HAVE_TZSET)
IF(NOT HAVE_TZSET)
    CHECK_FUNCTION_EXISTS(_tzset HAVE__TZSET)
    IF(NOT HAVE__TZSET)
        ADD_DEFINITIONS(-DU_HAVE_TZSET=0)
    ENDIF()
ENDIF()

CHECK_C_SOURCE_COMPILES("
#ifndef __USE_POSIX
#define __USE_POSIX
#endif
#include <stdlib.h>
#include <time.h>
#ifndef tzname
extern char* tzname[];
#endif
int main()
{
    return atoi(*tzname);
}" HAVE_TZNAME)
IF(NOT HAVE_TZNAME)
    CHECK_C_SOURCE_COMPILES("
#include <stdlib.h>
#include <time.h>
extern char* _tzname[];
int main()
{
    return atoi(*_tzname);
}" HAVE__TZNAME)
    IF(NOT HAVE__TZNAME)
        ADD_DEFINITIONS(-DU_HAVE_TIMEZONE=0)
    ENDIF()
ENDIF()

# Check standard integer types
IF(UNIX)
    FOREACH(TYPE int8_t uint8_t int16_t uint16_t int32_t uint32_t int64_t uint64_t)
        CHECK_TYPE_SIZE(${TYPE} HAVE_${TYPE})
        IF(NOT HAVE_${TYPE})
            STRING(TOUPPER ${TYPE} UPPER_TYPE)
            ADD_DEFINITIONS(-DU_HAVE_${UPPER_TYPE}=0)
        ENDIF()
    ENDFOREACH()
ENDIF()

# Wchars
CHECK_INCLUDE_FILE(wchar.h HAVE_WCHAR_H)
IF(HAVE_WCHAR_H)
    FIND_LIBRARY(LIBWCS wcs)
    FIND_LIBRARY(LIBW w)
    IF(LIBWCS)
        SET(CMAKE_REQUIRED_LIBRARIES "${LIBWCS}")
    ENDIF()
    IF(LIBW)
        LIST(APPEND CMAKE_REQUIRED_LIBRARIES "${LIBW}")
    ENDIF()
    CHECK_FUNCTION_EXISTS(wcscpy HAVE_WCSCPY)
    IF(NOT HAVE_WCSCPY)
        ADD_DEFINITIONS(-DU_HAVE_WCSCPY=0)
    ENDIF()
ELSE()
    ADD_DEFINITIONS(-DU_HAVE_WCHAR_H=0 -DU_HAVE_WCSCPY=0)
ENDIF()

# NOTE: In the autoconf build this size is checked but it is never
# communicated to the world. The definition is checked, however,
# in source/common/unicode/platform.h, so it appears that not setting
# the definition is a bug.
#
# This defect is corrected here.
IF(HAVE_WCHAR_H)
    UNSET(CMAKE_EXTRA_INCLUDE_FILES)
    CHECK_INCLUDE_FILE(stddef.h HAVE_STDDEF_H)
    IF(HAVE_STDDEF_H)
        LIST(APPEND CMAKE_EXTRA_INCLUDE_FILES stddef.h)
    ENDIF()
    LIST(APPEND CMAKE_EXTRA_INCLUDE_FILES stdlib.h string.h wchar.h)
    CHECK_TYPE_SIZE(wchar_t SIZEOF_WCHAR_T)
    IF(SIZEOF_WCHAR_T GREATER 0)
        ADD_DEFINITIONS(-DU_SIZEOF_WCHAR_T=${SIZEOF_WCHAR_T})
    ENDIF()
ENDIF()

# Check how to use Unicode literal strings
IF(CMAKE_SYSTEM_NAME STREQUAL AIX AND NOT CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_REQUIRED_FLAGS "-qutf")
    CHECK_C_SOURCE_COMPILES("
const unsigned short hello[] = u\"hello\";
int main()
{
    return 0;
}" CHECK_UTF16_STRING_AIX)
    IF(CHECK_UTF16_STRING_AIX)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -qutf")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -qutf")
    ENDIF()
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL SunOS AND NOT CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_REQUIRED_FLAGS "-xustr=ascii_utf16_ushort")
    CHECK_CXX_SOURCE_COMPILES("
const unsigned short hello[] = U\"hello\";
int main()
{
    return 0;
}" CHECK_UTF16_STRING_SOLARIS)
    IF(CHECK_UTF16_STRING_SOLARIS)
        ADD_DEFINITIONS(-DU_CHECK_UTF16_STRING=1)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -xustr=ascii_utf16_ushort")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -xustr=ascii_utf16_ushort")
    ENDIF()
ENDIF()

IF(CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_REQUIRED_FLAGS "-std=gnu99")
    SET(CMAKE_REQUIRED_DEFINITIONS "-D_GCC_")
    SET(GNU_UTF16_CHECK_PROGRAM "
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
#ifdef _GCC_
typedef __CHAR16_TYPE__ char16_t;
#endif
char16_t test[] = u\"This is a UTF-16 literal string.\";
#else
GCC IS TOO OLD!
#endif
int main()
{
    return 0;
}")
    CHECK_C_SOURCE_COMPILES("${GNU_UTF16_CHECK_PROGRAM}" CHECK_UTF16_STRING_C)
    UNSET(CMAKE_REQUIRED_DEFINITIONS)
    SET(CMAKE_REQUIRED_FLAGS "-std=c++0x")
    CHECK_CXX_SOURCE_COMPILES("${GNU_UTF16_CHECK_PROGRAM}" CHECK_UTF16_STRING_CXX)
    IF(CHECK_UTF16_STRING_C AND CHECK_UTF16_STRING_CXX)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GCC_ -std=gnu99")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    ENDIF()
    UNSET(CMAKE_REQUIRED_FLAGS)
ENDIF()

# Data packaging
# This has already been validated in top-level CMakeLists.txt
MESSAGE(STATUS "Data packaging mode - ${LOWER_ICU_DATA_PACKAGING}")

# Library suffix
IF(LIBRARY_SUFFIX)
    STRING(REGEX REPLACE "[^A-Za-z0-9_]" "_" LIBRARY_SUFFIX_C_NAME "${LIBRARY_SUFFIX}")
    ADD_DEFINITIONS(-DU_HAVE_LIB_SUFFIX=1 -DU_LIB_SUFFIX_C_NAME=${LIBRARY_SUFFIX_C_NAME})
ENDIF()

# Strict compile
IF(ENABLE_STRICT)
    IF(CMAKE_COMPILER_IS_GNUCXX)
        SET(STRICT_FLAGS "-Wall -pedantic -Wpointer-arith -Wwrite-strings -Wno-long-long")
        # TODO: Make sure this system name is correct on HP-UX
        IF(NOT CMAKE_SYSTEM_NAME STREQUAL HP-UX)
            SET(STRICT_FLAGS "${STRICT_FLAGS} -ansi")
        ENDIF()
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STRICT_FLAGS} -Wmissing-prototypes -Wshadow")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${STRICT_FLAGS}")
        IF(CMAKE_SYSTEM_NAME STREQUAL SunOS)
            ADD_DEFINITIONS(-D__STDC__=0)
        ENDIF()
    ELSEIF(MSVC)
        # These warnings are mind-numbing white noise
        # SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
        # SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    ENDIF()
ENDIF()

# Adjustments for MSVC
IF(MSVC)
    ADD_DEFINITIONS(/D_CRT_SECURE_NO_DEPRECATE /DU_ATTRIBUTE_DEPRECATED=)
    # Use a directory property here so that we can target on debug builds.
    # Unfortunately, we have to use this roundabout way of setting the
    # property because the APPEND_STRING flag doesn't work when setting a
    # directory property, and this *has* to be appended to the list.
    GET_PROPERTY(TOP_COMPILE_DEFS DIRECTORY "${CMAKE_SOURCE_DIR}"
                 PROPERTY COMPILE_DEFINITIONS_DEBUG)
    SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}"
                 PROPERTY COMPILE_DEFINITIONS_DEBUG ${TOP_COMPILE_DEFS} /DRBBI_DEBUG)
ENDIF()

# If static is enabled, then we want to set a compile definition of U_STATIC_IMPLEMENTATION
# in this directory and every subdirectory. This is to make sure that everyone is using the
# static implementation.
#
# However, there is a catch. If shared is also enabled, then the shared libraries must be
# built and linked with dynamic linkage. This is handled by turning off U_STATIC_IMPLEMENTATION
# in the library directories themselves and just adding it back for the static library
# targets in those directories. This is implemented in source/cmake-modules/IcuLibrary.cmake.
IF(ENABLE_STATIC)
    ADD_DEFINITIONS(-DU_STATIC_IMPLEMENTATION)
ENDIF()

# Figure out what style of assembly language to select for use with genccode when it
# generates assembly language from the data archive.
IF(CMAKE_COMPILER_IS_GNUCXX)
    IF(CMAKE_SYSTEM_NAME STREQUAL Darwin)
        SET(ASM_STYLE gcc-darwin)
    ELSEIF(CMAKE_SYSTEM_NAME STREQUAL SunOS)
        EXECUTE_PROCESS(COMMAND "${CMAKE_C_COMPILER}" -print-prog-name=as
                        OUTPUT_VARIABLE ASM_PROGRAM)
        EXECUTE_PROCESS(COMMAND "${ASM_PROGRAM}" --version
                        OUTPUT_VARIABLE ASM_PROGRAM_VERSION)
        IF(ASM_PROGRAM_VERSION MATCHES GNU)
            SET(ASM_STYLE gcc)
        ELSE()
            SET(ASM_STYLE sun-x86)
        ENDIF()
    ELSE()
        SET(ASM_STYLE gcc)
    ENDIF()
ELSEIF(CMAKE_C_COMPILER_ID STREQUAL SunPro)
    IF(CMAKE_SYSTEM_PROCESSOR STREQUAL sparc)
        SET(ASM_STYLE sun)
    ELSE()
        SET(ASM_STYLE sun-x86)
    ENDIF()
ELSEIF(CMAKE_C_COMPILER_ID STREQUAL XL)
    SET(ASM_STYLE xlc)
ELSEIF(CMAKE_C_COMPILER_ID STREQUAL HP)
    IF(CMAKE_SYSTEM_PROCESSOR STREQUAL ia64)
        SET(ASM_STYLE aCC-ia64)
    ELSE()
        SET(ASM_STYLE aCC-parisc)
    ENDIF()
ELSEIF(MSVC)
    SET(ASM_STYLE masm)
ELSE()
    MESSAGE(WARNING "No assembly language style could be determined for compiler ${CMAKE_C_COMPILER_ID}. Defaulting to gcc.")
    SET(ASM_STYLE gcc)
ENDIF()
SET(ASM_STYLE ${ASM_STYLE} CACHE STRING "The style of assembly language to generate")

# Set the bitness of the compilation (LOWER_LIBRARY_BITS was set and
# validated in top-level CMakeLists.txt)
IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
    IF(LOWER_LIBRARY_BITS STREQUAL 32)
        SET(BITS_TO_CHECK 32)
    ENDIF()
ELSEIF(CMAKE_SIZEOF_VOID_P EQUAL 4)
    IF(LOWER_LIBRARY_BITS MATCHES "64|64else32")
        SET(BITS_TO_CHECK 64)
    ENDIF()
ELSE()
    MESSAGE(FATAL_ERROR "Unrecognized value for sizeof(void*): ${CMAKE_SIZEOF_VOID_P}")
ENDIF()
IF(BITS_TO_CHECK)
    IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID STREQUAL SunPro)
        SET(CMAKE_REQUIRED_FLAGS "-m${BITS_TO_CHECK}")
    ELSEIF(CMAKE_C_COMPILER_ID STREQUAL XL)
        SET(CMAKE_REQUIRED_FLAGS "-q${BITS_TO_CHECK}")
    ELSEIF(CMAKE_C_COMPILER_ID STREQUAL HP)
        IF(CMAKE_SYSTEM_PROCESSOR STREQUAL ia64)
            SET(CMAKE_REQUIRED_FLAGS "+DD${BITS_TO_CHECK}")
        ELSE()
            IF(BITS_TO_CHECK EQUAL 64)
                SET(CMAKE_REQUIRED_FLAGS "+DA2.0W")
            ELSE()
                SET(CMAKE_REQUIRED_FLAGS "+DA2.0N")
            ENDIF()
        ENDIF()
    ENDIF()
    CHECK_C_SOURCE_COMPILES("int main(void) { return 0; }" CAN_BUILD_${BITS_TO_CHECK})
    IF(CAN_BUILD_${BITS_TO_CHECK})
        CHECK_C_SOURCE_RUNS("int main(void) { return (sizeof(void*) * 8 == ${BITS_TO_CHECK}) ? 0 : 1; }"
                            CAN_RUN_${BITS_TO_CHECK})
        IF(CAN_RUN_${BITS_TO_CHECK})
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_REQUIRED_FLAGS}")
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_REQUIRED_FLAGS}")
            IF("${CMAKE_C_COMPILER}" STREQUAL "${CMAKE_ASM_COMPILER}")
                SET(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${CMAKE_REQUIRED_FLAGS}")
            ENDIF()
        ENDIF()
    ENDIF()
ENDIF()

# See if we need the math library
CHECK_SYMBOL_EXISTS(log math.h LOG_WITHOUT_LIBM)
IF(NOT LOG_WITHOUT_LIBM)
    SET(CMAKE_REQUIRED_LIBRARIES m)
    CHECK_SYMBOL_EXISTS(log math.h LOG_WITH_LIBM)
    IF(LOG_WITH_LIBM)
        SET(LIBM m)
    ELSE()
        MESSAGE(FATAL_ERROR "Can't figure out how to use the log() function")
    ENDIF()
ENDIF()

