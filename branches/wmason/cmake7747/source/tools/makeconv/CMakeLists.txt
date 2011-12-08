CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/makeconv.1.in"
               "${CMAKE_CURRENT_BINARY_DIR}/makeconv.1")

ADD_EXECUTABLE(makeconv
    gencnvex.c genmbcs.cpp genmbcs.h makeconv.c makeconv.h ucnvstat.c)
LINK_ICU_LIBRARIES(makeconv ${TOOLUTIL_STUB})

INSTALL(TARGETS makeconv
        RUNTIME DESTINATION ${BIN_DIR})
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/makeconv.1"
        DESTINATION ${MAN_DIR}/man1
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

