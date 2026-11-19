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

static dataType checkExprWithExpected(astNode* expr,
                                      const typeDesc* expectedDesc,
                                      symtable* globalTable,
                                      astNode* currentNode, Errors mismatchCode,
                                      Errors* err);
static dataType checkMemberExpr(astNode* memberNode, symtable* globalTable,
                                astNode* currentNode, Errors* err);
static dataType checkRangeExpr(astNode* rangeNode, symtable* globalTable,
                               astNode* currentNode, Errors* err);
static dataType checkCallExprNode(astNode* callExprNode, symtable* globalTable,
                                  astNode* currentNode, Errors* err);
static int checkForEach(astNode* forNode, symtable* globalTable,
                        astNode* currentNode);
static dataType checkListLiteral(astNode* listNode, symtable* globalTable,
                                 astNode* currentNode, Errors* err);
static int resolveEmptyListLiteral(astNode* exprNode,
                                   const typeDesc* expectedDesc);
static bool adaptComplexDesc(const typeDesc* expected, astNode* exprNode);
static dataType checkLambdaExpr(astNode* lambdaNode, symtable* globalTable,
                                Errors* err);
static int resolveFuncVarReference(astNode* exprNode,
                                   const typeDesc* expectedDesc,
                                   symtable* globalTable);
static void typeDescArrayDestroy(typeDesc** arr, int count);
static void analyzeEnd(astNode* node, bool* allReturn, bool* allUnit);
static dataType resolveBodyType(astNode* bodyNode, dataType declaredType,
                                Errors* err);
static int semanticsFirstPass(const astNode* prog, symtable* globalTable);
static int semanticsSecondPass(const astNode* prog, symtable* globalTable);
static int checkBlock(astNode* blockNode, symtable* globalTable,
                      astNode* currentFunc, bool allowTrailingIfExpr,
                      const typeDesc* expectedRetDesc);
static int checkFunctionBody(astNode* funcNode, symtable* globalTable);

static int checkStatement(astNode* statNode, symtable* globalTable,
                          astNode* currentNode, bool isLastInExprBlock,
                          const typeDesc* expectedRetDesc);

static int checkVarDef(astNode* varNode, symtable* globalTable,
                       astNode* currentNode);
static int checkSet(astNode* setNode, symtable* globalTable,
                    astNode* currentNode);
