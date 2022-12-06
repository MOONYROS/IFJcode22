/**
 * @file generator.c
 * Implementace prekladace imperativniho jazyka IFJ22
 * 
 * @author Ondrej Lukasek (xlukas15)
 * @author Ondrej Koumar (xkouma02)
 * @author Jonas Morkus (xmorku03)
 * @author Milan Menc (xmencm00)
 * 
 * @brief This file is responsible for generating the code.
 * @date 2022-11
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "token.h"
#include "support.h"
#include "generator.h"
#include "symtable.h"
#include "expression.h"
#include "expStack.h"

tCodeLine* codeFirst = NULL;
tCodeLine* codeLast = NULL;
tCodeLine* funcCodeFirst = NULL;
tCodeLine* funcCodeLast = NULL;

const char tmpExpResultName[] = "tmpRes"; // tmpExpResultGigachad
const char expResultName[] = "tmpRes"; // tmpExpResultGigachad
//const char tmpFuncResultName[] = "tmpFnRes"; // tmpExpResultGigachad
const char funcPrefixName[] = "$func_";
const char funcRetValName[] = "%retval1";

extern int prgPass;
extern tSymTableItem* actFunc; // active function if processing function definition body

/**
 * @brief Function, that "translates" strings into processable form.
 * 
 * @param outStr output string
 * @param str input string
 * @return char*
 */
char* ifjCodeStr(char *outStr, char* str)
{
    char tmpStr[5];

    if (outStr == NULL)
        return NULL;

    strcpy(outStr, "string@");

    size_t srcLen = strlen(str);
    for (size_t i = 0; i < srcLen; i++)
    {
        size_t len = strlen(outStr);
        if ((str[i] >= 0 && str[i] <= 32) || (str[i]==35))
        {
            sprintf(tmpStr, "\\%03d", (int)str[i]);
            strcat(outStr, tmpStr);
        }
        else if (str[i] == 36)
        {
            strcat(outStr, "$");
        }
        else if (str[i] == 37)
        {
            strcat(outStr, "%%");
        }
        else if (str[i] == 92) // '\'
        {
            if (i + 1 < srcLen)
            {
                i++;
                switch (str[i])
                {
                case 'a':
                    strcat(outStr, "\\007");
                    break;
                case 'b':
                    strcat(outStr, "\\008");
                    break;
                case 't':
                    strcat(outStr, "\\009");
                    break;
                case 'n':
                    strcat(outStr, "\\010");
                    break;
                case 'v':
                    strcat(outStr, "\\011");
                    break;
                case 'f':
                    strcat(outStr, "\\012");
                    break;
                case 'r':
                    strcat(outStr, "\\013");
                    break;
                case 'e':
                    strcat(outStr, "\\027");
                    break;
                case '\'':
                    strcat(outStr, "\\039");
                    break;
                case '\\':
                    strcat(outStr, "\\092");
                    break;
                case '$':
                    strcat(outStr, "\\036");
                    break;
                case '\"':
                    strcat(outStr, "\\034");
                    break;
                default:
                    strcat(outStr, "\\");
                    outStr[len + 1] = str[i];
                    outStr[len + 2] = '\0';
                    break;
                }
            }
            else
                strcat(outStr, "\\092");
        }
        else
        {
            outStr[len] = str[i];
            outStr[len+1] = '\0';

        }
    }
    return outStr;
}

/**
 * @brief 
 * 
 * @param outStr output string
 * @param val value
 * @return char*
 */
char* ifjCodeInt(char* outStr, int val)
{
    if (outStr == NULL)
        return NULL;

    sprintf(outStr, "int@%d", val);
    return outStr;
}

/**
 * @brief
 * 
 * @param outStr output string
 * @param val value
 * @return char*
 */
char* ifjCodeReal(char* outStr, double val)
{
    if (outStr == NULL)
        return NULL;
    
    sprintf(outStr, "float@%a", val);
    return outStr;
}

char* ifjCodeNil(char* outStr)
{
    if (outStr == NULL)
        return NULL;

    sprintf(outStr, "nil@nil");
    return outStr;
}

/**
 * @brief
 * 
 * @param fmt
 * @param 
 * @return int
 */
int addCode(const char* fmt, ...)
{
    if (prgPass != 2)
        return 0;

    char code[MAX_IFJC_LEN];

    va_list args;
    va_start(args, fmt);
    int ret = vsprintf(code, fmt, args);
    va_end(args);

    tCodeLine* item = safe_malloc(sizeof(tCodeLine));
    item->code = safe_malloc(strlen(code) + 1);
    item->next = NULL;
    strcpy(item->code, code);
    if (actFunc == NULL)
    {
        if (codeFirst == NULL)
            codeFirst = item;
        else
            codeLast->next = item;
        codeLast = item;
    }
    else
    {
        if (funcCodeFirst == NULL)
            funcCodeFirst = item;
        else
            funcCodeLast->next = item;
        funcCodeLast = item;
    }

    return ret;
}

