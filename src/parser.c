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

// Parameter nodes kept until the function node exists
typedef struct
{
    astNode** items;
    size_t count;
    size_t capacity;
} ParamList;

#define PARAM_LIST_INIT_CAPACITY 4

// Parts of a for loop collected before its node is made
typedef struct
{
    char* name;        // loop variable
    astNode* from;     // range start or the list
    astNode* end;      // range end, NULL for a list
    astNode* step;     // step after by or NULL
    astNode* guard;    // guards joined with and or NULL
    astNode* body;     // loop body block
    bool isInclusive;  // true for to, false for until
    bool isRange;      // false when the source is a list
} ForParts;

// Work with tokens
static int readToken(parcerContext* ctx, Token* token);
static void freeTokenValue(Token* token);
static int matchDataType(parcerContext* ctx, dataType* type);
static int takeName(parcerContext* ctx, char** name);
static int checkTrailingComma(parcerContext* ctx, int commaLine);

// Types: simple, list and function
static int parseType(parcerContext* ctx, dataType* kind, typeDesc** desc);
static int parseTypeDesc(parcerContext* ctx, dataType* kind, typeDesc** desc);
static int parseListType(parcerContext* ctx, dataType* kind, typeDesc** desc);
static int parseFuncType(parcerContext* ctx, dataType* kind, typeDesc** desc);
static int parseTypeList(parcerContext* ctx, typeDesc*** list, int* count);
static void typeListFree(typeDesc** list, int count);

// Program, prologue and functions
static int parseProgram(parcerContext* ctx, astNode* prog);
static int parsePrologue(parcerContext* ctx);
static int addFunction(parcerContext* ctx, astNode* prog, bool isMain);
static int parseFunction(parcerContext* ctx, bool isMain, astNode** out);
static int parseFunctionParts(parcerContext* ctx, ParamList* params,
                              dataType* retType, typeDesc** retDesc,
                              astNode** body);
static int parseParams(parcerContext* ctx, ParamList* params);
static int parseParam(parcerContext* ctx, ParamList* params);
static int parseFunctionBody(parcerContext* ctx, astNode** out);
static int buildFunction(const char* name, dataType retType, typeDesc* retDesc,
                         bool isMain, astNode* body, ParamList* params,
                         astNode** out);
static int buildLambda(astNode* body, ParamList* params, astNode** out);
static int appendParams(astNode* node, ParamList* params);
static int paramListPush(ParamList* list, astNode* param);
static void paramListFree(ParamList* list);
static int wrapInBlock(astNode* node, astNode** out);

// Statements inside function body
static int parseBlock(parcerContext* ctx, astNode** out);
static int parseBlockStatements(parcerContext* ctx, astNode* block);
static int parseStatement(parcerContext* ctx, astNode** out);
static int parseValue(parcerContext* ctx, astNode** out);
static int parseVariable(parcerContext* ctx, astNode** out);
static int parseTypeAndValue(parcerContext* ctx, dataType* type,
                             typeDesc** desc, astNode** expr);
static int parseAssignment(parcerContext* ctx, astNode** out);
static int parseReturn(parcerContext* ctx, astNode** out);
static int parseIf(parcerContext* ctx, astNode** out);
static int parseIfParts(parcerContext* ctx, astNode** cond, astNode** thenBlock,
                        astNode** elseBlock);
static int parseElseIf(parcerContext* ctx, astNode** out);
static int parseWhile(parcerContext* ctx, astNode** out);
static int parseFor(parcerContext* ctx, astNode** out);
static int parseForHeader(parcerContext* ctx, ForParts* parts);
static int parseForSource(parcerContext* ctx, ForParts* parts);
static int parseForGuards(parcerContext* ctx, bool isMultiLine,
                          astNode** guard);
static int buildFor(ForParts* parts, astNode** out);
static void forPartsFree(ForParts* parts);
static int parseBranch(parcerContext* ctx, astNode** out);
static int parseOneLineBranch(parcerContext* ctx, astNode** out);
static int skipNewLineBeforeElse(parcerContext* ctx);
static int parseIfExpressionParts(parcerContext* ctx, astNode** cond,
                                  astNode** thenBlock, astNode** elseBlock);
static int parseBlockOrValue(parcerContext* ctx, astNode** out);
static int parseStatementEnd(parcerContext* ctx);


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
    int err = parseProgram(&ctx, prog);

    // Free tokens in one place for success and error
    freeTokenValue(&ctx.currentToken);
    if (ctx.hasPeek) freeTokenValue(&ctx.peekToken);

    // On error free the partial tree
    if (err != ERROR_SUCCESS)
    {
        astDestroy(prog);
        return err;
    }


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

