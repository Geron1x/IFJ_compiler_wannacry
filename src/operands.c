/**
 * @file operands.c
 * @brief Turn names, labels and values into IFJcode26
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#include "operands.h"

#include <stdio.h>

/** Helps to print code to the output */
Errors tryPrint(const char* code)
{
    if (code == NULL)  // Check for NULL
    {
        return ERROR_INTERNAL;
    }

    if (fputs(code, stdout) == EOF)  // Failed because fputs gave EOF
    {
        return ERROR_INTERNAL;
    }

    return ERROR_SUCCESS;
}

/** Prints bool@true or bool@false */
Errors writeLiteralBool(bool boolValue)
{
    if (boolValue == true)
    {
        return tryPrint("bool@true");
    }
    else
    {
        return tryPrint("bool@false");
    }
}

/** Prints int value to stdout */
Errors writeLiteralInt(long long intValue)
{
    if (printf("int@%lld", intValue) < 0)
    {
        return ERROR_INTERNAL;
    }

    return ERROR_SUCCESS;
}

/** Prints nil (nothing) */
Errors writeLiteralNil(void)
{
    return tryPrint("nil@nil");
}

/** Prints float value */
Errors writeLiteralFloat(double floatValue)
{
    if (printf("float@%a", floatValue) < 0)  // %a means in hex form
    {
        return ERROR_INTERNAL;
    }

    return ERROR_SUCCESS;
}
