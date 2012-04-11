ADD_DEFINITIONS(-DU_I18N_IMPLEMENTATION)
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}")
ICU_LIBRARY(${I18N_STUB}
    alphaindex.cpp anytrans.cpp anytrans.h astro.cpp astro.h basictz.cpp bms.cpp bmsearch.cpp
    bocsu.cpp bocsu.h brktrans.cpp brktrans.h buddhcal.cpp buddhcal.h calendar.cpp casetrn.cpp
    casetrn.h cecal.cpp cecal.h chnsecal.cpp chnsecal.h choicfmt.cpp coleitr.cpp coll.cpp
    colldata.cpp coptccal.cpp coptccal.h cpdtrans.cpp cpdtrans.h csdetect.cpp csdetect.h
    csmatch.cpp csmatch.h csr2022.cpp csr2022.h csrecog.cpp csrecog.h csrmbcs.cpp csrmbcs.h
    csrsbcs.cpp csrsbcs.h csrucode.cpp csrucode.h csrutf8.cpp csrutf8.h curramt.cpp currfmt.cpp
    currfmt.h currpinf.cpp currunit.cpp datefmt.cpp dcfmtsym.cpp decContext.c decContext.h
    decNumber.c decNumber.h decNumberLocal.h decfmtst.cpp decfmtst.h decimfmt.cpp digitlst.cpp
    digitlst.h dtfmtsym.cpp dtitv_impl.h dtitvfmt.cpp dtitvinf.cpp dtptngen.cpp dtptngen_impl.h
    dtrule.cpp esctrn.cpp esctrn.h ethpccal.cpp ethpccal.h fmtable.cpp fmtable_cnv.cpp
    format.cpp fphdlimp.cpp fphdlimp.h fpositer.cpp funcrepl.cpp funcrepl.h gregocal.cpp
    gregoimp.cpp gregoimp.h hebrwcal.cpp hebrwcal.h indiancal.cpp indiancal.h inputext.cpp
    inputext.h islamcal.cpp islamcal.h japancal.cpp japancal.h locdspnm.cpp measfmt.cpp
    measure.cpp msgfmt.cpp msgfmt_impl.h name2uni.cpp name2uni.h nfrlist.h nfrs.cpp nfrs.h
    nfrule.cpp nfrule.h nfsubs.cpp nfsubs.h nortrans.cpp nortrans.h nultrans.cpp nultrans.h
    numfmt.cpp numsys.cpp numsys_impl.h olsontz.cpp olsontz.h persncal.cpp persncal.h plurfmt.cpp
    plurrule.cpp plurrule_impl.h quant.cpp quant.h rbnf.cpp rbt.cpp rbt.h rbt_data.cpp rbt_data.h
    rbt_pars.cpp rbt_pars.h rbt_rule.cpp rbt_rule.h rbt_set.cpp rbt_set.h rbtz.cpp regexcmp.cpp
    regexcmp.h regexcst.h regeximp.h regeximp.cpp regexst.cpp regexst.h regextxt.cpp regextxt.h reldtfmt.cpp
    reldtfmt.h rematch.cpp remtrans.cpp remtrans.h repattrn.cpp search.cpp selfmt.cpp
    selfmtimpl.h simpletz.cpp smpdtfmt.cpp smpdtfst.cpp smpdtfst.h sortkey.cpp strmatch.cpp
    strmatch.h strrepl.cpp strrepl.h stsearch.cpp taiwncal.cpp taiwncal.h tblcoll.cpp
    timezone.cpp titletrn.cpp titletrn.h tmunit.cpp tmutamt.cpp tmutfmt.cpp tolowtrn.cpp
    tolowtrn.h toupptrn.cpp toupptrn.h translit.cpp transreg.cpp transreg.h tridpars.cpp
    tridpars.h tzfmt.cpp tzgnames.cpp tzgnames.h tznames.cpp tznames_impl.cpp
    tznames_impl.h tzrule.cpp tztrans.cpp ucal.cpp ucln_in.c ucln_in.h ucol.cpp ucol_bld.cpp
    ucol_bld.h ucol_cnt.cpp ucol_cnt.h ucol_elm.cpp ucol_elm.h ucol_imp.h ucol_res.cpp
    ucol_sit.cpp ucol_tok.cpp ucol_tok.h ucol_wgt.cpp ucol_wgt.h ucoleitr.cpp ucsdet.cpp
    ucurr.cpp ucurrimp.h udat.cpp udateintervalformat.cpp udatpg.cpp ulocdata.c umsg.cpp
    umsg_imp.h unesctrn.cpp unesctrn.h uni2name.cpp uni2name.h unum.cpp upluralrules.cpp
    uregex.cpp uregexc.cpp usearch.cpp uspoof.cpp uspoof_build.cpp uspoof_conf.cpp uspoof_conf.h
    uspoof_impl.cpp uspoof_impl.h uspoof_wsconf.cpp uspoof_wsconf.h usrchimp.h utmscale.c
    utrans.cpp vtzone.cpp vzone.cpp vzone.h windtfmt.cpp windtfmt.h winnmfmt.cpp winnmfmt.h
    wintzimpl.cpp wintzimpl.h zonemeta.cpp zonemeta.h zrule.cpp zrule.h ztrans.cpp ztrans.h
    # public headers
    unicode/alphaindex.h unicode/basictz.h unicode/bms.h unicode/bmsearch.h unicode/calendar.h
    unicode/choicfmt.h unicode/coleitr.h unicode/coll.h unicode/colldata.h unicode/curramt.h
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

