INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n")

ADD_EXECUTABLE(translit EXCLUDE_FROM_ALL
    main.cpp unaccent.cpp unaccent.h util.cpp util.h)
LINK_ICU_LIBRARIES(translit ${I18N_STUB})
DEPEND_ON_ICU_DATA(translit)

ADD_DEPENDENCIES(samples translit)

