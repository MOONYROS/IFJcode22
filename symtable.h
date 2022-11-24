//
//  lex.h
//  IFJ-prekladac
//
//  Created by Ondrej Lukasek on 15.10.2022.
//

#ifndef symtable_h
#define symtable_h

#include <stdbool.h>

#define ST_SIZE 509


/* typedef enum {
    DTUnknown, DTInt, DTFloat, DTString, DTNullInt, DTNullFloat, DTNullString, DTVoid
} tDataType; */


typedef struct FuncParam {
    char* name; 
    tTokenType dataType;
    struct FuncParam* next;
} tFuncParam;

typedef struct SymTable tSymTable;

typedef struct SymTableItem {
    char name[255];             // name of variable or function
    tTokenType dataType;         // data type variable or function
    int isDefined;              // ??? je to potreba
    int isFunction;             // 1 (true) if name is function
    int returned;               // function only: info if the function has correctly returned
    struct SymTable* localST;   // function only: pointer to local symbol table for funcion
    tFuncParam* params;         // function only : list of function paramaters
    struct SymTableItem* next;  // hash table items with the same hash
} tSymTableItem;

struct SymTable{
    tSymTableItem* items[ST_SIZE];
};

int get_hash(char *key);
void st_init(tSymTable *table);
tSymTableItem* st_search(tSymTable *table, char *key);
tSymTableItem* st_insert(tSymTable *table, char *key);
void st_delete(tSymTable*table, char *key);
void st_delete_all(tSymTable* table);
void st_print(tSymTable* table);

#endif /* symtable_h */
