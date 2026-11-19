/**
 * @file symtable.c
 * @brief File for the symbol-table implementation
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */
#include "symtable.h"

#include <stdlib.h>

#include "ast_internal.h"
#include "bst.h"
#include "error.h"
#include "string.h"


#define INSTANCE_NUM_START 1

struct variableData
{
    dataType type;
    typeDesc* typeInfo; /** type of a variable that requires descriptor */
    bool isConst;
    size_t instanceNum;
};

struct functionData
{
    int paramCount;
    dataType* paramType;
    dataType returnType;
    typeDesc* returnTypeInfo; /** return type of a function that requires
                                 descriptor */
    struct astNode*
        astNode; /**< get ptr to function's ast node from symtable */
    typeDesc**
        paramTypeInfos; /**< array of parameters that require descriptor */
    bool isAnalyzing; /**< flag that shows whether function is being analyzed */
    bool isChecked;   /**< flag that shows whether function has been checked */
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
    size_t varInstanceCounter; /**< counter to give repeating variable a unique
                               number for them to be distinguishable (enables
                               shadowing) */
};

static nodeData* stackFindVar(const char* key, symtable* symtable);

int symtableBaseFuncInit(symtable* funcTable)
{

    if (funcTable == NULL)
    {
        return ERROR_INTERNAL;
    }
    /** setting base values in symtable*/
    funcTable->varInstanceCounter = INSTANCE_NUM_START;
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
    paramsPrint[0] = S_TYPE_ANY;
    paramsToStr[0] = S_TYPE_ANY;
    paramsToInt[0] = S_TYPE_DOUBLE;
    paramsStrLength[0] = S_TYPE_STRING;

    paramsSubStr[0] = S_TYPE_STRING;
    paramsSubStr[1] = S_TYPE_INT;
    paramsSubStr[2] = S_TYPE_INT;

    paramsOrd[0] = S_TYPE_STRING;
    paramsOrd[1] = S_TYPE_INT;

    paramsChr[0] = S_TYPE_INT;


    Errors status = 0;
    /** print(value: Any): Unit */
    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "print", S_TYPE_UNIT, 1,
                                    paramsPrint, NULL);
    }
    /** readLine(): String */
    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "readLine", S_TYPE_STRING, 0,
                                    NULL, NULL);
    }
    /** readInt(): Int */
    if (status == 0)
    {
        status =
            symtableInsertFunc(funcTable, "readInt", S_TYPE_INT, 0, NULL, NULL);
    }
    /** readDouble(): Double */
    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "readDouble", S_TYPE_DOUBLE, 0,
                                    NULL, NULL);
    }
    // toStr(value: Any): String

    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "toStr", S_TYPE_STRING, 1,
                                    paramsToStr, NULL);
    }
    /** toInt(value: Double): Int */
    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "toInt", S_TYPE_INT, 1,
                                    paramsToInt, NULL);
    }
    /** strLength(value: String): Int */

    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "strLength", S_TYPE_INT, 1,
                                    paramsStrLength, NULL);
    }
    /** subStr(value: String, start: Int, end: Int): String */

    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "subStr", S_TYPE_STRING, 3,
                                    paramsSubStr, NULL);
    }
    /** ord(value: String, index: Int): Int */

    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "ord", S_TYPE_INT, 2, paramsOrd,
                                    NULL);
    }
    /** chr(code: Int): String */
    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "chr", S_TYPE_STRING, 1,
                                    paramsChr, NULL);
    }
    if (status == 0)
    {
        status = symtableInsertFunc(funcTable, "readBoolean", S_TYPE_BOOL, 0,
                                    NULL, NULL);
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

int symtableInsertFunc(symtable* symtable, const char* key, dataType returnType,
                       int paramCount, const dataType* paramType,
                       struct astNode* funcNode)
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
    data->funcData.returnType = returnType;
    data->funcData.paramTypeInfos = NULL;
    data->funcData.returnTypeInfo = NULL;

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
    data->funcData.isAnalyzing = false;
    data->funcData.isChecked = false;
    data->funcData.astNode = funcNode;
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
            /** Repeated function name, return redefinition error */
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

int stackPush(symtable* symtable)
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
    /** Remove the top scope */
    symtable->top = temp->prev;

    bstDestroy(&(temp->tree), eraseData);
    free(temp);
    if (symtable->scopeLevel > 0)
    {
        symtable->scopeLevel--;
    }
}