// Program is prologue and then functions, each function is added to prog
static int parseProgram(parcerContext* ctx, astNode* prog)
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
            CHECK_ERR(addFunction(ctx, prog, true));
        }
        // Ordinary function starts with def
        else if (ctx->currentToken.type == TYPE_DEF)
        {
            CHECK_ERR(addFunction(ctx, prog, false));
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
static int parsePrologue(parcerContext* ctx)
{
    CHECK_ERR(match(ctx, TYPE_IMPORT));

    // Only the name ifj26 is allowed
    if (!isWord(ctx, "ifj26"))
    {
        fprintf(stderr, "Syntax err: the idintifier needed 'ifj26'\n");
        return ERROR_SYNTAX;
    }
    CHECK_ERR(advance(ctx));

    CHECK_ERR(match(ctx, TYPE_DOT));
    CHECK_ERR(match(ctx, TYPE_MUL));

    // File may end right after prologue, missing main is checked later
    if (ctx->currentToken.type == TYPE_EOF)
    {
        return ERROR_SUCCESS;
    }
    return match(ctx, TYPE_EOL);
}

// Parse one function and add it to the program
static int addFunction(parcerContext* ctx, astNode* prog, bool isMain)
{
    astNode* func = NULL;
    CHECK_ERR(parseFunction(ctx, isMain, &func));
    return appendChild(prog, func);
}

// Function: def, name, parameters, return type, =, body
static int parseFunction(parcerContext* ctx, bool isMain, astNode** out)
{
    // Take the name, it must be freed on every return below
    CHECK_ERR(match(ctx, TYPE_DEF));
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    // Without a written return type the semantics finds it out
    ParamList params = {.items = NULL, .count = 0, .capacity = 0};
    dataType retType = S_TYPE_UNKNOWN;
    typeDesc* retDesc = NULL;
    astNode* body = NULL;
    int err = parseFunctionParts(ctx, &params, &retType, &retDesc, &body);

    // Function node takes the body, the parameters and the return type
    if (err == ERROR_SUCCESS)
    {
        err = buildFunction(name, retType, retDesc, isMain, body, &params, out);
    }
    // Return type not given to the function is freed here
    else
    {
        typeDescDestroy(retDesc);
    }

    // Parameters not given to the function are freed here
    paramListFree(&params);
    free(name);
    return err;
}

// Parameters in parentheses, optional return type, = and the body
static int parseFunctionParts(parcerContext* ctx, ParamList* params,
                              dataType* retType, typeDesc** retDesc,
                              astNode** body)
{
    CHECK_ERR(match(ctx, TYPE_LEFT_PAREN));
    CHECK_ERR(parseParams(ctx, params));
    CHECK_ERR(match(ctx, TYPE_RIGHT_PAREN));

    // Return type is optional
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(parseType(ctx, retType, retDesc));
    }

    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));
    return parseFunctionBody(ctx, body);
}

// Parameters separated by commas, comma before ) only with new line
static int parseParams(parcerContext* ctx, ParamList* params)
{
    // Parameters exist if the parenthesis is not closed right away
    if (ctx->currentToken.type == TYPE_RIGHT_PAREN)
    {
        return ERROR_SUCCESS;
    }
    CHECK_ERR(parseParam(ctx, params));

    // Each comma brings one more parameter
    while (ctx->currentToken.type == TYPE_COMMA)
    {
        int commaLine = ctx->currentToken.line;
        CHECK_ERR(advance(ctx));

        // Comma before ) only with new line
        if (ctx->currentToken.type == TYPE_RIGHT_PAREN)
        {
            return checkTrailingComma(ctx, commaLine);
        }
        CHECK_ERR(parseParam(ctx, params));
    }
    return ERROR_SUCCESS;
}

// One parameter: name, colon, type, kept in the list as a constant
static int parseParam(parcerContext* ctx, ParamList* params)
{
    // Take the name, it must be freed on every return below
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    dataType type = S_TYPE_UNKNOWN;
    typeDesc* desc = NULL;
    int err = match(ctx, TYPE_DOUBLE_DOT);
    if (err == ERROR_SUCCESS) err = parseType(ctx, &type, &desc);

    // Parameter is a constant without a value
    astNode* param = NULL;
    if (err == ERROR_SUCCESS)
    {
        err = astCreateVarDef(name, type, true, NULL, &param);
    }
    // Node takes the type, then goes to the list
    if (err == ERROR_SUCCESS)
    {
        astSetTypeDesc(param, desc);
        desc = NULL;
        err = paramListPush(params, param);
    }

    typeDescDestroy(desc);
    free(name);
    return err;
}

