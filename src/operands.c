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
