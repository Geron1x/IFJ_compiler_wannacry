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

/** Types of nodes stored in the abstract syntax tree */
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
    AST_STAT_FOR,
    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_CALL,
    AST_EXPR_VAR,
    AST_EXPR_LITERAL,
    AST_EXPR_IF,
    AST_EXPR_LAMBDA,
    AST_EXPR_LIST_LITERAL,
    AST_EXPR_MEMBER,
    AST_EXPR_RANGE,
    AST_EXPR_CALL_EXPR,
    AST_STAT_FOR_EACH
} astNodeType;


/** Operators stored in expression nodes */
typedef enum astoptype
{
    AST_OP_ADD,
    AST_OP_SUB,
    AST_OP_MUL,
    AST_OP_DIV,

    AST_OP_EQ,
    AST_OP_NEQ,

    AST_OP_L,
    AST_OP_G,
    AST_OP_LEQ,
    AST_OP_GEQ,

    AST_OP_NEG,
    AST_OP_POS,

    AST_OP_NOT,
    AST_OP_AND,
    AST_OP_OR,

    AST_OP_CONCAT
} astOpType;


typedef struct typeDesc typeDesc;

/**
 * @brief Sets a type descriptor on an AST node
 */
void astSetTypeDesc(astNode* node, typeDesc* desc);
/**
 * @brief Creates a descriptor for a primitive type
 */
typeDesc* typeCreatePrimitive(dataType type);
/**
 * @brief Creates a list descriptor with the element type
 */
typeDesc* typeCreateList(typeDesc* elemType);
/**
 * @brief Creates a descriptor for a function type
 */
typeDesc* typeCreateFunc(int paramCount, typeDesc** paramTypes,
                         typeDesc* returnType);
/**
 * @brief Creates a deep copy of a type descriptor
 */
typeDesc* typeDescClone(const typeDesc* src);
/**
 * @brief Compares two type descriptors
 */
bool typeDescEquals(const typeDesc* a, const typeDesc* b);
/**
 * @brief Frees a type descriptor and its nested descriptors
 */
void typeDescDestroy(typeDesc* desc);

/**
 * @brief Creates a lambda expression node
 */
int astCreateLambda(dataType retType, typeDesc* retTypeDesc, astNode* body,
                    astNode** retNode);
/**
 * @brief Creates an empty list-literal node
 */
int astCreateListLiteral(astNode** retNode);
/**
 * @brief Creates a member-access or member-call node
 */
int astCreateMember(const char* name, astNode* receiver, bool hasArgs,
                    astNode** retNode);
/**
 * @brief Creates a range expression node
 */
int astCreateRange(astNode* from, astNode* end, astNode* step, bool isInclusive,
                   astNode** retNode);
/**
 * @brief Creates a call applied to another expression
 */
int astCreateCallExpr(astNode* calleeExpr, astNode** retNode);
/**
 * @brief Creates a for-each loop node
 */
int astCreateForEach(const char* key, astNode* iterableExpr, astNode* body,
                     astNode** retNode);

/**
 * @brief Creates a main node Prog which stores every function node as a
 * child (AST_PROG)
 */
int astCreateProgram(astNode** progNode);
/**
 * @brief Creates a node to group statements into a single
 * scope (AST_BLOCK)
 */
int astCreateBlock(astNode** block);

/**
 * @brief Makes the statement (could be expression or some statement) node a
 * part (child) of the block node (statement is placed into the children array
 * of block node)
 */
int astBlockAppend(astNode* block, astNode* statement);
/**
 * @brief Creates a node to express if block (AST_STAT_IF)
 */
int astCreateIf(astNode* condition, astNode* thenBlock, astNode* elseBlock,
                astNode** retNode);
/**
 * @brief Creates a node to express while block (AST_STAT_WHILE)
 */
int astCreateWhile(astNode* condition, astNode* body, astNode** retNode);
/**
 * @brief Creates a node to define a function (AST_FUNC)
 */
int astCreateFuncDef(const char* key, dataType retType, astNode* body,
                     astNode** retNode, bool isMain);
/**
 * @brief Creates a node to define a variable or const (AST_STAT_VAR)
 */
int astCreateVarDef(const char* key, dataType type, bool isConst, astNode* expr,
                    astNode** retNode);
/**
 * @brief Creates a node to return
 * a value (or Unit) from a function (AST_STAT_RETURN)
 */
int astCreateReturn(astNode* expr, astNode** retNode);
/**
 * @brief Creates a node to assign a value into a variable (AST_STAT_SET)
 */
int astCreateSetter(const char* key, astNode* expr, astNode** retNode);
/**
 * @brief Creates a node to express a for loop block (AST_STAT_FOR)
 */
int astCreateFor(const char* key, astNode* from, astNode* end, astNode* step,
                 bool isInclusive, astNode* body, long long int intVal,
                 astNode** retNode);


/**
 * @brief Creates a node of unary operation (AST_EXPR_UNARY)
 */
int astCreateUnary(astOpType op, astNode* operand, astNode** retNode);
/**
 * @brief Creates a node of binary operation (AST_EXPR_BINARY)
 */
int astCreateBinary(astOpType op, astNode* left, astNode* right,
                    astNode** retNode);

/**
 * @brief Creates a function call node (AST_EXPR_CALL)
 */
int astCreateCall(const char* key, astNode** retNode);
/**
 * @brief Creates a node to interact with variable (AST_EXPR_VAR)
 */
int astCreateVarExpr(const char* key, astNode** retNode);
/**
 * @brief Creates an if expression (CONTROL extension) (AST_EXPR_IF)
 */
int astCreateIfExpr(astNode* condition, astNode* thenBlock, astNode* elseBlock,
                    astNode** retNode);

/**
 * @brief Creates a literal node with type Int (AST_EXPR_LITERAL)
 */
int astCreateLiteralInt(long long int val, astNode** retNode);
/**
 * @brief Creates a literal node with type Double (AST_EXPR_LITERAL)
 */
int astCreateLiteralDouble(double val, astNode** retNode);
/**
 * @brief Creates a literal node with type String (AST_EXPR_LITERAL)
 */
int astCreateLiteralString(const char* str, astNode** retNode);
/**
 * @brief Creates a literal node with type Bool (AST_EXPR_LITERAL)
 */
int astCreateLiteralBool(bool val, astNode** retNode);
/**
 * @brief Creates a literal node with type Unit (AST_EXPR_LITERAL)
 */
int astCreateLiteralUnit(astNode** retNode);

/**
 * @brief Destructor for the node and all dependent on it
 */
void astDestroy(astNode* node);

#endif
