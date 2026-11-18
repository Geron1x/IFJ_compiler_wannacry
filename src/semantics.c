/**
 * @file semantics.c
 * @brief File for the implementaion of semantic analysis
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */


#include "semantics.h"

#include <stdlib.h>
#include <string.h>

#include "ast_internal.h"
#include "error.h"
#include "symtable.h"

static void analyzeEnd(astNode* node, bool* allReturn, bool* allUnit);
static dataType resolveBodyType(astNode* bodyNode, dataType declaredType,
                                Errors* err);
static int semanticsFirstPass(const astNode* prog, symtable* globalTable);
static int semanticsSecondPass(const astNode* prog, symtable* globalTable);
static int checkBlock(astNode* blockNode, symtable* globalTable,
                      astNode* currentFunc);
static int checkFunctionBody(astNode* funcNode, symtable* globalTable);

static int checkStatement(astNode* statNode, symtable* globalTable,
                          astNode* currentNode);

static int checkVarDef(astNode* varNode, symtable* globalTable,
                       astNode* currentNode);
static int checkSet(astNode* setNode, symtable* globalTable,
                    astNode* currentNode);
static int checkIf(astNode* ifNode, symtable* globalTable,
                   astNode* currentNode);
static int checkWhile(astNode* whileNode, symtable* globalTable,
                      astNode* currentNode);
static int checkReturn(astNode* returnNode, symtable* globalTable,
                       astNode* currentNode);
static int checkFor(astNode* forNode, symtable* globalTable,
                    astNode* currentNode);

static dataType checkExpr(astNode* exprNode, symtable* globalTable,
                          astNode* currentNode, Errors* err);
static dataType checkBinaryExpr(astNode* biNode, symtable* globalTable,
                                astNode* currentNode, Errors* err);
static dataType checkUnaryExpr(astNode* uNode, symtable* globalTable,
                               astNode* currentNode, Errors* err);
static dataType checkCallExpr(astNode* callNode, symtable* globalTable,
                              astNode* currentNode, Errors* err);
static dataType checkIfExpr(astNode* ifNode, symtable* globalTable,
                            astNode* currentNode, Errors* err);
