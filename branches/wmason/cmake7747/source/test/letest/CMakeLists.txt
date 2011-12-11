INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/layout"
                    "${CMAKE_SOURCE_DIR}/layoutex"
                    "${CMAKE_SOURCE_DIR}")
SET(COMMON_SOURCES
    cmaps.cpp cmaps.h letsutil.cpp letsutil.h FontTableCache.cpp FontTableCache.h
    PortableFontInstance.cpp PortableFontInstance.h SimpleFontInstance.cpp
    SimpleFontInstance.h)
ADD_EXECUTABLE(letest EXCLUDE_FROM_ALL
    cfonts.cpp cfonts.h cletest.c letest.cpp letest.h xmlreader.cpp xmlreader.h
    ${COMMON_SOURCES})
LINK_ICU_LIBRARIES(letest ${LAYOUTEX_STUB} ${LAYOUT_STUB} ${CTESTFW_STUB})
ADD_EXECUTABLE(gendata EXCLUDE_FROM_ALL
    gendata.cpp ${COMMON_SOURCES})
LINK_ICU_LIBRARIES(gendata ${LAYOUTEX_STUB} ${LAYOUT_STUB} ${CTESTFW_STUB})

