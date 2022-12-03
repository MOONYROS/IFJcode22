//
//  parser.c
//  IFJ-prekladac
//
//  Created by Ondrej Lukasek on 02.11.2022.
//
#include <string.h>
#include <stdlib.h>

#include "support.h"
#include "token.h"
#include "lex.h"
#include "parser.h"
#include "tstack.h"
#include "symtable.h"
#include "expression.h"
#include "generator.h"

extern char* tokenName[tMaxToken];
extern const char tmpExpResultName[];
extern const char funcPrefixName[];
extern const char funcRetValName[];
extern tToken token;
extern FILE* inf;
extern int srcLine;

int prgPass = 1; // syntax/semantic pass number
int level = 0; // nesting level of syntax/semantic analysis
tSymTable gst; // global symbol table
tToken assignId; // latest identifier that can be assigned to
tSymTableItem* actFunc = NULL; // active function if processing function definition body
int funcVarCnt = 1;
int condCnt = 1;

typedef enum {
    isEmpty, notEmpty
} tStackState;

void insert_embedded_functions(tSymTable* st)
{
    if( (st_insert_function(st, "reads", tNullTypeString) == 0) ||
        (st_insert_function(st, "readi", tNullTypeInt) == 0) ||
        (st_insert_function(st, "readf", tNullTypeFloat) == 0) ||
        (st_insert_function(st, "write", tVoid) == 0) ||
        (st_insert_function(st, "strlen", tTypeInt) == 0) ||
        (st_insert_function(st, "substring", tNullTypeString) == 0) ||
        (st_insert_function(st, "ord", tTypeInt) == 0) ||
        (st_insert_function(st, "chr", tTypeString) == 0) )
        errorExit("cannot insert embedded functions to symbol table", CERR_INTERNAL);
    if( st_add_params(st, "strlen", tTypeString, "s") == 0 )
        errorExit("cannot insert function parameters to function in symbol table", CERR_INTERNAL);
    if( (st_add_params(st, "substring", tTypeString, "s") == 0) ||
        (st_add_params(st, "substring", tTypeInt, "i") == 0) ||
        (st_add_params(st, "substring", tTypeInt, "j") == 0) )
        errorExit("cannot insert function parameters to function in symbol table", CERR_INTERNAL);
    if (st_add_params(st, "ord", tTypeString, "c") == 0)
        errorExit("cannot insert function parameters to function in symbol table", CERR_INTERNAL);
    if (st_add_params(st, "chr", tTypeInt, "i") == 0)
        errorExit("cannot insert function parameters to function in symbol table", CERR_INTERNAL);
}

void on_stack_state_error(tStack* stack, tStackState cond, char* msg, int errCode)
{
    switch (cond)
    {
    case isEmpty:
        if (tstack_isEmpty(stack))
            errorExit(msg, errCode);
        break;
    case notEmpty:
        if (!tstack_isEmpty(stack))
            errorExit(msg, errCode);
        break;
    default:
        errorExit("unknown stack condition", CERR_INTERNAL);
        break;
    }
}

int ReadTokenPRINT(FILE *f, tToken *token)
{
    int ret;
    ret = ReadToken(f, token);
#if DEBUG_MSG == 11
    if(token->type == tInvalid)
        dbgMsg("INVALID: <<%s>>\n",token->data);
    else
        dbgMsg("TOKEN %s: <<%s>>\n", tokenName[token->type], token->data);
#endif
    return ret;
}

tTokenType strToToken(const char *tokenStr)
{
    for (int t = tTypeInt; t <= tEpilog; t++) {
        if (strcmp(tokenStr, tokenName[t]) == 0)
            return t;
    }
    return tInvalid;
}

void prl(char* str)
{
    level++;
    return; // jen aby nebyl warning, ze se str nic nedelame, kdy mame zakomentovany nasleduji radek
    dbgMsg("%*s%s\n", level*2, "", str);
}

void nextToken()
{
    ReadTokenPRINT(inf, &token);
    if (token.type == tInvalid)
    {
        char tmpStr[255];
        sprintf(tmpStr, "invalid token '%s'", token.data);
        errorExit(tmpStr, CERR_LEX);
    }
}

void matchTokenAndNext(tTokenType tokType)
{
    if (token.type != tokType)
    {
        char tmpStr[255];
        sprintf(tmpStr, "unexpected token '%s'", token.data);
        errorExit(tmpStr, CERR_SYNTAX);
    }
    nextToken();
}

