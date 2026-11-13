// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include <stdbool.h>

#include "error.h"
#include "scanner.h"

// Parser state shared by parser.c and expression.c
typedef struct
{
    Token currentToken;
    Scanner* globalScanner;
    Token peekToken;  // next token read ahead
    bool hasPeek;     // true if peekToken is filled
    Type prevType;    // type of the last read token
} parcerContext;

// Return the error right away if the call failed
#define CHECK_ERR(call)                         \
    do                                          \
    {                                           \
        int _err = (call);                      \
        if (_err != ERROR_SUCCESS) return _err; \
                                                \
    } while (0)

// Work with tokens, written in parser.c
int advance(parcerContext* ctx);
int peek(parcerContext* ctx);
int match(parcerContext* ctx, Type expectedType);

// Call arguments, written in parser.c
int parseCallArgs(parcerContext* ctx);

#endif