static int checkIf(astNode* ifNode, symtable* globalTable, astNode* currentNode,
                   bool isLastInExprBlock, const typeDesc* expectedRetDesc);
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
                            astNode* currentNode,
                            const typeDesc* expectedRetDesc, Errors* err);
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

        /** if function node has descriptor types add them into symtable */
        if (funcNode->typeInfo != NULL)
        {
            status = symtableSetFuncReturnTypeDesc(
                globalTable, funcNode->data.funcData.key, funcNode->typeInfo);
            if (status != 0)
            {
                return status;
            }
        }

        /** if function node has parameters descriptors add them into symtable
         */
        for (int j = 1; j <= paramCount; j++)
        {
            if (funcNode->children[j]->typeInfo != NULL)
            {
                status = symtableSetFuncParamTypeDesc(
                    globalTable, funcNode->data.funcData.key, j - 1,
                    funcNode->children[j]->typeInfo);
                if (status != 0)
                {
                    return status;
                }
            }
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
        if (funcNode->children[i]->typeInfo != NULL)
        {
            status = symtableSetVarTypeDesc(
                globalTable, funcNode->children[i]->data.varData.key,
                funcNode->children[i]->typeInfo);
            if (status != 0)
            {
                stackDestroy(globalTable);
                return status;
            }
        }
        funcNode->children[i]->data.varData.id = symtableGetVarId(
            globalTable, funcNode->children[i]->data.varData.key);
    }
    const typeDesc* declaredDesc =
        symtableGetFuncReturnTypeDesc(globalTable, funcNode->data.funcData.key);
    status = checkBlock(funcNode->children[0], globalTable, funcNode, true,
                        declaredDesc);
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
        if (actualBodyType == S_TYPE_LIST || actualBodyType == S_TYPE_FUNC)
        {
            astNode* last = funcNode->children[0];
            if (last != NULL && last->nodeType == AST_BLOCK &&
                last->childCount > 0)
            {
                last = last->children[last->childCount - 1];
            }
            if (last != NULL && last->typeInfo != NULL)
            {
                typeDescDestroy(funcNode->typeInfo);
                funcNode->typeInfo = typeDescClone(last->typeInfo);
                status = symtableSetFuncReturnTypeDesc(
                    globalTable, funcNode->data.funcData.key, last->typeInfo);
                if (status != 0)
                {
                    return status;
                }
            }
        }
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

    bool bodyAllReturn = false;
    bool bodyAllUnit = false;
    analyzeEnd(funcNode->children[0], &bodyAllReturn, &bodyAllUnit);

    if (declaredDesc != NULL && actualBodyType != S_TYPE_UNIT && !bodyAllReturn)
    {
        astNode* last = funcNode->children[0];
        if (last != NULL && last->nodeType == AST_BLOCK && last->childCount > 0)
        {
            last = last->children[last->childCount - 1];
        }
        if (last != NULL &&
            (declaredType == S_TYPE_LIST || declaredType == S_TYPE_FUNC) &&
            !adaptComplexDesc(declaredDesc, last))
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
        case AST_STAT_FOR_EACH:
            *allReturn = false;
            *allUnit = true;
            return;

        case AST_EXPR_BINARY:
        case AST_EXPR_UNARY:
        case AST_EXPR_LITERAL:
        case AST_EXPR_VAR:
        case AST_EXPR_CALL:
        case AST_EXPR_LAMBDA:
        case AST_EXPR_LIST_LITERAL:
        case AST_EXPR_MEMBER:
        case AST_EXPR_CALL_EXPR:
            *allReturn = false;
            *allUnit = (lastStat->retType == S_TYPE_UNIT);
            return;
        case AST_STAT_IF:
        {
            if (lastStat->childCount <= 2 || lastStat->children[2] == NULL)
            {
                *allReturn = false;
                *allUnit = true;
                return;
            }
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
        case AST_EXPR_LAMBDA:
        case AST_EXPR_LIST_LITERAL:
        case AST_EXPR_MEMBER:
        case AST_EXPR_CALL_EXPR:
            return lastStat->retType;
        case AST_STAT_IF:
        {
            if (lastStat->childCount <= 2 || lastStat->children[2] == NULL)
            {
                if (declaredType == S_TYPE_UNKNOWN)
                {
                    return S_TYPE_UNIT;
                }
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
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
        case AST_STAT_FOR_EACH:
            return S_TYPE_UNIT;

        case AST_STAT_RETURN:
            if (declaredType == S_TYPE_UNKNOWN)
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
            return declaredType;


        default:
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
    }
}

/** Check all statements in one block */
static int checkBlock(astNode* blockNode, symtable* globalTable,
                      astNode* currentFunc, bool allowTrailingIfExpr,
                      const typeDesc* expectedRetDesc)
{
    if (blockNode == NULL || globalTable == NULL)
    {
        return ERROR_INTERNAL;
    }

    int statCount = blockNode->childCount;
    Errors status = 0;
    for (int i = 0; i < statCount; i++)
    {
        const typeDesc* statementExpectedDesc = NULL;
        if (allowTrailingIfExpr && i == statCount - 1)
        {
            statementExpectedDesc = expectedRetDesc;
        }
        status = checkStatement(
            blockNode->children[i], globalTable, currentFunc,
            allowTrailingIfExpr && i == statCount - 1, statementExpectedDesc);
        if (status != 0)
        {
            return status;
        }
    }

    return 0;
}
static int checkStatement(astNode* statNode, symtable* globalTable,
                          astNode* currentNode, bool isLastInExprBlock,
                          const typeDesc* expectedRetDesc)
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
            status = checkIf(statNode, globalTable, currentNode,
                             isLastInExprBlock, expectedRetDesc);
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
        case AST_EXPR_LAMBDA:
        case AST_EXPR_LIST_LITERAL:
        case AST_EXPR_MEMBER:
        case AST_EXPR_CALL_EXPR:
        {
            Errors err = 0;
            dataType exprType =
                checkExprWithExpected(statNode, expectedRetDesc, globalTable,
                                      currentNode, ERROR_TYPE_MISMATCH, &err);
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
        case AST_STAT_FOR_EACH:
            return checkForEach(statNode, globalTable, currentNode);
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
        varNode->childCount == 0 || varNode->children[0] == NULL)
    {
        return ERROR_INTERNAL;
    }

    astNode* initExpr = varNode->children[0];
    Errors err = 0;
    dataType type =
        checkExprWithExpected(initExpr, varNode->typeInfo, globalTable,
                              currentNode, ERROR_TYPE_MISMATCH, &err);
    if (err != 0)
    {
        return err;
    }

    /** check type compatability */
    if (varNode->retType != S_TYPE_UNKNOWN)
    {
        if (varNode->retType == type)
        {
            if (varNode->typeInfo != NULL)
            {
                if (initExpr->typeInfo == NULL ||
                    !typeDescEquals(varNode->typeInfo, initExpr->typeInfo))
                {
                    return ERROR_TYPE_MISMATCH;
                }
            }
        }
        else if (varNode->retType == S_TYPE_DOUBLE && type == S_TYPE_INT)
        {
            type = S_TYPE_DOUBLE;
            initExpr->int2float = true;
        }
        else
        {
            return ERROR_TYPE_MISMATCH;
        }
    }
    else
    {
        varNode->retType = type;
        if (initExpr->typeInfo != NULL)
        {
            typeDescDestroy(varNode->typeInfo);
            varNode->typeInfo = typeDescClone(initExpr->typeInfo);
            if (varNode->typeInfo == NULL)
            {
                return ERROR_INTERNAL;
            }
        }
    }

    err = stackInsertVar(globalTable, varNode->data.varData.key, type,
                         varNode->data.varData.isConst);
    if (err != 0)
    {
        return err;
    }
    varNode->data.varData.id =
        symtableGetVarId(globalTable, varNode->data.varData.key);

    if (varNode->typeInfo != NULL)
    {
        err = symtableSetVarTypeDesc(globalTable, varNode->data.varData.key,
                                     varNode->typeInfo);
        if (err != 0)
        {
            return err;
        }
    }

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
                return S_TYPE_UNKNOWN;
            }
            exprNode->data.varData.id =
                symtableGetVarId(globalTable, exprNode->data.varData.key);

            const typeDesc* varDesc =
                symtableGetVarTypeDesc(globalTable, exprNode->data.varData.key);
            if (varDesc != NULL)
            {
                typeDescDestroy(exprNode->typeInfo);
                exprNode->typeInfo = typeDescClone(varDesc);
                if (exprNode->typeInfo == NULL)
                {
                    *err = ERROR_INTERNAL;
                    return S_TYPE_UNKNOWN;
                }
            }
            return exprNode->retType;
        }
        case AST_EXPR_BINARY:
            return checkBinaryExpr(exprNode, globalTable, currentNode, err);

        case AST_EXPR_UNARY:
            return checkUnaryExpr(exprNode, globalTable, currentNode, err);

        case AST_EXPR_CALL:
            return checkCallExpr(exprNode, globalTable, currentNode, err);
        case AST_EXPR_IF:
            return checkIfExpr(exprNode, globalTable, currentNode, NULL, err);
        case AST_EXPR_LAMBDA:
            return checkLambdaExpr(exprNode, globalTable, err);
        case AST_EXPR_LIST_LITERAL:
            return checkListLiteral(exprNode, globalTable, currentNode, err);
        case AST_EXPR_MEMBER:
            return checkMemberExpr(exprNode, globalTable, currentNode, err);
        case AST_EXPR_RANGE:
            return checkRangeExpr(exprNode, globalTable, currentNode, err);
        case AST_EXPR_CALL_EXPR:
            return checkCallExprNode(exprNode, globalTable, currentNode, err);
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

    const typeDesc* varDesc =
        symtableGetVarTypeDesc(globalTable, setNode->data.varData.key);

    astNode* expr = setNode->children[0];

    dataType expType = checkExprWithExpected(
        expr, varDesc, globalTable, currentNode, ERROR_TYPE_MISMATCH, &err);
    if (err != 0)
    {
        return err;
    }

    if (type != expType)
    {
        if (type == S_TYPE_DOUBLE && expType == S_TYPE_INT)
        {
            expr->int2float = true;
        }
        else
        {
            return ERROR_TYPE_MISMATCH;
        }
    }
    else if (varDesc != NULL)
    {
        if (expr->typeInfo == NULL ||
            (!typeDescEquals(varDesc, expr->typeInfo) &&
             !adaptComplexDesc(varDesc, expr)))
        {
            return ERROR_TYPE_MISMATCH;
        }
    }
    return 0;
}


