//
//  token.h
//  IFJ-prekladac
//
//  Created by Ondrej Lukasek on 15.10.2022.
//

#ifndef token_h
#define token_h

typedef enum{
    tInvalid,
    tPlus, tMinus, tMul, tDiv
} tTokenType;

typedef struct{
    tTokenType type;
    char data[1024];
} tToken;

#endif /* token_h */
