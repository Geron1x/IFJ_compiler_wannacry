// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "parser_internal.h"

// Parse one expression by precedence analysis
// Returns 0 ok, 1 lexical, 2 syntax, 99 out of memory
int parseExpression(parcerContext* ctx);

// True if token can begin an expression
bool canStartExpression(Type type);

#endif
