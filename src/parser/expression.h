// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "parser_internal.h"

// Parse one expression by precedence analysis
// Returns 0 ok, 1 lexical, 2 syntax, 99 out of memory
int parseExpression(parcerContext* ctx);

#endif
