ADD_EXECUTABLE(uciter8 EXCLUDE_FROM_ALL
    uciter8.c uit_len8.c uit_len8.h)
LINK_ICU_LIBRARIES(uciter8 ${COMMON_STUB})

ADD_DEPENDENCIES(samples uciter8)

