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

/** Makes a list of numbers from start to end with a step */
static int writeListRange(void)
{
    RETURN_IF_ERROR(writeLibraryStart("list$range"));

    // get all four params from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@isInclusive\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@isInclusive\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@step\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@step\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@end\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@end\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@value\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@value\n"));

    // step zero stops the program
    RETURN_IF_ERROR(tryPrint("JUMPIFNEQ ?list$range$stepOk LF@step int@0\n"));
    RETURN_IF_ERROR(tryPrint("EXIT int@40\n"));
    RETURN_IF_ERROR(tryPrint("LABEL ?list$range$stepOk\n"));

    // look which way we go
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@isUp\n"));
    RETURN_IF_ERROR(tryPrint("GT LF@isUp LF@step int@0\n"));

    // end is inside too, so move the end one step further
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$range$endOk LF@isInclusive bool@false\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?list$range$up LF@isUp bool@true\n"));
    RETURN_IF_ERROR(tryPrint("SUB LF@end LF@end int@1\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$range$endOk\n"));
    RETURN_IF_ERROR(tryPrint("LABEL ?list$range$up\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@end LF@end int@1\n"));
    RETURN_IF_ERROR(tryPrint("LABEL ?list$range$endOk\n"));

    // make all variables before the loop
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@count int@0\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@check\n"));

    // going up stops when we reach the end
    RETURN_IF_ERROR(tryPrint("LABEL ?list$range$loop\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?list$range$down LF@isUp bool@false\n"));
    RETURN_IF_ERROR(tryPrint("LT LF@check LF@value LF@end\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$range$done LF@check bool@false\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$range$push\n"));

    // going down stops when we reach the end too
    RETURN_IF_ERROR(tryPrint("LABEL ?list$range$down\n"));
    RETURN_IF_ERROR(tryPrint("GT LF@check LF@value LF@end\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$range$done LF@check bool@false\n"));

    // put the number on stack and go next
    RETURN_IF_ERROR(tryPrint("LABEL ?list$range$push\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@value\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@count LF@count int@1\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@value LF@value LF@step\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$range$loop\n"));

    // all numbers are on stack, make a list from them
    RETURN_IF_ERROR(tryPrint("LABEL ?list$range$done\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@count\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$make\n"));

    return writeLibraryEnd();
}

/** Puts all items of the list on the stack */
static int writeListUnpack(void)
{
    RETURN_IF_ERROR(writeLibraryStart("list$unpack"));

    // take the list from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));

    // make all variables before the loop
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@count LF@list int@0\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@index\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@item\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@check\n"));

    // stop after the last item
    RETURN_IF_ERROR(tryPrint("LABEL ?list$unpack$loop\n"));
    RETURN_IF_ERROR(tryPrint("GT LF@check LF@index LF@count\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$unpack$done LF@check bool@true\n"));

    // put the item on stack and go next
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@item LF@list LF@index\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@item\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@index LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$unpack$loop\n"));

    // end of unpack
    RETURN_IF_ERROR(tryPrint("LABEL ?list$unpack$done\n"));

    return writeLibraryEnd();
}

/** Makes one list from two */
static int writeListConcat(void)
{
    RETURN_IF_ERROR(writeLibraryStart("list$concat"));

    // get both lists from stack, right one is on top
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@right\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@right\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@left\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@left\n"));

    // put items of both lists on stack
    RETURN_IF_ERROR(tryPrint("PUSHS LF@left\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$unpack\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@right\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$unpack\n"));

    // count all items together
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@leftCount\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@leftCount LF@left int@0\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@rightCount\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@rightCount LF@right int@0\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@count LF@leftCount LF@rightCount\n"));

    // make one list from all of them
    RETURN_IF_ERROR(tryPrint("PUSHS LF@count\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$make\n"));

    return writeLibraryEnd();
}

/** Gives how many items the list has */
static int writeListLength(void)
{
    // start like every func
    RETURN_IF_ERROR(writeLibraryStart("list$length"));

    // length lives in the first cell
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@count LF@list int@0\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@count\n"));

    return writeLibraryEnd();
}

