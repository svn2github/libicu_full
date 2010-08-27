/*
*******************************************************************************
* Copyright (C) 2009-2010, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

/**
 * \file 
 * \brief C API: AlphabeticIndex class
 */

#include "unicode/utypes.h"

#include "unicode/alphaindex.h"
#include "unicode/coll.h"
#include "unicode/normalizer2.h"
#include "unicode/strenum.h"
#include "unicode/tblcoll.h"
#include "unicode/ulocdata.h"
#include "unicode/uniset.h"
#include "unicode/uobject.h"
#include "unicode/uscript.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"

#include "mutex.h"
#include "ucln_in.h"
#include "uhash.h"
#include "uassert.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(AlphabeticIndex)


// Forward Declarations
static int32_t U_CALLCONV
PreferenceComparator(const void *context, const void *left, const void *right);

static int32_t U_CALLCONV
sortCollateComparator(const void *context, const void *left, const void *right);

static int32_t U_CALLCONV
recordCompareFn(const void *context, const void *left, const void *right); 

//
//  UHash support function, delete a UnicodeSet
//     TODO:  move this function into uhash.
//
static void U_CALLCONV
uhash_deleteUnicodeSet(void *obj) {
    delete static_cast<UnicodeSet *>(obj);
}

//  UVector<Bucket *> support function, delete a Bucket.
static void U_CALLCONV
indexChars_deleteBucket(void *obj) {
    delete static_cast<AlphabeticIndex::Bucket *>(obj);
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


AlphabeticIndex::AlphabeticIndex(const Locale &locale, UErrorCode &status) {
    init(status);
    if (U_FAILURE(status)) {
        return;
    }
    locale_ = locale;
    comparator_ = Collator::createInstance(locale, status);
    if (comparator_ != NULL) {
        comparator_->setStrength(Collator::PRIMARY);
    }
    getIndexExemplars(*rawIndexChars_, locale, status);
    indexBuildRequired_ = TRUE;
    if (comparator_ == NULL && U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}


AlphabeticIndex::~AlphabeticIndex() {
    uhash_close(alreadyIn_);
    delete bucketList_;
    delete indexCharacters_;
    delete inputRecords_;
    delete noDistinctSorting_;
    delete notAlphabetic_;
    delete rawIndexChars_;
    delete comparator_;
    
    // TODO:  firstScriptCharacters_ is temporarily a pointer to the global FIRST_CHARS_IN_SCRIPTS.
    //        Delete it here if/when we get instance data rather than shared global data.
    // delete firstScriptCharacters_;
}


void AlphabeticIndex::addIndexCharacters(const UnicodeSet &additions, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    rawIndexChars_->addAll(additions);
}


void AlphabeticIndex::addLocale(const Locale &locale, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    UnicodeSet additions;
    getIndexExemplars(additions, locale, status);
    rawIndexChars_->addAll(additions);
}


static int32_t maxLabelCount_ = 99;    // Max number of index chars.


int32_t AlphabeticIndex::countLabels(UErrorCode &status) {
    buildIndex(status);
    if (U_FAILURE(status)) {
        return 0;
    }
    return bucketList_->size();
}


void AlphabeticIndex::buildIndex(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (!indexBuildRequired_) {
        return;
    }

    // Discard any already-built data.
    // This is important when the user builds and uses an index, then subsequently modifies it,
    // necessitating a rebuild.

    bucketList_->removeAllElements();
    indexCharacters_->removeAllElements();
    uhash_removeAll(alreadyIn_);
    noDistinctSorting_->clear();
    notAlphabetic_->clear();

    // first sort the incoming index chars, with a "best" ordering among items 
    // that are the same according to the collator

    UVector preferenceSorting(status);   // Vector of UnicodeStrings; owned by the vector.
    preferenceSorting.setDeleter(uhash_deleteUnicodeString);
    appendUnicodeSetToUVector(preferenceSorting, *rawIndexChars_, status);
    preferenceSorting.sortWithUComparator(PreferenceComparator, &status, status);

    // We now make a set of elements.
    // Some of the input may, however, be redundant.
    // That is, we might have c, ch, d, where "ch" sorts just like "c", "h"
    // So we make a pass through, filtering out those cases.
    // TODO: filtering these out would seem to be at odds with the eventual goal
    //       of being able to split buckets that contain too many items.

    UnicodeSet indexCharacterSet;
    for (int32_t psIndex=0; psIndex<preferenceSorting.size(); psIndex++) {
        UnicodeString item = *static_cast<const UnicodeString *>(preferenceSorting.elementAt(psIndex));
        // TODO:  Since preferenceSorting was originally populated from the contents of UnicodeSet,
        //        is it even possible for duplicates to show up in this check?
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
    
    // if the result is still too large, cut down to maxLabelCount_ elements, by removing every nth element
    //    Implemented by copying the elements to be retained to a new UVector.

    const int32_t size = indexCharacterSet.size() - 1;
    if (size > maxLabelCount_) {
        UVector *newIndexChars = new UVector(status);
        newIndexChars->setDeleter(uhash_deleteUnicodeString);
        int32_t count = 0;
        int32_t old = -1;
        for (int32_t srcIndex=0; srcIndex<indexCharacters_->size(); srcIndex++) {
            const UnicodeString *str = static_cast<const UnicodeString *>(indexCharacters_->elementAt(srcIndex));
            ++count;
            const int32_t bump = count * maxLabelCount_ / size;
            if (bump == old) {
                // it.remove();
            } else {
                newIndexChars->addElement(str->clone(), status);
                old = bump;
            }
        }
        delete indexCharacters_;
        indexCharacters_ = newIndexChars;
    }
    // firstStringsInScript(comparator);  // TODO: use collation method when fast enough
    firstScriptCharacters_ = FIRST_CHARS_IN_SCRIPTS;
    buildBucketList(status);    // Corresponds to Java BucketList constructor.

    // Bin the items into the buckets.
    bucketItems(status);

    indexBuildRequired_ = FALSE;
    resetLabelIterator(status);
}

//
//  buildBucketList()    Corresponds to the BucketList constructor in the Java version.

void AlphabeticIndex::buildBucketList(UErrorCode &status) {
    UnicodeString labelStr = getUnderflowLabel();
    Bucket *b = new Bucket(labelStr, *EMPTY_STRING, ALPHABETIC_INDEX_UNDERFLOW, status);
    bucketList_->addElement(b, status);

    // Build up the list, adding underflow, additions, overflow
    // insert infix labels as needed, using \uFFFF.
    const UnicodeString *last = static_cast<UnicodeString *>(indexCharacters_->elementAt(0));
    b = new Bucket(*last, *last, ALPHABETIC_INDEX_NORMAL, status);
    bucketList_->addElement(b, status);

    UnicodeSet lastSet; 
    UnicodeSet set;
    AlphabeticIndex::getScriptSet(lastSet, *last, status);
    lastSet.removeAll(*IGNORE_SCRIPTS);

    for (int i = 1; i < indexCharacters_->size(); ++i) {
        UnicodeString *current = static_cast<UnicodeString *>(indexCharacters_->elementAt(i));
        getScriptSet(set, *current, status);
        set.removeAll(*IGNORE_SCRIPTS);
        if (lastSet.containsNone(set)) {
            // check for adjacent
            UnicodeString overflowComparisonString = getOverflowComparisonString(*last, status);
            if (comparator_->compare(overflowComparisonString, *current) < 0) {
                labelStr = getInflowLabel();
                b = new Bucket(labelStr, overflowComparisonString, ALPHABETIC_INDEX_INFLOW, status);
                bucketList_->addElement(b, status);
                i++;
                lastSet = set;
            }
        }
        b = new Bucket(*current, *current, ALPHABETIC_INDEX_NORMAL, status);
        bucketList_->addElement(b, status);
        last = current;
        lastSet = set;
    }
    UnicodeString limitString = getOverflowComparisonString(*last, status);
    b = new Bucket(getOverflowLabel(), limitString, ALPHABETIC_INDEX_OVERFLOW, status);
    bucketList_->addElement(b, status);
    // final overflow bucket
}


//
//   Place all of the raw input records into the correct bucket.
//
//       Begin by sorting the input records; this lets us bin them in a single pass.
//
//       Note on storage management:  The input records are owned by the 
//       inputRecords_ vector, and will (eventually) be auto-deleted by it.
//       The Bucket objects have pointers to the Record objects, but do not own them.
//
void AlphabeticIndex::bucketItems(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }

    comparator_->setStrength(Collator::TERTIARY);
    inputRecords_->sortWithUComparator(recordCompareFn, comparator_, status);
    U_ASSERT(bucketList_->size() > 0);   // Should always have at least an overflow
                                         //   bucket, even if no user labels.
    int32_t bucketIndex = 0;
    Bucket *destBucket = static_cast<Bucket *>(bucketList_->elementAt(bucketIndex));
    Bucket *nextBucket = NULL;
    if (bucketIndex+1 < bucketList_->size()) {
        nextBucket = static_cast<Bucket *>(bucketList_->elementAt(bucketIndex+1));
    }
    int32_t itemIndex = 0;
    Record *r = static_cast<Record *>(inputRecords_->elementAt(itemIndex));
    while (itemIndex < inputRecords_->size()) {
        if (nextBucket == NULL ||
            comparator_->compare(*r->name_, nextBucket->lowerBoundary_) < 0) {
                // Record goes in current bucket.  Advance to next record,
                // stay on current bucket.
                destBucket->records_->addElement(r, status);
                ++itemIndex;
                r = static_cast<Record *>(inputRecords_->elementAt(itemIndex));
        } else {
            // Advance to the next bucket, stay on current record.
            bucketIndex++;
            destBucket = nextBucket;
            if (bucketIndex+1 < bucketList_->size()) {
                nextBucket = static_cast<Bucket *>(bucketList_->elementAt(bucketIndex+1));
            } else {
                nextBucket = NULL;
            }
            U_ASSERT(destBucket != NULL);
        }
    }
                
}


void AlphabeticIndex::getIndexExemplars(UnicodeSet  &dest, const Locale &locale, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }

    LocalULocaleDataPointer uld(ulocdata_open(locale.getName(), &status));
    UnicodeSet exemplars;
    ulocdata_getExemplarSet(uld.getAlias(), exemplars.toUSet(), 0, ULOCDATA_ES_INDEX, &status);
    if (U_SUCCESS(status)) {
        dest.addAll(exemplars);
        return;
    }
    status = U_ZERO_ERROR;  // Clear out U_MISSING_RESOURCE_ERROR

    // Locale data did not include explicit Index characters.
    // Synthesize a set of them from the locale's standard exemplar characters.

    ulocdata_getExemplarSet(uld.getAlias(), exemplars.toUSet(), 0, ULOCDATA_ES_STANDARD, &status);
    if (U_FAILURE(status)) {
        return;
    }

    // Upper-case any that aren't already so.
    //   (We only do this for synthesized index characters.)

    UnicodeSetIterator it(exemplars);
    UnicodeString upperC;
    UnicodeSet  lowersToRemove;
    UnicodeSet  uppersToAdd;
    while (it.next()) {
        const UnicodeString &exemplarC = it.getString();
        upperC = exemplarC;
        upperC.toUpper(locale);
        if (exemplarC != upperC) {
            lowersToRemove.add(exemplarC);
            uppersToAdd.add(upperC);
        }
    }
    exemplars.removeAll(lowersToRemove);
    exemplars.addAll(uppersToAdd);

    // get the exemplars, and handle special cases

    // question: should we add auxiliary exemplars?
    if (exemplars.containsSome(*CORE_LATIN)) {
        exemplars.addAll(*CORE_LATIN);
    }
    if (exemplars.containsSome(*HANGUL)) {
        // cut down to small list
        UnicodeSet BLOCK_HANGUL_SYLLABLES(UNICODE_STRING_SIMPLE("[:block=hangul_syllables:]"), status);
        exemplars.removeAll(BLOCK_HANGUL_SYLLABLES);
        exemplars.addAll(*HANGUL);
    }
    if (exemplars.containsSome(*ETHIOPIC)) {
        // cut down to small list
        // make use of the fact that Ethiopic is allocated in 8's, where
        // the base is 0 mod 8.
        UnicodeSetIterator  it(*ETHIOPIC);
        while (it.next() && !it.isString()) {
            if ((it.getCodepoint() & 0x7) != 0) {
                exemplars.remove(it.getCodepoint());
            }
        }
    }
    dest.addAll(exemplars);
}


/*
 * Return the string with interspersed CGJs. Input must have more than 2 codepoints.
 */
static const UChar32 CGJ = (UChar)0x034F;
UnicodeString AlphabeticIndex::separated(const UnicodeString &item) {
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


UBool AlphabeticIndex::operator==(const AlphabeticIndex& /* other */) const {
    return FALSE;
}


UBool AlphabeticIndex::operator!=(const AlphabeticIndex& /* other */) const {
    return FALSE;
}


const Collator &AlphabeticIndex::getCollator() const {
    return *comparator_;
}


const UnicodeString &AlphabeticIndex::getInflowLabel() const {
    return inflowLabel_;
}

const UnicodeString &AlphabeticIndex::getOverflowLabel() const {
    return overflowLabel_;
}


const UnicodeString &AlphabeticIndex::getUnderflowLabel() const {
    return underflowLabel_;
}


AlphabeticIndex &AlphabeticIndex::setInflowLabel(const UnicodeString &label, UErrorCode &/*status*/) {
    inflowLabel_ = label;
    return *this;
}


AlphabeticIndex &AlphabeticIndex::setOverflowLabel(const UnicodeString &label, UErrorCode &/*status*/) {
    overflowLabel_ = label;
    return *this;
}


AlphabeticIndex &AlphabeticIndex::setUnderflowLabel(const UnicodeString &label, UErrorCode &/*status*/) {
    underflowLabel_ = label;
    return *this;
}


UnicodeString AlphabeticIndex::getOverflowComparisonString(const UnicodeString &lowerLimit, UErrorCode &status) {
    for (int32_t i=0; i<firstScriptCharacters_->size(); i++) {
        const UnicodeString *s = 
                static_cast<const UnicodeString *>(firstScriptCharacters_->elementAt(i));
        if (comparator_->compare(*s, lowerLimit) > 0) {
            return *s;
        }
    }
    return *EMPTY_STRING;
}

UnicodeSet *AlphabeticIndex::getScriptSet(UnicodeSet &dest, const UnicodeString &codePoint, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return &dest;
    }
    UChar32 cp = codePoint.char32At(0);
    UScriptCode scriptCode = uscript_getScript(cp, &status);
    dest.applyIntPropertyValue(UCHAR_SCRIPT, scriptCode, status);
    return &dest;
}

//
//  init() - Common code for constructors.
//

void AlphabeticIndex::init(UErrorCode &status) {
    // Initialize statics if needed.
    AlphabeticIndex::staticInit(status);

    // Put the object into a known state so that the destructor will function.

    alreadyIn_             = NULL;
    bucketList_            = NULL;
    comparator_            = NULL;
    currentBucket_         = NULL;
    firstScriptCharacters_ = NULL;
    indexBuildRequired_    = TRUE;
    indexCharacters_       = NULL;
    inputRecords_          = NULL;
    itemsIterIndex_        = 0;
    labelsIterIndex_       = 0;
    noDistinctSorting_     = NULL;
    notAlphabetic_         = NULL;
    rawIndexChars_         = NULL;

    if (U_FAILURE(status)) {
        return;
    }
    alreadyIn_             = uhash_open(uhash_hashUnicodeString,    // Key Hash,
                                        uhash_compareUnicodeString, // key Comparator, 
                                        NULL,                       // value Comparator
                                        &status);
    uhash_setKeyDeleter(alreadyIn_, uhash_deleteUnicodeString);
    uhash_setValueDeleter(alreadyIn_, uhash_deleteUnicodeSet);

    bucketList_            = new UVector(status);
    bucketList_->setDeleter(indexChars_deleteBucket);
    indexCharacters_       = new UVector(status);
    indexCharacters_->setDeleter(uhash_deleteUnicodeString);
    indexCharacters_->setComparer(uhash_compareUnicodeString);

    inputRecords_          = new UVector(status);
    noDistinctSorting_     = new UnicodeSet();
    notAlphabetic_         = new UnicodeSet();
    // firstScriptCharacters_ = new UVector(status);
    rawIndexChars_         = new UnicodeSet();

    inflowLabel_.remove();
    inflowLabel_.append((UChar)0x2026);    // Ellipsis
    overflowLabel_ = inflowLabel_;
    underflowLabel_ = inflowLabel_;

    // TODO:  check for memory allocation failures.
}

   
static  UBool  indexCharactersAreInitialized = FALSE;

//  Index Characters Clean up function.  Delete statically allocated constant stuff.
U_CDECL_BEGIN
static UBool U_CALLCONV indexCharacters_cleanup(void) {
    AlphabeticIndex::staticCleanup();
    return TRUE;
}
U_CDECL_END

void AlphabeticIndex::staticCleanup() {
    delete ALPHABETIC;
    ALPHABETIC = NULL;
    delete HANGUL;
    HANGUL = NULL;
    delete ETHIOPIC;
    ETHIOPIC = NULL;
    delete CORE_LATIN;
    CORE_LATIN = NULL;
    delete IGNORE_SCRIPTS;
    IGNORE_SCRIPTS = NULL;
    delete TO_TRY;
    TO_TRY = NULL;
    delete FIRST_CHARS_IN_SCRIPTS;
    FIRST_CHARS_IN_SCRIPTS = NULL;
    delete EMPTY_STRING;
    EMPTY_STRING = NULL;
    nfkdNormalizer = NULL;  // ref to a singleton.  Do not delete.
    indexCharactersAreInitialized = FALSE;
}


UnicodeSet *AlphabeticIndex::ALPHABETIC;
UnicodeSet *AlphabeticIndex::HANGUL;
UnicodeSet *AlphabeticIndex::ETHIOPIC;
UnicodeSet *AlphabeticIndex::CORE_LATIN;
UnicodeSet *AlphabeticIndex::IGNORE_SCRIPTS;
UnicodeSet *AlphabeticIndex::TO_TRY;
UVector    *AlphabeticIndex::FIRST_CHARS_IN_SCRIPTS;
const UnicodeString *AlphabeticIndex::EMPTY_STRING;

//
//  staticInit()    One-time initialization of constants.
//                  Thread safe.  Called from constructors.
//                  Mutex overhead is not a concern.  AlphabeticIndex constructors are 
//                  sufficiently heavy that the cost of the mutex check is not significant.

void AlphabeticIndex::staticInit(UErrorCode &status) {
    static UMTX IndexCharsInitMutex;

    Mutex mutex(&IndexCharsInitMutex);
    if (indexCharactersAreInitialized || U_FAILURE(status)) {
        return;
    }
    UBool finishedInit = FALSE;

    {
        UnicodeString alphaString = UNICODE_STRING_SIMPLE("[[:alphabetic:]-[:mark:]]");
        ALPHABETIC = new UnicodeSet(alphaString, status);
        if (ALPHABETIC == NULL) {
            goto err;
        }
        
        HANGUL = new UnicodeSet();
        HANGUL->add(0xAC00).add(0xB098).add(0xB2E4).add(0xB77C).add(0xB9C8).add(0xBC14).add(0xC0AC).
                add(0xC544).add(0xC790).add(0xCC28).add(0xCE74).add(0xD0C0).add(0xD30C).add(0xD558);
        if (HANGUL== NULL) {
            goto err;
        }


        UnicodeString EthiopicStr = UNICODE_STRING_SIMPLE("[[:Block=Ethiopic:]&[:Script=Ethiopic:]]");
        ETHIOPIC = new UnicodeSet(EthiopicStr, status);
        if (ETHIOPIC == NULL) {
            goto err;
        }
       
        CORE_LATIN = new UnicodeSet((UChar32)0x61, (UChar32)0x7a);  // ('a', 'z');
        if (CORE_LATIN == NULL) {
            goto err;
        }

        UnicodeString IgnoreStr= UNICODE_STRING_SIMPLE(
                "[[:sc=Common:][:sc=inherited:][:script=Unknown:][:script=braille:]]");
        IGNORE_SCRIPTS = new UnicodeSet(IgnoreStr, status);
        IGNORE_SCRIPTS->freeze();
        if (IGNORE_SCRIPTS == NULL) {
            goto err;
        }

        UnicodeString nfcqcStr = UNICODE_STRING_SIMPLE("[:^nfcqc=no:]");
        TO_TRY = new UnicodeSet(nfcqcStr, status);
        if (TO_TRY == NULL) {
            goto err;
        }

        EMPTY_STRING = new UnicodeString();

        Collator *rootCollator = Collator::createInstance(Locale::getRoot(), status);
        FIRST_CHARS_IN_SCRIPTS = firstStringsInScript(rootCollator, status);
        delete rootCollator;
        if (FIRST_CHARS_IN_SCRIPTS == NULL) {
            goto err;
        }

        nfkdNormalizer = Normalizer2::getInstance(NULL, "nfkc", UNORM2_DECOMPOSE, status);
        if (nfkdNormalizer == NULL) {
            goto err;
        }
    }
    finishedInit = TRUE;

  err:
    if (!finishedInit && U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(status)) {
        indexCharacters_cleanup();
        return;
    }
    ucln_i18n_registerCleanup(UCLN_I18N_INDEX_CHARACTERS, indexCharacters_cleanup);
    indexCharactersAreInitialized = TRUE;
}


//
//  Comparison function for UVector<UnicodeString *> sorting with a collator.
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

//
//  Comparison function for UVector<Record *> sorting with a collator.
//
static int32_t U_CALLCONV
recordCompareFn(const void *context, const void *left, const void *right) {
    const UHashTok *leftTok = static_cast<const UHashTok *>(left);
    const UHashTok *rightTok = static_cast<const UHashTok *>(right);
    const AlphabeticIndex::Record *leftRec  = static_cast<const AlphabeticIndex::Record *>(leftTok->pointer);
    const AlphabeticIndex::Record *rightRec = static_cast<const AlphabeticIndex::Record *>(rightTok->pointer);
    const Collator *col = static_cast<const Collator *>(context);
    
    Collator::EComparisonResult r = col->compare(*leftRec->name_, *rightRec->name_);
    if (r == 0) {
        r = (leftRec->serialNumber_ < rightRec->serialNumber_) ? Collator::LESS : Collator::GREATER;
    }
    return (int32_t) r;
}


UVector *AlphabeticIndex::firstStringsInScript(Collator *ruleBasedCollator, UErrorCode &status) {

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


void AlphabeticIndex::addItem(const icu_45::UnicodeString &name, void *context, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    Record *r = new Record;
    // Note on ownership of the name string:  It stays with the Record.
    r->name_ = static_cast<UnicodeString *>(name.clone());
    r->context_ = context;
    r->serialNumber_ = ++recordCounter_;
    inputRecords_->addElement(r, status);
    indexBuildRequired_ = TRUE;
}


void AlphabeticIndex::removeAllItems(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    // TODO:  get an object deleter function on inputRecords
    inputRecords_->removeAllElements();
    indexBuildRequired_ = TRUE;
}


UBool AlphabeticIndex::nextLabel(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (indexBuildRequired_ && currentBucket_ != NULL) {
        status = U_ENUM_OUT_OF_SYNC_ERROR;
        return FALSE;
    }
    buildIndex(status);  
    if (U_FAILURE(status)) {
        return FALSE;
    }
    ++labelsIterIndex_;
    if (labelsIterIndex_ >= bucketList_->size()) {
        labelsIterIndex_ = bucketList_->size();
        return FALSE;
    }
    currentBucket_ = static_cast<Bucket *>(bucketList_->elementAt(labelsIterIndex_));
    resetItemIterator();
    return TRUE;
}

const UnicodeString &AlphabeticIndex::getLabel() const {
    if (currentBucket_ != NULL) {
        return currentBucket_->label_;
    } else {    
        return *EMPTY_STRING;
    }
}


UAlphabeticIndexLabelType AlphabeticIndex::getLabelType() const {
    if (currentBucket_ != NULL) {
        return currentBucket_->labelType_;
    } else {
        return ALPHABETIC_INDEX_NONE;
    }
}


void AlphabeticIndex::resetLabelIterator(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    buildIndex(status);
    labelsIterIndex_ = -1;
    currentBucket_ = NULL;
}


UBool AlphabeticIndex::nextItem(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (currentBucket_ == NULL) {
        // We are trying to iterate over the items in a bucket, but there is no
        // current bucket from the enumeration of buckets.
        status = U_INVALID_STATE_ERROR;
        return FALSE;
    }
    if (indexBuildRequired_) {
        status = U_ENUM_OUT_OF_SYNC_ERROR;
        return FALSE;
    }
    ++itemsIterIndex_;
    if (itemsIterIndex_ >= currentBucket_->records_->size()) {
        itemsIterIndex_  = currentBucket_->records_->size();
        return FALSE;
    }
    return TRUE;
}


const UnicodeString &AlphabeticIndex::getItemName() const {
    const UnicodeString *retStr = EMPTY_STRING;
    if (currentBucket_ != NULL &&
        itemsIterIndex_ >= 0 &&
        itemsIterIndex_ < currentBucket_->records_->size()) {
            Record *item = static_cast<Record *>(currentBucket_->records_->elementAt(itemsIterIndex_));
            retStr = item->name_;
    }
    return *retStr;
}

const void *AlphabeticIndex::getItemContext() const {
    const void *retPtr = NULL;
    if (currentBucket_ != NULL &&
        itemsIterIndex_ >= 0 &&
        itemsIterIndex_ < currentBucket_->records_->size()) {
            Record *item = static_cast<Record *>(currentBucket_->records_->elementAt(itemsIterIndex_));
            retPtr = item->context_;
    }
    return retPtr;
}


void AlphabeticIndex::resetItemIterator() {
    itemsIterIndex_ = -1;
}



AlphabeticIndex::Bucket::Bucket(const UnicodeString &label,
                                const UnicodeString &lowerBoundary,
                                UAlphabeticIndexLabelType type,
                                UErrorCode &status):
         label_(label), lowerBoundary_(lowerBoundary), labelType_(type), records_(NULL) {
    if (U_FAILURE(status)) {
        return;
    }
    records_ = new UVector(status);
    if (records_ == NULL && U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}


AlphabeticIndex::Bucket::~Bucket() {
    delete records_;
}

U_NAMESPACE_END
