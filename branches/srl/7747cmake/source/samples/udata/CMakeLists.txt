# Only data-writer needs this
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/tools/toolutil")

ADD_EXECUTABLE(data-reader EXCLUDE_FROM_ALL
    reader.c)
LINK_ICU_LIBRARIES(data-reader ${COMMON_STUB})

ADD_EXECUTABLE(data-writer EXCLUDE_FROM_ALL
    writer.c)
LINK_ICU_LIBRARIES(data-writer ${TOOLUTIL_STUB})

