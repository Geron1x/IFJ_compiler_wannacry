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
    node->int2float = false;
    node->nodeType = nodeType;
    node->retType = retType;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
}

static Errors expand(astNode* node)
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


Errors astCreateProgram(astNode** progNode)
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

Errors astCreateBlock(astNode** block)
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
Errors astBlockAppend(astNode* block, astNode* statement)
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
Errors astCreateVarDef(const char* key, dataType type, bool isConst,
                       astNode* expr, astNode** retNode)
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
Errors astCreateIf(astNode* condition, astNode* thenBlock, astNode* elseBlock,
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

Errors astCreateWhile(astNode* condition, astNode* body, astNode** retNode)
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

Errors astCreateFuncDef(const char* key, dataType retType, astNode* body,
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

Errors astCreateReturn(astNode* expr, astNode** retNode)
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
Errors astCreateSetter(const char* key, astNode* expr, astNode** retNode)
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

Errors astCreateUnary(astOpType op, astNode* operand, astNode** retNode)
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

Errors astCreateBinary(astOpType op, astNode* left, astNode* right,
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

Errors astCreateCall(const char* key, astNode** retNode)
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

Errors astCreateVarExpr(const char* key, astNode** retNode)
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

Errors astCreateLiteralInt(int val, astNode** retNode)
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

Errors astCreateLiteralDouble(double val, astNode** retNode)
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
Errors astCreateLiteralString(const char* str, astNode** retNode)
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
Errors astCreateLiteralBool(bool val, astNode** retNode)
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
Errors astCreateLiteralUnit(astNode** retNode)
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
    /** The union member to free is determined by the node type */
    if (node->retType == S_TYPE_STRING && node->nodeType == AST_EXPR_LITERAL)
    {
        free(node->data.strVal);
    }
    if (node->nodeType == AST_STAT_SET || node->nodeType == AST_STAT_VAR ||
        node->nodeType == AST_EXPR_VAR)
    {
        free(node->data.varData
                 .key); /** DANGEROUS PART, POSSIBLE MEM LEAK IF PARSER MAKE
                           MISTAKE IN AST NODE CREATION OF A LITERAL */
    }
    if (node->nodeType == AST_FUNC || node->nodeType == AST_EXPR_CALL)
    {
        free(node->data.funcData.key);
    }
    free(node);
}