// Body after =: indented block, or one value on the same line
static int parseFunctionBody(parcerContext* ctx, astNode** out)
{
    CHECK_ERR(parseBlockOrValue(ctx, out));

    // Nothing else may follow a one-line body, a block already ended the line
    if (ctx->currentToken.type != TYPE_EOL &&
        ctx->currentToken.type != TYPE_EOF && ctx->prevType != TYPE_DEDENT)
    {
        astDestroy(*out);
        *out = NULL;
        fprintf(stderr,
                "Syntax error in line %d: expected end of line after function "
                "body\n",
                ctx->currentToken.line);
        return ERROR_SYNTAX;
    }
    return ERROR_SUCCESS;
}

// Make the function node, then give it the parameters one by one
static int buildFunction(const char* name, dataType retType, typeDesc* retDesc,
                         bool isMain, astNode* body, ParamList* params,
                         astNode** out)
{
    // On failure the body and the return type still belong to us
    astNode* func = NULL;
    int err = astCreateFuncDef(name, retType, body, &func, isMain);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(body);
        typeDescDestroy(retDesc);
        return err;
    }
    // Function node owns the return type now
    astSetTypeDesc(func, retDesc);

    // Function node takes the parameters
    err = appendParams(func, params);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(func);
        return err;
    }
    *out = func;
    return ERROR_SUCCESS;
}

// Lambda after its (: parameters, ), =>, body
int parseLambda(parcerContext* ctx, astNode** out)
{
    // Parameters look like function parameters, each has a type
    ParamList params = {.items = NULL, .count = 0, .capacity = 0};
    astNode* body = NULL;
    int err = parseParams(ctx, &params);
    if (err == ERROR_SUCCESS) err = match(ctx, TYPE_RIGHT_PAREN);
    if (err == ERROR_SUCCESS) err = match(ctx, TYPE_ARROW);
    if (err == ERROR_SUCCESS) err = parseBlockOrValue(ctx, &body);
    if (err == ERROR_SUCCESS) err = buildLambda(body, &params, out);

    // Parameters not given to the lambda are freed here
    paramListFree(&params);
    return err;
}

// Lambda node: body first, then the parameters, return type is found by
// semantics
static int buildLambda(astNode* body, ParamList* params, astNode** out)
{
    // On failure the body still belongs to us
    astNode* lambda = NULL;
    int err = astCreateLambda(S_TYPE_UNKNOWN, NULL, body, &lambda);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(body);
        return err;
    }

    err = appendParams(lambda, params);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(lambda);
        return err;
    }
    *out = lambda;
    return ERROR_SUCCESS;
}

// Move every parameter from the list to the node
static int appendParams(astNode* node, ParamList* params)
{
    // Given parameter is removed from the list, the node owns it now
    for (size_t i = 0; i < params->count; i++)
    {
        int err = astBlockAppend(node, params->items[i]);
        if (err != ERROR_SUCCESS) return err;
        params->items[i] = NULL;
    }
    return ERROR_SUCCESS;
}

// ============================================================
// Helpers for building the tree
// ============================================================

// Add a parameter, double the size if needed, free it if no memory
static int paramListPush(ParamList* list, astNode* param)
{
    if (list->count == list->capacity)
    {
        size_t newCap = (list->capacity == 0) ? PARAM_LIST_INIT_CAPACITY
                                              : list->capacity * 2;
        astNode** grown = realloc(list->items, newCap * sizeof(astNode*));
        if (grown == NULL)
        {
            fprintf(stderr, "Internal err: out of memory\n");
            astDestroy(param);
            return ERROR_INTERNAL;
        }
        list->items = grown;
        list->capacity = newCap;
    }
    list->items[list->count] = param;
    list->count++;
    return ERROR_SUCCESS;
}

// Free parameters that are still in the list and the list itself
static void paramListFree(ParamList* list)
{
    for (size_t i = 0; i < list->count; i++) astDestroy(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

// Put one node into a new block, the node is freed if it fails
static int wrapInBlock(astNode* node, astNode** out)
{
    astNode* block = NULL;
    int err = astCreateBlock(&block);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(node);
        return err;
    }

    // appendChild frees the node itself when it fails
    err = appendChild(block, node);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(block);
        return err;
    }
    *out = block;
    return ERROR_SUCCESS;
}

// Join a new part to the result so far with an operator
// The part is freed on failure, the result stays with the caller
int joinBinary(astNode** result, astOpType op, astNode* part)
{
    // First part becomes the whole result
    if (*result == NULL)
    {
        *result = part;
        return ERROR_SUCCESS;
    }
    astNode* joined = NULL;
    int err = astCreateBinary(op, *result, part, &joined);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(part);
        return err;
    }
    *result = joined;
    return ERROR_SUCCESS;
}

// ============================================================
// Statements
// ============================================================

