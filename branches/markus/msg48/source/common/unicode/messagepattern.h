/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  messagepattern.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011mar14
*   created by: Markus W. Scherer
*/

#ifndef __MESSAGEPATTERN_H__
#define __MESSAGEPATTERN_H__

/**
 * \file
 * \brief C++ API: MessagePattern class: Parses and represents ICU MessageFormat patterns.
 */

#include "unicode/utypes.h"
#include "unicode/parseerr.h"
#include "unicode/unistr.h"

/**
 * Mode for when an apostrophe starts quoted literal text for MessageFormat output.
 * The default is DOUBLE_OPTIONAL unless overridden via ICUConfig
 * (/com/ibm/icu/ICUConfig.properties).
 * <p>
 * A pair of adjacent apostrophes always results in a single apostrophe in the output,
 * even when the pair is between two single, text-quoting apostrophes.
 * <p>
 * The following table shows examples of desired MessageFormat.format() output
 * with the pattern strings that yield that output.
 * <p>
 * <table>
 *   <tr>
 *     <th>Desired output</th>
 *     <th>DOUBLE_OPTIONAL</th>
 *     <th>DOUBLE_REQUIRED</th>
 *   </tr>
 *   <tr>
 *     <td>I see {many}</td>
 *     <td>I see '{many}'</td>
 *     <td>(same)</td>
 *   </tr>
 *   <tr>
 *     <td>I said {'Wow!'}</td>
 *     <td>I said '{''Wow!''}'</td>
 *     <td>(same)</td>
 *   </tr>
 *   <tr>
 *     <td>I don't know</td>
 *     <td>I don't know OR<br> I don''t know</td>
 *     <td>I don''t know</td>
 *   </tr>
 * </table>
 * @draft ICU 4.8
 */
enum UMessagePatternApostropheMode {
    /**
     * A literal apostrophe is represented by
     * either a single or a double apostrophe pattern character.
     * Within a MessageFormat pattern, a single apostrophe only starts quoted literal text
     * if it immediately precedes a curly brace {},
     * or a pipe symbol | if inside a choice format,
     * or a pound symbol # if inside a plural format.
     * <p>
     * This is the default behavior starting with ICU 4.8.
     * @draft ICU 4.8
     */
    UMSGPAT_APOS_DOUBLE_OPTIONAL,
    /**
     * A literal apostrophe must be represented by
     * a double apostrophe pattern character.
     * A single apostrophe always starts quoted literal text.
     * <p>
     * This is the behavior of ICU 4.6 and earlier, and of the JDK.
     * @draft ICU 4.8
     */
    UMSGPAT_APOS_DOUBLE_REQUIRED
};
typedef enum UMessagePatternApostropheMode UMessagePatternApostropheMode;

/**
 * MessagePattern::Part type constants.
 * @draft ICU 4.8
 */
enum UMessagePatternPartType {
    /**
     * Start of a message pattern (main or nested).
     * The length is 0 for the top-level message
     * and for a choice argument sub-message, otherwise 1 for the '{'.
     * The value indicates the nesting level, starting with 0 for the main message.
     * <p>
     * There is always a later MSG_LIMIT part.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_MSG_START,
    /**
     * End of a message pattern (main or nested).
     * The length is 0 for the top-level message and
     * the last sub-message of a choice argument,
     * otherwise 1 for the '}' or (in a choice argument style) the '|'.
     * The value indicates the nesting level, starting with 0 for the main message.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_MSG_LIMIT,
    /**
     * Indicates a substring of the pattern string which is to be skipped when formatting.
     * For example, an apostrophe that begins or ends quoted text
     * would be indicated with such a part.
     * The value is undefined and currently always 0.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_SKIP_SYNTAX,
    /**
     * Indicates that a syntax character needs to be inserted for auto-quoting.
     * The length is 0.
     * The value is the character code of the insertion character. (U+0027=APOSTROPHE)
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_INSERT_CHAR,
    /**
     * Indicates a syntactic (non-escaped) # symbol in a plural variant.
     * When formatting, replace this part's substring with the
     * (value-offset) for the plural argument value.
     * The value is undefined and currently always 0.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_REPLACE_NUMBER,
    /**
     * Start of an argument.
     * The length is 1 for the '{'.
     * The value is the ordinal value of the ArgType. Use getArgType().
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_ARG_START,
    /**
     * End of an argument.
     * The length is 1 for the '}'.
     * The value is the ordinal value of the ArgType. Use getArgType().
     * <p>
     * This part is followed by either an ARG_NUMBER or ARG_NAME,
     * followed by optional argument sub-parts (see UMessagePatternArgType constants)
     * and finally an ARG_LIMIT part.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_ARG_LIMIT,
    /**
     * The argument number, provided by the value.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_ARG_NUMBER,
    /**
     * The argument name.
     * The value is undefined and currently always 0.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_ARG_NAME,
    /**
     * The argument type.
     * The value is undefined and currently always 0.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_ARG_TYPE,
    /**
     * The argument style text.
     * The value is undefined and currently always 0.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_ARG_STYLE,
    /**
     * A selector substring in a "complex" argument style.
     * The value is undefined and currently always 0.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_ARG_SELECTOR,
    /**
     * An integer value, for example the offset or an explicit selector value
     * in a PluralFormat style.
     * The part value is the integer value.
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_ARG_INT,
    /**
     * A numeric value, for example the offset or an explicit selector value
     * in a PluralFormat style.
     * The part value is an index into an internal array of numeric values;
     * use getNumericValue().
     * @draft ICU 4.8
     */
    UMSGPAT_PART_TYPE_ARG_DOUBLE
};
typedef enum UMessagePatternPartType UMessagePatternPartType;

/**
 * Argument type constants.
 * Returned by Part.getArgType() for ARG_START and ARG_LIMIT parts.
 *
 * Messages nested inside an argument are each delimited by MSG_START and MSG_LIMIT,
 * with a nesting level one greater than the surrounding message.
 * @draft ICU 4.8
 */
