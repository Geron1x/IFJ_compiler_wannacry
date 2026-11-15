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
int tryPrint(const char* code)
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
int writeLiteralBool(bool boolValue)
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
int writeLiteralInt(long long intValue)
{
    if (printf("int@%lld", intValue) < 0)
    {
        return ERROR_INTERNAL;
    }

    return ERROR_SUCCESS;
}

/** Prints nil (nothing) */
int writeLiteralNil(void)
{
    return tryPrint("nil@nil");
}

/** Prints float value */
int writeLiteralFloat(double floatValue)
{
    if (printf("float@%a", floatValue) < 0)  // %a means in hex form
    {
        return ERROR_INTERNAL;
    }

    return ERROR_SUCCESS;
}

/** Prints string@ and chars */
int writeLiteralString(const char* stringValue)
{
    if (stringValue == NULL)
    {
        return ERROR_INTERNAL;
    }

    // prints: string@
    if (tryPrint("string@") != ERROR_SUCCESS)
    {
        return ERROR_INTERNAL;
    }

    for (size_t i = 0; stringValue[i] != '\0'; i++)
    {
        unsigned char currentChar = (unsigned char)stringValue[i];

        // 35 -> # ;    92 -> \ ;   >=127 -> not printable ASCII
        if (currentChar <= 32 || currentChar == 35 || currentChar == 92 ||
            currentChar >= 127)
        {
            if (printf("\\%03d", currentChar) < 0)  // char with \ in start
            {
                return ERROR_INTERNAL;
            }
        }
        else
        {
            if (fputc(currentChar, stdout) == EOF)  // print normal char
            {
                return ERROR_INTERNAL;
            }
        }
    }

    return ERROR_SUCCESS;
}
