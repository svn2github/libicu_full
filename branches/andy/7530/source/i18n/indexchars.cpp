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
#include "unicode/ulocdata.h"
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

static int32_t U_CALLCONV
sortCollateComparator(const void *context, const void *left, const void *right);

//
//  UHash support function, delete a UnicodeSet
//     TODO:  move this function into uhash.
//
static void U_CALLCONV
uhash_deleteUnicodeSet(void *obj) {
    delete static_cast<UnicodeSet *>(obj);
}

static const Normalizer2 *nfkdNormalizer;

//
//  Append the contents of a UnicodeSet to a UVector of UnicodeStrings.
//  Append everything - individual characters are handled as strings of length 1.
//  The destination vector owns the appended strings.

static void appendUnicodeSetToUVector(UVector &dest, const UnicodeSet &source, UErrorCode &status) {
    UnicodeSetIterator setIter(source);
    while (setIter.next()) {
        const UnicodeString &str = setIter.getString();
        dest.addElement(str.clone(), status);
    }   
}


IndexCharacters::IndexCharacters(const Locale &locale, UErrorCode &status) {
}


IndexCharacters::IndexCharacters(const Locale &locale, const UnicodeSet &additions, UErrorCode &status) {
}

IndexCharacters::IndexCharacters(const IndexCharacters &other, UErrorCode &status) {
}


static int32_t maxCount = 99;    // Max number of index chars.

IndexCharacters::IndexCharacters(const Locale &locale, RuleBasedCollator *collator, 
                   const UnicodeSet *exemplarChars, const UnicodeSet *additions, UErrorCode &status) {
    init(status);
    if (U_FAILURE(status)) {
        return;
    }
    comparator_ = collator;
    comparator_->setStrength(Collator::PRIMARY);

    UBool explicitIndexChars = TRUE;
    UnicodeSet *exemplars = NULL;
    if (exemplarChars == NULL) {
        exemplars = getIndexExemplars(locale, explicitIndexChars, status);
        // Note: explicitIndexChars is set if the locale data includes IndexChars.  Out parameter.
    } else {
        exemplars = static_cast<UnicodeSet *>(exemplarChars->cloneAsThawed());
    }

    if (additions != NULL) {
        exemplars->addAll(*additions);
    }

    // first sort them, with a "best" ordering among items that are the same according
    // to the collator
    UVector preferenceSorting(status);   // Vector of UnicodeStrings; owned by the vector.
    preferenceSorting.setDeleter(uhash_deleteUnicodeString);

    appendUnicodeSetToUVector(preferenceSorting, *exemplars, status);
    preferenceSorting.sortWithUComparator(PreferenceComparator, &status, status);

    UnicodeSet indexCharacterSet;

    // We now make a set of elements, uppercased
    // Some of the input may, however, be redundant.
    // That is, we might have c, ch, d, where "ch" sorts just like "c", "h"
    // So we make a pass through, filtering out those cases.

    for (int32_t psIndex=0; psIndex<preferenceSorting.size(); psIndex++) {
        UnicodeString item = *static_cast<const UnicodeString *>(preferenceSorting.elementAt(psIndex));
        if (!explicitIndexChars) {
            item.toUpper(locale);
        }
        if (indexCharacterSet.contains(item)) {
            UnicodeSetIterator itemAlreadyInIter(indexCharacterSet);
            while (itemAlreadyInIter.next()) {
                const UnicodeString &itemAlreadyIn = itemAlreadyInIter.getString();
                if (comparator_->compare(item, itemAlreadyIn) == 0) {
                    UnicodeSet *targets = static_cast<UnicodeSet *>(uhash_get(alreadyIn_, &itemAlreadyIn));
                    if (targets == NULL) {
                        // alreadyIn.put(itemAlreadyIn, targets = new LinkedHashSet<String>());
                        targets = new UnicodeSet();
                        uhash_put(alreadyIn_, itemAlreadyIn.clone(), targets, &status); 
                    }
                    targets->add(item);
                    break;
                }
            }
        } else if (item.moveIndex32(0, 1) < item.length() &&  // item contains more than one code point.
                   comparator_->compare(item, separated(item)) == 0) {
            noDistinctSorting_->add(item);
        } else if (!ALPHABETIC->containsSome(item)) {
            notAlphabetic_->add(item);
        } else {
            indexCharacterSet.add(item);
        }
    }

    // Move the set of index characters from the set into a vector, and sort
    // according to the collator.
    
    appendUnicodeSetToUVector(*indexCharacters_, indexCharacterSet, status);
    indexCharacters_->sortWithUComparator(sortCollateComparator, comparator_, status);
    
    // if the result is still too large, cut down to maxCount elements, by removing every nth element
    //    Implemented by copying the elements to be retained to a new UVector.

    const int32_t size = indexCharacterSet.size() - 1;
    if (size > maxCount) {
        UVector *newIndexChars = new UVector(status);
        newIndexChars->setDeleter(uhash_deleteUnicodeString);
        int32_t count = 0;
        int32_t old = -1;
        for (int32_t srcIndex=0; srcIndex<indexCharacters_->size(); srcIndex++) {
            const UnicodeString *str = static_cast<const UnicodeString *>(indexCharacters_->elementAt(srcIndex));
            ++count;
            const int32_t bump = count * maxCount / size;
            if (bump == old) {
                // it.remove();
            } else {
                newIndexChars->addElement(str->clone(), status);
                old = bump;
            }
        }
        delete indexCharacters_;
        indexCharacters_ = newIndexChars;
        firstScriptCharacters_ = FIRST_CHARS_IN_SCRIPTS;  // TODO, use collation method when fast enough.
                                                          //   Caution, will need to delete object in
                                                          //   destructor when making this change.
        // firstStringsInScript(comparator);
    }
 
}

