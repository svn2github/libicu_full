ADD_EXECUTABLE(gennorm2
    gennorm2.cpp n2builder.cpp n2builder.h)
LINK_ICU_LIBRARIES(gennorm2 ${TOOLUTIL_STUB})

INSTALL(TARGETS gennorm2
        RUNTIME DESTINATION ${SBIN_DIR})

