/**
 * @file ast.c
 * @brief File for the AST implementation
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */

#include "ast.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ast_internal.h"
#include "error.h"
#include "symtable.h"

/** Initial capacity of a node's children array */
#define AST_INIT_CAPACITY 2


static void astInitNode(astNode* node, astNodeType nodeType, dataType retType)
{
    if (node == NULL)
    {
        return;
    }
    node->typeInfo = NULL;
    node->int2float = false;
    node->nodeType = nodeType;
    node->retType = retType;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
}

static int expand(astNode* node)
{
    if (node == NULL)
    {
        return ERROR_INTERNAL;
    }
    /** Grow the child array before inserting a new node */
    if (node->childCapacity == 0)
    {
        node->childCapacity = AST_INIT_CAPACITY;
    }
    else
    {
        node->childCapacity *= 2;
    }
    astNode** temp =
        realloc(node->children, sizeof(astNode*) * node->childCapacity);

    if (temp == NULL)
    {
        return ERROR_INTERNAL;
    }
    node->children = temp;
    return 0;
}


int astCreateProgram(astNode** progNode)
{
    if (progNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *progNode = malloc(sizeof(astNode));

    if (*progNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*progNode, AST_PROG, S_TYPE_UNIT);

    return 0;
}

int astCreateBlock(astNode** block)
{
    if (block == NULL)
    {
        return ERROR_INTERNAL;
    }

    *block = malloc(sizeof(astNode));

    if (*block == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*block, AST_BLOCK, S_TYPE_UNIT);
    return 0;
}
int astBlockAppend(astNode* block, astNode* statement)
{
    if (block == NULL || statement == NULL)
    {
        return ERROR_INTERNAL;
    }
    if (block->childCapacity < block->childCount + 1)
    {
        Errors status = expand(block);
        if (status != 0)
        {
            return status;
        }
    }
    /** insert the child (statement) into children array of block node */
    block->children[block->childCount] = statement;
    /** increment children count (one more place was occupied) */
    block->childCount++;

    return 0;
}
int astCreateVarDef(const char* key, dataType type, bool isConst, astNode* expr,
                    astNode** retNode)
{
    if (key == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_STAT_VAR, type);
    (*retNode)->data.varData.id = 0;
    (*retNode)->data.varData.key = malloc(sizeof(char) * (strlen(key) + 1));

    if ((*retNode)->data.varData.key == NULL)
    {
        free(*retNode);
        *retNode = NULL;
        return ERROR_INTERNAL;
    }
    strcpy((*retNode)->data.varData.key, key);
    (*retNode)->data.varData.isConst = isConst;

    if (expr != NULL)
    {
        Errors status = astBlockAppend(*retNode, expr);
        if (status != 0)
        {
            free((*retNode)->data.varData.key);
            free(*retNode);
            *retNode = NULL;
            return status;
        }
    }

    return 0;
}
int astCreateIf(astNode* condition, astNode* thenBlock, astNode* elseBlock,
                astNode** retNode)
{
    if (retNode == NULL || condition == NULL || thenBlock == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_STAT_IF, S_TYPE_UNIT);
    /** Children are stored as condition, then block and optional else block */
    Errors status = astBlockAppend(*retNode, condition);
    if (status != 0)
    {
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    status = astBlockAppend(*retNode, thenBlock);
    if (status != 0)
    {
        free((*retNode)->children);
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    if (elseBlock != NULL)
    {
        status = astBlockAppend(*retNode, elseBlock);
        if (status != 0)
        {
            free((*retNode)->children);
            free(*retNode);
            *retNode = NULL;
            return status;
        }
    }

    return 0;
}

int astCreateWhile(astNode* condition, astNode* body, astNode** retNode)
{
    if (condition == NULL || retNode == NULL || body == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_STAT_WHILE, S_TYPE_UNIT);
    Errors status = astBlockAppend(*retNode, condition);
    if (status != 0)
    {
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    status = astBlockAppend(*retNode, body);
    if (status != 0)
    {
        free((*retNode)->children);
        free(*retNode);
        *retNode = NULL;
        return status;
    }

    return 0;
}
int astCreateFor(const char* key, astNode* from, astNode* end, astNode* step,
                 bool isInclusive, astNode* body, int intVal, astNode** retNode)
{
    if (retNode == NULL || key == NULL || from == NULL || end == NULL ||
        body == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_STAT_FOR, S_TYPE_UNIT);
    (*retNode)->data.varData.id = 0;
    (*retNode)->data.forData.key = malloc(sizeof(char) * (strlen(key) + 1));
    if ((*retNode)->data.forData.key == NULL)
    {
        free((*retNode));
        *retNode = NULL;
        return ERROR_INTERNAL;
    }
    strcpy((*retNode)->data.forData.key, key);
    (*retNode)->data.forData.isInclusive = isInclusive;
    (*retNode)->data.forData.intVal = intVal;
    (*retNode)->data.forData.hasStep = (step != NULL);
    Errors status = astBlockAppend(*retNode, from);
    if (status != 0)
    {
        free((*retNode)->data.forData.key);
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    status = astBlockAppend(*retNode, end);
    if (status != 0)
    {
        free((*retNode)->data.forData.key);
        free((*retNode)->children);
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    status = astBlockAppend(*retNode, body);
    if (status != 0)
    {
        free((*retNode)->data.forData.key);
        free((*retNode)->children);
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    if (step != NULL)
    {
        status = astBlockAppend(*retNode, step);
        if (status != 0)
        {
            free((*retNode)->data.forData.key);
            free((*retNode)->children);
            free(*retNode);
            *retNode = NULL;
            return ERROR_INTERNAL;
        }
    }
    return 0;
}
int astCreateFuncDef(const char* key, dataType retType, astNode* body,
                     astNode** retNode, bool isMain)
{
    if (key == NULL || body == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_FUNC, retType);
    (*retNode)->data.funcData.key = malloc(sizeof(char) * (strlen(key) + 1));
    if ((*retNode)->data.funcData.key == NULL)
    {
        free(*retNode);
        *retNode = NULL;
        return ERROR_INTERNAL;
    }
    strcpy((*retNode)->data.funcData.key, key);
    (*retNode)->data.funcData.isMain = isMain;
    Errors status = astBlockAppend(*retNode, body);
    if (status != 0)
    {
        free((*retNode)->data.funcData.key);
        free(*retNode);
        *retNode = NULL;
        return status;
    }


    return 0;
}

int astCreateReturn(astNode* expr, astNode** retNode)
{
    if (retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_STAT_RETURN, S_TYPE_UNIT);

    if (expr != NULL)
    {
        Errors status = astBlockAppend(*retNode, expr);
        if (status != 0)
        {
            free(*retNode);
            *retNode = NULL;
            return status;
        }
    }

    return 0;
}
int astCreateSetter(const char* key, astNode* expr, astNode** retNode)
{
    if (key == NULL || expr == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_STAT_SET, S_TYPE_UNIT);
    (*retNode)->data.varData.id = 0;
    (*retNode)->data.varData.key = malloc(sizeof(char) * (strlen(key) + 1));
    if ((*retNode)->data.varData.key == NULL)
    {
        free(*retNode);
        *retNode = NULL;
        return ERROR_INTERNAL;
    }
    strcpy((*retNode)->data.varData.key, key);
    Errors status = astBlockAppend(*retNode, expr);
    if (status != 0)
    {
        free((*retNode)->data.varData.key);
        free(*retNode);
        *retNode = NULL;
        return status;
    }

    return 0;
}

int astCreateUnary(astOpType op, astNode* operand, astNode** retNode)
{
    if (operand == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_EXPR_UNARY, S_TYPE_UNKNOWN);
    (*retNode)->data.opType = op;
    Errors status = astBlockAppend(*retNode, operand);
    if (status != 0)
    {
        free(*retNode);
        *retNode = NULL;
        return status;
    }

    return 0;
}

int astCreateBinary(astOpType op, astNode* left, astNode* right,
                    astNode** retNode)
{
    if (left == NULL || right == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_EXPR_BINARY, S_TYPE_UNKNOWN);
    (*retNode)->data.opType = op;
    /** The left and right operands are kept in child positions 0 and 1 */
    Errors status = astBlockAppend(*retNode, left);
    if (status != 0)
    {
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    status = astBlockAppend(*retNode, right);
    if (status != 0)
    {
        free((*retNode)->children);
        free(*retNode);
        *retNode = NULL;
        return status;
    }


    return 0;
}
int astCreateIfExpr(astNode* condition, astNode* thenBlock, astNode* elseBlock,
                    astNode** retNode)
{
    if (condition == NULL || thenBlock == NULL || elseBlock == NULL ||
        retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    astInitNode(*retNode, AST_EXPR_IF, S_TYPE_UNKNOWN);

    Errors status = astBlockAppend(*retNode, condition);
    if (status != 0)
    {
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    status = astBlockAppend(*retNode, thenBlock);
    if (status != 0)
    {
        free((*retNode)->children);
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    status = astBlockAppend(*retNode, elseBlock);
    if (status != 0)
    {
        free((*retNode)->children);
        free(*retNode);
        *retNode = NULL;
        return status;
    }
    return 0;
}
int astCreateCall(const char* key, astNode** retNode)
{
    if (key == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_CALL, S_TYPE_UNKNOWN);

    (*retNode)->data.funcData.key = malloc(sizeof(char) * (strlen(key) + 1));
    if ((*retNode)->data.funcData.key == NULL)
    {
        free(*retNode);
        *retNode = NULL;
        return ERROR_INTERNAL;
    }
    strcpy((*retNode)->data.funcData.key, key);

    return 0;
}

int astCreateVarExpr(const char* key, astNode** retNode)
{
    if (key == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_VAR, S_TYPE_UNKNOWN);
    (*retNode)->data.varData.id = 0;
    (*retNode)->data.varData.key = malloc(sizeof(char) * (strlen(key) + 1));
    if ((*retNode)->data.varData.key == NULL)
    {
        free(*retNode);
        *retNode = NULL;
        return ERROR_INTERNAL;
    }
    strcpy((*retNode)->data.varData.key, key);

    return 0;
}

int astCreateLiteralInt(int val,
                        astNode** retNode) /** CHANGE TO LONG LONG INT */
{
    if (retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_LITERAL, S_TYPE_INT);

    (*retNode)->data.intVal = val;

    return 0;
}

int astCreateLiteralDouble(double val, astNode** retNode)
{
    if (retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_LITERAL, S_TYPE_DOUBLE);

    (*retNode)->data.doubleVal = val;

    return 0;
}
int astCreateLiteralString(const char* str, astNode** retNode)
{
    if (retNode == NULL || str == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_LITERAL, S_TYPE_STRING);

    (*retNode)->data.strVal = malloc(sizeof(char) * (strlen(str) + 1));
    if ((*retNode)->data.strVal == NULL)
    {
        free(*retNode);
        *retNode = NULL;
        return ERROR_INTERNAL;
    }
    strcpy((*retNode)->data.strVal, str);

    return 0;
}
int astCreateLiteralBool(bool val, astNode** retNode)
{
    if (retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_LITERAL, S_TYPE_BOOL);

    (*retNode)->data.boolVal = val;

    return 0;
}
int astCreateLiteralUnit(astNode** retNode)
{
    if (retNode == NULL)
    {
        return ERROR_INTERNAL;
    }
    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_LITERAL, S_TYPE_UNIT);

    return 0;
}
int astCreateListLiteral(astNode** retNode)
{
    if (retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_LIST_LITERAL, S_TYPE_LIST);
    return 0;
}

/** LAMDAS/LIST */

typeDesc* typeCreatePrimitive(dataType type)
{
    typeDesc* desc = malloc(sizeof(typeDesc));
    if (desc == NULL)
    {
        return NULL;
    }
    desc->type = type;
    desc->elemType = NULL;
    return desc;
}

typeDesc* typeCreateList(typeDesc* elemType)
{
    if (elemType == NULL)
    {
        return NULL;
    }
    typeDesc* desc = malloc(sizeof(typeDesc));
    if (desc == NULL)
    {
        typeDescDestroy(elemType);
        return NULL;
    }
    desc->type = S_TYPE_LIST;
    desc->elemType = elemType;
    return desc;
}

typeDesc* typeCreateFunc(int paramCount, typeDesc** paramTypes,
                         typeDesc* returnType)
{
    if (returnType == NULL || (paramCount > 0 && paramTypes == NULL))
    {
        for (int i = 0; i < paramCount && paramTypes != NULL; i++)
        {
            typeDescDestroy(paramTypes[i]);
        }
        free(paramTypes);
        typeDescDestroy(returnType);
        return NULL;
    }

    typeDesc* desc = malloc(sizeof(typeDesc));
    if (desc == NULL)
    {
        for (int i = 0; i < paramCount; i++)
        {
            typeDescDestroy(paramTypes[i]);
        }
        free(paramTypes);
        typeDescDestroy(returnType);
        return NULL;
    }

    desc->type = S_TYPE_FUNC;
    desc->func.paramCount = paramCount;
    if (paramCount > 0)
    {
        desc->func.paramTypes = paramTypes;
    }
    else
    {
        desc->func.paramTypes = NULL;
    }
    desc->func.returnType = returnType;
    if (paramCount == 0 && paramTypes != NULL)
    {
        free(paramTypes);
    }
    return desc;
}

void typeDescDestroy(typeDesc* desc)
{
    if (desc == NULL)
    {
        return;
    }

    if (desc->type == S_TYPE_LIST)
    {
        typeDescDestroy(desc->elemType);
    }
    else if (desc->type == S_TYPE_FUNC)
    {
        if (desc->func.paramTypes != NULL)
        {
            for (int i = 0; i < desc->func.paramCount; i++)
            {
                typeDescDestroy(desc->func.paramTypes[i]);
            }
            free(desc->func.paramTypes);
        }
        typeDescDestroy(desc->func.returnType);
    }

    free(desc);
}

typeDesc* typeDescClone(const typeDesc* src)
{
    if (src == NULL)
    {
        return NULL;
    }

    if (src->type == S_TYPE_LIST)
    {
        typeDesc* clonedElem = typeDescClone(src->elemType);
        if (clonedElem == NULL && src->elemType != NULL)
        {
            return NULL;
        }
        return typeCreateList(clonedElem);
    }

    if (src->type == S_TYPE_FUNC)
    {
        typeDesc** clonedParams = NULL;
        if (src->func.paramCount > 0)
        {
            clonedParams = malloc(sizeof(typeDesc*) * src->func.paramCount);
            if (clonedParams == NULL)
            {
                return NULL;
            }
            for (int i = 0; i < src->func.paramCount; i++)
            {
                clonedParams[i] = typeDescClone(src->func.paramTypes[i]);
                if (clonedParams[i] == NULL && src->func.paramTypes[i] != NULL)
                {
                    for (int j = 0; j < i; j++)
                    {
                        typeDescDestroy(clonedParams[j]);
                    }
                    free(clonedParams);
                    return NULL;
                }
            }
        }

        typeDesc* clonedRet = typeDescClone(src->func.returnType);
        if (clonedRet == NULL && src->func.returnType != NULL)
        {
            for (int i = 0; i < src->func.paramCount; i++)
            {
                typeDescDestroy(clonedParams[i]);
            }
            free(clonedParams);
            return NULL;
        }

        return typeCreateFunc(src->func.paramCount, clonedParams, clonedRet);
    }

    return typeCreatePrimitive(src->type);
}

bool typeDescEquals(const typeDesc* a, const typeDesc* b)
{
    if (a == b)
    {
        return true;
    }
    if (a == NULL || b == NULL)
    {
        return false;
    }
    if (a->type != b->type)
    {
        return false;
    }

    if (a->type == S_TYPE_LIST)
    {
        return typeDescEquals(a->elemType, b->elemType);
    }

    if (a->type == S_TYPE_FUNC)
    {
        if (a->func.paramCount != b->func.paramCount)
        {
            return false;
        }
        if (!typeDescEquals(a->func.returnType, b->func.returnType))
        {
            return false;
        }
        for (int i = 0; i < a->func.paramCount; i++)
        {
            if (!typeDescEquals(a->func.paramTypes[i], b->func.paramTypes[i]))
            {
                return false;
            }
        }
    }

    return true;
}

void astSetTypeDesc(astNode* node, typeDesc* desc)
{
    if (node == NULL)
    {
        typeDescDestroy(desc);
        return;
    }
    if (node->typeInfo != NULL)
    {
        typeDescDestroy(node->typeInfo);
    }
    node->typeInfo = desc;
}

int astCreateLambda(dataType retType, typeDesc* retTypeDesc, astNode* body,
                    astNode** retNode)
{
    if (body == NULL || retNode == NULL)
    {
        typeDescDestroy(retTypeDesc);
        return ERROR_INTERNAL;
    }

    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        typeDescDestroy(retTypeDesc);
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_LAMBDA, retType);
    (*retNode)->typeInfo = retTypeDesc;

    Errors status = astBlockAppend(*retNode, body);
    if (status != 0)
    {
        typeDescDestroy((*retNode)->typeInfo);
        free(*retNode);
        *retNode = NULL;
        return status;
    }

    return 0;
}

int astCreateMember(const char* name, astNode* receiver, bool hasArgs,
                    astNode** retNode)
{
    if (name == NULL || receiver == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_MEMBER, S_TYPE_UNKNOWN);
    (*retNode)->data.memberData.key = malloc(strlen(name) + 1);
    if ((*retNode)->data.memberData.key == NULL)
    {
        free(*retNode);
        *retNode = NULL;
        return ERROR_INTERNAL;
    }
    strcpy((*retNode)->data.memberData.key, name);
    (*retNode)->data.memberData.hasArgs = hasArgs;

    Errors status = astBlockAppend(*retNode, receiver);
    if (status != 0)
    {
        free((*retNode)->data.memberData.key);
        free(*retNode);
        *retNode = NULL;
        return status;
    }

    return 0;
}

int astCreateRange(astNode* from, astNode* end, astNode* step, bool isInclusive,
                   astNode** retNode)
{
    if (from == NULL || end == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_RANGE, S_TYPE_UNKNOWN);
    (*retNode)->data.rangeData.isInclusive = isInclusive;
    (*retNode)->data.rangeData.hasStep = (step != NULL);

    Errors status = astBlockAppend(*retNode, from);
    if (status == 0) status = astBlockAppend(*retNode, end);
    if (status == 0 && step != NULL) status = astBlockAppend(*retNode, step);

    if (status != 0)
    {
        free((*retNode)->children);
        free(*retNode);
        *retNode = NULL;
        return status;
    }

    return 0;
}

int astCreateCallExpr(astNode* calleeExpr, astNode** retNode)
{
    if (calleeExpr == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_EXPR_CALL_EXPR, S_TYPE_UNKNOWN);

    Errors status = astBlockAppend(*retNode, calleeExpr);
    if (status != 0)
    {
        free(*retNode);
        *retNode = NULL;
        return status;
    }

    return 0;
}

int astCreateForEach(const char* key, astNode* iterableExpr, astNode* body,
                     astNode** retNode)
{
    if (key == NULL || iterableExpr == NULL || body == NULL || retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    *retNode = malloc(sizeof(astNode));
    if (*retNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    astInitNode(*retNode, AST_STAT_FOR_EACH, S_TYPE_UNIT);
    (*retNode)->data.forData.key = malloc(strlen(key) + 1);
    if ((*retNode)->data.forData.key == NULL)
    {
        free(*retNode);
        *retNode = NULL;
        return ERROR_INTERNAL;
    }
    strcpy((*retNode)->data.forData.key, key);

    Errors status = astBlockAppend(*retNode, iterableExpr);
    if (status == 0) status = astBlockAppend(*retNode, body);

    if (status != 0)
    {
        free((*retNode)->data.forData.key);
        free((*retNode)->children);
        free(*retNode);
        *retNode = NULL;
        return status;
    }

    return 0;
}


/** - LAMDAS/LIST */

void astDestroy(astNode* node)
{
    if (node == NULL)
    {
        return;
    }
    /** post-order traversal for all children present in the node */
    for (int i = 0; i < node->childCount; i++)
    {
        astDestroy(node->children[i]);
    }
    free(node->children);
    typeDescDestroy(node->typeInfo);
    /** The union member to free is determined by the node type */
    switch (node->nodeType)
    {
        case AST_EXPR_LITERAL:
            if (node->retType == S_TYPE_STRING)
            {
                free(node->data.strVal);
            }
            break;

        case AST_STAT_SET:
        case AST_STAT_VAR:
        case AST_EXPR_VAR:
            free(node->data.varData.key);
            break;

        case AST_FUNC:
        case AST_EXPR_CALL:
            free(node->data.funcData.key);
            break;

        case AST_EXPR_MEMBER:
            free(node->data.memberData.key);
            break;

        case AST_STAT_FOR:
        case AST_STAT_FOR_EACH:
            free(node->data.forData.key);
            break;

        default:
            break;
    }
    free(node);
}