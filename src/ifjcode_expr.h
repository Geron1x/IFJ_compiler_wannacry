/**
 * @file ifjcode_expr.h
 * @brief Turns expressions into stack code
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#ifndef IFJCODE_EXPR_H
#define IFJCODE_EXPR_H

#include <stdio.h>

#include "ast.h"

/**
 * @brief Label counter and the function we are in now
 */
typedef struct
{
    size_t labelCounter;
    const astNode* currentFunction;
} codegenContext;

/**
 * @brief Gives the next free label number
 *
 * @param context
 * @return New label number
 */
size_t takeLabelNumber(codegenContext* context);

/**
 * @brief Pushes value of the expression on the stack
 *
 * @param expression
 * @param context
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int pushExpression(const astNode* expression, codegenContext* context);

#endif
