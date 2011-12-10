# The data entry point in case we are building a shared library
SET(DATA_ENTRY_POINT "${ICU_PREFIX}dt${LIBRARY_SUFFIX_C_NAME}${ICU_LIB_MAJOR_VERSION}")
IF(IS_BIG_ENDIAN)
    SET(BYTE_ORDER_INDICATOR b)
    SET(BYTE_ORDER_OPPOSITE_INDICATOR l)
ELSE()
    SET(BYTE_ORDER_INDICATOR l)
    SET(BYTE_ORDER_OPPOSITE_INDICATOR b)
ENDIF()
SET(DATA_ARCHIVE_BASE "${ICU_PREFIX}dt${ICU_LIB_MAJOR_VERSION}@byte order@.dat")
STRING(REGEX REPLACE "(^.+)@byte order@" "${CMAKE_CURRENT_SOURCE_DIR}/in/\\1${BYTE_ORDER_INDICATOR}" SOURCE_ARCHIVE "${DATA_ARCHIVE_BASE}")
STRING(REGEX REPLACE "(^.+)@byte order@" "${CMAKE_CURRENT_BINARY_DIR}/\\1${BYTE_ORDER_INDICATOR}" DATA_ARCHIVE "${DATA_ARCHIVE_BASE}")
IF(EXISTS "${SOURCE_ARCHIVE}")
    ADD_CUSTOM_COMMAND(OUTPUT "${DATA_ARCHIVE}"
                       COMMAND "${CMAKE_COMMAND}" -E copy "${SOURCE_ARCHIVE}" "${DATA_ARCHIVE}"
                       DEPENDS "${SOURCE_ARCHIVE}")
ELSE()
    STRING(REGEX REPLACE "(^.+)@byte order@" "${CMAKE_CURRENT_SOURCE_DIR}/in/\\1${BYTE_ORDER_OPPOSITE_INDICATOR}" SOURCE_ARCHIVE "${DATA_ARCHIVE_BASE}")
    IF(NOT EXISTS "${SOURCE_ARCHIVE}")
        MESSAGE(FATAL_ERROR "The data file ${ICU_PREFIX}dt${ICU_LIB_MAJOR_VERSION}[bl].dat must exist in ${CMAKE_CURRENT_SOURCE_DIR}/in. It does not matter whether it is the big- or little-endian version of the file.")
    ENDIF()
    ADD_CUSTOM_COMMAND(OUTPUT "${DATA_ARCHIVE}"
                       COMMAND icupkg -t${BYTE_ORDER_INDICATOR} "${SOURCE_ARCHIVE}" "${DATA_ARCHIVE}"
                       DEPENDS "${SOURCE_ARCHIVE}")
ENDIF()

IF(LOWER_ICU_DATA_PACKAGING STREQUAL library)

    STRING(REGEX REPLACE "^(.*)\\.dat$" "\\1.c" DATA_ARCHIVE_C "${DATA_ARCHIVE}")

    GET_FILENAME_COMPONENT(DATA_ARCHIVE_GENCCODE_NAME "${DATA_ARCHIVE}" NAME_WE)
    ADD_CUSTOM_COMMAND(OUTPUT "${DATA_ARCHIVE_C}"
                       COMMAND genccode -d "${CMAKE_CURRENT_BINARY_DIR}" -f "${DATA_ARCHIVE_GENCCODE_NAME}" -e "${DATA_ENTRY_POINT}" "${DATA_ARCHIVE}"
                       DEPENDS "${DATA_ARCHIVE}")

    # The real data has the same output names as the stub data, but we have 
    # to give the CMake target a different name. So, after we go through the
    # normal ICU_LIBRARY processing we circle back and rename the output files.
    ICU_LIBRARY(${REAL_DATA_STUB} "${DATA_ARCHIVE_C}")
    TARGET_NAMES_FROM_STUB(${REAL_DATA_STUB} REAL_DATA_SHARED REAL_DATA_STATIC)
    TARGET_NAMES_FROM_STUB(${DATA_STUB} DATA_SHARED DATA_STATIC)
    IF(ENABLE_SHARED)
        SET_TARGET_PROPERTIES(${REAL_DATA_SHARED} PROPERTIES
                              OUTPUT_NAME ${DATA_SHARED})
        IF(WIN32)
            SET_TARGET_PROPERTIES(${REAL_DATA_SHARED} PROPERTIES
                                  OUTPUT_NAME_DEBUG "${DATA_SHARED}d")
        ENDIF()
    ENDIF()
    IF(ENABLE_STATIC)
        SET_TARGET_PROPERTIES(${REAL_DATA_STATIC} PROPERTIES
                              OUTPUT_NAME ${DATA_STATIC})
        IF(WIN32)
            SET_TARGET_PROPERTIES(${REAL_DATA_STATIC} PROPERTIES
                                  OUTPUT_NAME_DEBUG "${DATA_STATIC}d")
        ENDIF()
    ENDIF()

    INSTALL_ICU_LIBRARY(${REAL_DATA_STUB})

ELSEIF(LOWER_ICU_DATA_PACKAGING STREQUAL archive)

    # A dummy target to force creation of the data archive if necessary
    ADD_CUSTOM_TARGET(data-archive ALL
                      DEPENDS "${DATA_ARCHIVE}")
    INSTALL(FILES "${DATA_ARCHIVE}"
            DESTINATION "${DATA_DIR}"
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

ELSEIF(LOWER_ICU_DATA_PACKAGING STREQUAL files)

    MESSAGE(FATAL_ERROR "Files data packaging mode is not supported in this build system yet.")

ENDIF()

