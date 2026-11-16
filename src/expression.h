/**
 * @file expression.h
 * @brief Expression parsing by precedence analysis
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Heorhiy Haziy (xhaziyh00)
 *
 */

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "parser_internal.h"

/**
 * @brief Parses one expression using the precedence table and builds its tree.
 *
 * Stops at the first token that cannot continue the expression.
 *
 * @param ctx Parser state, current token is the first token of the expression.
 * @param out Receives the expression tree, NULL on error. If out is NULL the
 *            tree is freed right away.
 * @return ERROR_SUCCESS, ERROR_LEXICAL, ERROR_SYNTAX or ERROR_INTERNAL.
 */
int parseExpression(parcerContext* ctx, astNode** out);

/**
 * @brief Tells whether a token can start an expression.
 *
 * @param type Type of the token.
 * @return true if an expression can begin with this token.
 */
bool canStartExpression(Type type);

#endif
