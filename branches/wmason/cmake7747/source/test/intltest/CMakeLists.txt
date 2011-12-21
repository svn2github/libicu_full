ADD_EXECUTABLE(intltest EXCLUDE_FROM_ALL
    aliastst.cpp aliastst.h allcoll.cpp allcoll.h alphaindextst.cpp alphaindextst.h
    apicoll.cpp apicoll.h astrotst.cpp astrotst.h bidiconf.cpp bytestrietest.cpp
    calcasts.cpp calcasts.h callimts.cpp callimts.h calregts.cpp calregts.h caltest.cpp
    caltest.h caltztst.cpp caltztst.h canittst.cpp canittst.h citrtest.cpp citrtest.h
    cntabcol.cpp cntabcol.h convtest.cpp convtest.h cpdtrtst.cpp cpdtrtst.h csdetest.cpp
    csdetest.h currcoll.cpp currcoll.h dadrcal.cpp dadrcal.h dadrcoll.cpp dadrcoll.h
    dadrfmt.cpp dadrfmt.h dcfmapts.cpp dcfmapts.h dcfmtest.cpp dcfmtest.h decoll.cpp
    decoll.h dicttest.cpp dicttest.h dtfmapts.cpp dtfmapts.h dtfmrgts.cpp dtfmrgts.h
    dtfmtrtts.cpp dtfmtrtts.h dtfmttst.cpp dtfmttst.h dtifmtts.cpp dtifmtts.h
    dtptngts.cpp dtptngts.h encoll.cpp encoll.h escoll.cpp escoll.h ficoll.cpp
    ficoll.h fldset.cpp fldset.h frcoll.cpp frcoll.h g7coll.cpp g7coll.h
    icusvtst.cpp icusvtst.h idnaconf.cpp idnaconf.h idnaref.cpp idnaref.h incaltst.cpp
    incaltst.h intltest.cpp intltest.h intltest.vcxproj intltest.vcxproj.filters
    itercoll.cpp itercoll.h itformat.cpp itformat.h itmajor.cpp itmajor.h itrbbi.cpp
    itrbbi.h itrbnf.cpp itrbnf.h itrbnfp.cpp itrbnfp.h itrbnfrt.cpp itrbnfrt.h
    itspoof.cpp itspoof.h ittrans.cpp ittrans.h itutil.cpp itutil.h jacoll.cpp
    jacoll.h jamotest.cpp jamotest.h lcukocol.cpp lcukocol.h locnmtst.cpp locnmtst.h
    loctest.cpp loctest.h miscdtfm.cpp miscdtfm.h mnkytst.cpp mnkytst.h msfmrgts.cpp
    msfmrgts.h nmfmapts.cpp nmfmapts.h nmfmtrt.cpp nmfmtrt.h normconf.cpp normconf.h
    nptrans.cpp nptrans.h numfmtst.cpp numfmtst.h numrgts.cpp numrgts.h plurfmts.cpp
    plurfmts.h plurults.cpp plurults.h pptest.cpp pptest.h punyref.c punyref.h
    rbbiapts.cpp rbbiapts.h rbbitst.cpp rbbitst.h regcoll.cpp regcoll.h regextst.cpp
    regextst.h reptest.cpp reptest.h restest.cpp restest.h restsnew.cpp restsnew.h
    rndmcoll.cpp rndmcoll.h sdtfmtts.cpp sdtfmtts.h selfmts.cpp selfmts.h
    sfwdchit.cpp sfwdchit.h simplethread.cpp simplethread.h srchtest.cpp srchtest.h
    ssearch.cpp ssearch.h strcase.cpp strtest.cpp strtest.h svccoll.cpp svccoll.h
    tchcfmt.cpp tchcfmt.h testidn.cpp testidna.cpp testidna.h testutil.cpp testutil.h
    textfile.cpp textfile.h tfsmalls.cpp tfsmalls.h thcoll.cpp thcoll.h tmsgfmt.cpp
    tmsgfmt.h tokiter.cpp tokiter.h transapi.cpp transapi.h transrt.cpp transrt.h
    transtst.cpp transtst.h trcoll.cpp trcoll.h trnserr.cpp trnserr.h tscoll.cpp
    tscoll.h tsdate.cpp tsdate.h tsdcfmsy.cpp tsdcfmsy.h tsdtfmsy.cpp tsdtfmsy.h
    tsmthred.cpp tsmthred.h tsnmfmt.cpp tsnmfmt.h tsputil.cpp tsputil.h tstnorm.cpp
    tstnorm.h tstnrapi.cpp tufmtts.cpp tufmtts.h tzbdtest.cpp tzbdtest.h
    tzfmttst.cpp tzfmttst.h tzoffloc.cpp tzoffloc.h tzregts.cpp tzregts.h
    tzrulets.cpp tzrulets.h tztest.cpp tztest.h ucaconf.cpp ucaconf.h ucdtest.cpp
    ucdtest.h ucharstrietest.cpp uobjtest.cpp uobjtest.h usettest.cpp usettest.h
    ustrtest.cpp ustrtest.h uts46test.cpp utxttest.cpp utxttest.h uvectest.cpp
    uvectest.h v32test.cpp v32test.h wbnf.cpp wbnf.h windttst.cpp windttst.h
    winnmtst.cpp winnmtst.h winutil.cpp winutil.h)
LINK_ICU_LIBRARIES(intltest ${CTESTFW_STUB})
DEPEND_ON_ICU_DATA(intltest)

ADD_CUSTOM_TARGET(check-intltest
                  COMMAND intltest ${INTLTEST_OPTS})
ADD_DEPENDENCIES(check check-intltest)
ADD_CUSTOM_TARGET(check-exhaustive-intltest
                  COMMAND intltest -e)
ADD_DEPENDENCIES(check-exhaustive check-exhaustive-intltest)
ADD_CUSTOM_TARGET(xcheck-intltest
                  COMMAND intltest ${INTLTEST_OPTS} -x "${CMAKE_BINARY_DIR}/test-intltest.xml")
ADD_DEPENDENCIES(xcheck xcheck-intltest)

