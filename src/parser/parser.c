// Project: Implementace překladače imperativního jazyka IFJ26C
// Author:
// Heorhiy Haziy (xhaziyh00)
#include <stdio.h>
#include <string.h>

#include "../scanner.h"

// Macro for automatic error propagation
#define CHECK_ERR(call)             \
    do                              \
    {                               \
        int _err = (call);          \
        if (_err != 0) return _err; \
    } while (0)

// GLOBAL VAR
typedef struct
{
    Token currentToken;
    Scanner* globalScanner;

} parcerContext;

// read next token
int advance(parcerContext* ctx)
{
    int err = getToken(&(ctx->currentToken), ctx->globalScanner);
    if (err != 0)
    {
        // error in lexer
        fprintf(stderr, "Lexical err (line %d)\n", ctx->currentToken.line);
        return 1;
    }
    return 0;
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

int matchDataType(parcerContext* ctx)
{
    if (ctx->currentToken.type == TYPE_INT ||
        ctx->currentToken.type == TYPE_DOUBLE ||
        ctx->currentToken.type == TYPE_STRING)
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
int parseValue(parcerContext* ctx)
{
    // next tocken and name check
    CHECK_ERR(advance(ctx));
    CHECK_ERR(match(ctx, TYPE_IDENTIFIER));


    // If name doesnt exist parse type
    CHECK_ERR(advance(ctx));
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        // Skip ":"
        CHECK_ERR(advance(ctx));
        // Check valid type and advance
        CHECK_ERR(matchDataType(ctx));
    }

    // Parse "="
    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));


    // Advance past the value token
    CHECK_ERR(advance(ctx));

    // Check end of statement (newline)
    CHECK_ERR(match(ctx, TYPE_EOL));

    // Save to AST
    // CHECK_ERR(createNodeAST(valName, valType, valValue));

    return 0;
}

int parseVariable(parcerContext* ctx)
{
    // next token to check name
    CHECK_ERR(advance(ctx));
    CHECK_ERR(match(ctx, TYPE_IDENTIFIER));


    CHECK_ERR(advance(ctx));
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        // if type declaration exists parse it
        CHECK_ERR(advance(ctx));
        CHECK_ERR(matchDataType(ctx));
    }
    // parse "="
    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));


    // Advance past the value token
    CHECK_ERR(advance(ctx));

    // Check end of statement (newline)

    CHECK_ERR(match(ctx, TYPE_EOL));

    // save to AST
    //     CHECK_ERR(createNodeAST(varName, varType, varValue));


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

    CHECK_ERR(match(
        ctx,
        TYPE_ASSIGNMENT));  // def id(parametry) [: return-data-type] = (parse
                            // "=" in the end of def)
    CHECK_ERR(match(ctx, TYPE_EOL));     // must be /n after "="
    CHECK_ERR(match(ctx, TYPE_INDENT));  // space neccecary
    while (ctx->currentToken.type != TYPE_DEDENT)
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
            default:
                fprintf(stderr,
                        "Syntax err: unexpected statement in function body\n");
                return 2;
        }
    }
    CHECK_ERR(match(ctx, TYPE_DEDENT));
    return 0;
}


// 2. main function to parce

int parseProgram(parcerContext* ctx)
{
    CHECK_ERR(advance(ctx));  // first tokrn of the program

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

// point of entry in kompiler
int parse()
{
    Scanner scanner;
    scanner.stream = stdin;
    scanner.line = 1;
    parcerContext ctx;
    ctx.globalScanner = &scanner;

    // start parsing
    CHECK_ERR(parseProgram(&ctx));
    // just to test if program runned correctly
    printf("Syntax analysis finished succesfully (code 0).\n");
    return 0;
}