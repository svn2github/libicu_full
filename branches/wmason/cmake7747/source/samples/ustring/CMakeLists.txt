ADD_EXECUTABLE(ustring EXCLUDE_FROM_ALL
    ustring.cpp)
LINK_ICU_LIBRARIES(ustring ${COMMON_STUB})

ADD_DEPENDENCIES(samples ustring)