// Block of statements with indent: body of a function, if, else or while
static int parseBlock(parcerContext* ctx, astNode** out)
{
    CHECK_ERR(match(ctx, TYPE_INDENT));

    // Empty block node, statements are added one by one
    astNode* block = NULL;
    CHECK_ERR(astCreateBlock(&block));

    // On error the block is freed together with statements already in it
    int err = parseBlockStatements(ctx, block);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(block);
        return err;
    }
    *out = block;
    return ERROR_SUCCESS;
}

// Statements of a block until its end, each becomes a child of the block
static int parseBlockStatements(parcerContext* ctx, astNode* block)
{
    // Block cannot be empty, so read at least one statement
    do
    {
        astNode* stmt = NULL;
        CHECK_ERR(parseStatement(ctx, &stmt));
        CHECK_ERR(appendChild(block, stmt));

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
static int parseStatement(parcerContext* ctx, astNode** out)
{
    switch (ctx->currentToken.type)
    {
        case TYPE_VAR:
            return parseVariable(ctx, out);
        case TYPE_VAL:
            return parseValue(ctx, out);
        // Name can start assignment or expression
        case TYPE_IDENTIFIER:
            CHECK_ERR(peek(ctx));
            if (ctx->peekToken.type == TYPE_ASSIGNMENT)
            {
                return parseAssignment(ctx, out);
            }
            // Function call or just a name is an expression statement
            return parseExpression(ctx, out);
        case TYPE_RETURN:
            return parseReturn(ctx, out);
        case TYPE_IF:
            return parseIf(ctx, out);
        case TYPE_WHILE:
            return parseWhile(ctx, out);
        case TYPE_FOR:
            return parseFor(ctx, out);
        // Line with a value is an expression statement
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
        case TYPE_INTERP_START:
        case TYPE_TRUE:
        case TYPE_FALSE:
        case TYPE_LEFT_PAREN:
        case TYPE_LIST:
            return parseExpression(ctx, out);
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
}

// Constant: val, name, optional type, =, value
static int parseValue(parcerContext* ctx, astNode** out)
{
    // Skip val and take the name, it must be freed on every return below
    CHECK_ERR(advance(ctx));
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    // Without a written type the semantics finds it out
    dataType type = S_TYPE_UNKNOWN;
    typeDesc* desc = NULL;
    astNode* expr = NULL;
    int err = parseTypeAndValue(ctx, &type, &desc, &expr);

    // Constant node owns the value and the type, on failure they are freed here
    if (err == ERROR_SUCCESS)
        err = astCreateVarDef(name, type, true, expr, out);
    if (err == ERROR_SUCCESS)
    {
        astSetTypeDesc(*out, desc);
        desc = NULL;
    }
    else
    {
        astDestroy(expr);
    }
    typeDescDestroy(desc);
    free(name);
    return err;
}

// Variable: var, name, optional type, =, value
static int parseVariable(parcerContext* ctx, astNode** out)
{
    // Skip var and take the name, it must be freed on every return below
    CHECK_ERR(advance(ctx));
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    // Without a written type the semantics finds it out
    dataType type = S_TYPE_UNKNOWN;
    typeDesc* desc = NULL;
    astNode* expr = NULL;
    int err = parseTypeAndValue(ctx, &type, &desc, &expr);

    // Variable node owns the value and the type, on failure they are freed here
    if (err == ERROR_SUCCESS)
        err = astCreateVarDef(name, type, false, expr, out);
    if (err == ERROR_SUCCESS)
    {
        astSetTypeDesc(*out, desc);
        desc = NULL;
    }
    else
    {
        astDestroy(expr);
    }
    typeDescDestroy(desc);
    free(name);
    return err;
}

// Optional type after colon, then = and the value
static int parseTypeAndValue(parcerContext* ctx, dataType* type,
                             typeDesc** desc, astNode** expr)
{
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(parseType(ctx, type, desc));
    }
    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));
    return parseExpressionOrIf(ctx, expr);
}

// Assignment: name, =, value
static int parseAssignment(parcerContext* ctx, astNode** out)
{
    // Take the name, it must be freed on every return below
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    astNode* expr = NULL;
    int err = match(ctx, TYPE_ASSIGNMENT);
    if (err == ERROR_SUCCESS) err = parseExpressionOrIf(ctx, &expr);

    // Assignment node owns the value, on failure the value is freed here
    if (err == ERROR_SUCCESS)
    {
        err = astCreateSetter(name, expr, out);
        if (err != ERROR_SUCCESS) astDestroy(expr);
    }
    free(name);
    return err;
}

// Return: the word return and maybe a value
static int parseReturn(parcerContext* ctx, astNode** out)
{
    CHECK_ERR(advance(ctx));

    // Value is read only if the next token can start an expression
    astNode* expr = NULL;
    if (canStartExpression(ctx->currentToken.type))
    {
        CHECK_ERR(parseExpressionOrIf(ctx, &expr));
    }

    // Empty return has no child
    int err = astCreateReturn(expr, out);
    if (err != ERROR_SUCCESS) astDestroy(expr);
    return err;
}

