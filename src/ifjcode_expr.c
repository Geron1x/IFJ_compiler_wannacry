/**
 * @file ifjcode_expr.c
 * @brief Turns expressions into stack code
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#include "ifjcode_expr.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ast_internal.h"
#include "ifjcode.h"
#include "operands.h"

/** Gives the next free label number */
size_t takeLabelNumber(codegenContext* context)
{
    context->labelCounter++;

    return context->labelCounter;
}

/** Pushes literal on the stack */
static int pushLiteral(const astNode* literal)
{
    RETURN_IF_ERROR(tryPrint("PUSHS "));

    switch (literal->retType)
    {
        case S_TYPE_BOOL:
            RETURN_IF_ERROR(writeLiteralBool(literal->data.boolVal));
            break;

        case S_TYPE_INT:
            RETURN_IF_ERROR(writeLiteralInt(literal->data.intVal));
            break;

        case S_TYPE_UNIT:  // unit means nil
            RETURN_IF_ERROR(writeLiteralNil());
            break;

        case S_TYPE_DOUBLE:
            RETURN_IF_ERROR(writeLiteralFloat(literal->data.doubleVal));
            break;

        case S_TYPE_STRING:
            RETURN_IF_ERROR(writeLiteralString(literal->data.strVal));
            break;

        default:
            return ERROR_INTERNAL;
    }

    RETURN_IF_ERROR(tryPrint("\n"));

    return ERROR_SUCCESS;
}

/** Pushes variable on the stack */
static int pushVariable(const astNode* variable)
{
    // variable name from tree
    const char* name = variable->data.varData.key;

    size_t number = variable->data.varData.id;

    // number 0 means this is a function, not a variable
    bool isFunctionName = (number == 0);
    if (isFunctionName == true)
    {
        // function value is just its label as a string
        RETURN_IF_ERROR(tryPrint("PUSHS string@"));
        RETURN_IF_ERROR(writeFuncLabel(name));
        return tryPrint("\n");
    }

    // put it on the stack
    return writeVarInstruction("PUSHS", name, number);
}

/** Gives the IFJcode word for the operator, like a phrasebook */
static const char* translateOperator(astOpType operation, dataType resultType)
{
    switch (operation)
    {
        case AST_OP_ADD:  // +
            return "ADDS\n";

        case AST_OP_SUB:  // -
            return "SUBS\n";

        case AST_OP_L:  // <
            return "LTS\n";

        case AST_OP_G:  // >
            return "GTS\n";

        case AST_OP_EQ:  // ==
            return "EQS\n";

        case AST_OP_LEQ:  // <= , like not >
            return "GTS\nNOTS\n";

        case AST_OP_GEQ:  // >=, like not <
            return "LTS\nNOTS\n";

        case AST_OP_NEQ:  // !=
            return "EQS\nNOTS\n";

        case AST_OP_MUL:  // *
            return "MULS\n";

        case AST_OP_DIV:  // /
            if (resultType == S_TYPE_INT)
            {
                return "IDIVS\n";
            }
            return "DIVS\n";

        default:
            return NULL;
    }
}

/** Makes one string from two */
static int writeConcat(void)
{
    // take strings from stack, right one is on top
    RETURN_IF_ERROR(writeTempInstruction("POPS", 2));
    RETURN_IF_ERROR(writeTempInstruction("POPS", 1));

    // result goes to tmp1
    RETURN_IF_ERROR(tryPrint("CONCAT "));
    RETURN_IF_ERROR(writeTempVar(1));
    RETURN_IF_ERROR(tryPrint(" "));

    // two strings
    RETURN_IF_ERROR(writeTempVar(1));
    RETURN_IF_ERROR(tryPrint(" "));
    RETURN_IF_ERROR(writeTempVar(2));
    RETURN_IF_ERROR(tryPrint("\n"));

    // put the result back on stack
    RETURN_IF_ERROR(writeTempInstruction("PUSHS", 1));

    return ERROR_SUCCESS;
}

/** Checks if both sides are true, stops early if the left one is false */
static int pushAnd(const astNode* binary, codegenContext* context)
{
    const astNode* left = binary->children[0];
    const astNode* right = binary->children[1];
    size_t labelNumber = takeLabelNumber(context);

    // check the left side
    RETURN_IF_ERROR(pushExpression(left, context));
    RETURN_IF_ERROR(writeJumpIfFalse(LABEL_AND_FALSE, labelNumber));

    // count the right side
    RETURN_IF_ERROR(pushExpression(right, context));
    RETURN_IF_ERROR(tryPrint("JUMP "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_AND_END, labelNumber));
    RETURN_IF_ERROR(tryPrint("\n"));

    // left was false, so the answer is false
    RETURN_IF_ERROR(tryPrint("LABEL "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_AND_FALSE, labelNumber));
    RETURN_IF_ERROR(tryPrint("\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@false\n"));

    // end
    RETURN_IF_ERROR(tryPrint("LABEL "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_AND_END, labelNumber));
    RETURN_IF_ERROR(tryPrint("\n"));

    return ERROR_SUCCESS;
}