enum UMessagePatternArgType {
    /**
     * The argument has no specified type.
     * @draft ICU 4.8
     */
    UMSGPAT_ARG_TYPE_NONE,
    /**
     * The argument has a "simple" type which is provided by the ARG_TYPE part.
     * An ARG_STYLE part might follow that.
     * @draft ICU 4.8
     */
    UMSGPAT_ARG_TYPE_SIMPLE,
    /**
     * The argument is a ChoiceFormat with one or more
     * ((ARG_INT | ARG_DOUBLE), ARG_SELECTOR, message) tuples.
     * @draft ICU 4.8
     */
    UMSGPAT_ARG_TYPE_CHOICE,
    /**
     * The argument is a PluralFormat with an optional ARG_INT or ARG_DOUBLE offset
     * (e.g., offset:1)
     * and one or more (ARG_SELECTOR [explicit-value] message) tuples.
     * If the selector has an explicit value (e.g., =2), then
     * that value is provided by the ARG_INT or ARG_DOUBLE part preceding the message.
     * Otherwise the message immediately follows the ARG_SELECTOR.
     * @draft ICU 4.8
     */
    UMSGPAT_ARG_TYPE_PLURAL,
    /**
     * The argument is a SelectFormat with one or more (ARG_SELECTOR, message) pairs.
     * @draft ICU 4.8
     */
    UMSGPAT_ARG_TYPE_SELECT
};
typedef enum UMessagePatternArgType UMessagePatternArgType;

enum {
    /**
     * Return value from MessagePattern.validateArgumentName() for when
     * the string is a valid "pattern identifier" but not a number.
     * @draft ICU 4.8
     */
    UMSGPAT_ARG_NAME_NOT_NUMBER=-1,

    /**
     * Return value from MessagePattern.validateArgumentName() for when
     * the string is invalid.
     * It might not be a valid "pattern identifier",
     * or it have only ASCII digits but there is a leading zero or the number is too large.
     * @draft ICU 4.8
     */
    UMSGPAT_ARG_NAME_NOT_VALID=-2
};

/**
 * Special value that is returned by getNumericValue(Part) when no
 * numeric value is defined for a part.
 * @see MessagePattern.getNumericValue()
 * @draft ICU 4.8
 */
#define UMSGPAT_NO_NUMERIC_VALUE ((double)(-123456789))

U_NAMESPACE_BEGIN

class MessagePatternDoubleList;
class MessagePatternPartsList;

/**
 * Parses and represents ICU MessageFormat patterns.
 * Also handles patterns for ChoiceFormat, PluralFormat and SelectFormat.
 * Used in the implementations of those classes as well as in tools
 * for message validation, translation and format conversion.
 * <p>
 * The parser handles all syntax relevant for identifying message arguments.
 * This includes "complex" arguments whose style strings contain
 * nested MessageFormat pattern substrings.
 * For "simple" arguments (with no nested MessageFormat pattern substrings),
 * the argument style is not parsed any further.
 * <p>
 * The parser handles named and numbered message arguments and allows both in one message.
 * <p>
 * Once a pattern has been parsed successfully, iterate through the parsed data
 * with countParts(), getPart() and related methods.
 * <p>
 * The data logically represents a parse tree, but is stored and accessed
 * as a list of "parts" for fast and simple parsing and to minimize object allocations.
 * Arguments and nested messages are best handled via recursion.
 * For every _START "part", MessagePattern.getLimitPartIndex() efficiently returns
 * the index of the corresponding _LIMIT "part".
 * <p>
 * List of "parts":
 * <pre>
 * message = MSG_START (SKIP_SYNTAX | INSERT_CHAR | REPLACE_NUMBER | argument)* MSG_LIMIT
 * argument = noneArg | simpleArg | complexArg
 * complexArg = choiceArg | pluralArg | selectArg
 *
 * noneArg = ARG_START.NONE (ARG_NAME | ARG_NUMBER) ARG_LIMIT.NONE
 * simpleArg = ARG_START.SIMPLE (ARG_NAME | ARG_NUMBER) ARG_TYPE [ARG_STYLE] ARG_LIMIT.SIMPLE
 * choiceArg = ARG_START.CHOICE (ARG_NAME | ARG_NUMBER) choiceStyle ARG_LIMIT.CHOICE
 * pluralArg = ARG_START.PLURAL (ARG_NAME | ARG_NUMBER) pluralStyle ARG_LIMIT.PLURAL
 * selectArg = ARG_START.SELECT (ARG_NAME | ARG_NUMBER) selectStyle ARG_LIMIT.SELECT
 *
 * choiceStyle = ((ARG_INT | ARG_DOUBLE) ARG_SELECTOR message)+
 * pluralStyle = [ARG_INT | ARG_DOUBLE] (ARG_SELECTOR [ARG_INT | ARG_DOUBLE] message)+
 * selectStyle = (ARG_SELECTOR message)+
 * </pre>
 * <ul>
 *   <li>Literal output text is not represented directly by "parts" but accessed
 *       between parts of a message, from one part's getLimit() to the next part's getIndex().
 *   <li><code>ARG_START.CHOICE</code> stands for an ARG_START Part with ArgType CHOICE.
 *   <li>In the choiceStyle, the ARG_SELECTOR has the '#' or the less-than sign (U+2264).
 *   <li>In the pluralStyle, the first, optional numeric Part has the "offset:" value.
 *       The optional numeric Part between each (ARG_SELECTOR, message) pair
 *       is the value of an explicit-number selector like "=2",
 *       otherwise the selector is a non-numeric identifier.
 *   <li>The REPLACE_NUMBER Part can occur only in an immediate sub-message of the pluralStyle.
 * <p>
 * This class is not intended for public subclassing.
 *
 * @draft ICU 4.8
 */
class U_COMMON_API MessagePattern : public UObject {
public:
    /**
     * Constructs an empty MessagePattern with default UMessagePatternApostropheMode.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @draft ICU 4.8
     */
    MessagePattern(UErrorCode &errorCode);

    /**
     * Constructs an empty MessagePattern.
     * @param mode Explicit UMessagePatternApostropheMode.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @draft ICU 4.8
     */
    MessagePattern(UMessagePatternApostropheMode mode, UErrorCode &errorCode);

