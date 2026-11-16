// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "parser_debug.h"
#include "parser_internal.h"

// Functions used only inside this file

// Work with tokens
void freeTokenValue(Token* token);
int matchDataType(parcerContext* ctx);
int checkTrailingComma(parcerContext* ctx, int commaLine);

// Program, prologue and functions
int parseProgram(parcerContext* ctx);
int parsePrologue(parcerContext* ctx);
int parseFunction(parcerContext* ctx);

// Statements inside function body
int parseBlock(parcerContext* ctx);
int parseStatement(parcerContext* ctx);
int parseValue(parcerContext* ctx);
int parseVariable(parcerContext* ctx);
int parseReturn(parcerContext* ctx);
int parseIf(parcerContext* ctx);
int parseWhile(parcerContext* ctx);
int parseBranch(parcerContext* ctx);
int parseStatementEnd(parcerContext* ctx);


// ============================================================
// Entry point of the parser
// ============================================================

// Check the program from stdin, build its tree and return error code
int parse(astNode** out)
{
    // Caller gets nothing until the whole program is parsed
    if (out == NULL) return ERROR_INTERNAL;
    *out = NULL;

    // Create the root node of the program tree
    astNode* prog = NULL;
    CHECK_ERR(astCreateProgram(&prog));

    // Prepare scanner and parser state
    Scanner scanner;
    scannerInit(&scanner, stdin);
    parcerContext ctx = {.currentToken = {.type = TYPE_ERROR, .line = 1},
                         .globalScanner = &scanner,
                         .hasPeek = false,
                         .prevType = TYPE_ERROR};

    // Parse the whole program
    int err = parseProgram(&ctx);

    // Free tokens in one place for success and error
    freeTokenValue(&ctx.currentToken);
    if (ctx.hasPeek) freeTokenValue(&ctx.peekToken);

    // On error free the partial tree
    if (err != ERROR_SUCCESS)
    {
        astDestroy(prog);
        return err;
    }

    // Message for testing only
    fprintf(stderr, "Syntax analysis finished succesfully (code 0).\n");

    // Print the tree only in a build with -DAST_DEBUG
#ifdef AST_DEBUG
    astDump(prog, 0);
#endif

    // Give the finished tree to the caller
    *out = prog;
    return err;
}

// ============================================================
// Program structure
// ============================================================

// Program is prologue and then functions
int parseProgram(parcerContext* ctx)
{
    // Read the first token
    CHECK_ERR(advance(ctx));

    // Skip empty lines before prologue
    while (ctx->currentToken.type == TYPE_EOL)
    {
        CHECK_ERR(advance(ctx));
    }

    CHECK_ERR(parsePrologue(ctx));

    while (ctx->currentToken.type != TYPE_EOF)
    {
        // Skip empty lines between functions
        if (ctx->currentToken.type == TYPE_EOL)
        {
            CHECK_ERR(advance(ctx));
            continue;
        }

        // Main function starts with @main, def may be on next line
        if (ctx->currentToken.type == TYPE_MAIN)
        {
            CHECK_ERR(advance(ctx));
            if (ctx->currentToken.type == TYPE_EOL) CHECK_ERR(advance(ctx));
            CHECK_ERR(parseFunction(ctx));
        }
        // Ordinary function starts with def
        else if (ctx->currentToken.type == TYPE_DEF)
        {
            CHECK_ERR(parseFunction(ctx));
        }
        // Nothing else is allowed here
        else
        {
            fprintf(stderr, "Syntax err: function def expected (def)\n");
            return ERROR_SYNTAX;
        }
    }
    return ERROR_SUCCESS;
}

// Prologue is the line import ifj26.*
int parsePrologue(parcerContext* ctx)
{
    CHECK_ERR(match(ctx, TYPE_IMPORT));

    // Only the name ifj26 is allowed
    if (ctx->currentToken.type == TYPE_IDENTIFIER &&
        strcmp(ctx->currentToken.value.identifier, "ifj26") == 0)
    {
        CHECK_ERR(advance(ctx));
    }
    else
    {
        fprintf(stderr, "Syntax err: the idintifier needed 'ifj26'\n");
        return ERROR_SYNTAX;
    }

    CHECK_ERR(match(ctx, TYPE_DOT));
    CHECK_ERR(match(ctx, TYPE_MUL));

    // File may end right after prologue, missing main is checked later
    if (ctx->currentToken.type == TYPE_EOF)
    {
        return ERROR_SUCCESS;
    }
    return match(ctx, TYPE_EOL);
}

