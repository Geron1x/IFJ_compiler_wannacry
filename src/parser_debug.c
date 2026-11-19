// Project: Implementace překladače imperativního jazyka IFJ26
// Author:
// Heorhiy Haziy (xhaziyh00)
#include "parser_debug.h"

#include <stdio.h>

#include "ast_internal.h"

// Names in the same order as the enums in ast.h and symtable.h
static const char* const nodeNames[] = {
    "PROG",    "FUNC",   "BLOCK",  "VAR",    "SET",   "IF",        "WHILE",
    "RETURN",  "FOR",    "BINARY", "UNARY",  "CALL",  "VAR_EXPR",  "LITERAL",
    "IF_EXPR", "LAMBDA", "LIST",   "MEMBER", "RANGE", "CALL_EXPR", "FOR_EACH"};
static const char* const opNames[] = {
    "+",  "-",  "*",   "/",   "==", "!=", "<",  ">",
    "<=", ">=", "neg", "pos", "!",  "&&", "||", "++"};
static const char* const typeNames[] = {"Unit",   "Int",     "Double",
                                        "String", "Boolean", "Any",
                                        "Func",   "List",    "Unknown"};

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

// Print a full type, lists and function types show their inner types
static void dumpType(const typeDesc* desc)
{
    // Missing inner type is shown so the bug is visible
    if (desc == NULL)
    {
        fprintf(stderr, "?");
        return;
    }

    // List prints its element type in brackets
    if (desc->type == S_TYPE_LIST)
    {
        fprintf(stderr, "List[");
        dumpType(desc->elemType);
        fprintf(stderr, "]");
        return;
    }

    // Function prints parameter types and then the result type
    if (desc->type == S_TYPE_FUNC)
    {
        fprintf(stderr, "(");
        for (int i = 0; i < desc->func.paramCount; i++)
        {
            if (i > 0) fprintf(stderr, ", ");
            dumpType(desc->func.paramTypes[i]);
        }
        fprintf(stderr, ") => ");
        dumpType(desc->func.returnType);
        return;
    }

    // Simple type prints only its name
    fprintf(stderr, "%s", NAME(typeNames, desc->type));
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
        case AST_STAT_FOR:
            fprintf(stderr, " %s %s", node->data.forData.key,
                    node->data.forData.isInclusive ? "to" : "until");
            break;
        case AST_STAT_FOR_EACH:
            fprintf(stderr, " %s", node->data.forData.key);
            break;
        case AST_EXPR_MEMBER:
            fprintf(stderr, " .%s%s", node->data.memberData.key,
                    node->data.memberData.hasArgs ? "()" : "");
            break;
        case AST_EXPR_RANGE:
            fprintf(stderr, " %s",
                    node->data.rangeData.isInclusive ? "to" : "until");
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

    // Full type if the node has one, otherwise the simple type name
    fprintf(stderr, " : ");
    if (node->typeInfo != NULL)
        dumpType(node->typeInfo);
    else
        fprintf(stderr, "%s", NAME(typeNames, node->retType));
    fprintf(stderr, "\n");

    for (int i = 0; i < node->childCount; i++)
    {
        astDump(node->children[i], depth + 1);
    }
}