    /**
     * Constructs a MessagePattern with default UMessagePatternApostropheMode and
     * parses the MessageFormat pattern string.
     * @param pattern a MessageFormat pattern string
     * @param parseError Struct to receive information on the position
     *                   of an error within the pattern.
     *                   Can be NULL.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * TODO: turn @throws into UErrorCode specifics?
     * @throws IllegalArgumentException for syntax errors in the pattern string
     * @throws IndexOutOfBoundsException if certain limits are exceeded
     *         (e.g., argument number too high, argument name too long, etc.)
     * @throws NumberFormatException if a number could not be parsed
     * @draft ICU 4.8
     */
    MessagePattern(const UnicodeString &pattern, UParseError *parseError, UErrorCode &errorCode);

    /**
     * Copy constructor.
     * @param other Object to copy.
     * @draft ICU 4.8
     */
    MessagePattern(const MessagePattern &other);

    /**
     * Assignment operator.
     * @param other Object to copy.
     * @return *this=other
     * @draft ICU 4.8
     */
    MessagePattern &operator=(const MessagePattern &other);

    /**
     * Destructor.
     * @draft ICU 4.8
     */
    virtual ~MessagePattern();

    /**
     * Parses a MessageFormat pattern string.
     * @param pattern a MessageFormat pattern string
     * @param parseError Struct to receive information on the position
     *                   of an error within the pattern.
     *                   Can be NULL.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return *this
     * @throws IllegalArgumentException for syntax errors in the pattern string
     * @throws IndexOutOfBoundsException if certain limits are exceeded
     *         (e.g., argument number too high, argument name too long, etc.)
     * @throws NumberFormatException if a number could not be parsed
     * @draft ICU 4.8
     */
    MessagePattern &parse(const UnicodeString &pattern,
                          UParseError *parseError, UErrorCode &errorCode);

    /**
     * Parses a ChoiceFormat pattern string.
     * @param pattern a ChoiceFormat pattern string
     * @param parseError Struct to receive information on the position
     *                   of an error within the pattern.
     *                   Can be NULL.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return *this
     * @throws IllegalArgumentException for syntax errors in the pattern string
     * @throws IndexOutOfBoundsException if certain limits are exceeded
     *         (e.g., argument number too high, argument name too long, etc.)
     * @throws NumberFormatException if a number could not be parsed
     * @draft ICU 4.8
     */
    MessagePattern &parseChoiceStyle(const UnicodeString &pattern,
                                     UParseError *parseError, UErrorCode &errorCode);

    /**
     * Parses a PluralFormat pattern string.
     * @param pattern a PluralFormat pattern string
     * @param parseError Struct to receive information on the position
     *                   of an error within the pattern.
     *                   Can be NULL.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return *this
     * @throws IllegalArgumentException for syntax errors in the pattern string
     * @throws IndexOutOfBoundsException if certain limits are exceeded
     *         (e.g., argument number too high, argument name too long, etc.)
     * @throws NumberFormatException if a number could not be parsed
     * @draft ICU 4.8
     */
    MessagePattern &parsePluralStyle(const UnicodeString &pattern,
                                     UParseError *parseError, UErrorCode &errorCode);

    /**
     * Parses a SelectFormat pattern string.
     * @param pattern a SelectFormat pattern string
     * @param parseError Struct to receive information on the position
     *                   of an error within the pattern.
     *                   Can be NULL.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return *this
     * @throws IllegalArgumentException for syntax errors in the pattern string
     * @throws IndexOutOfBoundsException if certain limits are exceeded
     *         (e.g., argument number too high, argument name too long, etc.)
     * @throws NumberFormatException if a number could not be parsed
     * @draft ICU 4.8
     */
    MessagePattern &parseSelectStyle(const UnicodeString &pattern,
                                     UParseError *parseError, UErrorCode &errorCode);

    /**
     * Clears this MessagePattern, returning it to the state after the default constructor.
     * @draft ICU 4.8
     */
    void clear();

    /**
     * @param other another object to compare with.
     * @return TRUE if this object is equivalent to the other one.
     * @draft ICU 4.8
     */
    UBool operator==(const MessagePattern &other) const;

    /**
     * @param other another object to compare with.
     * @return FALSE if this object is equivalent to the other one.
     * @draft ICU 4.8
     */
    inline UBool operator!=(const MessagePattern &other) const {
        return !operator==(other);
    }

    /**
     * @return A hash code for this object.
     * @draft ICU 4.8
     */
    int32_t hashCode() const;

    /**
     * @return this instance's UMessagePatternApostropheMode.
     * @draft ICU 4.8
     */
    UMessagePatternApostropheMode getApostropheMode() const {
        return aposMode;
    }

    // Java has package-private jdkAposMode() here.
    // In C++, this is declared in the MessageImpl class.

    /**
     * @return the parsed pattern string (null if none was parsed).
     * @draft ICU 4.8
     */
    const UnicodeString &getPatternString() const {
        return msg;
    }

    /**
     * Does the parsed pattern have named arguments like {first_name}?
     * @return TRUE if the parsed pattern has at least one named argument.
     * @draft ICU 4.8
     */
    UBool hasNamedArguments() const {
        return hasArgNames;
    }

    /**
     * Does the parsed pattern have numbered arguments like {2}?
     * @return TRUE if the parsed pattern has at least one numbered argument.
     * @draft ICU 4.8
     */
    UBool hasNumberedArguments() const {
        return hasArgNumbers;
    }

    /**
     * Validates and parses an argument name or argument number string.
     * An argument name must be a "pattern identifier", that is, it must contain
     * no Unicode Pattern_Syntax or Pattern_White_Space characters.
     * If it only contains ASCII digits, then it must be a small integer with no leading zero.
     * @param name Input string.
     * @return &gt;=0 if the name is a valid number,
     *         ARG_NAME_NOT_NUMBER (-1) if it is a "pattern identifier" but not all ASCII digits,
     *         ARG_NAME_NOT_VALID (-2) if it is neither.
     * @draft ICU 4.8
     */
    static int32_t validateArgumentName(const UnicodeString &name);

    /**
     * Returns a version of the parsed pattern string where each ASCII apostrophe
     * is doubled (escaped) if it is not already, and if it is not interpreted as quoting syntax.
     * <p>
     * For example, this turns "I don't '{know}' {gender,select,female{h''er}other{h'im}}."
     * into "I don''t '{know}' {gender,select,female{h''er}other{h''im}}."
     * @return the deep-auto-quoted version of the parsed pattern string.
     * @see MessageFormat.autoQuoteApostrophe()
     * @draft ICU 4.8
     */
    UnicodeString autoQuoteApostropheDeep() const;