/** Run semantic analysis in declaration and body passes */
int semanticsCheck(const astNode* prog)
{
    if (prog == NULL)
    {
        return ERROR_INTERNAL;
    }
    symtable* globalTable = NULL;
    Errors status = symtableInit(&globalTable);
    if (status != 0)
    {
        return status;
    }
    status = symtableBaseFuncInit(globalTable);
    if (status != 0)
    {
        symtableDestroy(globalTable);
        return status;
    }
    status = semanticsFirstPass(prog, globalTable);
    if (status != 0)
    {
        symtableDestroy(globalTable);
        return status;
    }
    status = semanticsSecondPass(prog, globalTable);
    symtableDestroy(globalTable);
    if (status != 0)
    {
        return status;
    }
    return 0;
}
/** Register all function declarations before checking their bodies */
static int semanticsFirstPass(const astNode* prog, symtable* globalTable)
{
    int mainFoundCount = 0;
    int childNum = prog->childCount;
    for (int i = 0; i < childNum; i++)
    {
        astNode* funcNode = prog->children[i];

        if (funcNode == NULL || funcNode->nodeType != AST_FUNC)
        {
            continue;
        }

        int paramCount = funcNode->childCount - 1;
        if (funcNode->data.funcData.isMain)
        {
            mainFoundCount++;

            if (paramCount != 0)
            {
                return ERROR_WRONG_ARGUMENTS;
            }

            if (funcNode->retType != S_TYPE_UNIT &&
                funcNode->retType != S_TYPE_UNKNOWN)
            {
                return ERROR_TYPE_MISMATCH;
            }
        }
        dataType* paramTypes = NULL;

        if (paramCount > 0)
        {
            paramTypes = malloc(sizeof(dataType) * paramCount);
            if (paramTypes == NULL)
            {
                return ERROR_INTERNAL;
            }
        }
        /** j is index current paramenter (shifted by 1 to right cause under
         * index 0 is block node);
         *  k is index to parameters from j+1 to paramCount-1  */
        for (int j = 1; j <= paramCount; j++)
        {
            for (int k = j + 1; k <= paramCount; k++)
            {
                if (strcmp(funcNode->children[j]->data.varData.key,
                           funcNode->children[k]->data.varData.key) == 0)
                {
                    free(paramTypes);
                    return ERROR_REDEFINITION;
                }
            }
            paramTypes[j - 1] = funcNode->children[j]->retType;
        }
        Errors status = symtableInsertFunc(
            globalTable, funcNode->data.funcData.key, funcNode->retType,
            paramCount, paramTypes, funcNode);
        free(paramTypes);
        if (status != 0)
        {

            return status;
        }
    }

    if (mainFoundCount == 0)
    {
        return ERROR_UNDEFINED_NAME;
    }
    if (mainFoundCount > 1)
    {
        return ERROR_REDEFINITION;
    }

    return 0;
}
/** Check function bodies after all function signatures are available */
static int semanticsSecondPass(const astNode* prog, symtable* globalTable)
{
    int childNum = prog->childCount;
    for (int i = 0; i < childNum; i++)
    {
        astNode* funcNode = prog->children[i];
        if (funcNode == NULL || funcNode->nodeType != AST_FUNC)
        {
            continue;
        }

        Errors status = checkFunctionBody(funcNode, globalTable);
        if (status != 0)
        {
            return status;
        }
        if (funcNode->data.funcData.isMain &&
            symtableGetFuncReturn(globalTable, funcNode->data.funcData.key) !=
                S_TYPE_UNIT)
        {
            return ERROR_TYPE_MISMATCH;
        }
    }
    return 0;
}
/** Check one function and resolve its return type */
static int checkFunctionBody(astNode* funcNode, symtable* globalTable)
{
    if (symtableGetFuncIsChecked(globalTable, funcNode->data.funcData.key) ==
        true)
    {
        return 0;
    }

    symtableSetFuncIsAnalyzing(globalTable, funcNode->data.funcData.key, true);

    Errors status = stackPush(globalTable);
    if (status != 0)
    {
        return status;
    }
    int paramCount = funcNode->childCount;
    for (int i = 1; i < paramCount; i++)
    {
        status =
            stackInsertVar(globalTable, funcNode->children[i]->data.varData.key,
                           funcNode->children[i]->retType, true);
        if (status != 0)
        {
            stackDestroy(globalTable);
            return status;
        }
        funcNode->children[i]->data.varData.id = symtableGetVarId(
            globalTable, funcNode->children[i]->data.varData.key);
    }
    status = checkBlock(funcNode->children[0], globalTable, funcNode);
    stackDestroy(globalTable);
    if (status != 0)
    {
        return status;
    }

    dataType declaredType =
        symtableGetFuncReturn(globalTable, funcNode->data.funcData.key);
    dataType actualBodyType =
        resolveBodyType(funcNode->children[0], declaredType, &status);
    if (status != 0)
    {
        return status;
    }

    if (declaredType == S_TYPE_UNKNOWN)
    {
        if (actualBodyType == S_TYPE_UNKNOWN)
        {
            return ERROR_TYPE_MISMATCH;
        }
        funcNode->retType = actualBodyType;
        symtableSetFuncReturn(globalTable, funcNode->data.funcData.key,
                              actualBodyType);
    }
    else if (declaredType != actualBodyType)
    {
        if (actualBodyType == S_TYPE_INT && declaredType == S_TYPE_DOUBLE)
        {
            astNode* last = funcNode->children[0];
            if (funcNode->children[0]->nodeType == AST_BLOCK &&
                funcNode->children[0]->childCount > 0)
            {
                last = funcNode->children[0]
                           ->children[funcNode->children[0]->childCount - 1];
            }
            if (last != NULL)
            {
                last->int2float = true;
            }
        }
        else
        {
            return ERROR_TYPE_MISMATCH;
        }
    }

    symtableSetFuncIsAnalyzing(globalTable, funcNode->data.funcData.key, false);
    symtableSetFuncIsChecked(globalTable, funcNode->data.funcData.key, true);
    return 0;
}

