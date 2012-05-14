# Make sure this variable is set for anyone including this file
IF(VERBOSE_DATA)
    SET(DATA_ECHO_COMMAND echo)
ELSE()
    SET(DATA_ECHO_COMMAND echo_append)
ENDIF()

# This macro sets a variable to a set of inputs for building the data
# library. Following the DATA_VAR parameter, which is the name of the
# variable that will be set, a list of globbing expressions or file
# names may follow. Additionally, if the keyword DIRECTORY is
# encountered then all glob expressions or file names that follow will
# be relative to the give directory. The keyword FILES may appear at
# any point and can be used to visually offset the appearance of a
# given directory from a list of globbing expressions or file names.
# All file names are interpreted as being relative to
# CMAKE_CURRENT_SOURCE_DIR. If DIRECTORY is given, then its value will
# also be interpreted as being relative to the CMAKE_CURRENT_SOURCE_DIR.
MACRO(ICU_DATA_INPUT DATA_VAR)
    UNSET(${DATA_VAR})
    SET(CURRENT_INPUT_DIRECTORY .)
    UNSET(DATA_INPUT_STATE)
    FOREACH(INPUT ${ARGN})
        STRING(TOUPPER ${INPUT} UPPER_INPUT)
        IF(UPPER_INPUT STREQUAL DIRECTORY)
            SET(DATA_INPUT_STATE "DIRECTORY")
        ELSEIF(UPPER_INPUT STREQUAL FILES)
            SET(DATA_INPUT_STATE "FILES")
        ELSE()
            IF(DATA_INPUT_STATE STREQUAL "DIRECTORY")
                SET(CURRENT_INPUT_DIRECTORY ${INPUT})
            ELSEIF(DATA_INPUT_STATE STREQUAL "FILES")
                FILE(GLOB CURRENT_INPUT_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_INPUT_DIRECTORY}/${INPUT})
                IF(NOT CURRENT_INPUT_GLOB)
                    MESSAGE(ERROR "The glob pattern ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_INPUT_DIRECTORY}/${INPUT} does not match any files")
                ENDIF()
                LIST(APPEND ${DATA_VAR} ${CURRENT_INPUT_GLOB})
            ENDIF()
        ENDIF()
    ENDFOREACH()
ENDMACRO()

# This macro is intended for internal use. Unfortunately, the arguments in the COMMAND
# portion of the ADD_CUSTOM_COMMAND parameter cannot be inserted as a single list. We
# use this macro to set a sequence of 20 numbered variables, for example CMD_0. These
# are then used to define the COMMAND arguments of ADD_CUSTOM_COMMAND. Additionally,
# a variable COMMAND_TEXT is set to a textual representation that is used to print the
# command whenever VERBOSE_DATA is on.
#
# Six substitution parameters are used for various forms of the input and output
# parameters of the command:
#   @in@            : The full input name
#   @out@           : The full output name
#   @in.name@       : The file name only of the input name
#   @out.name@      : The file name only of the output name
#   @in.name.we@    : The file name without extension of the input name
#   @out.name.we@   : The file name without extension of the output name
#
MACRO(LAYOUT_COMMAND_ARGS LAYOUT_ARG_LIST LAYOUT_INPUT LAYOUT_OUTPUT)
    UNSET(COMMAND_TEXT)
    LIST(LENGTH ${LAYOUT_ARG_LIST} LAYOUT_ARG_LIST_LENGTH)
    FOREACH(NUM 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19)
        IF(${NUM} LESS LAYOUT_ARG_LIST_LENGTH)
            LIST(GET ${LAYOUT_ARG_LIST} ${NUM} LAYOUT_ARG)
            IF(LAYOUT_ARG STREQUAL @in@)
                SET(CMD_${NUM} "${LAYOUT_INPUT}")
            ELSEIF(LAYOUT_ARG STREQUAL @out@)
                SET(CMD_${NUM} "${LAYOUT_OUTPUT}")
            ELSEIF(LAYOUT_ARG STREQUAL @in.name.we@)
                GET_FILENAME_COMPONENT(LAYOUT_INPUT_WE "${LAYOUT_INPUT}" NAME_WE)
                SET(CMD_${NUM} "${LAYOUT_INPUT_WE}")
            ELSEIF(LAYOUT_ARG STREQUAL @out.name.we@)
                GET_FILENAME_COMPONENT(LAYOUT_OUTPUT_WE "${LAYOUT_OUTPUT}" NAME_WE)
                SET(CMD_${NUM} "${LAYOUT_OUTPUT_WE}")
            ELSEIF(LAYOUT_ARG STREQUAL @in.name@)
                GET_FILENAME_COMPONENT(LAYOUT_INPUT_NAME "${LAYOUT_INPUT}" NAME)
                SET(CMD_${NUM} "${LAYOUT_INPUT_NAME}")
            ELSEIF(LAYOUT_ARG STREQUAL @out.name@)
                GET_FILENAME_COMPONENT(LAYOUT_OUTPUT_NAME "${LAYOUT_OUTPUT}" NAME)
                SET(CMD_${NUM} "${LAYOUT_OUTPUT_NAME}")
            ELSE()
                SET(CMD_${NUM} "${LAYOUT_ARG}")
            ENDIF()
            IF(VERBOSE_DATA)
                SET(COMMAND_TEXT "${COMMAND_TEXT}${CMD_${NUM}} ")
            ENDIF()
        ELSE()
            UNSET(CMD_${NUM})
        ENDIF()
    ENDFOREACH()
