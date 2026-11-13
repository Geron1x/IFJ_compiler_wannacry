/**
 * @file semantics.c
 * @brief File for the implementaion of semantic analysis
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */


#include <stdlib.h>
#include <string.h>

#include "ast_internal.h"
#include "error.h"
#include "symtable.h"

static void analyzeEnd(astNode* node, bool* allReturn, bool* allUnit);
static dataType resolveBodyType(astNode* bodyNode, dataType declaredType,
                                Errors* err);
static Errors semanticsFirstPass(const astNode* prog, symtable* globalTable);
static Errors semanticsSecondPass(const astNode* prog, symtable* globalTable);
static Errors checkBlock(astNode* blockNode, symtable* globalTable,
                         astNode* currentFunc);
static Errors checkFunctionBody(astNode* funcNode, symtable* globalTable);

static Errors checkStatement(astNode* statNode, symtable* globalTable,
                             astNode* currentNode);

static dataType checkExpr(astNode* exprNode, symtable* globalTable,
                          Errors* err);
static Errors checkVarDef(astNode* varNode, symtable* globalTable);
static Errors checkSet(astNode* setNode, symtable* globalTable);
static Errors checkIf(astNode* ifNode, symtable* globalTable,
                      astNode* currentNode);
static Errors checkWhile(astNode* whileNode, symtable* globalTable,
                         astNode* currentNode);
static Errors checkReturn(astNode* returnNode, symtable* globalTable,
                          astNode* currentNode);

static dataType checkBinaryExpr(astNode* biNode, symtable* globalTable,
                                Errors* err);
static dataType checkUnaryExpr(astNode* uNode, symtable* globalTable,
                               Errors* err);
static dataType checkCallExpr(astNode* callNode, symtable* globalTable,
                              Errors* err);


