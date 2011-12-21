ADD_EXECUTABLE(convsamp EXCLUDE_FROM_ALL
    convsamp.cpp flagcb.c flagcb.h)
LINK_ICU_LIBRARIES(convsamp ${COMMON_STUB})
DEPEND_ON_ICU_DATA(convsamp)

ADD_DEPENDENCIES(samples convsamp)

