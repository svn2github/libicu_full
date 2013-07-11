/*
*******************************************************************************
* Copyright (C) 1996-2013, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

/*
* File coleitr.cpp
*
* 
*
* Created by: Helena Shih
*
* Modification History:
*
*  Date      Name        Description
*
*  6/23/97   helena      Adding comments to make code more readable.
* 08/03/98   erm         Synched with 1.2 version of CollationElementIterator.java
* 12/10/99   aliu        Ported Thai collation support from Java.
* 01/25/01   swquek      Modified to a C++ wrapper calling C APIs (ucoliter.h)
* 02/19/01   swquek      Removed CollationElementIterator() since it is 
*                        private constructor and no calls are made to it
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coleitr.h"
#include "unicode/ucoleitr.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "collationiterator.h"
#include "collationsets.h"
#include "collationtailoring.h"
#include "rulebasedcollator.h"
#include "uassert.h"
#include "ucol_imp.h"
#include "uhash.h"
#include "utf16collationiterator.h"

/* Constants --------------------------------------------------------------- */

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CollationElementIterator)

/* CollationElementIterator public constructor/destructor ------------------ */

CollationElementIterator::CollationElementIterator(
                                         const CollationElementIterator& other) 
        : UObject(other), iter_(NULL), rbc_(NULL), otherHalf_(0), dir_(0),
          m_data_(NULL), isDataOwned_(TRUE)
{
    if (other.m_data_ != NULL)
    {
        UErrorCode status = U_ZERO_ERROR;
        m_data_ = ucol_openElements(other.m_data_->iteratordata_.coll, NULL, 0, 
                                    &status);
    }
    *this = other;
}

CollationElementIterator::~CollationElementIterator()
{
    delete iter_;
    if (isDataOwned_) {
        ucol_closeElements(m_data_);
    }
}

/* CollationElementIterator public methods --------------------------------- */

namespace {

uint32_t getFirstHalf(uint32_t p, uint32_t lower32) {
    return (p & 0xffff0000) | ((lower32 >> 16) & 0xff00) | ((lower32 >> 8) & 0xff);
}
uint32_t getSecondHalf(uint32_t p, uint32_t lower32) {
    return (p << 16) | ((lower32 >> 8) & 0xff00) | (lower32 & 0x3f);
}
UBool ceNeedsTwoParts(int64_t ce) {
    return (ce & 0xffff00ff003f) != 0;
}

}  // namespace

int32_t CollationElementIterator::getOffset() const
{
    if (m_data_ != NULL) {
        return ucol_getOffset(m_data_);
    }
    return iter_->getOffset();
}

/**
* Get the ordering priority of the next character in the string.
* @return the next character's ordering. Returns NULLORDER if an error has 
*         occured or if the end of string has been reached
*/
int32_t CollationElementIterator::next(UErrorCode& status)
{
    if (m_data_ != NULL) {
        return ucol_next(m_data_, &status);
    }
    if (U_FAILURE(status)) { return NULLORDER; }
    if (dir_ == 0) {
        // The iter_ is already reset to the start of the text.
        dir_ = 2;
    } else if (dir_ < 0) {
        // illegal change of direction
        status = U_INVALID_STATE_ERROR;
        return NULLORDER;
    } else if (dir_ == 1) {
        // next() after setOffset()
        dir_ = 2;
    }
    if (otherHalf_ != 0) {
        uint32_t oh = otherHalf_;
        otherHalf_ = 0;
        return oh;
    }
    // No need to keep all CEs in the buffer when we iterate.
    iter_->clearCEsIfNoneRemaining();
    int64_t ce = iter_->nextCE(status);
    if (ce == Collation::NO_CE) { return NULLORDER; }
    // Turn the 64-bit CE into two old-style 32-bit CEs, without quaternary bits.
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t lower32 = (uint32_t)ce;
    uint32_t firstHalf = getFirstHalf(p, lower32);
    uint32_t secondHalf = getSecondHalf(p, lower32);
    if (secondHalf != 0) {
        otherHalf_ = secondHalf | 0xc0;  // continuation CE
    }
    return firstHalf;
}

