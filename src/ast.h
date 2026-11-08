/**
 * @file ast.h
 * @brief Header file for the AST implementation
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */

#ifndef AST_H
#define AST_H

#include "error.h"
#include "symtable.h"


typedef struct astNode astNode;

typedef enum astnodetype
{
    AST_PROG,
    AST_FUNC,
    AST_BLOCK,
    AST_STAT_VAR,
    AST_STAT_SET,
    AST_STAT_IF,
    AST_STAT_WHILE,
    AST_STAT_RETURN,
    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_CALL,
    AST_EXPR_VAR,
    AST_EXPR_LITERAL
    /** ADD OTHER TYPES WITH EXTENTIONS */
} astNodeType;

typedef enum astoptype
{
    AST_OP_ADD,
    AST_OP_SUB,
    AST_OP_MUL,
    AST_OP_DIV,
    AST_OP_CMP,

    AST_OP_EQ,
    AST_OP_NEQ,

    AST_OP_L,
    AST_OP_G,
    AST_OP_LEQ,
    AST_OP_GEQ,

    AST_OP_NEG,
    AST_OP_POS
} astOpType;

/**
 * @brief Create a main node Prog which stores every function node as a child
 * (AST_PROG)
 */
Errors astCreateProgram(astNode** progNode);
/**
 * @brief Creates a node to group statements into a single
 * scope (AST_BLOCK)
 */
Errors astCreateBlock(astNode** block);

/**
 * @brief Make the statement (could be expression or some statement) node a part
 * (child) of the block node (statement is placed into the children array of
 * block node)
 */
Errors astBlockAppend(astNode* block, astNode* statement);
/**
 * @brief Create a node to express if block (AST_STAT_IF)
 */
Errors astCreateIf(astNode* condition, astNode* thenBlock, astNode* elseBlock,
                   astNode** retNode);
/**
 * @brief Create a node to express while block (AST_STAT_WHILE)
 */
Errors astCreateWhile(astNode* condition, astNode* body, astNode** retNode);
/**
 * @brief Create a node to define a function (AST_FUNC)
 */
Errors astCreateFuncDef(const char* key, dataType retType, astNode* body,
                        astNode** retNode);
/**
 * @brief Create a node to define a variable or const (AST_STAT_VAR)
 */
Errors astCreateVarDef(const char* key, bool isConst, astNode* expr,
                       astNode** retNode);
/**
 * @brief Create a node to return
 * a value (or Unit) from a function (AST_STAT_RETURN)
 */
Errors astCreateReturn(astNode* expr, astNode** retNode);
/**
 * @brief Create a node to assign a value into a variable (AST_STAT_SET)
 */
Errors astCreateSetter(const char* key, astNode* expr, astNode** retNode);

/**
 * @brief Create a node of unary operation (AST_EXPR_UNARY)
 */
Errors astCreateUnary(astOpType op, astNode* operand, astNode** retNode);
/**
 * @brief Create a node of binary operation (AST_EXPR_BINARY)
 */
Errors astCreateBinary(astOpType op, astNode* left, astNode* right,
                       astNode** retNode);

/**
 * @brief Create a function call node (AST_EXPR_CALL)
 */
Errors astCreateCall(const char* key, astNode** retNode);
/**
 * @brief Create a node to interact with variable (AST_EXPR_VAR)
 */
Errors astCreateVarExpr(const char* key, astNode** retNode);

/**
 * @brief Create a literal node with type Int (AST_EXPR_LITERAL)
 */
Errors astCreateLiteralInt(int val, astNode** retNode);
/**
 * @brief Create a literal node with type Double (AST_EXPR_LITERAL)
 */
Errors astCreateLiteralDouble(double val, astNode** retNode);
/**
 * @brief Create a literal node with type String (AST_EXPR_LITERAL)
 */
Errors astCreateLiteralString(const char* str, astNode** retNode);
/**
 * @brief Create a literal node with type Bool (AST_EXPR_LITERAL)
 */
Errors astCreateLiteralBool(bool val, astNode** retNode);
/**
 * @brief Create a literal node with type Unit (AST_EXPR_LITERAL)
 */
Errors astCreateLiteralUnit(astNode** retNode);

/**
 * @brief Destructor for the node and all dependent on it
 */
void astDestroy(astNode* node);

#endif
