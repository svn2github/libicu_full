//---------------------------------------------------------------------------------
//
// Generated Header File.  Do not edit by hand.
//    This file contains the state table for the ICU Rule Based Break Iterator
//    rule parser.
//    It is generated by the Perl script "rbbicst.pl" from
//    the rule parser state definitions file "rbbirpt.txt".
//
//   Copyright (C) 2002 International Business Machines Corporation 
//   and others. All rights reserved.  
//
//---------------------------------------------------------------------------------
#ifndef RBBIRPT_H
#define RBBIRPT_H

U_NAMESPACE_BEGIN
//
// Character classes for RBBI rule scanning.
//
    static const uint8_t kRuleSet_digit_char = 128;
    static const uint8_t kRuleSet_white_space = 129;
    static const uint8_t kRuleSet_rule_char = 130;
    static const uint8_t kRuleSet_name_start_char = 131;
    static const uint8_t kRuleSet_name_char = 132;


enum RBBI_RuleParseAction {
    doExprOrOperator,
    doOptionEnd,
    doRuleErrorAssignExpr,
    doTagValue,
    doEndAssign,
    doRuleError,
    doVariableNameExpectedErr,
    doRuleChar,
    doLParen,
    doSlash,
    doStartTagValue,
    doDotAny,
    doExprFinished,
    doScanUnicodeSet,
    doExprRParen,
    doStartVariableName,
    doTagExpectedError,
    doTagDigit,
    doUnaryOpStar,
    doEndVariableName,
    doNOP,
    doUnaryOpQuestion,
    doExit,
    doStartAssign,
    doEndOfRule,
    doUnaryOpPlus,
    doExprStart,
    doOptionStart,
    doExprCatOperator,
    doReverseDir,
    doCheckVarDef,
    rbbiLastAction};

//-------------------------------------------------------------------------------
//
//  RBBIRuleTableEl    represents the structure of a row in the transition table
//                     for the rule parser state machine.
//-------------------------------------------------------------------------------
struct RBBIRuleTableEl {
    RBBI_RuleParseAction          fAction;
    uint8_t                       fCharClass;       // 0-127:    an individual ASCII character
                                                    // 128-255:  character class index
    uint8_t                       fNextState;       // 0-250:    normal next-stat numbers
                                                    // 255:      pop next-state from stack.
    uint8_t                       fPushState;
    UBool                         fNextChar;
};