/** check whether the end of a block returns or produces unit */
static void analyzeEnd(astNode* node, bool* allReturn, bool* allUnit)
{
    if (node == NULL)
    {
        *allReturn = false;
        *allUnit = true;
        return;
    }

    astNode* lastStat = node;
    if (node->nodeType == AST_BLOCK)
    {
        if (node->childCount == 0)
        {
            *allReturn = false;
            *allUnit = true;
            return;
        }
        lastStat = node->children[node->childCount - 1];
    }

    switch (lastStat->nodeType)
    {
        case AST_STAT_RETURN:
            *allReturn = true;
            *allUnit = false;
            return;

        case AST_STAT_VAR:
        case AST_STAT_SET:
        case AST_STAT_WHILE:
        case AST_STAT_FOR:
            *allReturn = false;
            *allUnit = true;
            return;

        case AST_EXPR_BINARY:
        case AST_EXPR_UNARY:
        case AST_EXPR_LITERAL:
        case AST_EXPR_VAR:
        case AST_EXPR_CALL:
            *allReturn = false;
            *allUnit = (lastStat->retType == S_TYPE_UNIT);
            return;
        case AST_EXPR_IF:
        {
            bool thenRet = false;
            bool thenUnit = false;
            bool elseRet = false;
            bool elseUnit = false;
            analyzeEnd(lastStat->children[1], &thenRet, &thenUnit);
            analyzeEnd(lastStat->children[2], &elseRet, &elseUnit);
            *allReturn = (thenRet && elseRet);
            if (*allReturn)
            {
                *allUnit = false;
            }
            else
            {
                *allUnit = (lastStat->retType == S_TYPE_UNIT);
            }
            return;
        }
        case AST_STAT_IF:
        {
            bool thenRet = false;
            bool thenUnit = false;
            bool elseRet = false;
            bool elseUnit = false;

            if (lastStat->childCount > 1)
            {
                analyzeEnd(lastStat->children[1], &thenRet, &thenUnit);
            }

            if (lastStat->childCount > 2 && lastStat->children[2] != NULL)
            {
                analyzeEnd(lastStat->children[2], &elseRet, &elseUnit);
            }
            else
            {
                elseRet = false;
                elseUnit = true;
            }

            *allReturn = (thenRet && elseRet);
            *allUnit = (thenUnit && elseUnit);
            return;
        }

        default:
            *allReturn = false;
            *allUnit = false;
            return;
    }
}
/** resolve the type produced by the last statement of a function body */
static dataType resolveBodyType(astNode* bodyNode, dataType declaredType,
                                Errors* err)
{
    if (bodyNode == NULL)
    {
        return S_TYPE_UNIT;
    }


    if (declaredType == S_TYPE_UNIT)
    {
        return S_TYPE_UNIT;
    }


    astNode* lastStat = bodyNode;
    if (bodyNode->nodeType == AST_BLOCK)
    {
        if (bodyNode->childCount == 0)
        {
            return S_TYPE_UNIT;
        }
        lastStat = bodyNode->children[bodyNode->childCount - 1];
    }
    switch (lastStat->nodeType)
    {
        case AST_EXPR_BINARY:
        case AST_EXPR_UNARY:
        case AST_EXPR_LITERAL:
        case AST_EXPR_VAR:
        case AST_EXPR_CALL:
            return lastStat->retType;
        case AST_EXPR_IF:
        {
            bool thenRet = false;
            bool thenUnit = false;
            bool elseRet = false;
            bool elseUnit = false;
            analyzeEnd(lastStat->children[1], &thenRet, &thenUnit);
            analyzeEnd(lastStat->children[2], &elseRet, &elseUnit);
            if (thenRet && elseRet)
            {
                if (declaredType == S_TYPE_UNKNOWN)
                {
                    *err = ERROR_TYPE_MISMATCH;
                    return S_TYPE_UNKNOWN;
                }
                return declaredType;
            }
            return lastStat->retType;
        }
        case AST_STAT_VAR:
        case AST_STAT_SET:
        case AST_STAT_WHILE:
        case AST_STAT_FOR:
            return S_TYPE_UNIT;

        case AST_STAT_RETURN:
            if (declaredType == S_TYPE_UNKNOWN)
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
            return declaredType;

        case AST_STAT_IF:
        {
            bool allReturn = false;
            bool allUnit = false;
            analyzeEnd(lastStat, &allReturn, &allUnit);

            if (allReturn)
            {
                if (declaredType == S_TYPE_UNKNOWN)
                {
                    *err = ERROR_TYPE_MISMATCH;
                    return S_TYPE_UNKNOWN;
                }
                return declaredType;
            }
            if (declaredType == S_TYPE_UNKNOWN && allUnit)
            {
                return S_TYPE_UNIT;
            }

            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }

        default:
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
    }
}

