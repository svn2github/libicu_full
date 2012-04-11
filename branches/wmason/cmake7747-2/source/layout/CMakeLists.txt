ADD_DEFINITIONS(-DU_LAYOUT_IMPLEMENTATION)
ICU_LIBRARY(${LAYOUT_STUB}
    AlternateSubstSubtables.cpp AlternateSubstSubtables.h AnchorTables.cpp AnchorTables.h
    ArabicLayoutEngine.cpp ArabicLayoutEngine.h ArabicShaping.cpp ArabicShaping.h
    AttachmentPosnSubtables.h CanonData.cpp CanonShaping.cpp CanonShaping.h
    CharSubstitutionFilter.h ClassDefinitionTables.cpp ClassDefinitionTables.h
    ContextualGlyphInsertion.h ContextualGlyphSubstProc.cpp ContextualGlyphSubstProc.h
    ContextualGlyphSubstitution.h ContextualSubstSubtables.cpp ContextualSubstSubtables.h
    CoverageTables.cpp CoverageTables.h CursiveAttachmentSubtables.cpp
    CursiveAttachmentSubtables.h DefaultCharMapper.h DeviceTables.cpp DeviceTables.h
    ExtensionSubtables.cpp ExtensionSubtables.h Features.cpp GDEFMarkFilter.cpp GDEFMarkFilter.h
    GXLayoutEngine.cpp GXLayoutEngine.h GlyphDefinitionTables.cpp GlyphDefinitionTables.h
    GlyphIterator.cpp GlyphIterator.h GlyphLookupTables.cpp GlyphLookupTables.h
    GlyphPositionAdjustments.cpp GlyphPositionAdjustments.h GlyphPositioningTables.cpp
    GlyphPositioningTables.h GlyphPosnLookupProc.cpp GlyphPosnLookupProc.h
    GlyphSubstLookupProc.cpp GlyphSubstLookupProc.h GlyphSubstitutionTables.cpp
    GlyphSubstitutionTables.h HanLayoutEngine.cpp HanLayoutEngine.h HangulLayoutEngine.cpp
    HangulLayoutEngine.h ICUFeatures.h IndicClassTables.cpp IndicLayoutEngine.cpp
    IndicLayoutEngine.h IndicRearrangement.h IndicRearrangementProcessor.cpp
    IndicRearrangementProcessor.h IndicReordering.cpp IndicReordering.h KernTable.cpp
    KernTable.h KhmerLayoutEngine.cpp KhmerLayoutEngine.h KhmerReordering.cpp
    KhmerReordering.h LEFontInstance.cpp LEFontInstance.h LEGlyphFilter.h LEGlyphStorage.cpp
    LEGlyphStorage.h LEInsertionList.cpp LEInsertionList.h LELanguages.h LEScripts.h
    LESwaps.h LETypes.h LayoutEngine.cpp LayoutEngine.h LayoutTables.h LigatureSubstProc.cpp
    LigatureSubstProc.h LigatureSubstSubtables.cpp LigatureSubstSubtables.h LigatureSubstitution.h
    LookupProcessor.cpp LookupProcessor.h LookupTables.cpp LookupTables.h Lookups.cpp
    Lookups.h MPreFixups.cpp MPreFixups.h MarkArrays.cpp MarkArrays.h MarkToBasePosnSubtables.cpp
    MarkToBasePosnSubtables.h MarkToLigaturePosnSubtables.cpp MarkToLigaturePosnSubtables.h
    MarkToMarkPosnSubtables.cpp MarkToMarkPosnSubtables.h MirroredCharData.cpp MorphStateTables.h
    MorphTables.cpp MorphTables.h MultipleSubstSubtables.cpp MultipleSubstSubtables.h
    NonContextualGlyphSubst.h NonContextualGlyphSubstProc.cpp NonContextualGlyphSubstProc.h
    OpenTypeLayoutEngine.cpp OpenTypeLayoutEngine.h OpenTypeTables.h OpenTypeUtilities.cpp
    OpenTypeUtilities.h PairPositioningSubtables.cpp PairPositioningSubtables.h
    ScriptAndLanguage.cpp ScriptAndLanguage.h ScriptAndLanguageTags.cpp ScriptAndLanguageTags.h
    SegmentArrayProcessor.cpp SegmentArrayProcessor.h SegmentSingleProcessor.cpp
    SegmentSingleProcessor.h ShapingTypeData.cpp SimpleArrayProcessor.cpp SimpleArrayProcessor.h
    SinglePositioningSubtables.cpp SinglePositioningSubtables.h SingleSubstitutionSubtables.cpp
    SingleSubstitutionSubtables.h SingleTableProcessor.cpp SingleTableProcessor.h
    StateTableProcessor.cpp StateTableProcessor.h StateTables.h SubstitutionLookups.cpp
    SubstitutionLookups.h SubtableProcessor.cpp SubtableProcessor.h ThaiLayoutEngine.cpp
    ThaiLayoutEngine.h ThaiShaping.cpp ThaiShaping.h ThaiStateTables.cpp TibetanLayoutEngine.cpp
    TibetanLayoutEngine.h TibetanReordering.cpp TibetanReordering.h TrimmedArrayProcessor.cpp
    TrimmedArrayProcessor.h ValueRecords.cpp ValueRecords.h loengine.cpp loengine.h
    ICU_LINK_LIBRARIES
    ${COMMON_STUB})

INSTALL_ICU_LIBRARY(${LAYOUT_STUB})
INSTALL(FILES LayoutEngine.h LEFontInstance.h LEGlyphFilter.h LEGlyphStorage.h
    LEInsertionList.h LELanguages.h LEScripts.h LESwaps.h LETypes.h loengine.h
        DESTINATION ${INCLUDE_DIR}/layout
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

