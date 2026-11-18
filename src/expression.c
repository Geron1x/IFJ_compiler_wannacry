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
    Type tokenType;  // exact token, tells which operator it is
    astNode* node;   // tree of an operand or folded expression, else NULL
} StackItem;

// Stack that grows when it is full
typedef struct
{
    StackItem* items;
    size_t count;
    size_t capacity;
} PrecStack;

// Push an item, double the size if needed, 99 if no memory
static int precStackPush(PrecStack* stack, StackItem item)
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
    stack->items[stack->count] = item;
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
    CHECK_ERR(precStackPush(stack, (StackItem){.kind = ITEM_HANDLE}));
    memmove(&stack->items[pos + 1], &stack->items[pos],
            (stack->count - 1 - pos) * sizeof(StackItem));
    // The mark is written whole, otherwise it keeps a copy of a moved node
    stack->items[pos] = (StackItem){.kind = ITEM_HANDLE};
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

// Operator of a binary node from its token
static int precBinaryOp(Type type, astOpType* op)
{
    switch (type)
    {
        case TYPE_MUL:
            *op = AST_OP_MUL;
            break;
        case TYPE_DIV:
            *op = AST_OP_DIV;
            break;
        case TYPE_ADD:
            *op = AST_OP_ADD;
            break;
        case TYPE_SUB:
            *op = AST_OP_SUB;
            break;
        case TYPE_LESSER:
            *op = AST_OP_L;
            break;
        case TYPE_LESSER_EQUAL:
            *op = AST_OP_LEQ;
            break;
        case TYPE_BIGGER:
            *op = AST_OP_G;
            break;
        case TYPE_BIGGER_EQUAL:
            *op = AST_OP_GEQ;
            break;
        case TYPE_EQUAL:
            *op = AST_OP_EQ;
            break;
        case TYPE_NOT_EQUAL:
            *op = AST_OP_NEQ;
            break;
        case TYPE_AND:
            *op = AST_OP_AND;
            break;
        case TYPE_OR:
            *op = AST_OP_OR;
            break;
        default:
            return ERROR_INTERNAL;
    }
    return ERROR_SUCCESS;
}

// Operator of a unary node from its token
static int precUnaryOp(Type type, astOpType* op)
{
    switch (type)
    {
        case TYPE_ADD:
            *op = AST_OP_POS;
            break;
        case TYPE_SUB:
            *op = AST_OP_NEG;
            break;
        case TYPE_EXCLAMATION:
            *op = AST_OP_NOT;
            break;
        default:
            return ERROR_INTERNAL;
    }
    return ERROR_SUCCESS;
}

// Build the node for the items after the mark, the rule is already checked
// Items give their nodes away only on success
static int precBuildNode(StackItem* rhs, size_t len, astNode** out)
{
    // One operand or expression in parentheses keeps its node
    if (len == 1 || (rhs[0].kind == ITEM_TERM && rhs[0].sym == PS_LPAREN))
    {
        size_t i = (len == 1) ? 0 : 1;
        *out = rhs[i].node;
        rhs[i].node = NULL;
        return ERROR_SUCCESS;
    }

    // Unary operator before an expression
    if (len == 2)
    {
        astOpType op;
        CHECK_ERR(precUnaryOp(rhs[0].tokenType, &op));
        CHECK_ERR(astCreateUnary(op, rhs[1].node, out));
        rhs[1].node = NULL;
        return ERROR_SUCCESS;
    }

    // Two expressions with an operator between them
    astOpType op;
    CHECK_ERR(precBinaryOp(rhs[1].tokenType, &op));
    CHECK_ERR(astCreateBinary(op, rhs[0].node, rhs[2].node, out));
    rhs[0].node = NULL;
    rhs[2].node = NULL;
    return ERROR_SUCCESS;
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

    // Build the node, on failure the items still own their nodes
    astNode* node = NULL;
    CHECK_ERR(precBuildNode(&stack->items[h], stack->count - h, &node));

    // Remove the mark with its items and push one expression
    stack->count = h - 1;
    int err =
        precStackPush(stack, (StackItem){.kind = ITEM_EXPR, .node = node});
    // Node that did not get onto the stack is freed here
    if (err != ERROR_SUCCESS) astDestroy(node);
    return err;
}