UnicodeSet *IndexCharacters::getIndexExemplars(
        const Locale &locale, UBool &explicitIndexChars, UErrorCode &status) {

    LocalULocaleDataPointer uld(ulocdata_open(locale.getName(), &status));
    UnicodeSet *exemplars = UnicodeSet::fromUSet(
            ulocdata_getExemplarSet(uld.getAlias(), NULL, 0, ULOCDATA_ES_INDEX, &status));
    if (exemplars != NULL) {
        explicitIndexChars = TRUE;
        return exemplars;
    }
    explicitIndexChars = false;

    exemplars = UnicodeSet::fromUSet(
            ulocdata_getExemplarSet(uld.getAlias(), NULL, 0, ULOCDATA_ES_STANDARD, &status));

    // get the exemplars, and handle special cases

    // question: should we add auxiliary exemplars?
    if (exemplars->containsSome(*CORE_LATIN)) {
        exemplars->addAll(*CORE_LATIN);
    }
    if (exemplars->containsSome(*HANGUL)) {
        // cut down to small list
        UnicodeSet BLOCK_HANGUL_SYLLABLES(UNICODE_STRING_SIMPLE("[:block=hangul_syllables:]"), status);
        exemplars->removeAll(BLOCK_HANGUL_SYLLABLES);
        exemplars->addAll(*HANGUL);
    }
    if (exemplars->containsSome(*ETHIOPIC)) {
        // cut down to small list
        // make use of the fact that Ethiopic is allocated in 8's, where
        // the base is 0 mod 8.
        UnicodeSetIterator  it(*ETHIOPIC);
        while (it.next() && !it.isString()) {
            if ((it.getCodepoint() & 0x7) != 0) {
                exemplars->remove(it.getCodepoint());
            }
        }
    }
    return exemplars;
}


/*
 * Return the string with interspersed CGJs. Input must have more than 2 codepoints.
 */