void processFunctionDefinition()
{
    // tFunction tFuncName tLPar arguments tRPar tColon type tLCurl statements tRCurl

    matchTokenAndNext(tFunction);
    tStack* tmpStack = tstack_init();
    tSymTableItem* fce = NULL;
    tToken tmpToken = { 0,0 };
    tmpToken.data = safe_malloc(MAX_TOKEN_LEN);
    //char tmpStr[255];

    if (prgPass == 1)
    {
        //dbgMsg(">>>>>>>> GST v program - tFunction >>>>>\n");
        //st_print(&gst);
        dbgMsg("function declaration %s ( ", token.data);
        fce = st_insert(&gst, token.data);
        if (fce == NULL)
        {
            errorExit("function already defined", CERR_SEM_FUNC);
            return; // prevention of IntelliSense C6011
        }
        fce->isFunction = 1;
    }
    else
    {
        dbgMsg("function definition %s ( ", token.data);
        fce = st_search(&gst, token.data);
        if (fce == NULL)
        {
            errorExit("function not found in symbol table", CERR_INTERNAL);
            return; // prevention of IntelliSense C6011
        }
    }
    actFunc = fce;

    addCode("LABEL %s%s", funcPrefixName, token.data);
    addCode("PUSHFRAME");
    addCode("CREATEFRAME");
    matchTokenAndNext(tFuncName);
    matchTokenAndNext(tLPar);

    // parse function arguments
    parse_arguments(tmpStack);
    // dbgMsg("- arguments:");
    // tstack_print(tmpStack);
    if (prgPass == 1)
    {
        // on 1st pass fill the symbol table with params
        fce->localST = safe_malloc(sizeof(tSymTable));
        st_init(fce->localST);
        while (!tstack_isEmpty(tmpStack))
        {
            // add parameters to function in symbol table
            if (!tstack_pop(tmpStack, &tmpToken)) // pop type definition
                errorExit("function definition parameter error type", CERR_INTERNAL);
            tTokenType parType = tmpToken.type;
            dbgMsg("%s ", tokenName[parType]);
            if (!tstack_pop(tmpStack, &tmpToken)) // pop var identifier
                errorExit("function definition parameter error identifier", CERR_INTERNAL);
            dbgMsg("%s ", tmpToken.data);
            st_add_params(&gst, fce->name, parType, tmpToken.data);
            if (tstack_peek(tmpStack) != NULL)
            {
                if (tstack_peek(tmpStack)->type == tComma) // pop possible comma for next parameter
                {
                    if (!tstack_pop(tmpStack, &tmpToken))
                        errorExit("function definition parameter error comma", CERR_INTERNAL);
                }
            }
        }
    }
    else
    {
        // on 2nd pass define function local variables to function code
        addCodeVariableDefs(fce->localST);
        // and fill them with call values
        int par = 1;
        while (!tstack_isEmpty(tmpStack))
        {
            tstack_pop(tmpStack, &tmpToken); // pop type definition - not checking results as the 1st pass passed ok
            tstack_pop(tmpStack, &tmpToken); // pop var identifier
            //addCode("DEFVAR LF@%s # funcdef", tmpToken.data);
            addCode("MOVE LF@%s LF@%%%d", tmpToken.data, par);
            par++;
            if (tstack_peek(tmpStack) != NULL)
            {
                if (tstack_peek(tmpStack)->type == tComma) // pop possible comma for next parameter
                {
                    if (!tstack_pop(tmpStack, &tmpToken))
                        errorExit("function definition parameter error comma", CERR_INTERNAL);
                }
            }
        }
    }
    on_stack_state_error(tmpStack, notEmpty, "stack should be empty after processsing arguments", CERR_INTERNAL);

    matchTokenAndNext(tRPar);
    matchTokenAndNext(tColon);

    // parse function return type
    parse_type(tmpStack);
    if (prgPass == 1)
    {
        dbgMsg(") : ");
        // tstack_print(tmpStack);
        if (!tstack_pop(tmpStack, &tmpToken))
            errorExit("function definition return type error", CERR_INTERNAL);
        fce->dataType = tmpToken.type;
        if (!tstack_isEmpty(tmpStack))
            errorExit("stack should be empty after processsing function type", CERR_INTERNAL);
        dbgMsg("%s\n", tokenName[fce->dataType]);
    }
    else
    {
        tstack_deleteItems(tmpStack);
        if (fce->dataType != tVoid)
            addCode("DEFVAR LF@%s", funcRetValName);
        dbgMsg(") : ");
        dbgMsg("%s {\n", tokenName[fce->dataType]);
    }
    // mame uvod funkce nadefinovany
    addCode("");

    matchTokenAndNext(tLCurl);
    // parse function statements
    if (prgPass == 1)
        parse_statements(fce->localST, NULL); // do not care about function body statements on the 1st pass
    else
        parse_statements(fce->localST, NULL);
    // dbgMsg("Function local symbol table:\n");
    // st_print(fce->localST);
    // free(fce->localST); neuvolnovat, jeste ji budou potrebovat dalsi
    matchTokenAndNext(tRCurl);

    if (prgPass == 2)
    {
        fce->isDefined = 1;
        if ((fce->dataType != tVoid) && (fce->hasReturn == 0))
            errorExit("missing return statement in non void function", CERR_SEM_RET);
        dbgMsg("} end function definition\n");
        addCode("POPFRAME");
        addCode("RETURN");
        addCode("");
    }

    actFunc = NULL;
    free(tmpToken.data);
    tstack_free(&tmpStack);
}