/**
 * @brief
 * 
 * @param table symbol table
 */
void addCodeVariableDefs(tSymTable* table)
{
    if (table == NULL)
        return;

    tSymTableItem* item;
    for (int i = 0; i < ST_SIZE; i++)
    {
        item = table->items[i];
        if (item != NULL)
        {
            if (!item->isFunction)
            {
                addCode("DEFVAR LF@%s", item->name);
            }
            while (item->next != NULL)
            {
                addCode("DEFVAR LF@%s", item->next->name);
                item = item->next;
            }
        }
    }
    addCode("");
}

/**
 * @brief Function that generates prolog code.
 * 
 * @param f file
 */
void genCodeProlog(FILE* f)
{
    fprintf(f, "# Gigachad PHP compiler generated code\n");
    fprintf(f, ".IFJcode22\n");
    fprintf(f, "JUMP $$main\n");
    fprintf(f, "\n");
}

/**
 * @brief 
 * 
 * @param f file
 */
void genCodeMain(FILE* f)
{
    fprintf(f, "LABEL $$main\n");
    fprintf(f, "CREATEFRAME\n");
    fprintf(f, "PUSHFRAME\n");
    fprintf(f, "CREATEFRAME\n");
    fprintf(f, "DEFVAR LF@otoc\n");
    fprintf(f, "DEFVAR LF@tmp\n");
    
    fprintf(f, "\n");
}

/**
 * @brief
 * 
 * @param f
 */
void generateCode(FILE* f)
{
    tCodeLine* item = codeFirst;
    while (item != NULL)
    {
        fprintf(f, "%s\n", item->code);
        tCodeLine* nxt = item->next;
        safe_free(item->code);
        safe_free(item);
        item = nxt;
    }
}

/**
 * @brief
 * 
 * @param f file
 */
void generateFuncCode(FILE* f)
{
    tCodeLine* item = funcCodeFirst;
    while (item != NULL)
    {
        fprintf(f, "%s\n", item->code);
        tCodeLine* nxt = item->next;
        safe_free(item->code);
        safe_free(item);
        item = nxt;
    }
    fprintf(f, "\n");
}

/**
 * @brief Function, that generates embedded functions.
 *
 * @param f file
 */
