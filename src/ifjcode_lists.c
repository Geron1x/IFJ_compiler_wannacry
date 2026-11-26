/**
 * @file ifjcode_lists.c
 * @brief Functions for LISTS
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#include "ifjcode_lists.h"

#include <stdio.h>

#include "ifjcode_lib.h"
#include "operands.h"

/** Makes a list from items on the stack */
static int writeListMake(void)
{
    // start like every func
    RETURN_IF_ERROR(writeLibraryStart("list$make"));

    // take how many items we got
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@count\n"));

    // first cell is for length, so plus one
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@size\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@size LF@count int@1\n"));

    // new list, length goes in first cell
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("NEWARRAY LF@list LF@size\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYSET LF@list int@0 LF@count\n"));

    // counter starts from the last item
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@index\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@index LF@count\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@item\n"));

    // stop when all items are in
    RETURN_IF_ERROR(tryPrint("LABEL ?list$make$loop\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?list$make$done LF@index int@0\n"));

    // item from stack goes in the list
    RETURN_IF_ERROR(tryPrint("POPS LF@item\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYSET LF@list LF@index LF@item\n"));

    // next item, back to the start
    RETURN_IF_ERROR(tryPrint("SUB LF@index LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$make$loop\n"));

    // list goes on stack
    RETURN_IF_ERROR(tryPrint("LABEL ?list$make$done\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@list\n"));

    return writeLibraryEnd();
}

/** Gives the item of the list on the index */
static int writeListGet(void)
{
    // start like every func
    RETURN_IF_ERROR(writeLibraryStart("list$get"));

    // take the list and the index
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@index\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@index\n"));

    // take the length from first cell
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@count LF@list int@0\n"));

    // check if index is negative
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@check\n"));
    RETURN_IF_ERROR(tryPrint("LT LF@check LF@index int@0\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?list$get$bad LF@check bool@true\n"));

    // check if index is outside the list
    RETURN_IF_ERROR(tryPrint("LT LF@check LF@index LF@count\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?list$get$bad LF@check bool@false\n"));

    // plus one cause first cell keeps length
    RETURN_IF_ERROR(tryPrint("ADD LF@index LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@item\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@item LF@list LF@index\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@item\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$get$end\n"));

    // bad index stops the program
    RETURN_IF_ERROR(tryPrint("LABEL ?list$get$bad\n"));
    RETURN_IF_ERROR(tryPrint("EXIT int@40\n"));

    // end of get
    RETURN_IF_ERROR(tryPrint("LABEL ?list$get$end\n"));

    return writeLibraryEnd();
}

/** Makes text from the list */
static int writeListToString(void)
{
    // start like every func
    RETURN_IF_ERROR(writeLibraryStart("list$toString"));

    // take the list from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));

    // how long the list is
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@count LF@list int@0\n"));

    // start the text
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@text\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@text string@List(\n"));

    // make all variables before the loop
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@index\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@item\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@part\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@check\n"));

    // stop after the last item
    RETURN_IF_ERROR(tryPrint("LABEL ?list$toString$loop\n"));
    RETURN_IF_ERROR(tryPrint("GT LF@check LF@index LF@count\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$toString$done LF@check bool@true\n"));

    // first item goes without comma
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?list$toString$first LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("CONCAT LF@text LF@text string@,\\032\n"));
    RETURN_IF_ERROR(tryPrint("LABEL ?list$toString$first\n"));

    // item becomes text and goes in the end
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@item LF@list LF@index\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@item\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$toStr\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@part\n"));
    RETURN_IF_ERROR(tryPrint("CONCAT LF@text LF@text LF@part\n"));

    // go to next item
    RETURN_IF_ERROR(tryPrint("ADD LF@index LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$toString$loop\n"));

    // close the bracket and give the text back
    RETURN_IF_ERROR(tryPrint("LABEL ?list$toString$done\n"));
    RETURN_IF_ERROR(tryPrint("CONCAT LF@text LF@text string@)\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@text\n"));

    return writeLibraryEnd();
}

/** Prints all functions for lists */
int writeListLibrary(void)
{
    RETURN_IF_ERROR(writeListMake());
    RETURN_IF_ERROR(writeListGet());
    RETURN_IF_ERROR(writeListToString());

    return ERROR_SUCCESS;
}