/** Check all statements in one block */
static int checkBlock(astNode* blockNode, symtable* globalTable,
                      astNode* currentFunc)
{
    if (blockNode == NULL || globalTable == NULL)
    {
        return ERROR_INTERNAL;
    }

    int statCount = blockNode->childCount;
    Errors status = 0;
    for (int i = 0; i < statCount; i++)
    {
        status =
            checkStatement(blockNode->children[i], globalTable, currentFunc);
        if (status != 0)
        {
            return status;
        }
    }

    return 0;
}
static int checkStatement(astNode* statNode, symtable* globalTable,
                          astNode* currentNode)
{
    Errors status = 0;
    switch (statNode->nodeType)
    {
        case AST_STAT_VAR:
            status = checkVarDef(statNode, globalTable, currentNode);
            break;
        case AST_STAT_SET:
            status = checkSet(statNode, globalTable, currentNode);
            break;
        case AST_STAT_IF:
            status = checkIf(statNode, globalTable, currentNode);
            break;
        case AST_STAT_WHILE:
            status = checkWhile(statNode, globalTable, currentNode);
            break;
        case AST_STAT_RETURN:
            status = checkReturn(statNode, globalTable, currentNode);
            break;
        case AST_EXPR_BINARY:
        case AST_EXPR_UNARY:
        case AST_EXPR_LITERAL:
        case AST_EXPR_VAR:
        case AST_EXPR_CALL:
        case AST_EXPR_IF:
        {
            Errors err = 0;
            dataType exprType =
                checkExpr(statNode, globalTable, currentNode, &err);
            if (err != 0)
            {
                return err;
            }
            if (exprType == S_TYPE_UNKNOWN)
            {
                return ERROR_TYPE_MISMATCH;
            }

            statNode->retType = exprType;
            return 0;
        }
        case AST_STAT_FOR:
            return checkFor(statNode, globalTable, currentNode);
        default:
            return ERROR_INTERNAL;
    }
    if (status != 0)
    {
        return status;
    }
    return 0;
}

static int checkVarDef(astNode* varNode, symtable* globalTable,
                       astNode* currentNode)
{
    if (varNode == NULL || globalTable == NULL || currentNode == NULL ||
        varNode->childCount == 0)
    {
        return ERROR_INTERNAL;
    }
    Errors err = 0;
    dataType type =
        checkExpr(varNode->children[0], globalTable, currentNode, &err);
    if (type == S_TYPE_UNKNOWN)
    {
        if (err != 0)
        {
            return err;
        }
        return ERROR_TYPE_MISMATCH;
    }

    if (varNode->retType != S_TYPE_UNKNOWN)
    {
        if (varNode->retType == type)
        {
        }
        else if (varNode->retType == S_TYPE_DOUBLE && type == S_TYPE_INT)
        {
            type = S_TYPE_DOUBLE;
            if (varNode->children[0] != NULL)
            {
                varNode->children[0]->int2float = true;
            }
        }
        else
        {
            return ERROR_TYPE_MISMATCH;
        }
    }
    else
    {
        varNode->retType = type;
    }
    Errors status = stackInsertVar(globalTable, varNode->data.varData.key, type,
                                   varNode->data.varData.isConst);
    if (status != 0)
    {
        return status;
    }
    varNode->data.varData.id =
        symtableGetVarId(globalTable, varNode->data.varData.key);
    return 0;
}
static dataType checkExpr(astNode* exprNode, symtable* globalTable,
                          astNode* currentNode, Errors* err)
{
    if (exprNode == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }
    switch (exprNode->nodeType)
    {
        case AST_EXPR_LITERAL:
        {
            if (exprNode->retType == S_TYPE_UNKNOWN)
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
            return exprNode->retType;
        }
        case AST_EXPR_VAR:
        {
            if (exprNode->data.varData.key == NULL)
            {
                *err = ERROR_INTERNAL;
                return S_TYPE_UNKNOWN;
            }
            exprNode->retType =
                symtableGetVarType(globalTable, exprNode->data.varData.key);
            if (exprNode->retType == S_TYPE_UNKNOWN)
            {
                *err = ERROR_UNDEFINED_NAME;
            }
            exprNode->data.varData.id =
                symtableGetVarId(globalTable, exprNode->data.varData.key);
            return exprNode->retType;
        }
        case AST_EXPR_BINARY:
            return checkBinaryExpr(exprNode, globalTable, currentNode, err);

        case AST_EXPR_UNARY:
            return checkUnaryExpr(exprNode, globalTable, currentNode, err);

        case AST_EXPR_CALL:
            return checkCallExpr(exprNode, globalTable, currentNode, err);
        case AST_EXPR_IF:
            return checkIfExpr(exprNode, globalTable, currentNode, err);
        default:
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
    }
}


