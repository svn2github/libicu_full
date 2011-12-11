INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/io")
ADD_EXECUTABLE(iotest EXCLUDE_FROM_ALL
    filetst.c iotest.cpp iotest.h stream.cpp strtst.c trnstst.c)
LINK_ICU_LIBRARIES(iotest ${CTESTFW_STUB} ${IO_STUB})
DEPEND_ON_ICU_DATA(iotest)

ADD_CUSTOM_TARGET(check-iotest
                  COMMAND iotest ${IOTEST_OPTS})
ADD_DEPENDENCIES(check check-iotest)
ADD_CUSTOM_TARGET(check-exhaustive-iotest
                  COMMAND iotest -e)
ADD_DEPENDENCIES(check-exhaustive check-exhaustive-iotest)

