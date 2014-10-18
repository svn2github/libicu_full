# load generated paths
include(IcuPaths)

ADD_DEFINITIONS(-DU_I18N_IMPLEMENTATION)
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}")
ICU_LIBRARY(${I18N_STUB}
    ${ICU_i18n}
    # public headers
    unicode/alphaindex.h unicode/basictz.h  unicode/calendar.h
    unicode/choicfmt.h unicode/coleitr.h unicode/coll.h  unicode/curramt.h
    unicode/currpinf.h unicode/currunit.h unicode/datefmt.h unicode/dcfmtsym.h
    unicode/decimfmt.h unicode/dtfmtsym.h unicode/dtitvfmt.h unicode/dtitvinf.h
    unicode/dtptngen.h unicode/dtrule.h unicode/fieldpos.h unicode/fmtable.h unicode/format.h
    unicode/fpositer.h unicode/gregocal.h unicode/locdspnm.h unicode/measfmt.h unicode/measunit.h
    unicode/measure.h unicode/msgfmt.h unicode/numfmt.h unicode/numsys.h unicode/plurfmt.h
    unicode/plurrule.h unicode/rbnf.h unicode/rbtz.h unicode/regex.h unicode/search.h
    unicode/selfmt.h unicode/simpletz.h unicode/smpdtfmt.h unicode/sortkey.h unicode/stsearch.h
    unicode/tblcoll.h unicode/timezone.h unicode/tmunit.h unicode/tmutamt.h unicode/tmutfmt.h
    unicode/translit.h unicode/tzfmt.h unicode/tznames.h unicode/tzrule.h unicode/tztrans.h
    unicode/ucal.h unicode/ucol.h unicode/ucoleitr.h unicode/ucsdet.h unicode/ucurr.h unicode/udat.h
    unicode/udateintervalformat.h unicode/udatpg.h unicode/uldnames.h unicode/ulocdata.h
    unicode/umsg.h unicode/unirepl.h unicode/unum.h unicode/upluralrules.h unicode/uregex.h
    unicode/usearch.h unicode/uspoof.h unicode/utmscale.h unicode/utrans.h unicode/vtzone.h
    ICU_LINK_LIBRARIES
    ${COMMON_STUB})

INSTALL_ICU_LIBRARY(${I18N_STUB})
INSTALL(DIRECTORY unicode
        DESTINATION ${INCLUDE_DIR}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        PATTERN ".svn" EXCLUDE)