    class Part;

    /**
     * Returns the number of "parts" created by parsing the pattern string.
     * Returns 0 if no pattern has been parsed or clear() was called.
     * @return the number of pattern parts.
     * @draft ICU 4.8
     */
    int32_t countParts() const {
        return partsLength;
    }

    /**
     * Gets the i-th pattern "part".
     * @param i The index of the Part data. (0..countParts()-1)
     * @return the i-th pattern "part".
     * @draft ICU 4.8
     */
    const Part &getPart(int32_t i) const {
        return parts[i];
    }

    /**
     * Returns the UMessagePatternPartType of the i-th pattern "part".
     * Convenience method for getPart(i).getType().
     * @param i The index of the Part data. (0..countParts()-1)
     * @return The UMessagePatternPartType of the i-th Part.
     * @draft ICU 4.8
     */
    UMessagePatternPartType getPartType(int32_t i) const {
        return getPart(i).type;
    }

    /**
     * Returns the pattern index of the specified pattern "part".
     * Convenience method for getPart(partIndex).getIndex().
     * @param partIndex The index of the Part data. (0..countParts()-1)
     * @return The pattern index of this Part.
     * @draft ICU 4.8
     */
    int32_t getPatternIndex(int32_t partIndex) const {
        return getPart(partIndex).index;
    }

    /**
     * Returns the substring of the pattern string indicated by the Part.
     * Convenience method for getPatternString().substring(part.getIndex(), part.getLimit()).
     * @param part a part of this MessagePattern.
     * @return the substring associated with part.
     * @draft ICU 4.8
     */
    UnicodeString getSubstring(const Part &part) const {
        return msg.tempSubString(part.index, part.length);
    }

    /**
     * Compares the part's substring with the input string s.
     * @param part a part of this MessagePattern.
     * @param s a string.
     * @return TRUE if getSubstring(part).equals(s).
     * @draft ICU 4.8
     */
    UBool partSubstringMatches(const Part &part, const UnicodeString &s) const {
        return msg.compare(part.index, part.length, s);
    }

    /**
     * Returns the numeric value associated with an ARG_INT or ARG_DOUBLE.
     * @param part a part of this MessagePattern.
     * @return the part's numeric value, or UMSGPAT_NO_NUMERIC_VALUE if this is not a numeric part.
     * @draft ICU 4.8
     */
    double getNumericValue(const Part &part) const;

    /**
     * Returns the "offset:" value of a PluralFormat argument, or 0 if none is specified.
     * @param pluralStart the index of the first PluralFormat argument style part. (0..countParts()-1)
     * @return the "offset:" value.
     * @draft ICU 4.8
     */
    double getPluralOffset(int32_t pluralStart) const;

    /**
     * Returns the index of the ARG|MSG_LIMIT part corresponding to the ARG|MSG_START at start.
     * @param start The index of some Part data (0..countParts()-1);
     *        this Part should be of Type ARG_START or MSG_START.
     * @return The first i>start where getPart(i).getType()==ARG|MSG_LIMIT at the same nesting level,
     *         or start itself if getPartType(msgStart)!=ARG|MSG_START.
     * @draft ICU 4.8
     */
    int32_t getLimitPartIndex(int32_t start) const {
        int32_t limit=getPart(start).limitPartIndex;
        if(limit<start) {
            return start;
        }
        return limit;
    }

    /**
     * A message pattern "part", representing a pattern parsing event.
     * There is a part for the start and end of a message or argument,
     * for quoting and escaping of and with ASCII apostrophes,
     * and for syntax elements of "complex" arguments.
     * @draft ICU 4.8
     */
    class Part : public UMemory {
    public:
        /**
         * Default constructor, do not use.
         * @internal
         */
        Part() {}

        /**
         * Returns the type of this part.
         * @return the part type.
         * @draft ICU 4.8
         */
        UMessagePatternPartType getType() const {
            return type;
        }

        /**
         * Returns the pattern string index associated with this Part.
         * @return this part's pattern string index.
         * @draft ICU 4.8
         */
        int32_t getIndex() const {
            return index;
        }

        /**
         * Returns the length of the pattern substring associated with this Part.
         * This is 0 for some parts.
         * @return this part's pattern string index.
         * @draft ICU 4.8
         */
        int32_t getLength() const {
            return length;
        }

        /**
         * Returns the pattern string limit (exclusive-end) index associated with this Part.
         * Convenience method for getIndex()+getLength().
         * @return this part's pattern string limit index, same as getIndex()+getLength().
         * @draft ICU 4.8
         */
        int32_t getLimit() const {
            return index+length;
        }

        /**
         * Returns a value associated with this part.
         * See the documentation of each part type for details.
         * @return the part value.
         * @draft ICU 4.8
         */
        int32_t getValue() const {
            return value;
        }

        /**
         * Returns the argument type if this part is of type ARG_START or ARG_LIMIT,
         * otherwise UMSGPAT_ARG_TYPE_NONE.
         * @return the argument type for this part.
         * @draft ICU 4.8
         */
        UMessagePatternArgType getArgType() const {
            UMessagePatternPartType type=getType();
            if(type==UMSGPAT_PART_TYPE_ARG_START || type==UMSGPAT_PART_TYPE_ARG_LIMIT) {
                return (UMessagePatternArgType)value;
            } else {
                return UMSGPAT_ARG_TYPE_NONE;
            }
        }

        /**
         * Indicates whether the Part type has a numeric value.
         * If so, then that numeric value can be retrieved via MessagePattern.getNumericValue().
         * @param type The Part type to be tested.
         * @return TRUE if the Part type has a numeric value.
         * @draft ICU 4.8
         */
        static UBool hasNumericValue(UMessagePatternPartType type) {
            return type==UMSGPAT_PART_TYPE_ARG_INT || type==UMSGPAT_PART_TYPE_ARG_DOUBLE;
        }

        /**
         * @param other another object to compare with.
         * @return TRUE if this object is equivalent to the other one.
         * @draft ICU 4.8
         */
        UBool operator==(const Part &other) const;

        /**
         * @param other another object to compare with.
         * @return FALSE if this object is equivalent to the other one.
         * @draft ICU 4.8
         */
        inline UBool operator!=(const Part &other) const {
            return !operator==(other);
        }

