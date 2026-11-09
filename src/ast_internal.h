#ifndef AST_INTERNAL_H
#define AST_INTERNAL_H

#include "ast.h"

struct astNode
{
    astNodeType nodeType;
    dataType retType; /**< what type of data will expression return*/

    struct astNode** children; /**< array to store descendent nodes */

    int childCount;    /**< number of occupied slots in children array */
    int childCapacity; /**< max amount of children to store without expanding */

    union
    {
        int intVal;       /**< value for literal node int */
        double doubleVal; /**< value for literal node double */
        char* strVal;     /**< value for literal node string */
        bool boolVal;     /**< value for literal node string */
        astOpType opType; /**< operator type if node is an operation */

        struct
        {
            char* key; /**< variable name */
            bool isConst;
        } varData;

        struct
        {
            char* key; /**< function name */
        } funcData;
    } data;
};

#endif