int stackInsertVar(symtable* symtable, const char* key, dataType type,
                   bool isConst)
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
    newData->varData.instanceNum = symtable->varInstanceCounter;
    newData->varData.typeInfo = NULL;

    bstStatus status = bstInsert(&(symtable->top->tree), key, newData);
    switch (status)
    {
        /** element was added successfully, increment instance counter */
        case BST_INSERTED:
            symtable->varInstanceCounter++;
            break;
        /** Variable already exists in the current scope */
        case BST_EXISTS:
            free(newData);
            return ERROR_REDEFINITION;
        /** Allocation error */
        case BST_ERROR:
            free(newData);
            return ERROR_INTERNAL;
        /** Guard against an unexpected BST status */
        default:
            free(newData);
            return ERROR_OTHER_SEMANTIC;
    }
    return 0;
}

/** Descriptor getters and setters */

const typeDesc* symtableGetFuncReturnTypeDesc(symtable* table, const char* key)
{
    if (table == NULL || key == NULL)
    {
        return NULL;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data == NULL || data->type != TYPE_FUNCTION)
    {
        return NULL;
    }

    return data->funcData.returnTypeInfo;
}

int symtableSetFuncReturnTypeDesc(symtable* table, const char* key,
                                  const typeDesc* desc)
{
    if (table == NULL || key == NULL)
    {
        return ERROR_INTERNAL;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data == NULL || data->type != TYPE_FUNCTION)
    {
        return ERROR_INTERNAL;
    }

    typeDesc* copy = NULL;
    if (desc != NULL)
    {
        copy = typeDescClone(desc);
        if (copy == NULL)
        {
            return ERROR_INTERNAL;
        }
    }

    typeDescDestroy(data->funcData.returnTypeInfo);
    data->funcData.returnTypeInfo = copy;
    return 0;
}

const typeDesc* symtableGetFuncParamTypeDesc(symtable* table, const char* key,
                                             int index)
{
    if (table == NULL || key == NULL || index < 0)
    {
        return NULL;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data == NULL || data->type != TYPE_FUNCTION)
    {
        return NULL;
    }

    if (index >= data->funcData.paramCount ||
        data->funcData.paramTypeInfos == NULL)
    {
        return NULL;
    }

    return data->funcData.paramTypeInfos[index];
}

int symtableSetFuncParamTypeDesc(symtable* table, const char* key, int index,
                                 const typeDesc* desc)
{
    if (table == NULL || key == NULL || index < 0)
    {
        return ERROR_INTERNAL;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data == NULL || data->type != TYPE_FUNCTION)
    {
        return ERROR_INTERNAL;
    }

    if (index >= data->funcData.paramCount)
    {
        return ERROR_INTERNAL;
    }

    if (data->funcData.paramTypeInfos == NULL)
    {
        data->funcData.paramTypeInfos =
            calloc((size_t)data->funcData.paramCount, sizeof(typeDesc*));
        if (data->funcData.paramTypeInfos == NULL)
        {
            return ERROR_INTERNAL;
        }
    }

    typeDesc* copy = NULL;
    if (desc != NULL)
    {
        copy = typeDescClone(desc);
        if (copy == NULL)
        {
            return ERROR_INTERNAL;
        }
    }

    typeDescDestroy(data->funcData.paramTypeInfos[index]);
    data->funcData.paramTypeInfos[index] = copy;
    return 0;
}

const typeDesc* symtableGetVarTypeDesc(symtable* table, const char* key)
{
    if (table == NULL || key == NULL)
    {
        return NULL;
    }

    nodeData* data = stackFindVar(key, table);
    if (data == NULL || data->type != TYPE_VARIABLE)
    {
        return NULL;
    }

    return data->varData.typeInfo;
}

int symtableSetVarTypeDesc(symtable* table, const char* key,
                           const typeDesc* desc)
{
    if (table == NULL || key == NULL)
    {
        return ERROR_INTERNAL;
    }

    nodeData* data = stackFindVar(key, table);
    if (data == NULL || data->type != TYPE_VARIABLE)
    {
        return ERROR_INTERNAL;
    }

    typeDesc* copy = NULL;
    if (desc != NULL)
    {
        copy = typeDescClone(desc);
        if (copy == NULL)
        {
            return ERROR_INTERNAL;
        }
    }

    typeDescDestroy(data->varData.typeInfo);
    data->varData.typeInfo = copy;
    return 0;
}
int symtableGetFuncParamCount(symtable* table, const char* key)
{
    if (table == NULL || key == NULL)
    {
        return -1;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data == NULL || data->type != TYPE_FUNCTION)
    {
        return -1;
    }

    return data->funcData.paramCount;
}

dataType symtableGetFuncParamType(symtable* table, const char* key, int index)
{
    if (table == NULL || key == NULL || index < 0)
    {
        return S_TYPE_UNKNOWN;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data == NULL || data->type != TYPE_FUNCTION)
    {
        return S_TYPE_UNKNOWN;
    }

    if (index >= data->funcData.paramCount || data->funcData.paramType == NULL)
    {
        return S_TYPE_UNKNOWN;
    }

    return data->funcData.paramType[index];
}
bool symtableGetFuncIsAnalyzing(symtable* table, const char* key)
{
    if (table == NULL || key == NULL)
    {
        return false;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data == NULL || data->type != TYPE_FUNCTION)
    {
        return false;
    }

    return data->funcData.isAnalyzing;
}