// if, condition, then, body, and maybe else or else if
static int parseIf(parcerContext* ctx, astNode** out)
{
    CHECK_ERR(advance(ctx));

    // Parts are collected first, the if node is made when all are ready
    astNode* cond = NULL;
    astNode* thenBlock = NULL;
    astNode* elseBlock = NULL;
    int err = parseIfParts(ctx, &cond, &thenBlock, &elseBlock);
    if (err == ERROR_SUCCESS)
    {
        err = astCreateIf(cond, thenBlock, elseBlock, out);
    }

    // On any failure the parts belong to nobody, so free them here
    if (err != ERROR_SUCCESS)
    {
        astDestroy(cond);
        astDestroy(thenBlock);
        astDestroy(elseBlock);
    }
    return err;
}

// Condition, then branch and optional else branch of an if
static int parseIfParts(parcerContext* ctx, astNode** cond, astNode** thenBlock,
                        astNode** elseBlock)
{
    // Condition goes until then
    CHECK_ERR(parseExpression(ctx, cond));
    CHECK_ERR(match(ctx, TYPE_THEN));
    CHECK_ERR(parseBranch(ctx, thenBlock));

    // After a one-line branch else may stand on the next line
    CHECK_ERR(skipNewLineBeforeElse(ctx));
    // No else means the if is finished
    if (ctx->currentToken.type != TYPE_ELSE)
    {
        return ERROR_SUCCESS;
    }
    CHECK_ERR(advance(ctx));

    // else if is just another if after else
    if (ctx->currentToken.type == TYPE_IF)
    {
        return parseElseIf(ctx, elseBlock);
    }
    return parseBranch(ctx, elseBlock);
}

// else if becomes a block with one inner if inside
static int parseElseIf(parcerContext* ctx, astNode** out)
{
    astNode* inner = NULL;
    CHECK_ERR(parseIf(ctx, &inner));
    return wrapInBlock(inner, out);
}

// while, condition, do, body
static int parseWhile(parcerContext* ctx, astNode** out)
{
    // Condition goes until do
    CHECK_ERR(advance(ctx));
    astNode* cond = NULL;
    CHECK_ERR(parseExpression(ctx, &cond));

    astNode* body = NULL;
    int err = match(ctx, TYPE_DO);
    if (err == ERROR_SUCCESS) err = parseBranch(ctx, &body);
    if (err == ERROR_SUCCESS) err = astCreateWhile(cond, body, out);

    // On any failure condition and body belong to nobody
    if (err != ERROR_SUCCESS)
    {
        astDestroy(cond);
        astDestroy(body);
    }
    return err;
}

// for, header, do, body
static int parseFor(parcerContext* ctx, astNode** out)
{
    CHECK_ERR(advance(ctx));

    // Every part is freed at the end unless the loop node took it
    ForParts parts = {.name = NULL};
    int err = parseForHeader(ctx, &parts);
    if (err == ERROR_SUCCESS) err = match(ctx, TYPE_DO);
    if (err == ERROR_SUCCESS) err = parseBranch(ctx, &parts.body);
    if (err == ERROR_SUCCESS) err = buildFor(&parts, out);
    forPartsFree(&parts);
    return err;
}

// Loop variable, arrow, source and guards, on one line or as a block
static int parseForHeader(parcerContext* ctx, ForParts* parts)
{
    // New line right after for opens an indented header
    bool isMultiLine = ctx->currentToken.type == TYPE_EOL;
    if (isMultiLine)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(match(ctx, TYPE_INDENT));
    }

    CHECK_ERR(takeName(ctx, &parts->name));
    CHECK_ERR(match(ctx, TYPE_LARROW));
    CHECK_ERR(parseForSource(ctx, parts));
    CHECK_ERR(parseForGuards(ctx, isMultiLine, &parts->guard));

    // Indented header ends before do, which is aligned with for
    if (isMultiLine)
    {
        CHECK_ERR(match(ctx, TYPE_EOL));
        CHECK_ERR(match(ctx, TYPE_DEDENT));
    }
    return ERROR_SUCCESS;
}

// Range with to or until and optional by step, anything else is a list
static int parseForSource(parcerContext* ctx, ForParts* parts)
{
    CHECK_ERR(parseExpression(ctx, &parts->from));

    // Without to or until the source is a list
    if (!isRangeStart(ctx)) return ERROR_SUCCESS;
    parts->isRange = true;
    parts->isInclusive = isWord(ctx, "to");
    CHECK_ERR(advance(ctx));
    CHECK_ERR(parseExpression(ctx, &parts->end));

    // Step is written after by
    if (isWord(ctx, "by"))
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(parseExpression(ctx, &parts->step));
    }
    return ERROR_SUCCESS;
}

