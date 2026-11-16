/**
 * @file parser_debug.h
 * @brief Debug printing of the AST built by the parser
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Heorhiy Haziy (xhaziyh00)
 *
 */

#ifndef PARSER_DEBUG_H
#define PARSER_DEBUG_H

#include "ast.h"

/**
 * @brief Prints the tree to stderr, one node per line, children indented.
 *
 * @param node Root of the tree to print, NULL prints a <null> line.
 * @param depth Indentation level of the node, 0 for the root.
 */
void astDump(const astNode* node, int depth);

#endif