static const struct RBBIRuleTableEl gRuleParseStateTable[] = {
    {doNOP, 0, 0, 0, TRUE}
    , {doExprStart, 254, 20, 8, FALSE}     //  1      start
    , {doNOP, 129, 1,0,  TRUE}     //  2 
    , {doExprStart, 36 /* $ */, 79, 89, FALSE}     //  3 
    , {doNOP, 33 /* ! */, 11,0,  TRUE}     //  4 
    , {doNOP, 59 /* ; */, 1,0,  TRUE}     //  5 
    , {doNOP, 252, 0,0,  FALSE}     //  6 
    , {doExprStart, 255, 20, 8, FALSE}     //  7 
    , {doEndOfRule, 59 /* ; */, 1,0,  TRUE}     //  8      break-rule-end
    , {doNOP, 129, 8,0,  TRUE}     //  9 
    , {doRuleError, 255, 94,0,  FALSE}     //  10 
    , {doNOP, 33 /* ! */, 13,0,  TRUE}     //  11      rev-option
    , {doReverseDir, 255, 19, 8, FALSE}     //  12 
    , {doOptionStart, 131, 15,0,  TRUE}     //  13      option-scan1
    , {doRuleError, 255, 94,0,  FALSE}     //  14 
    , {doNOP, 132, 15,0,  TRUE}     //  15      option-scan2
    , {doOptionEnd, 129, 1,0,  FALSE}     //  16 
    , {doOptionEnd, 59 /* ; */, 1,0,  FALSE}     //  17 
    , {doRuleError, 255, 94,0,  FALSE}     //  18 
    , {doExprStart, 255, 20, 8, FALSE}     //  19      reverse-rule
    , {doRuleChar, 254, 29,0,  TRUE}     //  20      term
    , {doNOP, 129, 20,0,  TRUE}     //  21 
    , {doRuleChar, 130, 29,0,  TRUE}     //  22 
    , {doNOP, 91 /* [ */, 85, 29, FALSE}     //  23 
    , {doLParen, 40 /* ( */, 20, 29, TRUE}     //  24 
    , {doNOP, 36 /* $ */, 79, 28, FALSE}     //  25 
    , {doDotAny, 46 /* . */, 29,0,  TRUE}     //  26 
    , {doRuleError, 255, 94,0,  FALSE}     //  27 
    , {doCheckVarDef, 255, 29,0,  FALSE}     //  28      term-var-ref
    , {doNOP, 129, 29,0,  TRUE}     //  29      expr-mod
    , {doUnaryOpStar, 42 /* * */, 34,0,  TRUE}     //  30 
    , {doUnaryOpPlus, 43 /* + */, 34,0,  TRUE}     //  31 
    , {doUnaryOpQuestion, 63 /* ? */, 34,0,  TRUE}     //  32 
    , {doNOP, 255, 34,0,  FALSE}     //  33 
    , {doExprCatOperator, 254, 20,0,  FALSE}     //  34      expr-cont
    , {doNOP, 129, 34,0,  TRUE}     //  35 
    , {doExprCatOperator, 130, 20,0,  FALSE}     //  36 
    , {doExprCatOperator, 91 /* [ */, 20,0,  FALSE}     //  37 
    , {doExprCatOperator, 40 /* ( */, 20,0,  FALSE}     //  38 
    , {doExprCatOperator, 36 /* $ */, 20,0,  FALSE}     //  39 
    , {doExprCatOperator, 46 /* . */, 20,0,  FALSE}     //  40 
    , {doExprCatOperator, 47 /* / */, 46,0,  FALSE}     //  41 
    , {doExprCatOperator, 123 /* { */, 58,0,  TRUE}     //  42 
    , {doExprOrOperator, 124 /* | */, 20,0,  TRUE}     //  43 
    , {doExprRParen, 41 /* ) */, 255,0,  TRUE}     //  44 
    , {doExprFinished, 255, 255,0,  FALSE}     //  45 
    , {doSlash, 47 /* / */, 48,0,  TRUE}     //  46      look-ahead
    , {doNOP, 255, 94,0,  FALSE}     //  47 
    , {doExprCatOperator, 254, 20,0,  FALSE}     //  48      expr-cont-no-slash
    , {doNOP, 129, 34,0,  TRUE}     //  49 
    , {doExprCatOperator, 130, 20,0,  FALSE}     //  50 
    , {doExprCatOperator, 91 /* [ */, 20,0,  FALSE}     //  51 
    , {doExprCatOperator, 40 /* ( */, 20,0,  FALSE}     //  52 
    , {doExprCatOperator, 36 /* $ */, 20,0,  FALSE}     //  53 
    , {doExprCatOperator, 46 /* . */, 20,0,  FALSE}     //  54 
    , {doExprOrOperator, 124 /* | */, 20,0,  TRUE}     //  55 
    , {doExprRParen, 41 /* ) */, 255,0,  TRUE}     //  56 
    , {doExprFinished, 255, 255,0,  FALSE}     //  57 
    , {doNOP, 129, 58,0,  TRUE}     //  58      tag-open
    , {doStartTagValue, 128, 61,0,  FALSE}     //  59 
    , {doTagExpectedError, 255, 94,0,  FALSE}     //  60 
    , {doNOP, 129, 65,0,  TRUE}     //  61      tag-value
    , {doNOP, 125 /* } */, 65,0,  FALSE}     //  62 
    , {doTagDigit, 128, 61,0,  TRUE}     //  63 
    , {doTagExpectedError, 255, 94,0,  FALSE}     //  64 
    , {doNOP, 129, 65,0,  TRUE}     //  65      tag-close
    , {doTagValue, 125 /* } */, 68,0,  TRUE}     //  66 
    , {doTagExpectedError, 255, 94,0,  FALSE}     //  67 
    , {doExprCatOperator, 254, 20,0,  FALSE}     //  68      expr-cont-no-tag
    , {doNOP, 129, 68,0,  TRUE}     //  69 
    , {doExprCatOperator, 130, 20,0,  FALSE}     //  70 
    , {doExprCatOperator, 91 /* [ */, 20,0,  FALSE}     //  71 
    , {doExprCatOperator, 40 /* ( */, 20,0,  FALSE}     //  72 
    , {doExprCatOperator, 36 /* $ */, 20,0,  FALSE}     //  73 
    , {doExprCatOperator, 46 /* . */, 20,0,  FALSE}     //  74 
    , {doExprCatOperator, 47 /* / */, 46,0,  FALSE}     //  75 
    , {doExprOrOperator, 124 /* | */, 20,0,  TRUE}     //  76 
    , {doExprRParen, 41 /* ) */, 255,0,  TRUE}     //  77 
    , {doExprFinished, 255, 255,0,  FALSE}     //  78 
    , {doStartVariableName, 36 /* $ */, 81,0,  TRUE}     //  79      scan-var-name
    , {doNOP, 255, 94,0,  FALSE}     //  80 
    , {doNOP, 131, 83,0,  TRUE}     //  81      scan-var-start
    , {doVariableNameExpectedErr, 255, 94,0,  FALSE}     //  82 
    , {doNOP, 132, 83,0,  TRUE}     //  83      scan-var-body
    , {doEndVariableName, 255, 255,0,  FALSE}     //  84 
    , {doScanUnicodeSet, 91 /* [ */, 255,0,  TRUE}     //  85      scan-unicode-set
    , {doScanUnicodeSet, 112 /* p */, 255,0,  TRUE}     //  86 
    , {doScanUnicodeSet, 80 /* P */, 255,0,  TRUE}     //  87 
    , {doNOP, 255, 94,0,  FALSE}     //  88 
    , {doNOP, 129, 89,0,  TRUE}     //  89      assign-or-rule
    , {doStartAssign, 61 /* = */, 20, 92, TRUE}     //  90 
    , {doNOP, 255, 28, 8, FALSE}     //  91 
    , {doEndAssign, 59 /* ; */, 1,0,  TRUE}     //  92      assign-end
    , {doRuleErrorAssignExpr, 255, 94,0,  FALSE}     //  93 
    , {doExit, 255, 94,0,  TRUE}     //  94      errorDeath
 };
static const char * const RBBIRuleStateNames[] = {    0,
     "start",
    0,
    0,
    0,
    0,
    0,
    0,
     "break-rule-end",
    0,
    0,
     "rev-option",
    0,
     "option-scan1",
    0,
     "option-scan2",
    0,
    0,
    0,
     "reverse-rule",
     "term",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
     "term-var-ref",
     "expr-mod",
    0,
    0,
    0,
    0,
     "expr-cont",
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
     "look-ahead",
    0,
     "expr-cont-no-slash",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
     "tag-open",
    0,
    0,
     "tag-value",
    0,
    0,
    0,
     "tag-close",
    0,
    0,
     "expr-cont-no-tag",
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
     "scan-var-name",
    0,
     "scan-var-start",
    0,
     "scan-var-body",
    0,
     "scan-unicode-set",
    0,
    0,
    0,
     "assign-or-rule",
    0,
    0,
     "assign-end",
    0,
     "errorDeath",
    0};

U_NAMESPACE_END
#endif
