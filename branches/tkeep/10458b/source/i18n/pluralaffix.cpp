/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: pluralaffix.cpp
 */

#include "unicode/utypes.h"
#include "cstring.h"
#include "digitaffix.h"
#include "pluralaffix.h"

U_NAMESPACE_BEGIN

// TODO: Duplicate code in quantityformatter.cpp
// other must always be first.
static const char * const gPluralForms[] = {
        "other", "zero", "one", "two", "few", "many"};

static int32_t getPluralIndex(const char *pluralForm) {
    int32_t len = UPRV_LENGTHOF(gPluralForms);
    for (int32_t i = 0; i < len; ++i) {
        if (uprv_strcmp(pluralForm, gPluralForms[i]) == 0) {
            return i;
        }
    }
    return -1;
}


PluralAffix::PluralAffix() : otherAffix() {
    affixes[0] = &otherAffix;
    for (int32_t i = 1; i < UPRV_LENGTHOF(affixes); ++i) {
        affixes[i] = NULL;
    }
}

PluralAffix::PluralAffix(const PluralAffix &other)
        : otherAffix(other.otherAffix) {
    affixes[0] = &otherAffix;
    for (int32_t i = 1; i < UPRV_LENGTHOF(affixes); ++i) {
        affixes[i] = other.affixes[i] ? new DigitAffix(*other.affixes[i]) : NULL;
    }
}

PluralAffix &
PluralAffix::operator=(const PluralAffix &other) {
    if (this == &other) {
        return *this;
    }
    for (int32_t i = 0; i < UPRV_LENGTHOF(affixes); ++i) {
        if (affixes[i] != NULL && other.affixes[i] != NULL) {
            *affixes[i] = *other.affixes[i];
        } else if (affixes[i] != NULL) {
            delete affixes[i];
            affixes[i] = NULL;
        } else if (other.affixes[i] != NULL) {
            affixes[i] = new DigitAffix(*other.affixes[i]);
        } else {
            // do nothing
        }
    }
    return *this;
}

PluralAffix::~PluralAffix() {
    for (int32_t i = 1; i < UPRV_LENGTHOF(affixes); ++i) {
        delete affixes[i];
    }
}

UBool
PluralAffix::setVariant(
        const char *variant, const UnicodeString &value, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    int32_t pluralIndex = getPluralIndex(variant);
    if (pluralIndex == -1) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    if (affixes[pluralIndex] == NULL) {
        affixes[pluralIndex] = new DigitAffix;
    }
    if (affixes[pluralIndex] == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }
    affixes[pluralIndex]->remove();
    affixes[pluralIndex]->append(value);
    return TRUE;
}

void
PluralAffix::remove() {
    affixes[0]->remove();
    for (int32_t i = 1; i < UPRV_LENGTHOF(affixes); ++i) {
        delete affixes[i];
        affixes[i] = NULL;
    }
}

void
PluralAffix::append(
        const UnicodeString &value, int32_t fieldId) {
    for (int32_t i = 0; i < UPRV_LENGTHOF(affixes); ++i) {
        if (affixes[i] != NULL) {
            affixes[i]->append(value, fieldId);
        }
    }
}

UBool
PluralAffix::append(
        const PluralAffix &rhs, int32_t fieldId, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    for (int i = 1; i < UPRV_LENGTHOF(affixes); ++i) {
        if (affixes[i] != NULL && rhs.affixes[i] != NULL) {
            affixes[i]->append(rhs.affixes[i]->toString(), fieldId);
        } else if (affixes[i] != NULL) {
            affixes[i]->append(rhs.affixes[0]->toString(), fieldId);
        } else if (rhs.affixes[i] != NULL) {
            affixes[i] = new DigitAffix(*affixes[0]);
            if (affixes[i] == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return FALSE;
            }
            affixes[i]->append(rhs.affixes[i]->toString(), fieldId);
        } else {
            // Do nothing.
        }
    }
    // Have to do this at the end so that in the for loop the other form
    // which is affixes[0] is guaranteed to be what it was before appending
    // began.
    affixes[0]->append(rhs.affixes[0]->toString(), fieldId);
    return TRUE;
}

const DigitAffix &
PluralAffix::getByVariant(const char *variant) const {
    int32_t pluralIndex = getPluralIndex(variant);
    if (pluralIndex == -1) {
        pluralIndex = 0;
    }
    const DigitAffix *affix = affixes[pluralIndex];
    if (affix == NULL) {
        affix = affixes[0];
    }
    return *affix;
}

UBool
PluralAffix::hasMultipleVariants() const {
    for (int32_t i = 1; i < UPRV_LENGTHOF(affixes); ++i) {
        if (affixes[i] != NULL) {
            return TRUE;
        }
    }
    return FALSE;
}

U_NAMESPACE_END

