// Project: Implementace překladače imperativního jazyka IFJ26C
// Author:
// Heorhiy Haziy (xhaziyh00)
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "parser_internal.h"

// Prototypes of internal functions

// Token helpers
void freeTokenValue(Token* token);
int matchDataType(parcerContext* ctx);

// Program structure
int parseProgram(parcerContext* ctx);
int parsePrologue(parcerContext* ctx);
int parseFunction(parcerContext* ctx);

// Statements
int parseBlock(parcerContext* ctx);
int parseStatement(parcerContext* ctx);
int parseValue(parcerContext* ctx);
int parseVariable(parcerContext* ctx);
int parseStatementEnd(parcerContext* ctx);


// ============================================================
// 2. main function to parce
// ============================================================
// point of entry in kompiler
int parse(void)
{
    // Initialize scanner state through its own init function
    Scanner scanner;
    scannerInit(&scanner, stdin);
    parcerContext ctx = {.currentToken = {.type = TYPE_ERROR, .line = 1},
                         .globalScanner = &scanner,
                         .hasPeek = false};

    // start parsing
    int err = parseProgram(&ctx);

    // Single cleanup point for success and error
    freeTokenValue(&ctx.currentToken);
    // Free buffered lookahead token too
    if (ctx.hasPeek) freeTokenValue(&ctx.peekToken);

    if (err == ERROR_SUCCESS)
    {
        // just to test if program runned correctly
        fprintf(stderr, "Syntax analysis finished succesfully (code 0).\n");
    }
    return err;
}

// =============================================================
// 3. functions to parse the program
// ==============================================================

int parseProgram(parcerContext* ctx)
{
    CHECK_ERR(advance(ctx));  // first token of the program

    // if empty lines exists => skip them
    while (ctx->currentToken.type == TYPE_EOL)
    {
        CHECK_ERR(advance(ctx));
    }

    CHECK_ERR(parsePrologue(ctx));

    while (ctx->currentToken.type != TYPE_EOF)
    {
        // skip empty lines
        if (ctx->currentToken.type == TYPE_EOL)
        {
            CHECK_ERR(advance(ctx));
            continue;
        }

        // check for existion of annotation of @main
        if (ctx->currentToken.type == TYPE_MAIN)
        {
            // the main must be def as @main
            // if the line starts by @main parse function
            CHECK_ERR(advance(ctx));
            if (ctx->currentToken.type == TYPE_EOL) CHECK_ERR(advance(ctx));
            CHECK_ERR(parseFunction(ctx));
        }
        else if (ctx->currentToken.type == TYPE_DEF)
        {
            // if the line starts by def parse function
            CHECK_ERR(parseFunction(ctx));
        }
        else
        {
            fprintf(stderr, "Syntax err: function def expected (def)\n");
            return ERROR_SYNTAX;
        }
    }
    return ERROR_SUCCESS;
}

// 1. parse prologue
int parsePrologue(parcerContext* ctx)
{
    // the start of program is import
    CHECK_ERR(match(ctx, TYPE_IMPORT));
    // parse the correction of import name
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
    CHECK_ERR(match(ctx, TYPE_EOL));
    return ERROR_SUCCESS;
}

int parseFunction(parcerContext* ctx)
{
    CHECK_ERR(match(ctx, TYPE_DEF));
    CHECK_ERR(match(ctx, TYPE_IDENTIFIER));
    CHECK_ERR(match(ctx, TYPE_LEFT_PAREN));

    // check params . If no closing paren after opened=> function has
    // parameters=>parse them
    if (ctx->currentToken.type != TYPE_RIGHT_PAREN)
    {
        // according to the rules of implementation we parse first param
        CHECK_ERR(match(ctx, TYPE_IDENTIFIER));
        CHECK_ERR(match(ctx, TYPE_DOUBLE_DOT));
        CHECK_ERR(matchDataType(ctx));

        // only then if we have comma => parse another param
        while (ctx->currentToken.type == TYPE_COMMA)
        {
            CHECK_ERR(advance(ctx));  // skip comma

            // Trailing comma before ')' is allowed
            if (ctx->currentToken.type == TYPE_RIGHT_PAREN)
            {
                break;
            }
            CHECK_ERR(match(ctx, TYPE_IDENTIFIER));
            CHECK_ERR(match(ctx, TYPE_DOUBLE_DOT));
            CHECK_ERR(matchDataType(ctx));
        }
    }

    CHECK_ERR(match(ctx, TYPE_RIGHT_PAREN));  // closing paren

    // in function definition is possible po have return data type
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(matchDataType(ctx));
    }
    // def id(parametry) [: return-data-type] = (parse"=" in the end of def)
    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));
    CHECK_ERR(match(ctx, TYPE_EOL));  // must be /n after "="

    // Function body is an indented block
    return parseBlock(ctx);
}

// block -> INDENT stmt sep { stmt sep } DEDENT
// Indented block of statements (function body, later if/else/while)
int parseBlock(parcerContext* ctx)
{
    CHECK_ERR(match(ctx, TYPE_INDENT));

    // Block cannot be empty, so at least one statement is parsed
    do
    {
        CHECK_ERR(parseStatement(ctx));

        // Common end of every statement: EOL, ';' or block end
        CHECK_ERR(parseStatementEnd(ctx));
    } while (ctx->currentToken.type != TYPE_DEDENT);

    return match(ctx, TYPE_DEDENT);
}

