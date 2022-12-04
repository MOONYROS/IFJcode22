//
//  support.h
//  IFJ-prekladac
//
//  Created by Ondrej Lukasek on 15.10.2022.
//

#include <stdio.h>

#ifndef support_h
#define support_h

#define DEBUG_MSG       1

#define CERR_OK         0
#define CERR_LEX        1
#define CERR_SYNTAX     2
#define CERR_SEM_FUNC   3
#define CERR_SEM_ARG    4
#define CERR_SEM_UNDEF  5
#define CERR_SEM_RET    6
#define CERR_SEM_TYPE   7
#define CERR_SEM_OTHER  8
#define CERR_INTERNAL   99

extern int srcLine;
extern int prgPass;

typedef struct AllocatedMemory {
    void* ptr;
    struct AllocatedMemory* next;
} tAllocatedMemory;

void *safe_malloc(size_t size);
void safe_free(void* ptr);
void safe_free_all();
void errorExit(char* msg, int errCode);
int dbgMsg(const char* fmt, ...);
int dbgMsg2(const char* fmt, ...);

#endif /* support_h */
