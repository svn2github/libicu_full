INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/toolutil")

MACRO(ICU_SIMPLE_C_TOOL TOOL_NAME)
    ADD_EXECUTABLE(${TOOL_NAME} ${TOOL_NAME}.c)
    LINK_ICU_LIBRARIES(${TOOL_NAME} ${TOOLUTIL_STUB})
    SET_TARGET_PROPERTIES(${TOOL_NAME} PROPERTIES
        LINKER_LANGUAGE CXX)
ENDMACRO(ICU_SIMPLE_C_TOOL)

MACRO(ICU_SIMPLE_CXX_TOOL TOOL_NAME)
    ADD_EXECUTABLE(${TOOL_NAME} ${TOOL_NAME}.cpp)
    LINK_ICU_LIBRARIES(${TOOL_NAME} ${TOOLUTIL_STUB})
ENDMACRO(ICU_SIMPLE_CXX_TOOL)

# Don't mix the first two in with the others. These generate
# libraries that may be used by the others.
ADD_SUBDIRECTORY(toolutil)
ADD_SUBDIRECTORY(ctestfw)

# These are the actual tools
IF(ENABLE_TOOLS)
	# These tools are "always" built.

	# Informational
	ADD_SUBDIRECTORY(icuinfo)

	# mapping (converter) tables
	ADD_SUBDIRECTORY(gencnval)
	ADD_SUBDIRECTORY(makeconv)

	# resource bundles
	ADD_SUBDIRECTORY(genrb)

	# packaging
	ADD_SUBDIRECTORY(icupkg)
	ADD_SUBDIRECTORY(pkgdata)
	ADD_SUBDIRECTORY(genccode)
	#ADD_SUBDIRECTORY(icuswap) <=== obsolete

	# for tests
	ADD_SUBDIRECTORY(gentest)

	# These are only needed to rebuild data from source.
	IF(ENABLE_DATAGEN)
		#ADD_SUBDIRECTORY(dumpce)
		ADD_SUBDIRECTORY(genbrk)
		ADD_SUBDIRECTORY(gencfu)
		ADD_SUBDIRECTORY(gencmn)
		#ADD_SUBDIRECTORY(genctd)
		ADD_SUBDIRECTORY(gennorm2)
		ADD_SUBDIRECTORY(gensprep)
		#ADD_SUBDIRECTORY(tzcode) <=== tzcode tools not yet implemented
	ENDIF()
ENDIF()

