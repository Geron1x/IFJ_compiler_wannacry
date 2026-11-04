/**
 * @file symtable.h
 * @brief header file for the symtable implementation
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H
#include <stdbool.h>

#include "bst.h"
#include "error.h"

typedef enum
{
    TYPE_UNIT,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_ANY,
    TYPE_UNKNOWN
} dataType;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_VARIABLE
} idType;

typedef struct symtable symtable;
typedef struct nodeData nodeData;

/**
 * @brief Opens new scope for local variables
 */
Errors stackPush(symtable* symtable);

/**
 * @brief Pops top scope and deletes all the variables inside
 */
void stackPop(symtable* symtable);

/**
 * @brief Inserts a variable into current scope (local tree), can give
 * redifinition feedback error code
 */
Errors stackInsertVar(symtable* symtable, const char* key, dataType type,
                      bool isConst, bool isInit);

/**
 * @brief Searches for a variable in the all present scopes
 */
nodeData* stackFindVar(const char* key, symtable* symtable);

/**
 * @brief Destructor to clear all scopes present in symtable
 */
void stackDestroy(symtable* symtable);

/**
 * @brief Destructor function to clear nodeData for bst.c
 */
void eraseData(nodeData* data);

/**
 * @brief Initialize all basic functions into symtable
 */
Errors symtableBaseFuncInit(symtable* funcTable);

/**
 * @brief Inserts function into symtable,
 * can give redifinition feedback error code
 */
Errors symtableInsertFunc(symtable* symtable, const char* key,
                          dataType returnType, int paramCount,
                          const dataType* paramType);

/**
 * @brief Initialize symtable
 */
Errors symtableInit(symtable** symtable);

/**
 * @brief Destructor for symtable
 */
void symtableDestroy(symtable* table);


#endif