INCLUDE(IcuDataInput)

ICU_RESET_DATA_BUILD_STATE()
SET(DATA_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/${DATA_NAME}")

INCLUDE(DataSources.cmake)

# DAT files
MESSAGE(STATUS "Creating data file rules")
ICU_DATA_TRANSFORM(INPUT ${DAT_SOURCES}
                   OUTPUT DAT_FILES
                   COMMAND icupkg -t${BYTE_ORDER_INDICATOR} @in@ @out@)
ICU_DATA_TRANSFORM(INPUT ${DAT_COLL_SOURCES}
                   OUTPUT DAT_FILES
                   OUTPUT_DIR coll
                   COMMAND icupkg -t${BYTE_ORDER_INDICATOR} @in@ @out@)

# Cnvalias
ICU_DATA_TRANSFORM(INPUT ${CNVALIAS_SOURCES}
                   OUTPUT_NAME cnvalias.icu
                   COMMAND gencnval -d "${DATA_BINARY_DIR}" @in@)

# BRK files
MESSAGE(STATUS "Creating break file rules")
ICU_DATA_TRANSFORM(INPUT ${BRK_SOURCES}
                   OUTPUT BRK_FILES
                   OUTPUT_DIR brkitr
                   OUTPUT_EXT .brk
                   DEPENDS ${DAT_FILES}
                   COMMAND genbrk -c -i "${DATA_BINARY_DIR}" -r @in@ -o @out@)
ICU_DATA_TRANSFORM(INPUT ${BRK_CTD_SOURCES}
                   OUTPUT_DIR brkitr
                   OUTPUT_EXT .ctd
                   DEPENDS ${DAT_FILES}
                   COMMAND genctd -c -i "${DATA_BINARY_DIR}" -o @out@ @in@)
# Confusables
# Confusables are confusable because to make one output element you need
# two input elements. So, the list provided by data sources is actually
# a list of pairs, which by default contains one pair.
MESSAGE(STATUS "Creating confusable file rules")
LIST(LENGTH CFU_SOURCES CFU_SOURCES_LENGTH)
IF(NOT CFU_SOURCES_LENGTH EQUAL 2)
    MESSAGE(FATAL_ERROR "Two confusable sources are requried, but ${CFU_SOURCES_LENGTH} were found")
ENDIF()
LIST(GET CFU_SOURCES 0 CFU_SOURCES_0)
LIST(GET CFU_SOURCES 1 CFU_SOURCES_1)
ICU_DATA_TRANSFORM(OUTPUT_NAME confusables.cfu
                   DEPENDS ${DAT_FILES}
                   COMMAND gencfu -c -i "${DATA_BINARY_DIR}" -r "${CFU_SOURCES_0}" -w "${CFU_SOURCES_1}" -o @out@)

# UCM files
MESSAGE(STATUS "Creating converter file rules")
ICU_DATA_TRANSFORM(INPUT ${UCM_SOURCES}
                   OUTPUT_EXT .cnv
                   COMMAND makeconv -c -d "${DATA_BINARY_DIR}" @in@)

# RES files
MESSAGE(STATUS "Creating resource file rules")
FOREACH(POOL ${POOL_BUNDLES})
    GET_FILENAME_COMPONENT(POOL_DIR "${POOL}" PATH)
    GET_FILENAME_COMPONENT(POOL_DIR "${POOL_DIR}" NAME)
    IF(POOL_DIR STREQUAL locales)
        SET(POOL_DIR .)
    ENDIF()
    ICU_DATA_TRANSFORM(INPUT "${POOL}"
                       OUTPUT_DIR "${POOL_DIR}"
                       COMMAND icupkg -t${BYTE_ORDER_INDICATOR} @in@ @out@)
ENDFOREACH()
MESSAGE(STATUS "Creating resource file rules - brkitr")
ICU_DATA_TRANSFORM(INPUT ${BRK_RES_SOURCES}
                   OUTPUT_DIR brkitr
                   OUTPUT_EXT .res
                   DEPENDS ${DAT_FILES} ${BRK_FILES}
                   COMMAND genrb -k -i "${DATA_BINARY_DIR}" -d "${DATA_BINARY_DIR}/brkitr" @in@)
ICU_GENERATED_LOCALE_INDEX(brkitr ${BRK_RES_SOURCES})
FOREACH(DIR locales coll rbnf translit misc)
    MESSAGE(STATUS "Creating resource file rules - ${DIR}")
    STRING(TOUPPER ${DIR} UPPER_DIR)
    IF(DIR MATCHES "(locales|misc)")
        SET(ODIR .)
    ELSE()
        SET(ODIR ${DIR})
    ENDIF()
    ICU_DATA_TRANSFORM(INPUT ${${UPPER_DIR}_SOURCES}
                       OUTPUT_DIR ${ODIR}
                       OUTPUT_EXT .res
                       DEPENDS ${DAT_FILES}
                       COMMAND genrb -k -i "${DATA_BINARY_DIR}" -d "${DATA_BINARY_DIR}/${ODIR}" @in@)
    IF(DIR STREQUAL locales)
        SET(GEN_LOCALE_ADDL_ARGS CLDR_VERSION ${GENRB_CLDR_VERSION})
    ELSE()
        UNSET(GEN_LOCALE_ADDL_ARGS)
    ENDIF()
    IF(NOT DIR MATCHES "(misc|translit)")
        ICU_GENERATED_LOCALE_INDEX(${ODIR} ${GEN_LOCALE_ADDL_ARGS} ${${UPPER_DIR}_SOURCES})
    ENDIF()
ENDFOREACH()
FOREACH(DIR curr lang region zone)
    MESSAGE(STATUS "Creating resource file rules - ${DIR}")
    STRING(TOUPPER ${DIR} UPPER_DIR)
    ICU_DATA_TRANSFORM(INPUT ${${UPPER_DIR}_SOURCES}
                       OUTPUT_DIR ${DIR}
                       OUTPUT_EXT .res
                       DEPENDS "${DATA_BINARY_DIR}/${DIR}/pool.res" ${DAT_FILES}
                       COMMAND genrb --usePoolBundle "${DATA_BINARY_DIR}/${DIR}" -k -i "${DATA_BINARY_DIR}" -d "${DATA_BINARY_DIR}/${DIR}" @in@)
    ICU_GENERATED_LOCALE_INDEX(${DIR} ${${UPPER_DIR}_SOURCES})
ENDFOREACH()

# SPP files
MESSAGE(STATUS "Creating stringprep file rules")
LIST(LENGTH SPREP_SOURCES SPREP_SOURCES_LENGTH)
IF(SPREP_SOURCES_LENGTH GREATER 0)
    LIST(GET SPREP_SOURCES 0 SPREP_SOURCES_0)
    GET_FILENAME_COMPONENT(SPREP_SOURCES_DIR "${SPREP_SOURCES_0}" PATH)
    ICU_DATA_TRANSFORM(INPUT ${SPREP_SOURCES}
                       OUTPUT_EXT .spp
                       DEPENDS "${DATA_BINARY_DIR}/unames.icu"
                       COMMAND gensprep -d "${DATA_BINARY_DIR}" -i "${DATA_BINARY_DIR}" -b @out.name.we@ -m "${CMAKE_CURRENT_SOURCE_DIR}/unidata" -u 3.2.0 -s "${SPREP_SOURCES_DIR}" @in.name@)
ENDIF()

# Create the data archive
MESSAGE(STATUS "Creating data archive rules")
SET(DATA_INPUT_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/${DATA_NAME}.txt")
FILE(WRITE "${DATA_INPUT_LIST_FILE}" "# List of all ${DATA_NAME} elements\n")
ICU_ALL_DATA_OUTPUT(DATA_ELEMENTS)
FOREACH(ELEM ${DATA_ELEMENTS})
    FILE(RELATIVE_PATH RELATIVE_ELEM "${DATA_BINARY_DIR}" "${ELEM}")
    FILE(APPEND "${DATA_INPUT_LIST_FILE}" "${RELATIVE_ELEM}\n")
ENDFOREACH()
ADD_CUSTOM_COMMAND(OUTPUT "${DATA_ARCHIVE}"
                   COMMAND icupkg new "${DATA_ARCHIVE}"
                   COMMAND icupkg -a "${DATA_INPUT_LIST_FILE}" -s "${DATA_BINARY_DIR}" "${DATA_ARCHIVE}"
                   DEPENDS ${DATA_ELEMENTS})