void processIfStatement(tSymTable* st)
{
    // tIf tLPar expression tRPar tLCurl statements tRCurl tElse tLCurl statements tRCurl

    tStack* tmpStack = tstack_init();
    tTokenType expType = tNone;
    char* funcName = safe_malloc(MAX_TOKEN_LEN);
    char* tmpStr = safe_malloc(MAX_TOKEN_LEN);
    int condNr = condCnt;
    if (prgPass == 2)
        condCnt++;

    dbgMsg2("IF ");
    matchTokenAndNext(tIf);
    matchTokenAndNext(tLPar);
    parse_expression(st, tmpStack);
    if (prgPass == 1)
    {
        tstack_deleteItems(tmpStack);
    }
    else
    {
        dbgMsg2(" ( ");
        sprintf(tmpStr, "TF@%%%%condRes%05d", condNr);
        addCode("DEFVAR TF@%%condRes%05d", condNr);
        expType = evalExp(tmpStr, tmpStack, st);
        dbgMsg2(" ) ");
        addCode("JUMPIFEQ $IFelse%05d TF@%%condRes%05d bool@false", condNr, condNr);
    }
    on_stack_state_error(tmpStack, notEmpty, "stack should be empty after processsing assignment expression", CERR_INTERNAL);
    matchTokenAndNext(tRPar);
    matchTokenAndNext(tLCurl);
    addCode("LABEL $IFtrue%05d", condNr);
    dbgMsg2(" {\n");
    parse_statements(st, NULL);
    //st_delete_scope(st, condNr);
    dbgMsg2("} ");
    addCode("JUMP $IFend%05d", condNr);
    matchTokenAndNext(tRCurl);
    dbgMsg2("ELSE ");
    matchTokenAndNext(tElse);
    matchTokenAndNext(tLCurl);
    addCode("LABEL $IFelse%05d", condNr);
    dbgMsg2("{\n");
    parse_statements(st, NULL);
    //st_delete_scope(st, condNr);
    dbgMsg2("} END IF\n");
    matchTokenAndNext(tRCurl);
    addCode("LABEL $IFend%05d", condNr);
    addCode("");

    safe_free(tmpStr);
    safe_free(funcName);
    tstack_free(&tmpStack);
}

void processWhileStatement(tSymTable* st)
{
    // tWhile tLPar expression tRPar tLCurl statements tRCurl

    tStack* tmpStack = tstack_init();
    tTokenType expType = tNone;
    char* funcName = safe_malloc(MAX_TOKEN_LEN);
    char* tmpStr = safe_malloc(MAX_TOKEN_LEN);
    int condNr = condCnt;
    if (prgPass == 2)
        condCnt++;

    dbgMsg2("WHILE ");
    matchTokenAndNext(tWhile);
    matchTokenAndNext(tLPar);
    parse_expression(st, tmpStack);
    if (prgPass == 1)
    {
        tstack_deleteItems(tmpStack);
    }
    else
    {
        addCode("LABEL $WHILEstart%05d", condNr);
        addCode("CREATEFRAME");
        addCode("DEFVAR TF@%%condRes%05d", condNr);
        dbgMsg2(" ( ");
        sprintf(tmpStr, "TF@%%%%condRes%05d", condNr);
        expType = evalExp(tmpStr, tmpStack, st);
        dbgMsg2(" ) ");
        addCode("JUMPIFEQ $WHILEend%05d TF@%%condRes%05d bool@false", condNr, condNr);
    }
    on_stack_state_error(tmpStack, notEmpty, "stack should be empty after processsing assignment expression", CERR_INTERNAL);
    matchTokenAndNext(tRPar);
    matchTokenAndNext(tLCurl);
    dbgMsg2(" {\n");
    parse_statements(st, NULL);
    addCode("JUMP $WHILEstart%05d", condNr);
    dbgMsg2("} end WHILE\n");
    matchTokenAndNext(tRCurl);
    addCode("LABEL $WHILEend%05d", condNr);
    addCode("");

    safe_free(tmpStr);
    free(funcName);
    tstack_free(&tmpStack);
}

