FOREACH(RES root en sr)
    IF(RES STREQUAL sr)
        SET(ADDL_GENRB_OPTS -e cp1251)
    ELSE()
        UNSET(ADDL_GENRB_OPTS)
    ENDIF()
    ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${RES}.res"
                       COMMAND genrb -s "${CMAKE_CURRENT_SOURCE_DIR}" -d "${CMAKE_CURRENT_BINARY_DIR}" ${ADDL_GENRB_OPTS} ${RES}.txt
                       DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${RES}.txt")
ENDFOREACH()
ADD_CUSTOM_TARGET(uresb_res)
DEPEND_ON_ICU_DATA(uresb_res)
ADD_DEPENDENCIES(uresb_res
                 "${CMAKE_CURRENT_BINARY_DIR}/root.res"
                 "${CMAKE_CURRENT_BINARY_DIR}/en.res"
                 "${CMAKE_CURRENT_BINARY_DIR}/sr.res")

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n"
                    "${CMAKE_SOURCE_DIR}/io"
                    "${CMAKE_SOURCE_DIR}/tools/toolutil")
ADD_EXECUTABLE(uresb EXCLUDE_FROM_ALL
    uresb.c)
LINK_ICU_LIBRARIES(uresb
                   ${IO_STUB}
                   ${TOOLUTIL_STUB})
SET_TARGET_PROPERTIES(uresb PROPERTIES
                      LINKER_LANGUAGE CXX)
ADD_DEPENDENCIES(uresb
                 uresb_res
                 "${CMAKE_SOURCE_DIR}/tools/toolutil/uoptions.h")

ADD_DEPENDENCIES(samples uresb)

