/*
*******************************************************************************
* Copyright (C) 2009-2010, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

/**
 * \file 
 * \brief C API: IndexCharacters class
 */

#include "unicode/utypes.h"

#include "unicode/coll.h"
#include "unicode/indexchars.h"
#include "unicode/strenum.h"
#include "unicode/uniset.h"
#include "unicode/uobject.h"
#include "unicode/uscript.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"

#include "uhash.h"
#include "uvector.h"

U_NAMESPACE_BEGIN


IndexCharacters::IndexCharacters(const Locale &locale, UErrorCode &status) {
}


IndexCharacters::IndexCharacters(const Locale &locale, const UnicodeSet &additions, UErrorCode &status) {
}

IndexCharacters::IndexCharacters(const IndexCharacters &other, UErrorCode &status) {
}


IndexCharacters::~IndexCharacters() {
}


UBool IndexCharacters::operator==(const IndexCharacters& other) const {
    return FALSE;
}


UBool IndexCharacters::operator!=(const IndexCharacters& other) const {
    return FALSE;
}

StringEnumeration *IndexCharacters::getIndexCharacters() const {
    return NULL;
}

Locale IndexCharacters::getLocale() const {
    return locale_;
}

const Collator &IndexCharacters::getCollator() const {
    return *comparator_;
}


UnicodeString IndexCharacters::getInflowLabel(UErrorCode &status) const {
    return inflowLabel_;
}

UnicodeString IndexCharacters::getOverflowLabel(UErrorCode &status) const {
    return overflowLabel_;
}


UnicodeString IndexCharacters::getUnderflowLabel(UErrorCode &status) const {
    return underflowLabel_;
}

UnicodeString IndexCharacters::getOverflowComparisonString(UnicodeString lowerLimit) {
    return UnicodeString();
}



void IndexCharacters::init(UErrorCode &status) {
    // TODO:  pull the constant stuff into a singleton.

    OVERFLOW_MARKER = (UChar)0xFFFF;
    INFLOW_MARKER = (UChar)0xFFFD;
    CGJ = (UChar)0x034F;

    // "[[:alphabetic:]-[:mark:]]");
    ALPHABETIC = new UnicodeSet();
    ALPHABETIC->applyIntPropertyValue(UCHAR_ALPHABETIC, 1, status);
    UnicodeSet tempSet;
    tempSet.applyIntPropertyValue(UCHAR_GENERAL_CATEGORY_MASK, U_GC_M_MASK, status);
    ALPHABETIC->removeAll(tempSet);
    
    HANGUL = new UnicodeSet();
    HANGUL->add(0xAC00).add(0xB098).add(0xB2E4).add(0xB77C).add(0xB9C8).add(0xBC14).add(0xC0AC).
            add(0xC544).add(0xC790).add(0xCC28).add(0xCE74).add(0xD0C0).add(0xD30C).add(0xD558);


    // ETHIOPIC = new UnicodeSet("[[:Block=Ethiopic:]&[:Script=Ethiopic:]]");
    ETHIOPIC = new UnicodeSet();
    ETHIOPIC->applyIntPropertyValue(UCHAR_SCRIPT, USCRIPT_ETHIOPIC, status);
    tempSet.applyIntPropertyValue(UCHAR_BLOCK, UBLOCK_ETHIOPIC, status);
    ETHIOPIC->retainAll(tempSet);
   
    CORE_LATIN = new UnicodeSet((UChar32)0x61, (UChar32)0x7a);  // ('a', 'z');

    IGNORE_SCRIPTS = new UnicodeSet();
    tempSet.applyIntPropertyValue(UCHAR_SCRIPT, USCRIPT_COMMON, status);
    IGNORE_SCRIPTS->addAll(tempSet);
    tempSet.applyIntPropertyValue(UCHAR_SCRIPT, USCRIPT_INHERITED, status);
    IGNORE_SCRIPTS->addAll(tempSet);
    tempSet.applyIntPropertyValue(UCHAR_SCRIPT, USCRIPT_UNKNOWN, status);
    IGNORE_SCRIPTS->addAll(tempSet);
    tempSet.applyIntPropertyValue(UCHAR_SCRIPT, USCRIPT_BRAILLE, status);
    IGNORE_SCRIPTS->addAll(tempSet);
    IGNORE_SCRIPTS->freeze();

    UnicodeString nfcqcStr = UNICODE_STRING_SIMPLE("[:^nfcqc=no:]");
    TO_TRY = new UnicodeSet(nfcqcStr, status);

    FIRST_CHARS_IN_SCRIPTS = new UVector(status);
    Collator *rootCollator = Collator::createInstance(Locale::getRoot(), status);
    firstStringsInScript(FIRST_CHARS_IN_SCRIPTS, rootCollator, status);


    // private final LinkedHashMap<String, Set<String>> alreadyIn = new LinkedHashMap<String, Set<String>>();
    
    indexCharacters_       = new UVector(status);
    noDistinctSorting_     = new UVector(status);
    notAlphabetic_         = new UVector(status);
    firstScriptCharacters_ = new UVector(status);
}


void IndexCharacters::firstStringsInScript(UVector *dest, Collator *ruleBasedCollator, UErrorCode &status) {

    if (U_FAILURE(status)) {
        return;
    }

    UnicodeString results[USCRIPT_CODE_LIMIT];

    UnicodeSetIterator siter(TO_TRY);
    while (siter.next()) {
        const UnicodeString &current = siter.getString();
        EComparisonResult r = ruleBasedCollator.compare(current, UNICODE_STRING_SIMPLE("a"));
        if (r < 0) {  // TODO fix; we only want "real" script characters, not
                      // symbols.
            continue;
        }

        int script = uscript_getScript(current.char32At(0));
        if (results[script].length() == 0) {
            results[script] = current;
        }
        else if (ruleBasedCollator.compare(current, results[script]) < 0) {
            results[script] = current;
        }
    }

    UnicodeSet extras;
    UnicodeSet expansions;
    UCollator *uRuleBasedCollator = ruleBasedCollator->getUCollator();
    uRuleBasedCollator.getContractionsAndExpansions(extras.toUSet(), expansions.toUSet(), true, status);
    extras.addAll(expansions).removeAll(TO_TRY);
    if (extras.size() != 0) {
        #if 0
        Normalizer2 normalizer = Normalizer2.getInstance(null, "nfkc", Mode.COMPOSE);
        for (String current : extras) {
            if (!TO_TRY.containsAll(current))
                continue;
            if (!normalizer.isNormalized(current) || ruleBasedCollator.compare(current, "a") < 0) {
                continue;
            }
            int script = UScript.getScript(current.codePointAt(0));
            if (results[script] == null) {
                results[script] = current;
            } else if (ruleBasedCollator.compare(current, results[script]) < 0) {
                results[script] = current;
            }
        }
        #endif
    }

        #if 0
        TreeSet<String> sorted = new TreeSet<String>(ruleBasedCollator);
        for (int i = 0; i < results.length; ++i) {
            if (results[i] != null) {
                sorted.add(results[i]);
            }
        }
        return Collections.unmodifiableList(new ArrayList<String>(sorted));
        #endif

    }
}



U_NAMESPACE_END

