// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#include "expression.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PREC_STACK_INIT_CAPACITY 16

// Terminal classes of the precedence table
typedef enum
{
    PS_UNARY,    // unary + - (later !)
    PS_MUL_DIV,  // * /
    PS_ADD_SUB,  // + -
    PS_REL,      // < <= > >=
    PS_EQ,       // == !=
    PS_LPAREN,   // (
    PS_RPAREN,   // )
    PS_OPERAND,  // i: literal, id, call, ()
    PS_END,      // $: token that ends expression
    PS_COUNT
} PrecSymbol;

// '<' shift, '>' reduce, '=' pair ( ), 'x' error
// Unary binds tighter than * / and is right-assoc
static const char precTable[PS_COUNT][PS_COUNT] = {
    //  u    */   +-   rel  eq   (    )    i    $
    {'<', '>', '>', '>', '>', '<', '>', '<', '>'},  // u
    {'<', '>', '>', '>', '>', '<', '>', '<', '>'},  // */
    {'<', '<', '>', '>', '>', '<', '>', '<', '>'},  // +-
    {'<', '<', '<', 'x', '>', '<', '>', '<', '>'},  // rel
    {'<', '<', '<', '<', 'x', '<', '>', '<', '>'},  // eq
    {'<', '<', '<', '<', '<', '<', '=', '<', 'x'},  // (
    {'x', '>', '>', '>', '>', 'x', '>', 'x', '>'},  // )
    {'x', '>', '>', '>', '>', 'x', '>', 'x', '>'},  // i
    {'<', '<', '<', '<', '<', '<', 'x', '<', 'x'},  // $
};

// Stack item kinds
typedef enum
{
    ITEM_TERM,   // terminal
    ITEM_EXPR,   // nonterminal E
    ITEM_HANDLE  // handle start mark '<'
} ItemKind;

typedef struct
{
    ItemKind kind;
    PrecSymbol sym;  // used for ITEM_TERM
} StackItem;

// Dynamic stack
typedef struct
{
    StackItem* items;
    size_t count;
    size_t capacity;
} PrecStack;

// Push item, grow by doubling; 0 ok, 99 memory
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

// Index of topmost terminal; $ at bottom always exists
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

// Insert handle mark right above topmost terminal
static int precStackInsertHandle(PrecStack* stack)
{
    size_t pos = precStackTopTerm(stack) + 1;
    CHECK_ERR(precStackPush(stack, ITEM_HANDLE, PS_END));  // grow by one
    memmove(&stack->items[pos + 1], &stack->items[pos],
            (stack->count - 1 - pos) * sizeof(StackItem));
    stack->items[pos].kind = ITEM_HANDLE;
    return ERROR_SUCCESS;
}

// Binary operator class
static bool precIsBinaryOp(PrecSymbol sym)
{
    return sym == PS_MUL_DIV || sym == PS_ADD_SUB || sym == PS_REL ||
           sym == PS_EQ;
}

// Check handle against rules
// E -> i | u E | E op E | ( E )
static bool precMatchRule(const StackItem* rhs, size_t len)
{
    // Rule E -> i: the handle consists of one operand terminal
    // (literal, identifier, function call, or the () literal).
    if (len == 1)
    {
        return rhs[0].kind == ITEM_TERM && rhs[0].sym == PS_OPERAND;
    }

    // Rule E -> u E: unary operator before expression
    if (len == 2)
    {
        return rhs[0].kind == ITEM_TERM && rhs[0].sym == PS_UNARY &&
               rhs[1].kind == ITEM_EXPR;
    }

    // The remaining rules have exactly three elements on the right-hand side.
    if (len != 3) return false;

    // Rule E -> E op E, where op is a binary operator:
    // *, /, +, -, <, <=, >, >=, == or !=.
    bool binary = rhs[0].kind == ITEM_EXPR && rhs[1].kind == ITEM_TERM &&
                  precIsBinaryOp(rhs[1].sym) && rhs[2].kind == ITEM_EXPR;

    // Rule E -> ( E ): the expression is enclosed in parentheses.
    bool parens = rhs[0].kind == ITEM_TERM && rhs[0].sym == PS_LPAREN &&
                  rhs[1].kind == ITEM_EXPR && rhs[2].kind == ITEM_TERM &&
                  rhs[2].sym == PS_RPAREN;
    return binary || parens;
}

