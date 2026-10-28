// Project: Implementace překladače imperativního jazyka IFJ26C
// Author:
//Heorhiy Haziy (xhaziyh00)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scanner.h"

// GLOBAL VAR
Token current_token;
Scanner* global_scanner;

// read next token
void advance() {
    int err = getToken(&current_token, global_scanner);
    if (err != 0) {
        // error in lexer
        fprintf(stderr, "Lexical err (line %d)\n", current_token.line);
        exit(1);
    }
}

// function to check the type of token 
void match(Type expected_type) {
    if (current_token.type == expected_type) {
        advance();
    } else {
        fprintf(stderr, "Syntax err in line  %d: unpredicted token\n", current_token.line);
        exit(2);  
    }
}

// 1. parse prologue 
void parse_prologue() {
    // the start of program is import
    match(TYPE_IMPORT);

    // parse the correction of import name 
    if (current_token.type == TYPE_IDENTIFIER && strcmp(current_token.value.identifier, "ifj26") == 0) {
        advance();
    } else {
        fprintf(stderr, "Syntax err: the idintifier needed 'ifj26'\n");
        exit(2);
    }

    match(TYPE_DOT);
    match(TYPE_MUL);  
    match(TYPE_EOL); 
}


// 2. main function to parce 

void parse_program() {
    advance(); //first tokrn of the program

    // if empty lines exists => skip them
    while (current_token.type == TYPE_EOL) {
        advance();
    }

    parse_prologue();  

    while (current_token.type != TYPE_EOF) {
        // skip empty lines
        if (current_token.type == TYPE_EOL) {
            advance();
            continue;
        }

        // check for existion of annotation of @main
        if (current_token.type == TYPE_MAIN) {
            // the main must be def as @main
            // if the line starts by @main parse function (will be added in future)
            //parse_function
            advance();
        } else if (current_token.type == TYPE_DEF) {
            // if the line starts by def parse function (will be added in future)
            //parse_function
            advance();
        } else {
            fprintf(stderr, "Syntax err: function def expected (def)\n");
            exit(2);
            
        }
    }
}

// point of entry in kompiler
int parse() {
    Scanner scanner;
    scanner.stream = stdin;
    scanner.line = 1;

    global_scanner = &scanner;

    // start parsing
    parse_program();

    printf("Syntax analysis finished succesfully (code 0).\n");
    return 0;
}