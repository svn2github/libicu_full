/*
**********************************************************************
*   Copyright (C) 2008-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/utypes.h"

#include "unicode/uchar.h"
#include "unicode/utf16.h"

#include "identifier_info.h"
#include "mutex.h"
#include "scriptset.h"


U_NAMESPACE_BEGIN

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

static UMutex gInitMutex = U_MUTEX_INITIALIZER;
static UBool gStaticsAreInitialized = FALSE;

IdentifierInfo::IdentifierInfo(UErrorCode &status):
         fIdentifier(NULL), fRequiredScripts(NULL), fScriptSetSet(NULL), 
         fCommonAmongAlternates(NULL), fNumerics(NULL), fIdentifierProfile(NULL) {
    if (U_FAILURE(status)) {
        return;
    }
    {
        Mutex lock(&gInitMutex);
        if (!gStaticsAreInitialized) {
            ASCII    = new UnicodeSet(0, 0x7f);
            JAPANESE = new ScriptSet();
            CHINESE  = new ScriptSet();
            KOREAN   = new ScriptSet();
            CONFUSABLE_WITH_LATIN = new ScriptSet();
            if (ASCII == NULL || JAPANESE == NULL || CHINESE == NULL || KOREAN == NULL 
                    || CONFUSABLE_WITH_LATIN == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            ASCII->freeze();
            JAPANESE->set(USCRIPT_LATIN, status).set(USCRIPT_HAN, status).set(USCRIPT_HIRAGANA, status)
                     .set(USCRIPT_KATAKANA, status);
            CHINESE->set(USCRIPT_LATIN, status).set(USCRIPT_HAN, status).set(USCRIPT_BOPOMOFO, status);
            KOREAN->set(USCRIPT_LATIN, status).set(USCRIPT_HAN, status).set(USCRIPT_HANGUL, status);
            CONFUSABLE_WITH_LATIN->set(USCRIPT_CYRILLIC, status).set(USCRIPT_GREEK, status)
                      .set(USCRIPT_CHEROKEE, status);
            gStaticsAreInitialized = TRUE;
        }
    }
    fIdentifier = new UnicodeString();
    fRequiredScripts = new ScriptSet();
    fScriptSetSet = uhash_open(uhash_hashScriptSet, uhash_compareScriptSet, NULL, &status);
    uhash_setKeyDeleter(fScriptSetSet, uhash_deleteScriptSet);
    fCommonAmongAlternates = new ScriptSet();
    fNumerics = new UnicodeSet();
    fIdentifierProfile = new UnicodeSet(0, 0x10FFFF);

    if (U_SUCCESS(status) && (fIdentifier == NULL || fRequiredScripts == NULL || fScriptSetSet == NULL ||
                              fCommonAmongAlternates == NULL || fNumerics == NULL || fIdentifierProfile == NULL)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
};

IdentifierInfo::~IdentifierInfo() {
    delete fIdentifier;
    delete fRequiredScripts;
    uhash_close(fScriptSetSet);
    delete fCommonAmongAlternates;
    delete fNumerics;
    delete fIdentifierProfile;
};


IdentifierInfo &IdentifierInfo::clear() {
    fRequiredScripts->resetAll();
    uhash_removeAll(fScriptSetSet);
    fNumerics->clear();
    fCommonAmongAlternates->resetAll();
    return *this;
}


IdentifierInfo &IdentifierInfo::setIdentifierProfile(const UnicodeSet &identifierProfile) {
    *fIdentifierProfile = identifierProfile;
    return *this;
}


const UnicodeSet &IdentifierInfo::getIdentifierProfile() const {
    return *fIdentifierProfile;
}


IdentifierInfo &IdentifierInfo::setIdentifier(const UnicodeString &identifier, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    *fIdentifier = identifier;
    clear();
    ScriptSet *temp = new ScriptSet(); // Will reuse this.
    UChar32 cp;
    for (int32_t i = 0; i < identifier.length(); i += U16_LENGTH(cp)) {
        cp = identifier.char32At(i);
        // Store a representative character for each kind of decimal digit
        if (u_charType(cp) == U_DECIMAL_DIGIT_NUMBER) {
            // Just store the zero character as a representative for comparison. Unicode guarantees it is cp - value
            fNumerics->add(cp - u_getNumericValue(cp));
        }
        UScriptCode extensions[500];
        int32_t extensionsCount = uscript_getScriptExtensions(cp, extensions, LENGTHOF(extensions), &status);
        if (U_FAILURE(status)) {
            return *this;
        }
        temp->resetAll();
        for (int32_t j=0; j<extensionsCount; j++) {
            temp->set(extensions[j], status);
        }
        temp->reset(USCRIPT_COMMON, status);
        temp->reset(USCRIPT_INHERITED, status);
        switch (temp->countMembers()) {
        case 0: break;
        case 1:
            // Single script, record it.
            fRequiredScripts->Union(*temp);
            break;
        default:
            if (!fRequiredScripts->intersects(*temp) 
                    && !uhash_geti(fScriptSetSet, temp)) {
                // If the set hasn't been added already, add it and create new temporary for the next pass,
                // so we don't rewrite what's already in the set.
                uhash_puti(fScriptSetSet, temp, 1, &status);  // Takes ownership of temp.
                temp = new ScriptSet();
            }
            break;
        }
    }
    // Now make a final pass through to remove alternates that came before singles.
    // [Kana], [Kana Hira] => [Kana]
    // This is relatively infrequent, so doesn't have to be optimized.
    // We also compute any commonalities among the alternates.
    if (uhash_count(fScriptSetSet) == 0) {
        fCommonAmongAlternates->resetAll();
    } else {
        fCommonAmongAlternates->setAll();
        int32_t it = -1;
        for (;;) {
            const UHashElement *hashEl = uhash_nextElement(fScriptSetSet, &it);
            if (hashEl == NULL) {
                break;
            }
            ScriptSet *next = static_cast<ScriptSet *>(hashEl->value.pointer);
            // final BitSet next = it.next();
            if (fRequiredScripts->intersects(*next)) {
                uhash_removeElement(fScriptSetSet, hashEl);
            } else {
                // [[Arab Syrc Thaa]; [Arab Syrc]] => [[Arab Syrc]]
                int32_t otherIt = -1;
                for (;;) {
                    const UHashElement *otherHashEl = uhash_nextElement(fScriptSetSet, &otherIt);
                    if (otherHashEl == NULL) {
                        break;
                    }
                    ScriptSet *other = static_cast<ScriptSet *>(otherHashEl->value.pointer);
                    if (next != other && next->contains(*other)) {
                        uhash_removeElement(fScriptSetSet, hashEl);
                        break;
                    }
                }
                fCommonAmongAlternates->intersect(*next);
            }
        }
        if (uhash_count(fScriptSetSet) == 0) {
            fCommonAmongAlternates->resetAll();
        }
    }
    // Note that the above code doesn't minimize alternatives. That is, it does not collapse
    // [[Arab Syrc Thaa]; [Arab Syrc]] to [[Arab Syrc]]
    // That would be a possible optimization, but is probably not worth the extra processing
    return *this;
}


const UnicodeString *IdentifierInfo::getIdentifier() const {
    return fIdentifier;
}

const ScriptSet *IdentifierInfo::getScripts() const {
    return fRequiredScripts;
}

const UHashtable *IdentifierInfo::getAlternates() const {
    return fScriptSetSet;
}


const UnicodeSet *IdentifierInfo::getNumerics() const {
    return fNumerics;
}

const ScriptSet *IdentifierInfo::getCommonAmongAlternates() const {
    return fCommonAmongAlternates;
}

URestrictionLevel IdentifierInfo::getRestrictionLevel(UErrorCode &status) const {
    if (!fIdentifierProfile->containsAll(*fIdentifier) || getNumerics()->size() > 1) {
        return USPOOF_UNRESTRICTIVE;
    }
    if (ASCII->containsAll(*fIdentifier)) {
        return USPOOF_ASCII;
    }
    ScriptSet temp;
    temp.Union(*fRequiredScripts);
    temp.reset(USCRIPT_COMMON, status);
    temp.reset(USCRIPT_INHERITED, status);
    // This is a bit tricky. We look at a number of factors.
    // The number of scripts in the text.
    // Plus 1 if there is some commonality among the alternates (eg [Arab Thaa]; [Arab Syrc])
    // Plus number of alternates otherwise (this only works because we only test cardinality up to 2.)
    int32_t cardinalityPlus = temp.countMembers() + 
            (fCommonAmongAlternates->countMembers() == 0 ? uhash_count(fScriptSetSet) : 1);
    if (cardinalityPlus < 2) {
        return USPOOF_HIGHLY_RESTRICTIVE;
    }
    if (containsWithAlternates(*JAPANESE, temp) || containsWithAlternates(*CHINESE, temp)
            || containsWithAlternates(*KOREAN, temp)) {
        return USPOOF_HIGHLY_RESTRICTIVE;
    }
    if (cardinalityPlus == 2 && temp.test(USCRIPT_LATIN, status) && !temp.intersects(*CONFUSABLE_WITH_LATIN)) {
        return USPOOF_MODERATELY_RESTRICTIVE;
    }
    return USPOOF_MINIMALLY_RESTRICTIVE;
}


UBool IdentifierInfo::containsWithAlternates(const ScriptSet &container, const ScriptSet &containee) const {
    if (!container.contains(containee)) {
        return FALSE;
    }
    for (int32_t iter = -1; ;) {
        const UHashElement *hashEl = uhash_nextElement(fScriptSetSet, &iter);
        if (hashEl == NULL) {
            break;
        }
        ScriptSet *alternatives = static_cast<ScriptSet *>(hashEl->value.pointer);
        if (!container.intersects(*alternatives)) {
            return false;
        }
    }
    return true;
}


UnicodeSet *IdentifierInfo::ASCII;
ScriptSet *IdentifierInfo::JAPANESE;
ScriptSet *IdentifierInfo::CHINESE;
ScriptSet *IdentifierInfo::KOREAN;
ScriptSet *IdentifierInfo::CONFUSABLE_WITH_LATIN;



U_NAMESPACE_END


/**
 * Order BitSets, first by shortest, then by items.
 * Conforms to UElementComparator from uelement.h
 * @internal
 */
U_CAPI int8_t U_CALLCONV IdentifierInfoScriptSetCompare(UElement e0, UElement e1) {
    icu::ScriptSet *s0 = static_cast<icu::ScriptSet *>(e0.pointer);
    icu::ScriptSet *s1 = static_cast<icu::ScriptSet *>(e1.pointer);
    int32_t diff = s0->countMembers() - s1->countMembers();
    if (diff != 0) return diff;
    int32_t i0 = s0->nextSetBit(0);
    int32_t i1 = s1->nextSetBit(0);
    while ((diff = i0-i1) == 0 && i0 > 0) {
        i0 = s0->nextSetBit((i0+1));
        i1 = s1->nextSetBit((i1+1));
    }
    return diff;    
}
