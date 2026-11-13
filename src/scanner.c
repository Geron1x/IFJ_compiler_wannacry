// Project: Implementace překladače imperativního jazyka IFJ26
// Authors:
// Artem Antonenko (xantona00)

#include "scanner.h"

// define states for finite automaton
typedef enum
{
    STATE_START,
    STATE_IDENTIFIER,
    STATE_INT_LITERAL,
    STATE_SLASH,
    STATE_LINE_COMMENT,
    STATE_BLOCK_COMMENT,
    STATE_LESSER,
    STATE_BIGGER,
    STATE_ASSIGNMENT,
    STATE_EXCLAMATION,
    STATE_ZERO,
    STATE_INT_UNDERSCORE,
    STATE_DOT,
    STATE_DOUBLE_LITERAL,
    STATE_DOUBLE_UNDERSCORE,
    STATE_EXP,
    STATE_EXP_SIGN,
    STATE_EXP_LITERAL,
    STATE_EXP_UNDERSCORE,
    STATE_STRING,
    STATE_STRING_ESC,
    STATE_MULTILINE_STRING,
    STATE_INTERP_SINGLE,
    STATE_INTERP_ESC,
    STATE_INTERP_MULTI,
    STATE_INTERP_ID
} State;

// dynamic string to save lexeme
typedef struct
{
    char* data;
    int len;
    int capacity;
} DynamicString;

#define STR_TRY(call)       \
    do                      \
    {                       \
        int _err = (call);  \
        if (_err != 0)      \
        {                   \
            strFree(&attr); \
            return _err;    \
        }                   \
    } while (0)

#define ALLOC_TOKEN(target, type, size)         \
    do                                          \
    {                                           \
        (target) = (type)malloc(size);          \
        if ((target) == NULL)                   \
        {                                       \
            fprintf(stderr, "Out of memory\n"); \
            strFree(&attr);                     \
            return 99;                          \
        }                                       \
    } while (0)

// init empty string for lexeme
int strInit(DynamicString* s)
{
    s->capacity = 16;
    s->len = 0;
    s->data = (char*)malloc(s->capacity * sizeof(char));
    if (s->data == NULL)
    {
        fprintf(stderr, "Out of memory");
        return 99;
    }
    s->data[0] = '\0';
    return 0;
}

// free string memory
void strFree(DynamicString* s)
{
    if (s->data != NULL)
    {
        free(s->data);
        s->data = NULL;
    }
    s->len = 0;
    s->capacity = 0;
}

// add char to lexeme string and resize buffer
int strAppend(DynamicString* s, char c)
{
    if (s->len + 1 >= s->capacity)
    {
        s->capacity *= 2;

        char* newData = (char*)realloc(s->data, s->capacity * sizeof(char));
        if (newData == NULL)
        {
            fprintf(stderr, "Out of memory in realloc!\n");
            return 99;
        }
        s->data = newData;
    }
    s->data[s->len++] = c;
    s->data[s->len] = '\0';
    return 0;
}

// struct for keyword and token mapping
typedef struct
{
    char* keyword;
    Type type;
} Keyword;

// list of reserved keywords
Keyword Keywords[] = {{"def", TYPE_DEF},
                      {"do", TYPE_DO},
                      {"Double", TYPE_DOUBLE},
                      {"else", TYPE_ELSE},
                      {"if", TYPE_IF},
                      {"import", TYPE_IMPORT},
                      {"Int", TYPE_INT},
                      {"return", TYPE_RETURN},
                      {"String", TYPE_STRING},
                      {"then", TYPE_THEN},
                      {"Unit", TYPE_UNIT},
                      {"val", TYPE_VAL},
                      {"var", TYPE_VAR},
                      {"while", TYPE_WHILE},
                      {"Boolean", TYPE_BOOLEAN},
                      {"true", TYPE_TRUE},
                      {"false", TYPE_FALSE},
                      {"for", TYPE_FOR},
                      {"List", TYPE_LIST},
                      {"case", TYPE_CASE},
                      {"class", TYPE_CLASS},
                      {"extension", TYPE_EXTENSION},
                      {"match", TYPE_MATCH},
                      {"type", TYPE_TYPE},
                      {NULL, 0}};