void processFunctionCall(tSymTable* st, tStack* stack)
{
    // tFuncName tLPar parameters tRPar

    tStack* tmpStack = tstack_init();
    tStack* expStack = tstack_init();
    tTokenType expType = tNone;
    char* funcName = safe_malloc(MAX_TOKEN_LEN);
    tToken tmpToken = { 0,0 };
    tmpToken.data = safe_malloc(MAX_TOKEN_LEN);

    strcpy(funcName, token.data);

    int funcNr = funcVarCnt;
    if (strcmp(funcName, "write") != 0)
        funcVarCnt++;

    //ulozime funkcni promennou misto funkce - aaa tohle je hodne temp a vubec to neni poradne zkontrolovany dal
    tmpToken.type = tIdentifier;
    tmpToken.data = safe_malloc(MAX_TOKEN_LEN);
    sprintf(tmpToken.data, "%s%s%05d", funcPrefixName, funcName, funcNr);
    tstack_pushl(stack, tmpToken);

    matchTokenAndNext(tFuncName);
    matchTokenAndNext(tLPar);
    parse_parameters(st, tmpStack);

    if (prgPass == 1)
    {
        // pri prvnim pruchodu nas parametry vubec nezajimaji
        tstack_deleteItems(tmpStack);
        if (strcmp(funcName, "write") != 0)
        {
            char tmpStr[255];
            tSymTableItem* sti = st_search(&gst, funcName);
            sprintf(tmpStr, "%s%s%05d", funcPrefixName, funcName, funcNr);
            tSymTableItem* stf = st_insert(st, tmpStr);
            if (stf == NULL)
            {
                errorExit("cannot insert function return variable to symbol table", CERR_INTERNAL);
                return;
            }
            if (sti != NULL)    // fce uz je deklarovana, tak dame jeji promenne i typ
                stf->dataType = sti->dataType;
        }
    }
    else
    {
        // najdeme funkci v symbol table
        tSymTableItem* sti = st_search(&gst, funcName);
        if (sti == NULL)
        {
            errorExit("function not defined", CERR_SEM_FUNC);
            return;
        }
        dbgMsg("function call %s (", funcName);
        if (strcmp(sti->name, "write") == 0)
        {
            // specialni sekce pro funkci write, ktera ma libovolny pocet parametru
            while (!tstack_isEmpty(tmpStack))
            {
                // aaa write asi taky bude muset umet zpracova expression :(
                if (!tstack_pop(tmpStack, &tmpToken))
                    errorExit("stack error processing function parameters", CERR_INTERNAL);
                if (tmpToken.type != tComma)
                {
                    char c[255], tmpStr[255];
                    strcpy(c, "WRITE ");
                    switch (tmpToken.type) {
                    case tLiteral:
                        dbgMsg(" \"%s\"", tmpToken.data);
                        strcat(c, ifjCodeStr(tmpStr, tmpToken.data));
                        break;
                    case tInt: // aaa osetrit jestli jsou to spravne typu INT
                    case tInt2:
                        {
                            dbgMsg(" %s", tmpToken.data);
                            int tmpi;
                            // tmpi = atoi(tmpToken.data);
                            if (sscanf(tmpToken.data, "%d", &tmpi) != 1)
                                errorExit("wrong integer constant", CERR_INTERNAL);
                            strcat(c, ifjCodeInt(tmpStr, tmpi));
                        }
                    break;
                    case tReal: // aaa osetrit spravne type real
                    case tReal2:
                        {
                            dbgMsg(" %s", tmpToken.data);
                            double tmpd;
                            if (sscanf(tmpToken.data, "%lf", &tmpd) != 1)
                                errorExit("wrong integer constant", CERR_INTERNAL);
                            strcat(c, ifjCodeReal(tmpStr, tmpd));
                        }
                    break;
                    case tIdentifier:
                        dbgMsg(" %s", tmpToken.data);
                        strcat(c, "LF@");
                        strcat(c, tmpToken.data);
                        break;
                    default:
                        errorExit("unknow write() parameter", CERR_INTERNAL);
                        break;
                    }
                    addCode(c);
                }
            }
            addCode("");
        }
        else
        {
            // function call
            addCode("CREATEFRAME");
            // addCode("DEFVAR LF@%s%s%05d", funcPrefixName, funcName, funcNr);
            // zkontrolujeme parametry volane funkce s nadefinovanou v global symbol table
            int parcount = tstack_count(tmpStack);
            int estim = st_nr_func_params(&gst, sti->name);
            /* ten pocet se ted uz neda kontrolovat, protoze je to oddelene carkami a navic jsou to vyrazy, mozna by to slo pres carky ??? aaa
            if (tstack_count(tmpStack) != st_nr_func_params(&gst, sti->name))
                errorExit("wrong number of function parameters", CERR_SEM_ARG); */
            tFuncParam* param = sti->params;
            int parnr = 1;
            while (!tstack_isEmpty(tmpStack))
            {
                tstack_deleteItems(expStack);
                tmpToken.type = tNone;
                while (!tstack_isEmpty(tmpStack) && (tmpToken.type != tComma))
                {
                    if (!tstack_pop(tmpStack, &tmpToken))
                        errorExit("stack error processing function parameters", CERR_INTERNAL);
                    if(tmpToken.type != tComma)
                        tstack_pushl(expStack, tmpToken);
                }
                tTokenType typ;
                char tmpStr[255];
                addCode("DEFVAR TF@%%%d", parnr);
                sprintf(tmpStr, "TF@%%%%%d", parnr);
                typ = evalExp(tmpStr, expStack, st);
                if (!tstack_isEmpty(tmpStack))
                    dbgMsg(", ");
                // jestlize nejsou predavane typy ani na jednu stranu kompatibilni, tak chyba, jinak to proste zkusime
                // bbb je otazka jestli nebyt chytrejsi pri predavani potencionalniho null
                if (!typeIsCompatible(param->dataType, typ) && !typeIsCompatible(typ, param->dataType))
                {
                    char msg[255];
                    sprintf(msg, "function argument type %s does not match declaration type %s", tokenName[typ], tokenName[param->dataType]);
                    errorExit(msg, CERR_SEM_ARG);
                }
                param = param->next;
                if (!tstack_isEmpty(tmpStack) && param == NULL)
                {
                    errorExit("more function parameters than expected", CERR_SEM_ARG);
                    return;
                }
                parnr++;
            }
            if (param != NULL)
                errorExit("less function parameters than expected", CERR_SEM_ARG);
            // a ted to konecne zavolame i v kodu a vysledek premistime do nasi funkcni promennw
            addCode("CALL %s%s", funcPrefixName, funcName);
            if (sti->dataType != tVoid)
                addCode("MOVE LF@%s%s%05d TF@%%retval1", funcPrefixName, funcName, funcNr);
            addCode("");
        }
        dbgMsg(" )\n");
    }
    on_stack_state_error(tmpStack, notEmpty, "stack should be empty after processsing function arguments", CERR_INTERNAL);

    matchTokenAndNext(tRPar);
    
    safe_free(tmpToken.data);
    safe_free(funcName);
    tstack_free(&expStack);
    tstack_free(&tmpStack);
}

