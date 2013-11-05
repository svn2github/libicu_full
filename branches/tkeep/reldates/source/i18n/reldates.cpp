#include "unicode/reldates.h"

U_NAMESPACE_BEGIN

RelativeDateTimeFormatter::RelativeDateTimeFormatter(UErrorCode& status) {
}

RelativeDateTimeFormatter::RelativeDateTimeFormatter(const Locale& locale, UErrorCode& status) {
}


RelativeDateTimeFormatter::RelativeDateTimeFormatter(const RelativeDateTimeFormatter& other) {
}


RelativeDateTimeFormatter& RelativeDateTimeFormatter::operator=(const RelativeDateTimeFormatter& other) {
    return *this;
}

RelativeDateTimeFormatter::~RelativeDateTimeFormatter() {
}


UnicodeString& RelativeDateTimeFormatter::format(
    double quantity, UDateDirection direction, UDateRelativeUnit unit,
    UnicodeString& appendTo, UErrorCode& status) const {
    return appendTo;
}

UnicodeString& RelativeDateTimeFormatter::format(
    UDateDirection direction, UDateAbsoluteUnit unit,
    UnicodeString& appendTo, UErrorCode& status) const {
    return appendTo;
}

UnicodeString& RelativeDateTimeFormatter::combineDateAndTime(
    const UnicodeString& relativeDateString, const UnicodeString& timeString,
    UnicodeString& appendTo, UErrorCode& status) const {
    return appendTo;
}

void RelativeDateTimeFormatter::setNumberFormat(const NumberFormat& nf) {
}

U_NAMESPACE_END

