/**
 * @file parser_internal.h
 * @brief Parser state and token helpers shared by parser.c and expression.c
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Heorhiy Haziy (xhaziyh00)
 *
 */

#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include <stdbool.h>

#include "ast.h"
#include "error.h"
#include "scanner.h"

/**
 * @struct parcerContext
 * @brief Parser state shared by parser.c and expression.c.
 */
typedef struct
{
    Token currentToken;      ///< token being processed now
    Scanner* globalScanner;  ///< scanner that gives the tokens
    Token peekToken;         ///< next token read ahead
    bool hasPeek;            ///< true if peekToken is filled
    Type prevType;           ///< type of the last read token
} parcerContext;

/**
 * @def CHECK_ERR
 * @brief Returns the error code right away if the call failed.
 */
#define CHECK_ERR(call)                         \
    do                                          \
    {                                           \
        int _err = (call);                      \
        if (_err != ERROR_SUCCESS) return _err; \
                                                \
    } while (0)

/**
 * @brief Moves to the next token, the old one is freed.
 *
 * @param ctx Parser state.
 * @return ERROR_SUCCESS or the error code from the scanner.
 */
int advance(parcerContext* ctx);

/**
 * @brief Reads the next token ahead without moving to it.
 *
 * After success the token is in ctx->peekToken.
 *
 * @param ctx Parser state.
 * @return ERROR_SUCCESS or the error code from the scanner.
 */
int peek(parcerContext* ctx);

/**
 * @brief Checks that the current token has the expected type and moves on.
 *
 * @param ctx Parser state.
 * @param expectedType Type the current token must have.
 * @return ERROR_SUCCESS, ERROR_SYNTAX or the error code from the scanner.
 */
int match(parcerContext* ctx, Type expectedType);

/**
 * @brief Parses a value that may also be an if with else.
 *
 * @param ctx Parser state, current token starts the value.
 * @param out Receives the tree of the value on success.
 * @return ERROR_SUCCESS, ERROR_LEXICAL, ERROR_SYNTAX or ERROR_INTERNAL.
 */
int parseExpressionOrIf(parcerContext* ctx, astNode** out);

/**
 * @brief Parses if used as a value, the else branch is required.
 *
 * @param ctx Parser state, current token is if.
 * @param out Receives the if expression node on success.
 * @return ERROR_SUCCESS, ERROR_LEXICAL, ERROR_SYNTAX or ERROR_INTERNAL.
 */
int parseIfExpression(parcerContext* ctx, astNode** out);

/**
 * @brief Parses a lambda after its opening parenthesis: parameters, ), =>,
 * body.
 *
 * @param ctx Parser state, current token follows the opening parenthesis.
 * @param out Receives the lambda node on success.
 * @return ERROR_SUCCESS, ERROR_LEXICAL, ERROR_SYNTAX or ERROR_INTERNAL.
 */
int parseLambda(parcerContext* ctx, astNode** out);

/**
 * @brief Parses call arguments: values in parentheses separated by commas.
 *
 * @param ctx Parser state, current token is the opening parenthesis.
 * @param call Call node, every argument is appended to it as a child.
 * @return ERROR_SUCCESS, ERROR_LEXICAL, ERROR_SYNTAX or ERROR_INTERNAL.
 */
int parseCallArgs(parcerContext* ctx, astNode* call);

/**
 * @brief Appends a child to a node, frees the child if appending fails.
 *
 * @param parent Node that receives the child.
 * @param child Node to append, owned by the parent on success.
 * @return ERROR_SUCCESS or ERROR_INTERNAL.
 */
int appendChild(astNode* parent, astNode* child);

/**
 * @brief Joins a new part to the result so far with a binary operator.
 *
 * The first part becomes the whole result. Joins guards of a for loop with &&
 * and parts of an interpolated string with +.
 *
 * @param result Result so far, NULL before the first part.
 * @param op Operator that joins the parts.
 * @param part New part, owned by the result on success, freed on failure.
 * @return ERROR_SUCCESS or ERROR_INTERNAL.
 */
int joinBinary(astNode** result, astOpType op, astNode* part);

#endif
