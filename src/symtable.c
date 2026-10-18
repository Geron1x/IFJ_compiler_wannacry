/**
 * @file symtable.c
 * @brief File for the symtable implementation
 * 
 * Project: Implementace překladače imperativního jazyka IFJ2026
 * 
 * @author Arkhyp Samoilenko (xsamoia00)
 * 
 */

#include "symtable.h"
#include <stdlib.h>

void EraseData(nodeData* data)
{
    if(data == NULL)
    {
        return;
    }
    if(data->type == TYPE_FUNCTION)
    {
        free(data->funcData.paramType);
    }
    free(data);
}
