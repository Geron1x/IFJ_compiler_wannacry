//Project: Implementace překladače imperativního jazyka IFJ26
//Authors:
//Artem Antonenko (xantona00)

#ifndef IFJ_SCANNER_H
#define IFJ_SCANNER_H

#include "error.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>


typedef union {
    char* identifier;
    char* string;
    double numberFloat;
    long long int numberInt;
} Val;

typedef enum {
    TYPE_DEF,
    TYPE_DO,
    TYPE_DOUBLE,
    TYPE_ELSE,
    TYPE_IF,
    TYPE_IMPORT,
    TYPE_INT,
    TYPE_RETURN,
    TYPE_STRING,
    TYPE_THEN,
    TYPE_UNIT,
    TYPE_VAL,
    TYPE_VAR,
    TYPE_WHILE,
    TYPE_ADD,
    TYPE_SUB,
    TYPE_MUL,
    TYPE_DIV,
    TYPE_LESSER,
    TYPE_LESSER_EQUAL,
    TYPE_EQUAL,
    TYPE_BIGGER_EQUAL,
    TYPE_BIGGER,
    TYPE_NOT_EQUAL,
    TYPE_ASSIGNMENT,
    TYPE_LEFT_PAREN,
    TYPE_RIGHT_PAREN,
    TYPE_DOUBLE_DOT,
    TYPE_COMMA,
    TYPE_SEMICOLON,
    TYPE_DOT,
    TYPE_MAIN,
    TYPE_EOL,
    TYPE_EOF,
    TYPE_INDENT,
    TYPE_DEDENT,
    TYPE_ERROR,
    TYPE_IDENTIFIER,
    TYPE_STRING_LITERAL,
    TYPE_INT_LITERAL,
    TYPE_DOUBLE_LITERAL,
    TYPE_UNIT_LITERAL,
    TYPE_EXCLAMATION
} Type;


typedef struct {
    Val value;
    Type type;
    int line;
} Token;

typedef struct {
    int line;
    int firstRead;
    FILE* stream;
} Scanner;

int getToken(Token* token, Scanner* scanner);

#endif //IFJ_SCANNER_H 