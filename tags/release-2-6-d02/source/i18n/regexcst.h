//---------------------------------------------------------------------------------
//
// Generated Header File.  Do not edit by hand.
//    This file contains the state table for the ICU Regular Expression Pattern Parser
//    It is generated by the Perl script "regexcst.pl" from
//    the rule parser state definitions file "regexcst.txt".
//
//   Copyright (C) 2002 International Business Machines Corporation 
//   and others. All rights reserved.  
//
//---------------------------------------------------------------------------------
#ifndef RBBIRPT_H
#define RBBIRPT_H

U_NAMESPACE_BEGIN
//
// Character classes for regex pattern scanning.
//
    static const uint8_t kRuleSet_digit_char = 128;
    static const uint8_t kRuleSet_white_space = 129;
    static const uint8_t kRuleSet_rule_char = 130;


enum Regex_PatternParseAction {
    doPossessivePlus,
    doCloseParen,
    doProperty,
    doBeginMatchMode,
    doOrOperator,
    doOpenCaptureParen,
    doBadOpenParenType,
    doRuleError,
    doIntevalLowerDigit,
    doBackslashs,
    doOctal,
    doNGOpt,
    doBackslashw,
    doMismatchedParenErr,
    doOpenLookBehind,
    doBackslashz,
    doIntervalError,
    doStar,
    doCaret,
    doEnterQuoteMode,
    doNGStar,
    doMatchMode,
    doIntervalUpperDigit,
    doOpenLookAheadNeg,
    doPlus,
    doOpenNonCaptureParen,
    doBackslashA,
    doBackslashB,
    doNGPlus,
    doSetMatchMode,
    doPatFinish,
    doBackslashD,
    doPossessiveInterval,
    doEscapeError,
    doBackslashG,
    doSuppressComments,
    doMatchModeParen,
    doOpt,
    doInterval,
    doLiteralChar,
    doIntervalInit,
    doOpenAtomicParen,
    doBackslashS,
    doOpenLookAhead,
    doBackRef,
    doDollar,
    doDotAny,
    doBackslashW,
    doBackslashX,
    doScanUnicodeSet,
    doBackslashZ,
    doPerlInline,
    doPossessiveOpt,
    doNOP,
    doConditionalExpr,
    doExit,
    doNGInterval,
    doPatStart,
    doBackslashb,
    doPossessiveStar,
    doBackslashd,
    doIntervalSame,
    doOpenLookBehindNeg,
    rbbiLastAction};

//-------------------------------------------------------------------------------
//
//  RegexTableEl       represents the structure of a row in the transition table
//                     for the pattern parser state machine.
//-------------------------------------------------------------------------------
struct RegexTableEl {
    Regex_PatternParseAction      fAction;
    uint8_t                       fCharClass;       // 0-127:    an individual ASCII character
                                                    // 128-255:  character class index
    uint8_t                       fNextState;       // 0-250:    normal next-state numbers
                                                    // 255:      pop next-state from stack.
    uint8_t                       fPushState;
    UBool                         fNextChar;
};

