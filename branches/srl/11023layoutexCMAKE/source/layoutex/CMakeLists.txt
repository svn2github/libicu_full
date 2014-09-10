# Copyright (C) 2014, International Business Machines and Others. All Rights Reserved.

cmake_minimum_required (VERSION 2.6)
project (libiculx)

find_package(PkgConfig)
pkg_check_modules (ICU_LE    icu-le-hb)
pkg_check_modules (ICU_UC    icu-uc)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ICU_UC_INCLUDE_DIRS}
    ${ICU_LE_INCLUDE_DIRS}
)

add_library(iculx SHARED
                  ParagraphLayout.cpp
                  LXUtilities.cpp
                  playout.cpp
                  plruns.cpp
                  RunArrays.cpp)

target_link_libraries(iculx
        ${ICU_UC_LDFLAGS}
        ${ICU_UC_LIBRARIES}
        ${ICU_LE_LDFLAGS}
        ${ICU_LE_LIBRARIES})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../config/icu.pc.in ${CMAKE_CURRENT_BINARY_DIR}/icu-lx.pc @ONLY)

INSTALL(TARGETS iculx)