// Function: def, name, parameters, return type, =, body
int parseFunction(parcerContext* ctx)
{
    CHECK_ERR(match(ctx, TYPE_DEF));
    CHECK_ERR(match(ctx, TYPE_IDENTIFIER));
    CHECK_ERR(match(ctx, TYPE_LEFT_PAREN));

    // Parameters exist if the parenthesis is not closed right away
    if (ctx->currentToken.type != TYPE_RIGHT_PAREN)
    {
        // First parameter: name and type
        CHECK_ERR(match(ctx, TYPE_IDENTIFIER));
        CHECK_ERR(match(ctx, TYPE_DOUBLE_DOT));
        CHECK_ERR(matchDataType(ctx));

        // Each comma brings one more parameter
        while (ctx->currentToken.type == TYPE_COMMA)
        {
            int commaLine = ctx->currentToken.line;
            CHECK_ERR(advance(ctx));

            // Comma before ) only with new line
            if (ctx->currentToken.type == TYPE_RIGHT_PAREN)
            {
                CHECK_ERR(checkTrailingComma(ctx, commaLine));
                break;
            }
            CHECK_ERR(match(ctx, TYPE_IDENTIFIER));
            CHECK_ERR(match(ctx, TYPE_DOUBLE_DOT));
            CHECK_ERR(matchDataType(ctx));
        }
    }

    CHECK_ERR(match(ctx, TYPE_RIGHT_PAREN));

    // Return type is optional
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(matchDataType(ctx));
    }

    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));

    // New line after = means a block body
    if (ctx->currentToken.type == TYPE_EOL)
    {
        CHECK_ERR(advance(ctx));
        return parseBlock(ctx);
    }

    // Otherwise the body is one expression on the same line
    CHECK_ERR(parseExpression(ctx, NULL));

    // Nothing else may follow it on this line
    if (ctx->currentToken.type != TYPE_EOL &&
        ctx->currentToken.type != TYPE_EOF)
    {
        fprintf(stderr,
                "Syntax error in line %d: expected end of line after function "
                "body\n",
                ctx->currentToken.line);
        return ERROR_SYNTAX;
    }
    return ERROR_SUCCESS;
}

// ============================================================
// Statements
// ============================================================

// Block of statements with indent: body of a function, if, else or while
int parseBlock(parcerContext* ctx)
{
    CHECK_ERR(match(ctx, TYPE_INDENT));

    // Block cannot be empty, so read at least one statement
    do
    {
        CHECK_ERR(parseStatement(ctx));

        // After if or while the inner block already ended the line
        if (ctx->prevType == TYPE_DEDENT)
        {
            continue;
        }

        // Every statement ends with new line, ; or end of block
        CHECK_ERR(parseStatementEnd(ctx));
    } while (ctx->currentToken.type != TYPE_DEDENT);

    return match(ctx, TYPE_DEDENT);
}

// One statement: val, var, assignment, return, if, while or expression
int parseStatement(parcerContext* ctx)
{
    switch (ctx->currentToken.type)
    {
        case TYPE_VAR:
            CHECK_ERR(parseVariable(ctx));
            break;
        case TYPE_VAL:
            CHECK_ERR(parseValue(ctx));
            break;
        // Name can start assignment or expression
        case TYPE_IDENTIFIER:
            CHECK_ERR(peek(ctx));
            if (ctx->peekToken.type == TYPE_ASSIGNMENT)
            {
                // Assignment: skip name and =, then read value
                CHECK_ERR(advance(ctx));
                CHECK_ERR(advance(ctx));
                CHECK_ERR(parseExpression(ctx, NULL));
            }
            else
            {
                // Function call or just a name
                CHECK_ERR(parseExpression(ctx, NULL));
            }
            break;
        case TYPE_RETURN:
            CHECK_ERR(parseReturn(ctx));
            break;
        case TYPE_IF:
            CHECK_ERR(parseIf(ctx));
            break;
        case TYPE_WHILE:
            CHECK_ERR(parseWhile(ctx));
            break;
        // Line with a value is an expression statement
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
        case TYPE_TRUE:
        case TYPE_FALSE:
        case TYPE_LEFT_PAREN:
            CHECK_ERR(parseExpression(ctx, NULL));
            break;
        // Line cannot start with an operator
        case TYPE_ADD:
        case TYPE_SUB:
        case TYPE_EXCLAMATION:
            fprintf(stderr,
                    "Syntax err in line %d: line cannot start with operator\n",
                    ctx->currentToken.line);
            return ERROR_SYNTAX;
        default:
            fprintf(stderr,
                    "Syntax err: unexpected statement in function body\n");
            return ERROR_SYNTAX;
    }
    return ERROR_SUCCESS;
}