// Replace handle by E
static int precReduce(PrecStack* stack, int line)
{
    // Find last handle mark
    size_t h = stack->count;
    while (h > 0 && stack->items[h - 1].kind != ITEM_HANDLE) h--;

    if (h == 0 || !precMatchRule(&stack->items[h], stack->count - h))
    {
        fprintf(stderr, "Syntax err in line %d: invalid expression\n", line);
        return ERROR_SYNTAX;
    }

    // Later: create AST node here
    stack->count = h - 1;  // drop handle and its content
    return precStackPush(stack, ITEM_EXPR, PS_END);
}

// Class of current token; caller must peek when token is '('
// expectOperand: true -> '+'/'-' is unary
static PrecSymbol precClassify(parcerContext* ctx, int depth,
                               bool expectOperand)
{
    switch (ctx->currentToken.type)
    {
        case TYPE_MUL:
        case TYPE_DIV:
            return PS_MUL_DIV;
        case TYPE_ADD:
        case TYPE_SUB:
            return expectOperand ? PS_UNARY : PS_ADD_SUB;
        case TYPE_LESSER:
        case TYPE_LESSER_EQUAL:
        case TYPE_BIGGER:
        case TYPE_BIGGER_EQUAL:
            return PS_REL;
        case TYPE_EQUAL:
        case TYPE_NOT_EQUAL:
            return PS_EQ;
        case TYPE_LEFT_PAREN:
            // '()' is Unit literal
            return (ctx->peekToken.type == TYPE_RIGHT_PAREN) ? PS_OPERAND
                                                             : PS_LPAREN;
        case TYPE_RIGHT_PAREN:
            // Unmatched ')' belongs to caller
            return (depth > 0) ? PS_RPAREN : PS_END;
        case TYPE_IDENTIFIER:
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
            return PS_OPERAND;
        default:
            return PS_END;
    }
}

// Consume operand: literal, id, call or '()'
static int precConsumeOperand(parcerContext* ctx)
{
    if (ctx->currentToken.type == TYPE_LEFT_PAREN)
    {
        CHECK_ERR(advance(ctx));  // '('
        return advance(ctx);      // ')'
    }
    if (ctx->currentToken.type == TYPE_IDENTIFIER)
    {
        CHECK_ERR(advance(ctx));
        // Call: args are parsed recursively
        if (ctx->currentToken.type == TYPE_LEFT_PAREN)
        {
            return parseCallArgs(ctx);
        }
        return ERROR_SUCCESS;
    }
    return advance(ctx);  // literal
}

// Push terminal and consume its tokens
static int precShift(parcerContext* ctx, PrecStack* stack, PrecSymbol in)
{
    CHECK_ERR(precStackPush(stack, ITEM_TERM, in));
    if (in == PS_OPERAND) return precConsumeOperand(ctx);
    return advance(ctx);
}

// Main shift/reduce loop; stack is freed by caller
static int precParse(parcerContext* ctx, PrecStack* stack)
{
    int depth = 0;  // open '(' in this expression
    // Operand expected at start
    bool expectOperand = true;
    CHECK_ERR(precStackPush(stack, ITEM_TERM, PS_END));  // bottom $

    while (1)
    {
        // '(' needs lookahead for '()'
        if (ctx->currentToken.type == TYPE_LEFT_PAREN) CHECK_ERR(peek(ctx));

        PrecSymbol in = precClassify(ctx, depth, expectOperand);
        PrecSymbol top = stack->items[precStackTopTerm(stack)].sym;

        // Input ended and only $ E left
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

        switch (precTable[top][in])
        {
            case '<':
                CHECK_ERR(precStackInsertHandle(stack));
                CHECK_ERR(precShift(ctx, stack, in));
                break;
            case '=':
                CHECK_ERR(precShift(ctx, stack, in));
                break;
            case '>':
                CHECK_ERR(precReduce(stack, ctx->currentToken.line));
                continue;  // depth unchanged by reduce
            default:
                fprintf(stderr, "Syntax err in line %d: invalid expression\n",
                        ctx->currentToken.line);
                return ERROR_SYNTAX;
        }

        // Track parens after shift
        if (in == PS_LPAREN) depth++;
        if (in == PS_RPAREN) depth--;
        // After operand or ')' an operator follows
        expectOperand = !(in == PS_OPERAND || in == PS_RPAREN);
    }
}

int parseExpression(parcerContext* ctx)
{
    PrecStack stack = {.items = NULL, .count = 0, .capacity = 0};
    // Single free point for every exit
    int err = precParse(ctx, &stack);
    free(stack.items);
    return err;
}
