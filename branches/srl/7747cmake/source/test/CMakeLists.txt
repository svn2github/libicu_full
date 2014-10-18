ADD_DEFINITIONS(-DU_TOPSRCDIR="${CMAKE_SOURCE_DIR}/" -DU_TOPBUILDDIR="${CMAKE_BINARY_DIR}/")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n"
                    "${CMAKE_SOURCE_DIR}/tools/toolutil"
                    "${CMAKE_SOURCE_DIR}/tools/ctestfw")

# Test subdirectories add their specific tests to these targets
# as dependencies. The only subdirectories that become a part
# of the check targets are intltest, iotest and cintltst.
ADD_CUSTOM_TARGET(check)
ADD_CUSTOM_TARGET(check-exhaustive)
ADD_CUSTOM_TARGET(xcheck)

ADD_SUBDIRECTORY(testdata)
ADD_SUBDIRECTORY(intltest)
IF(ENABLE_IO)
    ADD_SUBDIRECTORY(iotest)
ENDIF()
ADD_SUBDIRECTORY(cintltst)
ADD_SUBDIRECTORY(letest)