static int checkSet(astNode* setNode, symtable* globalTable,
                    astNode* currentNode)
{
    if (setNode == NULL || globalTable == NULL || currentNode == NULL ||
        setNode->childCount < 1 || setNode->children[0] == NULL ||
        setNode->data.varData.key == NULL)
    {
        return ERROR_INTERNAL;
    }

    dataType type = symtableGetVarType(globalTable, setNode->data.varData.key);
    if (type == S_TYPE_UNKNOWN)
    {
        return ERROR_UNDEFINED_NAME;
    }
    setNode->data.varData.id =
        symtableGetVarId(globalTable, setNode->data.varData.key);
    if (symtableGetIsVarConst(globalTable, setNode->data.varData.key) == true)
    {
        return ERROR_IMMUTABLE_WRITE;
    }
    Errors err = 0;
    dataType expType =
        checkExpr(setNode->children[0], globalTable, currentNode, &err);
    if (expType == S_TYPE_UNKNOWN)
    {
        if (err != 0)
        {
            return err;
        }
        return ERROR_TYPE_MISMATCH;
    }
    if (type != expType)
    {
        if (type == S_TYPE_DOUBLE && expType == S_TYPE_INT)
        {
            setNode->children[0]->int2float = true;
        }
        else
        {
            return ERROR_TYPE_MISMATCH;
        }
    }
    return 0;
}


static int checkIf(astNode* ifNode, symtable* globalTable, astNode* currentNode)
{
    if (ifNode == NULL || currentNode == NULL || ifNode->children[0] == NULL)
    {
        return ERROR_INTERNAL;
    }
    Errors status = 0;
    /** check condition */
    dataType type =
        checkExpr(ifNode->children[0], globalTable, currentNode, &status);
    if (status != 0)
    {
        return status;
    }
    if (type != S_TYPE_BOOL)
    {
        return ERROR_TYPE_MISMATCH;
    }
    if (ifNode->children[1] == NULL ||
        ifNode->children[1]->nodeType != AST_BLOCK)
    {
        return ERROR_INTERNAL;
    }
    /** check then block */

    status = stackPush(globalTable);
    if (status != 0)
    {
        return status;
    }
    status = checkBlock(ifNode->children[1], globalTable, currentNode);
    stackPop(globalTable);
    if (status != 0)
    {
        return status;
    }

    /** check else block */
    if (ifNode->childCount > 2 && ifNode->children[2] != NULL)
    {
        if (ifNode->children[2]->nodeType != AST_BLOCK)
        {
            return ERROR_INTERNAL;
        }
        status = stackPush(globalTable);
        if (status != 0)
        {
            return status;
        }
        status = checkBlock(ifNode->children[2], globalTable, currentNode);
        stackPop(globalTable);
        if (status != 0)
        {

            return status;
        }
    }
    return 0;
}


