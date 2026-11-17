/*
    Project name: Implementace překladače imperativního jazyka IFJ26
    Autor: xrusynm00   Rusyn Maksym,Heorhiy Haziy
*/

#include <stdio.h>

#include "ast.h"
#include "ifjcode.h"
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

    // Generate IFJcode26
    if (err == ERROR_SUCCESS)
    {
        err = generateIfjcode(tree);
    }

    // Free the tree
    astDestroy(tree);
    return err;
}
