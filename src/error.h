/*
 Project: Implementace překladače imperativního jazyka IFJ26
 Authors:
 Maksym Rusyn (xrusynm00)
*/

#ifndef ERROR_H
#define ERROR_H

typedef enum
{
    ERROR_LEXICAL = 1,
    ERROR_SYNTAX = 2,
    ERROR_UNDEFINED_NAME = 3,   // undefined function or variable
    ERROR_REDEFINITION = 4,     // redefinition of function or variable
    ERROR_WRONG_ARGUMENTS = 5,  // wrong number or type of arguments
    ERROR_TYPE_MISMATCH = 6,    // Type compatibility or type inference failed in expressions, bla bla bla etc..
    ERROR_IMMUTABLE_WRITE = 7,    // attempt to assign to an immutable variable
    ERROR_OTHER_SEMANTIC = 10,  // everything else
    ERROR_INTERNAL = 99,       // system is out of memory we are sinking

    ERROR_RUNTIME = 40,
}Errors; 

#endif