static int checkWhile(astNode* whileNode, symtable* globalTable,
                      astNode* currentNode)
{
    if (whileNode == NULL || globalTable == NULL || currentNode == NULL ||
        whileNode->childCount < 2 || whileNode->children == NULL ||
        whileNode->children[0] == NULL || whileNode->children[1] == NULL)
    {
        return ERROR_INTERNAL;
    }
    Errors err = 0;
    dataType type =
        checkExpr(whileNode->children[0], globalTable, currentNode, &err);
    if (err != 0)
    {
        return err;
    }
    if (type != S_TYPE_BOOL)
    {
        return ERROR_TYPE_MISMATCH;
    }

    Errors status = stackPush(globalTable);
    if (status != 0)
    {
        return status;
    }
    status = checkBlock(whileNode->children[1], globalTable, currentNode);
    stackPop(globalTable);
    if (status != 0)
    {
        return status;
    }

    return 0;
}
static int checkFor(astNode* forNode, symtable* globalTable,
                    astNode* currentNode)
{
    if (forNode == NULL || globalTable == NULL || currentNode == NULL ||
        forNode->data.forData.key == NULL || forNode->childCount < 3 ||
        forNode->children == NULL || forNode->children[0] == NULL ||
        forNode->children[1] == NULL || forNode->children[2] == NULL)
    {
        return ERROR_INTERNAL;
    }
    Errors err = 0;


    /** check from value */
    dataType fromType =
        checkExpr(forNode->children[0], globalTable, currentNode, &err);
    if (err != 0)
    {
        return err;
    }
    if (fromType != S_TYPE_INT)
    {
        return ERROR_TYPE_MISMATCH;
    }

    /** check end value */
    dataType endType =
        checkExpr(forNode->children[1], globalTable, currentNode, &err);
    if (err != 0)
    {
        return err;
    }
    if (endType != S_TYPE_INT)
    {
        return ERROR_TYPE_MISMATCH;
    }

    /** step check */
    if (forNode->data.forData.hasStep)
    {
        if (forNode->childCount < 4 || forNode->children[3] == NULL)
        {
            return ERROR_INTERNAL;
        }
        astNode* stepNode = forNode->children[3];
        dataType stepType = checkExpr(stepNode, globalTable, currentNode, &err);
        if (err != 0)
        {
            return err;
        }
        if (stepType != S_TYPE_INT)
        {
            return ERROR_TYPE_MISMATCH;
        }

        /** zero literal */
        if (stepNode->nodeType == AST_EXPR_LITERAL &&
            stepNode->data.intVal == 0)
        {
            return ERROR_OTHER_SEMANTIC;
        }
    }

    Errors status = stackPush(globalTable);
    if (status != 0)
    {
        return status;
    }

    status = stackInsertVar(globalTable, forNode->data.forData.key, S_TYPE_INT,
                            true);
    if (status != 0)
    {
        stackPop(globalTable);
        return status;
    }
    forNode->data.forData.id =
        symtableGetVarId(globalTable, forNode->data.forData.key);
    /** check if-guards + for loop body block*/
    status = checkBlock(forNode->children[2], globalTable, currentNode);
    stackPop(globalTable);
    if (status != 0)
    {
        return status;
    }

    return 0;
}
static int checkReturn(astNode* returnNode, symtable* globalTable,
                       astNode* currentNode)
{
    if (returnNode == NULL || globalTable == NULL || currentNode == NULL ||
        currentNode->data.funcData.key == NULL)
    {
        return ERROR_INTERNAL;
    }
    dataType expectedType =
        symtableGetFuncReturn(globalTable, currentNode->data.funcData.key);
    dataType type = S_TYPE_UNIT;

    if (returnNode->childCount > 0 && returnNode->children != NULL &&
        returnNode->children[0] != NULL)
    {
        Errors err = 0;
        type =
            checkExpr(returnNode->children[0], globalTable, currentNode, &err);
        if (err != 0)
        {
            return err;
        }
        if (type == S_TYPE_UNKNOWN)
        {
            return ERROR_TYPE_MISMATCH;
        }

        if (expectedType == S_TYPE_UNIT)
        {
            returnNode->children[0]->retType = type;
            returnNode->retType = S_TYPE_UNIT;
            return 0;
        }

        if (type == S_TYPE_INT && expectedType == S_TYPE_DOUBLE)
        {
            type = S_TYPE_DOUBLE;
            returnNode->children[0]->int2float = true;
        }
    }
    else
    {

        if (expectedType != S_TYPE_UNIT)
        {
            return ERROR_TYPE_MISMATCH;
        }
    }

    if (type != expectedType)
    {
        return ERROR_TYPE_MISMATCH;
    }

    returnNode->retType = expectedType;

    return 0;
}
static dataType checkIfExpr(astNode* ifNode, symtable* globalTable,
                            astNode* currentNode, Errors* err)
{
    if (ifNode == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL || ifNode->childCount < 3 || ifNode->children == NULL ||
        ifNode->children[0] == NULL || ifNode->children[1] == NULL ||
        ifNode->children[2] == NULL ||
        ifNode->children[1]->nodeType != AST_BLOCK ||
        ifNode->children[2]->nodeType != AST_BLOCK)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }
    /** condition must be bool */
    dataType condType =
        checkExpr(ifNode->children[0], globalTable, currentNode, err);
    if (*err != 0)
    {
        return S_TYPE_UNKNOWN;
    }
    if (condType != S_TYPE_BOOL)
    {
        *err = ERROR_TYPE_MISMATCH;
        return S_TYPE_UNKNOWN;
    }

    /** check then */
    Errors status = stackPush(globalTable);
    if (status != 0)
    {
        *err = status;
        return S_TYPE_UNKNOWN;
    }
    status = checkBlock(ifNode->children[1], globalTable, currentNode);
    stackPop(globalTable);
    if (status != 0)
    {
        *err = status;
        return S_TYPE_UNKNOWN;
    }

    /** check else */
    status = stackPush(globalTable);
    if (status != 0)
    {
        *err = status;
        return S_TYPE_UNKNOWN;
    }
    status = checkBlock(ifNode->children[2], globalTable, currentNode);
    stackPop(globalTable);
    if (status != 0)
    {
        *err = status;
        return S_TYPE_UNKNOWN;
    }

    astNode* thenBlock = ifNode->children[1];
    astNode* elseBlock = ifNode->children[2];

    astNode* thenLast = NULL;
    if (thenBlock->childCount > 0)
    {
        thenLast = thenBlock->children[thenBlock->childCount - 1];
    }

    astNode* elseLast = NULL;
    if (elseBlock->childCount > 0)
    {
        elseLast = elseBlock->children[elseBlock->childCount - 1];
    }

    bool thenIsReturn =
        (thenLast != NULL && thenLast->nodeType == AST_STAT_RETURN);
    bool elseIsReturn =
        (elseLast != NULL && elseLast->nodeType == AST_STAT_RETURN);

    /** get return type from if expression when return statement is present */
    if (thenIsReturn && elseIsReturn)
    {
        ifNode->retType = S_TYPE_UNIT;
        return S_TYPE_UNIT;
    }
    if (thenIsReturn)
    {
        ifNode->retType = resolveBodyType(elseBlock, S_TYPE_UNKNOWN, err);
        if (*err != 0)
        {
            return S_TYPE_UNKNOWN;
        }
        return ifNode->retType;
    }
    if (elseIsReturn)
    {

        ifNode->retType = resolveBodyType(thenBlock, S_TYPE_UNKNOWN, err);
        if (*err != 0)
        {
            return S_TYPE_UNKNOWN;
        }
        return ifNode->retType;
    }

    dataType thenType = resolveBodyType(thenBlock, S_TYPE_UNKNOWN, err);
    if (*err != 0)
    {
        return S_TYPE_UNKNOWN;
    }
    dataType elseType = resolveBodyType(elseBlock, S_TYPE_UNKNOWN, err);
    if (*err != 0)
    {
        return S_TYPE_UNKNOWN;
    }

    /** check return types of branches */
    dataType resType = S_TYPE_UNKNOWN;
    if (thenType == elseType && thenType != S_TYPE_UNKNOWN)
    {
        resType = thenType;
    }
    else if (thenType == S_TYPE_INT && elseType == S_TYPE_DOUBLE)
    {
        if (thenLast != NULL)
        {
            thenLast->int2float = true;
        }
        resType = S_TYPE_DOUBLE;
    }
    else if (thenType == S_TYPE_DOUBLE && elseType == S_TYPE_INT)
    {
        if (elseLast != NULL)
        {
            elseLast->int2float = true;
        }
        resType = S_TYPE_DOUBLE;
    }
    else
    {
        *err = ERROR_TYPE_MISMATCH;
        return S_TYPE_UNKNOWN;
    }

    ifNode->retType = resType;
    return resType;
}

