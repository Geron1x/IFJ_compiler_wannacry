// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#include "expression.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PREC_STACK_INIT_CAPACITY 16

// Groups of tokens used in the precedence table
typedef enum
{
    PS_UNARY,    // unary + - !
    PS_MUL_DIV,  // * /
    PS_ADD_SUB,  // + -
    PS_REL,      // < <= > >=
    PS_EQ,       // == !=
    PS_AND,      // &&
    PS_OR,       // ||
    PS_LPAREN,   // (
    PS_RPAREN,   // )
    PS_OPERAND,  // literal, name, call, ()
    PS_END,      // anything that ends the expression
    PS_COUNT
} PrecSymbol;

// Precedence table: row is the top of the stack, column is the next token
// '<' push the token, '>' fold the top into one expression, '=' pair of
// parentheses, 'x' error
// Unary operators are the strongest, && is stronger than ||
static const char precTable[PS_COUNT][PS_COUNT] = {
    //  u    */   +-   rel  eq   &&   ||   (    )    i    $
    {'<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>'},  // u
    {'<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>'},  // */
    {'<', '<', '>', '>', '>', '>', '>', '<', '>', '<', '>'},  // +-
    {'<', '<', '<', 'x', '>', '>', '>', '<', '>', '<', '>'},  // rel
    {'<', '<', '<', '<', 'x', '>', '>', '<', '>', '<', '>'},  // eq
    {'<', '<', '<', '<', '<', '>', '>', '<', '>', '<', '>'},  // &&
    {'<', '<', '<', '<', '<', '<', '>', '<', '>', '<', '>'},  // ||
    {'<', '<', '<', '<', '<', '<', '<', '<', '=', '<', 'x'},  // (
    {'x', '>', '>', '>', '>', '>', '>', 'x', '>', 'x', '>'},  // )
    {'x', '>', '>', '>', '>', '>', '>', 'x', '>', 'x', '>'},  // i
    {'<', '<', '<', '<', '<', '<', '<', '<', 'x', '<', 'x'},  // $
};

// What can lie on the stack
typedef enum
{
    ITEM_TERM,   // token
    ITEM_EXPR,   // already folded expression
    ITEM_HANDLE  // mark where folding starts
} ItemKind;

// One item of the stack
typedef struct
{
    ItemKind kind;
    PrecSymbol sym;  // token group, used only for ITEM_TERM
} StackItem;

// Stack that grows when it is full
typedef struct
{
    StackItem* items;
    size_t count;
    size_t capacity;
} PrecStack;

// Push an item, double the size if needed, 99 if no memory
static int precStackPush(PrecStack* stack, ItemKind kind, PrecSymbol sym)
{
    if (stack->count == stack->capacity)
    {
        size_t newCap = (stack->capacity == 0) ? PREC_STACK_INIT_CAPACITY
                                               : stack->capacity * 2;
        StackItem* grown = realloc(stack->items, newCap * sizeof(StackItem));
        if (grown == NULL)
        {
            fprintf(stderr, "Internal err: out of memory\n");
            return ERROR_INTERNAL;
        }
        stack->items = grown;
        stack->capacity = newCap;
    }
    stack->items[stack->count].kind = kind;
    stack->items[stack->count].sym = sym;
    stack->count++;
    return ERROR_SUCCESS;
}

// Find the highest token on the stack, the bottom one always exists
static size_t precStackTopTerm(const PrecStack* stack)
{
    size_t i = stack->count;
    while (i > 0)
    {
        i--;
        if (stack->items[i].kind == ITEM_TERM) return i;
    }
    return 0;
}

// Put the folding mark right above the highest token
static int precStackInsertHandle(PrecStack* stack)
{
    size_t pos = precStackTopTerm(stack) + 1;

    // Make room for one more item, then shift the rest up
    CHECK_ERR(precStackPush(stack, ITEM_HANDLE, PS_END));
    memmove(&stack->items[pos + 1], &stack->items[pos],
            (stack->count - 1 - pos) * sizeof(StackItem));
    stack->items[pos].kind = ITEM_HANDLE;
    return ERROR_SUCCESS;
}

// True for operators that stand between two expressions
static bool precIsBinaryOp(PrecSymbol sym)
{
    return sym == PS_MUL_DIV || sym == PS_ADD_SUB || sym == PS_REL ||
           sym == PS_EQ || sym == PS_AND || sym == PS_OR;
}

// Check that the items after the mark form a correct expression
static bool precMatchRule(const StackItem* rhs, size_t len)
{
    // One operand: literal, name, call or ()
    if (len == 1)
    {
        return rhs[0].kind == ITEM_TERM && rhs[0].sym == PS_OPERAND;
    }

    // Unary operator before an expression
    if (len == 2)
    {
        return rhs[0].kind == ITEM_TERM && rhs[0].sym == PS_UNARY &&
               rhs[1].kind == ITEM_EXPR;
    }

    // Other cases always have three items
    if (len != 3) return false;

    // Two expressions with an operator between them
    bool binary = rhs[0].kind == ITEM_EXPR && rhs[1].kind == ITEM_TERM &&
                  precIsBinaryOp(rhs[1].sym) && rhs[2].kind == ITEM_EXPR;

    // Expression in parentheses
    bool parens = rhs[0].kind == ITEM_TERM && rhs[0].sym == PS_LPAREN &&
                  rhs[1].kind == ITEM_EXPR && rhs[2].kind == ITEM_TERM &&
                  rhs[2].sym == PS_RPAREN;
    return binary || parens;
}

