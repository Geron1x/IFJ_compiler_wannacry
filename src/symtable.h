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

typedef enum
{
    TYPE_UNIT,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOL
} dataType;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_VARIABLE
} idType;

typedef struct variabledata
{
    dataType type;
    bool isConst;
    bool isInit;
    int instanceNum;
} variableData;

typedef struct functiondata
{
    int paramCount;
    dataType* paramType;
    dataType returnType;
    bool isDefined;

} functionData;

typedef struct nodeData
{
    idType type;

    union
    {
        functionData funcData;
        variableData varData;
    };
} nodeData;


void eraseData(nodeData* data);

#endif