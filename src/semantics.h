/**
 * @file semantics.h
 * @brief Header file for the implementaion of semantic analysis
 *
 * Project: Implementace překladače imperativního jazyka IFJ26
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */

#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include "error.h"
#include "symtable.h"
/**
 * @brief check the semantics of created ast and decide which types should be
 * placed instead of incomplete types
 */
Errors semanticsCheck(const astNode* prog);

#endif