void symtableSetFuncIsAnalyzing(symtable* table, const char* key, bool status)
{
    if (table == NULL || key == NULL)
    {
        return;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data != NULL && data->type == TYPE_FUNCTION)
    {
        data->funcData.isAnalyzing = status;
    }
}

bool symtableGetFuncIsChecked(symtable* table, const char* key)
{
    if (table == NULL || key == NULL)
    {
        return false;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data == NULL || data->type != TYPE_FUNCTION)
    {
        return false;
    }

    return data->funcData.isChecked;
}

void symtableSetFuncIsChecked(symtable* table, const char* key, bool status)
{
    if (table == NULL || key == NULL)
    {
        return;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data != NULL && data->type == TYPE_FUNCTION)
    {
        data->funcData.isChecked = status;
    }
}

struct astNode* symtableGetFuncNode(symtable* table, const char* key)
{
    if (table == NULL || key == NULL)
    {
        return NULL;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data == NULL || data->type != TYPE_FUNCTION)
    {
        return NULL;
    }

    return data->funcData.astNode;
}

void symtableSetFuncReturn(symtable* table, const char* key,
                           dataType returnType)
{
    if (table == NULL || key == NULL)
    {
        return;
    }

    nodeData* data = bstSearch(table->funcTree, key);
    if (data != NULL && data->type == TYPE_FUNCTION)
    {
        data->funcData.returnType = returnType;
    }
}
bool symtableGetIsVarConst(symtable* symtable, const char* key)
{
    if (symtable == NULL || key == NULL)
    {
        return false;
    }

    nodeData* data = stackFindVar(key, symtable);
    if (data == NULL)
    {
        return false;
    }

    return data->varData.isConst;
}
dataType symtableGetFuncReturn(symtable* symtable, const char* key)
{
    if (symtable == NULL || key == NULL)
    {
        return S_TYPE_UNKNOWN;
    }

    nodeData* data = bstSearch(symtable->funcTree, key);
    if (data == NULL)
    {
        return S_TYPE_UNKNOWN;
    }

    return data->funcData.returnType;
}
dataType symtableGetVarType(symtable* symtable, const char* key)
{
    if (symtable == NULL || key == NULL)
    {
        return S_TYPE_UNKNOWN;
    }

    nodeData* data = stackFindVar(key, symtable);
    if (data == NULL)
    {
        return S_TYPE_UNKNOWN;
    }

    return data->varData.type;
}
size_t symtableGetVarId(symtable* symtable, const char* key)
{
    nodeData* data = stackFindVar(key, symtable);
    if (data == NULL || data->type != TYPE_VARIABLE)
    {
        return 0;
    }
    return (size_t)data->varData.instanceNum;
}


/** Search for a variable from the innermost active scope */
static nodeData* stackFindVar(const char* key, symtable* symtable)
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
        /** Remove the top scope */
        symtable->top = symtable->top->prev;
        if (temp->tree != NULL)
        {
            bstDestroy(&(temp->tree), eraseData);
        }
        /** top of the stack is empty and should point at NULL */
        free(temp);
    }
    symtable->scopeLevel = 0;
    symtable->top = NULL;
}
int symtableInit(symtable** symTable)
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
    newTable->varInstanceCounter = INSTANCE_NUM_START;

    /** Store the created table in the caller */
    *symTable = newTable;
    return 0;
}
symtable* symtableCreateFuncContext(symtable* globalTable)
{
    if (globalTable == NULL)
    {
        return NULL;
    }

    symtable* newTable = malloc(sizeof(symtable));
    if (newTable == NULL)
    {
        return NULL;
    }

    newTable->funcTree = globalTable->funcTree;
    newTable->top = NULL;
    newTable->scopeLevel = 0;
    newTable->varInstanceCounter = INSTANCE_NUM_START;

    return newTable;
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

    if (data->type == TYPE_VARIABLE)
    {
        /** clears list's descriptor, if nothing present does nothing */
        typeDescDestroy(data->varData.typeInfo);
    }
    else if (data->type == TYPE_FUNCTION)
    {
        free(data->funcData.paramType);


        typeDescDestroy(data->funcData.returnTypeInfo);

        /** clears the array of parameters with descriptor */
        if (data->funcData.paramTypeInfos != NULL)
        {
            for (int i = 0; i < data->funcData.paramCount; i++)
            {
                typeDescDestroy(data->funcData.paramTypeInfos[i]);
            }
            free(data->funcData.paramTypeInfos);
        }
    }
    free(data);
}