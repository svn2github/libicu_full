INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n")

ADD_EXECUTABLE(numfmt EXCLUDE_FROM_ALL
    capi.c main.cpp util.cpp util.h)    
LINK_ICU_LIBRARIES(numfmt ${I18N_STUB})
DEPEND_ON_ICU_DATA(numfmt)

ADD_DEPENDENCIES(samples numfmt)

