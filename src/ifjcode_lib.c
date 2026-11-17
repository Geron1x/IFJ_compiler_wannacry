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

/** Prints code of print function */
static int writeLibraryPrint(void)
{
    // start like every func
    RETURN_IF_ERROR(tryPrint("\nLABEL $$print\n"));
    RETURN_IF_ERROR(tryPrint("CREATEFRAME\n"));
    RETURN_IF_ERROR(tryPrint("PUSHFRAME\n"));

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

    // both ways meet here(rome)
    RETURN_IF_ERROR(tryPrint("LABEL ?print$end\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS nil@nil\n"));
    RETURN_IF_ERROR(tryPrint("POPFRAME\n"));
    RETURN_IF_ERROR(tryPrint("RETURN\n"));

    return ERROR_SUCCESS;
}

/** Prints all built-in functions */
int writeLibrary(void)
{
    RETURN_IF_ERROR(writeLibraryPrint());

    return ERROR_SUCCESS;
}