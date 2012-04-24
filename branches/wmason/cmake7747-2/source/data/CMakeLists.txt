IF(IS_BIG_ENDIAN)
    SET(BYTE_ORDER_INDICATOR b)
    # This is a lower-case el ('L'), not a one ('1')
    SET(BYTE_ORDER_OPPOSITE_INDICATOR l)
    SET(BYTE_ORDER_NAME big)
ELSE()
    # This is a lower-case el ('L'), not a one ('1')
    SET(BYTE_ORDER_INDICATOR l)
    SET(BYTE_ORDER_OPPOSITE_INDICATOR b)
    SET(BYTE_ORDER_NAME little)
ENDIF()
SET(DATA_ARCHIVE_BASE "${ICU_PREFIX}dt${ICU_LIB_MAJOR_VERSION}@byte order@.dat")
STRING(REGEX REPLACE "(^.+)@byte order@" "${CMAKE_CURRENT_SOURCE_DIR}/in/\\1${BYTE_ORDER_INDICATOR}" SOURCE_ARCHIVE "${DATA_ARCHIVE_BASE}")
STRING(REGEX REPLACE "(^.+)@byte order@" "${CMAKE_CURRENT_BINARY_DIR}/\\1${BYTE_ORDER_INDICATOR}" DATA_ARCHIVE "${DATA_ARCHIVE_BASE}")
# Data name is used by cintltst
GET_FILENAME_COMPONENT(DATA_NAME "${DATA_ARCHIVE}" NAME_WE)
SET(DATA_NAME ${DATA_NAME} PARENT_SCOPE)
IF(EXISTS "${SOURCE_ARCHIVE}")
    ADD_CUSTOM_COMMAND(OUTPUT "${DATA_ARCHIVE}"
                       COMMAND "${CMAKE_COMMAND}" -E echo "Copying source archive \"${SOURCE_ARCHIVE}\""
                       COMMAND "${CMAKE_COMMAND}" -E copy "${SOURCE_ARCHIVE}" "${DATA_ARCHIVE}"
                       DEPENDS "${SOURCE_ARCHIVE}")
ELSE()
    STRING(REGEX REPLACE "(^.+)@byte order@" "${CMAKE_CURRENT_SOURCE_DIR}/in/\\1${BYTE_ORDER_OPPOSITE_INDICATOR}" SOURCE_ARCHIVE "${DATA_ARCHIVE_BASE}")
    IF(NOT EXISTS "${SOURCE_ARCHIVE}")
        MESSAGE(FATAL_ERROR "The data file ${ICU_PREFIX}dt${ICU_LIB_MAJOR_VERSION}[bl].dat must exist in ${CMAKE_CURRENT_SOURCE_DIR}/in. It does not matter whether it is the big- or little-endian version of the file.")
    ENDIF()
    ADD_CUSTOM_COMMAND(OUTPUT "${DATA_ARCHIVE}"
                       COMMAND "${CMAKE_COMMAND}" -E echo "Converting source archive \"${SOURCE_ARCHIVE}\" to ${BYTE_ORDER_NAME} endian"
                       COMMAND icupkg -t${BYTE_ORDER_INDICATOR} "${SOURCE_ARCHIVE}" "${DATA_ARCHIVE}"
                       DEPENDS "${SOURCE_ARCHIVE}")
ENDIF()

