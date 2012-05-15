INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/io")
ADD_EXECUTABLE(iotest EXCLUDE_FROM_ALL
    filetst.c iotest.cpp iotest.h stream.cpp strtst.c trnstst.c)
LINK_ICU_LIBRARIES(iotest ${CTESTFW_STUB} ${IO_STUB})
DEPEND_ON_ICU_DATA(iotest)

ADD_CUSTOM_TARGET(check-iotest
                  COMMAND iotest ${IOTEST_OPTS}
                  DEPENDS testdata)
ADD_DEPENDENCIES(check check-iotest)
ADD_CUSTOM_TARGET(check-exhaustive-iotest
                  COMMAND iotest -e
                  DEPENDS testdata)
ADD_DEPENDENCIES(check-exhaustive check-exhaustive-iotest)
ADD_CUSTOM_TARGET(xcheck-iotest
                  COMMAND iotest ${IOTEST_OPTS} -x "${CMAKE_BINARY_DIR}/test-iotest.xml"
                  DEPENDS testdata)
ADD_DEPENDENCIES(xcheck xcheck-iotest)

