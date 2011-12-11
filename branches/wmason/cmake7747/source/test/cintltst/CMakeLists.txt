ADD_DEFINITIONS(-DICU_UNICODE_VERSION="${ICU_UNICODE_VERSION}" -DICU_VERSION="${ICU_VERSION}" -DICUDATA_NAME="${DATA_NAME}")
ADD_EXECUTABLE(cintltst EXCLUDE_FROM_ALL
    bocu1tst.c callcoll.c callcoll.h calldata.h calltest.c capitst.c capitst.h cbiapts.c
    cbiapts.h cbididat.c cbiditst.c cbiditst.h cbkittst.c ccaltst.c ccaltst.h ccapitst.c
    ccapitst.h ccolltst.c ccolltst.h cconvtst.c cctest.c ccurrtst.c ccurrtst.h
    cdateintervalformattest.c cdattst.c cdattst.h cdetst.c cdetst.h cdtdptst.c cdtdptst.h
    cdtrgtst.c cdtrgtst.h cestst.c cestst.h cfintst.c cfintst.h cformtst.c cformtst.h
    cfrtst.c cfrtst.h cg7coll.c cg7coll.h chashtst.c cintltst.c cintltst.h citertst.c
    citertst.h cjaptst.c cjaptst.h cldrtest.c cloctst.c cloctst.h cmsccoll.c cmsgtst.c
    cmsgtst.h cnmdptst.c cnmdptst.h cnormtst.c cnormtst.h cnumtst.c cnumtst.h colutil.c
    cpluralrulestest.c cposxtst.c crestst.c crestst.h creststn.c creststn.h cstrcase.c
    cstrtest.c cturtst.c cturtst.h cucdapi.c cucdapi.h cucdtst.c currtest.c custrtrn.c
    custrtst.c cutiltst.c encoll.c encoll.h eurocreg.c files.out hpmufn.c idnatest.c
    nccbtst.c nccbtst.h ncnvfbts.c ncnvfbts.h ncnvtst.c nfsprep.c nfsprep.h nucnvtst.c
    nucnvtst.h putiltst.c reapits.c sorttest.c spooftest.c spreptst.c sprpdata.c stdnmtst.c
    tracetst.c trie2test.c trietest.c ucnvseltst.c ucnvseltst.h ucsdetst.c udatatst.c
    udatpg_test.c uenumtst.c usettest.c usrchtst.c utexttst.c utf16tst.c utf8tst.c
    utmstest.c utransts.c)
LINK_ICU_LIBRARIES(cintltst ${CTESTFW_STUB})
DEPEND_ON_ICU_DATA(cintltst)

ADD_CUSTOM_TARGET(check-cintltst
                  COMMAND cintltst ${CINTLTST_OPTS})
ADD_DEPENDENCIES(check check-cintltst)
ADD_CUSTOM_TARGET(check-exhaustive-cintltst
                  COMMAND cintltst -e)
ADD_DEPENDENCIES(check-exhaustive check-exhaustive-cintltst)