/** Checks if at least one side is true, stops early if the left one is true */
static int pushOr(const astNode* binary, codegenContext* context)
{
    const astNode* left = binary->children[0];
    const astNode* right = binary->children[1];
    size_t labelNumber = takeLabelNumber(context);

    // check the left side
    RETURN_IF_ERROR(pushExpression(left, context));
    RETURN_IF_ERROR(writeJumpIfTrue(LABEL_OR_TRUE, labelNumber));

    // count the right side
    RETURN_IF_ERROR(pushExpression(right, context));
    RETURN_IF_ERROR(tryPrint("JUMP "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_OR_END, labelNumber));
    RETURN_IF_ERROR(tryPrint("\n"));

    // left was true, so the answer is true
    RETURN_IF_ERROR(tryPrint("LABEL "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_OR_TRUE, labelNumber));
    RETURN_IF_ERROR(tryPrint("\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@true\n"));

    // end
    RETURN_IF_ERROR(tryPrint("LABEL "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_OR_END, labelNumber));
    RETURN_IF_ERROR(tryPrint("\n"));

    return ERROR_SUCCESS;
}

/** Pushes left and right, and after that - does the operation */
static int pushOperation(const astNode* binary, codegenContext* context)
{
    // take left and right side of the operation
    const astNode* left = binary->children[0];
    const astNode* right = binary->children[1];

    // AND
    if (binary->data.opType == AST_OP_AND)
    {
        return pushAnd(binary, context);
    }

    // OR
    if (binary->data.opType == AST_OP_OR)
    {
        return pushOr(binary, context);
    }

    // put left and right on the stack
    RETURN_IF_ERROR(pushExpression(left, context));
    RETURN_IF_ERROR(pushExpression(right, context));

    // makes one string from two
    if (binary->data.opType == AST_OP_ADD && binary->retType == S_TYPE_STRING)
    {
        return writeConcat();
    }

    // two lists become one
    if (binary->data.opType == AST_OP_CONCAT)
    {
        return tryPrint("CALL $$list$concat\n");
    }

    // print the operation text to stdout
    return tryPrint(translateOperator(binary->data.opType, binary->retType));
}

/** Variable keeps a list or a function, we look which one and use it */
static int pushVariableCall(const astNode* call, codegenContext* context)
{
    // name and number of the variable
    const char* name = call->data.funcData.key;
    size_t number = call->data.funcData.id;
    size_t labelNumber = takeLabelNumber(context);

    // look what type the variable has
    RETURN_IF_ERROR(tryPrint("TYPE "));
    RETURN_IF_ERROR(writeTempVar(1));
    RETURN_IF_ERROR(tryPrint(" "));
    RETURN_IF_ERROR(writeVarName(name, number));
    RETURN_IF_ERROR(tryPrint("\n"));

    // array means list, jump there
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_CALL_LIST, labelNumber));
    RETURN_IF_ERROR(tryPrint(" "));
    RETURN_IF_ERROR(writeTempVar(1));
    RETURN_IF_ERROR(tryPrint(" string@array\n"));

    // function part
    RETURN_IF_ERROR(writeVarInstruction("CALL", name, number));
    RETURN_IF_ERROR(tryPrint("JUMP "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_CALL_END, labelNumber));
    RETURN_IF_ERROR(tryPrint("\n"));

    // list part, take the item on the index
    RETURN_IF_ERROR(tryPrint("LABEL "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_CALL_LIST, labelNumber));
    RETURN_IF_ERROR(tryPrint("\n"));
    RETURN_IF_ERROR(writeVarInstruction("PUSHS", name, number));
    RETURN_IF_ERROR(tryPrint("CALL "));
    RETURN_IF_ERROR(writeFuncLabel("list$get"));
    RETURN_IF_ERROR(tryPrint("\n"));

    // end
    RETURN_IF_ERROR(tryPrint("LABEL "));
    RETURN_IF_ERROR(writeJumpLabel(LABEL_CALL_END, labelNumber));

    return tryPrint("\n");
}

