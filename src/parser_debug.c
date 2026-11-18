// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#include "parser_debug.h"

#include <stdio.h>

#include "ast_internal.h"

// Names in the same order as the enums in ast.h and symtable.h
static const char* const nodeNames[] = {
    "PROG",  "FUNC",  "BLOCK",    "VAR",     "SET",
    "IF",    "WHILE", "RETURN",   "FOR",     "BINARY",
    "UNARY", "CALL",  "VAR_EXPR", "LITERAL", "IF_EXPR"};
static const char* const opNames[] = {
    "+",  "-",  "*",   "/",   "==", "!=", "<", ">",
    "<=", ">=", "neg", "pos", "!",  "&&", "||"};
static const char* const typeNames[] = {"Unit",    "Int", "Double", "String",
                                        "Boolean", "Any", "Unknown"};

// Take a name from a table, unknown value gives ?
static const char* nameOf(const char* const* names, size_t count, int value)
{
    if (value < 0 || (size_t)value >= count) return "?";
    return names[value];
}

#define NAME(table, value) \
    nameOf(table, sizeof(table) / sizeof(table[0]), (int)(value))

// Print the value of a literal by its type
static void dumpLiteral(const astNode* node)
{
    switch (node->retType)
    {
        case S_TYPE_INT:
            fprintf(stderr, " %d", node->data.intVal);
            break;
        case S_TYPE_DOUBLE:
            fprintf(stderr, " %g", node->data.doubleVal);
            break;
        case S_TYPE_STRING:
            fprintf(stderr, " \"%s\"", node->data.strVal);
            break;
        case S_TYPE_BOOL:
            fprintf(stderr, " %s", node->data.boolVal ? "true" : "false");
            break;
        default:
            fprintf(stderr, " ()");
            break;
    }
}

// Print names, operators and values stored in the node
static void dumpDetails(const astNode* node)
{
    switch (node->nodeType)
    {
        case AST_FUNC:
            fprintf(stderr, " %s%s", node->data.funcData.key,
                    node->data.funcData.isMain ? " @main" : "");
            break;
        case AST_EXPR_CALL:
            fprintf(stderr, " %s", node->data.funcData.key);
            break;
        case AST_STAT_VAR:
            fprintf(stderr, " %s %s",
                    node->data.varData.isConst ? "val" : "var",
                    node->data.varData.key);
            break;
        case AST_STAT_SET:
        case AST_EXPR_VAR:
            fprintf(stderr, " %s", node->data.varData.key);
            break;
        case AST_EXPR_BINARY:
        case AST_EXPR_UNARY:
            fprintf(stderr, " %s", NAME(opNames, node->data.opType));
            break;
        case AST_EXPR_LITERAL:
            dumpLiteral(node);
            break;
        default:
            break;
    }
}

// Print one node and then all its children one level deeper
void astDump(const astNode* node, int depth)
{
    // Missing child is shown so the bug is visible
    if (node == NULL)
    {
        fprintf(stderr, "%*s<null>\n", depth * 2, "");
        return;
    }

    // Indent, node name, details and result type
    fprintf(stderr, "%*s%s", depth * 2, "", NAME(nodeNames, node->nodeType));
    dumpDetails(node);
    fprintf(stderr, " : %s\n", NAME(typeNames, node->retType));

    for (int i = 0; i < node->childCount; i++)
    {
        astDump(node->children[i], depth + 1);
    }
}
