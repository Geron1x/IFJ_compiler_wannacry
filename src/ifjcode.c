/**
 * @file ifjcode.c
 * @brief Walks the tree and prints IFJcode26
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#include "ifjcode.h"

#include <stdbool.h>
#include <stdio.h>

#include "ast_internal.h"
#include "ifjcode_expr.h"
#include "ifjcode_lib.h"
#include "operands.h"

/** Gives the main function */
static const astNode* findMainFunction(const astNode* program)
{
    size_t functionCount = (size_t)program->childCount;

    // Check every function in the program one by one
    for (size_t i = 0; i < functionCount; i++)
    {
        // Take the current function from the list
        const astNode* function = program->children[i];

        // Skip it if this is not a function
        if (function->nodeType != AST_FUNC)
        {
            continue;
        }

        // Parser marks main with isMain so we return it
        if (function->data.funcData.isMain == true)
        {
            return function;
        }
    }

    return NULL;
}

/** Gives the block with all commands of the function */
static const astNode* getFunctionBody(const astNode* function)
{
    // body is child 0 of the function node
    return function->children[0];
}

/** Prints the first lines and calls main */
static int writeProgramStart(const astNode* mainNode)
{
    RETURN_IF_ERROR(tryPrint(".IFJcode26\n"));
    RETURN_IF_ERROR(tryPrint("CALL "));

    // Prints $$ and the real name of main
    RETURN_IF_ERROR(writeFuncLabel(mainNode->data.funcData.key));

    RETURN_IF_ERROR(tryPrint("\n"));
    RETURN_IF_ERROR(tryPrint("EXIT int@0\n"));

    return ERROR_SUCCESS;
}

/** Prints DEFVAR for temp variable */
static int defineTempVar(size_t number)
{
    return writeTempInstruction("DEFVAR", number);
}

/** Prints the way out of the function */
static int writeFunctionExit(const codegenContext* context)
{
    // look if the func - returns Unit
    bool isFunctionUnit;
    isFunctionUnit = (context->currentFunction->retType == S_TYPE_UNIT);

    // Unit func gives ()
    if (isFunctionUnit == true)
    {
        RETURN_IF_ERROR(writeTempInstruction("POPS", 1));
        RETURN_IF_ERROR(tryPrint("PUSHS nil@nil\n"));
    }

    RETURN_IF_ERROR(tryPrint("POPFRAME\n"));
    RETURN_IF_ERROR(tryPrint("RETURN\n"));

    return ERROR_SUCCESS;
}

/** Tells if the if has else */
static bool ifHasElse(const astNode* ifNode)
{
    // else is always the third child
    bool hasThirdChild = (ifNode->childCount > 2);

    return hasThirdChild;
}

/** Checks if the command puts something on the stack */
static bool givesValue(const astNode* node)
{
    // look what kind of command we got
    switch (node->nodeType)
    {
        // expression gives a value
        case AST_EXPR_BINARY:
        case AST_EXPR_UNARY:
        case AST_EXPR_CALL:
        case AST_EXPR_VAR:
        case AST_EXPR_LITERAL:
        case AST_EXPR_IF:
            return true;

        // if without else gives nothing
        case AST_STAT_IF:
            return ifHasElse(node);

        default:
            return false;
    }
}

/** Counts the value and puts it into the variable */
static int generateSetVariable(const astNode* command, codegenContext* context)
{
    // get everything we need from the node
    const astNode* value = command->children[0];
    const char* name = command->data.varData.key;
    size_t number = command->data.varData.id;

    // count and put it on the stack
    RETURN_IF_ERROR(pushExpression(value, context));

    // move value from stack to the variable
    return writeVarInstruction("POPS", name, number);
}

/** Prints command from the block */
static int generateCommand(const astNode* command, codegenContext* context)
{
    switch (command->nodeType)
    {
        // all kinds of expressions
        case AST_EXPR_BINARY:
        case AST_EXPR_UNARY:
        case AST_EXPR_CALL:
        case AST_EXPR_VAR:
        case AST_EXPR_LITERAL:
        case AST_EXPR_IF:
            // just put the value on the stack
            return pushExpression(command, context);

        // new variable or new value for a variable
        case AST_STAT_VAR:
        case AST_STAT_SET:
            return generateSetVariable(command, context);

        default:
            return ERROR_INTERNAL;
    }
}

