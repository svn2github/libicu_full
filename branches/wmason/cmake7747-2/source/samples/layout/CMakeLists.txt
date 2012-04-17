ADD_DEFINITIONS(-DLE_USE_CMEMORY)
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/layout"
                    "${CMAKE_SOURCE_DIR}/layoutex")

SET(LAYOUT_BUILDABLE TRUE)
UNSET(PLATFORM_LIBS)

# The pkg-config search fore the required libraries fails
# when non-default bitness is being build. If the variable
# BITS_TO_CHECK is set, then we know we have non-default
# bitness.
IF(UNIX AND NOT BITS_TO_CHECK)
    FIND_PACKAGE(PkgConfig)
    IF(NOT PKG_CONFIG_FOUND)
        MESSAGE(WARNING "pkg-config was not found, so the layout sample cannot be built")
        SET(LAYOUT_BUILDABLE FALSE)
    ELSE()
        FOREACH(PKG libgnomeui-2.0 freetype2 cairo)
            STRING(REGEX REPLACE "[-.]" "" PKG_PREFIX ${PKG})
            pkg_check_modules(${PKG_PREFIX} ${PKG})
            IF(NOT ${PKG_PREFIX}_FOUND)
                MESSAGE(WARNING "${PKG} was not found, so the layout sample cannot be built")
                SET(LAYOUT_BUILDABLE FALSE)
                BREAK()
            ENDIF()
            INCLUDE_DIRECTORIES(${${PKG_PREFIX}_INCLUDE_DIRS})
            LINK_DIRECTORIES(${${PKG_PREFIX}_LIBRARY_DIRS})
            LIST(APPEND PLATFORM_LIBS ${${PKG_PREFIX}_LIBRARIES})
        ENDFOREACH()
    ENDIF()
    SET(PLATFORM_SOURCES
        GnomeFontInstance.cpp GnomeFontInstance.h GnomeFontMap.cpp GnomeFontMap.h
        GnomeGUISupport.cpp GnomeGUISupport.h)
    SET(PLATFORM_CXX_SOURCES
        gnomelayout.cpp)
    SET(PLATFORM_C_SOURCES
        cgnomelayout.c)
ELSEIF(WIN32)
    MESSAGE(FATAL_ERROR "The Windows build is not yet done")
ENDIF()

IF(LAYOUT_BUILDABLE)
    SET(COMMON_SOURCES
        cmaps.cpp cmaps.h FontMap.cpp FontMap.h FontTableCache.cpp FontTableCache.h
        paragraph.cpp paragraph.h ScriptCompositeFontInstance.cpp
        ScriptCompositeFontInstance.h UnicodeReader.cpp UnicodeReader.h
        ${PLATFORM_SOURCES})

    ADD_EXECUTABLE(layout EXCLUDE_FROM_ALL
                   ${COMMON_SOURCES} ${PLATFORM_CXX_SOURCES})
    LINK_ICU_LIBRARIES(layout ${LAYOUTEX_STUB} ${I18N_STUB})
    TARGET_LINK_LIBRARIES(layout ${PLATFORM_LIBS})

    ADD_EXECUTABLE(clayout EXCLUDE_FROM_ALL
        gnomeglue.cpp gnomeglue.h pflow.c pflow.h rsurface.cpp rsurface.h
        ucreader.cpp ucreader.h ${COMMON_SOURCES} ${PLATFORM_C_SOURCES})
    LINK_ICU_LIBRARIES(clayout ${LAYOUTEX_STUB} ${I18N_STUB})
    TARGET_LINK_LIBRARIES(clayout ${PLATFORM_LIBS})    

    ADD_DEPENDENCIES(samples layout clayout)
ENDIF()