void processReturn(tSymTable* st, tStack* stack)
{
    // tReturn returnValue tSemicolon

    tStack* tmpStack = tstack_init();
    tTokenType expType = tNone;
    char* funcName = safe_malloc(MAX_TOKEN_LEN);

    matchTokenAndNext(tReturn);
    if (prgPass == 2)
        dbgMsg("return with ");
    // vracime vyraz
    parse_returnValue(st, tmpStack);
    // tstack_print(tmpStack);
    if (prgPass == 1)
    {
        tstack_deleteItems(tmpStack);
    }
    else
    {
        char tmpStr[255];
        sprintf(tmpStr, "LF@%%%s", funcRetValName);
        dbgMsg(" expression [ ");
        int cnt = tstack_count(tmpStack);
        if (cnt > 0)
            expType = evalExp(tmpStr, tmpStack, st);
        else
            expType = tNone;
        dbgMsg(" ] : %s\n", tokenName[expType]);
        on_stack_state_error(tmpStack, notEmpty, "stack should be empty after processsing return expression", CERR_INTERNAL);
        // st_print(&gst);
        if (actFunc != NULL) // check return type if in function definition body
        {
            if (cnt == 0)
            {   // navrat bez paramtetru
                if (actFunc->dataType != tVoid)
                    errorExit("missing value in return statement", CERR_SEM_RET);
            }
            else
            {
                if (actFunc->dataType == tVoid)
                    errorExit("void function returning value", CERR_SEM_RET);
                else if (actFunc->dataType != expType)
                    errorExit("wrong data type in return statement", CERR_SEM_ARG);
            }
            actFunc->hasReturn++;
        }
        // addCode("MOVE LF@%s TF@%s", funcRetValName, tmpExpResultName);
        addCode("POPFRAME");
        addCode("RETURN");
    }
    matchTokenAndNext(tSemicolon);

    free(funcName);
    tstack_free(&tmpStack);
}

