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

    // todo, body of the func
    RETURN_IF_ERROR(tryPrint("PUSHS nil@nil\n"));
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