        /**
         * @return A hash code for this object.
         * @draft ICU 4.8
         */
        int32_t hashCode() const {
            return ((type*37+index)*37+length)*37+value;
        }

    private:
        friend class MessagePattern;

        static const int32_t MAX_LENGTH=0xffff;
        static const int32_t MAX_VALUE=0x7fff;

        // Some fields are not final because they are modified during pattern parsing.
        // After pattern parsing, the parts are effectively immutable.
        UMessagePatternPartType type;
        int32_t index;
        uint16_t length;
        int16_t value;
        int32_t limitPartIndex;
    };

private:
    void preParse(const UnicodeString &pattern, UParseError *parseError, UErrorCode &errorCode);

    void postParse();

    int32_t parseMessage(int32_t index, int32_t msgStartLength,
                         int32_t nestingLevel, UMessagePatternArgType parentType,
                         UParseError *parseError, UErrorCode &errorCode) {
        if(U_SUCCESS(errorCode)) {
            errorCode=U_UNSUPPORTED_ERROR;
        }
        return 0;
#if 0  // TODO
        if(nestingLevel>Part.MAX_VALUE) {
            throw new IndexOutOfBoundsException();
        }
        int32_t msgStart=parts.size();
        addPart(UMSGPAT_PART_TYPE_MSG_START, index, msgStartLength, nestingLevel);
        index+=msgStartLength;
        while(index<msg.length()) {
            char c=msg.charAt(index++);
            if(c=='\'') {
                if(index==msg.length()) {
                    // The apostrophe is the last character in the pattern. 
                    // Add a Part for auto-quoting.
                    addPart(UMSGPAT_PART_TYPE_INSERT_CHAR, index, 0, '\'');  // value=char to be inserted
                    needsAutoQuoting=TRUE;
                } else {
                    c=msg.charAt(index);
                    if(c=='\'') {
                        // double apostrophe, skip the second one
                        addPart(UMSGPAT_PART_TYPE_SKIP_SYNTAX, index++, 1, 0);
                    } else if(
                        aposMode==ApostropheMode.DOUBLE_REQUIRED ||
                        c=='{' || c=='}' ||
                        (parentType==UMSGPAT_ARG_TYPE_CHOICE && c=='|') ||
                        (parentType==UMSGPAT_ARG_TYPE_PLURAL && c=='#')
                    ) {
                        // skip the quote-starting apostrophe
                        addPart(UMSGPAT_PART_TYPE_SKIP_SYNTAX, index-1, 1, 0);
                        // find the end of the quoted literal text
                        for(;;) {
                            index=msg.indexOf('\'', index+1);
                            if(index>=0) {
                                if((index+1)<msg.length() && msg.charAt(index+1)=='\'') {
                                    // double apostrophe inside quoted literal text
                                    // still encodes a single apostrophe, skip the second one
                                    addPart(UMSGPAT_PART_TYPE_SKIP_SYNTAX, ++index, 1, 0);
                                } else {
                                    // skip the quote-ending apostrophe
                                    addPart(UMSGPAT_PART_TYPE_SKIP_SYNTAX, index++, 1, 0);
                                    break;
                                }
                            } else {
                                // The quoted text reaches to the end of the of the message.
                                index=msg.length();
                                // Add a Part for auto-quoting.
                                addPart(UMSGPAT_PART_TYPE_INSERT_CHAR, index, 0, '\'');  // value=char to be inserted
                                needsAutoQuoting=TRUE;
                                break;
                            }
                        }
                    } else {
                        // Interpret the apostrophe as literal text.
                        // Add a Part for auto-quoting.
                        addPart(UMSGPAT_PART_TYPE_INSERT_CHAR, index, 0, '\'');  // value=char to be inserted
                        needsAutoQuoting=TRUE;
                    }
                }
            } else if(parentType==UMSGPAT_ARG_TYPE_PLURAL && c=='#') {
                // The unquoted # in a plural message fragment will be replaced
                // with the (number-offset).
                addPart(UMSGPAT_PART_TYPE_REPLACE_NUMBER, index-1, 1, 0);
            } else if(c=='{') {
                index=parseArg(index-1, 1, nestingLevel);
            } else if((nestingLevel>0 && c=='}') || (parentType==UMSGPAT_ARG_TYPE_CHOICE && c=='|')) {
                // Finish the message before the terminator.
                // In a choice style, report the "}" substring only for the following ARG_LIMIT,
                // not for this MSG_LIMIT.
                int32_t limitLength=(parentType==UMSGPAT_ARG_TYPE_CHOICE && c=='}') ? 0 : 1;
                addLimitPart(msgStart, UMSGPAT_PART_TYPE_MSG_LIMIT, index-1, limitLength, nestingLevel);
                if(parentType==UMSGPAT_ARG_TYPE_CHOICE) {
                    // Let the choice style parser see the '}' or '|'.
                    return index-1;
                } else {
                    // continue parsing after the '}'
                    return index;
                }
            }  // else: c is part of literal text
        }
        if(nestingLevel>0 && !inTopLevelChoiceMessage(nestingLevel, parentType)) {
            throw new IllegalArgumentException(
                "Unmatched '{' braces in message \""+prefix()+"\"");
        }
        addLimitPart(msgStart, UMSGPAT_PART_TYPE_MSG_LIMIT, index, 0, nestingLevel);
        return index;
#endif
    }