Errors semanticsCheck(const astNode* prog)
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
static Errors semanticsFirstPass(const astNode* prog, symtable* globalTable)
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

            if (funcNode->retType != S_TYPE_UNIT)
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
static Errors semanticsSecondPass(const astNode* prog, symtable* globalTable)
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
    }
    return 0;
}
static Errors checkFunctionBody(astNode* funcNode, symtable* globalTable)
{
    if (symtableGetFuncIsChecked(globalTable, funcNode->data.funcData.key) ==
        true)
    {
        return 0;
    }


    symtableSetFuncIsAnalyzing(globalTable, funcNode->data.funcData.key, true);

    stackPush(globalTable);
    int paramCount = funcNode->childCount;
    Errors status = 0;
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

        case AST_STAT_VAR:
        case AST_STAT_SET:
        case AST_STAT_WHILE:
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

static Errors checkBlock(astNode* blockNode, symtable* globalTable,
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
static Errors checkStatement(astNode* statNode, symtable* globalTable,
                             astNode* currentNode)
{
    Errors status = 0;
    switch (statNode->nodeType)
    {
        case AST_STAT_VAR:
            status = checkVarDef(statNode, globalTable);
            break;
        case AST_STAT_SET:
            status = checkSet(statNode, globalTable);
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
        {
            Errors err = 0;
            dataType exprType = checkExpr(statNode, globalTable, &err);
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
        default:
            return ERROR_INTERNAL;
    }
    if (status != 0)
    {
        return status;
    }
    return 0;
}

static Errors checkVarDef(astNode* varNode, symtable* globalTable)
{
    if (varNode == NULL || varNode->childCount == 0)
    {
        return ERROR_INTERNAL;
    }
    Errors err = 0;
    dataType type = checkExpr(varNode->children[0], globalTable, &err);
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
            if (varNode->childCount > 0 && varNode->children[0] != NULL)
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
    return 0;
}
static dataType checkExpr(astNode* exprNode, symtable* globalTable, Errors* err)
{
    if (exprNode == NULL)
    {
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }
    switch (exprNode->nodeType)
    {
        case AST_EXPR_LITERAL:
            if (exprNode->retType == S_TYPE_UNKNOWN)
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
            return exprNode->retType;

        case AST_EXPR_VAR:
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
            return exprNode->retType;

        case AST_EXPR_BINARY:
            return checkBinaryExpr(exprNode, globalTable, err);

        case AST_EXPR_UNARY:
            return checkUnaryExpr(exprNode, globalTable, err);

        case AST_EXPR_CALL:
            return checkCallExpr(exprNode, globalTable, err);

        default:
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
    }
}


static Errors checkSet(astNode* setNode, symtable* globalTable)
{
    if (setNode == NULL || setNode->childCount < 1 ||
        setNode->children[0] == NULL || setNode->data.varData.key == NULL)
    {
        return ERROR_INTERNAL;
    }

    dataType type = symtableGetVarType(globalTable, setNode->data.varData.key);
    if (type == S_TYPE_UNKNOWN)
    {
        return ERROR_UNDEFINED_NAME;
    }
    if (symtableGetIsVarConst(globalTable, setNode->data.varData.key) == true)
    {
        return ERROR_IMMUTABLE_WRITE;
    }
    Errors err = 0;
    dataType expType = checkExpr(setNode->children[0], globalTable, &err);
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


static Errors checkIf(astNode* ifNode, symtable* globalTable,
                      astNode* currentNode)
{
    if (ifNode == NULL || currentNode == NULL || ifNode->children[0] == NULL)
    {
        return ERROR_INTERNAL;
    }
    Errors err = 0;
    dataType type = checkExpr(ifNode->children[0], globalTable, &err);
    if (err != 0)
    {
        return err;
    }
    if (type != S_TYPE_BOOL)
    {
        return ERROR_TYPE_MISMATCH;
    }
    if (ifNode->children[1] != NULL)
    {
        Errors status = stackPush(globalTable);
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
    }
    if (ifNode->childCount > 2 && ifNode->children[2] != NULL)
    {
        Errors status = stackPush(globalTable);
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


static Errors checkWhile(astNode* whileNode, symtable* globalTable,
                         astNode* currentNode)
{
    if (whileNode == NULL || globalTable == NULL || currentNode == NULL ||
        whileNode->childCount < 2 || whileNode->children == NULL ||
        whileNode->children[0] == NULL || whileNode->children[1] == NULL)
    {
        return ERROR_INTERNAL;
    }
    Errors err = 0;
    dataType type = checkExpr(whileNode->children[0], globalTable, &err);
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
static Errors checkReturn(astNode* returnNode, symtable* globalTable,
                          astNode* currentNode)
{
    if (returnNode == NULL || globalTable == NULL || currentNode == NULL ||
        currentNode->data.funcData.key == NULL)
    {
        return ERROR_INTERNAL;
    }

    return 0;
}


static dataType checkBinaryExpr(astNode* biNode, symtable* globalTable,
                                Errors* err)
{
    if (biNode == NULL || biNode->childCount < 2 || biNode->children == NULL ||
        biNode->children[0] == NULL || biNode->children[1] == NULL)
    {
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }

    dataType typeLeft = checkExpr(biNode->children[0], globalTable, err);
    if (typeLeft == S_TYPE_UNKNOWN)
    {
        return S_TYPE_UNKNOWN;
    }
    dataType typeRight = checkExpr(biNode->children[1], globalTable, err);
    if (typeRight == S_TYPE_UNKNOWN)
    {
        return S_TYPE_UNKNOWN;
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


static dataType checkCallExpr(astNode* callNode, symtable* globalTable,
                              Errors* err)
{
    if (callNode == NULL || globalTable == NULL || err == NULL ||
        callNode->data.funcData.key == NULL)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }

    return 0;
}


static dataType checkUnaryExpr(astNode* uNode, symtable* globalTable,
                               Errors* err)
{
    if (uNode == NULL || uNode->childCount < 1 || uNode->children == NULL ||
        uNode->children[0] == NULL)
    {
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }

    dataType type = checkExpr(uNode->children[0], globalTable, err);
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

    *err = ERROR_TYPE_MISMATCH;
    return S_TYPE_UNKNOWN;
}