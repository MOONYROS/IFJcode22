//
//  support.h
//  IFJ-prekladac
//
//  Created by Ondrej Lukasek on 15.10.2022.
//

#include "token.h"

char *tokenName[tMaxToken] = {
    "tNone",
    "tTypeInt", "tTypeFloat", "tTypeString", "tNullTypeInt", "tNullTypeFloat", "tNullTypeString", "tVoid",
    "tIf", "tElse", "tWhile", "tFunction", "tReturn", "tNull",
    "tInvalid", "tIdentifier", "tFuncName", "tType", "tNullType",
    "tPlus", "tMinus", "tConcat","tMul", "tDiv", "tLPar", "tRPar", "tLCurl", "tRCurl", "tColon", "tSemicolon", "tComma",
    "tAssign", "tIdentical",
    "tExclamation", "tNotIdentical",
    "tLess", "tLessEq", "tMore", "tMoreEq",
    "tInt", "tReal", "tReal2", "tInt2",
    "tLiteral", "tEpilog"
};

int typeIsCompatible(tTokenType dst, tTokenType src)
{
    if (dst == src)
        return 1;

    switch (dst)
    {
    case tNullTypeInt:
        if (src == tTypeInt || src == tNullType)
            return 1;
        break;
    case tNullTypeFloat:
        if (src == tTypeFloat || src == tNullType)
            return 1;
        break;
    case tNullTypeString:
        if (src == tTypeString || src == tNullType)
            return 1;
        break;
    default:
        break;
    }
    return 0;
}