    int32_t parseArg(int32_t index, int32_t argStartLength, int32_t nestingLevel,
                     UParseError *parseError, UErrorCode &errorCode) {
        if(U_SUCCESS(errorCode)) {
            errorCode=U_UNSUPPORTED_ERROR;
        }
        return 0;
#if 0  // TODO
        int32_t argStart=parts.size();
        UMessagePatternArgType argType=UMSGPAT_ARG_TYPE_NONE;
        addPart(UMSGPAT_PART_TYPE_ARG_START, index, argStartLength, argType.ordinal());
        int32_t nameIndex=index=skipWhiteSpace(index+argStartLength);
        if(index==msg.length()) {
            throw new IllegalArgumentException(
                "Unmatched '{' braces in message \""+prefix()+"\"");
        }
        // parse argument name or number
        index=skipIdentifier(index);
        int32_t number=parseArgNumber(nameIndex, index);
        if(number>=0) {
            int32_t length=index-nameIndex;
            if(length>Part.MAX_LENGTH || number>Part.MAX_VALUE) {
                throw new IndexOutOfBoundsException(
                    "Argument number too large: "+prefix(nameIndex));
            }
            hasArgNumbers=TRUE;
            addPart(UMSGPAT_PART_TYPE_ARG_NUMBER, nameIndex, length, number);
        } else if(number==ARG_NAME_NOT_NUMBER) {
            int32_t length=index-nameIndex;
            if(length>Part.MAX_LENGTH) {
                throw new IndexOutOfBoundsException(
                    "Argument name too long: "+prefix(nameIndex));
            }
            hasArgNames=TRUE;
            addPart(UMSGPAT_PART_TYPE_ARG_NAME, nameIndex, length, 0);
        } else {  // number<-1 (ARG_NAME_NOT_VALID)
            throw new IllegalArgumentException("Bad argument syntax: "+prefix(nameIndex));
        }
        index=skipWhiteSpace(index);
        if(index==msg.length()) {
            throw new IllegalArgumentException(
                "Unmatched '{' braces in message \""+prefix()+"\"");
        }
        char c=msg.charAt(index);
        if(c=='}') {
            // all done
        } else if(c!=',') {
            throw new IllegalArgumentException("Bad argument syntax: "+prefix(nameIndex));
        } else /* ',' */ {
            // parse argument type: case-sensitive a-zA-Z
            int32_t typeIndex=index=skipWhiteSpace(index+1);
            while(index<msg.length() && isArgTypeChar(msg.charAt(index))) {
                ++index;
            }
            int32_t length=index-typeIndex;
            index=skipWhiteSpace(index);
            if(index==msg.length()) {
                throw new IllegalArgumentException(
                    "Unmatched '{' braces in message \""+prefix()+"\"");
            }
            if(length==0 || ((c=msg.charAt(index))!=',' && c!='}')) {
                throw new IllegalArgumentException("Bad argument syntax: "+prefix(nameIndex));
            }
            if(length>Part.MAX_LENGTH) {
                throw new IndexOutOfBoundsException(
                    "Argument type name too long: "+prefix(nameIndex));
            }
            argType=UMSGPAT_ARG_TYPE_SIMPLE;
            if(length==6) {
                // case-insensitive comparisons for complex-type names
                if(isChoice(typeIndex)) {
                    argType=UMSGPAT_ARG_TYPE_CHOICE;
                } else if(isPlural(typeIndex)) {
                    argType=UMSGPAT_ARG_TYPE_PLURAL;
                } else if(isSelect(typeIndex)) {
                    argType=UMSGPAT_ARG_TYPE_SELECT;
                }
            }
            // change the ARG_START type from NONE to argType
            parts.get(argStart).value=(short)argType.ordinal();
            if(argType==UMSGPAT_ARG_TYPE_SIMPLE) {
                addPart(UMSGPAT_PART_TYPE_ARG_TYPE, typeIndex, length, 0);
            }
            // look for an argument style (pattern)
            if(c=='}') {
                if(argType!=UMSGPAT_ARG_TYPE_SIMPLE) {
                    throw new IllegalArgumentException(
                        "No style field for complex argument: "+prefix(nameIndex));
                }
            } else /* ',' */ {
                ++index;
                if(argType==UMSGPAT_ARG_TYPE_SIMPLE) {
                    index=parseSimpleStyle(index);
                } else if(argType==UMSGPAT_ARG_TYPE_CHOICE) {
                    index=parseChoiceStyle(index, nestingLevel);
                } else {
                    index=parsePluralOrSelectStyle(argType, index, nestingLevel);
                }
            }
        }
        // Argument parsing stopped on the '}'.
        addLimitPart(argStart, UMSGPAT_PART_TYPE_ARG_LIMIT, index, 1, argType.ordinal());
        return index+1;
#endif
    }

    int32_t parseSimpleStyle(int32_t index, UParseError *parseError, UErrorCode &errorCode) {
        if(U_SUCCESS(errorCode)) {
            errorCode=U_UNSUPPORTED_ERROR;
        }
        return 0;
#if 0  // TODO
        int32_t start=index;
        int32_t nestedBraces=0;
        while(index<msg.length()) {
            char c=msg.charAt(index++);
            if(c=='\'') {
                // Treat apostrophe as quoting but include it in the style part.
                // Find the end of the quoted literal text.
                index=msg.indexOf('\'', index);
                if(index<0) {
                    throw new IllegalArgumentException(
                        "Quoted literal argument style text reaches to the end of the message: \""+
                        prefix(start)+"\"");
                }
                // skip the quote-ending apostrophe
                ++index;
            } else if(c=='{') {
                ++nestedBraces;
            } else if(c=='}') {
                if(nestedBraces>0) {
                    --nestedBraces;
                } else {
                    int32_t length=--index-start;
                    if(length>Part.MAX_LENGTH) {
                        throw new IndexOutOfBoundsException(
                            "Argument style text too long: "+prefix(start));
                    }
                    addPart(UMSGPAT_PART_TYPE_ARG_STYLE, start, length, 0);
                    return index;
                }
            }  // c is part of literal text
        }
        throw new IllegalArgumentException(
            "Unmatched '{' braces in message \""+prefix()+"\"");
#endif
    }