static const UChar32 CGJ = (UChar)0x034F;
UnicodeString separated(const UnicodeString &item) {
    UnicodeString result;
    if (item.length() == 0) {
        return result;
    }
    int32_t i = 0;
    for (;;) {
        UChar32  cp = item.char32At(i);
        result.append(cp);
        i = item.moveIndex32(i, 1);
        if (i >= item.length()) {
            break;
        }
        result.append(CGJ);
    }
    return result;
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

UnicodeString IndexCharacters::getOverflowComparisonString(const UnicodeString &lowerLimit) {
    for (int32_t i=0; i<firstScriptCharacters_->size(); i++) {
        const UnicodeString *s = 
                static_cast<const UnicodeString *>(firstScriptCharacters_->elementAt(i));
        if (comparator_->compare(*s, lowerLimit) > 0) {
            return *s;   // returning string by value
        }
    }
    return UnicodeString();
}


UnicodeSet *getScriptSet(const UnicodeString &codePoint, UErrorCode &status) {
    UChar32 cp = codePoint.char32At(0);
    UScriptCode scriptCode = uscript_getScript(cp, &status);
    UnicodeSet *set = new UnicodeSet();
    set->applyIntPropertyValue(UCHAR_SCRIPT, scriptCode, status);
    return set;
}

void IndexCharacters::init(UErrorCode &status) {
    // TODO:  pull the constant stuff into a singleton.

    OVERFLOW_MARKER = (UChar)0xFFFF;
    INFLOW_MARKER = (UChar)0xFFFD;

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
    alreadyIn_             = uhash_open(uhash_hashUnicodeString,    // Key Hash,
                                        uhash_compareUnicodeString, // key Comparator, 
                                        NULL,                       // value Comparator
                                        &status);
    uhash_setKeyDeleter(alreadyIn_, uhash_deleteUnicodeString);
    uhash_setValueDeleter(alreadyIn_, uhash_deleteUnicodeSet);

    indexCharacters_ = new UVector(status);
    indexCharacters_->setDeleter(uhash_deleteUnicodeString);
    indexCharacters_->setComparer(uhash_compareUnicodeString);




    noDistinctSorting_     = new UnicodeSet();
    notAlphabetic_         = new UnicodeSet();
    firstScriptCharacters_ = new UVector(status);

    // TODO: move to all constant stuff to a singleton, init it once.
    nfkdNormalizer         = Normalizer2::getInstance(NULL, "nfkc", UNORM2_DECOMPOSE, status);
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

    UnicodeString n1 = nfkdNormalizer->normalize(*s1, status);
    UnicodeString n2 = nfkdNormalizer->normalize(*s2, status);
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


void IndexCharacters::addItem(const icu_45::UnicodeString &name, void *context, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (buckets_ != NULL) {
        // This index is already built (being iterated.)
        // Refuse to modify it.  TODO:  review this.  
        status = U_INVALID_STATE_ERROR;
        return;
    }
    Record *r = new Record;
    // Note on ownership of the name string:  It stays with the Record.
    r->name_ = static_cast<UnicodeString *>(name.clone());
    r->context_ = context;
    inputRecords_->addElement(r, status);
}


UBool IndexCharacters::nextLabel() {
    if (buckets_ == NULL) {
        buildIndex();
        labelsIterIndex_ = 0;
    } else {
        ++labelsIterIndex_;
    } 
    if (labelsIterIndex_ >= buckets_->size()) {
        labelsIterIndex_ = buckets_->size();
        return FALSE;
    }
    return TRUE;
}

const UnicodeString &IndexCharacters::getLabel() {
    if (buckets_ == NULL) {
        buildIndex();
    }
    if (labelsIterIndex >= buckets_->size()) {
        return EMPTY_STRING;
    } else {
        Bucket *b = static_cast<Bucket>(buckets_->elementAt(labelsiterIndex));
        return b->label_;
    }
}





U_NAMESPACE_END