UBool CollationElementIterator::operator!=(
                                  const CollationElementIterator& other) const
{
    return !(*this == other);
}

UBool CollationElementIterator::operator==(
                                    const CollationElementIterator& that) const
{
    if (this == &that) {
        return TRUE;
    }

    if ((m_data_ != NULL) != (that.m_data_ != NULL)) { return FALSE; }
    if (m_data_ != NULL) {
        if (m_data_ == that.m_data_) {
            return TRUE;
        }

        // option comparison
        if (m_data_->iteratordata_.coll != that.m_data_->iteratordata_.coll)
        {
            return FALSE;
        }

        // the constructor and setText always sets a length
        // and we only compare the string not the contents of the normalization
        // buffer
        int thislength = (int)(m_data_->iteratordata_.endp - m_data_->iteratordata_.string);
        int thatlength = (int)(that.m_data_->iteratordata_.endp - that.m_data_->iteratordata_.string);
        
        if (thislength != thatlength) {
            return FALSE;
        }

        if (uprv_memcmp(m_data_->iteratordata_.string, 
                        that.m_data_->iteratordata_.string, 
                        thislength * U_SIZEOF_UCHAR) != 0) {
            return FALSE;
        }
        if (getOffset() != that.getOffset()) {
            return FALSE;
        }

        // checking normalization buffer
        if ((m_data_->iteratordata_.flags & UCOL_ITER_HASLEN) == 0) {
            if ((that.m_data_->iteratordata_.flags & UCOL_ITER_HASLEN) != 0) {
                return FALSE;
            }
            // both are in the normalization buffer
            if (m_data_->iteratordata_.pos 
                - m_data_->iteratordata_.writableBuffer.getBuffer()
                != that.m_data_->iteratordata_.pos 
                - that.m_data_->iteratordata_.writableBuffer.getBuffer()) {
                // not in the same position in the normalization buffer
                return FALSE;
            }
        }
        else if ((that.m_data_->iteratordata_.flags & UCOL_ITER_HASLEN) == 0) {
            return FALSE;
        }
        // checking ce position
        return (m_data_->iteratordata_.CEpos - m_data_->iteratordata_.CEs)
                == (that.m_data_->iteratordata_.CEpos 
                                            - that.m_data_->iteratordata_.CEs);
    }
    return
        (rbc_ == that.rbc_ || *rbc_ == *that.rbc_) &&
        otherHalf_ == that.otherHalf_ &&
        dir_ == that.dir_ &&
        string_ == that.string_ &&
        *iter_ == *that.iter_;
}

/**
* Get the ordering priority of the previous collation element in the string.
* @param status the error code status.
* @return the previous element's ordering. Returns NULLORDER if an error has 
*         occured or if the start of string has been reached.
*/
int32_t CollationElementIterator::previous(UErrorCode& status)
{
    if (m_data_ != NULL) {
        return ucol_previous(m_data_, &status);
    }
    if (U_FAILURE(status)) { return NULLORDER; }
    if (dir_ == 0) {
        iter_->resetToOffset(string_.length());
        dir_ = -1;
    } else if (dir_ == 1) {
        // previous() after setOffset()
        dir_ = -1;
    } else if (dir_ > 1) {
        // illegal change of direction
        status = U_INVALID_STATE_ERROR;
        return NULLORDER;
    }
    if (otherHalf_ != 0) {
        uint32_t oh = otherHalf_;
        otherHalf_ = 0;
        return oh;
    }
    int64_t ce = iter_->previousCE(status);
    if (ce == Collation::NO_CE) { return NULLORDER; }
    // Turn the 64-bit CE into two old-style 32-bit CEs, without quaternary bits.
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t lower32 = (uint32_t)ce;
    uint32_t firstHalf = getFirstHalf(p, lower32);
    uint32_t secondHalf = getSecondHalf(p, lower32);
    if (secondHalf != 0) {
        otherHalf_ = firstHalf;
        return secondHalf | 0xc0;  // continuation CE
    }
    return firstHalf;
}