void processAssignment(tSymTable* st)
{
    // tAssign expression tSemicolon

    char tmpStr[MAX_IFJC_LEN];

    matchTokenAndNext(tAssign);
    if (prgPass == 2)
        dbgMsg("Assignment %s = ", assignId.data);
    tStack* tmpStack = tstack_init();
    tTokenType expType = tNone;
    parse_expression(st, tmpStack);
    on_stack_state_error(tmpStack, isEmpty, "assignment with an empty expression", CERR_SYNTAX); //aaa
    tSymTableItem* sti;
    if (prgPass == 1)
    {
        tstack_deleteItems(tmpStack);
        sti = st_searchinsert(st, assignId.data);
        if (sti == NULL)
        {
            errorExit("assignment variable cannot be inserted to the symbol table", CERR_INTERNAL);
            return; // prevent C6011
        }
    }
    else
    {
        // tstack_print(tmpStack);
        sprintf(tmpStr, "LF@%s", assignId.data);
        sti = st_search(st, assignId.data);
        if (sti == NULL)
        {  
            // addCode("DEFVAR %s # assign", tmpStr);
            errorExit("assignment variable cannot be found in the symbol table", CERR_INTERNAL);
            return; // prevent C6011
        }
        dbgMsg("expression [ ");
        //int cnt = tstack_count(tmpStack);
        expType = evalExp(tmpStr, tmpStack, st);
        on_stack_state_error(tmpStack, notEmpty, "stack should be empty after processsing assignment expression", CERR_INTERNAL);
        sti->dataType = expType;
        // addCode("");
        // addCode("MOVE LF@%s TF@%s", assignId.data, tmpExpResultName);
        dbgMsg(" ]");
        matchTokenAndNext(tSemicolon);
        //dbgMsg(">>>>>>>> GST v nextTerminal - tAssign expression >>>>>\n");
        //st_print(&gst);
        dbgMsg(" : %s\n", tokenName[expType]);
    }
    tstack_free(&tmpStack);
}

void parse()
{
    if (SkipProlog(inf))
        dbgMsg("PASS 1 - PROLOG OK\n");
    else
        errorExit("Invalid PROLOG", CERR_SYNTAX);
    // preparation
    token.data = safe_malloc(MAX_TOKEN_LEN);
    assignId.data = safe_malloc(MAX_TOKEN_LEN);
    st_init(&gst);
    insert_embedded_functions(&gst);
    actFunc = NULL;
    // first pass
    prgPass = 1;
    nextToken();
    parse_program();
    // second pass
    prgPass = 2;
    srcLine = 1;
    funcVarCnt = 1;
    fseek(inf, 0, SEEK_SET);
    if (SkipProlog(inf))
        dbgMsg("PASS 2 - PROLOG OK\n");
    else
        errorExit("Invalid PROLOG", CERR_SYNTAX);  // tohle by nemelo nastat, kdyz to pri prvnim pruchodu proslo, ale pro jistotu
    addCodeVariableDefs(&gst);
    nextToken();
    parse_program();
    //dbgMsg("Global symbol table after the second pass:\n");
    //st_print(&gst);
    // release all not needed structures
    st_delete_all(&gst);
    free(assignId.data);
    free(token.data);
    // execute generated code - for test purposes only
    dbgMsg("..... code execution .....\n");
    FILE* outf = fopen("temp.ifjcode", "w");
    genCodeProlog(outf);
    generateEmbeddedFunctions(outf);
    generateFuncCode(outf);
    genCodeMain(outf);
    generateCode(outf);
    fclose(outf);
#if defined(_WIN32) || defined(WIN32)
    system("ic22int temp.ifjcode");
#else
    system("./ic22int temp.ifjcode");
#endif
}

/*
void parse_programs()
{
    prl("programs");

    switch (token.type)
    {
    case tEpilog:
        break;

    case tFunction:
    case tIf:
    case tWhile:
    case tSemicolon:
    case tIdentifier:
    case tReturn:
    case tMinus:
    case tLPar:
    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
    case tFuncName:
        parse_program();
        parse_programs();
        break;

    default:
        // epsilon
        parse_program();
        parse_programs();
        break;
    }
    
    // while (token.type != tEpilog)
    //     parse_program();
    
    level--;
} */

