/**
 * @file ifjcode_lib.h
 * @brief Built-in functions of IFJ26 in IFJcode26
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Maksym Rusyn (xrusynm00)
 *
 */

#ifndef IFJCODE_LIB_H
#define IFJCODE_LIB_H

/**
 * @brief Prints the start of a built-in function
 *
 * @param name
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeLibraryStart(const char* name);

/**
 * @brief Prints the end of a built-in function
 *
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeLibraryEnd(void);

/**
 * @brief Prints all built-in functions
 *
 * @return ERROR_SUCCESS if everything worked, otherwise ERROR_INTERNAL
 */
int writeLibrary(void);

#endif