/** Pushes all arguments and calls the function */
static int pushCall(const astNode* call, codegenContext* context)
{
    size_t argumentCount = (size_t)call->childCount;

    // put every argument on stack, from left to right
    for (size_t i = 0; i < argumentCount; i++)
    {
        RETURN_IF_ERROR(pushExpression(call->children[i], context));
    }

    // number above 0 means a variable that keeps a function
    size_t number = call->data.funcData.id;
    bool isVariableCall = (number > 0);
    if (isVariableCall == true)
    {
        return pushVariableCall(call, context);
    }

    RETURN_IF_ERROR(tryPrint("CALL "));
    RETURN_IF_ERROR(writeFuncLabel(call->data.funcData.key));
    RETURN_IF_ERROR(tryPrint("\n"));

    return ERROR_SUCCESS;
}

/** Calls the function that comes from an expression */
static int pushCallExpr(const astNode* call, codegenContext* context)
{
    const astNode* function = call->children[0];
    bool isList = (function->retType == S_TYPE_LIST);
    bool isFunction = (function->retType == S_TYPE_FUNC);

    if (isList == false && isFunction == false)
    {
        return ERROR_INTERNAL;
    }

    // arguments go after the function
    size_t childCount = (size_t)call->childCount;
    for (size_t i = 1; i < childCount; i++)
    {
        RETURN_IF_ERROR(pushExpression(call->children[i], context));
    }

    RETURN_IF_ERROR(pushExpression(function, context));

    if (isList == true)
    {
        RETURN_IF_ERROR(tryPrint("CALL "));
        RETURN_IF_ERROR(writeFuncLabel("list$get"));
        return tryPrint("\n");
    }

    // the function gives us its label, keep it in tmp1
    RETURN_IF_ERROR(writeTempInstruction("POPS", 1));

    RETURN_IF_ERROR(tryPrint("CALL "));
    RETURN_IF_ERROR(writeTempVar(1));

    return tryPrint("\n");
}

/** Puts all items on the stack and makes a list from them */
static int pushListLiteral(const astNode* list, codegenContext* context)
{
    size_t itemCount = (size_t)list->childCount;

    // put every item on stack
    for (size_t i = 0; i < itemCount; i++)
    {
        RETURN_IF_ERROR(pushExpression(list->children[i], context));
    }

    RETURN_IF_ERROR(tryPrint("PUSHS "));
    RETURN_IF_ERROR(writeLiteralInt((long long)itemCount));
    RETURN_IF_ERROR(tryPrint("\n"));

    RETURN_IF_ERROR(tryPrint("CALL "));
    RETURN_IF_ERROR(writeFuncLabel("list$make"));
    RETURN_IF_ERROR(tryPrint("\n"));

    return ERROR_SUCCESS;
}

/** Calls the list function, its name is the same as the member */
static int writeListCall(const char* name)
{
    RETURN_IF_ERROR(tryPrint("CALL $$list$"));
    RETURN_IF_ERROR(tryPrint(name));

    return tryPrint("\n");
}

/** Puts start, end, step and the end kind on stack and makes the list */
static int pushRange(const astNode* start, const astNode* end,
                     const astNode* step, bool isInclusive,
                     codegenContext* context)
{
    // start and end of the numbers
    RETURN_IF_ERROR(pushExpression(start, context));
    RETURN_IF_ERROR(pushExpression(end, context));

    // no step means we go by one
    if (step != NULL)
    {
        RETURN_IF_ERROR(pushExpression(step, context));
    }
    else
    {
        RETURN_IF_ERROR(tryPrint("PUSHS int@1\n"));
    }

    // is the end inside or not
    RETURN_IF_ERROR(tryPrint("PUSHS "));
    RETURN_IF_ERROR(writeLiteralBool(isInclusive));
    RETURN_IF_ERROR(tryPrint("\n"));

    return writeListCall("range");
}

/** Tells if we already have a list function for this member */
static bool isListMember(const char* name)
{
    const char* members[] = {"head",   "tail",      "take",
                             "drop",   "slice",     "toString",
                             "filter", "takeWhile", "dropWhile"};

    // count how many names we have
    size_t memberCount = sizeof(members) / sizeof(members[0]);

    // look for the name in our list
    for (size_t i = 0; i < memberCount; i++)
    {
        if (strcmp(name, members[i]) == 0)
        {
            return true;
        }
    }

    return false;
}

