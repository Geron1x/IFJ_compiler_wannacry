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
int writeLibraryStart(const char* name)
{
    RETURN_IF_ERROR(tryPrint("\nLABEL "));
    RETURN_IF_ERROR(writeFuncLabel(name));
    RETURN_IF_ERROR(tryPrint("\n"));
    RETURN_IF_ERROR(tryPrint("CREATEFRAME\n"));
    RETURN_IF_ERROR(tryPrint("PUSHFRAME\n"));

    return ERROR_SUCCESS;
}

/** Prints the end of a built-in function */
int writeLibraryEnd(void)
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

    // list must become text before we write it
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@type\n"));
    RETURN_IF_ERROR(tryPrint("TYPE LF@type LF@value\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFNEQ ?print$write LF@type string@array\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@value\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$toString\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@value\n"));

    // write the value
    RETURN_IF_ERROR(tryPrint("LABEL ?print$write\n"));
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

/** Prints function that cuts double to int */
static int writeLibraryToInt(void)
{
    RETURN_IF_ERROR(writeLibraryStart("toInt"));

    RETURN_IF_ERROR(tryPrint("FLOAT2INTS\n"));

    return writeLibraryEnd();
}

/** Prints function that makes char from number */
static int writeLibraryChr(void)
{
    RETURN_IF_ERROR(writeLibraryStart("chr"));

    // make char from the number on stack
    RETURN_IF_ERROR(tryPrint("INT2CHARS\n"));

    return writeLibraryEnd();
}

/** Prints function that gives length of string */
static int writeLibraryStrLength(void)
{
    RETURN_IF_ERROR(writeLibraryStart("strLength"));

    // take the string from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@value\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@value\n"));

    // count the length
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@length\n"));
    RETURN_IF_ERROR(tryPrint("STRLEN LF@length LF@value\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@length\n"));

    return writeLibraryEnd();
}

/** Prints function that gives code of char in string */
static int writeLibraryOrd(void)
{
    RETURN_IF_ERROR(writeLibraryStart("ord"));

    // get both params from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@index\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@value\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@index\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@value\n"));

    // count the length
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@length\n"));
    RETURN_IF_ERROR(tryPrint("STRLEN LF@length LF@value\n"));

    // check if index is negative
    RETURN_IF_ERROR(tryPrint("PUSHS LF@index\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS int@0\n"));
    RETURN_IF_ERROR(tryPrint("LTS\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@true\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQS ?ord$bad\n"));

    // check if index is outside the string
    RETURN_IF_ERROR(tryPrint("PUSHS LF@index\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@length\n"));
    RETURN_IF_ERROR(tryPrint("LTS\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@false\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQS ?ord$bad\n"));

    // all good, take the char code
    RETURN_IF_ERROR(tryPrint("PUSHS LF@value\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@index\n"));
    RETURN_IF_ERROR(tryPrint("STRI2INTS\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?ord$end\n"));

    // bad index gives -1
    RETURN_IF_ERROR(tryPrint("LABEL ?ord$bad\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS int@-1\n"));

    // end of ord
    RETURN_IF_ERROR(tryPrint("LABEL ?ord$end\n"));

    return writeLibraryEnd();
}

/** Prints function that makes string from any value */
static int writeLibraryToStr(void)
{
    RETURN_IF_ERROR(writeLibraryStart("toStr"));

    // take the value from stack
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@value\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@value\n"));

    // look what type we got and jump to its part
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@type\n"));
    RETURN_IF_ERROR(tryPrint("TYPE LF@type LF@value\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?toStr$int LF@type string@int\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?toStr$float LF@type string@float\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?toStr$bool LF@type string@bool\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?toStr$unit LF@type string@nil\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?toStr$list LF@type string@array\n"));

    // no jump worked so its already a string
    RETURN_IF_ERROR(tryPrint("PUSHS LF@value\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?toStr$end\n"));

    // int to string, like 5 becomes "5"
    RETURN_IF_ERROR(tryPrint("LABEL ?toStr$int\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@value\n"));
    RETURN_IF_ERROR(tryPrint("INT2STRS\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?toStr$end\n"));

    // double to string, like 3.1415 becomes "3.14"
    RETURN_IF_ERROR(tryPrint("LABEL ?toStr$float\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@value\n"));
    RETURN_IF_ERROR(tryPrint("FLOAT2STRS\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?toStr$end\n"));

    // bool case, true gives "true" and false gives "false"
    RETURN_IF_ERROR(tryPrint("LABEL ?toStr$bool\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQ ?toStr$true LF@value bool@true\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS string@false\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?toStr$end\n"));
    RETURN_IF_ERROR(tryPrint("LABEL ?toStr$true\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS string@true\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?toStr$end\n"));

    // list case, list makes its own text
    RETURN_IF_ERROR(tryPrint("LABEL ?toStr$list\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@value\n"));
    RETURN_IF_ERROR(tryPrint("CALL $$list$toString\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?toStr$end\n"));

    // empty value is shown like ()
    RETURN_IF_ERROR(tryPrint("LABEL ?toStr$unit\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS string@()\n"));

    // end of toStr
    RETURN_IF_ERROR(tryPrint("LABEL ?toStr$end\n"));

    return writeLibraryEnd();
}

