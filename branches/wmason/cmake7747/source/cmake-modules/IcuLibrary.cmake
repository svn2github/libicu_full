# Utility macro for use in this module to retrieve a target name
# from a stub. Upon exit SHARED_VAR will hold the name of the
# shared library target, and STATIC_VAR will hold the name of the
# static library target.
MACRO(TARGET_NAMES_FROM_STUB STUB_NAME SHARED_VAR STATIC_VAR)
    # Don't try to make an ICU library name from a non-ICU library stub
    LIST(FIND ALL_STUBS ${STUB_NAME} STUB_INDEX)
    IF(STUB_INDEX EQUAL -1)
        UNSET(${SHARED_VAR})
        UNSET(${STATIC_VAR})
    ELSE()
        SET(${SHARED_VAR} "${ICU_PREFIX}${STUB_NAME}")
        IF(WIN32)
            SET(${SHARED_VAR} "${${SHARED_VAR}}${ICU_LIB_MAJOR_VERSION}")
        ENDIF()
        SET(${STATIC_VAR} "${ICU_STATIC_PREFIX}${ICU_PREFIX}${STUB_NAME}")
        IF(WIN32)
            SET(${STATIC_VAR} "${${STATIC_VAR}}${ICU_LIB_MAJOR_VERSION}")
        ENDIF()
    ENDIF()
ENDMACRO()

# Link ICU libraries to a given target. Following TARGET_NAME is a list of
# stub names of ICU libraries.
#
# There is a trick here. If static is enabled, then generally we want to
# link everything to the static libraries. However, if shared is also enabled,
# then if the TARGET_NAME here corresponds to one of the ICU shared libraries,
# then we must link it to other ICU shared libraries. So, we check the target
# below and link appropriately depending on whether it is an ICU library or
# not.
#
# Example:
# LINK_ICU_LIBRARIES(target1 ${I18N_STUB} ${COMMON_STUB} ${DATA_STUB})
MACRO(LINK_ICU_LIBRARIES TARGET_NAME)
    IF(${ARGC} GREATER 1)
        # See if we are dealing with a shared ICU library target while also
        # building statically.
        IF(ENABLE_STATIC)
            SET(FORCE_SHARED_LINKAGE FALSE)
            IF(ENABLE_SHARED)
                FOREACH(STUB ${ALL_STUBS})
                    TARGET_NAMES_FROM_STUB(${STUB} LINK_SHARED_TARGET LINK_STATIC_TARGET)
                    IF(${TARGET_NAME} STREQUAL ${LINK_SHARED_TARGET})
                        SET(FORCE_SHARED_LINKAGE TRUE)
                        BREAK()
                    ENDIF()
                ENDFOREACH()
            ENDIF()
        ELSE()
            SET(FORCE_SHARED_LINKAGE TRUE)
        ENDIF()
        UNSET(ICU_LIBRARIES_TO_LINK)
        FOREACH(LIB ${ARGN})
            TARGET_NAMES_FROM_STUB(${LIB} LINK_SHARED_TARGET LINK_STATIC_TARGET)
            IF(FORCE_SHARED_LINKAGE)
                LIST(APPEND ICU_LIBRARIES_TO_LINK ${LINK_SHARED_TARGET})
            ELSE()
                LIST(APPEND ICU_LIBRARIES_TO_LINK ${LINK_STATIC_TARGET})
            ENDIF()
        ENDFOREACH()
        TARGET_LINK_LIBRARIES(${TARGET_NAME} ${ICU_LIBRARIES_TO_LINK})
    ENDIF()
ENDMACRO()