/** Does the member of the list, like head or drop */
static int pushMember(const astNode* member, codegenContext* context)
{
    // what member and on what list
    const char* name = member->data.memberData.key;
    const astNode* receiver = member->children[0];
    size_t childCount = (size_t)member->childCount;

    // List.range makes numbers, the end is not inside
    bool isListRange = (receiver->nodeType == AST_EXPR_VAR &&
                        strcmp(receiver->data.varData.key, "List") == 0);
    if (isListRange == true)
    {
        const astNode* step = NULL;
        if (childCount > 3)
        {
            step = member->children[3];
        }
        return pushRange(member->children[1], member->children[2], step, false,
                         context);
    }

    // toList makes a list from to or until
    bool isRangeToList = (receiver->nodeType == AST_EXPR_RANGE);
    if (isRangeToList == true)
    {
        const astNode* step = NULL;
        if (receiver->data.rangeData.hasStep == true)
        {
            step = receiver->children[2];
        }
        return pushRange(receiver->children[0], receiver->children[1], step,
                         receiver->data.rangeData.isInclusive, context);
    }

    if (isListMember(name) == false)
    {
        return ERROR_INTERNAL;
    }

    // head is item on zero, zero goes before the list
    bool isHead = (strcmp(name, "head") == 0);
    if (isHead == true)
    {
        RETURN_IF_ERROR(tryPrint("PUSHS int@0\n"));
        RETURN_IF_ERROR(pushExpression(receiver, context));
        return writeListCall("get");
    }

    // put the list on stack
    RETURN_IF_ERROR(pushExpression(receiver, context));

    // take is slice from zero
    bool isTake = (strcmp(name, "take") == 0);
    if (isTake == true)
    {
        RETURN_IF_ERROR(tryPrint("PUSHS int@0\n"));
        RETURN_IF_ERROR(pushExpression(member->children[1], context));
        return writeListCall("slice");
    }

    // put all args and call the list function
    for (size_t i = 1; i < childCount; i++)
    {
        RETURN_IF_ERROR(pushExpression(member->children[i], context));
    }

    return writeListCall(name);
}

/** Handles minus or plus before a number */
static int pushUnary(const astNode* unary, codegenContext* context)
{
    // number comes first, sign comes later
    RETURN_IF_ERROR(pushExpression(unary->children[0], context));

    // plus changes nothing
    if (unary->data.opType == AST_OP_POS)
    {
        return ERROR_SUCCESS;
    }

    // not just flips the value
    if (unary->data.opType == AST_OP_NOT)
    {
        return tryPrint("NOTS\n");
    }

    // just put -1 on stack
    RETURN_IF_ERROR(tryPrint("PUSHS "));
    if (unary->retType == S_TYPE_INT)
    {
        RETURN_IF_ERROR(writeLiteralInt(-1));
    }
    else
    {
        RETURN_IF_ERROR(writeLiteralFloat(-1.0));
    }
    RETURN_IF_ERROR(tryPrint("\n"));

    RETURN_IF_ERROR(tryPrint("MULS\n"));

    return ERROR_SUCCESS;
}

/** Pushes value of the expression on the stack */
int pushExpression(const astNode* expression, codegenContext* context)
{
    // look what kind of expression we got
    switch (expression->nodeType)
    {
        case AST_EXPR_LITERAL:
            RETURN_IF_ERROR(pushLiteral(expression));
            break;

        case AST_EXPR_BINARY:
            RETURN_IF_ERROR(pushOperation(expression, context));
            break;

        case AST_EXPR_CALL:
            RETURN_IF_ERROR(pushCall(expression, context));
            break;

        case AST_EXPR_UNARY:
            RETURN_IF_ERROR(pushUnary(expression, context));
            break;

        case AST_EXPR_VAR:
            RETURN_IF_ERROR(pushVariable(expression));
            break;

        case AST_EXPR_IF:
            RETURN_IF_ERROR(generateIf(expression, context));
            break;

        case AST_EXPR_LAMBDA:
            RETURN_IF_ERROR(generateLambda(expression, context));
            break;

        case AST_EXPR_CALL_EXPR:
            RETURN_IF_ERROR(pushCallExpr(expression, context));
            break;

        case AST_EXPR_LIST_LITERAL:
            RETURN_IF_ERROR(pushListLiteral(expression, context));
            break;

        case AST_EXPR_MEMBER:
            RETURN_IF_ERROR(pushMember(expression, context));
            break;

        default:
            return ERROR_INTERNAL;
    }

    // int must be double here
    if (expression->int2float == true)
    {
        RETURN_IF_ERROR(tryPrint("INT2FLOATS\n"));
    }

    return ERROR_SUCCESS;
}