/** Keeps only items where the function says true */
static int writeListFilter(void)
{
    RETURN_IF_ERROR(writeLibraryStart("list$filter"));

    // get both params from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));

    // make all variables before the loop
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@count LF@list int@0\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@index\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@kept\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@kept int@0\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@item\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@check\n"));

    // stop after the last item
    RETURN_IF_ERROR(tryPrint("LABEL ?list$filter$loop\n"));
    RETURN_IF_ERROR(tryPrint("GT LF@check LF@index LF@count\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$filter$done LF@check bool@true\n"));

    // ask the function about the item
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@item LF@list LF@index\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@item\n"));
    RETURN_IF_ERROR(tryPrint("CALL LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@check\n"));

    // true means we keep it on stack
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$filter$next LF@check bool@false\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@item\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@kept LF@kept int@1\n"));

    // go to next item
    RETURN_IF_ERROR(tryPrint("LABEL ?list$filter$next\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@index LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$filter$loop\n"));

    // kept items are on stack, make a list from them
    RETURN_IF_ERROR(tryPrint("LABEL ?list$filter$done\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@kept\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$make\n"));

    return writeLibraryEnd();
}

/** Counts items from the start while the function says true */
static int writeListCountWhile(void)
{
    RETURN_IF_ERROR(writeLibraryStart("list$countWhile"));

    // get both params from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));

    // make all variables before the loop
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@count\n"));
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@count LF@list int@0\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@index\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@number\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@number int@0\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@item\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@check\n"));

    // stop after the last item
    RETURN_IF_ERROR(tryPrint("LABEL ?list$countWhile$loop\n"));
    RETURN_IF_ERROR(tryPrint("GT LF@check LF@index LF@count\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$countWhile$done LF@check bool@true\n"));

    // ask the function, first false stops us
    RETURN_IF_ERROR(tryPrint("ARRAYGET LF@item LF@list LF@index\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@item\n"));
    RETURN_IF_ERROR(tryPrint("CALL LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@check\n"));
    RETURN_IF_ERROR(
        tryPrint("JUMPIFEQ ?list$countWhile$done LF@check bool@false\n"));

    // one more good item, go next
    RETURN_IF_ERROR(tryPrint("ADD LF@number LF@number int@1\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@index LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?list$countWhile$loop\n"));

    // give how many good items we found
    RETURN_IF_ERROR(tryPrint("LABEL ?list$countWhile$done\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@number\n"));

    return writeLibraryEnd();
}

/** Takes items from the start while the function says true */
static int writeListTakeWhile(void)
{
    // start like every func
    RETURN_IF_ERROR(writeLibraryStart("list$takeWhile"));

    // get both params from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));

    // count good items from the start
    RETURN_IF_ERROR(tryPrint("PUSHS LF@list\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$countWhile\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@number\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@number\n"));

    // takeWhile is just slice from zero till that number
    RETURN_IF_ERROR(tryPrint("PUSHS LF@list\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS int@0\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@number\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$slice\n"));

    return writeLibraryEnd();
}

/** Throws away items from the start while the function says true */
static int writeListDropWhile(void)
{
    // start like every func
    RETURN_IF_ERROR(writeLibraryStart("list$dropWhile"));

    // get both params from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@list\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@list\n"));

    // count good items from the start
    RETURN_IF_ERROR(tryPrint("PUSHS LF@list\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@predicate\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$countWhile\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@number\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@number\n"));

    // dropWhile is just drop that number
    RETURN_IF_ERROR(tryPrint("PUSHS LF@list\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@number\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$drop\n"));

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
    RETURN_IF_ERROR(writeListRange());
    RETURN_IF_ERROR(writeListUnpack());
    RETURN_IF_ERROR(writeListConcat());
    RETURN_IF_ERROR(writeListLength());
    RETURN_IF_ERROR(writeListFilter());
    RETURN_IF_ERROR(writeListCountWhile());
    RETURN_IF_ERROR(writeListTakeWhile());
    RETURN_IF_ERROR(writeListDropWhile());

    return ERROR_SUCCESS;
}