static const struct RegexTableEl gRuleParseStateTable[] = {
    {doNOP, 0, 0, 0, TRUE}
    , {doPatStart, 255, 2,0,  FALSE}     //  1      start
    , {doLiteralChar, 254, 14,0,  TRUE}     //  2      term
    , {doLiteralChar, 130, 14,0,  TRUE}     //  3 
    , {doScanUnicodeSet, 91 /* [ */, 14,0,  TRUE}     //  4 
    , {doNOP, 40 /* ( */, 27,0,  TRUE}     //  5 
    , {doDotAny, 46 /* . */, 14,0,  TRUE}     //  6 
    , {doCaret, 94 /* ^ */, 2,0,  TRUE}     //  7 
    , {doDollar, 36 /* $ */, 2,0,  TRUE}     //  8 
    , {doNOP, 92 /* \ */, 79,0,  TRUE}     //  9 
    , {doOrOperator, 124 /* | */, 2,0,  TRUE}     //  10 
    , {doCloseParen, 41 /* ) */, 255,0,  TRUE}     //  11 
    , {doPatFinish, 253, 2,0,  FALSE}     //  12 
    , {doRuleError, 255, 100,0,  FALSE}     //  13 
    , {doNOP, 42 /* * */, 57,0,  TRUE}     //  14      expr-quant
    , {doNOP, 43 /* + */, 60,0,  TRUE}     //  15 
    , {doNOP, 63 /* ? */, 63,0,  TRUE}     //  16 
    , {doIntervalInit, 123 /* { */, 66,0,  TRUE}     //  17 
    , {doNOP, 40 /* ( */, 23,0,  TRUE}     //  18 
    , {doNOP, 255, 20,0,  FALSE}     //  19 
    , {doOrOperator, 124 /* | */, 2,0,  TRUE}     //  20      expr-cont
    , {doCloseParen, 41 /* ) */, 255,0,  TRUE}     //  21 
    , {doNOP, 255, 2,0,  FALSE}     //  22 
    , {doSuppressComments, 63 /* ? */, 25,0,  TRUE}     //  23      open-paren-quant
    , {doNOP, 255, 27,0,  FALSE}     //  24 
    , {doNOP, 35 /* # */, 46, 14, TRUE}     //  25      open-paren-quant2
    , {doNOP, 255, 29,0,  FALSE}     //  26 
    , {doSuppressComments, 63 /* ? */, 29,0,  TRUE}     //  27      open-paren
    , {doOpenCaptureParen, 255, 2, 14, FALSE}     //  28 
    , {doOpenNonCaptureParen, 58 /* : */, 2, 14, TRUE}     //  29      open-paren-extended
    , {doOpenAtomicParen, 62 /* > */, 2, 14, TRUE}     //  30 
    , {doOpenLookAhead, 61 /* = */, 2, 20, TRUE}     //  31 
    , {doOpenLookAheadNeg, 33 /* ! */, 2, 20, TRUE}     //  32 
    , {doNOP, 60 /* < */, 43,0,  TRUE}     //  33 
    , {doNOP, 35 /* # */, 46, 2, TRUE}     //  34 
    , {doBeginMatchMode, 105 /* i */, 49,0,  FALSE}     //  35 
    , {doBeginMatchMode, 109 /* m */, 49,0,  FALSE}     //  36 
    , {doBeginMatchMode, 115 /* s */, 49,0,  FALSE}     //  37 
    , {doBeginMatchMode, 120 /* x */, 49,0,  FALSE}     //  38 
    , {doBeginMatchMode, 45 /* - */, 49,0,  FALSE}     //  39 
    , {doConditionalExpr, 40 /* ( */, 100,0,  TRUE}     //  40 
    , {doPerlInline, 123 /* { */, 100,0,  TRUE}     //  41 
    , {doBadOpenParenType, 255, 100,0,  FALSE}     //  42 
    , {doOpenLookBehind, 61 /* = */, 2, 20, TRUE}     //  43      open-paren-lookbehind
    , {doOpenLookBehindNeg, 33 /* ! */, 2, 20, TRUE}     //  44 
    , {doBadOpenParenType, 255, 100,0,  FALSE}     //  45 
    , {doNOP, 41 /* ) */, 255,0,  TRUE}     //  46      paren-comment
    , {doMismatchedParenErr, 253, 100,0,  FALSE}     //  47 
    , {doNOP, 255, 46,0,  TRUE}     //  48 
    , {doMatchMode, 105 /* i */, 49,0,  TRUE}     //  49      paren-flag
    , {doMatchMode, 109 /* m */, 49,0,  TRUE}     //  50 
    , {doMatchMode, 115 /* s */, 49,0,  TRUE}     //  51 
    , {doMatchMode, 120 /* x */, 49,0,  TRUE}     //  52 
    , {doMatchMode, 45 /* - */, 49,0,  TRUE}     //  53 
    , {doSetMatchMode, 41 /* ) */, 2,0,  TRUE}     //  54 
    , {doMatchModeParen, 58 /* : */, 2, 14, TRUE}     //  55 
    , {doNOP, 255, 100,0,  FALSE}     //  56 
    , {doNGStar, 63 /* ? */, 20,0,  TRUE}     //  57      quant-star
    , {doPossessiveStar, 43 /* + */, 20,0,  TRUE}     //  58 
    , {doStar, 255, 20,0,  FALSE}     //  59 
    , {doNGPlus, 63 /* ? */, 20,0,  TRUE}     //  60      quant-plus
    , {doPossessivePlus, 43 /* + */, 20,0,  TRUE}     //  61 
    , {doPlus, 255, 20,0,  FALSE}     //  62 
    , {doNGOpt, 63 /* ? */, 20,0,  TRUE}     //  63      quant-opt
    , {doPossessiveOpt, 43 /* + */, 20,0,  TRUE}     //  64 
    , {doOpt, 255, 20,0,  FALSE}     //  65 
    , {doNOP, 129, 66,0,  TRUE}     //  66      interval-open
    , {doNOP, 128, 69,0,  FALSE}     //  67 
    , {doIntervalError, 255, 100,0,  FALSE}     //  68 
    , {doIntevalLowerDigit, 128, 69,0,  TRUE}     //  69      interval-lower
    , {doNOP, 44 /* , */, 73,0,  TRUE}     //  70 
    , {doIntervalSame, 125 /* } */, 76,0,  TRUE}     //  71 
    , {doIntervalError, 255, 100,0,  FALSE}     //  72 
    , {doIntervalUpperDigit, 128, 73,0,  TRUE}     //  73      interval-upper
    , {doNOP, 125 /* } */, 76,0,  TRUE}     //  74 
    , {doIntervalError, 255, 100,0,  FALSE}     //  75 
    , {doNGInterval, 63 /* ? */, 20,0,  TRUE}     //  76      interval-type
    , {doPossessiveInterval, 43 /* + */, 20,0,  TRUE}     //  77 
    , {doInterval, 255, 20,0,  FALSE}     //  78 
    , {doBackslashA, 65 /* A */, 2,0,  TRUE}     //  79      backslash
    , {doBackslashB, 66 /* B */, 2,0,  TRUE}     //  80 
    , {doBackslashb, 98 /* b */, 2,0,  TRUE}     //  81 
    , {doBackslashd, 100 /* d */, 14,0,  TRUE}     //  82 
    , {doBackslashD, 68 /* D */, 14,0,  TRUE}     //  83 
    , {doBackslashG, 71 /* G */, 2,0,  TRUE}     //  84 
    , {doProperty, 78 /* N */, 14,0,  FALSE}     //  85 
    , {doProperty, 112 /* p */, 14,0,  FALSE}     //  86 
    , {doProperty, 80 /* P */, 14,0,  FALSE}     //  87 
    , {doEnterQuoteMode, 81 /* Q */, 2,0,  TRUE}     //  88 
    , {doBackslashS, 83 /* S */, 14,0,  TRUE}     //  89 
    , {doBackslashs, 115 /* s */, 14,0,  TRUE}     //  90 
    , {doBackslashW, 87 /* W */, 14,0,  TRUE}     //  91 
    , {doBackslashw, 119 /* w */, 14,0,  TRUE}     //  92 
    , {doBackslashX, 88 /* X */, 14,0,  TRUE}     //  93 
    , {doBackslashZ, 90 /* Z */, 2,0,  TRUE}     //  94 
    , {doBackslashz, 122 /* z */, 2,0,  TRUE}     //  95 
    , {doOctal, 48 /* 0 */, 14,0,  TRUE}     //  96 
    , {doBackRef, 128, 14,0,  TRUE}     //  97 
    , {doEscapeError, 253, 100,0,  FALSE}     //  98 
    , {doLiteralChar, 255, 14,0,  TRUE}     //  99 
    , {doExit, 255, 100,0,  TRUE}     //  100      errorDeath
 };
static const char * const RegexStateNames[] = {    0,
     "start",
     "term",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
     "expr-quant",
    0,
    0,
    0,
    0,
    0,
     "expr-cont",
    0,
    0,
     "open-paren-quant",
    0,
     "open-paren-quant2",
    0,
     "open-paren",
    0,
     "open-paren-extended",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
     "open-paren-lookbehind",
    0,
    0,
     "paren-comment",
    0,
    0,
     "paren-flag",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
     "quant-star",
    0,
    0,
     "quant-plus",
    0,
    0,
     "quant-opt",
    0,
    0,
     "interval-open",
    0,
    0,
     "interval-lower",
    0,
    0,
    0,
     "interval-upper",
    0,
    0,
     "interval-type",
    0,
    0,
     "backslash",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
     "errorDeath",
    0};

U_NAMESPACE_END
#endif