// Any number of guards, each is if and a condition
static int parseForGuards(parcerContext* ctx, bool isMultiLine, astNode** guard)
{
    while (true)
    {
        // In an indented header a guard may start on its own line
        if (isMultiLine && ctx->currentToken.type == TYPE_EOL)
        {
            CHECK_ERR(peek(ctx));
            if (ctx->peekToken.type != TYPE_IF) return ERROR_SUCCESS;
            CHECK_ERR(advance(ctx));
        }
        if (ctx->currentToken.type != TYPE_IF) return ERROR_SUCCESS;
        CHECK_ERR(advance(ctx));

        astNode* cond = NULL;
        CHECK_ERR(parseExpression(ctx, &cond));
        CHECK_ERR(joinBinary(guard, AST_OP_AND, cond));
    }
}

// Make the loop node, guards go into the body as one if
static int buildFor(ForParts* parts, astNode** out)
{
    // Body runs only when all guards are true
    if (parts->guard != NULL)
    {
        astNode* guarded = NULL;
        CHECK_ERR(astCreateIf(parts->guard, parts->body, NULL, &guarded));
        parts->guard = NULL;
        parts->body = NULL;
        CHECK_ERR(wrapInBlock(guarded, &parts->body));
    }

    // Loop over a list keeps only the list and the body
    if (!parts->isRange)
    {
        CHECK_ERR(astCreateForEach(parts->name, parts->from, parts->body, out));
    }
    else
    {
        CHECK_ERR(astCreateFor(parts->name, parts->from, parts->end,
                               parts->step, parts->isInclusive, parts->body, 0,
                               out));
    }

    // Loop node owns these parts now
    parts->from = NULL;
    parts->end = NULL;
    parts->step = NULL;
    parts->body = NULL;
    return ERROR_SUCCESS;
}

// Free every part the loop node did not take
static void forPartsFree(ForParts* parts)
{
    free(parts->name);
    astDestroy(parts->from);
    astDestroy(parts->end);
    astDestroy(parts->step);
    astDestroy(parts->guard);
    astDestroy(parts->body);
}

// Body of if, else or while: indented block or one statement on the same line
static int parseBranch(parcerContext* ctx, astNode** out)
{
    // New line opens an indented block
    if (ctx->currentToken.type == TYPE_EOL)
    {
        CHECK_ERR(advance(ctx));
        return parseBlock(ctx, out);
    }
    return parseOneLineBranch(ctx, out);
}

// One statement on the same line becomes a block with only that statement
static int parseOneLineBranch(parcerContext* ctx, astNode** out)
{
    // Definition alone in a branch is not allowed
    if (ctx->currentToken.type == TYPE_VAL ||
        ctx->currentToken.type == TYPE_VAR)
    {
        fprintf(stderr,
                "Syntax err in line %d: definition cannot be a one-line body\n",
                ctx->currentToken.line);
        return ERROR_SYNTAX;
    }

    astNode* stmt = NULL;
    CHECK_ERR(parseStatement(ctx, &stmt));
    return wrapInBlock(stmt, out);
}

// Skip the new line only if else follows it on the next line
static int skipNewLineBeforeElse(parcerContext* ctx)
{
    if (ctx->currentToken.type != TYPE_EOL) return ERROR_SUCCESS;
    CHECK_ERR(peek(ctx));
    if (ctx->peekToken.type == TYPE_ELSE) return advance(ctx);
    return ERROR_SUCCESS;
}

// Value that may also be an if with else
int parseExpressionOrIf(parcerContext* ctx, astNode** out)
{
    if (ctx->currentToken.type == TYPE_IF) return parseIfExpression(ctx, out);
    return parseExpression(ctx, out);
}

// If used as a value, its result is the value of the chosen branch
int parseIfExpression(parcerContext* ctx, astNode** out)
{
    CHECK_ERR(advance(ctx));

    // Parts are collected first, the node is made when all are ready
    astNode* cond = NULL;
    astNode* thenBlock = NULL;
    astNode* elseBlock = NULL;
    int err = parseIfExpressionParts(ctx, &cond, &thenBlock, &elseBlock);
    if (err == ERROR_SUCCESS)
        err = astCreateIfExpr(cond, thenBlock, elseBlock, out);

    // On any failure the parts belong to nobody, so free them here
    if (err != ERROR_SUCCESS)
    {
        astDestroy(cond);
        astDestroy(thenBlock);
        astDestroy(elseBlock);
    }
    return err;
}

