ADD_DEFINITIONS(-DU_IO_IMPLEMENTATION)
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n")
ICU_LIBRARY(${IO_STUB}
    locbund.cpp locbund.h sprintf.c sscanf.c ucln_io.c ucln_io.h ufile.c ufile.h
    ufmt_cmn.c ufmt_cmn.h uprintf.c uprintf.h uprntf_p.c uscanf.c uscanf.h
    uscanf_p.c ustdio.c ustream.cpp
    # public headers
    unicode/ustdio.h unicode/ustream.h
    ICU_LINK_LIBRARIES
    ${I18N_STUB})

INSTALL_ICU_LIBRARY(${IO_STUB})
INSTALL(DIRECTORY unicode
        DESTINATION ${INCLUDE_DIR}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        PATTERN ".svn" EXCLUDE)

