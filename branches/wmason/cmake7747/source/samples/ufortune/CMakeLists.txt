# Can be dll, static, common or files
SET(UFORTUNE_RESOURCE_MODE static CACHE STRING "The resource mode of ufortune: dll, static, common or files")

IF(NOT UFORTUNE_RESOURCE_MODE MATCHES "dll|static")
    ADD_DEFINITIONS(-DUFORTUNE_NOSETAPPDATA)
ENDIF()

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n"
                    "${CMAKE_SOURCE_DIR}/io")

ADD_EXECUTABLE(ufortune EXCLUDE_FROM_ALL
    ufortune.c)

ADD_SUBDIRECTORY(resources)

IF(UFORTUNE_RESOURCE_MODE MATCHES "dll|static")
    TARGET_LINK_LIBRARIES(ufortune ufortune_resources)
ENDIF()
LINK_ICU_LIBRARIES(ufortune ${IO_STUB})