# Create an ICU library target using the given STUB_NAME. Arguments following
# STUB_NAME consist of source file names, followed by an optional
# ICU_LINK_LIBRARIES token, followed by stub names of the ICU libraries that
# should be linked to the library being defined, followed by an optional
# LINK_LIBRARIES token, followed by a list of other libraries that should be
# linked to the library being defined.
#
# Note: ICU libraries are always referred to by their stub name.
#
# Example:
# ICU_LIBRARY(example
#             source1.cpp source1.h
#             source2.cpp source2.h
#             ICU_LINK_LIBRARIES
#             ${DATA_STUB}
#             LINK_LIBRARIES
#             m)
MACRO(ICU_LIBRARY STUB_NAME)
    UNSET(ICU_LIBRARY_SOURCES)
    UNSET(ICU_LIBRARY_ICU_LINK_LIBRARIES)
    UNSET(ICU_LIBRARY_LINK_LIBRARIES)
    SET(ICU_LIBRARY_CURRENT_LIST ICU_LIBRARY_SOURCES)
    FOREACH(ITEM ${ARGN})
        STRING(TOUPPER "${ITEM}" UPPER_ITEM)
        IF(UPPER_ITEM STREQUAL ICU_LINK_LIBRARIES)
            SET(ICU_LIBRARY_CURRENT_LIST ICU_LIBRARY_ICU_LINK_LIBRARIES)
        ELSEIF(UPPER_ITEM STREQUAL LINK_LIBRARIES)
            SET(ICU_LIBRARY_CURRENT_LIST ICU_LIBRARY_LINK_LIBRARIES)
        ELSE()
            LIST(APPEND ${ICU_LIBRARY_CURRENT_LIST} "${ITEM}")
        ENDIF()
    ENDFOREACH()
    TARGET_NAMES_FROM_STUB(${STUB_NAME} SHARED_TARGET STATIC_TARGET)
    IF(ENABLE_SHARED)
        ADD_LIBRARY(${SHARED_TARGET} SHARED ${ICU_LIBRARY_SOURCES})
        LINK_ICU_LIBRARIES(${SHARED_TARGET} ${ICU_LIBRARY_ICU_LINK_LIBRARIES})
        TARGET_LINK_LIBRARIES(${SHARED_TARGET} ${ICU_LIBRARY_LINK_LIBRARIES})
        SET_TARGET_PROPERTIES(${SHARED_TARGET} PROPERTIES
                              VERSION ${ICU_VERSION}
                              SOVERSION ${ICU_LIB_MAJOR_VERSION})
        IF(WIN32)
            SET_TARGET_PROPERTIES(${SHARED_TARGET} PROPERTIES
                                  OUTPUT_NAME_DEBUG "${SHARED_TARGET}d")
        ENDIF()
        # If static is also enabled, then there will be a top-level directory property
        # compile definition of U_STATIC_IMPLEMENTATION that is inherited by every
        # subdirectory. We want to turn that off in this directory and set it on the
        # static target alone. See below...
        IF(ENABLE_STATIC)
            GET_DIRECTORY_PROPERTY(DIR_DEFS COMPILE_DEFINITIONS)
            IF(DIR_DEFS)
                LIST(REMOVE_ITEM DIR_DEFS U_STATIC_IMPLEMENTATION)
                # Don't use SET_DIRECTORY_PROPERTY here because it occasionally
                # does not work with CMake 2.8.5.
                SET_PROPERTY(DIRECTORY PROPERTY COMPILE_DEFINITIONS ${DIR_DEFS})
            ENDIF()
        ENDIF()
    ENDIF()
    IF(ENABLE_STATIC)
        ADD_LIBRARY(${STATIC_TARGET} STATIC ${ICU_LIBRARY_SOURCES})
        LINK_ICU_LIBRARIES(${STATIC_TARGET} ${ICU_LIBRARY_ICU_LINK_LIBRARIES})
        TARGET_LINK_LIBRARIES(${STATIC_TARGET} ${ICU_LIBRARY_LINK_LIBRARIES})
        IF(WIN32)
            SET_TARGET_PROPERTIES(${STATIC_TARGET} PROPERTIES
                                  OUTPUT_NAME_DEBUG "${STATIC_TARGET}d")
        ENDIF()
        # If shared is also enabled, then we want to set the U_STATIC_IMPLEMENTATION
        # definition back on this target, since we just removed it from this directory. See
        # above...
        IF(ENABLE_SHARED)
            SET_TARGET_PROPERTIES(${STATIC_TARGET} PROPERTIES
                                  COMPILE_DEFINITIONS U_STATIC_IMPLEMENTATION)
        ENDIF()
    ENDIF()
ENDMACRO()

MACRO(INSTALL_ICU_LIBRARY STUB_NAME)
    TARGET_NAMES_FROM_STUB(${STUB_NAME} SHARED_TARGET STATIC_TARGET)
    UNSET(TARGETS_TO_INSTALL)
    IF(ENABLE_SHARED)
        LIST(APPEND TARGETS_TO_INSTALL ${SHARED_TARGET})
    ENDIF()
    IF(ENABLE_STATIC)
        LIST(APPEND TARGETS_TO_INSTALL ${STATIC_TARGET})
    ENDIF()
    INSTALL(TARGETS ${TARGETS_TO_INSTALL}
            ARCHIVE DESTINATION ${LIB_DIR}
            LIBRARY DESTINATION ${LIB_DIR}
            RUNTIME DESTINATION ${BIN_DIR})
ENDMACRO()

