/**
 * @file symtable.c
 * @brief File for the symtable implementation
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */
#include "symtable.h"

#include <stdlib.h>

#include "bst.h"
#include "error.h"
#include "string.h"

struct variableData
{
    dataType type;
    bool isConst;
    bool isInit;
    int instanceNum;
};

struct functionData
{
    int paramCount;
    dataType* paramType;
    dataType returnType;
    bool isDefined;
};

struct nodeData
{
    idType type; /**< shows which union member is active */

    union
    {
        struct functionData funcData;
        struct variableData varData;
    };
};
typedef struct scope
{
    bstNode* tree; /**< store all the variable info, several in one symtable,
                      cleared and filled throughout program life */
    struct scope* prev; /**< links to the enclosing scope */
} scope;


struct symtable
{
    bstNode* funcTree; /**< store all the function info, only one in symtable,
                          lives till the end of semantic analysis */
    scope* top;
    int scopeLevel;
    int varInstanceCounter; /**< counter to give repeating variable a unique
                               number for them to be distinguishable (enables
                               shadowing) */
};

Errors symtableBaseFuncInit(symtable* funcTable)
{

    if (funcTable == NULL)
    {
        return ERROR_INTERNAL;
    }
    /** setting base values in symtable*/
    funcTable->varInstanceCounter = 0;
    funcTable->top = NULL;
    funcTable->scopeLevel = 0;
    funcTable->funcTree = NULL;

    dataType* paramsPrint = malloc(sizeof(dataType) * 1);
    dataType* paramsToStr = malloc(sizeof(dataType) * 1);
    dataType* paramsToInt = malloc(sizeof(dataType) * 1);
    dataType* paramsStrLength = malloc(sizeof(dataType) * 1);
    dataType* paramsSubStr = malloc(sizeof(dataType) * 3);
    dataType* paramsOrd = malloc(sizeof(dataType) * 2);
    dataType* paramsChr = malloc(sizeof(dataType) * 1);

    if (!paramsPrint || !paramsToStr || !paramsToInt || !paramsStrLength ||
        !paramsSubStr || !paramsOrd || !paramsChr)
    {
        free(paramsPrint);
        free(paramsToStr);
        free(paramsToInt);
        free(paramsStrLength);
        free(paramsSubStr);
        free(paramsOrd);
        free(paramsChr);
        return ERROR_INTERNAL;
    }

    /** insert params types into temporary arrays */
    paramsPrint[0] = TYPE_ANY;
    paramsToStr[0] = TYPE_ANY;
    paramsToInt[0] = TYPE_DOUBLE;
    paramsStrLength[0] = TYPE_STRING;

    paramsSubStr[0] = TYPE_STRING;
    paramsSubStr[1] = TYPE_INT;
    paramsSubStr[2] = TYPE_INT;

    paramsOrd[0] = TYPE_STRING;
    paramsOrd[1] = TYPE_INT;

    paramsChr[0] = TYPE_INT;


    Errors status = 0;
    /** print(value: Any): Unit */
    if (status == 0)
    {
        status =
            symtableInsertFunc(funcTable, "print", TYPE_UNIT, 1, paramsPrint);
    }
    /** readLine(): String */
    if (status == 0)
    {
        status =
            symtableInsertFunc(funcTable, "readLine", TYPE_STRING, 0, NULL);
    }
    /** readInt(): Int */
    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "readInt", TYPE_INT, 0, NULL);
    }
    /** readDouble(): Double */
    if (status == 0)
    {
        status =
            symtableInsertFunc(funcTable, "readDouble", TYPE_DOUBLE, 0, NULL);
    }
    // toStr(value: Any): String

    if (status == 0)
    {
        status =
            symtableInsertFunc(funcTable, "toStr", TYPE_STRING, 1, paramsToStr);
    }
    /** toInt(value: Double): Int */
    if (status == 0)
    {
        status =
            symtableInsertFunc(funcTable, "toInt", TYPE_INT, 1, paramsToInt);
    }
    /** strLength(value: String): Int */

    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "strLength", TYPE_INT, 1,
                                    paramsStrLength);
    }
    /** subStr(value: String, start: Int, end: Int): String */

    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "subStr", TYPE_STRING, 3,
                                    paramsSubStr);
    }
    /** ord(value: String, index: Int): Int */

    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "ord", TYPE_INT, 2, paramsOrd);
    }
    /** chr(code: Int): String */
    if (status == 0)
    {
        status =
            symtableInsertFunc(funcTable, "chr", TYPE_STRING, 1, paramsChr);
    }

    free(paramsPrint);
    free(paramsToStr);
    free(paramsToInt);
    free(paramsStrLength);
    free(paramsSubStr);
    free(paramsOrd);
    free(paramsChr);

    if (status != 0 && funcTable->funcTree != NULL)
    {
        bstDestroy(&(funcTable->funcTree), eraseData);
        funcTable->funcTree = NULL;
    }

    return status;
}

