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

#include "ast.h"

/**
 * @brief Pushes value of the expression on the stack
 *
 * @param expression
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int pushExpression(const astNode* expression);

#endif
