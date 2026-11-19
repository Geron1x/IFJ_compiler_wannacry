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

/** Prints a variable name to stdout */
int writeVarName(const char* name, size_t number)
{
    if (name == NULL)
    {
        return ERROR_INTERNAL;
    }

    if (tryPrint("LF@") != ERROR_SUCCESS)  // Prints LF@
    {
        return ERROR_INTERNAL;
    }

    if (tryPrint(name) != ERROR_SUCCESS)  // Prints var name
    {
        return ERROR_INTERNAL;
    }

    // Prints $ and num, so var will never repeat
    if (printf("$%zu", number) < 0)
    {
        return ERROR_INTERNAL;
    }

    return ERROR_SUCCESS;
}

/** Prints label for function to stdout */
int writeFuncLabel(const char* name)
{
    // just check before printing anything
    if (name == NULL)
    {
        return ERROR_INTERNAL;
    }

    // print $$ before the name, cause I don't want any matches with user
    if (tryPrint("$$") != ERROR_SUCCESS)
    {
        return ERROR_INTERNAL;
    }

    if (tryPrint(name) != ERROR_SUCCESS)
    {
        return ERROR_INTERNAL;
    }

    return ERROR_SUCCESS;
}

/** Prints a temporary variable name */
int writeTempVar(size_t number)
{
    return writeVarName("$tmp", number);
}

/** Prints instruction with temp variable */
int writeTempInstruction(const char* instruction, size_t number)
{
    // for example POPS LF@$tmp$2
    RETURN_IF_ERROR(tryPrint(instruction));
    RETURN_IF_ERROR(tryPrint(" "));
    RETURN_IF_ERROR(writeTempVar(number));
    RETURN_IF_ERROR(tryPrint("\n"));

    return ERROR_SUCCESS;
}

/** Prints instruction and variable */
int writeVarInstruction(const char* instruction, const char* name,
                        size_t number)
{
    RETURN_IF_ERROR(tryPrint(instruction));
    RETURN_IF_ERROR(tryPrint(" "));
    RETURN_IF_ERROR(writeVarName(name, number));
    RETURN_IF_ERROR(tryPrint("\n"));

    return ERROR_SUCCESS;
}

/** Gives us a text for jump labels */
static const char* getJumpLabelText(jumpLabelType type)
{
    switch (type)
    {
        case LABEL_IF_ELSE:
            return "if$else";

        case LABEL_IF_END:
            return "if$end";

        case LABEL_WHILE_START:
            return "while$start";

        case LABEL_WHILE_END:
            return "while$end";

        case LABEL_AND_FALSE:
            return "and$false";

        case LABEL_AND_END:
            return "and$end";

        case LABEL_OR_TRUE:
            return "or$true";

        case LABEL_OR_END:
            return "or$end";

        case LABEL_FOR_CHECK:
            return "for$check";

        case LABEL_FOR_NEXT:
            return "for$next";

        case LABEL_FOR_END:
            return "for$end";

        default:  // not in the list
            return NULL;
    }
}

/** Prints a jump label to stdout */
int writeJumpLabel(jumpLabelType type, size_t number)
{
    // words from getJumpLabelText, like while$start etc..
    const char* labelText = getJumpLabelText(type);

    if (labelText == NULL)
    {
        return ERROR_INTERNAL;
    }

    // ? -> marks a jump
    if (tryPrint("?") != ERROR_SUCCESS)
    {
        return ERROR_INTERNAL;
    }

    if (tryPrint(labelText) != ERROR_SUCCESS)
    {
        return ERROR_INTERNAL;
    }

    if (printf("$%zu", number) < 0)
    {
        return ERROR_INTERNAL;
    }

    return ERROR_SUCCESS;
}