// initialize scanner state and bind input stream
void scannerInit(Scanner* scanner, FILE* stream)
{
    scanner->line = 1;
    scanner->firstRead = 1;
    scanner->stream = stream;

    scanner->indentStack[0] = 0;  // base indent level is zero spaces
    scanner->indentTop = 0;
    scanner->pendingDedents = 0;
    scanner->atLineStart = true;
    scanner->parenLevel = 0;
    scanner->lfSeen = false;

    scanner->modeStack[0] = MODE_DEFAULT;
    scanner->modeTop = 0;
}

// read next token from input stream
int getToken(Token* token, Scanner* scanner)
{
    State state = STATE_START;  // set start state of automaton

    if (scanner->modeStack[scanner->modeTop] == MODE_INTERP_SINGLE)
    {
        state = STATE_INTERP_SINGLE;
    }
    else if (scanner->modeStack[scanner->modeTop] == MODE_INTERP_MULTI)
    {
        state = STATE_INTERP_MULTI;
    }

    token->line = scanner->line;  // save line number for token
    bool hasNewLine = false;      // flag to track newline in comments
    token->lfBefore =
        scanner->lfSeen;      // save flag indicating preceding newline
    scanner->lfSeen = false;  // reset newline flag for next token

    // emit pending dedent tokens first
    if (scanner->pendingDedents > 0)
    {
        scanner->pendingDedents--;
        token->type = TYPE_DEDENT;
        return 0;
    }

    // check indent logic at start of line
    while (scanner->atLineStart &&
           scanner->modeStack[scanner->modeTop] == MODE_DEFAULT)
    {
        // reset space counter
        int spaces = 0;
        int c;

        // count spaces to find indent level
        while ((c = fgetc(scanner->stream)) == ' ')
        {
            spaces++;
        }

        // skip empty line
        if (c == '\n')
        {
            scanner->line++;
            continue;
        }

        // handle carriage return symbol
        if (c == '\r')
        {
            int next = fgetc(scanner->stream);
            if (next == '\n')
            {
                scanner->line++;
            }
            else
            {
                ungetc(next, scanner->stream);
            }
            continue;
        }

        // tab causes lexical error
        if (c == '\t')
        {
            token->type = TYPE_ERROR;
            return 1;
        }

        // check start of line comment
        if (c == '/')
        {
            int next = fgetc(scanner->stream);
            if (next == '/')
            {
                while ((c = fgetc(scanner->stream)) != '\n' && c != EOF)
                {
                }
                if (c == '\n') scanner->line++;
                continue;
            }
            else if (next == '*')
            {
                int commentLevel = 1;
                bool hasNewlineInside = false;
                while (commentLevel > 0)
                {
                    c = fgetc(scanner->stream);
                    if (c == EOF)
                    {
                        token->type = TYPE_ERROR;
                        return 1;
                    }
                    if (c == '\n')
                    {
                        scanner->line++;
                        hasNewlineInside = true;
                    }
                    else if (c == '/')
                    {
                        if ((next = fgetc(scanner->stream)) == '*')
                            commentLevel++;
                        else
                            ungetc(next, scanner->stream);
                    }
                    else if (c == '*')
                    {
                        if ((next = fgetc(scanner->stream)) == '/')
                            commentLevel--;
                        else
                            ungetc(next, scanner->stream);
                    }
                }
                while ((c = fgetc(scanner->stream)) == ' ')
                {
                }
                if (hasNewlineInside && c != '\n' && c != EOF)
                {
                    token->type = TYPE_ERROR;
                    return 2;
                }
            }
            else
            {
                ungetc(next, scanner->stream);
            }
        }

        // handle eof token generation
        if (c == EOF)
        {
            if (scanner->indentTop > 0)
            {
                scanner->indentTop--;
                token->type = TYPE_DEDENT;
                return 0;
            }
            token->type = TYPE_EOF;
            return 0;
        }

        // skip empty line before indent check
        if (c == '\n')
        {
            scanner->line++;
            continue;
        }

        ungetc(c, scanner->stream);
        scanner->atLineStart = false;

        // check for indent token generation
        if (spaces > scanner->indentStack[scanner->indentTop])
        {
            scanner->indentTop++;
            scanner->indentStack[scanner->indentTop] = spaces;
            token->type = TYPE_INDENT;
            return 0;
        }

        // check for dedent token generation
        if (spaces < scanner->indentStack[scanner->indentTop])
        {
            int targetIndex = -1;
            for (int i = scanner->indentTop - 1; i >= 0; i--)
            {
                if (scanner->indentStack[i] == spaces)
                {
                    targetIndex = i;
                    break;
                }
            }
            // syntax error if indent level does not match stack
            if (targetIndex == -1)
            {
                token->type = TYPE_ERROR;
                return 2;
            }

            // count how many dedents to emit
            int dedentCount = scanner->indentTop - targetIndex;
            scanner->indentTop = targetIndex;

            // save extra dedents for subsequent tokens
            scanner->pendingDedents = dedentCount - 1;
            token->type = TYPE_DEDENT;
            return 0;
        }

        break;
    }

    int commentLevel = 0;  // counter for nested block comments
    DynamicString attr = {.data = NULL, .len = 0, .capacity = 0};

    // main loop of finite automaton
    while (true)
    {
        int c = fgetc(scanner->stream);  // read next character from
        // check automaton state
        switch (state)
        {
            case STATE_START:
                // ignore basic whitespace symbols
                if (c == ' ' || c == '\t')
                {
                    continue;
                }

                token->line = scanner->line;

                // handle end of line symbol
                if (c == '\n')
                {
                    scanner->line++;
                    if (scanner->parenLevel > 0)
                    {
                        scanner->lfSeen = true;
                        continue;
                    }
                    token->line = scanner->line;
                    scanner->atLineStart = true;
                    token->type = TYPE_EOL;
                    return 0;
                }

                // handle right paren token
                if (c == ')')
                {
                    if (scanner->parenLevel > 0)
                    {
                        scanner->parenLevel--;
                    }
                    token->type = TYPE_RIGHT_PAREN;
                    return 0;
                }

                if (c == ':')
                {
                    token->type = TYPE_DOUBLE_DOT;
                    return 0;
                }

                if (c == ',')
                {
                    token->type = TYPE_COMMA;
                    return 0;
                }

                if (c == ';')
                {
                    token->type = TYPE_SEMICOLON;
                    return 0;
                }

                if (c == '-')
                {
                    token->type = TYPE_SUB;
                    return 0;
                }

                if (c == '[')
                {
                    scanner->parenLevel++;
                    token->type = TYPE_LEFT_BRACKET;
                    return 0;
                }

                if (c == ']')
                {
                    if (scanner->parenLevel > 0)
                    {
                        scanner->parenLevel--;
                    }
                    token->type = TYPE_RIGHT_BRACKET;
                    return 0;
                }

                // handle right brace returning from interpolation expression
                if (c == '}')
                {
                    if (scanner->modeTop > 0 &&
                        scanner->modeStack[scanner->modeTop] == MODE_DEFAULT)
                    {
                        scanner
                            ->modeTop--;  // pop MODE_DEFAULT, return to string
                        token->type = TYPE_RIGHT_BRACE;
                        return 0;
                    }
                    token->type = TYPE_ERROR;
                    return 1;
                }

                if (c == '&')
                {
                    int next = fgetc(scanner->stream);
                    if (next == '&')
                    {
                        token->type = TYPE_AND;
                        return 0;
                    }
                    ungetc(next, scanner->stream);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                if (c == '|')
                {
                    int next = fgetc(scanner->stream);
                    if (next == '|')
                    {
                        token->type = TYPE_OR;
                        return 0;
                    }
                    ungetc(next, scanner->stream);
                    token->type = TYPE_ERROR;
                    return 1;
                }


                if (c == '+')
                {
                    int next = fgetc(scanner->stream);
                    if (next == '+')
                    {
                        token->type = TYPE_CONCAT;
                        return 0;
                    }
                    ungetc(next, scanner->stream);
                    token->type = TYPE_ADD;
                    return 0;
                }

                if (c == '*')
                {
                    token->type = TYPE_MUL;
                    return 0;
                }

                if (c == '/')
                {
                    state = STATE_SLASH;
                    break;
                }

                if (c == '=')
                {
                    state = STATE_ASSIGNMENT;
                    break;
                }

                if (c == '<')
                {
                    state = STATE_LESSER;
                    break;
                }

                if (c == '>')
                {
                    state = STATE_BIGGER;
                    break;
                }

                if (c == '!')
                {
                    state = STATE_EXCLAMATION;
                    break;
                }

                if (c == '0')
                {
                    STR_TRY(strInit(&attr));
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_ZERO;
                    break;
                }

                if (c == '"')
                {
                    int c2 = fgetc(scanner->stream);
                    if (c2 == '"')
                    {
                        int c3 = fgetc(scanner->stream);
                        // check for multiline string literal
                        if (c3 == '"')
                        {
                            STR_TRY(strInit(&attr));
                            state = STATE_MULTILINE_STRING;
                            break;
                        }
                        else
                        {
                            // return empty string literal token
                            ungetc(c3, scanner->stream);
                            token->type = TYPE_STRING_LITERAL;
                            ALLOC_TOKEN(token->value.string, char*, 1);
                            token->value.string[0] = '\0';
                            return 0;
                        }
                    }
                    // fallback to single line string state
                    ungetc(c2, scanner->stream);
                    STR_TRY(strInit(&attr));
                    state = STATE_STRING;
                    break;
                }

                // start int literal parse
                if (isdigit(c))
                {
                    STR_TRY(strInit(&attr));
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_INT_LITERAL;
                    break;
                }

                // check for interpolated string prefix
                if (c == 's')
                {
                    int next1 = fgetc(scanner->stream);
                    if (next1 == '"')
                    {
                        int next2 = fgetc(scanner->stream);
                        if (next2 == '"')
                        {
                            int next3 = fgetc(scanner->stream);
                            // multiline interpolated string start: s"""
                            if (next3 == '"')
                            {
                                scanner->modeTop++;
                                scanner->modeStack[scanner->modeTop] =
                                    MODE_INTERP_MULTI;
                                token->type = TYPE_INTERP_START;
                                return 0;
                            }
                            // fallback if only s"" encountered
                            ungetc(next3, scanner->stream);
                        }
                        ungetc(next2, scanner->stream);

                        // singleline interpolated string start: s"
                        scanner->modeTop++;
                        scanner->modeStack[scanner->modeTop] =
                            MODE_INTERP_SINGLE;
                        token->type = TYPE_INTERP_START;
                        return 0;
                    }
                    // not an interpolated string prefix, rollback character
                    ungetc(next1, scanner->stream);
                }

                // start identifier or keyword parse
                if (isalpha(c) || c == '_')
                {
                    STR_TRY(strInit(&attr));
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_IDENTIFIER;
                    break;
                }

                // check for main annotation token
                if (c == '@')
                {
                    char expected[] = "main";
                    for (int i = 0; i < 4; i++)
                    {
                        int next = fgetc(scanner->stream);
                        if (next != expected[i])
                        {
                            token->type = TYPE_ERROR;
                            return 1;
                        }
                    }

                    int next = fgetc(scanner->stream);

                    // ensure @main is not a prefix of an identifier
                    if (isalnum(next) || next == '_')
                    {
                        token->type = TYPE_ERROR;
                        return 1;
                    }

                    ungetc(next, scanner->stream);

                    token->type = TYPE_MAIN;
                    return 0;
                }

                // handle left paren token
                if (c == '(')
                {
                    scanner->parenLevel++;
                    token->type = TYPE_LEFT_PAREN;
                    return 0;
                }

                // check double literal starting with dot or dot token
                if (c == '.')
                {
                    int next = fgetc(scanner->stream);
                    if (isdigit(next))
                    {
                        STR_TRY(strInit(&attr));
                        STR_TRY(strAppend(&attr, c));
                        STR_TRY(strAppend(&attr, next));
                        state = STATE_DOUBLE_LITERAL;
                        break;
                    }
                    ungetc(next, scanner->stream);
                    token->type = TYPE_DOT;
                    return 0;
                }


                // handle eof inside start state
                if (c == EOF)
                {
                    if (scanner->indentTop > 0)
                    {
                        scanner->indentTop--;
                        token->type = TYPE_DEDENT;
                        return 0;
                    }
                    scanner->atLineStart = false;
                    token->type = TYPE_EOF;
                    return 0;
                }
                // lexical error for unhandled characters
                token->type = TYPE_ERROR;
                return 1;
                break;


            // resolve assign or equal operator
            case STATE_ASSIGNMENT:
                if (c == '=')
                {
                    token->type = TYPE_EQUAL;
                    return 0;
                }
                if (c == '>')
                {
                    token->type = TYPE_ARROW;
                    return 0;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_ASSIGNMENT;
                return 0;
            // resolve lesser or lesser equal operator
            case STATE_LESSER:
                if (c == '=')
                {
                    token->type = TYPE_LESSER_EQUAL;
                    return 0;
                }
                if (c == '-')
                {
                    token->type = TYPE_LARROW;
                    return 0;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_LESSER;
                return 0;
            // resolve bigger or bigger equal operator
            case STATE_BIGGER:
                if (c == '=')
                {
                    token->type = TYPE_BIGGER_EQUAL;
                    return 0;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_BIGGER;
                return 0;
            // resolve not equal operator
            case STATE_EXCLAMATION:
                if (c == '=')
                {
                    token->type = TYPE_NOT_EQUAL;
                    return 0;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_EXCLAMATION;
                return 0;
            // resolve divide operator or comment
            case STATE_SLASH:
                if (c == '/')
                {
                    state = STATE_LINE_COMMENT;
                    break;
                }
                if (c == '*')
                {
                    commentLevel = 1;
                    state = STATE_BLOCK_COMMENT;
                    break;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_DIV;
                return 0;
            // read multiline block comment
            case STATE_BLOCK_COMMENT:

                if (c == EOF)
                {
                    token->type = TYPE_ERROR;
                    return 1;
                }

                if (c == '\n')
                {
                    scanner->line++;
                    hasNewLine = true;
                    break;
                }

                // increase depth for nested comment
                if (c == '/')
                {
                    int next = fgetc(scanner->stream);
                    if (next == '*')
                    {
                        commentLevel++;
                    }
                    else
                    {
                        ungetc(next, scanner->stream);
                    }
                    break;
                }

                // decrease depth if comment closes
                if (c == '*')
                {
                    int next = fgetc(scanner->stream);
                    if (next == '/')
                    {
                        commentLevel--;
                        if (commentLevel == 0)
                        {
                            if (hasNewLine && scanner->parenLevel == 0)
                            {
                                scanner->atLineStart = true;
                                token->type = TYPE_EOL;
                                return 0;
                            }
                            else
                            {
                                scanner->lfSeen = true;
                            }
                            state = STATE_START;
                        }
                    }
                    else
                    {
                        ungetc(next, scanner->stream);
                    }
                }
                token->type = TYPE_ERROR;
                break;

            // read single line comment state
            case STATE_LINE_COMMENT:
                if (c == '\n')
                {
                    scanner->line++;
                    if (scanner->parenLevel > 0)
                    {
                        scanner->lfSeen = true;
                        state = STATE_START;
                        break;
                    }
                    token->line = scanner->line;
                    scanner->atLineStart = true;
                    token->type = TYPE_EOL;
                    return 0;
                }

                if (c == EOF)
                {
                    ungetc(c, scanner->stream);
                    state = STATE_START;
                    break;
                }
                break;

            // process zero prefix states
            case STATE_ZERO:
                if (c == '.')
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_DOT;
                    break;
                }

                if (c == 'e' || c == 'E')
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_EXP;
                    break;
                }

                // lexical error on leading zero
                if (isdigit(c))
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                ungetc(c, scanner->stream);
                token->type = TYPE_INT_LITERAL;
                token->value.numberInt = 0;
                strFree(&attr);
                return 0;

            // read integer literal lexeme
            case STATE_INT_LITERAL:

                if (isdigit(c))
                {
                    STR_TRY(strAppend(&attr, c));
                    break;
                }

                if (c == '_')
                {
                    state = STATE_INT_UNDERSCORE;
                    break;
                }

                if (c == '.')
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_DOT;
                    break;
                }

                if (c == 'e' || c == 'E')
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_EXP;
                    break;
                }

                ungetc(c, scanner->stream);
                token->type = TYPE_INT_LITERAL;
                token->value.numberInt = strtoll(attr.data, NULL, 10);
                strFree(&attr);
                return 0;

            // read underscore inside int literal
            case STATE_INT_UNDERSCORE:
                if (isdigit(c))
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_INT_LITERAL;
                    break;
                }

                if (c == '_')
                {
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;
            // switch to double literal state
            case STATE_DOT:
                if (isdigit(c))
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_DOUBLE_LITERAL;
                    break;
                }
                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            // read double literal lexeme
            case STATE_DOUBLE_LITERAL:
                if (isdigit(c))
                {
                    STR_TRY(strAppend(&attr, c));
                    break;
                }

                if (c == '_')
                {
                    state = STATE_DOUBLE_UNDERSCORE;
                    break;
                }

                if (c == 'e' || c == 'E')
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_EXP;
                    break;
                }

                if (c == 'd' || c == 'D')
                {
                    token->type = TYPE_DOUBLE_LITERAL;
                    token->value.numberFloat = strtod(attr.data, NULL);
                    strFree(&attr);
                    return 0;
                }

                ungetc(c, scanner->stream);
                token->type = TYPE_DOUBLE_LITERAL;
                token->value.numberFloat = strtod(attr.data, NULL);
                strFree(&attr);
                return 0;

            // read underscore inside double literal
            case STATE_DOUBLE_UNDERSCORE:
                if (isdigit(c))
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_DOUBLE_LITERAL;
                    break;
                }

                if (c == '_')
                {
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            // switch to exponent state
            case STATE_EXP:
                if (c == '+' || c == '-')
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_EXP_SIGN;
                    break;
                }

                if (isdigit(c))
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_EXP_LITERAL;
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            // read exponent sign symbol
            case STATE_EXP_SIGN:
                if (isdigit(c))
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_EXP_LITERAL;
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;


            // read exponent literal lexeme
            case STATE_EXP_LITERAL:
                if (isdigit(c))
                {
                    STR_TRY(strAppend(&attr, c));
                    break;
                }

                if (c == '_')
                {
                    state = STATE_EXP_UNDERSCORE;
                    break;
                }

                if (c == 'd' || c == 'D')
                {
                    token->type = TYPE_DOUBLE_LITERAL;
                    token->value.numberFloat = strtod(attr.data, NULL);
                    strFree(&attr);
                    return 0;
                }

                ungetc(c, scanner->stream);
                token->type = TYPE_DOUBLE_LITERAL;
                token->value.numberFloat = strtod(attr.data, NULL);
                strFree(&attr);
                return 0;

            // read underscore inside exponent literal
            case STATE_EXP_UNDERSCORE:
                if (isdigit(c))
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_EXP_LITERAL;
                    break;
                }

                if (c == '_')
                {
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            // read identifier lexeme string
            case STATE_IDENTIFIER:
                if (isalnum(c) || c == '_')
                {
                    STR_TRY(strAppend(&attr, c));
                    break;
                }
                ungetc(c, scanner->stream);

                // single underscore cannot be an identifier
                if (attr.len == 1 && attr.data[0] == '_')
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                token->type = TYPE_IDENTIFIER;

                // check if identifier matches keyword
                for (int i = 0; Keywords[i].keyword != NULL; i++)
                {
                    if (strcmp(attr.data, Keywords[i].keyword) == 0)
                    {
                        token->type = Keywords[i].type;
                        break;
                    }
                }

                // allocate memory for identifier lexeme
                if (token->type == TYPE_IDENTIFIER)
                {
                    ALLOC_TOKEN(token->value.identifier, char*, attr.len + 1);
                    strcpy(token->value.identifier, attr.data);
                }

                strFree(&attr);
                return 0;

            // read text inside string literal
            case STATE_STRING:
                if (c == '"')
                {
                    token->type = TYPE_STRING_LITERAL;
                    ALLOC_TOKEN(token->value.string, char*, attr.len + 1);
                    strcpy(token->value.string, attr.data);

                    strFree(&attr);
                    return 0;
                }

                if (c == '\\')
                {
                    state = STATE_STRING_ESC;
                    break;
                }

                if (c == '\n' || c == EOF)
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                STR_TRY(strAppend(&attr, c));
                break;

            // read string escape sequence
            case STATE_STRING_ESC:
                if (c == '"')
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_STRING;
                    break;
                }

                if (c == 'n')
                {
                    STR_TRY(strAppend(&attr, '\n'));
                    state = STATE_STRING;
                    break;
                }

                if (c == 't')
                {
                    STR_TRY(strAppend(&attr, '\t'));
                    state = STATE_STRING;
                    break;
                }

                if (c == 'r')
                {
                    STR_TRY(strAppend(&attr, '\r'));
                    state = STATE_STRING;
                    break;
                }

                if (c == '\\')
                {
                    STR_TRY(strAppend(&attr, c));
                    state = STATE_STRING;
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            // read text inside multiline string
            case STATE_MULTILINE_STRING:
                if (c == EOF)
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                if (c == '\n')
                {
                    scanner->line++;
                    STR_TRY(strAppend(&attr, '\n'));
                    break;
                }

                if (c == '"')
                {
                    int c2 = fgetc(scanner->stream);
                    if (c2 == '"')
                    {
                        int c3 = fgetc(scanner->stream);
                        // finish multiline string token
                        if (c3 == '"')
                        {
                            token->type = TYPE_STRING_LITERAL;
                            ALLOC_TOKEN(token->value.string, char*,
                                        attr.len + 1);
                            strcpy(token->value.string, attr.data);
                            strFree(&attr);
                            return 0;
                        }

                        STR_TRY(strAppend(&attr, '"'));
                        STR_TRY(strAppend(&attr, '"'));
                        ungetc(c3, scanner->stream);
                        break;
                    }
                    STR_TRY(strAppend(&attr, '"'));
                    ungetc(c2, scanner->stream);
                    break;
                }

                STR_TRY(strAppend(&attr, c));
                break;
                // read characters inside singleline interpolated string
            case STATE_INTERP_SINGLE:
                if (attr.data == NULL)
                {
                    STR_TRY(strInit(&attr));
                }

                if (c == EOF)
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                // closing quote of interpolated string
                if (c == '"')
                {
                    // if string part accumulated, return it first
                    if (attr.len > 0)
                    {
                        ungetc(c, scanner->stream);
                        token->type = TYPE_STR_PART;
                        ALLOC_TOKEN(token->value.string, char*, attr.len + 1);
                        strcpy(token->value.string, attr.data);
                        strFree(&attr);
                        return 0;
                    }
                    // finish interpolated string
                    scanner->modeTop--;
                    token->type = TYPE_INTERP_END;
                    strFree(&attr);
                    return 0;
                }

                // interpolation entry or escaped dollar
                if (c == '$')
                {
                    // return accumulated text before interpolation
                    if (attr.len > 0)
                    {
                        ungetc(c, scanner->stream);
                        token->type = TYPE_STR_PART;
                        ALLOC_TOKEN(token->value.string, char*, attr.len + 1);
                        strcpy(token->value.string, attr.data);
                        strFree(&attr);
                        return 0;
                    }

                    int next = fgetc(scanner->stream);

                    // escaped dollar: $$
                    if (next == '$')
                    {
                        STR_TRY(strAppend(&attr, '$'));
                        break;
                    }

                    // embedded expression: ${expr}
                    if (next == '{')
                    {
                        scanner->modeTop++;
                        scanner->modeStack[scanner->modeTop] = MODE_DEFAULT;
                        token->type = TYPE_LEFT_BRACE;
                        strFree(&attr);
                        return 0;
                    }

                    // embedded variable: $id
                    if (isalpha(next) || next == '_')
                    {
                        ungetc(next, scanner->stream);
                        state = STATE_INTERP_ID;
                        break;
                    }

                    // invalid character after dollar
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                // escape sequence start
                if (c == '\\')
                {
                    state = STATE_INTERP_ESC;
                    break;
                }

                // unescaped newline not allowed in singleline strings
                if (c == '\n')
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                STR_TRY(strAppend(&attr, c));
                break;

            // read escape sequence inside interpolated string
            case STATE_INTERP_ESC:
                if (c == '"')
                {
                    STR_TRY(strAppend(&attr, '"'));
                }
                else if (c == 'n')
                {
                    STR_TRY(strAppend(&attr, '\n'));
                }
                else if (c == 't')
                {
                    STR_TRY(strAppend(&attr, '\t'));
                }
                else if (c == 'r')
                {
                    STR_TRY(strAppend(&attr, '\r'));
                }
                else if (c == '\\')
                {
                    STR_TRY(strAppend(&attr, '\\'));
                }
                else
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }
                state = STATE_INTERP_SINGLE;
                break;

            // read variable identifier after $
            case STATE_INTERP_ID:
                if (attr.data == NULL)
                {
                    STR_TRY(strInit(&attr));
                }

                if (isalnum(c) || c == '_')
                {
                    STR_TRY(strAppend(&attr, c));
                    break;
                }

                ungetc(c, scanner->stream);

                // single underscore cannot be an identifier
                if (attr.len == 1 && attr.data[0] == '_')
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                token->type = TYPE_IDENTIFIER;
                ALLOC_TOKEN(token->value.identifier, char*, attr.len + 1);
                strcpy(token->value.identifier, attr.data);
                strFree(&attr);
                return 0;

            // read characters inside multiline interpolated string
            case STATE_INTERP_MULTI:
                if (attr.data == NULL)
                {
                    STR_TRY(strInit(&attr));
                }

                if (c == EOF)
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                // newlines are part of multiline strings
                if (c == '\n')
                {
                    scanner->line++;
                    STR_TRY(strAppend(&attr, '\n'));
                    break;
                }

                // check triple quote closing
                if (c == '"')
                {
                    int c2 = fgetc(scanner->stream);
                    if (c2 == '"')
                    {
                        int c3 = fgetc(scanner->stream);
                        if (c3 == '"')
                        {
                            // emit remaining text if buffer not empty
                            if (attr.len > 0)
                            {
                                ungetc('"', scanner->stream);
                                ungetc('"', scanner->stream);
                                ungetc('"', scanner->stream);
                                token->type = TYPE_STR_PART;
                                ALLOC_TOKEN(token->value.string, char*,
                                            attr.len + 1);
                                strcpy(token->value.string, attr.data);
                                strFree(&attr);
                                return 0;
                            }
                            // finish multiline interpolated string
                            scanner->modeTop--;
                            token->type = TYPE_INTERP_END;
                            strFree(&attr);
                            return 0;
                        }
                        STR_TRY(strAppend(&attr, '"'));
                        STR_TRY(strAppend(&attr, '"'));
                        ungetc(c3, scanner->stream);
                        break;
                    }
                    STR_TRY(strAppend(&attr, '"'));
                    ungetc(c2, scanner->stream);
                    break;
                }

                // interpolation entry or escaped dollar in multiline
                if (c == '$')
                {
                    if (attr.len > 0)
                    {
                        ungetc(c, scanner->stream);
                        token->type = TYPE_STR_PART;
                        ALLOC_TOKEN(token->value.string, char*, attr.len + 1);
                        strcpy(token->value.string, attr.data);
                        strFree(&attr);
                        return 0;
                    }

                    int next = fgetc(scanner->stream);

                    if (next == '$')
                    {
                        STR_TRY(strAppend(&attr, '$'));
                        break;
                    }

                    if (next == '{')
                    {
                        scanner->modeTop++;
                        scanner->modeStack[scanner->modeTop] = MODE_DEFAULT;
                        token->type = TYPE_LEFT_BRACE;
                        strFree(&attr);
                        return 0;
                    }

                    if (isalpha(next) || next == '_')
                    {
                        ungetc(next, scanner->stream);
                        state = STATE_INTERP_ID;
                        break;
                    }

                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                STR_TRY(strAppend(&attr, c));
                break;
        }
    }
    return 0;
}
