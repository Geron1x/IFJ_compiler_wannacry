// Project: Implementace překladače imperativního jazyka IFJ26C
// Author:
// Heorhiy Haziy (xhaziyh00)
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bst.h"
#include "../scanner.h"

// GLOBAL VAR
typedef struct
{
    Token currentToken;
    Scanner* globalScanner;

} parcerContext;

// Macro for automatic error propagation
#define CHECK_ERR(call)             \
    do                              \
    {                               \
        int _err = (call);          \
        if (_err != 0) return _err; \
                                    \
    } while (0)

// Prototypes of internal functions

// Token helpers
void freeTokenValue(Token* token);
int advance(parcerContext* ctx);
int match(parcerContext* ctx, Type expectedType);
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

// Values and calls (call each other recursively)
int parseExprOrCall(parcerContext* ctx);
int parseCallArgs(parcerContext* ctx);

// point of entry in kompiler
int parse(void)
{
    // Initialize scanner state through its own init function
    Scanner scanner;
    scannerInit(&scanner, stdin);
    parcerContext ctx = {.currentToken = {.type = TYPE_ERROR, .line = 1},
                         .globalScanner = &scanner};

    // start parsing
    int err = parseProgram(&ctx);

    // Single cleanup point for success and error
    freeTokenValue(&ctx.currentToken);

    if (err == 0)
    {
        // just to test if program runned correctly
        fprintf(stderr, "Syntax analysis finished succesfully (code 0).\n");
    }
    return err;
}
// ============================================================
// 2. main function to parce
// ============================================================

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
            return 2;
        }
    }
    return 0;
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
        return 2;
    }

    CHECK_ERR(match(ctx, TYPE_DOT));
    CHECK_ERR(match(ctx, TYPE_MUL));
    CHECK_ERR(match(ctx, TYPE_EOL));
    return 0;
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

// stmt -> varDef | assign | call | bare identifier
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
            // Skip identifier name
            CHECK_ERR(advance(ctx));
            if (ctx->currentToken.type == TYPE_ASSIGNMENT)
            {
                // Assignment: id = value
                CHECK_ERR(advance(ctx));
                CHECK_ERR(parseExprOrCall(ctx));
            }
            else if (ctx->currentToken.type == TYPE_LEFT_PAREN)
            {
                // Function call: id(args)
                CHECK_ERR(parseCallArgs(ctx));
            }
            // Otherwise bare identifier, e.g. return value at end of body
            break;
        default:
            fprintf(stderr,
                    "Syntax err: unexpected statement in function body\n");
            return 2;
    }
    return 0;
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
    CHECK_ERR(parseExprOrCall(ctx));

    // Save to AST
    // CHECK_ERR(createNodeAST(valName, valType, valValue));

    return 0;
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
    CHECK_ERR(parseExprOrCall(ctx));

    // save to AST
    //     CHECK_ERR(createNodeAST(varName, varType, varValue));


    return 0;
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
        return 0;
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
        return 0;
    }

    fprintf(stderr,
            "Syntax err in line %d: expected end of statement (newline or "
            "';')\n",
            ctx->currentToken.line);
    return 2;
}

// value -> literal | '()' | ID [ callArgs ]
// Value of initialization, assignment or call argument
int parseExprOrCall(parcerContext* ctx)
{
    switch (ctx->currentToken.type)
    {
        // Literal is a complete value
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
            return advance(ctx);

        // Unit literal '()'
        case TYPE_LEFT_PAREN:
            CHECK_ERR(advance(ctx));
            return match(ctx, TYPE_RIGHT_PAREN);

        // Variable or function call
        case TYPE_IDENTIFIER:
            CHECK_ERR(advance(ctx));
            if (ctx->currentToken.type == TYPE_LEFT_PAREN)
            {
                // Recursion: arguments may contain calls
                return parseCallArgs(ctx);
            }
            return 0;

        default:
            fprintf(stderr,
                    "Syntax err in line %d: expected value (literal, variable "
                    "or call)\n",
                    ctx->currentToken.line);
            return 2;
    }
}

// callArgs -> '(' [ value { ',' value } [ ',' ] ] ')'
int parseCallArgs(parcerContext* ctx)
{
    CHECK_ERR(match(ctx, TYPE_LEFT_PAREN));

    // Parse arguments until ')'
    while (ctx->currentToken.type != TYPE_RIGHT_PAREN)
    {
        // Each argument is consumed or an error is returned, so no endless
        // loop on EOF
        CHECK_ERR(parseExprOrCall(ctx));

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
        return 2;
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
        return 2;
    }
}

// read next token
int advance(parcerContext* ctx)
{
    // Release previous token before overwriting it
    freeTokenValue(&ctx->currentToken);
    // Mark token as empty so a failed read cannot cause double free
    ctx->currentToken.type = TYPE_ERROR;

    int err = getToken(&(ctx->currentToken), ctx->globalScanner);
    if (err != 0)
    {
        // error in lexer
        fprintf(stderr, "Lexical err (line %d)\n", ctx->currentToken.line);
        return 1;
    }
    return 0;
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
