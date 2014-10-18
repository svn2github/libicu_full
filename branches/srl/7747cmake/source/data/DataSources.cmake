# DAT files
ICU_DATA_INPUT(DAT_SOURCES
               DIRECTORY in
               FILES nfc.nrm nfkc.nrm nfkc_cf.nrm unames.icu uts46.nrm)
ICU_DATA_INPUT(DAT_COLL_SOURCES
               DIRECTORY in/coll
               FILES invuca.icu ucadata.icu)

# Cnvalias
ICU_DATA_INPUT(CNVALIAS_SOURCES
               DIRECTORY mappings
               FILES convrtrs.txt)

# BRK files
ICU_DATA_INPUT(BRK_SOURCES
               DIRECTORY brkitr
               FILES char.txt line*.txt sent*.txt title.txt word*.txt)
ICU_DATA_INPUT(BRK_CTD_SOURCES
               DIRECTORY brkitr
               FILES khmerdict.txt thaidict.txt)
SET(BRK_RES_CLDR_VERSION 21.0.1)
ICU_DATA_INPUT(BRK_RES_SOURCES
               DIRECTORY brkitr
               FILES el.txt en*.txt fi.txt ja.txt root.txt)

# Confusables
ICU_DATA_INPUT(CFU_SOURCES
               DIRECTORY unidata
               FILES confusables*.txt)

# UCM files
ICU_DATA_INPUT(UCM_SOURCES
               DIRECTORY mappings
               FILES *.ucm)

# RES files
FOREACH(DIR locales curr lang region zone coll rbnf)
    STRING(TOUPPER ${DIR} UPPER_DIR)
    ICU_DATA_INPUT(${UPPER_DIR}_SOURCES
                   DIRECTORY ${DIR}
                   FILES *.txt)
ENDFOREACH()
ICU_DATA_INPUT(TRANSLIT_SOURCES
               DIRECTORY translit
               FILES el.txt en.txt root.txt)
FOREACH(CLDR BRK_RES COLLATION CURR LANG GENRB RBNF REGION ZONE)
    SET(${CLDR}_CLDR_VERSION 21.0.1)
ENDFOREACH()
ICU_DATA_INPUT(POOL_BUNDLES
               FILES curr/pool.res lang/pool.res region/pool.res zone/pool.res locales/pool.res)

# SPP files
ICU_DATA_INPUT(SPREP_SOURCES
               DIRECTORY sprep
               FILES *.txt)

# Misc files
ICU_DATA_INPUT(MISC_SOURCES
               DIRECTORY misc
               FILES *.txt)

