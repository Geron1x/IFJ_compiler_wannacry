/**
 * @file operands.h
 * @brief Turn names, labels and values into IFJcode26
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#ifndef OPERANDS_H
#define OPERANDS_H

#include <stdbool.h>

#include "error.h"

/**
 * @brief Prints a piece of code to the output
 *
 * @param code The code to print
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
Errors tryPrint(const char* code);

/**
 * @brief Prints bool@true or bool@false to the output
 *
 * @param boolValue true or false
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
Errors writeLiteralBool(bool boolValue);

#endif