IF(LOWER_ICU_DATA_PACKAGING STREQUAL library)

    SET(DATA_ENTRY_POINT "${ICU_PREFIX}dt${ICU_LIB_MAJOR_VERSION}")
    IF(GENCCODE_OBJECT_GENERATION)
        # On Windows we generate an .obj file
        STRING(REGEX REPLACE "^(.*)[lb]\\.dat$" "\\1${CMAKE_C_OUTPUT_EXTENSION}" DATA_ARCHIVE_GEN "${DATA_ARCHIVE}")
        SET(GENCCODE_OPTS -o)
        # We have to link in a dummy obj on Windows in order for linkage of the genccode-
        # generated object to work.
        IF(WIN32)
            SET(REAL_DATA_ADDL_SOURCES dummy.c)
        ENDIF()
    ELSE()
        # The genccode output must be assembly language, if we are not generating a native
        # object file. If C is selected, then the data can't be loaded because of
        # "invalid format" (ticket #9270). Note that the ASM_STYLE variable is set in
        # cmake-modules/Configure.cmake.
        STRING(REGEX REPLACE "^(.*)[lb]\\.dat$" "\\1.s" DATA_ARCHIVE_GEN "${DATA_ARCHIVE}")
        SET(GENCCODE_OPTS -a ${ASM_STYLE})
    ENDIF()
    GET_FILENAME_COMPONENT(DATA_ARCHIVE_GENCCODE_NAME "${DATA_ARCHIVE_GEN}" NAME_WE)
    ADD_CUSTOM_COMMAND(OUTPUT "${DATA_ARCHIVE_GEN}"
                       COMMAND genccode ${GENCCODE_OPTS} -d "${CMAKE_CURRENT_BINARY_DIR}" -f "${DATA_ARCHIVE_GENCCODE_NAME}" -e ${DATA_ENTRY_POINT} "${DATA_ARCHIVE}"
                       DEPENDS "${DATA_ARCHIVE}")

    # The real data has the same output names as the stub data, but we have 
    # to give the CMake target a different name. So, after we go through the
    # normal ICU_LIBRARY processing we circle back and rename the output files.
    ICU_LIBRARY(${REAL_DATA_STUB} "${DATA_ARCHIVE_GEN}" ${REAL_DATA_ADDL_SOURCES})
    TARGET_NAMES_FROM_STUB(${REAL_DATA_STUB} REAL_DATA_SHARED REAL_DATA_STATIC)
    TARGET_NAMES_FROM_STUB(${DATA_STUB} DATA_SHARED DATA_STATIC)
    IF(ENABLE_SHARED)
        SET_TARGET_PROPERTIES(${REAL_DATA_SHARED} PROPERTIES
                              OUTPUT_NAME ${DATA_SHARED}
                              LINKER_LANGUAGE C)
        IF(WIN32)
            SET_TARGET_PROPERTIES(${REAL_DATA_SHARED} PROPERTIES
                                  OUTPUT_NAME_DEBUG "${DATA_SHARED}d")
        ENDIF()
    ENDIF()
    IF(ENABLE_STATIC)
        SET_TARGET_PROPERTIES(${REAL_DATA_STATIC} PROPERTIES
                              OUTPUT_NAME ${DATA_STATIC}
                              LINKER_LANGUAGE C)
        IF(WIN32)
            SET_TARGET_PROPERTIES(${REAL_DATA_STATIC} PROPERTIES
                                  OUTPUT_NAME_DEBUG "${DATA_STATIC}d")
        ENDIF()
    ENDIF()

    INSTALL_ICU_LIBRARY(${REAL_DATA_STUB})

ELSEIF(LOWER_ICU_DATA_PACKAGING STREQUAL archive)

    ADD_CUSTOM_TARGET(icurealdata ALL
                      DEPENDS "${DATA_ARCHIVE}")
    INSTALL(FILES "${DATA_ARCHIVE}"
            DESTINATION "${DATA_DIR}"
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

ELSEIF(LOWER_ICU_DATA_PACKAGING STREQUAL files)

    STRING(REGEX REPLACE "^(.*)\\.dat$" "\\1" DATA_EXTRACTION_DIR "${DATA_ARCHIVE}")
    FILE(MAKE_DIRECTORY "${DATA_EXTRACTION_DIR}")
    ADD_CUSTOM_TARGET(icurealdata ALL
                      COMMAND icupkg -d "${DATA_EXTRACTION_DIR}" -x \\* "${DATA_ARCHIVE}"
                      DEPENDS "${DATA_ARCHIVE}")
    INSTALL(DIRECTORY "${DATA_EXTRACTION_DIR}"
            DESTINATION "${DATA_DIR}"
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
            DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                                  GROUP_READ GROUP_EXECUTE
                                  WORLD_READ WORLD_EXECUTE)

ENDIF()