// Condition, then branch and the required else branch of an if value
static int parseIfExpressionParts(parcerContext* ctx, astNode** cond,
                                  astNode** thenBlock, astNode** elseBlock)
{
    CHECK_ERR(parseExpression(ctx, cond));
    CHECK_ERR(match(ctx, TYPE_THEN));
    CHECK_ERR(parseBlockOrValue(ctx, thenBlock));

    // Else is required, it may stand on the next line
    CHECK_ERR(skipNewLineBeforeElse(ctx));
    if (ctx->currentToken.type != TYPE_ELSE)
    {
        fprintf(stderr,
                "Syntax err in line %d: if used as a value needs else\n",
                ctx->currentToken.line);
        return ERROR_SYNTAX;
    }
    CHECK_ERR(advance(ctx));

    // Else if is another if value inside a block
    if (ctx->currentToken.type == TYPE_IF)
    {
        astNode* inner = NULL;
        CHECK_ERR(parseIfExpression(ctx, &inner));
        return wrapInBlock(inner, elseBlock);
    }
    return parseBlockOrValue(ctx, elseBlock);
}

// Indented block after a new line, or one value on the same line as a block
// Reads branches of an if value, lambda bodies and function bodies
static int parseBlockOrValue(parcerContext* ctx, astNode** out)
{
    // New line opens an indented block
    if (ctx->currentToken.type == TYPE_EOL)
    {
        CHECK_ERR(advance(ctx));
        return parseBlock(ctx, out);
    }

    // One value is wrapped, because semantics wants a block
    astNode* value = NULL;
    CHECK_ERR(parseExpressionOrIf(ctx, &value));
    return wrapInBlock(value, out);
}

// End of statement: new line, ; or end of block or file
static int parseStatementEnd(parcerContext* ctx)
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
        CHECK_ERR(parseExpressionOrIf(ctx, &arg));
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
static int checkTrailingComma(parcerContext* ctx, int commaLine)
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

// Simple data type: Int, Double, String, Unit or Boolean
static int matchDataType(parcerContext* ctx, dataType* type)
{
    // Type word of the language to the type of the tree
    switch (ctx->currentToken.type)
    {
        case TYPE_INT:
            *type = S_TYPE_INT;
            break;
        case TYPE_DOUBLE:
            *type = S_TYPE_DOUBLE;
            break;
        case TYPE_STRING:
            *type = S_TYPE_STRING;
            break;
        case TYPE_UNIT:
            *type = S_TYPE_UNIT;
            break;
        case TYPE_BOOLEAN:
            *type = S_TYPE_BOOL;
            break;
        default:
            fprintf(stderr, "Syntax error in line %d: expected data type\n",
                    ctx->currentToken.line);
            return ERROR_SYNTAX;
    }
    return advance(ctx);
}

// ============================================================
// Types
// ============================================================

// Type after a colon, only a list or a function type gets a description
static int parseType(parcerContext* ctx, dataType* kind, typeDesc** desc)
{
    // Simple type is fully told by its kind, no description is made
    if (ctx->currentToken.type != TYPE_LIST &&
        ctx->currentToken.type != TYPE_LEFT_PAREN)
    {
        return matchDataType(ctx, kind);
    }
    return parseTypeDesc(ctx, kind, desc);
}

// Any type with a full description, desc is written only on success
static int parseTypeDesc(parcerContext* ctx, dataType* kind, typeDesc** desc)
{
    if (ctx->currentToken.type == TYPE_LIST)
        return parseListType(ctx, kind, desc);
    if (ctx->currentToken.type == TYPE_LEFT_PAREN)
        return parseFuncType(ctx, kind, desc);

    // Simple type word
    CHECK_ERR(matchDataType(ctx, kind));
    *desc = typeCreatePrimitive(*kind);
    return (*desc == NULL) ? ERROR_INTERNAL : ERROR_SUCCESS;
}

// List type: List, [, type of items, ]
static int parseListType(parcerContext* ctx, dataType* kind, typeDesc** desc)
{
    CHECK_ERR(advance(ctx));
    CHECK_ERR(match(ctx, TYPE_LEFT_BRACKET));

    // Item type may be a list or a function too
    dataType itemKind = S_TYPE_UNKNOWN;
    typeDesc* item = NULL;
    CHECK_ERR(parseTypeDesc(ctx, &itemKind, &item));
    int err = match(ctx, TYPE_RIGHT_BRACKET);
    if (err != ERROR_SUCCESS)
    {
        typeDescDestroy(item);
        return err;
    }

    // List description takes the item, on failure it frees it itself
    *desc = typeCreateList(item);
    if (*desc == NULL) return ERROR_INTERNAL;
    *kind = S_TYPE_LIST;
    return ERROR_SUCCESS;
}

