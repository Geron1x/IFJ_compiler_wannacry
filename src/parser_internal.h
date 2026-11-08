// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include <stdbool.h>

#include "scanner.h"

// Parser state shared by parser.c and expression.c
typedef struct
{
    Token currentToken;
    Scanner* globalScanner;
    Token peekToken;  // lookahead buffer
    bool hasPeek;     // peekToken holds a token
} parcerContext;

// Macro for automatic error propagation
#define CHECK_ERR(call)             \
    do                              \
    {                               \
        int _err = (call);          \
        if (_err != 0) return _err; \
                                    \
    } while (0)

// Token helpers (parser.c)
int advance(parcerContext* ctx);
int peek(parcerContext* ctx);
int match(parcerContext* ctx, Type expectedType);

// Call arguments, LL part (parser.c)
int parseCallArgs(parcerContext* ctx);

#endif
