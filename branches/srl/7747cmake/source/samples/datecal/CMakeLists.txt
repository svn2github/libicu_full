INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n"
                    "${CMAKE_SOURCE_DIR}/io")

ADD_EXECUTABLE(datecal EXCLUDE_FROM_ALL
    cal.cpp ccal.c)
LINK_ICU_LIBRARIES(datecal ${IO_STUB})

ADD_DEPENDENCIES(samples datecal)