    int32_t parseChoiceStyle(int32_t index, int32_t nestingLevel,
                             UParseError *parseError, UErrorCode &errorCode) {
        if(U_SUCCESS(errorCode)) {
            errorCode=U_UNSUPPORTED_ERROR;
        }
        return 0;
#if 0  // TODO
        int32_t start=index;
        index=skipWhiteSpace(index);
        if(index==msg.length() || msg.charAt(index)=='}') {
            throw new IllegalArgumentException(
                "Missing choice argument pattern in \""+prefix()+"\"");
        }
        for(;;) {
            // The choice argument style contains |-separated (number, separator, message) triples.
            // Parse the number.
            int32_t numberIndex=index;
            index=skipDouble(index);
            int32_t length=index-numberIndex;
            if(length==0) {
                throw new IllegalArgumentException("Bad choice pattern syntax: "+prefix(start));
            }
            if(length>Part.MAX_LENGTH) {
                throw new IndexOutOfBoundsException(
                    "Choice number too long: "+prefix(numberIndex));
            }
            parseDouble(numberIndex, index, TRUE);  // adds ARG_INT or ARG_DOUBLE
            // Parse the separator.
            index=skipWhiteSpace(index);
            if(index==msg.length()) {
                throw new IllegalArgumentException("Bad choice pattern syntax: "+prefix(start));
            }
            char c=msg.charAt(index);
            if(!(c=='#' || c=='<' || c=='\u2264')) {  // U+2264 is <=
                throw new IllegalArgumentException(
                    "Expected choice separator (#<\u2264) instead of '"+c+
                    "' in choice pattern "+prefix(start));
            }
            addPart(UMSGPAT_PART_TYPE_ARG_SELECTOR, index, 1, 0);
            // Parse the message fragment.
            index=parseMessage(++index, 0, nestingLevel+1, UMSGPAT_ARG_TYPE_CHOICE);
            // parseMessage(..., CHOICE) returns the index of the terminator, or msg.length().
            if(index==msg.length()) {
                return index;
            }
            if(msg.charAt(index)=='}') {
                if(!inMessageFormatPattern(nestingLevel)) {
                    throw new IllegalArgumentException(
                        "Bad choice pattern syntax: "+prefix(start));
                }
                return index;
            }  // else the terminator is '|'
            index=skipWhiteSpace(index+1);
        }
#endif
    }

    int32_t parsePluralOrSelectStyle(UMessagePatternArgType argType, int32_t index, int32_t nestingLevel,
                                     UParseError *parseError, UErrorCode &errorCode) {
        if(U_SUCCESS(errorCode)) {
            errorCode=U_UNSUPPORTED_ERROR;
        }
        return 0;
#if 0  // TODO
        int32_t start=index;
        UBool isEmpty=TRUE;
        UBool hasOther=FALSE;
        for(;;) {
            // First, collect the selector looking for a small set of terminators.
            // It would be a little faster to consider the syntax of each possible
            // token right here, but that makes the code too complicated.
            index=skipWhiteSpace(index);
            UBool eos=index==msg.length();
            if(eos || msg.charAt(index)=='}') {
                if(eos==inMessageFormatPattern(nestingLevel)) {
                    throw new IllegalArgumentException(
                        "Bad "+
                        (argType==UMSGPAT_ARG_TYPE_PLURAL ? "plural" : "select")+
                        " pattern syntax: "+prefix(start));
                }
                if(!hasOther) {
                    throw new IllegalArgumentException(
                        "Missing 'other' keyword in "+
                        (argType==UMSGPAT_ARG_TYPE_PLURAL ? "plural" : "select")+
                        " pattern in \""+prefix()+"\"");
                }
                return index;
            }
            int32_t selectorIndex=index;
            if(argType==UMSGPAT_ARG_TYPE_PLURAL && msg.charAt(selectorIndex)=='=') {
                // explicit-value plural selector: =double
                index=skipDouble(index+1);
                int32_t length=index-selectorIndex;
                if(length==1) {
                    throw new IllegalArgumentException(
                        "Bad "+
                        (argType==UMSGPAT_ARG_TYPE_PLURAL ? "plural" : "select")+
                        " pattern syntax: "+prefix(start));
                }
                if(length>Part.MAX_LENGTH) {
                    throw new IndexOutOfBoundsException(
                        "Argument selector too long: "+prefix(selectorIndex));
                }
                addPart(UMSGPAT_PART_TYPE_ARG_SELECTOR, selectorIndex, length, 0);
                parseDouble(selectorIndex+1, index, FALSE);  // adds ARG_INT or ARG_DOUBLE
            } else {
                index=skipIdentifier(index);
                int32_t length=index-selectorIndex;
                if(length==0) {
                    throw new IllegalArgumentException(
                        "Bad "+
                        (argType==UMSGPAT_ARG_TYPE_PLURAL ? "plural" : "select")+
                        " pattern syntax: "+prefix(start));
                }
                // Note: The ':' in "offset:" is just beyond the skipIdentifier() range.
                if( argType==UMSGPAT_ARG_TYPE_PLURAL && length==6 && index<msg.length() &&
                    msg.regionMatches(selectorIndex, "offset:", 0, 7)
                ) {
                    // plural offset, not a selector
                    if(!isEmpty) {
                        throw new IllegalArgumentException(
                            "Plural argument 'offset:' (if present) must precede key-message pairs: "+
                            prefix(start));
                    }
                    // allow whitespace between offset: and its value
                    int32_t valueIndex=skipWhiteSpace(index+1);  // The ':' is at index.
                    index=skipDouble(valueIndex);
                    if(index==valueIndex) {
                        throw new IllegalArgumentException(
                            "Missing value for plural 'offset:' at "+prefix(start));
                    }
                    if((index-valueIndex)>Part.MAX_LENGTH) {
                        throw new IndexOutOfBoundsException(
                            "Plural offset value too long: "+prefix(valueIndex));
                    }
                    parseDouble(valueIndex, index, FALSE);  // adds ARG_INT or ARG_DOUBLE
                    isEmpty=FALSE;
                    continue;  // no message fragment after the offset
                } else {
                    // normal selector word
                    if(length>Part.MAX_LENGTH) {
                        throw new IndexOutOfBoundsException(
                            "Argument selector too long: "+prefix(selectorIndex));
                    }
                    addPart(UMSGPAT_PART_TYPE_ARG_SELECTOR, selectorIndex, length, 0);
                    if(msg.regionMatches(selectorIndex, "other", 0, length)) {
                        hasOther=TRUE;
                    }
                }
            }

            // parse the message fragment following the selector
            index=skipWhiteSpace(index);
            if(index==msg.length() || msg.charAt(index)!='{') {
                throw new IllegalArgumentException(
                    "No message fragment after "+
                    (argType==UMSGPAT_ARG_TYPE_PLURAL ? "plural" : "select")+
                    " selector: "+prefix(selectorIndex));
            }
            index=parseMessage(index, 1, nestingLevel+1, argType);
            isEmpty=FALSE;
        }
#endif
    }

