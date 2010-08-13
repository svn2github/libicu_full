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
#include "unicode/normalizer2.h"
#include "unicode/strenum.h"
#include "unicode/tblcoll.h"
#include "unicode/uniset.h"
#include "unicode/uobject.h"
#include "unicode/uscript.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"

#include "uhash.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

// Forward Declarations
static int32_t U_CALLCONV
PreferenceComparator(const void *context, const void *left, const void *right);


IndexCharacters::IndexCharacters(const Locale &locale, UErrorCode &status) {
}


IndexCharacters::IndexCharacters(const Locale &locale, const UnicodeSet &additions, UErrorCode &status) {
}

IndexCharacters::IndexCharacters(const IndexCharacters &other, UErrorCode &status) {
}


IndexCharacters::IndexCharacters(const Locale &locale, RuleBasedCollator *collator, 
                   const UnicodeSet *exemplarChars, const UnicodeSet *additions, UErrorCode &status) {
    init(status);
    if (U_FAILURE(status)) {
        return;
    }
    comparator_ = collator;
    comparator_->setStrength(Collator::PRIMARY);

    UBool explicitIndexChars = TRUE;
    UnicodeSet *exemplars = new UnicodeSet(*exemplarChars);
    if (exemplars == NULL) {
        exemplars = getIndexExemplars(locale, explicitIndexChars);
        // Note: explicitIndexChars is set if the locale data includes IndexChars.  Out parameter.
    }

    if (additions != NULL) {
        exemplars->addAll(*additions);
    }

    // first sort them, with an "best" ordering among items that are the same according
    // to the collator
    UVector preferenceSorting(status);   // Vector of UnicodeStrings; owned by the vector.
    preferenceSorting.setDeleter(uhash_deleteUnicodeString);

    UnicodeSetIterator exemplarIter(*exemplars);
    while (exemplarIter.next()) {
        const UnicodeString &exemplarStr = exemplarIter.getString();
        preferenceSorting.addElement(exemplarStr.clone(), status);
    }   
    preferenceSorting.sortWithUComparator(PreferenceComparator, &status, status);


    UVector indexCharacterSet(status);
    indexCharacterSet.setDeleter(uhash_deleteUnicodeString);

    // TreeSet<String> indexCharacterSet = new TreeSet<String>(comparator);

    // We nw make a sorted array of elements, uppercased
    // Some of the input may, however, be redundant.
    // That is, we might have c, ch, d, where "ch" sorts just like "c", "h"
    // So we make a pass through, filtering out those cases.


}



IndexCharacters::~IndexCharacters() {
}


UBool IndexCharacters::operator==(const IndexCharacters& other) const {
    return FALSE;
}


UBool IndexCharacters::operator!=(const IndexCharacters& other) const {
    return FALSE;
}


UnicodeSet *getIndexExemplars(const Locale locale, UBool &explicitIndexChars) {
    return NULL;
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

    Collator *rootCollator = Collator::createInstance(Locale::getRoot(), status);
    FIRST_CHARS_IN_SCRIPTS = firstStringsInScript(rootCollator, status);


    // private final LinkedHashMap<String, Set<String>> alreadyIn = new LinkedHashMap<String, Set<String>>();
    
    indexCharacters_       = new UVector(status);
    noDistinctSorting_     = new UVector(status);
    notAlphabetic_         = new UVector(status);
    firstScriptCharacters_ = new UVector(status);
}


//
//  Comparison function for UVector for sorting with a collator.
//
static int32_t U_CALLCONV
sortCollateComparator(const void *context, const void *left, const void *right) {
    const UHashTok *leftTok = static_cast<const UHashTok *>(left);
    const UHashTok *rightTok = static_cast<const UHashTok *>(right);
    const UnicodeString *leftString  = static_cast<const UnicodeString *>(leftTok->pointer);
    const UnicodeString *rightString = static_cast<const UnicodeString *>(rightTok->pointer);
    const Collator *col = static_cast<const Collator *>(context);
    
    if (leftString == rightString) {
        // Catches case where both are NULL
        return 0;
    }
    if (leftString == NULL) {
        return 1;
    };
    if (rightString == NULL) {
        return -1;
    }
    Collator::EComparisonResult r = col->compare(*leftString, *rightString);
    return (int32_t) r;
}



