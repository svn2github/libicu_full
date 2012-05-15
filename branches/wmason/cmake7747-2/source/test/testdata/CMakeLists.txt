INCLUDE(IcuDataInput)

ICU_RESET_DATA_BUILD_STATE()
SET(DATA_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/out/build")

# UCM files
ICU_DATA_INPUT(TEST_UCM_SOURCES
               FILES *.ucm)

# RES files
ICU_DATA_INPUT(TEST_RES_SOURCES
               FILES calendar.txt casing.txt conversion.txt DataDrivenCollationTest.txt format.txt
                     icuio.txt idna_rules.txt mc.txt root.txt sh.txt sh_YU.txt structLocale.txt
                     te.txt te_IN.txt te_IN_REVISED.txt testaliases.txt testempty.txt testtypes.txt)

# Old RES files
ICU_DATA_INPUT(TEST_OLD_RES_SOURCES
               FILES *.res)

ICU_DATA_TRANSFORM(OUTPUT_NAME test.icu
                   COMMAND gentest -d "${DATA_BINARY_DIR}")

ICU_DATA_TRANSFORM(INPUT "${DATA_BINARY_DIR}/testtable32.txt"
                   OUTPUT_NAME testtable32.res
                   DEPENDS icurealdata
                   COMMAND genrb -k -i "${CMAKE_BINARY_DIR}/data" -d "${DATA_BINARY_DIR}" "${DATA_BINARY_DIR}/testtable32.txt")

ICU_DATA_TRANSFORM(INPUT nfs4_cs_prep_ci.txt
                   OUTPUT_NAME nfscsi.spp
                   DEPENDS icurealdata
                   COMMAND gensprep -i "${CMAKE_BINARY_DIR}/data" -s "${CMAKE_CURRENT_SOURCE_DIR}" -d "${DATA_BINARY_DIR}" -b nfscsi -u 3.2.0 @in@)
ICU_DATA_TRANSFORM(INPUT nfs4_cs_prep_cs.txt
                   OUTPUT_NAME nfscss.spp
                   DEPENDS icurealdata
                   COMMAND gensprep -i "${CMAKE_BINARY_DIR}/data" -s "${CMAKE_CURRENT_SOURCE_DIR}" -d "${DATA_BINARY_DIR}" -b nfscss -u 3.2.0 @in@)
ICU_DATA_TRANSFORM(INPUT nfs4_cis_prep.txt
                   OUTPUT_NAME nfscis.spp
                   DEPENDS icurealdata
                   COMMAND gensprep -i "${CMAKE_BINARY_DIR}/data" -s "${CMAKE_CURRENT_SOURCE_DIR}" -d "${DATA_BINARY_DIR}" -b nfscis -u 3.2.0 -n "${CMAKE_SOURCE_DIR}/data/unidata" -k @in@)
ICU_DATA_TRANSFORM(INPUT nfs4_mixed_prep_s.txt
                   OUTPUT_NAME nfsmxs.spp
                   DEPENDS icurealdata
                   COMMAND gensprep -i "${CMAKE_BINARY_DIR}/data" -s "${CMAKE_CURRENT_SOURCE_DIR}" -d "${DATA_BINARY_DIR}" -b nfsmxs -u 3.2.0 -n "${CMAKE_SOURCE_DIR}/data/unidata" -k @in@)
ICU_DATA_TRANSFORM(INPUT nfs4_mixed_prep_p.txt
                   OUTPUT_NAME nfsmxp.spp
                   DEPENDS icurealdata
                   COMMAND gensprep -i "${CMAKE_BINARY_DIR}/data" -s "${CMAKE_CURRENT_SOURCE_DIR}" -d "${DATA_BINARY_DIR}" -b nfsmxp -u 3.2.0 -n "${CMAKE_SOURCE_DIR}/data/unidata" -k @in@)

ICU_DATA_TRANSFORM(INPUT ${TEST_UCM_SOURCES}
                   OUTPUT_EXT .cnv
                   COMMAND makeconv --small -c -d "${DATA_BINARY_DIR}" @in@)

ICU_DATA_TRANSFORM(INPUT "${CMAKE_CURRENT_SOURCE_DIR}/testnorm.txt"
                   OUTPUT_EXT .nrm
                   COMMAND gennorm2 @in@ -o @out@)

ADD_CUSTOM_COMMAND(OUTPUT "${DATA_BINARY_DIR}/testtable32.txt"
                   COMMAND gentest -r -d "${DATA_BINARY_DIR}")
ICU_DATA_TRANSFORM(INPUT ${TEST_RES_SOURCES} "${DATA_BINARY_DIR}/testtable32.txt"
                   OUTPUT_EXT .res
                   DEPENDS icurealdata
                   COMMAND genrb -q -k -i "${CMAKE_BINARY_DIR}/data" -d "${DATA_BINARY_DIR}" @in@)
ICU_DATA_TRANSFORM(INPUT "${CMAKE_CURRENT_SOURCE_DIR}/iscii.bin"
                   OUTPUT_EXT .res
                   COMMAND genrb -k -eISCII,version=0 -d "${DATA_BINARY_DIR}" @in@)

ADD_CUSTOM_COMMAND(OUTPUT "${DATA_BINARY_DIR}/../testdata/nam.typ"
                   COMMAND "${CMAKE_COMMAND}" -E copy "${DATA_BINARY_DIR}/te_IN.res" "${DATA_BINARY_DIR}/../testdata/nam.typ"
                   DEPENDS "${DATA_BINARY_DIR}/te_IN.res")
ICU_DATA_TRANSFORM(INPUT ${TEST_OLD_RES_SOURCES}
                   COMMAND "${CMAKE_COMMAND}" -E copy @in@ @out@)

SET(DATA_INPUT_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/testdata.txt")
FILE(WRITE "${DATA_INPUT_LIST_FILE}" "# List of all testdata elements\n")
ICU_ALL_DATA_OUTPUT(DATA_ELEMENTS)
FOREACH(ELEM ${DATA_ELEMENTS})
    FILE(RELATIVE_PATH RELATIVE_ELEM "${DATA_BINARY_DIR}" "${ELEM}")
    FILE(APPEND "${DATA_INPUT_LIST_FILE}" "${RELATIVE_ELEM}\n")
ENDFOREACH()
SET(TEST_DATA_ARCHIVE "${DATA_BINARY_DIR}/../testdata.dat")
ADD_CUSTOM_COMMAND(OUTPUT "${TEST_DATA_ARCHIVE}"
                   COMMAND icupkg new "${TEST_DATA_ARCHIVE}"
                   COMMAND icupkg -a "${DATA_INPUT_LIST_FILE}" -s "${DATA_BINARY_DIR}" "${TEST_DATA_ARCHIVE}"
                   DEPENDS ${DATA_ELEMENTS} "${DATA_BINARY_DIR}/../testdata/nam.typ")
# We've got to make a custom target because just making tests depend on
# the test data archive itself won't work, due to the fact that
# dependencies on custom commands are not visible outside the directory
# in which they are defined.
ADD_CUSTOM_TARGET(testdata
                  DEPENDS "${TEST_DATA_ARCHIVE}")