// True if the name is a word of a for range: to, until or by
static bool isRangeWord(const char* name)
{
    return strcmp(name, "to") == 0 || strcmp(name, "until") == 0 ||
           strcmp(name, "by") == 0;
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
            return (ctx->peekToken.type == TYPE_RIGHT_PAREN ||
                    ctx->peekToken.type == TYPE_IF)
                       ? PS_OPERAND
                       : PS_LPAREN;
        // Extra closing parenthesis belongs to the caller
        case TYPE_RIGHT_PAREN:
            return (depth > 0) ? PS_RPAREN : PS_END;
        // Words to, until and by after a value end it, they start a range
        case TYPE_IDENTIFIER:
            return (!expectOperand &&
                    isRangeWord(ctx->currentToken.value.identifier))
                       ? PS_END
                       : PS_OPERAND;
        // Literals, including true and false
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
        case TYPE_INTERP_START:
        case TYPE_TRUE:
        case TYPE_FALSE:
            return PS_OPERAND;
        // Any other token ends the expression
        default:
            return PS_END;
    }
}

// Build a literal node from the token
static int precCreateLiteral(const Token* tok, astNode** out)
{
    switch (tok->type)
    {
        // Token keeps long long, the tree keeps int, spec needs no range check
        case TYPE_INT_LITERAL:
            return astCreateLiteralInt((int)tok->value.numberInt, out);
        case TYPE_DOUBLE_LITERAL:
            return astCreateLiteralDouble(tok->value.numberFloat, out);
        case TYPE_STRING_LITERAL:
            return astCreateLiteralString(tok->value.string, out);
        case TYPE_TRUE:
            return astCreateLiteralBool(true, out);
        case TYPE_FALSE:
            return astCreateLiteralBool(false, out);
        default:
            return ERROR_INTERNAL;
    }
}

// Wrap a value into a toStr call, the value is freed on failure
static int interpToStr(astNode* value, astNode** out)
{
    astNode* call = NULL;
    int err = astCreateCall("toStr", &call);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(value);
        return err;
    }
    // appendChild frees the value itself when it fails
    err = appendChild(call, value);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(call);
        return err;
    }
    *out = call;
    return ERROR_SUCCESS;
}

// Join a new part to the string built so far with plus
// The part is freed on failure, the result stays with the caller
static int interpJoin(astNode** result, astNode* part)
{
    // First part becomes the whole result
    if (*result == NULL)
    {
        *result = part;
        return ERROR_SUCCESS;
    }
    astNode* sum = NULL;
    int err = astCreateBinary(AST_OP_ADD, *result, part, &sum);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(part);
        return err;
    }
    *result = sum;
    return ERROR_SUCCESS;
}

// Read one part of the string: text, $name or ${expression}
static int interpPart(parcerContext* ctx, astNode** part)
{
    astNode* value = NULL;
    switch (ctx->currentToken.type)
    {
        // Plain text becomes a string literal
        case TYPE_STR_PART:
            CHECK_ERR(
                astCreateLiteralString(ctx->currentToken.value.string, &value));
            break;
        // Name after dollar becomes toStr of that name
        case TYPE_IDENTIFIER:
            CHECK_ERR(
                astCreateVarExpr(ctx->currentToken.value.identifier, &value));
            CHECK_ERR(interpToStr(value, &value));
            break;
        // Expression in braces becomes toStr of that expression
        case TYPE_LEFT_BRACE:
            CHECK_ERR(advance(ctx));
            CHECK_ERR(parseExpressionOrIf(ctx, &value));
            if (ctx->currentToken.type != TYPE_RIGHT_BRACE)
            {
                astDestroy(value);
                fprintf(stderr, "Syntax err in line %d: expected }\n",
                        ctx->currentToken.line);
                return ERROR_SYNTAX;
            }
            CHECK_ERR(interpToStr(value, &value));
            break;
        default:
            fprintf(stderr,
                    "Syntax err in line %d: invalid interpolated string\n",
                    ctx->currentToken.line);
            return ERROR_SYNTAX;
    }

    // Step over the last token of the part
    int err = advance(ctx);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(value);
        return err;
    }
    *part = value;
    return ERROR_SUCCESS;
}