/** Check operand types and numeric conversions for a binary expression */
static dataType checkBinaryExpr(astNode* biNode, symtable* globalTable,
                                astNode* currentNode, Errors* err)
{
    if (biNode == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL || biNode->childCount < 2 || biNode->children == NULL ||
        biNode->children[0] == NULL || biNode->children[1] == NULL)
    {
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }

    dataType typeLeft =
        checkExpr(biNode->children[0], globalTable, currentNode, err);
    if (typeLeft == S_TYPE_UNKNOWN)
    {
        return S_TYPE_UNKNOWN;
    }
    dataType typeRight =
        checkExpr(biNode->children[1], globalTable, currentNode, err);
    if (typeRight == S_TYPE_UNKNOWN)
    {
        return S_TYPE_UNKNOWN;
    }

    /** && || check */
    if (biNode->data.opType == AST_OP_AND || biNode->data.opType == AST_OP_OR)
    {
        if (typeLeft != S_TYPE_BOOL || typeRight != S_TYPE_BOOL)
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }
        biNode->retType = S_TYPE_BOOL;
        return S_TYPE_BOOL;
    }

    if (typeLeft != typeRight)
    {
        if (typeLeft == S_TYPE_DOUBLE && typeRight == S_TYPE_INT)
        {
            typeRight = S_TYPE_DOUBLE;
            biNode->children[1]->int2float = true;
        }
        else if (typeLeft == S_TYPE_INT && typeRight == S_TYPE_DOUBLE)
        {
            typeLeft = S_TYPE_DOUBLE;
            biNode->children[0]->int2float = true;
        }
        else
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }
    }

    switch (biNode->data.opType)
    {
        case AST_OP_ADD:
        case AST_OP_SUB:
        case AST_OP_MUL:
        case AST_OP_DIV:
            if (typeLeft == S_TYPE_INT || typeLeft == S_TYPE_DOUBLE ||
                typeLeft == S_TYPE_STRING)
            {
                if (typeLeft == S_TYPE_STRING &&
                    biNode->data.opType != AST_OP_ADD)
                {
                    *err = ERROR_TYPE_MISMATCH;
                    return S_TYPE_UNKNOWN;
                }
            }
            else
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
            break;
        case AST_OP_L:
        case AST_OP_G:
        case AST_OP_LEQ:
        case AST_OP_GEQ:
            if (typeLeft == S_TYPE_INT || typeLeft == S_TYPE_DOUBLE ||
                typeLeft == S_TYPE_STRING)
            {
                typeLeft = S_TYPE_BOOL;
            }
            else
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
            break;
        case AST_OP_EQ:
        case AST_OP_NEQ:
            if (typeLeft == S_TYPE_INT || typeLeft == S_TYPE_DOUBLE ||
                typeLeft == S_TYPE_STRING || typeLeft == S_TYPE_BOOL)
            {
                typeLeft = S_TYPE_BOOL;
            }
            else
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
            break;
        default:
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
    }
    if (typeLeft == S_TYPE_UNKNOWN)
    {
        *err = ERROR_TYPE_MISMATCH;
        return S_TYPE_UNKNOWN;
    }
    biNode->retType = typeLeft;
    return typeLeft;
}