ENDMACRO()

# This macro is currently unused, but I left it here just in case.
MACRO(ICU_DATA_GENERATED_INPUT)
    UNSET(GEN_INPUT_COMMANDS)
    UNSET(GEN_INPUT_NAME)
    UNSET(GEN_INPUT_STATE)
    FOREACH(ARG ${ARGN})
        STRING(TOUPPER ${ARG} UPPER_ARG)
        IF(UPPER_ARG STREQUAL INPUT_NAME)
            SET(GEN_INPUT_STATE "INPUT_NAME")
        ELSEIF(UPPER_ARG STREQUAL COMMAND)
            SET(GEN_INPUT_STATE "COMMAND")
        ELSE()
            IF(GEN_INPUT_STATE STREQUAL "INPUT_NAME")
                SET(GEN_INPUT_NAME ${ARG})
            ELSEIF(GEN_INPUT_STATE STREQUAL "COMMAND")
                LIST(APPEND GEN_INPUT_COMMANDS ${ARG})
            ELSE()
                MESSAGE(FATAL_ERROR "Malformed ICU_DATA_GENERATED_INPUT call: INPUT_NAME or COMMAND must appear before any other arguments")
            ENDIF()
        ENDIF()
    ENDFOREACH()
    IF(NOT GEN_INPUT_NAME)
        MESSAGE(FATAL_ERROR "INPUT_NAME is required when using the ICU_DATA_GENERATED_INPUT macro")
    ENDIF()
    LAYOUT_COMMAND_ARGS(GEN_INPUT_COMMANDS "" "")
    ADD_CUSTOM_COMMAND(OUTPUT "${GEN_INPUT_NAME}"
                       COMMAND "${CMAKE_COMMAND}" -E ${DATA_ECHO_COMMAND} ${COMMAND_TEXT}
                       COMMAND ${CMD_0} ${CMD_1} ${CMD_2} ${CMD_3} ${CMD_4} ${CMD_5} ${CMD_6} ${CMD_7} ${CMD_8} ${CMD_9} ${CMD_10} ${CMD_11} ${CMD_12} ${CMD_13} ${CMD_14} ${CMD_15} ${CMD_16} ${CMD_17} ${CMD_18} ${CMD_19})
    LIST(APPEND ALL_DATA_INPUTS "${GEN_INPUT_NAME}")
ENDMACRO()

