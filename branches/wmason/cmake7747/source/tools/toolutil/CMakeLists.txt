ADD_DEFINITIONS(-DU_TOOLUTIL_IMPLEMENTATION)
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n")
ICU_LIBRARY(${TOOLUTIL_STUB}
    dbgutil.cpp dbgutil.h denseranges.cpp denseranges.h filestrm.c filestrm.h filetools.cpp
    filetools.h flagparser.c flagparser.h package.cpp package.h pkg_genc.c pkg_genc.h
    pkg_gencmn.c pkg_gencmn.h pkg_icu.cpp pkg_icu.h pkg_imp.h pkgitems.cpp swapimpl.cpp
    swapimpl.h toolutil.cpp toolutil.h ucbuf.c ucbuf.h ucln_tu.c ucm.c ucm.h ucmstate.c
    udbgutil.cpp udbgutil.h unewdata.c unewdata.h uoptions.c uoptions.h uparse.c uparse.h
    writesrc.c writesrc.h xmlparser.cpp xmlparser.h
    ICU_LINK_LIBRARIES
    ${I18N_STUB})

