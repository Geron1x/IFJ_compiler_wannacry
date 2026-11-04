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
    idType type;

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
    struct scope* prev;
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
    /** checking whether funceTable is even present */
    if (funcTable == NULL)
    {
        return ERROR_INTERNAL;
    }
    /** setting base values in symtable*/
    funcTable->varInstanceCounter = 0;
    funcTable->top = NULL;
    funcTable->scopeLevel = 0;
    funcTable->funcTree = NULL;

    /** allocatic temporary arrays to add into symtable all basic function
     *  In semantic analysis or anywhere else you should remember to free these
     * temporary array because symtableInsertFunc does not free memory for you
     */
    dataType* paramsPrint = malloc(sizeof(dataType) * 1);
    dataType* paramsToStr = malloc(sizeof(dataType) * 1);
    dataType* paramsToInt = malloc(sizeof(dataType) * 1);
    dataType* paramsStrLength = malloc(sizeof(dataType) * 1);
    dataType* paramsSubStr = malloc(sizeof(dataType) * 3);
    dataType* paramsOrd = malloc(sizeof(dataType) * 2);
    dataType* paramsChr = malloc(sizeof(dataType) * 1);

    /** checking whether even one failed malloc was detected
     *  if so - clean all the temporary arrays and return error
     */
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
    /** clean all tempopary arrays
     *  dont forget to do so in other parts when inserting value into funcTable
     */
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
    /** check whether symtable is present and key is valid */
    if (symtable == NULL || key == NULL)
    {
        return ERROR_INTERNAL;
    }

    nodeData* data = malloc(sizeof(nodeData));
    if (data == NULL)
    {
        return ERROR_INTERNAL;
    }
    /** inserting function information into new nodeData */
    data->type = TYPE_FUNCTION;
    data->funcData.paramCount = paramCount;
    data->funcData.isDefined = true;
    data->funcData.returnType = returnType;
    /** check whether function has any parameters */
    if (paramCount > 0 && paramType != NULL)
    {
        /** if so create array to store all the data types of params */
        data->funcData.paramType = malloc(sizeof(dataType) * paramCount);
        if (data->funcData.paramType == NULL)
        {
            free(data);
            return ERROR_INTERNAL;
        }
        /** copy from inserted array all the parameter types into its own
         * array*/
        memcpy(data->funcData.paramType, paramType,
               sizeof(dataType) * paramCount);
    }
    else
    {
        /** if no params given just NULL */
        data->funcData.paramType = NULL;
    }
    /** inserting new node with the data we got and store feedback code from
     *  bst lib*/
    bstStatus status = bstInsert(&(symtable->funcTree), key, data);
    /** check whether all went smooth */
    if (status != BST_INSERTED)
    {
        /** it did not, the node either already present or allocation
         * failed, clear all created mess and send feedback code*/
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
    /** checking presence of symtable */
    if (symtable == NULL)
    {
        return ERROR_INTERNAL;
    }
    scope* newScope = malloc(sizeof(scope));
    if (newScope == NULL)
    {
        return ERROR_INTERNAL;
    }
    /** placing current top stack as a stack after new Scope */
    newScope->prev = symtable->top;
    /** tree is yet to build */
    newScope->tree = NULL;
    /** placing the new scope on top of the stack */
    symtable->top = newScope;
    /** deepening scope level by one */
    symtable->scopeLevel++;
    return 0;
}

void stackPop(symtable* symtable)
{
    /** checking presence of symtable and at least one scope */
    if (symtable == NULL || symtable->top == NULL)
    {
        return;
    }
    /** saving current top of the stack into temp var */
    scope* temp = symtable->top;
    /** popping the stack  */
    symtable->top = temp->prev;
    /** clearing local tree from previous top of the stack */
    bstDestroy(&(temp->tree), eraseData);
    /** clearing the stack (scope) itself */
    free(temp);
    /** checking whether scopeLevel is not going to go negative */
    if (symtable->scopeLevel > 0)
    {
        /** dectrementing scope level, because one stack got deleted */
        symtable->scopeLevel--;
    }
}

Errors stackInsertVar(symtable* symtable, const char* key, dataType type,
                      bool isConst, bool isInit)
{
    /** check whether symtable is present and atleast
     * one scope is present too */
    if (symtable == NULL || symtable->top == NULL)
    {
        return ERROR_INTERNAL;
    }
    nodeData* newData = malloc(sizeof(nodeData));
    if (newData == NULL)
    {
        return ERROR_INTERNAL;
    }
    /** set given variable info into created nodeData element*/
    newData->type = TYPE_VARIABLE;
    newData->varData.type = type;
    newData->varData.isConst = isConst;
    newData->varData.isInit = isInit;
    newData->varData.instanceNum = symtable->varInstanceCounter;
    /** insert a new node which contains nodeData with all the info that was
     * passed to the function about variable, create status variable to get
     * feedback code */
    bstStatus status = bstInsert(&(symtable->top->tree), key, newData);
    /** status check, in case of unsuccessful insertion clear created data on
     * heap */
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
    /** checking whether key is valid, symtable is present, there is atleast one
     * scope */
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
    /** checking whether symtable is present */
    if (symtable == NULL)
    {
        return;
    }
    /** pass through all stack and clear them */
    while (symtable->top != NULL)
    {
        /** saving the current top of the stack */
        scope* temp = symtable->top;
        /** popping the stack */
        symtable->top = symtable->top->prev;
        /** if the tree is present, clear it */
        if (temp->tree != NULL)
        {
            /** clearing local tree */
            bstDestroy(&(temp->tree), eraseData);
        }
        /** the the previous top */
        free(temp);
    }
    /** top of the stack is empty and should point at NULL */
    symtable->top = NULL;
}
Errors symtableInit(symtable** symTable)
{
    /** checking whether the address of ptr to symtable we got is even valid */
    if (symTable == NULL)
    {
        return ERROR_INTERNAL;
    }

    symtable* newTable = malloc(sizeof(symtable));
    if (newTable == NULL)
    {
        return ERROR_INTERNAL;
    }
    /** setting base values for symtable */
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
    /** checking whether symtable present */
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