// Constant: val, name, optional type, =, value
int parseValue(parcerContext* ctx)
{
    // Skip val and read the name
    CHECK_ERR(advance(ctx));
    CHECK_ERR(match(ctx, TYPE_IDENTIFIER));

    // Type after colon is optional
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(matchDataType(ctx));
    }

    // Value after = is required
    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));
    CHECK_ERR(parseExpression(ctx, NULL));

    // Here the constant will be saved to AST later
    // CHECK_ERR(createNodeAST(valName, valType, valValue));

    return ERROR_SUCCESS;
}

// Variable: var, name, optional type, =, value
int parseVariable(parcerContext* ctx)
{
    // Skip var and read the name
    CHECK_ERR(advance(ctx));
    CHECK_ERR(match(ctx, TYPE_IDENTIFIER));


    // Type after colon is optional
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(matchDataType(ctx));
    }

    // Value after = is required
    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));
    CHECK_ERR(parseExpression(ctx, NULL));

    // Here the variable will be saved to AST later
    //     CHECK_ERR(createNodeAST(varName, varType, varValue));


    return ERROR_SUCCESS;
}

// Return: the word return and maybe a value
int parseReturn(parcerContext* ctx)
{
    CHECK_ERR(advance(ctx));

    // Value is read only if the next token can start an expression
    if (canStartExpression(ctx->currentToken.type))
    {
        CHECK_ERR(parseExpression(ctx, NULL));
    }

    return ERROR_SUCCESS;
}

// if, condition, then, body, and maybe else or else if
int parseIf(parcerContext* ctx)
{
    // Condition goes until then
    CHECK_ERR(advance(ctx));
    CHECK_ERR(parseExpression(ctx, NULL));
    CHECK_ERR(match(ctx, TYPE_THEN));
    CHECK_ERR(parseBranch(ctx));

    // No else means the if is finished
    if (ctx->currentToken.type != TYPE_ELSE)
    {
        return ERROR_SUCCESS;
    }
    CHECK_ERR(advance(ctx));

    // else if is just another if after else
    // else
    if (ctx->currentToken.type == TYPE_IF)
    {
        return parseIf(ctx);
    }
    return parseBranch(ctx);
}

// while, condition, do, body
int parseWhile(parcerContext* ctx)
{
    // Condition goes until do
    CHECK_ERR(advance(ctx));
    CHECK_ERR(parseExpression(ctx, NULL));
    CHECK_ERR(match(ctx, TYPE_DO));
    return parseBranch(ctx);
}

// Body of if, else or while: new line, then indented block
int parseBranch(parcerContext* ctx)
{
    CHECK_ERR(match(ctx, TYPE_EOL));
    return parseBlock(ctx);
}

// End of statement: new line, ; or end of block or file
int parseStatementEnd(parcerContext* ctx)
{
    // ; separates statements, a new line may follow it
    if (ctx->currentToken.type == TYPE_SEMICOLON)
    {
        CHECK_ERR(advance(ctx));
        if (ctx->currentToken.type == TYPE_EOL)
        {
            return advance(ctx);
        }
        return ERROR_SUCCESS;
    }

    // New line ends the statement
    if (ctx->currentToken.type == TYPE_EOL)
    {
        return advance(ctx);
    }

    // End of block or file also ends it, but is not consumed here
    if (ctx->currentToken.type == TYPE_DEDENT ||
        ctx->currentToken.type == TYPE_EOF)
    {
        return ERROR_SUCCESS;
    }

    fprintf(stderr,
            "Syntax err in line %d: expected end of statement (newline or "
            "';')\n",
            ctx->currentToken.line);
    return ERROR_SYNTAX;
}

