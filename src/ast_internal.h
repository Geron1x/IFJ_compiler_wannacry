/**
 * @file ast_internal.h
 * @brief Header file to define astNode structre
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */

#ifndef AST_INTERNAL_H
#define AST_INTERNAL_H

#include "ast.h"

/** type descriptor */
typedef struct typeDesc
{
    dataType type; /** identifier for lists/lambdas/primitive descriptors */
    union
    {
        /** if type is a list make elemType the type of elements inside of the
         * list */
        struct typeDesc* elemType;

        /** if type is a function, pass according values */
        struct
        {
            int paramCount;
            struct typeDesc** paramTypes; /**< array of descriptor for the
                                             parameters of the function */
            struct typeDesc* returnType;  /**< descriptor of return type */
        } func;
    };
} typeDesc;

struct astNode
{
    astNodeType nodeType;
    dataType retType; /**< what type of data will expression return*/

    struct astNode** children; /**< array to store descendent nodes */
    typeDesc* typeInfo;
    int childCount;    /**< number of occupied slots in children array */
    int childCapacity; /**< max amount of children to store without expanding */

    bool int2float;
    union
    {
        int intVal; /**< value for literal node int !!CHANGE TO LONG LONG INT!!
                     */
        double doubleVal; /**< value for literal node double */
        char* strVal;     /**< value for literal node string */
        bool boolVal;     /**< value for literal node string */
        astOpType opType; /**< operator type if node is an operation */

        struct
        {
            size_t id;
            char* key; /**< variable name */
            bool isConst;
        } varData;

        struct
        {
            size_t id;
            char* key; /**< function name */
            bool isMain;
        } funcData;

        struct
        {
            size_t id;
            int intVal;
            char* key;
            bool isInclusive;
            bool hasStep;
        } forData;
        struct
        {
            char* key;
            bool hasArgs;
        } memberData;
        struct
        {
            bool isInclusive;
            bool hasStep;
        } rangeData;
    } data;
};

#endif