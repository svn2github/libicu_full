CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/genrb.1.in"
               "${CMAKE_CURRENT_BINARY_DIR}/genrb.1")
IF(ENABLE_IO)
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/derb.1.in"
               "${CMAKE_CURRENT_BINARY_DIR}/derb.1")
ENDIF()

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n")
SET(MYTARGETS genrb)

ADD_EXECUTABLE(genrb
    errmsg.c errmsg.h genrb.c genrb.h parse.cpp parse.h prscmnts.cpp prscmnts.h rbutil.c
    rbutil.h read.c read.h reslist.c reslist.h rle.c rle.h ustr.c ustr.h wrtjava.c wrtxml.cpp)
LINK_ICU_LIBRARIES(genrb ${TOOLUTIL_STUB})

IF(ENABLE_IO)
ICU_SIMPLE_C_TOOL(derb)
set(MYTARGETS ${MYTARGETS} derb)
ENDIF()

INSTALL(TARGETS ${MYTARGETS}
        RUNTIME DESTINATION ${BIN_DIR})
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/genrb.1" "${CMAKE_CURRENT_BINARY_DIR}/derb.1"
        DESTINATION ${MAN_DIR}/man1
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

