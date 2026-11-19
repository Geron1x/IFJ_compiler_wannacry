/**
 * @file ifjcode_lib.c
 * @brief Built-in functions of IFJ26 in IFJcode26
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#include "ifjcode_lib.h"

#include <stdio.h>

#include "operands.h"

/** Prints the start of a built-in function */
static int writeLibraryStart(const char* name)
{
    RETURN_IF_ERROR(tryPrint("\nLABEL "));
    RETURN_IF_ERROR(writeFuncLabel(name));
    RETURN_IF_ERROR(tryPrint("\n"));
    RETURN_IF_ERROR(tryPrint("CREATEFRAME\n"));
    RETURN_IF_ERROR(tryPrint("PUSHFRAME\n"));

    return ERROR_SUCCESS;
}

/** Prints the end of a built-in function */
static int writeLibraryEnd(void)
{
    //  close the frame
    RETURN_IF_ERROR(tryPrint("POPFRAME\n"));
    RETURN_IF_ERROR(tryPrint("RETURN\n"));

    return ERROR_SUCCESS;
}

/** Prints code of print function */
static int writeLibraryPrint(void)
{
    // start like every func
    RETURN_IF_ERROR(writeLibraryStart("print"));

    // take the value from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@value\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@value\n"));

    // if its () go to unit, else write it
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?print$unit LF@value nil@nil\n"));
    RETURN_IF_ERROR(tryPrint("WRITE LF@value\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?print$end\n"));

    // unit case, so we print () like a text
    RETURN_IF_ERROR(tryPrint("LABEL ?print$unit\n"));
    RETURN_IF_ERROR(tryPrint("WRITE string@()\n"));

    // all ways meet here(rome)
    RETURN_IF_ERROR(tryPrint("LABEL ?print$end\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS nil@nil\n"));

    return writeLibraryEnd();
}

/** Prints a function that reads line from input */
static int writeLibraryRead(const char* name, const char* type,
                            const char* defaultValue)
{
    RETURN_IF_ERROR(writeLibraryStart(name));

    // read the value from input
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@value\n"));
    RETURN_IF_ERROR(tryPrint("READ LF@value "));
    RETURN_IF_ERROR(tryPrint(type));
    RETURN_IF_ERROR(tryPrint("\n"));

    // if we got a value go to end, else put the default
    RETURN_IF_ERROR(tryPrint("JUMPIFNEQ ?"));
    RETURN_IF_ERROR(tryPrint(name));
    RETURN_IF_ERROR(tryPrint("$end LF@value nil@nil\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@value "));
    RETURN_IF_ERROR(tryPrint(defaultValue));
    RETURN_IF_ERROR(tryPrint("\n"));

    // give the value back
    RETURN_IF_ERROR(tryPrint("LABEL ?"));
    RETURN_IF_ERROR(tryPrint(name));
    RETURN_IF_ERROR(tryPrint("$end\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@value\n"));

    return writeLibraryEnd();
}

/** Prints all built-in functions */
int writeLibrary(void)
{
    RETURN_IF_ERROR(writeLibraryPrint());

    // all read functions
    RETURN_IF_ERROR(writeLibraryRead("readInt", "int", "int@0"));
    RETURN_IF_ERROR(writeLibraryRead("readDouble", "float", "float@0x0p+0"));
    RETURN_IF_ERROR(writeLibraryRead("readLine", "string", "string@"));

    return ERROR_SUCCESS;
}