// Function type: (, parameter types, ), =>, return type
static int parseFuncType(parcerContext* ctx, dataType* kind, typeDesc** desc)
{
    CHECK_ERR(advance(ctx));

    // Parameter types until the closing parenthesis
    typeDesc** params = NULL;
    int count = 0;
    int err = parseTypeList(ctx, &params, &count);
    if (err == ERROR_SUCCESS) err = match(ctx, TYPE_RIGHT_PAREN);
    if (err == ERROR_SUCCESS) err = match(ctx, TYPE_ARROW);

    // Return type may be a function again
    dataType retKind = S_TYPE_UNKNOWN;
    typeDesc* ret = NULL;
    if (err == ERROR_SUCCESS) err = parseTypeDesc(ctx, &retKind, &ret);
    if (err != ERROR_SUCCESS)
    {
        typeListFree(params, count);
        return err;
    }

    // Function description takes all parts, on failure it frees them itself
    *desc = typeCreateFunc(count, params, ret);
    if (*desc == NULL) return ERROR_INTERNAL;
    *kind = S_TYPE_FUNC;
    return ERROR_SUCCESS;
}

// Types separated by commas, empty list if ) comes at once
static int parseTypeList(parcerContext* ctx, typeDesc*** list, int* count)
{
    if (ctx->currentToken.type == TYPE_RIGHT_PAREN) return ERROR_SUCCESS;

    while (1)
    {
        // Parse one type and add it to the end of the array
        dataType kind = S_TYPE_UNKNOWN;
        typeDesc* item = NULL;
        CHECK_ERR(parseTypeDesc(ctx, &kind, &item));
        typeDesc** grown = realloc(*list, sizeof(typeDesc*) * (*count + 1));
        if (grown == NULL)
        {
            typeDescDestroy(item);
            return ERROR_INTERNAL;
        }
        *list = grown;
        (*list)[(*count)++] = item;

        // Comma brings one more type
        if (ctx->currentToken.type != TYPE_COMMA) return ERROR_SUCCESS;
        CHECK_ERR(advance(ctx));
    }
}

// Free every type in the array and the array
static void typeListFree(typeDesc** list, int count)
{
    for (int i = 0; i < count; i++) typeDescDestroy(list[i]);
    free(list);
}

// Take the name string from the token and move on, the caller frees it
static int takeName(parcerContext* ctx, char** name)
{
    if (ctx->currentToken.type != TYPE_IDENTIFIER)
    {
        fprintf(stderr, "Syntax err in line  %d: unpredicted token\n",
                ctx->currentToken.line);
        return ERROR_SYNTAX;
    }

    // Token gives the string away, so advance does not free it
    *name = ctx->currentToken.value.identifier;
    ctx->currentToken.value.identifier = NULL;

    // On failure nobody will use the name, so free it here
    int err = advance(ctx);
    if (err != ERROR_SUCCESS)
    {
        free(*name);
        *name = NULL;
    }
    return err;
}

// Check that the token has the expected type and move on
int match(parcerContext* ctx, Type expectedType)
{
    if (ctx->currentToken.type == expectedType) return advance(ctx);

    // Any other token is a syntax error
    fprintf(stderr, "Syntax err in line  %d: unpredicted token\n",
            ctx->currentToken.line);
    return ERROR_SYNTAX;
}

// True if the current token is a name with exactly this text
bool isWord(const parcerContext* ctx, const char* word)
{
    return ctx->currentToken.type == TYPE_IDENTIFIER &&
           strcmp(ctx->currentToken.value.identifier, word) == 0;
}

// Read one token from the scanner, a lexical error also gets a message
static int readToken(parcerContext* ctx, Token* token)
{
    int err = getToken(token, ctx->globalScanner);
    // Return scanner code as is: 1 lexical, 99 memory
    if (err == ERROR_LEXICAL)
        fprintf(stderr, "Lexical err (line %d)\n", token->line);
    return err;
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
    return readToken(ctx, &ctx->currentToken);
}

// Look at the next token without moving, result is in ctx->peekToken
int peek(parcerContext* ctx)
{
    // Already read, nothing to do
    if (ctx->hasPeek) return ERROR_SUCCESS;

    ctx->peekToken.type = TYPE_ERROR;
    CHECK_ERR(readToken(ctx, &ctx->peekToken));
    ctx->hasPeek = true;
    return ERROR_SUCCESS;
}

// Free the string that the token owns
static void freeTokenValue(Token* token)
{
    // Names, strings and text parts of interpolated strings own memory
    if (token->type == TYPE_IDENTIFIER || token->type == TYPE_STRING_LITERAL ||
        token->type == TYPE_STR_PART)
    {
        free(token->value.identifier);
        token->value.identifier = NULL;
    }
}
