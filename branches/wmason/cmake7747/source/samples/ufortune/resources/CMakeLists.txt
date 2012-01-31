FOREACH(LOCALE root es)
    ADD_CUSTOM_COMMAND(OUTPUT ${LOCALE}.res
                       COMMAND genrb -e UTF-8 -d "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/${LOCALE}.txt"
                       DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${LOCALE}.txt")
    FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/res-list.txt" "${LOCALE}.res\n")
    LIST(APPEND RES_DEPS "${LOCALE}.res")
ENDFOREACH()

SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/res-list.txt")

IF(UFORTUNE_RESOURCE_MODE STREQUAL files)
    ADD_DEPENDENCIES(ufortune ${RES_DEPS})
ELSE()
    ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ufortune_resources.dat"
                       COMMAND icupkg new "${CMAKE_CURRENT_BINARY_DIR}/ufortune_resources.dat"
                       COMMAND icupkg -a "${CMAKE_CURRENT_BINARY_DIR}/res-list.txt" "${CMAKE_CURRENT_BINARY_DIR}/ufortune_resources.dat"
                       DEPENDS ${RES_DEPS})
    IF(UFORTUNE_RESOURCE_MODE MATCHES "dll|static")
        ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ufortune_resources.s"
                           COMMAND genccode -a ${ASM_STYLE} -d "${CMAKE_CURRENT_BINARY_DIR}" -f ufortune_resources -e fortune_resources "${CMAKE_CURRENT_BINARY_DIR}/ufortune_resources.dat"
                           DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/ufortune_resources.dat")
        IF(UFORTUNE_RESOURCE_MODE STREQUAL dll)
            SET(LIB_STYLE SHARED)
        ELSE()
            SET(LIB_STYLE STATIC)
        ENDIF()
        ADD_LIBRARY(ufortune_resources ${LIB_STYLE} "${CMAKE_CURRENT_BINARY_DIR}/ufortune_resources.s")
    ELSE()
        ADD_DEPENDENCIES(ufortune "${CMAKE_CURRENT_BINARY_DIR}/ufortune_resources.dat")
    ENDIF()
ENDIF()

