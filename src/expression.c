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
        case TYPE_CONCAT:
            *op = AST_OP_CONCAT;
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

// True if the current token is to or until, the start of a range
bool isRangeStart(const parcerContext* ctx)
{
    return isWord(ctx, "to") || isWord(ctx, "until");
}

// Find the group of the current token, for ( the caller must peek first
// expectOperand tells if + and - are unary here
static PrecSymbol precClassify(parcerContext* ctx, int depth,
                               bool expectOperand)
{
    // Value ended with an indented block, next line is a new statement
    if (!expectOperand && ctx->prevType == TYPE_DEDENT) return PS_END;

    switch (ctx->currentToken.type)
    {
        case TYPE_MUL:
        case TYPE_DIV:
            return PS_MUL_DIV;
        // Plus and minus are unary where an operand is expected
        case TYPE_ADD:
        case TYPE_SUB:
            return expectOperand ? PS_UNARY : PS_ADD_SUB;
        // List join has the level of plus and is never unary
        case TYPE_CONCAT:
            return PS_ADD_SUB;
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
        // Parenthesis with ) or if right after it is one operand
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
            return (!expectOperand && (isRangeStart(ctx) || isWord(ctx, "by")))
                       ? PS_END
                       : PS_OPERAND;
        // Literals, including true and false
        case TYPE_INT_LITERAL:
        case TYPE_DOUBLE_LITERAL:
        case TYPE_STRING_LITERAL:
        case TYPE_INTERP_START:
        case TYPE_TRUE:
        case TYPE_FALSE:
        // Word List starts a list value
        case TYPE_LIST:
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
        case TYPE_INT_LITERAL:
            return astCreateLiteralInt(tok->value.numberInt, out);
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
        if (err == ERROR_SUCCESS) err = joinBinary(&result, AST_OP_ADD, part);
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

// Call the result again while ( follows, like goo()(5)
// The wrapper takes the callee, so out always holds the whole tree
static int parseCallChain(parcerContext* ctx, astNode** out)
{
    while (ctx->currentToken.type == TYPE_LEFT_PAREN)
    {
        astNode* wrapper = NULL;
        CHECK_ERR(astCreateCallExpr(*out, &wrapper));
        *out = wrapper;
        CHECK_ERR(parseCallArgs(ctx, wrapper));
    }
    return ERROR_SUCCESS;
}

// Members after a value: .name or .name(args), as many as written
// Each member takes the value so far, out always holds the whole tree
static int parseMembers(parcerContext* ctx, astNode** out)
{
    while (ctx->currentToken.type == TYPE_DOT)
    {
        // Name of the member comes right after the dot
        CHECK_ERR(advance(ctx));
        if (ctx->currentToken.type != TYPE_IDENTIFIER)
        {
            fprintf(stderr, "Syntax err in line %d: expected member name\n",
                    ctx->currentToken.line);
            return ERROR_SYNTAX;
        }
        CHECK_ERR(peek(ctx));
        bool hasArgs = ctx->peekToken.type == TYPE_LEFT_PAREN;

        // Make the node before advance frees the name
        astNode* member = NULL;
        CHECK_ERR(astCreateMember(ctx->currentToken.value.identifier, *out,
                                  hasArgs, &member));
        *out = member;
        CHECK_ERR(advance(ctx));

        // Arguments go after the receiver, then maybe an index like
        // lst.take(2)(0)
        if (hasArgs)
        {
            CHECK_ERR(parseCallArgs(ctx, *out));
            CHECK_ERR(parseCallChain(ctx, out));
        }
    }
    return ERROR_SUCCESS;
}

// List value: word List and items in parentheses, read like call arguments
// Before a dot the word is the receiver of List.range
static int parseListWord(parcerContext* ctx, astNode** out)
{
    CHECK_ERR(peek(ctx));

    // Members after the word are read by parseMembers
    if (ctx->peekToken.type == TYPE_DOT)
    {
        CHECK_ERR(astCreateVarExpr("List", out));
        return advance(ctx);
    }
    if (ctx->peekToken.type != TYPE_LEFT_PAREN)
    {
        fprintf(stderr, "Syntax err in line %d: expected ( or . after List\n",
                ctx->currentToken.line);
        return ERROR_SYNTAX;
    }

    // Items become children, then maybe an index like List(1, 2)(0)
    CHECK_ERR(astCreateListLiteral(out));
    CHECK_ERR(advance(ctx));
    CHECK_ERR(parseCallArgs(ctx, *out));
    return parseCallChain(ctx, out);
}

// Read one operand and build its node: literal, name, call, list,
// interpolated string, (), or if and lambda in parentheses
static int precConsumeOperand(parcerContext* ctx, astNode** out)
{
    // Interpolated string is one operand
    if (ctx->currentToken.type == TYPE_INTERP_START)
        return parseInterpolation(ctx, out);

    // List value is one operand
    if (ctx->currentToken.type == TYPE_LIST) return parseListWord(ctx, out);

    // If value in parentheses is one operand
    if (ctx->currentToken.type == TYPE_LEFT_PAREN &&
        ctx->peekToken.type == TYPE_IF)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(parseIfExpression(ctx, out));
        return match(ctx, TYPE_RIGHT_PAREN);
    }

    // Empty parentheses: Unit value, or a lambda without parameters if =>
    // follows
    if (ctx->currentToken.type == TYPE_LEFT_PAREN)
    {
        CHECK_ERR(advance(ctx));
        CHECK_ERR(peek(ctx));
        if (ctx->peekToken.type == TYPE_ARROW) return parseLambda(ctx, out);
        CHECK_ERR(astCreateLiteralUnit(out));
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
            CHECK_ERR(parseCallArgs(ctx, *out));
            return parseCallChain(ctx, out);
        }
        CHECK_ERR(astCreateVarExpr(ctx->currentToken.value.identifier, out));
        return advance(ctx);
    }

    // Literal is one token
    CHECK_ERR(precCreateLiteral(&ctx->currentToken, out));
    return advance(ctx);
}

