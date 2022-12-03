//
//  expression.h
//  IFJ-prekladac
//
//  Created by Ondrej Lukasek on 15.10.2022.
//

#ifndef expression_h
#define expression_h

#include <stdio.h>
#include <stdbool.h>
#include "token.h"
#include "tstack.h"
#include "symtable.h"

// function to evaluate expression
// returns type of and exp expression
// generates code to ??? tohle musime upresnit, jak budeme delat
// in case of semantic failure calls errorExit(msg, errno)
tTokenType evalExp(char* tgtVar, tStack* exp, tSymTable* st);
tTokenType const2type(tTokenType ctype);

#endif /* expression_h */