/** Prints all commands of the block */
static int generateBlock(const astNode* block, codegenContext* context)
{
    // not a block - get out of here
    if (block->nodeType != AST_BLOCK)
    {
        return ERROR_INTERNAL;
    }

    // just counter
    size_t commandCount = (size_t)block->childCount;

    if (commandCount == 0)
    {
        return tryPrint("PUSHS nil@nil\n");
    }

    // we look for the last one by index
    size_t lastIndex = commandCount - 1;
    for (size_t i = 0; i < lastIndex; i++)
    {
        const astNode* command = block->children[i];
        RETURN_IF_ERROR(generateCommand(command, context));

        // clean stack after the command
        if (givesValue(command) == true)
        {
            RETURN_IF_ERROR(writeTempInstruction("POPS", 1));
        }
    }
    // last command is the result of the block
    const astNode* lastCommand = block->children[lastIndex];
    RETURN_IF_ERROR(generateCommand(lastCommand, context));

    if (givesValue(lastCommand) == false)
    {
        RETURN_IF_ERROR(tryPrint("PUSHS nil@nil\n"));
    }

    return ERROR_SUCCESS;
}

/** Defines all variables of the function */
static int defineVariables(const astNode* node)
{
    // if node is variable - we define it
    if (node->nodeType == AST_STAT_VAR)
    {
        const char* name = node->data.varData.key;
        size_t number = node->data.varData.id;
        RETURN_IF_ERROR(writeVarInstruction("DEFVAR", name, number));
    }

    // check every child
    size_t childCount = (size_t)node->childCount;
    for (size_t i = 0; i < childCount; i++)
    {
        RETURN_IF_ERROR(defineVariables(node->children[i]));
    }

    return ERROR_SUCCESS;
}

/** Takes the arguments from the stack into the param */
static int popParams(const astNode* function)
{
    // children without commands
    size_t paramCount = (size_t)function->childCount - 1;

    // take arg from the end
    for (size_t i = paramCount; i > 0; i--)
    {
        const astNode* param = function->children[i];
        const char* name = param->data.varData.key;
        size_t number = param->data.varData.id;

        RETURN_IF_ERROR(writeVarInstruction("POPS", name, number));
    }

    return ERROR_SUCCESS;
}

/** Prints function */
static int generateFunction(const astNode* function, codegenContext* context)
{
    // Program should have only functions here
    if (function->nodeType != AST_FUNC)
    {
        return ERROR_INTERNAL;
    }

    // which func we are in rn
    context->currentFunction = function;

    RETURN_IF_ERROR(tryPrint("\nLABEL "));
    RETURN_IF_ERROR(writeFuncLabel(function->data.funcData.key));
    RETURN_IF_ERROR(tryPrint("\n"));
    RETURN_IF_ERROR(tryPrint("CREATEFRAME\n"));
    RETURN_IF_ERROR(tryPrint("PUSHFRAME\n"));

    // temp variables
    RETURN_IF_ERROR(defineTempVar(1));
    RETURN_IF_ERROR(defineTempVar(2));

    // define all variables at the start
    RETURN_IF_ERROR(defineVariables(function));

    // params take their values from the stack
    RETURN_IF_ERROR(popParams(function));

    // print all commands of the function
    RETURN_IF_ERROR(generateBlock(getFunctionBody(function), context));

    RETURN_IF_ERROR(writeFunctionExit(context));

    return ERROR_SUCCESS;
}

/** Prints IFJcode26 */
int generateIfjcode(const astNode* program)
{
    if (program == NULL)
    {
        return ERROR_INTERNAL;
    }

    // Notebook for the label counter and the current function
    codegenContext context;
    context.labelCounter = 0;
    context.currentFunction = NULL;

    // We need main to know what to call at the start
    const astNode* mainNode = findMainFunction(program);
    if (mainNode == NULL)
    {
        return ERROR_INTERNAL;
    }

    // First lines of the program
    RETURN_IF_ERROR(writeProgramStart(mainNode));

    // How many functions the program has
    size_t functionCount = (size_t)program->childCount;

    // Print every function in the order from the tree
    for (size_t i = 0; i < functionCount; i++)
    {
        RETURN_IF_ERROR(generateFunction(program->children[i], &context));
    }

    // add our library at the end
    RETURN_IF_ERROR(writeLibrary());

    return ERROR_SUCCESS;
}
