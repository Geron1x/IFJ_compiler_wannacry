/*
    Project name: Implementace překladače imperativního jazyka IFJ26
    Autor: xrusynm00   Rusyn Maksym,Heorhiy Haziy
*/

#include <stdio.h>

#include "ast.h"
#include "parser.h"

int main()
{
    // Parse the program and get its tree
    astNode* tree = NULL;
    int err = parse(&tree);

    // Free the tree, semantics and code generation will use it here later
    astDestroy(tree);
    return err;
}