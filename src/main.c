/*
    Project name: Implementace překladače imperativního jazyka IFJ26
    Autor: xrusynm00   Rusyn Maksym,Heorhiy Haziy
*/

#include <stdio.h>

#include "ast.h"
#include "parser.h"
#include "semantics.h"

int main(void)
{
    // Parse the program and get its tree
    astNode* tree = NULL;
    int err = parse(&tree);

    // Check names and types only when the tree has no syntax errors
    if (err == ERROR_SUCCESS)
    {
        err = semanticsCheck(tree);
    }

    // Free the tree, code generation will use it here later
    astDestroy(tree);
    return err;
}