/** Check function existence, arguments and recursive calls */
static dataType checkCallExpr(astNode* callNode, symtable* globalTable,
                              astNode* currentNode, Errors* err)
{
    if (callNode == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL || callNode->data.funcData.key == NULL)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }
    int paramCount =
        symtableGetFuncParamCount(globalTable, callNode->data.funcData.key);
    if (paramCount == -1)
    {
        *err = ERROR_UNDEFINED_NAME;
        return S_TYPE_UNKNOWN;
    }
    if (paramCount != callNode->childCount)
    {
        *err = ERROR_WRONG_ARGUMENTS;
        return S_TYPE_UNKNOWN;
    }
    dataType type = S_TYPE_UNKNOWN;
    dataType expectedType = S_TYPE_UNKNOWN;
    for (int i = 0; i < paramCount; i++)
    {
        type = checkExpr(callNode->children[i], globalTable, currentNode, err);
        if (*err != 0)
        {
            return S_TYPE_UNKNOWN;
        }
        expectedType = symtableGetFuncParamType(globalTable,
                                                callNode->data.funcData.key, i);

        if (expectedType == S_TYPE_ANY)
        {
            if (type == S_TYPE_UNKNOWN)
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
        }
        else
        {
            if (expectedType != type)
            {
                if (type == S_TYPE_INT && expectedType == S_TYPE_DOUBLE)
                {
                    callNode->children[i]->int2float = true;
                }
                else
                {
                    *err = ERROR_WRONG_ARGUMENTS;
                    return S_TYPE_UNKNOWN;
                }
            }
            else if (expectedType == S_TYPE_UNKNOWN)
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
        }
    }
    dataType retType =
        symtableGetFuncReturn(globalTable, callNode->data.funcData.key);
    if (retType == S_TYPE_UNKNOWN)
    {
        if (symtableGetFuncIsAnalyzing(globalTable,
                                       callNode->data.funcData.key) == true)
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }
        astNode* node =
            symtableGetFuncNode(globalTable, callNode->data.funcData.key);
        if (node == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
        symtable* callContext = NULL;
        callContext = symtableCreateFuncContext(globalTable);
        if (callContext == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
        *err = checkFunctionBody(node, callContext);
        stackDestroy(callContext);
        free(callContext);
        if (*err != 0)
        {
            return S_TYPE_UNKNOWN;
        }
        retType =
            symtableGetFuncReturn(globalTable, callNode->data.funcData.key);
    }
    callNode->retType = retType;
    return retType;
}


static dataType checkUnaryExpr(astNode* uNode, symtable* globalTable,
                               astNode* currentNode, Errors* err)
{
    if (uNode == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL || uNode->childCount < 1 || uNode->children == NULL ||
        uNode->children[0] == NULL)
    {
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }

    dataType type =
        checkExpr(uNode->children[0], globalTable, currentNode, err);
    if (type == S_TYPE_UNKNOWN || *err != 0)
    {
        return S_TYPE_UNKNOWN;
    }

    if ((uNode->data.opType == AST_OP_NEG ||
         uNode->data.opType == AST_OP_POS) &&
        (type == S_TYPE_INT || type == S_TYPE_DOUBLE))
    {
        uNode->retType = type;
        return type;
    }
    if (uNode->data.opType == AST_OP_NOT)
    {
        if (type == S_TYPE_BOOL)
        {
            uNode->retType = S_TYPE_BOOL;
            return S_TYPE_BOOL;
        }
        *err = ERROR_TYPE_MISMATCH;
        return S_TYPE_UNKNOWN;
    }
    *err = ERROR_TYPE_MISMATCH;
    return S_TYPE_UNKNOWN;
}