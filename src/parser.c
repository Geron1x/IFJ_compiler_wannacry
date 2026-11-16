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

// Work with tokens
void freeTokenValue(Token* token);
int matchDataType(parcerContext* ctx, dataType* type);
int takeName(parcerContext* ctx, char** name);
int checkTrailingComma(parcerContext* ctx, int commaLine);

// Program, prologue and functions
int parseProgram(parcerContext* ctx, astNode* prog);
int parsePrologue(parcerContext* ctx);
int addFunction(parcerContext* ctx, astNode* prog, bool isMain);
int parseFunction(parcerContext* ctx, bool isMain, astNode** out);
int parseFunctionParts(parcerContext* ctx, ParamList* params, dataType* retType,
                       astNode** body);
int parseParams(parcerContext* ctx, ParamList* params);
int parseParam(parcerContext* ctx, ParamList* params);
int parseFunctionBody(parcerContext* ctx, astNode** out);
int buildFunction(const char* name, dataType retType, bool isMain,
                  astNode* body, ParamList* params, astNode** out);
int paramListPush(ParamList* list, astNode* param);
void paramListFree(ParamList* list);
int wrapInBlock(astNode* node, astNode** out);

// Statements inside function body
int parseBlock(parcerContext* ctx, astNode** out);
int parseBlockStatements(parcerContext* ctx, astNode* block);
int parseStatement(parcerContext* ctx, astNode** out);
int parseValue(parcerContext* ctx, astNode** out);
int parseVariable(parcerContext* ctx, astNode** out);
int parseTypeAndValue(parcerContext* ctx, dataType* type, astNode** expr);
int parseAssignment(parcerContext* ctx, astNode** out);
int parseReturn(parcerContext* ctx, astNode** out);
int parseIf(parcerContext* ctx, astNode** out);
int parseIfParts(parcerContext* ctx, astNode** cond, astNode** thenBlock,
                 astNode** elseBlock);
int parseElseIf(parcerContext* ctx, astNode** out);
int parseWhile(parcerContext* ctx, astNode** out);
int parseBranch(parcerContext* ctx, astNode** out);
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

// Program is prologue and then functions, each function is added to prog
int parseProgram(parcerContext* ctx, astNode* prog)
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

// Parse one function and add it to the program
int addFunction(parcerContext* ctx, astNode* prog, bool isMain)
{
    astNode* func = NULL;
    CHECK_ERR(parseFunction(ctx, isMain, &func));
    return appendChild(prog, func);
}

// Function: def, name, parameters, return type, =, body
int parseFunction(parcerContext* ctx, bool isMain, astNode** out)
{
    // Take the name, it must be freed on every return below
    CHECK_ERR(match(ctx, TYPE_DEF));
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    // Without a written return type the semantics finds it out
    ParamList params = {.items = NULL, .count = 0, .capacity = 0};
    dataType retType = S_TYPE_UNKNOWN;
    astNode* body = NULL;
    int err = parseFunctionParts(ctx, &params, &retType, &body);

    // Function node takes the body and the parameters
    if (err == ERROR_SUCCESS)
    {
        err = buildFunction(name, retType, isMain, body, &params, out);
    }

    // Parameters not given to the function are freed here
    paramListFree(&params);
    free(name);
    return err;
}

// Parameters in parentheses, optional return type, = and the body
int parseFunctionParts(parcerContext* ctx, ParamList* params, dataType* retType,
                       astNode** body)
{
    CHECK_ERR(match(ctx, TYPE_LEFT_PAREN));
    CHECK_ERR(parseParams(ctx, params));
    CHECK_ERR(match(ctx, TYPE_RIGHT_PAREN));

    // Return type is optional
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(matchDataType(ctx, retType));
    }

    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));
    return parseFunctionBody(ctx, body);
}

// Parameters separated by commas, comma before ) only with new line
int parseParams(parcerContext* ctx, ParamList* params)
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
int parseParam(parcerContext* ctx, ParamList* params)
{
    // Take the name, it must be freed on every return below
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    dataType type = S_TYPE_UNKNOWN;
    int err = match(ctx, TYPE_DOUBLE_DOT);
    if (err == ERROR_SUCCESS) err = matchDataType(ctx, &type);

    // Parameter is a constant without a value
    astNode* param = NULL;
    if (err == ERROR_SUCCESS)
    {
        err = astCreateVarDef(name, type, true, NULL, &param);
    }
    if (err == ERROR_SUCCESS) err = paramListPush(params, param);

    free(name);
    return err;
}

// Body after =: indented block, or one expression on the same line
int parseFunctionBody(parcerContext* ctx, astNode** out)
{
    // New line after = means a block body
    if (ctx->currentToken.type == TYPE_EOL)
    {
        CHECK_ERR(advance(ctx));
        return parseBlock(ctx, out);
    }

    // Otherwise the body is one expression on the same line
    astNode* expr = NULL;
    CHECK_ERR(parseExpression(ctx, &expr));

    // Nothing else may follow it on this line
    if (ctx->currentToken.type != TYPE_EOL &&
        ctx->currentToken.type != TYPE_EOF)
    {
        astDestroy(expr);
        fprintf(stderr,
                "Syntax error in line %d: expected end of line after function "
                "body\n",
                ctx->currentToken.line);
        return ERROR_SYNTAX;
    }

    // One line body is a block with one element
    return wrapInBlock(expr, out);
}

// Make the function node, then give it the parameters one by one
int buildFunction(const char* name, dataType retType, bool isMain,
                  astNode* body, ParamList* params, astNode** out)
{
    // On failure the body still belongs to us
    astNode* func = NULL;
    int err = astCreateFuncDef(name, retType, body, &func, isMain);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(body);
        return err;
    }

    // Given parameter is removed from the list, the function owns it now
    for (size_t i = 0; i < params->count; i++)
    {
        err = astBlockAppend(func, params->items[i]);
        if (err != ERROR_SUCCESS)
        {
            astDestroy(func);
            return err;
        }
        params->items[i] = NULL;
    }
    *out = func;
    return ERROR_SUCCESS;
}