void generateEmbeddedFunctions(FILE* f)
{
    fprintf(f, "LABEL $func_readi\n");
    //fprintf(f, "PUSHFRAME\n");
    //fprintf(f, "CREATEFRAME\n");
    fprintf(f, "DEFVAR TF@%%retval1\n");
    fprintf(f, "READ TF@%%retval1 int\n");
    //fprintf(f, "POPFRAME\n");
    fprintf(f, "RETURN\n");
    fprintf(f, "\n");

    fprintf(f, "LABEL $func_readf\n");
    fprintf(f, "DEFVAR TF@%%retval1\n");
    fprintf(f, "READ TF@%%retval1 float\n");
    fprintf(f, "RETURN\n");
    fprintf(f, "\n");

    fprintf(f, "LABEL $func_reads\n");
    fprintf(f, "DEFVAR TF@%%retval1\n");
    fprintf(f, "READ TF@%%retval1 string\n");
    fprintf(f, "RETURN\n");
    fprintf(f, "\n");

    fprintf(f, "LABEL $func_strlen\n");
    fprintf(f, "PUSHFRAME\n");
    fprintf(f, "CREATEFRAME\n");
    fprintf(f, "DEFVAR LF@s\n");
    fprintf(f, "MOVE LF@s LF@%%1\n");
    fprintf(f, "DEFVAR LF@%%retval1\n");
    fprintf(f, "STRLEN LF@%%retval1 LF@s\n");
    fprintf(f, "POPFRAME\n");
    fprintf(f, "RETURN\n");
    fprintf(f, "\n");

    fprintf(f, "LABEL $func_substring\n");
    fprintf(f, "PUSHFRAME\n");
    fprintf(f, "CREATEFRAME\n");
    fprintf(f, "DEFVAR LF@s\n");
    fprintf(f, "DEFVAR LF@i\n");
    fprintf(f, "DEFVAR LF@j\n");
    fprintf(f, "DEFVAR TF@tmp\n");
    fprintf(f, "MOVE LF@s LF@%%1\n");
    fprintf(f, "MOVE LF@i LF@%%2\n");
    fprintf(f, "MOVE LF@j LF@%%3\n");
    fprintf(f, "DEFVAR LF@%%retval1\n");
    // osetreni chyb vstupnich parametru
    fprintf(f, "LT TF@tmp LF@i int@0\n");
    fprintf(f, "JUMPIFEQ $func_substring_nullend TF@tmp bool@true\n");
    fprintf(f, "LT TF@tmp LF@j int@0\n");
    fprintf(f, "JUMPIFEQ $func_substring_nullend TF@tmp bool@true\n");
    fprintf(f, "GT TF@tmp LF@i LF@j\n");
    fprintf(f, "JUMPIFEQ $func_substring_nullend TF@tmp bool@true\n");
    // do TF@len si vypocitam delku vstupniho retezce - bacha nesmime odted zavolat CREATEFRAME
    fprintf(f, "CREATEFRAME\n");
    fprintf(f, "DEFVAR TF@%%1\n");
    fprintf(f, "MOVE TF@%%1 LF@s\n");
    fprintf(f, "CALL $func_strlen\n");
    fprintf(f, "DEFVAR TF@len\n");
    fprintf(f, "MOVE TF@len TF@%%retval1\n");
    // a osetrujeme dalsi chyby spojene s delkou
    fprintf(f, "DEFVAR TF@tmp\n"); // musime zalozit znovu, protoze jsme si ji zrusili predchozim CREATEFRAME pred volanim funkce
    fprintf(f, "GT TF@tmp LF@i TF@len\n");
    fprintf(f, "JUMPIFEQ $func_substring_nullend TF@tmp bool@true\n");
    fprintf(f, "JUMPIFEQ $func_substring_nullend LF@i TF@len\n");
    fprintf(f, "GT TF@tmp LF@j TF@len\n");
    fprintf(f, "JUMPIFEQ $func_substring_nullend TF@tmp bool@true\n");
    // vykonny kod - vlastni smycka
    fprintf(f, "DEFVAR TF@ndx\n");
    fprintf(f, "MOVE TF@ndx LF@i\n");
    fprintf(f, "MOVE LF@%%retval1 string@\n");
    fprintf(f, "LABEL $func_substring_smycka\n");
    fprintf(f, "JUMPIFEQ $func_substring_end TF@ndx LF@j\n");
    fprintf(f, "GETCHAR TF@tmp LF@s TF@ndx\n");
    fprintf(f, "CONCAT LF@%%retval1 LF@%%retval1 TF@tmp\n");
    fprintf(f, "ADD TF@ndx TF@ndx int@1\n");
    fprintf(f, "JUMP $func_substring_smycka\n");
    // a koncime
    fprintf(f, "JUMP $func_substring_end\n");
    // vracime null pri chybe
    fprintf(f, "LABEL $func_substring_nullend\n");
    fprintf(f, "MOVE LF@%%retval1 nil@nil\n");
    fprintf(f, "LABEL $func_substring_end\n");
    fprintf(f, "POPFRAME\n");
    fprintf(f, "RETURN\n");
    fprintf(f, "\n");

    fprintf(f, "LABEL $func_ord\n");
    fprintf(f, "PUSHFRAME\n");
    fprintf(f, "CREATEFRAME\n");
    fprintf(f, "DEFVAR LF@c\n");
    fprintf(f, "MOVE LF@c LF@%%1\n");
    fprintf(f, "DEFVAR LF@%%retval1\n");
    fprintf(f, "DEFVAR TF@tmp\n");
    fprintf(f, "STRLEN TF@tmp LF@c\n");
    fprintf(f, "JUMPIFEQ $func_ord_zerolen TF@tmp int@0\n");
    fprintf(f, "STRI2INT LF@%%retval1 LF@c int@0\n");
    fprintf(f, "JUMP $func_ord_end\n");
    fprintf(f, "LABEL $func_ord_zerolen\n");
    fprintf(f, "MOVE LF@%%retval1 int@0\n");
    fprintf(f, "LABEL $func_ord_end\n");
    fprintf(f, "POPFRAME\n");
    fprintf(f, "RETURN\n");
    fprintf(f, "\n");

    fprintf(f, "LABEL $func_chr\n");
    fprintf(f, "PUSHFRAME\n");
    fprintf(f, "CREATEFRAME\n");
    fprintf(f, "DEFVAR LF@i\n");
    fprintf(f, "MOVE LF@i LF@%%1\n");
    fprintf(f, "DEFVAR LF@%%retval1\n");
    fprintf(f, "INT2CHAR LF@%%retval1 LF@i\n");
    fprintf(f, "POPFRAME\n");
    fprintf(f, "RETURN\n");
    fprintf(f, "\n");
}