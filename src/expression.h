// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "parser_internal.h"

// Parse one expression using the precedence table
// Returns 0 ok, 1 lexical, 2 syntax, 99 no memory
int parseExpression(parcerContext* ctx);

// True if the token can start an expression
bool canStartExpression(Type type);

#endif