# This creates a locale index resource in the given OUTPUT_DIR directory. The
# arguments following OUTPUT_DIR are a list of locale-specific resource files,
# the names of which provide the locale names that are inserted into the
# locale index resource.
#
# If the keyword CLDR_VERSION appears in the argument list, then then argument
# following it is inserted into the resource under the "CLDRVersion" keyword.
MACRO(ICU_GENERATED_LOCALE_INDEX OUTPUT_DIR)
    SET(GEN_LOCALE_ARGN ${ARGN})
    UNSET(GEN_LOCALE_CLDR_VERSION)
    FOREACH(ELEMENT ${GEN_LOCALE_ARGN})
        IF(GEN_LOCALE_WANTS_CLDR_VERSION)
            SET(GEN_LOCALE_CLDR_VERSION ${ELEMENT})
            SET(GEN_LOCALE_WANTS_CLDR_VERSION FALSE)
        ELSE()
            STRING(TOUPPER ${ELEMENT} UPPER_ELEMENT)
            IF(UPPER_ELEMENT MATCHES "^CLDR_VERSION$")
                SET(GEN_LOCALE_WANTS_CLDR_VERSION TRUE)
            ELSE()
                SET(GEN_LOCALE_WANTS_CLDR_VERSION FALSE)
            ENDIF()
        ENDIF()
    ENDFOREACH()
    LIST(REMOVE_ITEM GEN_LOCALE_ARGN CLDR_VERSION ${GEN_LOCALE_CLDR_VERSION})
    SET(FNAME "${DATA_BINARY_DIR}/${OUTPUT_DIR}/res_index.txt")
    FILE(WRITE "${FNAME}" "// WARNING: This file is automatically generated\n")
    FILE(APPEND "${FNAME}" "res_index:table(nofallback) {\n")
    IF(DEFINED GEN_LOCALE_CLDR_VERSION)
        FILE(APPEND "${FNAME}" "    CLDRVersion { \"${GEN_LOCALE_CLDR_VERSION}\" }\n")
    ENDIF()
    FILE(APPEND "${FNAME}" "    InstalledLocales {\n")
    FOREACH(ARG ${GEN_LOCALE_ARGN})
        GET_FILENAME_COMPONENT(GEN_LOCALE_NAME "${ARG}" NAME_WE)
        IF(NOT GEN_LOCALE_NAME STREQUAL root)
            FILE(APPEND "${FNAME}" "        ${GEN_LOCALE_NAME} { \"\" }\n")
        ENDIF()
    ENDFOREACH()
    FILE(APPEND "${FNAME}" "    }\n")
    FILE(APPEND "${FNAME}" "}\n")
    LIST(APPEND ${INPUT_LIST_VAR} ${FNAME})
    LIST(APPEND ALL_DATA_INPUTS ${FNAME})
    IF(VERBOSE_DATA)
        SET(COMMAND_TEXT genrb -k -i "${DATA_BINARY_DIR}" -s "${DATA_BINARY_DIR}/${OUTPUT_DIR}" -d "${DATA_BINARY_DIR}/${OUTPUT_DIR}" ${FNAME})
    ENDIF()
    GET_FILENAME_COMPONENT(OUTPUT "${DATA_BINARY_DIR}/${OUTPUT_DIR}/res_index.res" REALPATH)
    ADD_CUSTOM_COMMAND(OUTPUT "${OUTPUT}"
                       COMMAND "${CMAKE_COMMAND}" -E ${DATA_ECHO_COMMAND} ${COMMAND_TEXT}
                       COMMAND genrb -k -i "${DATA_BINARY_DIR}" -s "${DATA_BINARY_DIR}/${OUTPUT_DIR}" -d "${DATA_BINARY_DIR}/${OUTPUT_DIR}" ${FNAME})
    LIST(APPEND ALL_DATA_OUTPUTS "${OUTPUT}")
ENDMACRO()