Errors symtableInsertFunc(symtable* symtable, const char* key,
                          dataType returnType, int paramCount,
                          const dataType* paramType)
{
    if (symtable == NULL || key == NULL)
    {
        return ERROR_INTERNAL;
    }

    nodeData* data = malloc(sizeof(nodeData));
    if (data == NULL)
    {
        return ERROR_INTERNAL;
    }

    data->type = TYPE_FUNCTION;
    data->funcData.paramCount = paramCount;
    data->funcData.isDefined = true;
    data->funcData.returnType = returnType;

    if (paramCount > 0 && paramType != NULL)
    {
        /* Functions own their parameter type array */
        data->funcData.paramType = malloc(sizeof(dataType) * paramCount);
        if (data->funcData.paramType == NULL)
        {
            free(data);
            return ERROR_INTERNAL;
        }
        memcpy(data->funcData.paramType, paramType,
               sizeof(dataType) * paramCount);
    }
    else
    {
        data->funcData.paramType = NULL;
    }

    bstStatus status = bstInsert(&(symtable->funcTree), key, data);

    if (status != BST_INSERTED)
    {
        if (data->funcData.paramType != NULL)
        {
            free(data->funcData.paramType);
        }
        free(data);
        if (status == BST_EXISTS)
        {
            /** it was reoccuring node, send redifinition error code */
            return ERROR_REDEFINITION;
        }
        else
        {
            /** it was something else (probably allocation failure) send
             * internal error code */
            return ERROR_INTERNAL;
        }
    }
    return 0;
}

Errors stackPush(symtable* symtable)
{
    if (symtable == NULL)
    {
        return ERROR_INTERNAL;
    }
    scope* newScope = malloc(sizeof(scope));
    if (newScope == NULL)
    {
        return ERROR_INTERNAL;
    }

    newScope->prev = symtable->top;
    newScope->tree = NULL;
    symtable->top = newScope;
    symtable->scopeLevel++;
    return 0;
}

void stackPop(symtable* symtable)
{
    if (symtable == NULL || symtable->top == NULL)
    {
        return;
    }
    scope* temp = symtable->top;
    /** popping the stack  */
    symtable->top = temp->prev;

    bstDestroy(&(temp->tree), eraseData);
    free(temp);
    if (symtable->scopeLevel > 0)
    {
        symtable->scopeLevel--;
    }
}

Errors stackInsertVar(symtable* symtable, const char* key, dataType type,
                      bool isConst, bool isInit)
{
    if (symtable == NULL || symtable->top == NULL)
    {
        return ERROR_INTERNAL;
    }
    nodeData* newData = malloc(sizeof(nodeData));
    if (newData == NULL)
    {
        return ERROR_INTERNAL;
    }

    newData->type = TYPE_VARIABLE;
    newData->varData.type = type;
    newData->varData.isConst = isConst;
    newData->varData.isInit = isInit;
    newData->varData.instanceNum = symtable->varInstanceCounter;

    bstStatus status = bstInsert(&(symtable->top->tree), key, newData);
    switch (status)
    {
        /** element was added successfully, increment instance counter */
        case BST_INSERTED:
            symtable->varInstanceCounter++;
            break;
        /** Such variable was already present in local tree,
         * Redifinition error */
        case BST_EXISTS:
            free(newData);
            return ERROR_REDEFINITION;
        /** Allocation error */
        case BST_ERROR:
            free(newData);
            return ERROR_INTERNAL;
        /** safe guard check for undifined behaviour */
        default:
            free(newData);
            return ERROR_OTHER_SEMANTIC;
    }
    return 0;
}

nodeData* stackFindVar(const char* key, symtable* symtable)
{
    if (key == NULL || symtable == NULL || symtable->top == NULL)
    {
        return NULL;
    }
    nodeData* data = NULL;
    scope* temp = symtable->top;
    /** search throughout all the scopes in the symtable, stop when there are no
     * more scopes to go or landed on some data*/
    while (temp != NULL && data == NULL)
    {
        data = bstSearch(temp->tree, key);
        temp = temp->prev;
    }

    return data;
}
void stackDestroy(symtable* symtable)
{

    if (symtable == NULL)
    {
        return;
    }
    /** pass through all stack and clear them */
    while (symtable->top != NULL)
    {
        scope* temp = symtable->top;
        /** popping the stack */
        symtable->top = symtable->top->prev;
        if (temp->tree != NULL)
        {
            bstDestroy(&(temp->tree), eraseData);
        }
        /** top of the stack is empty and should point at NULL */
        free(temp);
    }
    symtable->top = NULL;
}
Errors symtableInit(symtable** symTable)
{

    if (symTable == NULL)
    {
        return ERROR_INTERNAL;
    }

    symtable* newTable = malloc(sizeof(symtable));
    if (newTable == NULL)
    {
        return ERROR_INTERNAL;
    }

    newTable->funcTree = NULL;
    newTable->top = NULL;
    newTable->scopeLevel = 0;
    newTable->varInstanceCounter = 0;

    /** transferring created table into passed argument */
    *symTable = newTable;
    return 0;
}
void symtableDestroy(symtable* symtable)
{
    if (symtable == NULL)
    {
        return;
    }
    /** clear all scopes */
    stackDestroy(symtable);
    /** clear function tree */
    bstDestroy(&(symtable->funcTree), eraseData);
    /** clear the symtable */
    free(symtable);
}
void eraseData(nodeData* data)
{
    if (data == NULL)
    {
        return;
    }
    /** checking whether there are any paramenters store into array in function
     * node*/
    if (data->type == TYPE_FUNCTION)
    {
        free(data->funcData.paramType);
    }
    free(data);
}
