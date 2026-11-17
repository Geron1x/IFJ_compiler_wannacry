/**
 * @file ifjcode.h
 * @brief Walks the tree and prints IFJcode26
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#ifndef IFJCODE_H
#define IFJCODE_H

#include "ast.h"

/**
 * @brief Prints IFJcode26
 *
 * @param program
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int generateIfjcode(const astNode* program);

#endif
