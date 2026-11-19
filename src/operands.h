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
 * @brief Leaves the function with the error if the call failed
 */
#define RETURN_IF_ERROR(call)             \
    do                                    \
    {                                     \
        int macroResult = (call);         \
        if (macroResult != ERROR_SUCCESS) \
        {                                 \
            return macroResult;           \
        }                                 \
    } while (0)

/**
 * @brief Types of labels we jump to
 */
typedef enum
{
    /* Base */
    LABEL_IF_ELSE,     /**< Start of else, false if jumps here */
    LABEL_IF_END,      /**< After the whole if, then part jumps here */
    LABEL_WHILE_START, /**< Before the check, loop comes back here */
    LABEL_WHILE_END,   /**< After the loop, false check jumps here */

    /* Boolthen */
    LABEL_AND_FALSE, /**< Left side of && was false, so result is false */
    LABEL_AND_END,   /**< After &&, right side jumps here */
    LABEL_OR_TRUE,   /**< Left side of || was true, so result is true */
    LABEL_OR_END,    /**< After ||, right side jumps here */

    /* Control */
    LABEL_FOR_CHECK, /**< Before the check, for comes back here */
    LABEL_FOR_NEXT,  /**< Go to the next number, failed guard jumps here */
    LABEL_FOR_END    /**< After the loop, false check jumps here */
} jumpLabelType;

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

/**
 * @brief Prints instruction with temp variable
 *
 * @param instruction
 * @param number
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeTempInstruction(const char* instruction, size_t number);

/**
 * @brief Prints instruction and variable
 *
 * @param instruction
 * @param name
 * @param number
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeVarInstruction(const char* instruction, const char* name,
                        size_t number);

/**
 * @brief Prints a label we can jump to
 *
 * @param type
 * @param number
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeJumpLabel(jumpLabelType type, size_t number);

/**
 * @brief Jumps to label if the value on the stack is false
 *
 * @param type
 * @param number
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeJumpIfFalse(jumpLabelType type, size_t number);

#endif
