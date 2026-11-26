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

/** Gives the part of the list from one index until another */
static int writeListSlice(void)
{
    RETURN_IF_ERROR(writeLibraryStart("list$slice"));

    // get all three params from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@until\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@until\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@from\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@from\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));

    // how long the list is
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@count LF@list int@0\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@check\n"));

    // start below zero, move it to zero
    RETURN_IF_ERROR(tryPrint("LT LF@check LF@from int@0\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$slice$fromOk LF@check bool@false\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@from int@0\n"));
    RETURN_IF_ERROR(tryPrint("LABEL ?list$slice$fromOk\n"));

    // end past the list, move it to the end
    RETURN_IF_ERROR(tryPrint("GT LF@check LF@until LF@count\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$slice$untilOk LF@check bool@false\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@until LF@count\n"));
    RETURN_IF_ERROR(tryPrint("LABEL ?list$slice$untilOk\n"));

    // end before start gives empty list
    RETURN_IF_ERROR(tryPrint("LT LF@check LF@until LF@from\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$slice$sizeOk LF@check bool@false\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@until LF@from\n"));
    RETURN_IF_ERROR(tryPrint("LABEL ?list$slice$sizeOk\n"));

    // new list, length goes in first cell
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@newCount\n"));
    RETURN_IF_ERROR(tryPrint("SUB LF@newCount LF@until LF@from\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@size\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@size LF@newCount int@1\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@result\n"));
    RETURN_IF_ERROR(tryPrint("NEWARRAY LF@result LF@size\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYSET LF@result int@0 LF@newCount\n"));

    // make all variables before the loop
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@oldIndex\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@oldIndex LF@from int@1\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@newIndex\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@newIndex int@1\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@item\n"));

    // stop when we copied all
    RETURN_IF_ERROR(tryPrint("LABEL ?list$slice$loop\n"));
    RETURN_IF_ERROR(tryPrint("GT LF@check LF@newIndex LF@newCount\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?list$slice$done LF@check bool@true\n"));

    // take item and put it in new list
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@item LF@list LF@oldIndex\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYSET LF@result LF@newIndex LF@item\n"));

    // both counters go next
    RETURN_IF_ERROR(tryPrint("ADD LF@oldIndex LF@oldIndex int@1\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@newIndex LF@newIndex int@1\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$slice$loop\n"));

    // new list goes on stack
    RETURN_IF_ERROR(tryPrint("LABEL ?list$slice$done\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@result\n"));

    return writeLibraryEnd();
}

/** Throws away first items of the list */
static int writeListDrop(void)
{
    RETURN_IF_ERROR(writeLibraryStart("list$drop"));

    // get both params from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@number\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@number\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));

    // how long the list is
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@count LF@list int@0\n"));

    // drop is just slice from that number till the end
    RETURN_IF_ERROR(tryPrint("PUSHS LF@list\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@number\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@count\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$slice\n"));

    return writeLibraryEnd();
}

/** Gives the list without the first item */
static int writeListTail(void)
{
    RETURN_IF_ERROR(writeLibraryStart("list$tail"));

    // take the list from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));

    // check if list is empty
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@count LF@list int@0\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?list$tail$bad LF@count int@0\n"));

    // throw away first item
    RETURN_IF_ERROR(tryPrint("PUSHS LF@list\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS int@1\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$drop\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$tail$end\n"));

    // empty list stops the program
    RETURN_IF_ERROR(tryPrint("LABEL ?list$tail$bad\n"));
    RETURN_IF_ERROR(tryPrint("EXIT int@40\n"));

    // end
    RETURN_IF_ERROR(tryPrint("LABEL ?list$tail$end\n"));

    return writeLibraryEnd();
}

/** Prints all functions for lists */
int writeListLibrary(void)
{
    RETURN_IF_ERROR(writeListMake());
    RETURN_IF_ERROR(writeListGet());
    RETURN_IF_ERROR(writeListToString());
    RETURN_IF_ERROR(writeListSlice());
    RETURN_IF_ERROR(writeListDrop());
    RETURN_IF_ERROR(writeListTail());

    return ERROR_SUCCESS;
}