void parse_program()
{
    prl("program");

    while (token.type != tEpilog)
    {
        switch (token.type)
        {
        case tFunction:
            // function definition
            // tFunction tFuncName tLPar arguments tRPar tColon type tLCurl statements tRCurl
            processFunctionDefinition();
            break;

        case tIf:
        case tWhile:
        case tSemicolon:
        case tIdentifier:
        case tReturn:
        case tMinus:
        case tLPar:
        case tInt:
        case tReal:
        case tReal2:
        case tInt2:
        case tNull:
        case tLiteral:
        case tFuncName:
            parse_statement(&gst, NULL);
            break;

        default:
            errorExit("unexpected token in program", CERR_SYNTAX);
            break;
        }
        // dbgMsg("program loop\n");
    }

    level--;
}

void parse_statements(tSymTable *st, tStack* stack)
{
    prl("statements");

    switch (token.type)
    {
    case tIf:
    case tWhile:
    case tSemicolon:
    case tIdentifier:
    case tReturn:
    case tMinus:
    case tLPar:
    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
    case tFuncName:
        parse_statement(st, stack);
        parse_statements(st, stack);
        break;

    default:
        // epsilon
        break;
    }

    level--;
}

void parse_statement(tSymTable* st, tStack* stack)
{
    prl("statement");

    switch (token.type)
    {
    case tIf:
        // tIf tLPar expression tRPar tLCurl statements tRCurl tElse tLCurl statements tRCurl
        processIfStatement(st);
        break;

    case tWhile:
        // tWhile tLPar expression tRPar tLCurl statements tRCurl
        processWhileStatement(st);
        break;

    case tSemicolon:
        matchTokenAndNext(tSemicolon);
        break;

    case tIdentifier:
        // identifier as statement - possible assignment
        // dbgMsg("Asi bude prirazeni do %s\n", token.data);
        assignId.type = token.type;
        strcpy(assignId.data, token.data);
        matchTokenAndNext(tIdentifier);
        parse_nextTerminal(st);
        break;

    case tReturn:
        // return from function
        // tReturn returnValue tSemicolon
        processReturn(st, stack);
        break;

    case tMinus:
    case tLPar:
    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
    case tFuncName:
        parse_preExpression(st, stack);
        break;

    default:
        errorExit("unexpected token in statement", CERR_SYNTAX);
        break;
    }

    level--;
}

void parse_functionCall(tSymTable* st, tStack* stack)
{
    prl("functionCall");

    switch (token.type)
    {
    case tFuncName:
        // function call
        // tFuncName tLPar parameters tRPar
        processFunctionCall(st, stack);
        break;

    default:
        errorExit("unexpected token in statement", CERR_SYNTAX);
        break;
    }

    level--;
}

void parse_returnValue(tSymTable* st, tStack* stack)
{
    prl("returnValue");

    switch (token.type)
    {
    case tLPar:
    case tMinus:
    case tIdentifier:
    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
    case tFuncName:
        parse_expression(st, stack);
        break;

    default:
        // epsilon
        break;
    }

    level--;
}

void parse_nextTerminal(tSymTable* st)
{
    prl("nextTerminal");

    switch (token.type)
    {
        case tAssign:
            // tAssign expression tSemicolon
            processAssignment(st);
            break;

        case tPlus:
        case tMinus:
        case tMul:
        case tDiv:
        case tConcat:
        case tLess:
        case tLessEq:
        case tMore:
        case tMoreEq:
        case tIdentical:
        case tNotIdentical:
            parse_expression2(st, NULL);
            matchTokenAndNext(tSemicolon);
            break;

        default:
            // epsilon
            parse_expression2(st, NULL);
            matchTokenAndNext(tSemicolon);
            break;
    }

    level--;
}

void parse_preExpression(tSymTable* st, tStack* stack)
{
    prl("preExpresssion");

    switch (token.type)
    {
    case tMinus:
        matchTokenAndNext(tMinus);
        parse_minusTerm(stack);
        parse_expression2(st, stack);
        matchTokenAndNext(tSemicolon);
        break;

    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
        parse_const(stack);
        parse_expression2(st, stack);
        matchTokenAndNext(tSemicolon);
        break;

    case tFuncName:
        parse_functionCall(st, stack);
        parse_expression2(st, stack);
        matchTokenAndNext(tSemicolon);
        break;

    case tLPar:
        matchTokenAndNext(tLPar);
        parse_const(stack);
        parse_expression2(st, stack);
        matchTokenAndNext(tRPar);
        matchTokenAndNext(tSemicolon);
        break;

    default:
        errorExit("unexpected token in expression", CERR_SYNTAX);
        break;
    }

    level--;
}

