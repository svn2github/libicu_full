/*
 * @(#)MarkToBasePositioningSubtables.cpp	1.5 00/03/15
 *
 * (C) Copyright IBM Corp. 1998, 1999, 2000 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "AnchorTables.h"
#include "MarkArrays.h"
#include "GlyphPositioningTables.h"
#include "AttachmentPositioningSubtables.h"
#include "MarkToBasePositioningSubtables.h"
#include "GlyphIterator.h"
#include "LESwaps.h"

LEGlyphID MarkToBasePositioningSubtable::findBaseGlyph(GlyphIterator *glyphIterator)
{
    if (glyphIterator->prev()) {
        return glyphIterator->getCurrGlyphID();
    }

    return 0xFFFF;
}

le_int32 MarkToBasePositioningSubtable::process(GlyphIterator *glyphIterator, LEFontInstance *fontInstance)
{
    LEGlyphID markGlyph = glyphIterator->getCurrGlyphID();
    le_int32 markCoverage = getGlyphCoverage((LEGlyphID) markGlyph);

    if (markCoverage < 0) {
        // markGlyph isn't a covered mark glyph
        return 0;
    }

    LEPoint markAnchor;
    MarkArray *markArray = (MarkArray *) ((char *) this + SWAPW(markArrayOffset));
    le_int32 markClass = markArray->getMarkClass(markGlyph, markCoverage, fontInstance, markAnchor);
    le_uint16 mcCount = SWAPW(classCount);

    if (markClass < 0 || markClass >= mcCount) {
        // markGlyph isn't in the mark array or its
        // mark class is too big. The table is mal-formed!
        return 0;
    }

    // FIXME: We probably don't want to find a base glyph before a previous ligature... 
    GlyphIterator baseIterator(*glyphIterator, lfIgnoreMarks /*| lfIgnoreLigatures*/);
    LEGlyphID baseGlyph = findBaseGlyph(&baseIterator);
    le_int32 baseCoverage = getBaseCoverage((LEGlyphID) baseGlyph);
    BaseArray *baseArray = (BaseArray *) ((char *) this + SWAPW(baseArrayOffset));
    le_uint16 baseCount = SWAPW(baseArray->baseRecordCount);

    if (baseCoverage < 0 || baseCoverage >= baseCount) {
        // The base glyph isn't covered, or the coverage
        // index is too big. The latter means that the
        // table is mal-formed...
        return 0;
    }

    BaseRecord *baseRecord = &baseArray->baseRecordArray[baseCoverage * mcCount];
    Offset anchorTableOffset = SWAPW(baseRecord->baseAnchorTableOffsetArray[markClass]);
    AnchorTable *anchorTable = (AnchorTable *) ((char *) baseArray + anchorTableOffset);
    LEPoint baseAnchor, markAdvance, pixels;

    anchorTable->getAnchor(baseGlyph, fontInstance, baseAnchor);

    fontInstance->getGlyphAdvance(markGlyph, pixels);
    fontInstance->pixelsToUnits(pixels, markAdvance);

    float anchorDiffX = baseAnchor.fX - markAnchor.fX;
    float anchorDiffY = baseAnchor.fY - markAnchor.fY;

    if (glyphIterator->isRightToLeft()) {
        float adjustX = markAdvance.fX + anchorDiffX;

        glyphIterator->adjustCurrGlyphPositionAdjustment(anchorDiffX, -anchorDiffY, -adjustX, anchorDiffY);
    } else {
        LEPoint baseAdvance;

        fontInstance->getGlyphAdvance(baseGlyph, pixels);
        fontInstance->pixelsToUnits(pixels, baseAdvance);

        float adjustX = baseAdvance.fX - anchorDiffX;
        float advAdjustX = adjustX - markAdvance.fX;

        glyphIterator->adjustCurrGlyphPositionAdjustment(-adjustX, -anchorDiffY, advAdjustX, anchorDiffY);
    }

    return 1;
}
