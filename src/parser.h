/**
 * @file parser.h
 * @brief Syntax analyzer interface for the IFJ26 compiler
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Heorhiy Haziy (xhaziyh00)
 *
 */

#ifndef PARSER_H
#define PARSER_H


/**
 * @brief Checks syntax of the IFJ26 program read from stdin.
 *
 * @return ERROR_SUCCESS, ERROR_LEXICAL, ERROR_SYNTAX or ERROR_INTERNAL.
 */
int parse(void);


#endif