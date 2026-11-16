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

#include "ast.h"

/**
 * @brief Parses the IFJ26 program read from stdin and builds its AST.
 *
 * @param out Receives the program tree on success, NULL on error.
 * @return ERROR_SUCCESS, ERROR_LEXICAL, ERROR_SYNTAX or ERROR_INTERNAL.
 */
int parse(astNode** out);


#endif