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
 * @return ERROR_SUCCESS if everything worked, in the other way ERROR_INTERNAL
 */
Errors tryPrint(const char* code);

#endif
