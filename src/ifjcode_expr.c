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

/** Pushes value of the expression on the stack */
int pushExpression(const astNode* expression)
{
    // look what kind of expression we got
    switch (expression->nodeType)
    {
        case AST_EXPR_LITERAL:
            RETURN_IF_ERROR(pushLiteral(expression));
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
