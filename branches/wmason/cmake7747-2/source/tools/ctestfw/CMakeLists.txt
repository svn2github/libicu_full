ADD_DEFINITIONS(-DT_CTEST_IMPLEMENTATION)
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}")
ICU_LIBRARY(${CTESTFW_STUB}
    ctest.c datamap.cpp testdata.cpp tstdtmod.cpp ucln_ct.c uperf.cpp
    unicode/ctest.h unicode/datamap.h unicode/testdata.h unicode/testlog.h
    unicode/testtype.h unicode/tstdtmod.h unicode/uperf.h unicode/utimer.h
    ICU_LINK_LIBRARIES
    ${TOOLUTIL_STUB})

