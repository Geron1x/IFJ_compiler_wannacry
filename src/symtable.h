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
#include <stddef.h>

#include "bst.h"
#include "error.h"
/** Types stored in the semantic symbol table */
typedef enum
{
    S_TYPE_UNIT,
    S_TYPE_INT,
    S_TYPE_DOUBLE,
    S_TYPE_STRING,
    S_TYPE_BOOL,
    S_TYPE_ANY,
    S_TYPE_FUNC,
    S_TYPE_LIST,
    S_TYPE_UNKNOWN
} dataType;

/** Kinds of identifiers stored in the symbol table */
typedef enum
{
    TYPE_FUNCTION,
    TYPE_VARIABLE
} idType;

typedef struct astNode astNode;
typedef struct symtable symtable;
typedef struct nodeData nodeData;
typedef struct typeDesc typeDesc;


/**
 * @brief Get the return type descriptor of a function
 */
const typeDesc* symtableGetFuncReturnTypeDesc(symtable* table, const char* key);
/**
 * @brief Set the return type descriptor of a function
 */
int symtableSetFuncReturnTypeDesc(symtable* table, const char* key,
                                  const typeDesc* desc);
/**
 * @brief Get a function parameter type descriptor by index
 */
const typeDesc* symtableGetFuncParamTypeDesc(symtable* table, const char* key,
                                             int index);
/**
 * @brief Set a function parameter type descriptor by index
 */
int symtableSetFuncParamTypeDesc(symtable* table, const char* key, int index,
                                 const typeDesc* desc);
/**
 * @brief Get the type descriptor of a variable
 */
const typeDesc* symtableGetVarTypeDesc(symtable* table, const char* key);
/**
 * @brief Set the type descriptor of a variable
 */
int symtableSetVarTypeDesc(symtable* table, const char* key,
                           const typeDesc* desc);


/**
 * @brief Creates a symbol table context for a function
 */
symtable* symtableCreateFuncContext(symtable* globalTable);
/**
 * @brief Returns the number of function parameters
 */
int symtableGetFuncParamCount(symtable* table, const char* key);
/**
 * @brief Returns the type of a function parameter
 */
dataType symtableGetFuncParamType(symtable* table, const char* key, int index);
/**
 * @brief Returns an id (instance counter number) of variable from passed
 * variable name (key)
 */
size_t symtableGetVarId(symtable* symtable, const char* key);
/**
 * @brief Returns whether a function is being analyzed
 * (flag to identify recurion)
 */
bool symtableGetFuncIsAnalyzing(symtable* table, const char* key);
/**
 * @brief Sets whether a function is being analyzed (flag to identify recurion)
 */
void symtableSetFuncIsAnalyzing(symtable* table, const char* key, bool status);
/**
 * @brief Returns whether a function has been checked (flag to show that the
 * function was already analyzed)
 */
bool symtableGetFuncIsChecked(symtable* table, const char* key);
/**
 * @brief Sets whether a function has been checked (flag to show that the
 * function was already analyzed)
 */
void symtableSetFuncIsChecked(symtable* table, const char* key, bool status);
/**
 * @brief Returns the AST node of a function
 */
struct astNode* symtableGetFuncNode(symtable* table, const char* key);
/**
 * @brief Sets the return type of a function
 */
void symtableSetFuncReturn(symtable* table, const char* key,
                           dataType returnType);
/**
 * @brief returns dataType of a variable,
 * Searched through all scopes,
 * Returns S_TYPE_UNKNOWN if no variable was found
 */
dataType symtableGetVarType(symtable* symtable, const char* key);
/**
 * @brief returns whether passed variable is val or var
 */
bool symtableGetIsVarConst(symtable* symtable, const char* key);
/**
 * @brief returns data type that function under passed key returns
 */
dataType symtableGetFuncReturn(symtable* symtable, const char* key);

/** - SETTERS/GETTERS */

/**
 * @brief Opens new scope for local variables
 */
int stackPush(symtable* symtable);

/**
 * @brief Pops top scope and deletes all the variables inside
 */
void stackPop(symtable* symtable);

/**
 * @brief Inserts a variable into current scope (local tree), can give
 * redifinition feedback error code
 */
int stackInsertVar(symtable* symtable, const char* key, dataType type,
                   bool isConst);

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
int symtableBaseFuncInit(symtable* funcTable);

/**
 * @brief Inserts function into symtable,
 * can give redifinition feedback error code
 */
int symtableInsertFunc(symtable* symtable, const char* key, dataType returnType,
                       int paramCount, const dataType* paramType,
                       astNode* funcNode);

/**
 * @brief Initialize symtable
 */
int symtableInit(symtable** symtable);

/**
 * @brief Destructor for symtable
 */
void symtableDestroy(symtable* table);


#endif