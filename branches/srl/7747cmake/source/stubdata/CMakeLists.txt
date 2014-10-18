ICU_LIBRARY(${DATA_STUB} stubdata.c)

# If the real data is not in a library, then we want to
# install the stub data library.
IF(NOT LOWER_ICU_DATA_PACKAGING STREQUAL library)
    INSTALL_ICU_LIBRARY(${DATA_STUB})
ENDIF()