/** Prints function that cuts a part of string */
static int writeLibrarySubStr(void)
{
    RETURN_IF_ERROR(writeLibraryStart("subStr"));

    // take params from the end
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@value\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@start\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@end\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@end\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@start\n"));
    RETURN_IF_ERROR(tryPrint("POPS LF@value\n"));

    // count the length
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@length\n"));
    RETURN_IF_ERROR(tryPrint("STRLEN LF@length LF@value\n"));

    // check if start is negative
    RETURN_IF_ERROR(tryPrint("PUSHS LF@start\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS int@0\n"));
    RETURN_IF_ERROR(tryPrint("LTS\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@true\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQS ?substr$error\n"));

    // check if end is negative
    RETURN_IF_ERROR(tryPrint("PUSHS LF@end\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS int@0\n"));
    RETURN_IF_ERROR(tryPrint("LTS\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@true\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQS ?substr$error\n"));

    // check if start is bigger than end
    RETURN_IF_ERROR(tryPrint("PUSHS LF@start\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@end\n"));
    RETURN_IF_ERROR(tryPrint("GTS\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@true\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQS ?substr$error\n"));

    // start outside the string - error
    RETURN_IF_ERROR(tryPrint("PUSHS LF@start\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@length\n"));
    RETURN_IF_ERROR(tryPrint("LTS\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@false\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQS ?substr$error\n"));

    // check if end is bigger than length
    RETURN_IF_ERROR(tryPrint("PUSHS LF@end\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@length\n"));
    RETURN_IF_ERROR(tryPrint("GTS\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@true\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQS ?substr$error\n"));

    // make all variables before the loop
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@result\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@result string@\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@index\n"));
    RETURN_IF_ERROR(tryPrint("MOVE LF@index LF@start\n"));
    RETURN_IF_ERROR(tryPrint("DEFVAR LF@char\n"));

    // loop goes while index is smaller than end
    RETURN_IF_ERROR(tryPrint("LABEL ?substr$loop\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@index\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@end\n"));
    RETURN_IF_ERROR(tryPrint("LTS\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS bool@false\n"));
    RETURN_IF_ERROR(tryPrint("JUMPIFEQS ?substr$done\n"));

    // take one char, add it to result and go next
    RETURN_IF_ERROR(tryPrint("GETCHAR LF@char LF@value LF@index\n"));
    RETURN_IF_ERROR(tryPrint("CONCAT LF@result LF@result LF@char\n"));
    RETURN_IF_ERROR(tryPrint("ADD LF@index LF@index int@1\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?substr$loop\n"));

    // result goes on stack
    RETURN_IF_ERROR(tryPrint("LABEL ?substr$done\n"));
    RETURN_IF_ERROR(tryPrint("PUSHS LF@result\n"));
    RETURN_IF_ERROR(tryPrint("JUMP ?substr$end\n"));

    // error part
    RETURN_IF_ERROR(tryPrint("LABEL ?substr$error\n"));
    RETURN_IF_ERROR(tryPrint("EXIT int@40\n"));

    // end of substr
    RETURN_IF_ERROR(tryPrint("LABEL ?substr$end\n"));

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
    RETURN_IF_ERROR(writeLibraryRead("readBoolean", "bool", "bool@false"));

    RETURN_IF_ERROR(writeLibraryToInt());

    RETURN_IF_ERROR(writeLibraryChr());

    RETURN_IF_ERROR(writeLibraryStrLength());

    RETURN_IF_ERROR(writeLibraryOrd());

    RETURN_IF_ERROR(writeLibraryToStr());

    RETURN_IF_ERROR(writeLibrarySubStr());

    return ERROR_SUCCESS;
}
