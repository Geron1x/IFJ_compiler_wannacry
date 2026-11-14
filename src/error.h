/**
 * @file error.h
 * @brief Error codes definition for the IFJ26 compiler
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#ifndef ERROR_H
#define ERROR_H

/** Compiler return error codes */
typedef enum
{
    ERROR_SUCCESS = 0,         /**< Compilation finished successfully */
    ERROR_LEXICAL = 1,         /**< Lexical analysis error */
    ERROR_SYNTAX = 2,          /**< Syntax analysis error */
    ERROR_UNDEFINED_NAME = 3,  /**< Undefined function or variable */
    ERROR_REDEFINITION = 4,    /**< Redefinition of function or variable */
    ERROR_WRONG_ARGUMENTS = 5, /**< Wrong number or type of arguments */
    ERROR_TYPE_MISMATCH = 6,   /**< Type compatibility or inference error */
    ERROR_IMMUTABLE_WRITE = 7, /**< Assignment to an immutable variable */
    ERROR_OTHER_SEMANTIC = 10, /**< Other semantic error */
    ERROR_INTERNAL = 99,       /**< system is out of memory */

    ERROR_RUNTIME = 40, /**< Runtime error code for interpreter */
} Errors;

#endif
