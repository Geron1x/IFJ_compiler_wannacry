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
#include <stdio.h>

#include "error.h"

/**
 * @brief Prints a piece of code to the output
 *
 * @param code The code to print
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int tryPrint(const char* code);

/**
 * @brief Prints bool@true or bool@false to the output
 *
 * @param boolValue true or false
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeLiteralBool(bool boolValue);

/**
 * @brief Prints int@ and the number to the output
 *
 * @param intValue
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeLiteralInt(long long intValue);

/**
 * @brief Prints nil@nil to the output
 *
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeLiteralNil(void);

/**
 * @brief Prints float@ and number to the output
 *
 * @param floatValue
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeLiteralFloat(double floatValue);

/**
 * @brief Prints string@ and chars to the output
 *
 * @param stringValue
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeLiteralString(const char* stringValue);

/**
 * @brief Prints a variable name
 *
 * @param name
 * @param number
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeVarName(const char* name, size_t number);

/**
 * @brief Prints function label
 *
 * @param name
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeFuncLabel(const char* name);

/**
 * @brief Prints name of temporary variable
 *
 * @param number
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeTempVar(size_t number);

#endif