void parse_expression(tSymTable* st, tStack* stack)
{
    prl("expression");

    switch (token.type)
    {
    case tMinus:
    case tIdentifier:
    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
    case tFuncName:
        parse_term(st, stack);
        parse_expression2(st, stack);
        break;
    
    case tLPar:
        matchTokenAndNext(tLPar);
        parse_expression(st, stack);
        matchTokenAndNext(tRPar);
        parse_expression2(st, stack);
        break;

    default:
        errorExit("unexpected token in expression", CERR_SYNTAX); // aa tady jsme meli EPS i kdyz ani predtim nebyl v gramatice
        break;
    }

    level--;
}

void parse_expression2(tSymTable* st, tStack* stack)
{
    prl("expression2");

    switch (token.type)
    {
    case tPlus:
    case tMinus:
    case tMul:
    case tDiv:
    case tConcat:
    case tLess:
    case tLessEq:
    case tMore:
    case tMoreEq:
    case tIdentical:
    case tNotIdentical:
        tstack_pushl(stack, token);
        nextToken();
        parse_expression(st, stack);
        break;

    default:
        // epsilon
        break;
    }

    level--;
}

void parse_arguments(tStack* stack)
{
    prl("arguments");
    
    switch (token.type)
    {
    case tNullTypeInt:
    case tNullTypeFloat:
    case tNullTypeString:
    case tTypeInt:
    case tTypeFloat:
    case tTypeString:
    case tVoid:
        parse_type(stack);
        tstack_pushl(stack, token);
        matchTokenAndNext(tIdentifier);
        parse_argumentVars(stack);
        break;

    default:
        // epsilon
        break;
    }

    level--;
}

void parse_argumentVars(tStack* stack)
{
    prl("argumentVars");

    switch (token.type)
    {
    case tComma:
        tstack_pushl(stack, token);
        matchTokenAndNext(tComma);
        parse_type(stack);
        tstack_pushl(stack, token);
        matchTokenAndNext(tIdentifier);
        parse_argumentVars(stack);
        break;

    default:
        // epsilon
        break;
    }

    level--;
}

void parse_parameters(tSymTable* st, tStack* stack)
{
    prl("parameters");

    switch (token.type)
    {
    case tLPar:
    case tMinus:
    case tIdentifier:
    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
    case tFuncName:
        parse_expression(st, stack);
        parse_parameters2(st, stack);
        break;

    default:
        // epsilon
        break;
    }

    level--;
}

void parse_parameters2(tSymTable* st, tStack* stack)
{
    prl("parameters2");

    switch (token.type)
    {
    case tComma:
        tstack_pushl(stack, token);
        matchTokenAndNext(tComma);
        parse_expression(st, stack);
        parse_parameters2(st, stack);
        break;

    default:
        // epsilon
        break;
    }

    level--;
}

void parse_term(tSymTable* st, tStack* stack)
{
    prl("term");

    switch (token.type)
    {
    case tMinus:
        matchTokenAndNext(tMinus);
        parse_minusTerm(stack);
        break;

    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
        parse_const(stack);
        break;

    case tIdentifier:
        tstack_pushl(stack, token);
        matchTokenAndNext(tIdentifier);
        break;

    case tFuncName:
        parse_functionCall(st, stack);
        break;

    default:
        errorExit("unexpected token in term", CERR_SYNTAX);
        break;
    }

    level--;
}

void parse_minusTerm(tStack* stack)
{
    prl("minusTerm");

    switch (token.type)
    {
    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
        parse_const(stack);
        break;

    case tIdentifier:
        matchTokenAndNext(tIdentifier);
        break;

    case tFuncName:
        matchTokenAndNext(tFuncName);
        break;

    default:
        errorExit("const, identifier or funcion name expected", CERR_SYNTAX);
        break;
    }

    level--;
}

void parse_const(tStack* stack)
{
    prl("const");

    switch (token.type)
    {
    case tInt:
    case tReal:
    case tReal2:
    case tInt2:
    case tNull:
    case tLiteral:
        tstack_pushl(stack, token);
        nextToken();
        break;

    default:
        errorExit("const expected", CERR_SYNTAX);
        break;
    }

    level--;
}

void parse_type(tStack* stack)
{
    prl("type");

    switch (token.type)
    {
    case tNullTypeInt:
    case tNullTypeFloat:
    case tNullTypeString:
    case tTypeInt:
    case tTypeFloat:
    case tTypeString:
    case tVoid:
        tstack_pushl(stack, token);
        nextToken();
        break;

    default:
        errorExit("type expected", CERR_SYNTAX);
        break;
    }

    level--;
}