/**
* Resets the cursor to the beginning of the string.
*/
void CollationElementIterator::reset()
{
    if (m_data_ != NULL) {
        ucol_reset(m_data_);
        return;
    }
    iter_ ->resetToOffset(0);
    dir_ = 0;
}

void CollationElementIterator::setOffset(int32_t newOffset, 
                                         UErrorCode& status)
{
    if (m_data_ != NULL) {
        ucol_setOffset(m_data_, newOffset, &status);
        return;
    }
    if (U_FAILURE(status)) { return; }
    iter_->resetToOffset(newOffset);
    dir_ = 1;
}

/**
* Sets the source to the new source string.
*/
void CollationElementIterator::setText(const UnicodeString& source,
                                       UErrorCode& status)
{
    if (U_FAILURE(status)) {
        return;
    }

    if (m_data_ != NULL) {
        int32_t length = source.length();
        UChar *string = NULL;
        if (m_data_->isWritable && m_data_->iteratordata_.string != NULL) {
            uprv_free((UChar *)m_data_->iteratordata_.string);
        }
        m_data_->isWritable = TRUE;
        if (length > 0) {
            string = (UChar *)uprv_malloc(U_SIZEOF_UCHAR * length);
            /* test for NULL */
            if (string == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            u_memcpy(string, source.getBuffer(), length);
        }
        else {
            string = (UChar *)uprv_malloc(U_SIZEOF_UCHAR);
            /* test for NULL */
            if (string == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            *string = 0;
        }
        /* Free offsetBuffer before initializing it. */
        ucol_freeOffsetBuffer(&(m_data_->iteratordata_));
        uprv_init_collIterate(m_data_->iteratordata_.coll, string, length, 
            &m_data_->iteratordata_, &status);

        m_data_->reset_   = TRUE;
        return;
    }

    string_ = source;
    const UChar *s = string_.getBuffer();
    CollationIterator *newIter;
    UBool numeric = rbc_->settings->isNumeric();
    if (rbc_->settings->dontCheckFCD()) {
        newIter = new UTF16CollationIterator(rbc_->data, numeric, s, s, s + string_.length());
    } else {
        newIter = new FCDUTF16CollationIterator(rbc_->data, numeric, s, s, s + string_.length());
    }
    if (newIter == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    delete iter_;
    iter_ = newIter;
}

// Sets the source to the new character iterator.
void CollationElementIterator::setText(CharacterIterator& source, 
                                       UErrorCode& status)
{
    if (U_FAILURE(status)) 
        return;

    if (m_data_ != NULL) {
        int32_t length = source.getLength();
        UChar *buffer = NULL;

        if (length == 0) {
            buffer = (UChar *)uprv_malloc(U_SIZEOF_UCHAR);
            /* test for NULL */
            if (buffer == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            *buffer = 0;
        }
        else {
            buffer = (UChar *)uprv_malloc(U_SIZEOF_UCHAR * length);
            /* test for NULL */
            if (buffer == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            /* 
            Using this constructor will prevent buffer from being removed when
            string gets removed
            */
            UnicodeString string;
            source.getText(string);
            u_memcpy(buffer, string.getBuffer(), length);
        }

        if (m_data_->isWritable && m_data_->iteratordata_.string != NULL) {
            uprv_free((UChar *)m_data_->iteratordata_.string);
        }
        m_data_->isWritable = TRUE;
        /* Free offsetBuffer before initializing it. */
        ucol_freeOffsetBuffer(&(m_data_->iteratordata_));
        uprv_init_collIterate(m_data_->iteratordata_.coll, buffer, length, 
            &m_data_->iteratordata_, &status);
        m_data_->reset_   = TRUE;
        return;
    }

    source.getText(string_);
    setText(string_, status);
}

int32_t CollationElementIterator::strengthOrder(int32_t order) const
{
    UColAttributeValue s;
    if (m_data_ != NULL) {
        s = ucol_getStrength(m_data_->iteratordata_.coll);
    } else {
        s = (UColAttributeValue)rbc_->settings->getStrength();
    }
    // Mask off the unwanted differences.
    if (s == UCOL_PRIMARY) {
        order &= RuleBasedCollator::PRIMARYDIFFERENCEONLY;
    }
    else if (s == UCOL_SECONDARY) {
        order &= RuleBasedCollator::SECONDARYDIFFERENCEONLY;
    }

    return order;
}

/* CollationElementIterator private constructors/destructors --------------- */

/** 
* This is the "real" constructor for this class; it constructs an iterator
* over the source text using the specified collator
*/
CollationElementIterator::CollationElementIterator(
                                               const UnicodeString& sourceText,
                                               const RuleBasedCollator* order,
                                               UErrorCode& status)
        : iter_(NULL), rbc_(NULL), otherHalf_(0), dir_(0),
          m_data_(NULL), isDataOwned_(TRUE)
{
    if (U_FAILURE(status)) {
        return;
    }

    int32_t length = sourceText.length();
    UChar *string = NULL;

    if (length > 0) {
        string = (UChar *)uprv_malloc(U_SIZEOF_UCHAR * length);
        /* test for NULL */
        if (string == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        /* 
        Using this constructor will prevent buffer from being removed when
        string gets removed
        */
        u_memcpy(string, sourceText.getBuffer(), length);
    }
    else {
        string = (UChar *)uprv_malloc(U_SIZEOF_UCHAR);
        /* test for NULL */
        if (string == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        *string = 0;
    }
    m_data_ = ucol_openElements(order->ucollator, string, length, &status);

    /* Test for buffer overflows */
    if (U_FAILURE(status)) {
        return;
    }
    m_data_->isWritable = TRUE;
}

CollationElementIterator::CollationElementIterator(
                                               const UnicodeString &source,
                                               const RuleBasedCollator2 *coll,
                                               UErrorCode &status)
        : iter_(NULL), rbc_(coll), otherHalf_(0), dir_(0),
          m_data_(NULL), isDataOwned_(FALSE)
{
    setText(source, status);
}

/** 
* This is the "real" constructor for this class; it constructs an iterator over 
* the source text using the specified collator
*/
CollationElementIterator::CollationElementIterator(
                                           const CharacterIterator& sourceText,
                                           const RuleBasedCollator* order,
                                           UErrorCode& status)
        : iter_(NULL), rbc_(NULL), otherHalf_(0), dir_(0),
          m_data_(NULL), isDataOwned_(TRUE)
{
    if (U_FAILURE(status))
        return;

    int32_t length = sourceText.getLength();
    UChar *buffer;
    if (length > 0) {
        buffer = (UChar *)uprv_malloc(U_SIZEOF_UCHAR * length);
        /* test for NULL */
        if (buffer == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        /* 
        Using this constructor will prevent buffer from being removed when
        string gets removed
        */
        UnicodeString string(buffer, length, length);
        ((CharacterIterator &)sourceText).getText(string);
        const UChar *temp = string.getBuffer();
        u_memcpy(buffer, temp, length);
    }
    else {
        buffer = (UChar *)uprv_malloc(U_SIZEOF_UCHAR);
        /* test for NULL */
        if (buffer == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        *buffer = 0;
    }
    m_data_ = ucol_openElements(order->ucollator, buffer, length, &status);

    /* Test for buffer overflows */
    if (U_FAILURE(status)) {
        return;
    }
    m_data_->isWritable = TRUE;
}

CollationElementIterator::CollationElementIterator(
                                           const CharacterIterator &source,
                                           const RuleBasedCollator2 *coll,
                                           UErrorCode &status)
        : iter_(NULL), rbc_(coll), otherHalf_(0), dir_(0),
          m_data_(NULL), isDataOwned_(FALSE)
{
    // We only call source.getText() which should be const anyway.
    setText(const_cast<CharacterIterator &>(source), status);
}

/* CollationElementIterator protected methods ----------------------------- */

const CollationElementIterator& CollationElementIterator::operator=(
                                         const CollationElementIterator& other)
{
    if (this == &other) {
        return *this;
    }

    if (other.m_data_ != NULL)
    {
        UCollationElements *ucolelem      = this->m_data_;
        UCollationElements *otherucolelem = other.m_data_;
        collIterate        *coliter       = &(ucolelem->iteratordata_);
        collIterate        *othercoliter  = &(otherucolelem->iteratordata_);
        int                length         = 0;

        // checking only UCOL_ITER_HASLEN is not enough here as we may be in 
        // the normalization buffer
        length = (int)(othercoliter->endp - othercoliter->string);

        ucolelem->reset_         = otherucolelem->reset_;
        ucolelem->isWritable     = TRUE;

        /* create a duplicate of string */
        if (length > 0) {
            coliter->string = (UChar *)uprv_malloc(length * U_SIZEOF_UCHAR);
            if(coliter->string != NULL) {
                uprv_memcpy((UChar *)coliter->string, othercoliter->string,
                    length * U_SIZEOF_UCHAR);
            } else { // Error: couldn't allocate memory. No copying should be done
                length = 0;
            }
        }
        else {
            coliter->string = NULL;
        }

        /* start and end of string */
        coliter->endp = coliter->string == NULL ? NULL : coliter->string + length;

        /* handle writable buffer here */

        if (othercoliter->flags & UCOL_ITER_INNORMBUF) {
            coliter->writableBuffer = othercoliter->writableBuffer;
            coliter->writableBuffer.getTerminatedBuffer();
        }

        /* current position */
        if (othercoliter->pos >= othercoliter->string && 
            othercoliter->pos <= othercoliter->endp)
        {
            U_ASSERT(coliter->string != NULL);
            coliter->pos = coliter->string + 
                (othercoliter->pos - othercoliter->string);
        }
        else {
            coliter->pos = coliter->writableBuffer.getTerminatedBuffer() + 
                (othercoliter->pos - othercoliter->writableBuffer.getBuffer());
        }

        /* CE buffer */
        int32_t CEsize;
        if (coliter->extendCEs) {
            uprv_memcpy(coliter->CEs, othercoliter->CEs, sizeof(uint32_t) * UCOL_EXPAND_CE_BUFFER_SIZE);
            CEsize = sizeof(othercoliter->extendCEs);
            if (CEsize > 0) {
                othercoliter->extendCEs = (uint32_t *)uprv_malloc(CEsize);
                uprv_memcpy(coliter->extendCEs, othercoliter->extendCEs, CEsize);
            }
            coliter->toReturn = coliter->extendCEs + 
                (othercoliter->toReturn - othercoliter->extendCEs);
            coliter->CEpos    = coliter->extendCEs + CEsize;
        } else {
            CEsize = (int32_t)(othercoliter->CEpos - othercoliter->CEs);
            if (CEsize > 0) {
                uprv_memcpy(coliter->CEs, othercoliter->CEs, CEsize);
            }
            coliter->toReturn = coliter->CEs + 
                (othercoliter->toReturn - othercoliter->CEs);
            coliter->CEpos    = coliter->CEs + CEsize;
        }

        if (othercoliter->fcdPosition != NULL) {
            U_ASSERT(coliter->string != NULL);
            coliter->fcdPosition = coliter->string + 
                (othercoliter->fcdPosition 
                - othercoliter->string);
        }
        else {
            coliter->fcdPosition = NULL;
        }
        coliter->flags       = othercoliter->flags/*| UCOL_ITER_HASLEN*/;
        coliter->origFlags   = othercoliter->origFlags;
        coliter->coll = othercoliter->coll;
        this->isDataOwned_ = TRUE;
    }

    if (other.iter_ != NULL) {
        CollationIterator *newIter;
        const FCDUTF16CollationIterator *otherFCDIter =
                dynamic_cast<const FCDUTF16CollationIterator *>(other.iter_);
        if(otherFCDIter != NULL) {
            newIter = new FCDUTF16CollationIterator(*otherFCDIter, string_.getBuffer());
        } else {
            const UTF16CollationIterator *otherIter =
                    dynamic_cast<const UTF16CollationIterator *>(other.iter_);
            if(otherIter != NULL) {
                newIter = new UTF16CollationIterator(*otherIter, string_.getBuffer());
            } else {
                newIter = NULL;
            }
        }
        if(newIter != NULL) {
            delete iter_;
            iter_ = newIter;
            rbc_ = other.rbc_;
            otherHalf_ = other.otherHalf_;
            dir_ = other.dir_;

            string_ = other.string_;
        }
    }

    return *this;
}

namespace {

class MaxExpSink : public ContractionsAndExpansions::CESink {
public:
    MaxExpSink(UHashtable *h, UErrorCode &ec) : maxExpansions(h), errorCode(ec) {}
    virtual ~MaxExpSink();
    virtual void handleCE(int64_t /*ce*/) {}
    virtual void handleExpansion(const int64_t ces[], int32_t length) {
        if (length <= 1) {
            // We do not need to add single CEs into the map.
            return;
        }
        int32_t count = 0;  // number of CE "halves"
        for (int32_t i = 0; i < length; ++i) {
            count += ceNeedsTwoParts(ces[i]) ? 2 : 1;
        }
        // last "half" of the last CE
        int64_t ce = ces[length - 1];
        uint32_t p = (uint32_t)(ce >> 32);
        uint32_t lower32 = (uint32_t)ce;
        uint32_t lastHalf = getSecondHalf(p, lower32);
        if (lastHalf == 0) {
            lastHalf = getFirstHalf(p, lower32);
            U_ASSERT(lastHalf != 0);
        } else {
            lastHalf |= 0xc0;  // old-style continuation CE
        }
        if (count > uhash_igeti(maxExpansions, (int32_t)lastHalf)) {
            uhash_iputi(maxExpansions, (int32_t)lastHalf, count, &errorCode);
        }
    }

private:
    UHashtable *maxExpansions;
    UErrorCode &errorCode;
};

MaxExpSink::~MaxExpSink() {}

}  // namespace

UHashtable *
CollationElementIterator::computeMaxExpansions(const CollationData *data, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return NULL; }
    UHashtable *maxExpansions = uhash_open(uhash_hashLong, uhash_compareLong,
                                           uhash_compareLong, &errorCode);
    if (U_FAILURE(errorCode)) { return NULL; }
    MaxExpSink sink(maxExpansions, errorCode);
    ContractionsAndExpansions(NULL, NULL, &sink, TRUE).forData(data, errorCode);
    if (U_FAILURE(errorCode)) {
        uhash_close(maxExpansions);
        return NULL;
    }
    return maxExpansions;
}

int32_t
CollationElementIterator::getMaxExpansion(int32_t order) const {
    if (m_data_ != NULL) {
        return ucol_getMaxExpansion(m_data_, (uint32_t)order);
    }
    return getMaxExpansion(
        static_cast<const UHashtable *>(rbc_->tailoring->maxExpansionsSingleton.fInstance),
        order);
}

int32_t
CollationElementIterator::getMaxExpansion(const UHashtable *maxExpansions, int32_t order) {
    if (order == 0) { return 1; }
    int32_t max;
    if(maxExpansions != NULL && (max = uhash_igeti(maxExpansions, order)) != 0) {
        return max;
    }
    if ((order & 0xc0) == 0xc0) {
        // old-style continuation CE
        return 2;
    } else {
        return 1;
    }
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_COLLATION */

/* eof */