// ============================================================
// Helpers for building the tree
// ============================================================

// Add a parameter, double the size if needed, free it if no memory
int paramListPush(ParamList* list, astNode* param)
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
void paramListFree(ParamList* list)
{
    for (size_t i = 0; i < list->count; i++) astDestroy(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

// Put one node into a new block, the node is freed if it fails
int wrapInBlock(astNode* node, astNode** out)
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

// ============================================================
// Statements
// ============================================================

// Block of statements with indent: body of a function, if, else or while
int parseBlock(parcerContext* ctx, astNode** out)
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
int parseBlockStatements(parcerContext* ctx, astNode* block)
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
int parseStatement(parcerContext* ctx, astNode** out)
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
        // Line with a value is an expression statement
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
        case TYPE_TRUE:
        case TYPE_FALSE:
        case TYPE_LEFT_PAREN:
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
int parseValue(parcerContext* ctx, astNode** out)
{
    // Skip val and take the name, it must be freed on every return below
    CHECK_ERR(advance(ctx));
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    // Without a written type the semantics finds it out
    dataType type = S_TYPE_UNKNOWN;
    astNode* expr = NULL;
    int err = parseTypeAndValue(ctx, &type, &expr);

    // Constant node owns the value, on failure the value is freed here
    if (err == ERROR_SUCCESS)
    {
        err = astCreateVarDef(name, type, true, expr, out);
        if (err != ERROR_SUCCESS) astDestroy(expr);
    }
    free(name);
    return err;
}

// Variable: var, name, optional type, =, value
int parseVariable(parcerContext* ctx, astNode** out)
{
    // Skip var and take the name, it must be freed on every return below
    CHECK_ERR(advance(ctx));
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    // Without a written type the semantics finds it out
    dataType type = S_TYPE_UNKNOWN;
    astNode* expr = NULL;
    int err = parseTypeAndValue(ctx, &type, &expr);

    // Variable node owns the value, on failure the value is freed here
    if (err == ERROR_SUCCESS)
    {
        err = astCreateVarDef(name, type, false, expr, out);
        if (err != ERROR_SUCCESS) astDestroy(expr);
    }
    free(name);
    return err;
}

// Optional type after colon, then = and the value
int parseTypeAndValue(parcerContext* ctx, dataType* type, astNode** expr)
{
    if (ctx->currentToken.type == TYPE_DOUBLE_DOT)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(matchDataType(ctx, type));
    }
    CHECK_ERR(match(ctx, TYPE_ASSIGNMENT));
    return parseExpression(ctx, expr);
}

// Assignment: name, =, value
int parseAssignment(parcerContext* ctx, astNode** out)
{
    // Take the name, it must be freed on every return below
    char* name = NULL;
    CHECK_ERR(takeName(ctx, &name));

    astNode* expr = NULL;
    int err = match(ctx, TYPE_ASSIGNMENT);
    if (err == ERROR_SUCCESS) err = parseExpression(ctx, &expr);

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
int parseReturn(parcerContext* ctx, astNode** out)
{
    CHECK_ERR(advance(ctx));

    // Value is read only if the next token can start an expression
    astNode* expr = NULL;
    if (canStartExpression(ctx->currentToken.type))
    {
        CHECK_ERR(parseExpression(ctx, &expr));
    }

    // Empty return has no child
    int err = astCreateReturn(expr, out);
    if (err != ERROR_SUCCESS) astDestroy(expr);
    return err;
}

// if, condition, then, body, and maybe else or else if
int parseIf(parcerContext* ctx, astNode** out)
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
int parseIfParts(parcerContext* ctx, astNode** cond, astNode** thenBlock,
                 astNode** elseBlock)
{
    // Condition goes until then
    CHECK_ERR(parseExpression(ctx, cond));
    CHECK_ERR(match(ctx, TYPE_THEN));
    CHECK_ERR(parseBranch(ctx, thenBlock));

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
int parseElseIf(parcerContext* ctx, astNode** out)
{
    astNode* inner = NULL;
    CHECK_ERR(parseIf(ctx, &inner));
    return wrapInBlock(inner, out);
}

// while, condition, do, body
int parseWhile(parcerContext* ctx, astNode** out)
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

// Body of if, else or while: new line, then indented block
int parseBranch(parcerContext* ctx, astNode** out)
{
    CHECK_ERR(match(ctx, TYPE_EOL));
    return parseBlock(ctx, out);
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

// Data type: Int, Double, String, Unit or Boolean, type may be NULL if not
// needed
int matchDataType(parcerContext* ctx, dataType* type)
{
    // Type word of the language to the type of the tree
    dataType found;
    switch (ctx->currentToken.type)
    {
        case TYPE_INT:
            found = S_TYPE_INT;
            break;
        case TYPE_DOUBLE:
            found = S_TYPE_DOUBLE;
            break;
        case TYPE_STRING:
            found = S_TYPE_STRING;
            break;
        case TYPE_UNIT:
            found = S_TYPE_UNIT;
            break;
        case TYPE_BOOLEAN:
            found = S_TYPE_BOOL;
            break;
        default:
            fprintf(stderr, "Syntax error in line %d: expected data type\n",
                    ctx->currentToken.line);
            return ERROR_SYNTAX;
    }
    if (type != NULL) *type = found;
    return advance(ctx);
}

// Take the name string from the token and move on, the caller frees it
int takeName(parcerContext* ctx, char** name)
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