    /**
     * Validates and parses an argument name or argument number string.
     * This internal method assumes that the input substring is a "pattern identifier".
     * @return &gt;=0 if the name is a valid number,
     *         ARG_NAME_NOT_NUMBER (-1) if it is a "pattern identifier" but not all ASCII digits,
     *         ARG_NAME_NOT_VALID (-2) if it is neither.
     * @see #validateArgumentName(String)
     */
    static int32_t parseArgNumber(const UnicodeString &s, int32_t start, int32_t limit) {
        return UMSGPAT_ARG_NAME_NOT_VALID;
#if 0  // TODO
        // If the identifier contains only ASCII digits, then it is an argument _number_
        // and must not have leading zeros (except "0" itself).
        // Otherwise it is an argument _name_.
        if(start>=limit) {
            return ARG_NAME_NOT_VALID;
        }
        int32_t number;
        // Defer numeric errors until we know there are only digits.
        UBool badNumber;
        char c=s.charAt(start++);
        if(c=='0') {
            if(start==limit) {
                return 0;
            } else {
                number=0;
                badNumber=TRUE;  // leading zero
            }
        } else if('1'<=c && c<='9') {
            number=c-'0';
            badNumber=FALSE;
        } else {
            return ARG_NAME_NOT_NUMBER;
        }
        while(start<limit) {
            c=s.charAt(start++);
            if('0'<=c && c<='9') {
                if(number>=Integer.MAX_VALUE/10) {
                    badNumber=TRUE;  // overflow
                }
                number=number*10+(c-'0');
            } else {
                return ARG_NAME_NOT_NUMBER;
            }
        }
        // There are only ASCII digits.
        if(badNumber) {
            return ARG_NAME_NOT_VALID;
        } else {
            return number;
        }
#endif
    }

    int32_t parseArgNumber(int32_t start, int32_t limit) {
        return parseArgNumber(msg, start, limit);
    }

    /**
     * Parses a number from the specified message substring.
     * @param start start index into the message string
     * @param limit limit index into the message string, must be start<limit
     * @param allowInfinity TRUE if U+221E is allowed (for ChoiceFormat)
     */
    void parseDouble(int32_t start, int32_t limit, UBool allowInfinity,
                     UParseError *parseError, UErrorCode &errorCode) {
        if(U_SUCCESS(errorCode)) {
            errorCode=U_UNSUPPORTED_ERROR;
        }
#if 0  // TODO
        assert start<limit;
        // fake loop for easy exit and single throw statement
        for(;;) {
            // fast path for small integers and infinity
            int32_t value=0;
            int32_t isNegative=0;  // not boolean so that we can easily add it to value
            int32_t index=start;
            char c=msg.charAt(index++);
            if(c=='-') {
                isNegative=1;
                if(index==limit) {
                    break;  // no number
                }
                c=msg.charAt(index++);
            } else if(c=='+') {
                if(index==limit) {
                    break;  // no number
                }
                c=msg.charAt(index++);
            }
            if(c==0x221e) {  // infinity
                if(allowInfinity && index==limit) {
                    addArgDoublePart(
                        isNegative!=0 ? Double.NEGATIVE_INFINITY : Double.POSITIVE_INFINITY,
                        start, limit-start);
                    return;
                } else {
                    break;
                }
            }
            // try to parse the number as a small integer but fall back to a double
            while('0'<=c && c<='9') {
                value=value*10+(c-'0');
                if(value>(Part.MAX_VALUE+isNegative)) {
                    break;  // not a small-enough integer
                }
                if(index==limit) {
                    addPart(UMSGPAT_PART_TYPE_ARG_INT, start, limit-start, isNegative!=0 ? -value : value);
                    return;
                }
                c=msg.charAt(index++);
            }
            // Let Double.parseDouble() throw a NumberFormatException.
            double numericValue=Double.parseDouble(msg.substring(start, limit));
            addArgDoublePart(numericValue, start, limit-start);
            return;
        }
        throw new NumberFormatException(
            "Bad syntax for numeric value: "+msg.substring(start, limit));
#endif
    }

    // Java has package-private appendReducedApostrophes() here.
    // In C++, this is declared in the MessageImpl class.

    int32_t skipWhiteSpace(int32_t index);

    int32_t skipIdentifier(int32_t index);

    /**
     * Skips a sequence of characters that could occur in a double value.
     * Does not fully parse or validate the value.
     */
    int32_t skipDouble(int32_t index);

    static UBool isArgTypeChar(UChar32 c);

    UBool isChoice(int32_t index);

    UBool isPlural(int32_t index);

    UBool isSelect(int32_t index);

    /**
     * @return TRUE if we are inside a MessageFormat (sub-)pattern,
     *         as opposed to inside a top-level choice/plural/select pattern.
     */
    UBool inMessageFormatPattern(int32_t nestingLevel);

    /**
     * @return TRUE if we are in a MessageFormat sub-pattern
     *         of a top-level ChoiceFormat pattern.
     */
    UBool inTopLevelChoiceMessage(int32_t nestingLevel, UMessagePatternArgType parentType);

    void addPart(UMessagePatternPartType type, int32_t index, int32_t length,
                 int32_t value, UErrorCode &errorCode);

    void addLimitPart(int32_t start,
                      UMessagePatternPartType type, int32_t index, int32_t length,
                      int32_t value, UErrorCode &errorCode);

    void addArgDoublePart(double numericValue, int32_t start, int32_t length, UErrorCode &errorCode);

    void setParseError(UParseError *parseError, int32_t index);

    // No ICU "poor man's RTTI" for this class nor its subclasses.
    virtual UClassID getDynamicClassID() const;

    UBool init(UErrorCode &errorCode);
    void copyStorage(const MessagePattern &other, UErrorCode &errorCode);

    UMessagePatternApostropheMode aposMode;
    UnicodeString msg;
    // ArrayList<Part> parts=new ArrayList<Part>();
    MessagePatternPartsList *partsList;
    Part *parts;
    int32_t partsLength;
    // ArrayList<Double> numericValues;
    MessagePatternDoubleList *numericValuesList;
    double *numericValues;
    int32_t numericValuesLength;
    UBool hasArgNames;
    UBool hasArgNumbers;
    UBool needsAutoQuoting;
};

U_NAMESPACE_END

#endif  // __MESSAGEPATTERN_H__