// stmt -> varDef | assign | expr
// One statement of a block
int parseStatement(parcerContext* ctx)
{
    switch (ctx->currentToken.type)
    {
        case TYPE_VAR:
            CHECK_ERR(parseVariable(ctx));
            break;
        case TYPE_VAL:
            // parce var some code
            CHECK_ERR(parseValue(ctx));
            break;
        case TYPE_IDENTIFIER:
            // Look past identifier: assignment or expression
            CHECK_ERR(peek(ctx));
            if (ctx->peekToken.type == TYPE_ASSIGNMENT)
            {
                // Assignment: id = value
                CHECK_ERR(advance(ctx));  // identifier
                CHECK_ERR(advance(ctx));  // '='
                CHECK_ERR(parseExpression(ctx));
            }
            else
            {
                // Call or bare identifier (return value)
                CHECK_ERR(parseExpression(ctx));
            }
            break;
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
        case TYPE_LEFT_PAREN:
            // Expression statement
            CHECK_ERR(parseExpression(ctx));
            break;
        case TYPE_ADD:
        case TYPE_SUB:
            // Spec: logical line cannot start with operator
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

int parseValue(parcerContext* ctx)
{
    // next tocken and name check
    CHECK_ERR(advance(ctx));
    CHECK_ERR(match(ctx, TYPE_IDENTIFIER));

    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        // Skip ":"
        CHECK_ERR(advance(ctx));
        // Check valid type and advance
        CHECK_ERR(matchDataType(ctx));
    }

    // Parse "="
    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));

    // Initial value: literal, variable or call; statement end is checked by
    // caller
    CHECK_ERR(parseExpression(ctx));

    // Save to AST
    // CHECK_ERR(createNodeAST(valName, valType, valValue));

    return ERROR_SUCCESS;
}

int parseVariable(parcerContext* ctx)
{
    // next token to check name
    CHECK_ERR(advance(ctx));
    CHECK_ERR(match(ctx, TYPE_IDENTIFIER));


    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        // if type declaration exists parse it
        CHECK_ERR(advance(ctx));
        CHECK_ERR(matchDataType(ctx));
    }
    // parse "="
    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));

    // Initial value: literal, variable or call; statement end is checked by
    // caller
    CHECK_ERR(parseExpression(ctx));

    // save to AST
    //     CHECK_ERR(createNodeAST(varName, varType, varValue));


    return ERROR_SUCCESS;
}

// Statement separator: EOL | ';' [EOL] | before DEDENT/EOF
int parseStatementEnd(parcerContext* ctx)
{
    // ';' separates statements; ';' at end of line is allowed
    if (ctx->currentToken.type == TYPE_SEMICOLON)
    {
        CHECK_ERR(advance(ctx));
        if (ctx->currentToken.type == TYPE_EOL)
        {
            return advance(ctx);
        }
        return ERROR_SUCCESS;
    }

    // Newline ends the statement
    if (ctx->currentToken.type == TYPE_EOL)
    {
        return advance(ctx);
    }

    // Block end or file end terminates statement without consuming
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

// callArgs -> '(' [ expr { ',' expr } [ ',' ] ] ')'
int parseCallArgs(parcerContext* ctx)
{
    CHECK_ERR(match(ctx, TYPE_LEFT_PAREN));

    // Parse arguments until ')'
    while (ctx->currentToken.type != TYPE_RIGHT_PAREN)
    {
        // Each argument is consumed or an error is returned, so no endless
        // loop on EOF
        CHECK_ERR(parseExpression(ctx));

        // No comma means the list ends
        if (ctx->currentToken.type != TYPE_COMMA)
        {
            break;
        }
        // Skip comma; trailing comma before ')' is allowed
        CHECK_ERR(advance(ctx));
    }
    return match(ctx, TYPE_RIGHT_PAREN);
}

int matchDataType(parcerContext* ctx)
{
    if (ctx->currentToken.type == TYPE_INT ||
        ctx->currentToken.type == TYPE_DOUBLE ||
        ctx->currentToken.type == TYPE_STRING ||
        ctx->currentToken.type == TYPE_UNIT)
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

// function to check the type of token
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

// read next token
int advance(parcerContext* ctx)
{
    // Release previous token before overwriting it
    freeTokenValue(&ctx->currentToken);
    // Mark token as empty so a failed read cannot cause double free
    ctx->currentToken.type = TYPE_ERROR;

    // Take buffered token if peek already read it
    if (ctx->hasPeek)
    {
        // Heap string ownership moves to currentToken
        ctx->currentToken = ctx->peekToken;
        ctx->hasPeek = false;
        return ERROR_SUCCESS;
    }

    int err = getToken(&(ctx->currentToken), ctx->globalScanner);
    if (err != ERROR_SUCCESS)
    {
        // Pass scanner code through (1 lexical, 99 memory)
        if (err == ERROR_LEXICAL)
            fprintf(stderr, "Lexical err (line %d)\n", ctx->currentToken.line);
        return err;
    }
    return ERROR_SUCCESS;
}

// Fill lookahead buffer, read it via ctx->peekToken
int peek(parcerContext* ctx)
{
    // Already buffered, nothing to read
    if (ctx->hasPeek) return ERROR_SUCCESS;

    ctx->peekToken.type = TYPE_ERROR;
    int err = getToken(&ctx->peekToken, ctx->globalScanner);
    if (err != ERROR_SUCCESS)
    {
        // Pass scanner code through (1 lexical, 99 memory)
        if (err == ERROR_LEXICAL)
            fprintf(stderr, "Lexical err (line %d)\n", ctx->peekToken.line);
        return err;
    }
    ctx->hasPeek = true;
    return ERROR_SUCCESS;
}

// Free heap string owned by the token
void freeTokenValue(Token* token)
{
    // Only these token types carry malloc'ed strings
    if (token->type == TYPE_IDENTIFIER || token->type == TYPE_STRING_LITERAL)
    {
        // identifier and string share one slot in the union
        free(token->value.identifier);
        token->value.identifier = NULL;
    }
}