// Range in parentheses, like (1 to 5) or (0 until n by 2)
// The value on the stack is the start, the range node takes its place
static int precParseRange(parcerContext* ctx, astNode** node)
{
    // Word to keeps the last number, until leaves it out
    bool isInclusive = isWord(ctx, "to");
    CHECK_ERR(advance(ctx));

    // End of the range, then maybe a step after by
    astNode* end = NULL;
    astNode* step = NULL;
    int err = parseExpression(ctx, &end);
    if (err == ERROR_SUCCESS && isWord(ctx, "by"))
    {
        err = advance(ctx);
        if (err == ERROR_SUCCESS) err = parseExpression(ctx, &step);
    }

    // The range must close right away, so (1 to 5 to 6) is an error
    if (err == ERROR_SUCCESS && ctx->currentToken.type != TYPE_RIGHT_PAREN)
    {
        fprintf(stderr, "Syntax err in line %d: expected ) after range\n",
                ctx->currentToken.line);
        err = ERROR_SYNTAX;
    }

    // Start stays on the stack if something failed, it is freed there
    astNode* range = NULL;
    if (err == ERROR_SUCCESS)
        err = astCreateRange(*node, end, step, isInclusive, &range);
    if (err != ERROR_SUCCESS)
    {
        astDestroy(end);
        astDestroy(step);
        return err;
    }
    *node = range;
    return ERROR_SUCCESS;
}

// Push the token group, an operand gets its node right on the stack
// Parenthesis with name and colon after it is a lambda, in becomes operand
static int precShift(parcerContext* ctx, PrecStack* stack, PrecSymbol* in)
{
    StackItem item = {
        .kind = ITEM_TERM, .sym = *in, .tokenType = ctx->currentToken.type};
    CHECK_ERR(precStackPush(stack, item));
    if (*in == PS_OPERAND)
    {
        astNode** node = &stack->items[stack->count - 1].node;
        CHECK_ERR(precConsumeOperand(ctx, node));
        // Members right after the operand, like lst.head
        return parseMembers(ctx, node);
    }
    CHECK_ERR(advance(ctx));
    if (*in != PS_LPAREN || ctx->currentToken.type != TYPE_IDENTIFIER)
        return ERROR_SUCCESS;

    // Look past the name: colon means lambda parameters
    CHECK_ERR(peek(ctx));
    if (ctx->peekToken.type != TYPE_DOUBLE_DOT) return ERROR_SUCCESS;

    // Pushed parenthesis turns into one operand holding the lambda
    StackItem* top = &stack->items[stack->count - 1];
    top->sym = PS_OPERAND;
    *in = PS_OPERAND;
    return parseLambda(ctx, &top->node);
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

    while (true)
    {
        // For ( look ahead: ) or if after it makes one operand
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

        // Word to or until after a value in parentheses starts a range
        if (top == PS_LPAREN && in == PS_END && isRangeStart(ctx))
        {
            CHECK_ERR(
                precParseRange(ctx, &stack->items[stack->count - 1].node));
            continue;
        }

        // Table says what to do next
        switch (precTable[top][in])
        {
            case '<':
                CHECK_ERR(precStackInsertHandle(stack));
                CHECK_ERR(precShift(ctx, stack, &in));
                break;
            case '=':
                CHECK_ERR(precShift(ctx, stack, &in));
                break;
            // Folding reads no token, so go to the next step at once
            case '>':
                CHECK_ERR(precReduce(stack, ctx->currentToken.line));
                // Members after a value in parentheses, like (a ++ b).head
                if (top == PS_RPAREN && ctx->currentToken.type == TYPE_DOT)
                {
                    CHECK_ERR(parseMembers(
                        ctx, &stack->items[stack->count - 1].node));
                }
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
        case TYPE_LIST:
            return true;
        default:
            return false;
    }
}
