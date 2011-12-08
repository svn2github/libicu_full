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
    appendable.cpp bmpset.cpp bmpset.h brkeng.cpp brkeng.h brkiter.cpp
    bytestream.cpp bytestrie.cpp bytestriebuilder.cpp bytestrieiterator.cpp caniter.cpp
    chariter.cpp charstr.cpp charstr.h cmemory.c cmemory.h cpputils.h cstring.c cstring.h
    cwchar.c cwchar.h dictbe.cpp dictbe.h dtintrv.cpp errorcode.cpp filterednormalizer2.cpp
    hash.h icudataver.c icuplug.c icuplugimp.h localsvc.h locavailable.cpp locbased.cpp
    locbased.h locdispnames.cpp locid.cpp loclikely.cpp locmap.c locmap.h locresdata.cpp
    locutil.cpp locutil.h messageimpl.h messagepattern.cpp msvcres.h mutex.cpp mutex.h
    normalizer2.cpp normalizer2impl.cpp normalizer2impl.h normlzr.cpp parsepos.cpp
    patternprops.cpp patternprops.h propname.cpp propname.h propname_data.h propsvec.c propsvec.h
    punycode.cpp punycode.h putil.cpp putilimp.h rbbi.cpp rbbidata.cpp rbbidata.h rbbinode.cpp
    rbbinode.h rbbirb.cpp rbbirb.h rbbirpt.h rbbiscan.cpp rbbiscan.h rbbisetb.cpp rbbisetb.h
    rbbistbl.cpp rbbitblb.cpp rbbitblb.h resbund.cpp resbund_cnv.cpp ruleiter.cpp ruleiter.h
    schriter.cpp serv.cpp serv.h servlk.cpp servlkf.cpp servloc.h servls.cpp servnotf.cpp
    servnotf.h servrbf.cpp servslkf.cpp sprpimpl.h stringpiece.cpp stringtriebuilder.cpp
    triedict.cpp triedict.h uarrsort.c uarrsort.h uassert.h ubidi.c ubidi_props.c ubidi_props.h
    ubidiimp.h ubidiln.c ubidiwrt.c ubrk.cpp ubrkimpl.h ucase.cpp ucase.h
    ucasemap.cpp ucasemap_titlecase_brkiter.cpp ucat.c uchar.c ucharstrie.cpp
    ucharstriebuilder.cpp ucharstrieiterator.cpp uchriter.cpp ucln.h ucln_cmn.c
    ucln_cmn.h ucln_imp.h ucmndata.c ucmndata.h ucnv.c ucnv2022.cpp
    ucnv_bld.cpp ucnv_bld.h ucnv_cb.c ucnv_cnv.c ucnv_cnv.h ucnv_ct.c ucnv_err.c ucnv_ext.cpp
    ucnv_ext.h ucnv_imp.h ucnv_io.cpp ucnv_io.h ucnv_lmb.c ucnv_set.c ucnv_u16.c ucnv_u32.c
    ucnv_u7.c ucnv_u8.c ucnvbocu.cpp ucnvdisp.c ucnvhz.c ucnvisci.c ucnvlat1.c ucnvmbcs.c
    ucnvmbcs.h ucnvscsu.c ucnvsel.cpp ucol_data.h ucol_swp.cpp ucol_swp.h udata.cpp udatamem.c
    udatamem.h udataswp.c udataswp.h uelement.h uenum.c uenumimp.h uhash.c uhash.h uhash_us.cpp
    uidna.cpp uinit.c uinvchar.c uinvchar.h uiter.cpp ulist.c ulist.h uloc.cpp uloc_tag.c
    ulocimp.h umapfile.c umapfile.h umath.c umutex.c umutex.h unames.cpp unifilt.cpp
    unifunct.cpp uniset.cpp uniset_closure.cpp uniset_props.cpp unisetspan.cpp unisetspan.h
    unistr.cpp unistr_case.cpp unistr_case_locale.cpp unistr_cnv.cpp unistr_props.cpp
    unistr_titlecase_brkiter.cpp unorm.cpp unorm_it.c unorm_it.h unormcmp.cpp unormimp.h
    uobject.cpp uposixdefs.h uprops.cpp uprops.h ures_cnv.c uresbund.cpp uresdata.c uresdata.h
    uresimp.h ureslocs.h usc_impl.c usc_impl.h uscript.c uset.cpp uset_imp.h uset_props.cpp
    usetiter.cpp ushape.cpp usprep.cpp ustack.cpp ustr_cnv.c ustr_cnv.h ustr_imp.h
    ustr_titlecase_brkiter.cpp ustr_wcs.cpp ustrcase.cpp ustrcase_locale.cpp ustrenum.cpp
    ustrenum.h ustrfmt.c ustrfmt.h ustring.cpp ustrtrns.cpp utext.cpp utf_impl.c util.cpp
    util.h util_props.cpp utrace.c utracimp.h utrie.cpp utrie.h utrie2.cpp utrie2.h
    utrie2_builder.cpp utrie2_impl.h uts46.cpp utypes.c uvector.cpp uvector.h uvectr32.cpp
    uvectr32.h uvectr64.cpp uvectr64.h wintz.c wintz.h
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
    ${LIBDL})

INSTALL_ICU_LIBRARY(${COMMON_STUB})
INSTALL(DIRECTORY unicode
        DESTINATION ${INCLUDE_DIR}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        PATTERN ".svn" EXCLUDE)