UVector *IndexCharacters::firstStringsInScript(Collator *ruleBasedCollator, UErrorCode &status) {

    if (U_FAILURE(status)) {
        return NULL;
    }

    UnicodeString results[USCRIPT_CODE_LIMIT];
    UnicodeString LOWER_A = UNICODE_STRING_SIMPLE("a");

    UnicodeSetIterator siter(*TO_TRY);
    while (siter.next()) {
        const UnicodeString &current = siter.getString();
        Collator::EComparisonResult r = ruleBasedCollator->compare(current, LOWER_A);
        if (r < 0) {  // TODO fix; we only want "real" script characters, not
                      // symbols.
            continue;
        }

        int script = uscript_getScript(current.char32At(0), &status);
        if (results[script].length() == 0) {
            results[script] = current;
        }
        else if (ruleBasedCollator->compare(current, results[script]) < 0) {
            results[script] = current;
        }
    }

    UnicodeSet extras;
    UnicodeSet expansions;
    RuleBasedCollator *rbc = dynamic_cast<RuleBasedCollator *>(ruleBasedCollator);
    const UCollator *uRuleBasedCollator = rbc->getUCollator();
    ucol_getContractionsAndExpansions(uRuleBasedCollator, extras.toUSet(), expansions.toUSet(), true, &status);
    extras.addAll(expansions).removeAll(*TO_TRY);
    if (extras.size() != 0) {
        const Normalizer2 *normalizer = Normalizer2::getInstance(NULL, "nfkc", UNORM2_COMPOSE, status);
        UnicodeSetIterator extrasIter(extras);
        while (extrasIter.next()) {
            const UnicodeString &current = extrasIter.next();
            if (!TO_TRY->containsAll(current))
                continue;
            if (!normalizer->isNormalized(current, status) || 
                ruleBasedCollator->compare(current, LOWER_A) < 0) {
                continue;
            }
            int script = uscript_getScript(current.char32At(0), &status);
            if (results[script].length() == 0) {
                results[script] = current;
            } else if (ruleBasedCollator->compare(current, results[script]) < 0) {
                results[script] = current;
            }
        }
    }

    UVector *dest = new UVector(status);
    dest->setDeleter(uhash_deleteUnicodeString);
    for (uint32_t i = 0; i < sizeof(results) / sizeof(results[0]); ++i) {
        if (results[i].length() > 0) {
            dest->addElement(results[i].clone(), status);
        }
    }
    dest->sortWithUComparator(sortCollateComparator, ruleBasedCollator, status);
    return dest;
}


/**
 * Comparator that returns "better" items first, where shorter NFKD is better, and otherwise NFKD binary order is
 * better, and otherwise binary order is better.
 *
 * For use with array sort or UVector.
 * @param context  A UErrorCode pointer.
 * @param left     A UHashTok pointer, which must refer to a UnicodeString *
 * @param right    A UHashTok pointer, which must refer to a UnicodeString *
 */
static int32_t U_CALLCONV
PreferenceComparator(const void *context, const void *left, const void *right) {
    const UHashTok *leftTok  = static_cast<const UHashTok *>(left);
    const UHashTok *rightTok = static_cast<const UHashTok *>(right);
    const UnicodeString *s1  = static_cast<const UnicodeString *>(leftTok->pointer);
    const UnicodeString *s2  = static_cast<const UnicodeString *>(rightTok->pointer);
    UErrorCode &status       = *(UErrorCode *)(context);   // static and const cast.
    if (s1 == s2) {
        return 0;
    }

    const Normalizer2 *normalizer = Normalizer2::getInstance(NULL, "nfkc", UNORM2_DECOMPOSE, status);
    UnicodeString n1 = normalizer->normalize(*s1, status);
    UnicodeString n2 = normalizer->normalize(*s2, status);
    int32_t result = n1.length() - n2.length();
    if (result != 0) {
        return result;
    }

    result = n1.compareCodePointOrder(n2);
    if (result != 0) {
        return result;
    }
    return s1->compareCodePointOrder(*s2);
}




U_NAMESPACE_END