# Create build rules for transforming a set of data inputs into a set of data outputs.
# The data outputs are then used to create the data archive. A number of keywords can
# be used to control the behavior:
#
#   INPUT : <optional> A list of data inputs. If INPUT is not set, then OUTPUT_NAME must be set.
#   OUTPUT : <optional> A list to hold the outputs that are identified.
#   COMMAND : The command that will be used to create the outputs. A number of substitution
#             parameters are supported, since a new command is generated for each input.
#             The substitution parameters are: @in@, @out@, @in.name@, @out.name@,
#             @in.name.we@, and @out.name.we@.
#   DEPENDS : <optional> A list of things on which the generated commands will depend.
#   OUTPUT_DIR : <optional> The directory, relative to DATA_BINARY_DIR, in which to place outputs.
#   OUTPUT_EXT : <optional> The file name extension of the generated outputs.
#   OUTPUT_NAME : <optional> A set name of a single output. Either INPUT or OUTPUT_NAME must be set.
#
MACRO(ICU_DATA_TRANSFORM)
    SET(XFORM_OUTPUT_DIR .)
    UNSET(XFORM_OUTPUT_EXT)
    UNSET(XFORM_OUTPUT_NAME)
    UNSET(XFORM_INPUTS)
    UNSET(XFORM_STATE)
    UNSET(XFORM_COMMAND)
    SET(XFORM_KEYWORDS OUTPUT INPUT COMMAND DEPENDS OUTPUT_DIR OUTPUT_EXT OUTPUT_NAME)
    FOREACH(ARG ${ARGN})
        STRING(TOUPPER ${ARG} UPPER_ARG)
        LIST(FIND XFORM_KEYWORDS "${UPPER_ARG}" XFORM_KEYWORD_INDEX)
        IF(XFORM_KEYWORD_INDEX EQUAL -1)
            # It is important to use MATCHES here instead of STREQUAL. With STREQUAL
            # thrre are some weird interactions going on that prevent the checks from
            # working.
            IF(XFORM_STATE MATCHES "^OUTPUT$")
                SET(XFORM_OUTPUT ${ARG})
            ELSEIF(XFORM_STATE MATCHES "INPUT")
                LIST(APPEND XFORM_INPUTS ${ARG})
            ELSEIF(XFORM_STATE MATCHES "COMMAND")
                LIST(APPEND XFORM_COMMAND ${ARG})
            ELSEIF(XFORM_STATE MATCHES "DEPENDS")
                LIST(APPEND XFORM_DEPENDS ${ARG})
            ELSEIF(XFORM_STATE MATCHES "OUTPUT_DIR")
                SET(XFORM_OUTPUT_DIR ${ARG})
            ELSEIF(XFORM_STATE MATCHES "OUTPUT_EXT")
                SET(XFORM_OUTPUT_EXT ${ARG})
            ELSEIF(XFORM_STATE MATCHES "OUTPUT_NAME")
                SET(XFORM_OUTPUT_NAME ${ARG})
            ENDIF()
        ELSE()
            SET(XFORM_STATE "${UPPER_ARG}")
        ENDIF()
    ENDFOREACH()
    IF(XFORM_INPUTS)
        FOREACH(INPUT ${XFORM_INPUTS})
            IF(XFORM_OUTPUT_NAME)
                SET(CUR_OUT_NAME "${XFORM_OUTPUT_NAME}")
            ELSE()
                GET_FILENAME_COMPONENT(CUR_OUT_NAME "${INPUT}" NAME)
                IF(XFORM_OUTPUT_EXT AND CUR_OUT_NAME MATCHES ".+\\.")
                    STRING(REGEX REPLACE "^(.+)\\..*$" "\\1${XFORM_OUTPUT_EXT}" CUR_OUT_NAME "${CUR_OUT_NAME}")
                ENDIF()
            ENDIF()
            IF(NOT EXISTS "${DATA_BINARY_DIR}/${XFORM_OUTPUT_DIR}")
                FILE(MAKE_DIRECTORY "${DATA_BINARY_DIR}/${XFORM_OUTPUT_DIR}")
            ENDIF()
            SET(OUTPUT "${DATA_BINARY_DIR}/${XFORM_OUTPUT_DIR}/${CUR_OUT_NAME}")
            GET_FILENAME_COMPONENT(OUTPUT "${OUTPUT}" REALPATH)
            LIST(APPEND ALL_DATA_OUTPUTS "${OUTPUT}")
            IF(XFORM_OUTPUT)
                LIST(APPEND ${XFORM_OUTPUT} "${OUTPUT}")
            ENDIF()
            LAYOUT_COMMAND_ARGS(XFORM_COMMAND "${INPUT}" "${OUTPUT}")
            ADD_CUSTOM_COMMAND(OUTPUT "${OUTPUT}"
                               COMMAND "${CMAKE_COMMAND}" -E ${DATA_ECHO_COMMAND} ${COMMAND_TEXT}
                               COMMAND ${CMD_0} ${CMD_1} ${CMD_2} ${CMD_3} ${CMD_4} ${CMD_5} ${CMD_6} ${CMD_7} ${CMD_8} ${CMD_9} ${CMD_10} ${CMD_11} ${CMD_12} ${CMD_13} ${CMD_14} ${CMD_15} ${CMD_16} ${CMD_17} ${CMD_18} ${CMD_19} 
                               MAIN_DEPENDENCY "${INPUT}"
                               DEPENDS ${XFORM_DEPENDS})
        ENDFOREACH()
    ELSE()
        IF(NOT XFORM_OUTPUT_NAME)
            MESSAGE(FATAL_ERROR "Macro ICU_DATA_TRANSFORM: If INPUTS are empty, then OUTPUT_NAME must be set")
        ENDIF()
        SET(OUTPUT "${DATA_BINARY_DIR}/${XFORM_OUTPUT_DIR}/${XFORM_OUTPUT_NAME}")
        GET_FILENAME_COMPONENT(OUTPUT "${OUTPUT}" REALPATH)
        LIST(APPEND ALL_DATA_OUTPUTS "${OUTPUT}")
        IF(XFORM_OUTPUT)
            LIST(APPEND ${XFORM_OUTPUT} "${OUTPUT}")
        ENDIF()
        LAYOUT_COMMAND_ARGS(XFORM_COMMAND "" "${OUTPUT}")
        ADD_CUSTOM_COMMAND(OUTPUT "${OUTPUT}"
                           COMMAND "${CMAKE_COMMAND}" -E ${DATA_ECHO_COMMAND} ${COMMAND_TEXT}
                           COMMAND ${CMD_0} ${CMD_1} ${CMD_2} ${CMD_3} ${CMD_4} ${CMD_5} ${CMD_6} ${CMD_7} ${CMD_8} ${CMD_9} ${CMD_10} ${CMD_11} ${CMD_12} ${CMD_13} ${CMD_14} ${CMD_15} ${CMD_16} ${CMD_17} ${CMD_18} ${CMD_19} 
                           DEPENDS ${XFORM_DEPENDS})
    ENDIF()
ENDMACRO()

# Set OUTPUT_VAR to the list of all data outputs that were found using
# above macros, ICU_DATA_TRANSFORM and ICU_GENERATED_LOCALE_INDEX.
MACRO(ALL_ICU_DATA_OUTPUT OUTPUT_VAR)
    SET(${OUTPUT_VAR} ${ALL_DATA_OUTPUTS})
ENDMACRO()