// Fold everything after the last mark into one expression
static int precReduce(PrecStack* stack, int line)
{
    // Find the last mark
    size_t h = stack->count;
    while (h > 0 && stack->items[h - 1].kind != ITEM_HANDLE) h--;

    if (h == 0 || !precMatchRule(&stack->items[h], stack->count - h))
    {
        fprintf(stderr, "Syntax err in line %d: invalid expression\n", line);
        return ERROR_SYNTAX;
    }

    // Here an AST node will be created later

    // Remove the mark with its items and push one expression
    stack->count = h - 1;
    return precStackPush(stack, ITEM_EXPR, PS_END);
}

// Find the group of the current token, for ( the caller must peek first
// expectOperand tells if + and - are unary here
static PrecSymbol precClassify(parcerContext* ctx, int depth,
                               bool expectOperand)
{
    switch (ctx->currentToken.type)
    {
        case TYPE_MUL:
        case TYPE_DIV:
            return PS_MUL_DIV;
        // Plus and minus are unary where an operand is expected
        case TYPE_ADD:
        case TYPE_SUB:
            return expectOperand ? PS_UNARY : PS_ADD_SUB;
        // Logical not is always unary
        case TYPE_EXCLAMATION:
            return PS_UNARY;
        // Logical and, logical or
        case TYPE_AND:
            return PS_AND;
        case TYPE_OR:
            return PS_OR;
        // Comparison
        case TYPE_LESSER:
        case TYPE_LESSER_EQUAL:
        case TYPE_BIGGER:
        case TYPE_BIGGER_EQUAL:
            return PS_REL;
        // Equal and not equal
        case TYPE_EQUAL:
        case TYPE_NOT_EQUAL:
            return PS_EQ;
        // Empty parentheses are the Unit value
        // Unit
        case TYPE_LEFT_PAREN:
            return (ctx->peekToken.type == TYPE_RIGHT_PAREN) ? PS_OPERAND
                                                             : PS_LPAREN;
        // Extra closing parenthesis belongs to the caller
        case TYPE_RIGHT_PAREN:
            return (depth > 0) ? PS_RPAREN : PS_END;
        // Names and literals, including true and false
        case TYPE_IDENTIFIER:
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
        case TYPE_TRUE:
        case TYPE_FALSE:
            return PS_OPERAND;
        // Any other token ends the expression
        default:
            return PS_END;
    }
}

// Read one operand: literal, name, call or ()
static int precConsumeOperand(parcerContext* ctx)
{
    // Empty parentheses are two tokens
    if (ctx->currentToken.type == TYPE_LEFT_PAREN)
    {
        CHECK_ERR(advance(ctx));
        return advance(ctx);
    }

    // Name followed by ( is a call, arguments are read recursively
    if (ctx->currentToken.type == TYPE_IDENTIFIER)
    {
        CHECK_ERR(advance(ctx));
        if (ctx->currentToken.type == TYPE_LEFT_PAREN)
        {
            return parseCallArgs(ctx);
        }
        return ERROR_SUCCESS;
    }

    // Literal is one token
    return advance(ctx);
}

// Push the token group to the stack and read its tokens
static int precShift(parcerContext* ctx, PrecStack* stack, PrecSymbol in)
{
    CHECK_ERR(precStackPush(stack, ITEM_TERM, in));
    if (in == PS_OPERAND) return precConsumeOperand(ctx);
    return advance(ctx);
}

// Main loop: push or fold until the expression ends, caller frees the stack
static int precParse(parcerContext* ctx, PrecStack* stack)
{
    // Number of open parentheses in this expression
    int depth = 0;
    // At the start we wait for an operand
    bool expectOperand = true;
    // Bottom of the stack
    CHECK_ERR(precStackPush(stack, ITEM_TERM, PS_END));

    while (1)
    {
        // For ( look ahead to see if it is ()
        if (ctx->currentToken.type == TYPE_LEFT_PAREN) CHECK_ERR(peek(ctx));

        PrecSymbol in = precClassify(ctx, depth, expectOperand);
        PrecSymbol top = stack->items[precStackTopTerm(stack)].sym;

        // Input is over: success only if one expression is left
        if (top == PS_END && in == PS_END)
        {
            if (stack->count == 2 && stack->items[1].kind == ITEM_EXPR)
            {
                return ERROR_SUCCESS;
            }
            fprintf(stderr, "Syntax err in line %d: expected expression\n",
                    ctx->currentToken.line);
            return ERROR_SYNTAX;
        }

        // Table says what to do next
        switch (precTable[top][in])
        {
            case '<':
                CHECK_ERR(precStackInsertHandle(stack));
                CHECK_ERR(precShift(ctx, stack, in));
                break;
            case '=':
                CHECK_ERR(precShift(ctx, stack, in));
                break;
            // Folding reads no token, so go to the next step at once
            case '>':
                CHECK_ERR(precReduce(stack, ctx->currentToken.line));
                continue;
            default:
                fprintf(stderr, "Syntax err in line %d: invalid expression\n",
                        ctx->currentToken.line);
                return ERROR_SYNTAX;
        }

        // Count parentheses after a push
        if (in == PS_LPAREN) depth++;
        if (in == PS_RPAREN) depth--;
        // After an operand or ) an operator must follow
        expectOperand = !(in == PS_OPERAND || in == PS_RPAREN);
    }
}

// Parse one expression, the stack is freed here on every exit
int parseExpression(parcerContext* ctx)
{
    PrecStack stack = {.items = NULL, .count = 0, .capacity = 0};
    int err = precParse(ctx, &stack);
    free(stack.items);
    return err;
}

// True if the token can start an expression
bool canStartExpression(Type type)
{
    switch (type)
    {
        case TYPE_IDENTIFIER:
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
        case TYPE_LEFT_PAREN:
        case TYPE_ADD:
        case TYPE_SUB:
        case TYPE_EXCLAMATION:
        case TYPE_TRUE:
        case TYPE_FALSE:
            return true;
        default:
            return false;
    }
}
