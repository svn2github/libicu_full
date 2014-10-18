# load generated paths
include(IcuPaths)

FILE(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/icu" NATIVE_ICU_PLUGINS)
STRING(REPLACE "\\" "\\\\" NATIVE_ICU_PLUGINS "${NATIVE_ICU_PLUGINS}")
ADD_DEFINITIONS(-DU_COMMON_IMPLEMENTATION -DDEFAULT_ICU_PLUGINS="${NATIVE_ICU_PLUGINS}")
IF(DATA_PACKAGING_MODE STREQUAL common)
    FILE(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/${DATA_DIR}" NATIVE_DATA_DEFAULT)
    STRING(REPLACE "\\" "\\\\" NATIVE_DATA_RESULT "${NATIVE_DATA_RESULT}")
    ADD_DEFINITIONS(-DU_ICU_DATA_DEFAULT_DIR="${NATIVE_DATA_RESULT}")
ENDIF()
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/i18n")
ICU_LIBRARY(${COMMON_STUB}
    ${ICU_common}
    # public headers
    unicode/appendable.h unicode/brkiter.h unicode/bytestream.h unicode/bytestrie.h
    unicode/bytestriebuilder.h unicode/caniter.h unicode/chariter.h unicode/dbbi.h
    unicode/docmain.h unicode/dtintrv.h unicode/errorcode.h unicode/icudataver.h unicode/icuplug.h
    unicode/idna.h unicode/localpointer.h unicode/locid.h unicode/messagepattern.h
    unicode/normalizer2.h unicode/normlzr.h unicode/parseerr.h unicode/parsepos.h
    unicode/platform.h unicode/ptypes.h unicode/putil.h unicode/rbbi.h unicode/rep.h
    unicode/resbund.h unicode/schriter.h unicode/std_string.h unicode/strenum.h
    unicode/stringpiece.h unicode/stringtriebuilder.h unicode/symtable.h unicode/ubidi.h
    unicode/ubrk.h unicode/ucasemap.h unicode/ucat.h unicode/uchar.h unicode/ucharstrie.h
    unicode/ucharstriebuilder.h unicode/uchriter.h unicode/uclean.h unicode/ucnv.h
    unicode/ucnv_cb.h unicode/ucnv_err.h unicode/ucnvsel.h unicode/uconfig.h unicode/udata.h
    unicode/uenum.h unicode/uidna.h unicode/uiter.h unicode/uloc.h unicode/umachine.h
    unicode/umisc.h unicode/unifilt.h unicode/unifunct.h unicode/unimatch.h unicode/uniset.h
    unicode/unistr.h unicode/unorm.h unicode/unorm2.h unicode/uobject.h unicode/urename.h
    unicode/urep.h unicode/ures.h unicode/uscript.h unicode/uset.h unicode/usetiter.h
    unicode/ushape.h unicode/usprep.h unicode/ustring.h unicode/ustringtrie.h unicode/utext.h
    unicode/utf.h unicode/utf16.h unicode/utf32.h unicode/utf8.h unicode/utf_old.h
    unicode/utrace.h unicode/utypes.h unicode/uvernum.h unicode/uversion.h
    ICU_LINK_LIBRARIES
    ${DATA_STUB}
    LINK_LIBRARIES
    ${LIBM} ${LIBTHREAD_WEAK} ${LIBDL})

INSTALL_ICU_LIBRARY(${COMMON_STUB})
INSTALL(DIRECTORY unicode
        DESTINATION ${INCLUDE_DIR}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        PATTERN ".svn" EXCLUDE)

