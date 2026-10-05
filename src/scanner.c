#include "scanner.h"

typedef enum {
    STATE_START,
    STATE_IDENTIFIER,
    STATE_INT_LITERAL,
    STATE_SLASH,
    STATE_LINE_COMMENT,
    STATE_BLOCK_COMMENT,
    STATE_LESSER,
    STATE_BIGGER,
    STATE_ASSIGNMENT,
    STATE_EXCLAMATION
} State;

int getToken(Token* token, Scanner* scanner) {
    State state = STATE_START;
    token->line = scanner->line;
    int commentLevel = 0;

    while(true) {
        int c = fgetc(scanner->stream);
        switch (state) {
            case STATE_START:
                if(c == ' ') {
                    continue;
                }

                if(c == '\t') {
                    token->type = TYPE_ERROR;
                    return 1;
                }

                if(c == '\n') {
                    scanner->line++;
                    token->line = scanner->line;
                    token->type = TYPE_EOL;
                    return 0;
                }

                if(c == '(') {
                    token->type = TYPE_LEFT_PAREN;
                    return 0;
                }

                if(c == ')') {
                    token->type = TYPE_RIGHT_PAREN;
                    return 0;
                }

                if(c == ':') {
                    token->type = TYPE_DOUBLE_DOT;
                    return 0;
                }

                if(c == ',') {
                    token->type = TYPE_COMMA;
                    return 0;
                }

                if(c == ';') {
                    token->type = TYPE_SEMICOLON;
                    return 0;
                }

                if(c == '-') {
                    token->type = TYPE_SUB;
                    return 0;
                }

                if(c == '+') {
                    token->type = TYPE_ADD;
                    return 0;
                }

                if(c == '*') {
                    token->type = TYPE_MUL;
                    return 0;
                }

                if (c == '/') {
                    state = STATE_SLASH;
                    break;
                }

                if(c == '=') {
                    state = STATE_ASSIGNMENT;
                    break;
                }

                if(c == '<') {
                    state = STATE_LESSER;
                    break;
                }

                if(c == '>') {
                    state = STATE_BIGGER;
                    break;
                }

                if(c == '!') {
                    state = STATE_EXCLAMATION;
                    break;
                }

                if(isdigit(c)) {
                    token->value.numberInt = c - '0';
                    state = STATE_INT_LITERAL;
                    break;
                }

                if (c == EOF) {
                    token->type = TYPE_EOF;
                    return 0;
                }

                break;



            case STATE_ASSIGNMENT:
                if (c == '=') {
                        token->type = TYPE_EQUAL;
                        return 0;
                    }
                    ungetc(c, scanner->stream);
                    token->type = TYPE_ASSIGNMENT;
                    return 0;

            case STATE_LESSER:
                if (c == '=') {
                        token->type = TYPE_LESSER_EQUAL;
                        return 0;
                    }
                    ungetc(c, scanner->stream);
                    token->type = TYPE_LESSER;
                    return 0;

            case STATE_BIGGER:
                if (c == '=') {
                        token->type = TYPE_BIGGER_EQUAL;
                        return 0;
                    }
                    ungetc(c, scanner->stream);
                    token->type = TYPE_BIGGER;
                    return 0;

            case STATE_EXCLAMATION:
                if (c == '=') {
                        token->type = TYPE_NOT_EQUAL;
                        return 0;
                    }
                    ungetc(c, scanner->stream);
                    token->type = TYPE_ERROR;
                    return 1;

            case STATE_SLASH:
                if(c =='/') {
                    state = STATE_LINE_COMMENT;
                    break;
                }
                if(c =='*') {
                    commentLevel = 1;
                    state = STATE_BLOCK_COMMENT;
                    break;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_DIV;
                return 0;

            case STATE_BLOCK_COMMENT:
                if(c == EOF) {
                    token->type = TYPE_ERROR;
                    return 1;
                }

                if (c == '\n') {
                    scanner->line++;
                    break;
                }

                if(c == '/') {
                    int next = fgetc(scanner->stream);
                    if(next == '*') {
                        commentLevel++;
                    } else { ungetc(next, scanner->stream);
                    }
                    break;
                }

                if(c =='*') {
                    int next = fgetc(scanner->stream);
                    if(next == '/') {
                        commentLevel--;
                        if(commentLevel ==0) {
                            state = STATE_START;
                        }
                    } else {
                        ungetc(next, scanner->stream);
                    }
                }

                break;

            case STATE_LINE_COMMENT:
                if (c == '\n') {
                    scanner->line++;
                    token->line = scanner->line;
                    token->type = TYPE_EOL;
                    return 0;
                }

                if(c == EOF) {
                    token->type = TYPE_EOF;
                    return 0;
                }
                break;

            case STATE_INT_LITERAL:

                if(isdigit(c)) {
                    token->value.numberInt = token->value.numberInt * 10 + (c - '0');
                    break;
                }

                ungetc(c, scanner->stream);
                token->type = TYPE_INT_LITERAL;
                return 0;

        }
    }
    return 0;
}

int main() {

    return 0;
}