// Interpolated string: all parts joined with plus into one String value
static int parseInterpolation(parcerContext* ctx, astNode** out)
{
    // Skip the opening quote
    CHECK_ERR(advance(ctx));

    astNode* result = NULL;
    int err = ERROR_SUCCESS;
    // Read parts until the closing quote
    while (err == ERROR_SUCCESS && ctx->currentToken.type != TYPE_INTERP_END)
    {
        astNode* part = NULL;
        err = interpPart(ctx, &part);
        if (err == ERROR_SUCCESS) err = interpJoin(&result, part);
    }

    // Empty string without parts is just an empty literal
    if (err == ERROR_SUCCESS && result == NULL)
        err = astCreateLiteralString("", &result);
    // Skip the closing quote
    if (err == ERROR_SUCCESS) err = advance(ctx);

    if (err != ERROR_SUCCESS)
    {
        astDestroy(result);
        return err;
    }
    *out = result;
    return ERROR_SUCCESS;
}

// Read one operand and build its node: literal, name, call or ()
static int precConsumeOperand(parcerContext* ctx, astNode** out)
{
    // Interpolated string is one operand
    if (ctx->currentToken.type == TYPE_INTERP_START)
        return parseInterpolation(ctx, out);

    // If value in parentheses is one operand
    if (ctx->currentToken.type == TYPE_LEFT_PAREN &&
        ctx->peekToken.type == TYPE_IF)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(parseIfExpression(ctx, out));
        return match(ctx, TYPE_RIGHT_PAREN);
    }

    // Empty parentheses are two tokens and mean the Unit value
    if (ctx->currentToken.type == TYPE_LEFT_PAREN)
    {
        CHECK_ERR(astCreateLiteralUnit(out));
        CHECK_ERR(advance(ctx));
        return advance(ctx);
    }

    // Name with ( is a call, make the node before advance frees the name
    if (ctx->currentToken.type == TYPE_IDENTIFIER)
    {
        CHECK_ERR(peek(ctx));
        if (ctx->peekToken.type == TYPE_LEFT_PAREN)
        {
            CHECK_ERR(astCreateCall(ctx->currentToken.value.identifier, out));
            CHECK_ERR(advance(ctx));
            return parseCallArgs(ctx, *out);
        }
        CHECK_ERR(astCreateVarExpr(ctx->currentToken.value.identifier, out));
        return advance(ctx);
    }

    // Literal is one token
    CHECK_ERR(precCreateLiteral(&ctx->currentToken, out));
    return advance(ctx);
}

// Push the token group, an operand gets its node right on the stack
static int precShift(parcerContext* ctx, PrecStack* stack, PrecSymbol in)
{
    StackItem item = {
        .kind = ITEM_TERM, .sym = in, .tokenType = ctx->currentToken.type};
    CHECK_ERR(precStackPush(stack, item));
    if (in == PS_OPERAND)
    {
        return precConsumeOperand(ctx, &stack->items[stack->count - 1].node);
    }
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
    CHECK_ERR(
        precStackPush(stack, (StackItem){.kind = ITEM_TERM, .sym = PS_END}));

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

// Parse one expression and give its tree to the caller
// The stack and every node left on it are freed here on every return
int parseExpression(parcerContext* ctx, astNode** out)
{
    PrecStack stack = {.items = NULL, .count = 0, .capacity = 0};
    int err = precParse(ctx, &stack);

    // On success the only expression lies right above the bottom
    astNode* result = NULL;
    if (err == ERROR_SUCCESS)
    {
        result = stack.items[1].node;
        stack.items[1].node = NULL;
    }

    // Free nodes that stayed on the stack after an error
    for (size_t i = 0; i < stack.count; i++) astDestroy(stack.items[i].node);
    free(stack.items);

    // Caller without a place for the tree does not need it
    if (out == NULL)
        astDestroy(result);
    else
        *out = result;
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
        case TYPE_INTERP_START:
        case TYPE_LEFT_PAREN:
        case TYPE_ADD:
        case TYPE_SUB:
        case TYPE_EXCLAMATION:
        case TYPE_TRUE:
        case TYPE_FALSE:
        case TYPE_IF:
            return true;
        default:
            return false;
    }
}