static int checkIf(astNode* ifNode, symtable* globalTable, astNode* currentNode,
                   bool isLastInExprBlock, const typeDesc* expectedRetDesc)
{
    if (ifNode == NULL || currentNode == NULL || ifNode->children[0] == NULL)
    {
        return ERROR_INTERNAL;
    }


    Errors status = 0;

    if (isLastInExprBlock && ifNode->childCount > 2 &&
        ifNode->children[2] != NULL)
    {
        checkIfExpr(ifNode, globalTable, currentNode, expectedRetDesc, &status);
        return status;
    }
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
    status =
        checkBlock(ifNode->children[1], globalTable, currentNode, false, NULL);
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
        status = checkBlock(ifNode->children[2], globalTable, currentNode,
                            false, NULL);
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
    status = checkBlock(whileNode->children[1], globalTable, currentNode, false,
                        NULL);
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
    status =
        checkBlock(forNode->children[2], globalTable, currentNode, false, NULL);
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
    if (returnNode == NULL || globalTable == NULL || currentNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    if (currentNode->nodeType == AST_EXPR_LAMBDA)
    {
        return ERROR_OTHER_SEMANTIC;
    }

    if (currentNode->data.funcData.key == NULL)
    {
        return ERROR_INTERNAL;
    }
    dataType expectedType =
        symtableGetFuncReturn(globalTable, currentNode->data.funcData.key);
    dataType type = S_TYPE_UNIT;
    Errors err = 0;
    if (returnNode->childCount > 0 && returnNode->children != NULL &&
        returnNode->children[0] != NULL)
    {
        astNode* retExpr = returnNode->children[0];
        const typeDesc* expectedDesc = symtableGetFuncReturnTypeDesc(
            globalTable, currentNode->data.funcData.key);

        type = checkExprWithExpected(retExpr, expectedDesc, globalTable,
                                     currentNode, ERROR_TYPE_MISMATCH, &err);
        if (err != 0)
        {
            return err;
        }

        /** if function returns unit the actual result of a function does not
         * matter */
        if (expectedType == S_TYPE_UNIT)
        {
            retExpr->retType = type;
            returnNode->retType = S_TYPE_UNIT;
            return 0;
        }

        if (type == S_TYPE_INT && expectedType == S_TYPE_DOUBLE)
        {
            type = S_TYPE_DOUBLE;
            retExpr->int2float = true;
        }
        else if (type == expectedType && expectedDesc != NULL)
        {

            if (retExpr->typeInfo == NULL ||
                (!typeDescEquals(expectedDesc, retExpr->typeInfo) &&
                 !adaptComplexDesc(expectedDesc, retExpr)))
            {
                return ERROR_TYPE_MISMATCH;
            }
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
                            astNode* currentNode,
                            const typeDesc* expectedRetDesc, Errors* err)
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
    status = checkBlock(ifNode->children[1], globalTable, currentNode, true,
                        expectedRetDesc);
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
    status = checkBlock(ifNode->children[2], globalTable, currentNode, true,
                        expectedRetDesc);
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
        if ((ifNode->retType == S_TYPE_LIST ||
             ifNode->retType == S_TYPE_FUNC) &&
            elseLast != NULL && elseLast->typeInfo != NULL)
        {
            typeDescDestroy(ifNode->typeInfo);
            ifNode->typeInfo = typeDescClone(elseLast->typeInfo);
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
        if ((ifNode->retType == S_TYPE_LIST ||
             ifNode->retType == S_TYPE_FUNC) &&
            thenLast != NULL && thenLast->typeInfo != NULL)
        {
            typeDescDestroy(ifNode->typeInfo);
            ifNode->typeInfo = typeDescClone(thenLast->typeInfo);
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
        if (thenType == S_TYPE_LIST || thenType == S_TYPE_FUNC)
        {
            if (thenLast == NULL || elseLast == NULL ||
                thenLast->typeInfo == NULL || elseLast->typeInfo == NULL ||
                !typeDescEquals(thenLast->typeInfo, elseLast->typeInfo))
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
            typeDescDestroy(ifNode->typeInfo);
            ifNode->typeInfo = typeDescClone(thenLast->typeInfo);
            if (ifNode->typeInfo == NULL)
            {
                *err = ERROR_INTERNAL;
                return S_TYPE_UNKNOWN;
            }
        }
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
    /** ++ check */
    if (biNode->data.opType == AST_OP_CONCAT)
    {
        if (typeLeft != S_TYPE_LIST || typeRight != S_TYPE_LIST ||
            biNode->children[0]->typeInfo == NULL ||
            biNode->children[1]->typeInfo == NULL ||
            !typeDescEquals(biNode->children[0]->typeInfo,
                            biNode->children[1]->typeInfo))
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }
        typeDescDestroy(biNode->typeInfo);
        biNode->typeInfo = typeDescClone(biNode->children[0]->typeInfo);
        if (biNode->typeInfo == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
        biNode->retType = S_TYPE_LIST;
        return S_TYPE_LIST;
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
static dataType checkLambdaExpr(astNode* lambdaNode, symtable* globalTable,
                                Errors* err)
{
    if (lambdaNode == NULL || globalTable == NULL || err == NULL ||
        lambdaNode->childCount < 1 || lambdaNode->children == NULL ||
        lambdaNode->children[0] == NULL ||
        lambdaNode->children[0]->nodeType != AST_BLOCK)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }

    /** create new table with function tree but empty scopes to ensure
     * isolaction of lambda function  */
    symtable* lambdaTable = symtableCreateFuncContext(globalTable);
    if (lambdaTable == NULL)
    {
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }
    Errors status = stackPush(lambdaTable);
    if (status != 0)
    {
        free(lambdaTable);
        *err = status;
        return S_TYPE_UNKNOWN;
    }

    int paramCount = lambdaNode->childCount - 1;
    typeDesc** paramDescs = NULL;
    if (paramCount > 0)
    {
        paramDescs = calloc((size_t)paramCount, sizeof(typeDesc*));
        if (paramDescs == NULL)
        {
            stackDestroy(lambdaTable);
            free(lambdaTable);
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
    }

    /** insert parameters of lambda function into symtable with (if present)
     * descriptor types */
    for (int i = 1; i < paramCount + 1; i++)
    {
        astNode* param = lambdaNode->children[i];
        if (param == NULL || param->data.varData.key == NULL ||
            param->retType == S_TYPE_UNKNOWN)
        {
            typeDescArrayDestroy(paramDescs, paramCount);
            stackDestroy(lambdaTable);
            free(lambdaTable);
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }

        status = stackInsertVar(lambdaTable, param->data.varData.key,
                                param->retType, true);
        if (status != 0)
        {
            typeDescArrayDestroy(paramDescs, paramCount);
            stackDestroy(lambdaTable);
            free(lambdaTable);
            *err = status;
            return S_TYPE_UNKNOWN;
        }

        param->data.varData.id =
            symtableGetVarId(lambdaTable, param->data.varData.key);

        if (param->typeInfo != NULL)
        {
            status = symtableSetVarTypeDesc(
                lambdaTable, param->data.varData.key, param->typeInfo);
            if (status != 0)
            {
                typeDescArrayDestroy(paramDescs, paramCount);
                stackDestroy(lambdaTable);
                free(lambdaTable);
                *err = status;
                return S_TYPE_UNKNOWN;
            }
            paramDescs[i - 1] = typeDescClone(param->typeInfo);
        }
        else
        {
            paramDescs[i - 1] = typeCreatePrimitive(param->retType);
        }

        if (paramDescs[i - 1] == NULL)
        {
            typeDescArrayDestroy(paramDescs, paramCount);
            stackDestroy(lambdaTable);
            free(lambdaTable);
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
    }

    /** check the body of lambda function with address to itself */
    astNode* body = lambdaNode->children[0];
    status = checkBlock(body, lambdaTable, lambdaNode, true, NULL);
    if (status != 0)
    {
        typeDescArrayDestroy(paramDescs, paramCount);
        stackDestroy(lambdaTable);
        free(lambdaTable);
        *err = status;
        return S_TYPE_UNKNOWN;
    }

    /** get return type for lambda from checking the last statement/expression
     */
    dataType retType = resolveBodyType(body, S_TYPE_UNKNOWN, &status);
    if (status != 0 || retType == S_TYPE_UNKNOWN)
    {
        typeDescArrayDestroy(paramDescs, paramCount);
        stackDestroy(lambdaTable);
        free(lambdaTable);
        if (status != 0)
        {
            *err = status;
        }
        else
        {
            *err = ERROR_TYPE_MISMATCH;
        }
        return S_TYPE_UNKNOWN;
    }
    typeDesc* retDesc = NULL;
    if ((retType == S_TYPE_LIST || retType == S_TYPE_FUNC) &&
        body->childCount > 0 && body->children[body->childCount - 1] != NULL &&
        body->children[body->childCount - 1]->typeInfo != NULL)
    {
        retDesc = typeDescClone(body->children[body->childCount - 1]->typeInfo);
    }
    else
    {
        retDesc = typeCreatePrimitive(retType);
    }

    if (retDesc == NULL)
    {
        typeDescArrayDestroy(paramDescs, paramCount);
        stackDestroy(lambdaTable);
        free(lambdaTable);
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }
    /** assembly lambdas's type descriptor and insert it into ast*/
    typeDesc* lambdaDesc = typeCreateFunc(paramCount, paramDescs, retDesc);
    stackDestroy(lambdaTable);
    free(lambdaTable);

    if (lambdaDesc == NULL)
    {
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }

    typeDescDestroy(lambdaNode->typeInfo);
    lambdaNode->typeInfo = lambdaDesc;
    lambdaNode->retType = S_TYPE_FUNC;
    return S_TYPE_FUNC;
}
/** check whether key of some identifier is not a variable but a function */
static int resolveFuncVarReference(astNode* exprNode,
                                   const typeDesc* expectedDesc,
                                   symtable* globalTable)
{
    if (exprNode == NULL || exprNode->nodeType != AST_EXPR_VAR ||
        exprNode->data.varData.key == NULL)
    {
        return ERROR_INTERNAL;
    }

    const char* key = exprNode->data.varData.key;
    int paramCount = symtableGetFuncParamCount(globalTable, key);
    if (paramCount == -1)
    {
        /** such function was not found */
        return ERROR_UNDEFINED_NAME;
    }

    if (expectedDesc == NULL || expectedDesc->type != S_TYPE_FUNC)
    {
        return ERROR_TYPE_MISMATCH;
    }

    if (expectedDesc->func.paramCount != paramCount)
    {
        return ERROR_TYPE_MISMATCH;
    }

    /** if regular function still does not have a return type find it */
    dataType funcRetType = symtableGetFuncReturn(globalTable, key);
    if (funcRetType == S_TYPE_UNKNOWN)
    {
        if (symtableGetFuncIsAnalyzing(globalTable, key))
        {
            return ERROR_TYPE_MISMATCH;
        }
        astNode* funcNode = symtableGetFuncNode(globalTable, key);
        if (funcNode == NULL)
        {
            return ERROR_INTERNAL;
        }
        symtable* callContext = symtableCreateFuncContext(globalTable);
        if (callContext == NULL)
        {
            return ERROR_INTERNAL;
        }
        Errors status = checkFunctionBody(funcNode, callContext);
        stackDestroy(callContext);
        free(callContext);
        if (status != 0)
        {
            return status;
        }
        funcRetType = symtableGetFuncReturn(globalTable, key);
    }

    /** check return type */
    const typeDesc* funcRetDesc =
        symtableGetFuncReturnTypeDesc(globalTable, key);
    if (funcRetDesc != NULL)
    {
        if (!typeDescEquals(funcRetDesc, expectedDesc->func.returnType))
        {
            return ERROR_TYPE_MISMATCH;
        }
    }
    else if (expectedDesc->func.returnType == NULL ||
             expectedDesc->func.returnType->type != funcRetType)
    {
        return ERROR_TYPE_MISMATCH;
    }

    /** check types of parameters */
    for (int i = 0; i < paramCount; i++)
    {
        dataType pType = symtableGetFuncParamType(globalTable, key, i);
        const typeDesc* paramDesc =
            symtableGetFuncParamTypeDesc(globalTable, key, i);
        const typeDesc* expParam = expectedDesc->func.paramTypes[i];

        if (expParam == NULL)
        {
            return ERROR_TYPE_MISMATCH;
        }
        if (paramDesc != NULL)
        {
            if (!typeDescEquals(paramDesc, expParam))
            {
                return ERROR_TYPE_MISMATCH;
            }
        }
        else if (pType != expParam->type)
        {
            return ERROR_TYPE_MISMATCH;
        }
    }

    /** insert information in ast */
    typeDescDestroy(exprNode->typeInfo);
    exprNode->typeInfo = typeDescClone(expectedDesc);
    exprNode->retType = S_TYPE_FUNC;
    return 0;
}
static void typeDescArrayDestroy(typeDesc** arr, int count)
{
    if (arr == NULL)
    {
        return;
    }
    for (int i = 0; i < count; i++)
    {
        typeDescDestroy(arr[i]);
    }
    free(arr);
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
    const char* callerKey = callNode->data.funcData.key;

    /** check if it is a functional variable */
    dataType localVarType = symtableGetVarType(globalTable, callerKey);
    if (localVarType != S_TYPE_UNKNOWN)
    {
        if (localVarType == S_TYPE_LIST)
        {
            if (callNode->childCount != 1)
            {
                *err = ERROR_WRONG_ARGUMENTS;
                return S_TYPE_UNKNOWN;
            }
            dataType idxType =
                checkExpr(callNode->children[0], globalTable, currentNode, err);
            if (*err != 0)
            {
                return S_TYPE_UNKNOWN;
            }
            if (idxType != S_TYPE_INT)
            {
                *err = ERROR_WRONG_ARGUMENTS;
                return S_TYPE_UNKNOWN;
            }

            const typeDesc* listDesc =
                symtableGetVarTypeDesc(globalTable, callerKey);
            if (listDesc == NULL || listDesc->type != S_TYPE_LIST ||
                listDesc->elemType == NULL)
            {
                *err = ERROR_INTERNAL;
                return S_TYPE_UNKNOWN;
            }

            dataType elemType = listDesc->elemType->type;
            if (elemType == S_TYPE_LIST || elemType == S_TYPE_FUNC)
            {
                typeDescDestroy(callNode->typeInfo);
                callNode->typeInfo = typeDescClone(listDesc->elemType);
                if (callNode->typeInfo == NULL)
                {
                    *err = ERROR_INTERNAL;
                    return S_TYPE_UNKNOWN;
                }
            }
            callNode->retType = elemType;
            return elemType;
        }

        if (localVarType != S_TYPE_FUNC)
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }

        const typeDesc* funcDesc =
            symtableGetVarTypeDesc(globalTable, callerKey);
        if (funcDesc == NULL || funcDesc->type != S_TYPE_FUNC)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }

        if (funcDesc->func.paramCount != callNode->childCount)
        {
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }

        for (int i = 0; i < funcDesc->func.paramCount; i++)
        {
            astNode* arg = callNode->children[i];
            const typeDesc* expParam = funcDesc->func.paramTypes[i];
            dataType argType =
                checkExprWithExpected(arg, expParam, globalTable, currentNode,
                                      ERROR_WRONG_ARGUMENTS, err);
            if (*err != 0)
            {
                return S_TYPE_UNKNOWN;
            }

            if (expParam->type != S_TYPE_ANY)
            {
                if (expParam->type != argType)
                {
                    if (argType == S_TYPE_INT &&
                        expParam->type == S_TYPE_DOUBLE)
                    {
                        arg->int2float = true;
                    }
                    else
                    {
                        *err = ERROR_WRONG_ARGUMENTS;
                        return S_TYPE_UNKNOWN;
                    }
                }
                else if ((expParam->type == S_TYPE_FUNC ||
                          expParam->type == S_TYPE_LIST) &&
                         (!typeDescEquals(expParam, arg->typeInfo) &&
                          !adaptComplexDesc(expParam, arg)))
                {
                    *err = ERROR_WRONG_ARGUMENTS;
                    return S_TYPE_UNKNOWN;
                }
            }
        }

        dataType retType = S_TYPE_UNIT;
        if (funcDesc->func.returnType != NULL)
        {
            retType = funcDesc->func.returnType->type;
        }
        if (funcDesc->func.returnType != NULL &&
            (retType == S_TYPE_FUNC || retType == S_TYPE_LIST))
        {
            typeDescDestroy(callNode->typeInfo);
            callNode->typeInfo = typeDescClone(funcDesc->func.returnType);
        }
        callNode->retType = retType;
        return retType;
    }

    /** call of a regular function*/
    int paramCount = symtableGetFuncParamCount(globalTable, callerKey);
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

    for (int i = 0; i < paramCount; i++)
    {
        astNode* arg = callNode->children[i];
        dataType expectedType =
            symtableGetFuncParamType(globalTable, callerKey, i);
        const typeDesc* expectedDesc =
            symtableGetFuncParamTypeDesc(globalTable, callerKey, i);
        dataType type =
            checkExprWithExpected(arg, expectedDesc, globalTable, currentNode,
                                  ERROR_WRONG_ARGUMENTS, err);
        if (*err != 0)
        {
            return S_TYPE_UNKNOWN;
        }

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
                    arg->int2float = true;
                }
                else
                {
                    *err = ERROR_WRONG_ARGUMENTS;
                    return S_TYPE_UNKNOWN;
                }
            }
            else if (expectedDesc != NULL)
            {
                if (arg->typeInfo == NULL ||
                    !adaptComplexDesc(expectedDesc, arg))
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

    const typeDesc* retDesc =
        symtableGetFuncReturnTypeDesc(globalTable, callerKey);
    if (retDesc != NULL)
    {
        typeDescDestroy(callNode->typeInfo);
        callNode->typeInfo = typeDescClone(retDesc);
    }

    callNode->retType = retType;
    return retType;
}

/** Adapt compatible complex descriptors and mark implicit conversions */
static bool adaptComplexDesc(const typeDesc* expected, astNode* exprNode)
{
    if (expected == NULL || exprNode == NULL || exprNode->typeInfo == NULL)
    {
        return false;
    }

    if (typeDescEquals(expected, exprNode->typeInfo))
    {
        return true;
    }

    /** check implicit conversion to double */
    if (exprNode->nodeType == AST_EXPR_LAMBDA &&
        expected->type == S_TYPE_FUNC &&
        exprNode->typeInfo->type == S_TYPE_FUNC &&
        expected->func.paramCount == exprNode->typeInfo->func.paramCount)
    {
        for (int i = 0; i < expected->func.paramCount; i++)
        {
            if (!typeDescEquals(expected->func.paramTypes[i],
                                exprNode->typeInfo->func.paramTypes[i]))
            {
                return false;
            }
        }

        if (expected->func.returnType != NULL &&
            exprNode->typeInfo->func.returnType != NULL &&
            expected->func.returnType->type == S_TYPE_DOUBLE &&
            exprNode->typeInfo->func.returnType->type == S_TYPE_INT)
        {
            astNode* body = exprNode->children[0];
            if (body != NULL && body->nodeType == AST_BLOCK &&
                body->childCount > 0)
            {
                astNode* last = body->children[body->childCount - 1];
                if (last != NULL)
                {
                    last->int2float = true;
                }
            }
            exprNode->typeInfo->func.returnType->type = S_TYPE_DOUBLE;
            return true;
        }
    }

    if (exprNode->nodeType == AST_EXPR_LIST_LITERAL &&
        expected->type == S_TYPE_LIST &&
        exprNode->typeInfo->type == S_TYPE_LIST && expected->elemType != NULL &&
        exprNode->typeInfo->elemType != NULL &&
        expected->elemType->type == S_TYPE_DOUBLE &&
        exprNode->typeInfo->elemType->type == S_TYPE_INT)
    {
        for (int i = 0; i < exprNode->childCount; i++)
        {
            if (exprNode->children[i] != NULL &&
                exprNode->children[i]->retType == S_TYPE_INT)
            {
                exprNode->children[i]->int2float = true;
            }
        }
        exprNode->typeInfo->elemType->type = S_TYPE_DOUBLE;
        return true;
    }

    return false;
}

static dataType checkExprWithExpected(astNode* expr,
                                      const typeDesc* expectedDesc,
                                      symtable* globalTable,
                                      astNode* currentNode, Errors mismatchCode,
                                      Errors* err)
{
    if (expr == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }

    *err = resolveEmptyListLiteral(expr, expectedDesc);
    if (*err != 0)
    {
        if (*err == ERROR_TYPE_MISMATCH)
        {
            *err = mismatchCode;
        }
        return S_TYPE_UNKNOWN;
    }

    if (expr->nodeType == AST_EXPR_VAR && expr->data.varData.key != NULL &&
        symtableGetVarType(globalTable, expr->data.varData.key) ==
            S_TYPE_UNKNOWN)
    {
        *err = resolveFuncVarReference(expr, expectedDesc, globalTable);
        if (*err != 0)
        {
            if (*err == ERROR_TYPE_MISMATCH)
            {
                *err = mismatchCode;
            }
            return S_TYPE_UNKNOWN;
        }
        return S_TYPE_FUNC;
    }

    dataType type = checkExpr(expr, globalTable, currentNode, err);
    if (*err != 0 || type == S_TYPE_UNKNOWN)
    {
        if (*err == 0)
        {
            *err = ERROR_TYPE_MISMATCH;
        }
        return S_TYPE_UNKNOWN;
    }

    if (expectedDesc != NULL &&
        (expectedDesc->type == S_TYPE_LIST ||
         expectedDesc->type == S_TYPE_FUNC) &&
        type == expectedDesc->type && !adaptComplexDesc(expectedDesc, expr))
    {
        *err = mismatchCode;
        return S_TYPE_UNKNOWN;
    }
    return type;
}

static int resolveEmptyListLiteral(astNode* exprNode,
                                   const typeDesc* expectedDesc)
{
    if (exprNode == NULL || exprNode->nodeType != AST_EXPR_LIST_LITERAL ||
        exprNode->childCount > 0 || exprNode->typeInfo != NULL)
    {
        return 0; /** list was not empty */
    }

    if (expectedDesc == NULL || expectedDesc->type != S_TYPE_LIST ||
        expectedDesc->elemType == NULL)
    {
        return ERROR_TYPE_MISMATCH; /** empty list that does not have compatable
                                       type */
    }

    exprNode->typeInfo = typeDescClone(expectedDesc);
    if (exprNode->typeInfo == NULL)
    {
        return ERROR_INTERNAL;
    }
    exprNode->retType = S_TYPE_LIST;
    return 0;
}
static dataType checkListLiteral(astNode* listNode, symtable* globalTable,
                                 astNode* currentNode, Errors* err)
{
    if (listNode == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }

    /** check whether empty list's type was given by
     * resolveEmptyListLiteral fucntion
     */
    if (listNode->childCount == 0)
    {
        if (listNode->typeInfo != NULL &&
            listNode->typeInfo->type == S_TYPE_LIST)
        {
            listNode->retType = S_TYPE_LIST;
            return S_TYPE_LIST;
        }
        *err = ERROR_TYPE_MISMATCH;
        return S_TYPE_UNKNOWN;
    }

    dataType elemType = S_TYPE_UNKNOWN;
    const typeDesc* elemDescRef = NULL;
    bool hasInt = false;
    bool hasDouble = false;

    for (int i = 0; i < listNode->childCount; i++)
    {
        astNode* elem = listNode->children[i];
        if (elem == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }

        /** if first element of the list was identified
         *  allow the use of empty lists
         */
        if (elemDescRef != NULL)
        {
            *err = resolveEmptyListLiteral(elem, elemDescRef);
            if (*err != 0)
            {
                return S_TYPE_UNKNOWN;
            }
        }

        dataType type = checkExpr(elem, globalTable, currentNode, err);
        if (*err != 0 || type == S_TYPE_UNKNOWN)
        {
            if (*err == 0)
            {
                *err = ERROR_TYPE_MISMATCH;
            }
            return S_TYPE_UNKNOWN;
        }

        if (i == 0)
        {
            elemType = type;
            elemDescRef = elem->typeInfo;
            if (type == S_TYPE_INT)
            {
                hasInt = true;
            }
            if (type == S_TYPE_DOUBLE)
            {
                hasDouble = true;
            }
        }
        else
        {
            if (type == elemType)
            {
                if (elemDescRef != NULL)
                {
                    if (elem->typeInfo == NULL ||
                        !typeDescEquals(elemDescRef, elem->typeInfo))
                    {
                        *err = ERROR_TYPE_MISMATCH;
                        return S_TYPE_UNKNOWN;
                    }
                }
            }
            else if ((elemType == S_TYPE_INT && type == S_TYPE_DOUBLE) ||
                     (elemType == S_TYPE_DOUBLE && type == S_TYPE_INT))
            {
                elemType = S_TYPE_DOUBLE;
                hasInt = true;
                hasDouble = true;
            }
            else
            {
                *err = ERROR_TYPE_MISMATCH;
                return S_TYPE_UNKNOWN;
            }
        }
    }

    /** if in list values are mixed (ints and doubles) convert them all into
     * double */
    if (hasInt && hasDouble)
    {
        for (int i = 0; i < listNode->childCount; i++)
        {
            if (listNode->children[i]->retType == S_TYPE_INT)
            {
                listNode->children[i]->int2float = true;
            }
        }
    }

    typeDesc* elemDesc = NULL;
    if (elemDescRef != NULL)
    {
        elemDesc = typeDescClone(elemDescRef);
    }
    else
    {
        elemDesc = typeCreatePrimitive(elemType);
    }
    if (elemDesc == NULL)
    {
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }

    typeDesc* listDesc = typeCreateList(elemDesc);
    if (listDesc == NULL)
    {
        *err = ERROR_INTERNAL;
        return S_TYPE_UNKNOWN;
    }

    typeDescDestroy(listNode->typeInfo);
    listNode->typeInfo = listDesc;
    listNode->retType = S_TYPE_LIST;
    return S_TYPE_LIST;
}
static int checkForEach(astNode* forNode, symtable* globalTable,
                        astNode* currentNode)
{
    if (forNode == NULL || globalTable == NULL || currentNode == NULL ||
        forNode->data.forData.key == NULL || forNode->childCount < 2 ||
        forNode->children == NULL || forNode->children[0] == NULL ||
        forNode->children[1] == NULL)
    {
        return ERROR_INTERNAL;
    }

    Errors err = 0;
    astNode* iterable = forNode->children[0];
    dataType iterType = checkExpr(iterable, globalTable, currentNode, &err);
    if (err != 0)
    {
        return err;
    }
    if (iterType != S_TYPE_LIST || iterable->typeInfo == NULL ||
        iterable->typeInfo->type != S_TYPE_LIST ||
        iterable->typeInfo->elemType == NULL)
    {
        return ERROR_TYPE_MISMATCH;
    }

    const typeDesc* elemDesc = iterable->typeInfo->elemType;
    err = stackPush(globalTable);
    if (err != 0)
    {
        return err;
    }

    err = stackInsertVar(globalTable, forNode->data.forData.key, elemDesc->type,
                         true);
    if (err != 0)
    {
        stackPop(globalTable);
        return err;
    }

    if (elemDesc->type == S_TYPE_LIST || elemDesc->type == S_TYPE_FUNC)
    {
        err = symtableSetVarTypeDesc(globalTable, forNode->data.forData.key,
                                     elemDesc);
        if (err != 0)
        {
            stackPop(globalTable);
            return err;
        }
    }

    forNode->data.forData.id =
        symtableGetVarId(globalTable, forNode->data.forData.key);

    err =
        checkBlock(forNode->children[1], globalTable, currentNode, false, NULL);
    stackPop(globalTable);
    return err;
}

static dataType checkCallExprNode(astNode* callExprNode, symtable* globalTable,
                                  astNode* currentNode, Errors* err)
{
    if (callExprNode == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL || callExprNode->childCount < 1 ||
        callExprNode->children == NULL || callExprNode->children[0] == NULL)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }

    astNode* caller = callExprNode->children[0];
    dataType callerType = checkExpr(caller, globalTable, currentNode, err);
    if (*err != 0 || callerType == S_TYPE_UNKNOWN)
    {
        if (*err == 0) *err = ERROR_TYPE_MISMATCH;
        return S_TYPE_UNKNOWN;
    }

    int argCount = callExprNode->childCount - 1;

    /* 1. Result indexetion of expression of type list[T]*/
    if (callerType == S_TYPE_LIST)
    {
        if (argCount != 1)
        {
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }
        dataType idxType =
            checkExpr(callExprNode->children[1], globalTable, currentNode, err);
        if (*err != 0)
        {
            return S_TYPE_UNKNOWN;
        }
        if (idxType != S_TYPE_INT)
        {
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }

        if (caller->typeInfo == NULL || caller->typeInfo->type != S_TYPE_LIST ||
            caller->typeInfo->elemType == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }

        dataType elemType = caller->typeInfo->elemType->type;
        if (elemType == S_TYPE_LIST || elemType == S_TYPE_FUNC)
        {
            typeDescDestroy(callExprNode->typeInfo);
            callExprNode->typeInfo = typeDescClone(caller->typeInfo->elemType);
        }
        callExprNode->retType = elemType;
        return elemType;
    }
    /** Call to the result of funtional type */
    if (callerType == S_TYPE_FUNC)
    {
        const typeDesc* funcDesc = caller->typeInfo;
        if (funcDesc == NULL || funcDesc->type != S_TYPE_FUNC)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }

        if (funcDesc->func.paramCount != argCount)
        {
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }

        for (int i = 0; i < argCount; i++)
        {
            astNode* arg = callExprNode->children[i + 1];
            const typeDesc* expParam = funcDesc->func.paramTypes[i];
            dataType argType =
                checkExprWithExpected(arg, expParam, globalTable, currentNode,
                                      ERROR_WRONG_ARGUMENTS, err);
            if (*err != 0)
            {
                return S_TYPE_UNKNOWN;
            }

            if (expParam->type != S_TYPE_ANY)
            {
                if (expParam->type != argType)
                {
                    if (argType == S_TYPE_INT &&
                        expParam->type == S_TYPE_DOUBLE)
                    {
                        arg->int2float = true;
                    }
                    else
                    {
                        *err = ERROR_WRONG_ARGUMENTS;
                        return S_TYPE_UNKNOWN;
                    }
                }
                else if ((expParam->type == S_TYPE_FUNC ||
                          expParam->type == S_TYPE_LIST) &&
                         (!typeDescEquals(expParam, arg->typeInfo) &&
                          !adaptComplexDesc(expParam, arg)))
                {
                    *err = ERROR_WRONG_ARGUMENTS;
                    return S_TYPE_UNKNOWN;
                }
            }
        }

        dataType retType = S_TYPE_UNIT;
        if (funcDesc->func.returnType != NULL)
        {
            retType = funcDesc->func.returnType->type;
        }
        if (funcDesc->func.returnType != NULL &&
            (retType == S_TYPE_FUNC || retType == S_TYPE_LIST))
        {
            typeDescDestroy(callExprNode->typeInfo);
            callExprNode->typeInfo = typeDescClone(funcDesc->func.returnType);
        }
        callExprNode->retType = retType;
        return retType;
    }

    *err = ERROR_TYPE_MISMATCH;
    return S_TYPE_UNKNOWN;
}
static dataType checkRangeExpr(astNode* rangeNode, symtable* globalTable,
                               astNode* currentNode, Errors* err)
{
    if (rangeNode == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL || rangeNode->childCount < 2 ||
        rangeNode->children == NULL || rangeNode->children[0] == NULL ||
        rangeNode->children[1] == NULL)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }

    dataType fromType =
        checkExpr(rangeNode->children[0], globalTable, currentNode, err);
    if (*err != 0)
    {
        return S_TYPE_UNKNOWN;
    }
    if (fromType != S_TYPE_INT)
    {
        *err = ERROR_TYPE_MISMATCH;
        return S_TYPE_UNKNOWN;
    }

    dataType endType =
        checkExpr(rangeNode->children[1], globalTable, currentNode, err);
    if (*err != 0)
    {
        return S_TYPE_UNKNOWN;
    }
    if (endType != S_TYPE_INT)
    {
        *err = ERROR_TYPE_MISMATCH;
        return S_TYPE_UNKNOWN;
    }

    if (rangeNode->data.rangeData.hasStep)
    {
        if (rangeNode->childCount < 3 || rangeNode->children[2] == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
        astNode* stepNode = rangeNode->children[2];
        dataType stepType = checkExpr(stepNode, globalTable, currentNode, err);
        if (*err != 0)
        {
            return S_TYPE_UNKNOWN;
        }
        if (stepType != S_TYPE_INT)
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }
        if (stepNode->nodeType == AST_EXPR_LITERAL &&
            stepNode->data.intVal == 0)
        {
            *err = ERROR_OTHER_SEMANTIC;
            return S_TYPE_UNKNOWN;
        }
    }

    rangeNode->retType = S_TYPE_UNIT;
    return S_TYPE_UNIT;
}
static dataType checkMemberExpr(astNode* memberNode, symtable* globalTable,
                                astNode* currentNode, Errors* err)
{
    if (memberNode == NULL || globalTable == NULL || currentNode == NULL ||
        err == NULL || memberNode->data.memberData.key == NULL ||
        memberNode->childCount < 1 || memberNode->children == NULL ||
        memberNode->children[0] == NULL)
    {
        if (err != NULL)
        {
            *err = ERROR_INTERNAL;
        }
        return S_TYPE_UNKNOWN;
    }

    const char* memberName = memberNode->data.memberData.key;
    bool hasArgs = memberNode->data.memberData.hasArgs;
    int argCount = memberNode->childCount - 1;
    astNode* receiver = memberNode->children[0];

    /** static call List.range(start, end, step?) */
    if (receiver->nodeType == AST_EXPR_VAR &&
        receiver->data.varData.key != NULL &&
        strcmp(receiver->data.varData.key, "List") == 0 &&
        symtableGetVarType(globalTable, "List") == S_TYPE_UNKNOWN)
    {
        if (strcmp(memberName, "range") != 0 || !hasArgs)
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }
        if (argCount != 2 && argCount != 3)
        {
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }
        for (int i = 1; i <= argCount; i++)
        {
            dataType aType = checkExpr(memberNode->children[i], globalTable,
                                       currentNode, err);
            if (*err != 0)
            {
                return S_TYPE_UNKNOWN;
            }
            if (aType != S_TYPE_INT)
            {
                *err = ERROR_WRONG_ARGUMENTS;
                return S_TYPE_UNKNOWN;
            }
        }
        if (argCount == 3 &&
            memberNode->children[3]->nodeType == AST_EXPR_LITERAL &&
            memberNode->children[3]->data.intVal == 0)
        {
            *err = ERROR_OTHER_SEMANTIC;
            return S_TYPE_UNKNOWN;
        }

        typeDesc* listDesc = typeCreateList(typeCreatePrimitive(S_TYPE_INT));
        if (listDesc == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
        typeDescDestroy(memberNode->typeInfo);
        memberNode->typeInfo = listDesc;
        memberNode->retType = S_TYPE_LIST;
        return S_TYPE_LIST;
    }

    /** Range member (a to b).toList */
    if (receiver->nodeType == AST_EXPR_RANGE)
    {
        checkRangeExpr(receiver, globalTable, currentNode, err);
        if (*err != 0)
        {
            return S_TYPE_UNKNOWN;
        }
        if (strcmp(memberName, "toList") != 0 || hasArgs || argCount != 0)
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }

        typeDesc* listDesc = typeCreateList(typeCreatePrimitive(S_TYPE_INT));
        if (listDesc == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
        typeDescDestroy(memberNode->typeInfo);
        memberNode->typeInfo = listDesc;
        memberNode->retType = S_TYPE_LIST;
        return S_TYPE_LIST;
    }

    /** methods and members of List[T] */
    dataType recvType = checkExpr(receiver, globalTable, currentNode, err);
    if (*err != 0)
    {
        return S_TYPE_UNKNOWN;
    }
    if (recvType != S_TYPE_LIST || receiver->typeInfo == NULL ||
        receiver->typeInfo->type != S_TYPE_LIST ||
        receiver->typeInfo->elemType == NULL)
    {
        *err = ERROR_TYPE_MISMATCH;
        return S_TYPE_UNKNOWN;
    }

    const typeDesc* listDesc = receiver->typeInfo;
    const typeDesc* elemDesc = listDesc->elemType;

    /** members with no () .head, .tail, .toString */
    if (strcmp(memberName, "head") == 0)
    {
        if (hasArgs || argCount != 0)
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }
        if (elemDesc->type == S_TYPE_LIST || elemDesc->type == S_TYPE_FUNC)
        {
            typeDescDestroy(memberNode->typeInfo);
            memberNode->typeInfo = typeDescClone(elemDesc);
        }
        memberNode->retType = elemDesc->type;
        return elemDesc->type;
    }

    if (strcmp(memberName, "tail") == 0)
    {
        if (hasArgs || argCount != 0)
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }
        typeDescDestroy(memberNode->typeInfo);
        memberNode->typeInfo = typeDescClone(listDesc);
        memberNode->retType = S_TYPE_LIST;
        return S_TYPE_LIST;
    }

    if (strcmp(memberName, "toString") == 0)
    {
        if (hasArgs || argCount != 0)
        {
            *err = ERROR_TYPE_MISMATCH;
            return S_TYPE_UNKNOWN;
        }
        memberNode->retType = S_TYPE_STRING;
        return S_TYPE_STRING;
    }

    /** Methods with 1 argument Int: .drop(n), .take(n) */
    if (strcmp(memberName, "drop") == 0 || strcmp(memberName, "take") == 0)
    {
        if (!hasArgs || argCount != 1)
        {
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }
        dataType aType =
            checkExpr(memberNode->children[1], globalTable, currentNode, err);
        if (*err != 0)
        {
            return S_TYPE_UNKNOWN;
        }
        if (aType != S_TYPE_INT)
        {
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }
        typeDescDestroy(memberNode->typeInfo);
        memberNode->typeInfo = typeDescClone(listDesc);
        memberNode->retType = S_TYPE_LIST;
        return S_TYPE_LIST;
    }

    /** Methods with 2 argumentInt: .slice(from, until) */
    if (strcmp(memberName, "slice") == 0)
    {
        if (!hasArgs || argCount != 2)
        {
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }
        for (int i = 1; i <= 2; i++)
        {
            dataType aType = checkExpr(memberNode->children[i], globalTable,
                                       currentNode, err);
            if (*err != 0)
            {
                return S_TYPE_UNKNOWN;
            }
            if (aType != S_TYPE_INT)
            {
                *err = ERROR_WRONG_ARGUMENTS;
                return S_TYPE_UNKNOWN;
            }
        }
        typeDescDestroy(memberNode->typeInfo);
        memberNode->typeInfo = typeDescClone(listDesc);
        memberNode->retType = S_TYPE_LIST;
        return S_TYPE_LIST;
    }

    /**  other methods .filter(pred), .takeWhile(pred),
     *  .dropWhile(pred)
     * */
    if (strcmp(memberName, "filter") == 0 ||
        strcmp(memberName, "takeWhile") == 0 ||
        strcmp(memberName, "dropWhile") == 0)
    {
        if (!hasArgs || argCount != 1)
        {
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }

        typeDesc** predParams = malloc(sizeof(typeDesc*));
        if (predParams == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }
        predParams[0] = typeDescClone(elemDesc);
        typeDesc* expectedPred =
            typeCreateFunc(1, predParams, typeCreatePrimitive(S_TYPE_BOOL));
        if (expectedPred == NULL)
        {
            *err = ERROR_INTERNAL;
            return S_TYPE_UNKNOWN;
        }

        astNode* predArg = memberNode->children[1];
        dataType predType =
            checkExprWithExpected(predArg, expectedPred, globalTable,
                                  currentNode, ERROR_WRONG_ARGUMENTS, err);
        if (*err != 0)
        {
            typeDescDestroy(expectedPred);
            return S_TYPE_UNKNOWN;
        }
        if (predType != S_TYPE_FUNC)
        {
            typeDescDestroy(expectedPred);
            *err = ERROR_WRONG_ARGUMENTS;
            return S_TYPE_UNKNOWN;
        }

        typeDescDestroy(expectedPred);
        typeDescDestroy(memberNode->typeInfo);
        memberNode->typeInfo = typeDescClone(listDesc);
        memberNode->retType = S_TYPE_LIST;
        return S_TYPE_LIST;
    }

    *err = ERROR_TYPE_MISMATCH;
    return S_TYPE_UNKNOWN;
}