// Call arguments: values in parentheses separated by commas
int parseCallArgs(parcerContext* ctx, astNode* call)
{
    CHECK_ERR(match(ctx, TYPE_LEFT_PAREN));

    // Read arguments until the closing parenthesis
    while (ctx->currentToken.type != TYPE_RIGHT_PAREN)
    {
        // Each argument becomes a child of the call node
        astNode* arg = NULL;
        CHECK_ERR(parseExpression(ctx, &arg));
        CHECK_ERR(appendChild(call, arg));

        // No comma means the list is over
        if (ctx->currentToken.type != TYPE_COMMA)
        {
            break;
        }
        // Skip comma, comma before ) only with new line
        int commaLine = ctx->currentToken.line;
        CHECK_ERR(advance(ctx));
        if (ctx->currentToken.type == TYPE_RIGHT_PAREN)
        {
            CHECK_ERR(checkTrailingComma(ctx, commaLine));
        }
    }
    return match(ctx, TYPE_RIGHT_PAREN);
}

// Append a child, if it fails free the child so nothing leaks
int appendChild(astNode* parent, astNode* child)
{
    int err = astBlockAppend(parent, child);
    if (err != ERROR_SUCCESS) astDestroy(child);
    return err;
}

// Comma before ) is allowed only if ) is on a later line
int checkTrailingComma(parcerContext* ctx, int commaLine)
{
    if (ctx->currentToken.line == commaLine)
    {
        fprintf(stderr,
                "Syntax error in line %d: trailing comma needs a new line "
                "before ')'\n",
                commaLine);
        return ERROR_SYNTAX;
    }
    return ERROR_SUCCESS;
}

// ============================================================
// Work with tokens
// ============================================================

// Data type: Int, Double, String, Unit or Boolean
int matchDataType(parcerContext* ctx)
{
    if (ctx->currentToken.type == TYPE_INT ||
        ctx->currentToken.type == TYPE_DOUBLE ||
        ctx->currentToken.type == TYPE_STRING ||
        ctx->currentToken.type == TYPE_UNIT ||
        ctx->currentToken.type == TYPE_BOOLEAN)
    {
        return advance(ctx);
    }
    else
    {
        fprintf(stderr, "Syntax error in line %d: expected data type\n",
                ctx->currentToken.line);
        return ERROR_SYNTAX;
    }
}

// Check that the token has the expected type and move on
int match(parcerContext* ctx, Type expectedType)
{
    if (ctx->currentToken.type == expectedType)
    {
        return advance(ctx);
    }
    else
    {
        fprintf(stderr, "Syntax err in line  %d: unpredicted token\n",
                ctx->currentToken.line);
        return ERROR_SYNTAX;
    }
}

// Move to the next token
int advance(parcerContext* ctx)
{
    // Remember which token we are leaving
    ctx->prevType = ctx->currentToken.type;

    // Free the old token before replacing it
    freeTokenValue(&ctx->currentToken);
    // Mark it empty so a failed read cannot free it twice
    ctx->currentToken.type = TYPE_ERROR;

    // If peek already read the next token, take it
    if (ctx->hasPeek)
    {
        // The string now belongs to currentToken
        ctx->currentToken = ctx->peekToken;
        ctx->hasPeek = false;
        return ERROR_SUCCESS;
    }

    // Otherwise ask the scanner
    int err = getToken(&(ctx->currentToken), ctx->globalScanner);
    if (err != ERROR_SUCCESS)
    {
        // Return scanner code as is: 1 lexical, 99 memory
        if (err == ERROR_LEXICAL)
            fprintf(stderr, "Lexical err (line %d)\n", ctx->currentToken.line);
        return err;
    }
    return ERROR_SUCCESS;
}

// Look at the next token without moving, result is in ctx->peekToken
int peek(parcerContext* ctx)
{
    // Already read, nothing to do
    if (ctx->hasPeek) return ERROR_SUCCESS;

    ctx->peekToken.type = TYPE_ERROR;
    int err = getToken(&ctx->peekToken, ctx->globalScanner);
    if (err != ERROR_SUCCESS)
    {
        // Return scanner code as is: 1 lexical, 99 memory
        if (err == ERROR_LEXICAL)
            fprintf(stderr, "Lexical err (line %d)\n", ctx->peekToken.line);
        return err;
    }
    ctx->hasPeek = true;
    return ERROR_SUCCESS;
}

// Free the string that the token owns
void freeTokenValue(Token* token)
{
    // Only names and strings have allocated memory
    if (token->type == TYPE_IDENTIFIER || token->type == TYPE_STRING_LITERAL)
    {
        free(token->value.identifier);
        token->value.identifier = NULL;
    }
}
