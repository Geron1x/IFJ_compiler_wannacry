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
#include "ifjcode_expr.h"

/**
 * @brief Prints IFJcode26
 *
 * @param program
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int generateIfjcode(const astNode* program);

/**
 * @brief Prints the whole if
 *
 * @param ifNode
 * @param context
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int generateIf(const astNode* ifNode, codegenContext* context);

/**
 * @brief Prints the lambda and pushes its label as a string
 *
 * @param lambda
 * @param context
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int generateLambda(const astNode* lambda, codegenContext* context);

#endif
