# This macro ensures that the real ICU data is built before the
# given target. The common library depends on the stub data, which
# has the same output as the real data build. Without an explicit
# dependency on the real data, CMake would only believe that the
# test executables depended on the stub data.
#
# Do the same thing for the other data formats, as well.
MACRO(DEPEND_ON_ICU_DATA TARGET)
    IF(LOWER_ICU_DATA_PACKAGING STREQUAL library)
        TARGET_NAMES_FROM_STUB(${REAL_DATA_STUB} SHARED_REAL_DATA STATIC_REAL_DATA)
        # Only depend on static if there is no shared at all
        IF(ENABLE_SHARED)
            ADD_DEPENDENCIES(${TARGET} ${SHARED_REAL_DATA})
        ELSEIF(ENABLE_STATIC)
            ADD_DEPENDENCIES(${TARGET} ${STATIC_REAL_DATA})
        ENDIF()
    ELSE()
        ADD_DEPENDENCIES(${TARGET} icurealdata)
    ENDIF()
ENDMACRO(DEPEND_ON_ICU_DATA TARGET)

ADD_DEFINITIONS(-DU_TOPSRCDIR="${CMAKE_SOURCE_DIR}" -DU_TOPBUILDDIR="${CMAKE_BINARY_DIR}")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n"
                    "${CMAKE_SOURCE_DIR}/tools/toolutil"
                    "${CMAKE_SOURCE_DIR}/tools/ctestfw")

# Test subdirectories add their specific tests to these targets
# as dependencies. The only subdirectories that become a part
# of the check targets are intltest, iotest and cintltst.
ADD_CUSTOM_TARGET(check)
ADD_CUSTOM_TARGET(check-exhaustive)
ADD_CUSTOM_TARGET(xcheck)

#ADD_SUBDIRECTORY(testdata)
ADD_SUBDIRECTORY(intltest)
IF(ENABLE_IO)
    ADD_SUBDIRECTORY(iotest)
ENDIF()
ADD_SUBDIRECTORY(cintltst)
ADD_SUBDIRECTORY(letest)

