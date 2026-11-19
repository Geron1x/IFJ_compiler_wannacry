/**
 * @file ifjcode_expr.c
 * @brief Turns expressions into stack code
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#include "ifjcode_expr.h"

#include <stdio.h>

#include "ast_internal.h"
#include "operands.h"

/** Gives the next free label number */
size_t takeLabelNumber(codegenContext* context)
{
    context->labelCounter++;

    return context->labelCounter;
}

/** Pushes literal on the stack */
static int pushLiteral(const astNode* literal)
{
    RETURN_IF_ERROR(tryPrint("PUSHS "));

    switch (literal->retType)
    {
        case S_TYPE_BOOL:
            RETURN_IF_ERROR(writeLiteralBool(literal->data.boolVal));
            break;

        case S_TYPE_INT:
            RETURN_IF_ERROR(writeLiteralInt(literal->data.intVal));
            break;

        case S_TYPE_UNIT:  // unit means nil
            RETURN_IF_ERROR(writeLiteralNil());
            break;

        case S_TYPE_DOUBLE:
            RETURN_IF_ERROR(writeLiteralFloat(literal->data.doubleVal));
            break;

        case S_TYPE_STRING:
            RETURN_IF_ERROR(writeLiteralString(literal->data.strVal));
            break;

        default:
            return ERROR_INTERNAL;
    }

    RETURN_IF_ERROR(tryPrint("\n"));

    return ERROR_SUCCESS;
}

/** Pushes variable on the stack */
static int pushVariable(const astNode* variable)
{
    // variable name from tree
    const char* name = variable->data.varData.key;

    size_t number = variable->data.varData.id;

    // put it on the stack
    return writeVarInstruction("PUSHS", name, number);
}

/** Gives the IFJcode word for the operator, like a phrasebook */
static const char* translateOperator(astOpType operation, dataType resultType)
{
    switch (operation)
    {
        case AST_OP_ADD:  // +
            return "ADDS\n";

        case AST_OP_SUB:  // -
            return "SUBS\n";

        case AST_OP_L:  // <
            return "LTS\n";

        case AST_OP_G:  // >
            return "GTS\n";

        case AST_OP_EQ:  // ==
            return "EQS\n";

        case AST_OP_LEQ:  // <= , like not >
            return "GTS\nNOTS\n";

        case AST_OP_GEQ:  // >=, like not <
            return "LTS\nNOTS\n";

        case AST_OP_NEQ:  // !=
            return "EQS\nNOTS\n";

        case AST_OP_MUL:  // *
            return "MULS\n";

        case AST_OP_DIV:  // /
            if (resultType == S_TYPE_INT)
            {
                return "IDIVS\n";
            }
            return "DIVS\n";

        default:
            return NULL;
    }
}

/** Makes one string from two */
static int writeConcat(void)
{
    // take strings from stack, right one is on top
    RETURN_IF_ERROR(writeTempInstruction("POPS", 2));
    RETURN_IF_ERROR(writeTempInstruction("POPS", 1));

    // result goes to tmp1
    RETURN_IF_ERROR(tryPrint("CONCAT "));
    RETURN_IF_ERROR(writeTempVar(1));
    RETURN_IF_ERROR(tryPrint(" "));

    // two strings
    RETURN_IF_ERROR(writeTempVar(1));
    RETURN_IF_ERROR(tryPrint(" "));
    RETURN_IF_ERROR(writeTempVar(2));
    RETURN_IF_ERROR(tryPrint("\n"));

    // put the result back on stack
    RETURN_IF_ERROR(writeTempInstruction("PUSHS", 1));

    return ERROR_SUCCESS;
}

/** Pushes left and right, and after that - does the operation */
static int pushOperation(const astNode* binary, codegenContext* context)
{
    // take left and right side of the operation
    const astNode* left = binary->children[0];
    const astNode* right = binary->children[1];

    // put left and right on the stack
    RETURN_IF_ERROR(pushExpression(left, context));
    RETURN_IF_ERROR(pushExpression(right, context));

    // makes one string from two
    if (binary->data.opType == AST_OP_ADD && binary->retType == S_TYPE_STRING)
    {
        return writeConcat();
    }

    // print the operation text to stdout
    return tryPrint(translateOperator(binary->data.opType, binary->retType));
}

/** Pushes all arguments and calls the function */
static int pushCall(const astNode* call, codegenContext* context)
{
    size_t argumentCount = (size_t)call->childCount;

    // put every argument on stack, from left to right
    for (size_t i = 0; i < argumentCount; i++)
    {
        RETURN_IF_ERROR(pushExpression(call->children[i], context));
    }

    RETURN_IF_ERROR(tryPrint("CALL "));
    RETURN_IF_ERROR(writeFuncLabel(call->data.funcData.key));
    RETURN_IF_ERROR(tryPrint("\n"));

    return ERROR_SUCCESS;
}

/** Handles minus or plus before a number */
static int pushUnary(const astNode* unary, codegenContext* context)
{
    // number comes first, sign comes later
    RETURN_IF_ERROR(pushExpression(unary->children[0], context));

    // plus changes nothing
    if (unary->data.opType == AST_OP_POS)
    {
        return ERROR_SUCCESS;
    }

    // just put -1 on stack
    RETURN_IF_ERROR(tryPrint("PUSHS "));
    if (unary->retType == S_TYPE_INT)
    {
        RETURN_IF_ERROR(writeLiteralInt(-1));
    }
    else
    {
        RETURN_IF_ERROR(writeLiteralFloat(-1.0));
    }
    RETURN_IF_ERROR(tryPrint("\n"));

    RETURN_IF_ERROR(tryPrint("MULS\n"));

    return ERROR_SUCCESS;
}

/** Pushes value of the expression on the stack */
int pushExpression(const astNode* expression, codegenContext* context)
{
    // look what kind of expression we got
    switch (expression->nodeType)
    {
        case AST_EXPR_LITERAL:
            RETURN_IF_ERROR(pushLiteral(expression));
            break;

        case AST_EXPR_BINARY:
            RETURN_IF_ERROR(pushOperation(expression, context));
            break;

        case AST_EXPR_CALL:
            RETURN_IF_ERROR(pushCall(expression, context));
            break;

        case AST_EXPR_UNARY:
            RETURN_IF_ERROR(pushUnary(expression, context));
            break;

        case AST_EXPR_VAR:
            RETURN_IF_ERROR(pushVariable(expression));
            break;

        default:
            return ERROR_INTERNAL;
    }

    // int must be double here
    if (expression->int2float == true)
    {
        RETURN_IF_ERROR(tryPrint("INT2FLOATS\n"));
    }

    return ERROR_SUCCESS;
}
